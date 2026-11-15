// In-memory diagnostic log. Nothing touches the SD card while an IR exchange
// is running (file I/O is slow enough to break the link timing); the log is
// written out afterwards with dbg_flush.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void dbg_reset(void);
void dbg_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// Hex dump; at most max bytes are printed, the rest is summarised.
void dbg_hex(const char *tag, const uint8_t *data, size_t len, size_t max);
// Append the log to a file on the SD card and clear it. Returns 0 on success.
int dbg_flush(const char *path);
// Microseconds since the app started.
uint64_t dbg_now_us(void);

#ifdef __cplusplus
}
#endif
