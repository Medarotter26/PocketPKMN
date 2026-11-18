// PC test of review finding 6 (v0.86, reworked in v0.87 after the advisor's review): the "move in progress" mark of
// source/storage.cpp (depositOne, withdrawOne, bankSaveMove, countInSave, settleMove, resolveMoves). storage.cpp needs
// the 3DS libraries, so this is a faithful model of its rules (like tests/test_files.c for the file saves):
//   deposit:  [refused if a marked PocketPKMN copy of it exists]  count before (boxes + party of THIS save)
//             1. entry saved WITH mark 1 + record (this save, count)   2. game written without it   3. mark cleared
//             failed game write: still in the game (count unchanged) -> the entry goes at once ("Nothing was moved")
//   withdraw: a marked entry: only into the save of its record, settled there first
//             1. mark 2 + record (this save, count) saved   2. game written with it   3. entry cleared
//             refused game write -> mark cleared at once
//   settle (every save scan, only against the record's save):  deposit: count unchanged -> entry removed;
//             withdraw: count + 1 -> entry removed;  anything else -> entry kept, mark cleared.
// Every Pokemon has a hidden identity (uid); sameMon only sees the "pid" - so twins (clones, in-game trades) are
// different Pokemon that look the same. Cuts before every step, then what the player may do meanwhile (nothing, take it
// into the party, change it, put it in the Day Care, the save is away for a while), with or without a twin in the save,
// and withdrawals into the save it came from or into another save. Must hold after the settle: the Pokemon is never
// lost; never in two places - except when the game hid it where PocketPKMN cannot look (Day Care), where only an extra
// copy (never a loss) is allowed. Usage: bash tests/run_moves.sh
#include <stdio.h>
#include <string.h>

#define SLOTS 8
typedef struct { int present, uid, pid; } mon;
typedef struct { mon box[SLOTS], party[SLOTS], daycare; int here; } save;
typedef struct { mon m; int mark, rec_save, rec_count; } entry;

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("  FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static int same(const mon *a, const mon *b) { return a->present && b->present && a->pid == b->pid; }
static int count_in(const save *s, const mon *m) // countInSave: boxes + party (not the Day Care)
{
	int n = 0;
	for (int i = 0; i < SLOTS; i++)
		n += same(&s->box[i], m) + same(&s->party[i], m);
	return n;
}
static int copies(const save *sv, int nsaves, const entry *e, int uid) // the real count, Day Care too
{
	int n = e->m.present && e->m.uid == uid;
	for (int k = 0; k < nsaves; k++) {
		for (int i = 0; i < SLOTS; i++)
			n += (sv[k].box[i].present && sv[k].box[i].uid == uid) + (sv[k].party[i].present && sv[k].party[i].uid == uid);
		n += sv[k].daycare.present && sv[k].daycare.uid == uid;
	}
	return n;
}
static int free_box(save *s)
{
	for (int i = 0; i < SLOTS; i++)
		if (!s->box[i].present)
			return i;
	return -1;
}

static void settle(save *sv, entry *e, int s)
{
	if (!e->m.present || !e->mark || !sv[s].here)
		return;
	int now = count_in(&sv[s], &e->m);
	int extra = (e->mark == 1 && now == e->rec_count) || (e->mark == 2 && now == e->rec_count + 1);
	if (extra)
		memset(e, 0, sizeof *e);
	else
		e->mark = 0;
}
static void resolve(save *sv, entry *e) { if (e->m.present && e->mark) settle(sv, e, e->rec_save); }

// step 1..3 = the power goes before that step, 4 = none; fail = the game write fails (nothing written), refuse = it is
// refused. Returns 0 if the app refused the move up front.
static int deposit(save *sv, entry *e, int s, int slot, int step, int fail)
{
	mon m = sv[s].box[slot];
	if (e->m.present && e->mark && same(&e->m, &m))
		return 0; // a marked copy of it exists
	int before = count_in(&sv[s], &m);
	if (step <= 1) return 1;
	e->m = m; e->mark = 1; e->rec_save = s; e->rec_count = before;     // 1.
	if (step <= 2) return 1;
	if (fail) {                                                        // 2. fails, nothing written
		if (count_in(&sv[s], &m) == before)
			memset(e, 0, sizeof *e);                                   // taken out at once (F4)
		return 1;
	}
	memset(&sv[s].box[slot], 0, sizeof(mon));                          // 2.
	if (step <= 3) return 1;
	e->mark = 0;                                                       // 3.
	return 1;
}
static int withdraw(save *sv, entry *e, int d, int step, int refuse)
{
	if (e->mark) {
		if (e->rec_save != d)
			return 0;                                                  // F3: only into the record's save
		settle(sv, e, d);
		if (!e->m.present)
			return 1;
	}
	int before = count_in(&sv[d], &e->m), slot = free_box(&sv[d]);
	if (slot < 0) return 0;
	if (step <= 1) return 1;
	e->mark = 2; e->rec_save = d; e->rec_count = before;               // 1.
	if (step <= 2) return 1;
	if (refuse) { e->mark = 0; return 1; }                             // refused: mark cleared at once
	sv[d].box[slot] = e->m;                                            // 2.
	if (step <= 3) return 1;
	memset(e, 0, sizeof *e);                                           // 3.
	return 1;
}

// the player meanwhile: 0 nothing, 1 takes it into the party, 2 changes it (bytes - the model's pid stays), 3 puts it
// in the Day Care
static void player(save *s, int uid, int what)
{
	for (int i = 0; i < SLOTS; i++)
		if (s->box[i].present && s->box[i].uid == uid) {
			if (what == 1) { s->party[0] = s->box[i]; memset(&s->box[i], 0, sizeof(mon)); }
			if (what == 3) { s->daycare = s->box[i]; memset(&s->box[i], 0, sizeof(mon)); }
			return;
		}
}

int main(void)
{
	const int UID = 7, PID = 4242, TWIN_UID = 8;
	int runs = 0;
	for (int dir = 0; dir < 2; dir++)
		for (int step = 1; step <= 4; step++)
			for (int what = 0; what <= 3; what++)
				for (int twin = 0; twin < 2; twin++)
					for (int away = 0; away < 2; away++)
						for (int other = 0; other < (dir ? 2 : 1); other++) {
							save sv[2];
							entry e;
							memset(sv, 0, sizeof sv);
							memset(&e, 0, sizeof e);
							sv[0].here = sv[1].here = 1;
							mon m = {1, UID, PID}, t = {1, TWIN_UID, PID};
							if (twin)
								sv[0].box[5] = t;               // a clone / in-game trade with the same PID
							int dest = other ? 1 : 0;
							if (dir == 0) {
								sv[0].box[1] = m;
								deposit(sv, &e, 0, 1, step, 0);
							} else {
								e.m = m;
								withdraw(sv, &e, dest, step, 0);
							}
							player(&sv[dest], UID, what);
							// before any settle: two copies only with the PocketPKMN one marked
							if (copies(sv, 2, &e, UID) == 2)
								CHECK(e.mark, "run %d: two copies, PocketPKMN one NOT marked", runs);
							if (away) { // the save is not in the console at the next start: nothing settles
								sv[dest].here = 0;
								int had = e.m.present;
								resolve(sv, &e);
								CHECK(e.m.present == had, "run %d: settled with its save away", runs);
								// and it cannot be put into the other save meanwhile (F3)
								if (e.mark && e.m.present)
									CHECK(!withdraw(sv, &e, 1 - dest, 4, 0) || e.rec_save == 1 - dest,
									      "run %d: marked copy went into another save", runs);
								sv[dest].here = 1;
							}
							resolve(sv, &e);
							int n = copies(sv, 2, &e, UID), tw = twin ? copies(sv, 2, &e, TWIN_UID) : 1;
							CHECK(n >= 1, "run %d (%s, cut before %d, player %d, twin %d, away %d, other %d): LOST",
							      runs, dir ? "withdraw" : "deposit", step, what, twin, away, other);
							CHECK(tw == 1, "run %d: the twin was touched (%d)", runs, tw);
							if (what != 3)
								CHECK(n == 1, "run %d (%s, cut before %d, player %d, twin %d, away %d, other %d): %d copies",
								      runs, dir ? "withdraw" : "deposit", step, what, twin, away, other, n);
							CHECK(!e.m.present || !e.mark, "run %d: still marked after the settle", runs);
							runs++;
						}
	// F4: a deposit whose game write fails (nothing written) - pressing A again must not make a second copy
	for (int twin = 0; twin < 2; twin++) {
		save sv[2];
		entry e;
		memset(sv, 0, sizeof sv);
		memset(&e, 0, sizeof e);
		sv[0].here = sv[1].here = 1;
		mon m = {1, UID, PID}, t = {1, TWIN_UID, PID};
		sv[0].box[1] = m;
		if (twin)
			sv[0].box[5] = t;
		deposit(sv, &e, 0, 1, 4, 1);     // fails
		CHECK(!e.m.present, "F4 (twin %d): the PocketPKMN copy stayed after a failed write", twin);
		entry e2 = e;
		deposit(sv, &e2, 0, 1, 4, 0);    // retried, completes
		resolve(sv, &e2);
		CHECK(copies(sv, 2, &e2, UID) == 1, "F4 (twin %d): %d copies after the retry", twin, copies(sv, 2, &e2, UID));
		runs++;
	}
	// a refused withdraw: the mark goes at once (no block until the next scan), it stays in PocketPKMN
	{
		save sv[2];
		entry e;
		memset(sv, 0, sizeof sv);
		memset(&e, 0, sizeof e);
		sv[0].here = 1;
		mon m = {1, UID, PID};
		e.m = m;
		withdraw(sv, &e, 0, 4, 1);
		CHECK(e.m.present && !e.mark, "refused withdraw: still marked or gone");
		runs++;
	}
	printf("finding 6 model: %d runs, %d checks, %d failure(s)\n", runs, checks, failures);
	printf(failures ? "MOVE TESTS: %d FAILED\n" : "move tests passed (%d failures)\n", failures);
	return failures != 0;
}
