// PC test of the pending-walk file (v0.70, review finding 4 + addendum C): source/walk_pending.c.
//  1. save / load round trip; the save number rises.
//  2. a stop inside wp_save at each of its file steps: the load gives the old or the new record, never none, never
//     a broken one; a damaged newest copy falls back to the older one.
//  3. delivery as main.c does it (mark "started" + save, do it, mark "done" + save), stopped at every point and
//     started again (load + settle + run the rest): no catch or item is ever delivered twice; with no stop every one
//     is delivered once; a stop between "started" and the delivery loses at most that one step.
//  4. steps that wait for their game stay "to do" and the file stays until they are done.
#include "walk_pending.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char base[128], tmp[140], bak[140];

static void write_raw(const char *path, const wp_record *r)
{
	FILE *f = fopen(path, "wb");
	fwrite(r, 1, sizeof(*r), f);
	fclose(f);
}

static void sample(wp_record *r, int ncatch, int nitem)
{
	wp_init(r);
	r->state = WP_ENDED;
	r->walker = 0x1234abcd;
	r->bank_box = 0;
	r->bank_slot = 0;
	r->bank_fp = 0xdeadbeef;
	r->walk_steps = 1234;
	r->watts = 77;
	r->one_slot = 1;
	snprintf(r->nick, sizeof(r->nick), "TEST");
	for (int i = 0; i < WP_ENTRY_SIZE; i++)
		r->entry[i] = (uint8_t)(i * 7);
	r->ncatch = ncatch;
	r->nitem = nitem;
	for (int i = 0; i < ncatch; i++)
		r->catches[i].species = 100 + i;
	for (int i = 0; i < nitem; i++)
		r->item_id[i] = 200 + (unsigned)i;
}

// --- 3: a model of main.c's delivery with a "stop" at step number `stop_at` ---------------------------------------
// world = what really arrived (in the game / PocketPKMN), outside the file
static int world_catch[WP_MAX_CATCHES], world_item[WP_MAX_ITEMS], world_watts, world_gains, world_return;
static int step_no, stop_at, game_here;

#define POINT() do { if (++step_no == stop_at) return 1; } while (0) // 1 = "the power went off here"

static int save_or_stop(wp_record *r)
{
	POINT();
	return wp_save(base, r) != 0 ? 2 : 0;
}

// one simple step (watts / gains / return): started, done, finished
static int simple_step(wp_record *r, uint8_t *st, int *world)
{
	if (*st != WP_TODO)
		return 0;
	*st = WP_STARTED;
	int s = save_or_stop(r);
	if (s)
		return s;
	POINT();
	(*world)++;
	POINT();
	*st = WP_DONE;
	return save_or_stop(r);
}

static int list_steps(wp_record *r, uint8_t *st, int n, int *world)
{
	for (int i = 0; i < n; i++) {
		if (st[i] != WP_TODO)
			continue;
		if (!game_here)
			continue; // nothing made: stays "to do"
		st[i] = WP_STARTED;
		int s = save_or_stop(r);
		if (s)
			return s;
		POINT();
		world[i]++;
		POINT();
		st[i] = WP_DONE;
		s = save_or_stop(r);
		if (s)
			return s;
	}
	return 0;
}

// returns 1 if stopped
static int run(wp_record *r)
{
	int s;
	if ((s = simple_step(r, &r->st_watts, &world_watts)))
		return s;
	if ((s = simple_step(r, &r->st_gains, &world_gains)))
		return s;
	if ((s = list_steps(r, r->st_catch, r->ncatch, world_catch)))
		return s;
	if ((s = list_steps(r, r->st_item, r->nitem, world_item)))
		return s;
	if (game_here && (s = simple_step(r, &r->st_return, &world_return)))
		return s;
	if (wp_all_done(r))
		wp_remove(base);
	return 0;
}

int main(void)
{
	snprintf(base, sizeof(base), "/tmp/pocketpkmn_pending_%d.bin", (int)getpid());
	snprintf(tmp, sizeof(tmp), "%s.tmp", base);
	snprintf(bak, sizeof(bak), "%s.bak", base);
	wp_remove(base);

	// 1. round trip
	wp_record a, b;
	sample(&a, 3, 13);
	CHECK(wp_save(base, &a) == 0, "save");
	CHECK(wp_load(base, &b) == 1 && !memcmp(&a, &b, sizeof(a)), "round trip");
	uint32_t seq1 = a.seq;
	a.st_watts = WP_DONE;
	CHECK(wp_save(base, &a) == 0 && a.seq == seq1 + 1, "seq rises");
	CHECK(access(tmp, F_OK) != 0 && access(bak, F_OK) != 0, "no .tmp / .bak left after a save");

	// 2. a stop inside wp_save: old record O in base, new record N being written
	wp_record O, N, got;
	sample(&O, 2, 2);
	wp_remove(base);
	wp_save(base, &O);
	N = O;
	N.st_gains = WP_DONE;
	N.seq = O.seq; // wp_save would raise it; build the file states by hand
	{
		wp_record n2 = N;
		n2.seq = O.seq + 1;
		// wp_save's own check value: save it once into a scratch name to get the bytes
		char scratch[160];
		snprintf(scratch, sizeof(scratch), "%s.scratch", base);
		wp_record t = N;
		wp_save(scratch, &t); // t.seq = O.seq + 1 with a valid check
		n2 = t;
		wp_remove(scratch);
		// (a) .tmp written, base still old
		write_raw(tmp, &n2);
		CHECK(wp_load(base, &got) && got.seq == n2.seq && got.st_gains == WP_DONE, "stop after .tmp: newest");
		// (b) base renamed to .bak, .tmp still there
		rename(base, bak);
		CHECK(wp_load(base, &got) && got.seq == n2.seq, "stop after base -> .bak: newest");
		// (c) .tmp renamed to base, .bak still there
		rename(tmp, base);
		CHECK(wp_load(base, &got) && got.seq == n2.seq, "stop after .tmp -> base: newest");
		// (d) only .bak (the old one) left, the new copy damaged
		FILE *f = fopen(base, "r+b");
		fseek(f, 40, SEEK_SET);
		fputc(0x5A, f);
		fclose(f);
		CHECK(wp_load(base, &got) && got.seq == O.seq && got.st_gains == WP_TODO, "damaged newest: the older copy");
		// (e) a half-written newest copy
		f = fopen(base, "wb");
		fwrite(&n2, 1, sizeof(n2) / 2, f);
		fclose(f);
		CHECK(wp_load(base, &got) && got.seq == O.seq, "half-written newest: the older copy");
		wp_remove(base);
		CHECK(!wp_load(base, &got), "removed: nothing found");
	}

	// 3. delivery stopped at every point
	int total_runs = 0, lost = 0;
	for (stop_at = 1; stop_at < 200; stop_at++) {
		memset(world_catch, 0, sizeof(world_catch));
		memset(world_item, 0, sizeof(world_item));
		world_watts = world_gains = world_return = 0;
		game_here = 1;
		wp_record r;
		sample(&r, 3, 13);
		wp_remove(base);
		wp_save(base, &r); // written right after the link closes
		step_no = 0;
		int stopped = run(&r);
		// start again: load, settle, run the rest (no further stop)
		int target = stop_at;
		stop_at = -1;
		if (wp_load(base, &r)) {
			wp_settle_started(&r);
			wp_save(base, &r);
			run(&r);
		}
		stop_at = target;
		CHECK(!wp_load(base, &r), "stop at %d: the file is left over", stop_at);
		int lost_here = 0;
		for (int i = 0; i < 3; i++) {
			CHECK(world_catch[i] <= 1, "stop at %d: catch %d delivered %d times", stop_at, i, world_catch[i]);
			lost_here += world_catch[i] == 0;
		}
		for (int i = 0; i < 13; i++) {
			CHECK(world_item[i] <= 1, "stop at %d: item %d delivered %d times", stop_at, i, world_item[i]);
			lost_here += world_item[i] == 0;
		}
		CHECK(world_watts <= 1 && world_gains <= 1 && world_return <= 1, "stop at %d: a step ran twice", stop_at);
		lost_here += !world_watts + !world_gains + !world_return;
		CHECK(lost_here <= 1, "stop at %d: %d steps lost", stop_at, lost_here);
		lost += lost_here;
		total_runs++;
		if (!stopped)
			break; // stop_at is past the last point: a run without a stop
	}
	{ // no stop at all: everything once
		int ok = 1;
		for (int i = 0; i < 3; i++)
			ok &= world_catch[i] == 1;
		for (int i = 0; i < 13; i++)
			ok &= world_item[i] == 1;
		CHECK(ok && world_watts == 1 && world_gains == 1 && world_return == 1, "run without a stop: not all once");
	}

	// 4. the game is not here: catches and items wait, the file stays; later the game is here: delivered once
	{
		memset(world_catch, 0, sizeof(world_catch));
		memset(world_item, 0, sizeof(world_item));
		world_watts = world_gains = world_return = 0;
		wp_record r;
		sample(&r, 2, 3);
		wp_remove(base);
		wp_save(base, &r);
		stop_at = -1;
		game_here = 0;
		run(&r);
		CHECK(wp_load(base, &r) && r.st_catch[0] == WP_TODO && r.st_item[2] == WP_TODO && r.st_gains == WP_DONE,
		      "game not here: catches / items must wait, gains done");
		CHECK(!wp_all_done(&r), "game not here: reported as all done");
		game_here = 1;
		run(&r);
		CHECK(!wp_load(base, &r), "game here later: the file is left over");
		CHECK(world_catch[0] == 1 && world_catch[1] == 1 && world_item[0] == 1 && world_item[2] == 1 &&
		      world_gains == 1 && world_watts == 1, "game here later: not delivered once each");
	}

	// a "maybe ended" walk is never all done (nothing is delivered before it is confirmed)
	{
		wp_record r;
		sample(&r, 0, 0);
		r.state = WP_MAYBE;
		r.st_watts = r.st_gains = r.st_return = WP_DONE;
		CHECK(!wp_all_done(&r), "maybe-ended counted as done");
	}
	wp_remove(base);
	printf("pending walk: %d stop point(s) tried, %d step(s) lost in total (each at most one per stop), %d failure(s)\n",
	       total_runs, lost, fails);
	if (fails)
		return 1;
	printf("pending tests passed\n");
	return 0;
}
