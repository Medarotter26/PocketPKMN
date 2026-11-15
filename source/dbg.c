#include "dbg.h"

#include <3ds.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_CAP (512 * 1024)

static char log_buf[LOG_CAP];
static size_t log_len;
static int log_overflowed;
static u64 start_tick;

uint64_t dbg_now_us(void)
{
	if (!start_tick)
		start_tick = svcGetSystemTick();
	return (uint64_t)((svcGetSystemTick() - start_tick) / CPU_TICKS_PER_USEC);
}

void dbg_reset(void)
{
	log_len = 0;
	log_overflowed = 0;
}

static void append(const char *s, size_t n)
{
	if (log_len + n > LOG_CAP) {
		log_overflowed = 1;
		return;
	}
	memcpy(log_buf + log_len, s, n);
	log_len += n;
}

void dbg_printf(const char *fmt, ...)
{
	// v0.13 (LINK_REVIEW 11.1b): 384 bytes, and a line that did not fit ends in
	// " [cut]" so a truncated line can never look complete (test 12 lost the end
	// of the stats line silently).
	static const char cut[] = " [cut]";
	char line[384];
	uint64_t us = dbg_now_us();
	int n = snprintf(line, sizeof(line), "[%6lu.%03lu] ", (unsigned long)(us / 1000), (unsigned long)(us % 1000));
	va_list ap;
	va_start(ap, fmt);
	size_t room = sizeof(line) - (size_t)n - 1; // keep one byte for the newline
	int m = vsnprintf(line + n, room, fmt, ap);
	va_end(ap);
	if (m < 0)
		return;
	size_t total;
	if ((size_t)m >= room) {
		// vsnprintf stored room - 1 characters; overwrite the end with the marker.
		total = (size_t)n + room - 1;
		memcpy(line + total - (sizeof(cut) - 1), cut, sizeof(cut) - 1);
	} else {
		total = (size_t)n + (size_t)m;
	}
	line[total++] = '\n';
	append(line, total);
}

void dbg_hex(const char *tag, const uint8_t *data, size_t len, size_t max)
{
	char line[256];
	size_t shown = len < max ? len : max;
	size_t pos = 0;
	while (pos < shown || (pos == 0 && len == 0)) {
		int n = snprintf(line, sizeof(line), "    %s %03x:", tag, (unsigned)pos);
		for (size_t i = 0; i < 16 && pos < shown; i++, pos++)
			n += snprintf(line + n, sizeof(line) - (size_t)n, " %02x", data[pos]);
		line[n++] = '\n';
		append(line, (size_t)n);
		if (len == 0)
			break;
	}
	if (shown < len) {
		int n = snprintf(line, sizeof(line), "    %s ... %u more byte(s)\n", tag, (unsigned)(len - shown));
		append(line, (size_t)n);
	}
}

int dbg_flush(const char *path)
{
	FILE *f = fopen(path, "ab");
	if (!f)
		return -1;
	if (log_overflowed)
		fputs("(log buffer filled up; later lines were dropped)\n", f);
	size_t w = fwrite(log_buf, 1, log_len, f);
	int bad = (w != log_len);
	if (fclose(f) != 0)
		bad = 1;
	dbg_reset();
	return bad ? -1 : 0;
}
