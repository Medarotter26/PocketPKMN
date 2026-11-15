// Pokewalker IR protocol, master (game) side.
//
// Portable C: no 3DS dependencies. The byte transport is supplied by the
// caller, so the same code runs on a 3DS (ir:u or raw I2C) and on a PC
// (unit tests, or a TCP link to a walker emulator).
//
// Protocol facts follow Dmitry Grinberg's Pokewalker write-up and the
// reverse-pokewalker documentation; see docs in the project folder.

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PW_HEADER_SIZE   8
#define PW_MAX_PAYLOAD   128
#define PW_MAX_PACKET    (PW_HEADER_SIZE + PW_MAX_PAYLOAD)
#define PW_XOR_KEY       0xAA
#define PW_ADVERT_BYTE   0xFC   // walker's "I am here" byte (0x56 on the wire)
#define PW_EEPROM_SIZE   0x10000

// Commands (master -> walker unless noted)
enum {
	PW_CMD_EEPROM_WRITE_CMP_LO = 0x00, // compressed write at (extra<<8)
	PW_CMD_EEPROM_WRITE_RAW_LO = 0x02, // raw 128-byte write at (extra<<8)
	PW_CMD_EEPROM_WRITE_ACK    = 0x04, // walker -> master
	PW_CMD_EEPROM_WRITE_ANY    = 0x0A, // arbitrary address/length (not used by the game)
	PW_CMD_EEPROM_READ         = 0x0C,
	PW_CMD_EEPROM_READ_REPLY   = 0x0E, // walker -> master
	PW_CMD_IDENTITY_REQ        = 0x20,
	PW_CMD_IDENTITY_REPLY      = 0x22, // walker -> master
	PW_CMD_PING                = 0x24,
	PW_CMD_PONG                = 0x26, // walker -> master
	PW_CMD_IDENTITY_PAIR       = 0x32, // reply 0x34
	PW_CMD_WALK_START_PAIR     = 0x38,
	PW_CMD_IDENTITY_RETURN     = 0x40, // reply 0x42
	PW_CMD_WALK_END            = 0x4E, // reply 0x50
	PW_CMD_IDENTITY_SEND       = 0x52, // reply 0x54
	PW_CMD_WALK_START          = 0x5A,
	PW_CMD_EEPROM_WRITE_CMP_HI = 0x80, // compressed write at (extra<<8)+0x80
	PW_CMD_EEPROM_WRITE_RAW_HI = 0x82, // raw 128-byte write at (extra<<8)+0x80
	PW_CMD_DISCONNECT          = 0xF4, // no reply
	PW_CMD_SYNACK              = 0xF8, // walker -> master
	PW_CMD_SYN                 = 0xFA,
};

#define PW_EXTRA_MASTER  0x01
#define PW_EXTRA_SLAVE   0x02

// Error codes (negative). Zero or positive values are success / byte counts.
enum {
	PW_OK              = 0,
	PW_ERR_TRANSPORT   = -1,  // transport reported a failure
	PW_ERR_TIMEOUT     = -2,  // nothing received in time
	PW_ERR_SHORT       = -3,  // fewer than 8 bytes received
	PW_ERR_CHECKSUM    = -4,
	PW_ERR_SESSION     = -5,  // reply carried a different session id
	PW_ERR_UNEXPECTED  = -6,  // reply had the wrong command byte
	PW_ERR_LENGTH      = -7,  // reply payload had the wrong size
	PW_ERR_ARG         = -8,
	PW_ERR_NO_WALKER   = -9,  // no advertisement byte seen
	PW_ERR_CANCELLED   = -10,
	PW_ERR_VERIFY      = -11, // data read back after a write did not match
};

// Byte transport. Bytes are exactly what goes over the air (already XORed).
typedef struct pw_transport {
	// Send len bytes. Return 0 on success, negative on failure.
	int (*send)(void *ctx, const uint8_t *data, size_t len);
	// Receive one burst: wait up to first_ms for the first byte, then keep
	// reading until the line has been idle for a few milliseconds.
	// Return the byte count, 0 on timeout, negative on failure.
	int (*recv)(void *ctx, uint8_t *buf, size_t cap, uint32_t first_ms);
	void *ctx;
} pw_transport;

// Optional observer for diagnostics: called with the decoded (un-XORed)
// bytes of every packet sent (dir 0) and every burst received (dir 1).
typedef void (*pw_trace_fn)(void *user, int dir, const uint8_t *data, size_t len, int status);

// Optional cancel poll: return non-zero to abort a long wait.
typedef int (*pw_cancel_fn)(void *user);

typedef struct pw_link {
	pw_transport io;
	uint8_t session[4];     // raw session bytes carried in every packet
	int connected;
	uint32_t reply_ms;      // how long to wait for a reply
	int read_attempts;      // tries per EEPROM read chunk (default 6)
	unsigned read_retries;  // statistics: chunk reads that had to be repeated
	pw_trace_fn trace;
	pw_cancel_fn cancel;
	void *user;
	// last received packet, decoded
	uint8_t rx[PW_MAX_PACKET + 8];
	int rx_len;
} pw_link;

// Identity data returned by 0x20 -> 0x22 (0x68 bytes). Multi-byte fields are
// decoded into host order by pw_identity_parse.
#define PW_IDENTITY_SIZE 0x68
typedef struct pw_identity {
	uint32_t unk0, unk1;
	uint16_t unk2, unk3;
	uint16_t tid, sid;
	uint8_t  unique_id[0x28];
	uint8_t  event_bitmap[0x10];
	uint16_t trainer_name[8];
	uint8_t  event_index;
	uint8_t  flags;            // 0x01 paired, 0x02 has Pokemon, 0x04 a Pokemon joined
	uint8_t  proto_ver, proto_subver;
	uint32_t last_sync;        // seconds since 2000-01-01 (see docs)
	uint32_t total_steps;
} pw_identity;

#define PW_IDFLAG_PAIRED    0x01
#define PW_IDFLAG_HAS_POKE  0x02
#define PW_IDFLAG_JOINED    0x04

// 16-byte Pokemon summary used all over the walker's EEPROM.
typedef struct pw_poke_summary {
	uint16_t species;
	uint16_t held_item;
	uint16_t moves[4];
	uint8_t  level;
	uint8_t  variant;   // low 5 bits form, 0x20 female
	uint8_t  flags;     // 0x01 has form, 0x02 shiny
} pw_poke_summary;

// EEPROM addresses used so far
#define PW_EE_ROUTE_INFO      0x8F00   // RouteInfo, 0xBE bytes
#define PW_EE_HEALTH_DATA     0x0156   // HealthData, 0x18 bytes (first copy)
#define PW_EE_CURRENT_WATTS   0xCE8A   // u16 big-endian, refreshed by 0x20

// --- packet helpers (exposed for tests) ------------------------------------
uint16_t pw_checksum(const uint8_t *packet, size_t len);
// Build a decoded packet into out (at least 8+payload_len bytes); returns its length.
size_t pw_build_packet(uint8_t *out, uint8_t cmd, uint8_t extra, const uint8_t session[4],
                       const uint8_t *payload, size_t payload_len);

// --- link ------------------------------------------------------------------
void pw_link_init(pw_link *l, const pw_transport *io);

// Wait up to wait_ms for the walker's advertisement, then do the FA/F8
// handshake. local_nonce is our half of the session id.
int pw_connect(pw_link *l, uint32_t wait_ms, uint32_t local_nonce);

// Send one command and receive its reply into l->rx. Returns the reply's
// payload length, or a negative error. expect_cmd < 0 skips the command check.
int pw_command(pw_link *l, uint8_t cmd, uint8_t extra, const uint8_t *payload, size_t payload_len,
               int expect_cmd);

int pw_get_identity(pw_link *l, pw_identity *out, uint8_t raw[PW_IDENTITY_SIZE]);
int pw_ping(pw_link *l);
// Read len bytes (any length) starting at addr; chunks of up to 128.
int pw_eeprom_read(pw_link *l, uint16_t addr, uint8_t *out, size_t len);
// Write len bytes (a multiple of 128) to a 128-byte-aligned address with raw
// 0x02 / 0x82 writes. Each block is retried on any error (a repeated write of
// the same bytes is harmless). With verify set, every block is read back and
// rewritten until it matches (PW_ERR_VERIFY if it never does).
int pw_eeprom_write(pw_link *l, uint16_t addr, const uint8_t *data, size_t len, int verify);

// Send our identity with one of 0x32 / 0x40 / 0x52 / 0x60; expects cmd + 2.
int pw_send_identity(pw_link *l, uint8_t cmd, const uint8_t identity[0x68]);

// One-shot actions with side effects: never repeated automatically.
int pw_walk_start(pw_link *l);          // 0x5A -> 0x5A
int pw_walk_start_pairing(pw_link *l);  // 0x38 -> 0x38
int pw_walk_end(pw_link *l);            // 0x4E -> 0x50
// Reset (0x2A) with a 0x28-byte unique id as payload, the way the game resets a walker (docs/PAIRING.md 3):
// clears trainer, pairing, watts, events, items, caught list; keeps images and lifetime steps. The game does
// not wait for a reply: returns PW_OK if any reply came, PW_ERR_TIMEOUT if none (the reset may still have
// happened - read the identity again to know).
int pw_walker_reset(pw_link *l, const uint8_t unique_id[0x28]);

// Tell the walker to drop the link (no reply expected).
int pw_disconnect(pw_link *l);

// --- decoding helpers ------------------------------------------------------
void pw_identity_parse(const uint8_t raw[PW_IDENTITY_SIZE], pw_identity *out);
void pw_poke_summary_parse(const uint8_t raw[16], pw_poke_summary *out);
// Gen 4 16-bit text -> ASCII (letters, digits, common punctuation; '?' otherwise).
void pw_text_to_ascii(const uint16_t *text, size_t max_chars, char *out, size_t out_size);
// ASCII (letters, digits, space) -> Gen 4 16-bit text, padded with 0xFFFF up
// to max_chars. Other characters are left out. Returns the number of characters.
size_t pw_text_from_ascii(const char *in, uint16_t *text, size_t max_chars);
const char *pw_strerror(int err);

#ifdef __cplusplus
}
#endif
