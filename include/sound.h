// sound.h - v0.55 background music and sound effects (user: "can we add background music? can you make this
// properly loop" / "sound effects for a successful transfer, a sound effect to play as the transfer is happening,
// and an unsuccessful transfer, and a button click").
//
// Everything is loaded into memory once at start-up and played by the 3DS sound chip (ndsp) from there: nothing is
// read from the SD card later, so playing a sound is safe while a walker is connected (no SD access, no waiting).
// - Music: sdmc:/3ds/pocketpkmn/music.bin (tools/make_music.py: an intro played once, then a loop forever). It is
//   the user's own file on their SD card and is never built into the app or the release. No file = no music.
// - Effects: romfs:/sound/{click,transfer,success,fail}.bin (tools/make_sfx.py, PocketPKMN's own sounds).
// Needs the console's DSP firmware (sdmc:/3ds/dspfirm.cdc, dumped once with DSP1); without it the app is silent.
#ifndef SOUND_H
#define SOUND_H

#ifdef __cplusplus
extern "C" {
#endif

enum { SND_CLICK, SND_TRANSFER, SND_SUCCESS, SND_FAIL, SND_COUNT };

void sound_init(int music_on, int effects_on); // after the folder move, before the first screen
void sound_exit(void);
void sound_set_music(int on);   // starts / stops the music (from its beginning)
void sound_set_effects(int on);
int sound_music_on(void);
int sound_effects_on(void);
int sound_music_available(void); // 1 if music.bin was loaded
int sound_available(void);       // 1 if the sound chip started (DSP firmware present)

void sound_play(int id);          // a one-shot effect (SND_CLICK, SND_SUCCESS, SND_FAIL)
void sound_transfer(int on);      // the transfer sound loops while on; the music goes quieter meanwhile

// hidScanInput() + a click for a newly pressed button (A, B, X, Y, D-pad, L, R, START, touch). The menus call
// this instead of hidScanInput(); hidKeysDown() etc. work as usual afterwards.
void sound_input(void);

#ifdef __cplusplus
}
#endif

#endif
