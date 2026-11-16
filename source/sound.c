// sound.c - see include/sound.h.
#include "sound.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>

#include "app_paths.h"
#include "dbg.h"

#define MUSIC_PATH APP_DIR "/music.bin"
#define CH_MUSIC 0
#define CH_FX 1
#define CH_TRANSFER 2
#define MUSIC_VOL 0.55f
// v0.57 (console test: "the transfer soundeffect is really quiet and the music isn't quiet enough during transfer,
// fade also takes too long"): transfer at full level (its file is louder too), fade 0.5 s (was 1.5 s).
// v0.59 (user: "the music isn't getting quiet quick enough ... Volume should be dim and finished fading by the time
// the ir arc animation begins ... you can restore the prior music volume level"): back to 0.18; the dim now happens
// in sound_link(1), on the main thread, before the IR link opens (the fade thread hardly runs during IR traffic,
// which is why the fade crawled).
#define MUSIC_VOL_QUIET 0.18f // while a walker link is open (user: "the background music should dim")
#define LINK_DIM_S 0.25f      // sound_link(1): the whole dim takes this long, before IR starts
#define FX_VOL 0.9f
#define TRANSFER_VOL 1.0f
// user: "It should be a gradual fade down not a sudden jump to lower volume": volumes glide to their targets,
// a full change taking about FADE_S seconds, moved in small steps by a low-priority thread every FADE_TICK_MS
#define FADE_S 0.5f
#define FADE_TICK_MS 10

typedef struct clip {
	s16 *data;      // linear memory (the DSP reads it)
	u32 rate, intro, loop; // samples per channel
	u16 channels;
} clip;

static int ready, music_on, fx_on, transfer_on, link_on;
static clip music, fx[SND_COUNT];
static ndspWaveBuf wb_music[2], wb_fx, wb_transfer;

// fades (shared with the fade thread; every change under the lock)
static LightLock lock;
static float music_cur, music_target, transfer_cur, transfer_target;
static int transfer_playing; // the transfer loop is queued (stays until its fade-out reaches 0)
static Thread fade_thread;
static volatile int fade_run;

// "PPMU" u32 rate, u16 channels, u16 0, u32 intro, u32 loop, PCM16 (tools/make_music.py / make_sfx.py)
static int load_clip(const char *path, clip *c, u32 max_bytes)
{
	memset(c, 0, sizeof(*c));
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	u8 h[20];
	int ok = 0;
	if (fread(h, 1, sizeof h, f) == sizeof h && !memcmp(h, "PPMU", 4)) {
		c->rate = h[4] | h[5] << 8 | h[6] << 16 | (u32)h[7] << 24;
		c->channels = (u16)(h[8] | h[9] << 8);
		c->intro = h[12] | h[13] << 8 | h[14] << 16 | (u32)h[15] << 24;
		c->loop = h[16] | h[17] << 8 | h[18] << 16 | (u32)h[19] << 24;
		u32 bytes = (c->intro + c->loop) * c->channels * 2;
		if (c->rate >= 8000 && c->rate <= 48000 && (c->channels == 1 || c->channels == 2) && bytes > 0 &&
		    bytes <= max_bytes && linearSpaceFree() > bytes + 8u * 1024 * 1024) { // keep room for the screens
			c->data = linearAlloc(bytes);
			if (c->data && fread(c->data, 1, bytes, f) == bytes) {
				DSP_FlushDataCache(c->data, bytes);
				ok = 1;
			} else if (c->data) {
				linearFree(c->data);
				c->data = NULL;
			}
		}
	}
	fclose(f);
	return ok;
}

static void set_vol(int ch, float vol)
{
	float mix[12] = { vol, vol };
	ndspChnSetMix(ch, mix);
}

static void chn_setup(int ch, const clip *c, float vol)
{
	ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetRate(ch, (float)c->rate);
	ndspChnSetFormat(ch, c->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	set_vol(ch, vol);
}

static float glide(float cur, float target, float step)
{
	if (cur < target)
		return cur + step > target ? target : cur + step;
	return cur - step < target ? target : cur - step;
}

// v0.62 (console: the app hung on HOME -> Close, twice): the fade thread rests while the app is suspended (HOME menu,
// sleep) and stops when the system closes the app; it no longer calls the sound service while holding `lock`
// (the main thread takes `lock` for every click - the two must never wait on each other).
static volatile int fade_paused;
static aptHookCookie fade_hook;

static void fade_apt_hook(APT_HookType hook, void *param)
{
	(void)param;
	if (hook == APTHOOK_ONSUSPEND || hook == APTHOOK_ONSLEEP)
		fade_paused = 1;
	else if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP)
		fade_paused = 0;
	else if (hook == APTHOOK_ONEXIT) {
		fade_paused = 1;
		fade_run = 0;
	}
}

// every FADE_TICK_MS: move both volumes a step towards their targets; a faded-out transfer loop is stopped
static void fade_main(void *arg)
{
	(void)arg;
	const float step = (float)FADE_TICK_MS / 1000.0f / FADE_S; // a 0 -> 1 change takes FADE_S seconds
	while (fade_run) {
		if (!fade_paused) {
			int set_music = 0, set_transfer = 0, stop_transfer = 0;
			float mv = 0, tv = 0;
			LightLock_Lock(&lock); // only our own numbers under the lock ...
			if (music_cur != music_target) {
				music_cur = glide(music_cur, music_target, step * MUSIC_VOL);
				mv = music_cur;
				set_music = 1;
			}
			if (transfer_playing && transfer_cur != transfer_target) {
				transfer_cur = glide(transfer_cur, transfer_target, step * TRANSFER_VOL);
				tv = transfer_cur;
				set_transfer = 1;
				if (transfer_cur <= 0.0f && transfer_target <= 0.0f) {
					transfer_playing = 0;
					stop_transfer = 1;
				}
			}
			LightLock_Unlock(&lock);
			if (set_music) // ... the sound service outside it
				set_vol(CH_MUSIC, mv);
			if (set_transfer)
				set_vol(CH_TRANSFER, tv);
			if (stop_transfer)
				ndspChnWaveBufClear(CH_TRANSFER);
		}
		svcSleepThread((s64)FADE_TICK_MS * 1000000LL);
	}
}

static float music_level(void)
{
	return link_on || (transfer_on && fx_on) ? MUSIC_VOL_QUIET : MUSIC_VOL;
}

void sound_link(int on)
{
	if (on == link_on)
		return;
	LightLock_Lock(&lock);
	link_on = on;
	music_target = music_level();
	LightLock_Unlock(&lock);
	if (!on || !ready || !music.data || !music_on)
		return; // coming back up: the fade thread does it once the link is closed
	// going down: done right here, on the caller's thread, before the IR link opens
	const float step = (MUSIC_VOL - MUSIC_VOL_QUIET) * FADE_TICK_MS / 1000.0f / LINK_DIM_S;
	for (int guard = 0; guard < 200; guard++) {
		LightLock_Lock(&lock);
		music_cur = glide(music_cur, music_target, step);
		set_vol(CH_MUSIC, music_cur);
		int done = music_cur == music_target;
		LightLock_Unlock(&lock);
		if (done)
			break;
		svcSleepThread((s64)FADE_TICK_MS * 1000000LL);
	}
}

static void music_start(void) // under the lock
{
	if (!ready || !music.data)
		return;
	music_cur = music_target = music_level();
	chn_setup(CH_MUSIC, &music, music_cur);
	memset(wb_music, 0, sizeof wb_music);
	int n = 0;
	if (music.intro) { // played once ...
		wb_music[n].data_vaddr = music.data;
		wb_music[n].nsamples = music.intro;
		ndspChnWaveBufAdd(CH_MUSIC, &wb_music[n++]);
	}
	if (music.loop) { // ... then this part forever
		wb_music[n].data_vaddr = music.data + music.intro * music.channels;
		wb_music[n].nsamples = music.loop;
		wb_music[n].looping = true;
		ndspChnWaveBufAdd(CH_MUSIC, &wb_music[n++]);
	}
}

void sound_init(int want_music, int want_fx)
{
	music_on = want_music;
	fx_on = want_fx;
	LightLock_Init(&lock);
	if (R_FAILED(ndspInit())) {
		dbg_printf("sound: no sound (ndspInit failed - is sdmc:/3ds/dspfirm.cdc there?)");
		return;
	}
	ready = 1;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	static const char *const names[SND_COUNT] = { "click", "transfer", "success", "fail" };
	// the app's romfs may not be mounted yet (storage.cpp mounts it later as "romfs:"): like wbx_sprites.c, use it
	// if it is there, else mount it under an own name just for this and unmount it again
	const char *dev = "romfs";
	int own = 0;
	if (FindDevice("romfs:") < 0) {
		if (R_SUCCEEDED(romfsMountSelf("wbxsnd"))) {
			dev = "wbxsnd";
			own = 1;
		}
	}
	int got = 0;
	for (int i = 0; i < SND_COUNT; i++) {
		char path[64];
		snprintf(path, sizeof path, "%s:/sound/%s.bin", dev, names[i]);
		got += load_clip(path, &fx[i], 2u * 1024 * 1024);
	}
	if (own)
		romfsUnmount("wbxsnd");
	int m = load_clip(MUSIC_PATH, &music, 24u * 1024 * 1024);
	dbg_printf("sound: %d of %d effects, music %s", got, SND_COUNT, m ? "loaded" : "none (" MUSIC_PATH " missing or too big)");
	if (m)
		dbg_printf("sound: music %lu Hz, %u ch, intro %lu + loop %lu samples", (unsigned long)music.rate,
		           music.channels, (unsigned long)music.intro, (unsigned long)music.loop);
	// the fade thread: lowest priority (runs only when the app's own thread waits), same core
	s32 prio = 0x30;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	fade_run = 1;
	fade_thread = threadCreate(fade_main, NULL, 4096, prio + 1 > 0x3F ? 0x3F : prio + 1, -2, false);
	if (!fade_thread) {
		fade_run = 0;
		dbg_printf("sound: no fade thread (volume changes are immediate)");
	} else
		aptHook(&fade_hook, fade_apt_hook, NULL); // v0.62: rest while suspended, stop when the app closes
	LightLock_Lock(&lock);
	if (music_on)
		music_start();
	LightLock_Unlock(&lock);
}

void sound_exit(void)
{
	if (fade_thread) {
		aptUnhook(&fade_hook);
		fade_run = 0;
		// v0.62: never wait forever on exit (it sleeps 10 ms between steps; 1 s is plenty)
		if (R_FAILED(threadJoin(fade_thread, 1000000000ULL)))
			dbg_printf("sound: fade thread did not stop within 1 s");
		else
			threadFree(fade_thread);
		fade_thread = NULL;
	}
	if (!ready)
		return;
	for (int ch = 0; ch < 3; ch++)
		ndspChnWaveBufClear(ch);
	ndspExit();
	ready = 0;
	if (music.data)
		linearFree(music.data);
	for (int i = 0; i < SND_COUNT; i++)
		if (fx[i].data)
			linearFree(fx[i].data);
	memset(&music, 0, sizeof music);
	memset(fx, 0, sizeof fx);
}

void sound_set_music(int on)
{
	LightLock_Lock(&lock);
	music_on = on;
	if (ready) {
		ndspChnWaveBufClear(CH_MUSIC);
		if (on)
			music_start();
	}
	LightLock_Unlock(&lock);
}

void sound_set_effects(int on)
{
	LightLock_Lock(&lock);
	fx_on = on;
	if (!on && ready) {
		ndspChnWaveBufClear(CH_FX);
		ndspChnWaveBufClear(CH_TRANSFER);
		transfer_playing = 0;
		transfer_cur = transfer_target = 0;
	}
	music_target = music_level();
	if (!fade_thread && music.data) {
		music_cur = music_target;
		set_vol(CH_MUSIC, music_cur);
	}
	LightLock_Unlock(&lock);
}

int sound_music_on(void) { return music_on; }
int sound_effects_on(void) { return fx_on; }
int sound_music_available(void) { return music.data != NULL; }
int sound_available(void) { return ready; }

void sound_play(int id)
{
	if (!ready || !fx_on || id < 0 || id >= SND_COUNT || id == SND_TRANSFER || !fx[id].data)
		return;
	LightLock_Lock(&lock);
	ndspChnWaveBufClear(CH_FX); // a new effect replaces the one still playing
	chn_setup(CH_FX, &fx[id], FX_VOL);
	memset(&wb_fx, 0, sizeof wb_fx);
	wb_fx.data_vaddr = fx[id].data;
	wb_fx.nsamples = fx[id].intro + fx[id].loop;
	ndspChnWaveBufAdd(CH_FX, &wb_fx);
	LightLock_Unlock(&lock);
}

void sound_transfer(int on)
{
	if (on == transfer_on)
		return;
	LightLock_Lock(&lock);
	transfer_on = on;
	if (ready) {
		music_target = music_level(); // the music glides down / back up
		const clip *c = &fx[SND_TRANSFER];
		if (on && fx_on && c->data) {
			if (!transfer_playing) { // start silent, then fade in
				transfer_cur = 0;
				chn_setup(CH_TRANSFER, c, fade_thread ? 0.0f : TRANSFER_VOL);
				memset(&wb_transfer, 0, sizeof wb_transfer);
				wb_transfer.data_vaddr = c->data;
				wb_transfer.nsamples = c->intro + c->loop;
				wb_transfer.looping = true;
				ndspChnWaveBufAdd(CH_TRANSFER, &wb_transfer);
				transfer_playing = 1;
			}
			transfer_target = TRANSFER_VOL; // (a fade-out still running turns back up)
		} else {
			transfer_target = 0; // fades out; the fade thread stops the loop at 0
		}
		if (!fade_thread) { // no thread: change at once
			music_cur = music_target;
			if (music.data)
				set_vol(CH_MUSIC, music_cur);
			if (!on && transfer_playing) {
				ndspChnWaveBufClear(CH_TRANSFER);
				transfer_playing = 0;
			}
		}
	}
	LightLock_Unlock(&lock);
}

void sound_input(void)
{
	hidScanInput();
	if (hidKeysDown() & (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_L | KEY_R |
	                     KEY_START | KEY_TOUCH))
		sound_play(SND_CLICK);
}
