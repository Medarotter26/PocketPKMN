// 3DS infrared transport for the Pokewalker protocol.
//
// Talks to the console's IR chip (NXP SC16IS750) directly over I2C, the method
// the existing Pokewalker homebrew uses. Needs Luma3DS (it maps the I/O
// registers into homebrew).
//
// The system IR service (ir:u) was tried first and fails with a real walker:
// it reports a send as finished ~12 ms after it started, the walker answers
// ~5 ms after our packet, and ir:u cannot listen while sending, so every reply
// is missed (hardware test 1, 2026-11-14). That code is kept in attic/ir_iru.c.

#pragma once

#include "pw_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns 0 on success and fills *out; on failure returns a negative value and
// logs the reason (see dbg.h).
int ir_raw_open(pw_transport *out);
void ir_raw_close(void);
// Write the bus-error / retry counters to the log and reset them.
void ir_raw_stats_log(void);

// 1 (default): talk to the chip through the system's i2c::IR service when its
// self-check passes (no collisions with the C-stick reads on New 3DS);
// 0: always drive the I2C bus registers directly. Read at ir_raw_open.
extern int ir_raw_prefer_service;
// Optional hook the transport calls before risky steps so the log reaches the
// SD card even if the console freezes.
extern void (*ir_raw_checkpoint)(void);
const char *ir_raw_access_name(void);

// Test A ("Bus watch"): for duration_us, record busy periods of the I2C bus the
// IR chip is on, by reading only its control register (nothing is written).
// Fills up to cap start times / lengths (us), returns the count (-1 if the I/O
// registers are not mapped); *turns = loop turns made, *dropped = periods past cap.
int ir_raw_bus_watch(uint32_t duration_us, uint32_t *starts, uint32_t *lens, int cap, uint32_t *turns,
                     uint32_t *dropped);

// Bus sniff: at every busy -> idle edge, also record the controller's data and
// control registers (plain reads, nothing written). Returns the edge count.
int ir_raw_bus_sniff(uint32_t duration_us, uint32_t *times, uint8_t *data, uint8_t *cnt, int cap);

// v0.12 burst guard (docs/LINK_REVIEW.md 9): 1 = on (default). Read at ir_raw_open.
extern int ir_raw_guard_enabled;

#ifdef __cplusplus
}
#endif
