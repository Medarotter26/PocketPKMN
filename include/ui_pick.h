// ui_pick.h - game select + box view for "Send to walker" (v0.37). See source/ui_pick.c.
#ifndef WBX_UI_PICK_H
#define WBX_UI_PICK_H

#ifdef __cplusplus
extern "C" {
#endif

// 1 = a Pokemon is now in the walker slot (storage_pick_commit), 0 = cancelled, -1 = no game / error (why).
int ui_pick_for_walk(char *why, int why_size);

// v0.43: the walker slot holds a Pokemon back from a walk. A = send it again (returns 1), X = put_back() and show its
// message (returns 0), B = back (0). sprite = its walker sprite (may be NULL), label = "Goodra  Lv.100".
int ui_slot_waiting(const unsigned char *sprite, const char *label, int (*put_back)(char *msg, int size));

#ifdef __cplusplus
}
#endif

#endif
