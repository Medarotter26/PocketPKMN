// PC-side test of the protocol core against a mock walker.
// Reference bytes come from a capture of a real HeartGold/SoulSilver talking
// to a walker (pokewalker-ir-linux walk_start.log): the game's handshake
// packet, the walker's nonce, and the game's identity request.

#include "pw_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

// --- mock walker -----------------------------------------------------------

typedef struct mock {
	uint8_t eeprom[PW_EEPROM_SIZE];
	uint8_t identity[PW_IDENTITY_SIZE];
	uint8_t session[4];
	int connected;
	uint8_t out[PW_MAX_PACKET];   // pending reply, wire-encoded
	size_t out_len;
	int corrupt_next_read;        // flip a byte in the next 0x0E reply
	int corrupt_identity;         // flip a byte in the next N 0x22 replies
	int adverts_before_listen;    // ignore the first N handshakes
	int fail_sends;               // next N sends report a transport (bus) error
	int drop_replies;             // next N replies are lost (timeout)
	uint8_t last_tx[PW_MAX_PACKET];
	size_t last_tx_len;
	int disconnects;
	int writes, spoil_next_write, walk_starts, walk_ends;
	uint8_t identity_rx[PW_IDENTITY_SIZE];
} mock;

static const uint8_t WALKER_NONCE[4] = { 0xde, 0xad, 0xbe, 0xef };

static void mock_reply(mock *m, uint8_t cmd, uint8_t extra, const uint8_t *session,
                       const uint8_t *payload, size_t len)
{
	m->out_len = pw_build_packet(m->out, cmd, extra, session, payload, len);
	for (size_t i = 0; i < m->out_len; i++)
		m->out[i] ^= PW_XOR_KEY;
}

static int mock_send(void *ctx, const uint8_t *data, size_t len)
{
	mock *m = ctx;
	uint8_t p[PW_MAX_PACKET];
	if (m->fail_sends > 0) {
		m->fail_sends--;
		return -1;
	}
	if (len < PW_HEADER_SIZE || len > PW_MAX_PACKET)
		return 0; // a real walker just ignores junk
	for (size_t i = 0; i < len; i++)
		p[i] = data[i] ^ PW_XOR_KEY;
	memcpy(m->last_tx, p, len);
	m->last_tx_len = len;

	uint16_t sum = (uint16_t)(p[2] | (p[3] << 8));
	if (sum != pw_checksum(p, len))
		return 0;

	if (p[0] == PW_CMD_SYN) {
		if (m->adverts_before_listen > 0) {
			m->adverts_before_listen--;
			return 0;
		}
		for (int i = 0; i < 4; i++)
			m->session[i] = p[4 + i] ^ WALKER_NONCE[i];
		m->connected = 1;
		mock_reply(m, PW_CMD_SYNACK, PW_EXTRA_SLAVE, WALKER_NONCE, NULL, 0);
		return 0;
	}
	if (!m->connected || memcmp(p + 4, m->session, 4) != 0)
		return 0;

	switch (p[0]) {
	case PW_CMD_IDENTITY_REQ:
		mock_reply(m, PW_CMD_IDENTITY_REPLY, PW_EXTRA_SLAVE, m->session, m->identity, PW_IDENTITY_SIZE);
		if (m->corrupt_identity > 0) {
			m->corrupt_identity--;
			m->out[PW_HEADER_SIZE + 5] ^= 0x04;
		}
		break;
	case PW_CMD_PING:
		mock_reply(m, PW_CMD_PONG, PW_EXTRA_SLAVE, m->session, NULL, 0);
		break;
	case PW_CMD_EEPROM_READ: {
		uint16_t addr = (uint16_t)((p[8] << 8) | p[9]);
		uint8_t n = p[10];
		mock_reply(m, PW_CMD_EEPROM_READ_REPLY, PW_EXTRA_SLAVE, m->session, m->eeprom + addr, n);
		if (m->corrupt_next_read) {
			m->corrupt_next_read = 0;
			m->out[PW_HEADER_SIZE + 1] ^= 0x10;
		}
		break;
	}
	case PW_CMD_EEPROM_WRITE_RAW_LO:
	case PW_CMD_EEPROM_WRITE_RAW_HI: {
		if (len != PW_HEADER_SIZE + 128)
			break;
		uint16_t addr = (uint16_t)((p[1] << 8) | (p[0] == PW_CMD_EEPROM_WRITE_RAW_HI ? 0x80 : 0));
		memcpy(m->eeprom + addr, p + PW_HEADER_SIZE, 128);
		m->writes++;
		if (m->spoil_next_write) {   // the write "lands" wrong once
			m->spoil_next_write = 0;
			m->eeprom[addr + 7] ^= 0x01;
		}
		mock_reply(m, PW_CMD_EEPROM_WRITE_ACK, PW_EXTRA_SLAVE, m->session, NULL, 0);
		break;
	}
	case PW_CMD_IDENTITY_PAIR:
	case PW_CMD_IDENTITY_RETURN:
	case PW_CMD_IDENTITY_SEND:
	case 0x60:
		if (len == PW_HEADER_SIZE + PW_IDENTITY_SIZE)
			memcpy(m->identity_rx, p + PW_HEADER_SIZE, PW_IDENTITY_SIZE);
		mock_reply(m, (uint8_t)(p[0] + 2), PW_EXTRA_SLAVE, m->session, NULL, 0);
		break;
	case PW_CMD_WALK_START:
	case PW_CMD_WALK_START_PAIR:
		m->walk_starts++;
		mock_reply(m, p[0], PW_EXTRA_SLAVE, m->session, NULL, 0);
		break;
	case PW_CMD_WALK_END:
		m->walk_ends++;
		mock_reply(m, 0x50, PW_EXTRA_SLAVE, m->session, NULL, 0);
		break;
	case PW_CMD_DISCONNECT:
		m->connected = 0;
		m->disconnects++;
		break;
	}
	return 0;
}

static int mock_recv(void *ctx, uint8_t *buf, size_t cap, uint32_t first_ms)
{
	mock *m = ctx;
	(void)first_ms;
	if (m->out_len && m->connected && m->drop_replies > 0) {
		m->drop_replies--;
		m->out_len = 0;
		return 0;
	}
	if (m->out_len) {
		size_t n = m->out_len < cap ? m->out_len : cap;
		memcpy(buf, m->out, n);
		m->out_len = 0;
		return (int)n;
	}
	if (!m->connected) {
		buf[0] = PW_ADVERT_BYTE ^ PW_XOR_KEY; // 0x56 on the wire
		return 1;
	}
	return 0; // silence
}

static int trace_count;
static void count_trace(void *user, int dir, const uint8_t *d, size_t len, int status)
{
	(void)user; (void)dir; (void)d; (void)len; (void)status;
	trace_count++;
}

// --- tests -----------------------------------------------------------------

static void test_checksum_vectors(void)
{
	const uint8_t syn[] = { 0xfa, 0x01, 0x35, 0x99, 0x57, 0x2e, 0x48, 0x03 };
	const uint8_t idreq[] = { 0x20, 0x01, 0x73, 0xa0, 0x89, 0x83, 0xf6, 0xec };
	CHECK(pw_checksum(syn, sizeof(syn)) == 0x9935);
	CHECK(pw_checksum(idreq, sizeof(idreq)) == 0xA073);

	uint8_t out[PW_MAX_PACKET];
	const uint8_t nonce[4] = { 0x57, 0x2e, 0x48, 0x03 };
	size_t n = pw_build_packet(out, PW_CMD_SYN, PW_EXTRA_MASTER, nonce, NULL, 0);
	CHECK(n == 8 && memcmp(out, syn, 8) == 0);
}

static void fill_identity(uint8_t *id)
{
	memset(id, 0, PW_IDENTITY_SIZE);
	id[0x00] = 1;                       // unk0
	id[0x08] = 8;                       // unk2
	id[0x0C] = 0x41; id[0x0D] = 0x97;   // TID 0x9741
	id[0x0E] = 0x48; id[0x0F] = 0x97;   // SID 0x9748
	for (int i = 0; i < 0x28; i++) id[0x10 + i] = (uint8_t)(i + 1);
	const uint16_t name[] = { 0x0137, 0x0145, 0x0151, 0x0146, 0x0145, 0xFFFF }; // "Mamba"
	for (int i = 0; i < 6; i++) { id[0x48 + 2 * i] = name[i] & 0xFF; id[0x49 + 2 * i] = name[i] >> 8; }
	id[0x5B] = PW_IDFLAG_PAIRED | PW_IDFLAG_HAS_POKE;
	id[0x5F] = 2;
	id[0x60] = 0x2b; id[0x61] = 0x80; id[0x62] = 0x23; id[0x63] = 0x8c; // last sync, big-endian
	id[0x64] = 0x00; id[0x65] = 0x01; id[0x66] = 0xE2; id[0x67] = 0x40; // 123456 steps
}

static void test_session_and_identity(void)
{
	static mock m;
	memset(&m, 0, sizeof(m));
	fill_identity(m.identity);
	m.adverts_before_listen = 2; // make the first two handshakes fail

	pw_transport io = { mock_send, mock_recv, &m };
	pw_link l;
	pw_link_init(&l, &io);
	l.trace = count_trace;

	CHECK(pw_connect(&l, 2000, 0x572e4803) == PW_OK);
	CHECK(l.connected);
	const uint8_t want_session[4] = { 0x89, 0x83, 0xf6, 0xec };
	CHECK(memcmp(l.session, want_session, 4) == 0);

	pw_identity id;
	uint8_t raw[PW_IDENTITY_SIZE];
	m.corrupt_identity = 2; // hardware test 3: the first identity reply had a bad checksum
	CHECK(pw_get_identity(&l, &id, raw) == PW_OK);
	CHECK(l.read_retries == 2);
	// The request we sent must match the game's captured bytes exactly.
	const uint8_t idreq[] = { 0x20, 0x01, 0x73, 0xa0, 0x89, 0x83, 0xf6, 0xec };
	CHECK(m.last_tx_len == 8 && memcmp(m.last_tx, idreq, 8) == 0);

	CHECK(id.unk0 == 1 && id.unk2 == 8);
	CHECK(id.tid == 0x9741 && id.sid == 0x9748);
	CHECK(id.unique_id[0] == 1 && id.unique_id[0x27] == 0x28);
	CHECK(id.flags == (PW_IDFLAG_PAIRED | PW_IDFLAG_HAS_POKE));
	CHECK(id.last_sync == 0x2b80238c);
	CHECK(id.total_steps == 123456);
	char name[16];
	pw_text_to_ascii(id.trainer_name, 8, name, sizeof(name));
	CHECK(strcmp(name, "Mamba") == 0);
	uint16_t enc[8];
	CHECK(pw_text_from_ascii("Ma-mba", enc, 8) == 5);
	CHECK(memcmp(enc, id.trainer_name, 5 * 2) == 0 && enc[5] == 0xFFFF && enc[7] == 0xFFFF);
	CHECK(memcmp(raw, m.identity, PW_IDENTITY_SIZE) == 0);

	CHECK(pw_ping(&l) == PW_OK);
	CHECK(trace_count > 0);

	CHECK(pw_disconnect(&l) == PW_OK);
	CHECK(m.disconnects == 1 && !l.connected);
}

static void test_eeprom_read(void)
{
	static mock m;
	memset(&m, 0, sizeof(m));
	for (size_t i = 0; i < PW_EEPROM_SIZE; i++)
		m.eeprom[i] = (uint8_t)((i * 7) ^ (i >> 8));

	pw_transport io = { mock_send, mock_recv, &m };
	pw_link l;
	pw_link_init(&l, &io);
	CHECK(pw_connect(&l, 1000, 0) == PW_OK);

	// Odd start, odd length, crossing chunk boundaries; one reply gets corrupted.
	static uint8_t buf[PW_EEPROM_SIZE];
	m.corrupt_next_read = 1;
	CHECK(pw_eeprom_read(&l, 0x8F03, buf, 0x1BE) == PW_OK);
	CHECK(memcmp(buf, m.eeprom + 0x8F03, 0x1BE) == 0);

	// Whole EEPROM in two halves (the address is 16-bit).
	CHECK(pw_eeprom_read(&l, 0x0000, buf, 0x8000) == PW_OK);
	CHECK(pw_eeprom_read(&l, 0x8000, buf + 0x8000, 0x8000) == PW_OK);
	CHECK(memcmp(buf, m.eeprom, PW_EEPROM_SIZE) == 0);

	// Bus errors on send and lost replies are retried (as seen on a New 3DS XL).
	l.read_retries = 0;
	memset(buf, 0, 0x100);
	m.fail_sends = 2;
	m.drop_replies = 3;
	CHECK(pw_eeprom_read(&l, 0x1000, buf, 0x80) == PW_OK);
	CHECK(memcmp(buf, m.eeprom + 0x1000, 0x80) == 0);
	CHECK(l.read_retries == 5);
	// ...but not forever: six failures in a row end the read.
	m.drop_replies = 6;
	CHECK(pw_eeprom_read(&l, 0x1000, buf, 0x80) == PW_ERR_TIMEOUT);
	m.drop_replies = 0;
	m.fail_sends = 6;
	CHECK(pw_eeprom_read(&l, 0x1000, buf, 0x80) == PW_ERR_TRANSPORT);
	m.fail_sends = 0;
	CHECK(pw_eeprom_read(&l, 0x1000, buf, 0x80) == PW_OK);

	// Summary parse
	const uint8_t raw[16] = { 0x9B, 0x00, 0x00, 0x00, 0x21, 0x00, 0x2B, 0x00, 0, 0, 0, 0, 5, 0x20, 0x02, 0 };
	pw_poke_summary s;
	pw_poke_summary_parse(raw, &s);
	CHECK(s.species == 155 && s.moves[0] == 33 && s.moves[1] == 43 && s.level == 5);
	CHECK((s.variant & 0x20) && (s.flags & 0x02));
}

static void test_writes(void)
{
	static mock m;
	memset(&m, 0, sizeof(m));
	pw_transport io = { mock_send, mock_recv, &m };
	pw_link l;
	pw_link_init(&l, &io);
	CHECK(pw_connect(&l, 1000, 7) == PW_OK);

	static uint8_t data[0x400];
	for (size_t i = 0; i < sizeof(data); i++)
		data[i] = (uint8_t)(i * 13 + 5);

	// Alignment is enforced.
	CHECK(pw_eeprom_write(&l, 0xD701, data, 0x80, 0) == PW_ERR_ARG);
	CHECK(pw_eeprom_write(&l, 0xD700, data, 0x81, 0) == PW_ERR_ARG);
	CHECK(pw_eeprom_write(&l, 0xFF80, data, 0x100, 0) == PW_ERR_ARG);

	// Low and high halves of a page use 0x02 / 0x82; data lands where it should.
	CHECK(pw_eeprom_write(&l, 0xD700, data, 0x400, 0) == PW_OK);
	CHECK(memcmp(m.eeprom + 0xD700, data, 0x400) == 0);
	CHECK(m.writes == 8);

	// A lost ack and a send error are retried.
	m.drop_replies = 1;
	m.fail_sends = 1;
	CHECK(pw_eeprom_write(&l, 0x1000, data, 0x80, 0) == PW_OK);
	CHECK(memcmp(m.eeprom + 0x1000, data, 0x80) == 0);

	// Verify catches a block that landed wrong and writes it again.
	m.writes = 0;
	m.spoil_next_write = 1;
	CHECK(pw_eeprom_write(&l, 0x2000, data, 0x100, 1) == PW_OK);
	CHECK(memcmp(m.eeprom + 0x2000, data, 0x100) == 0);
	CHECK(m.writes == 3);

	// Identity, walk start / end.
	uint8_t ident[PW_IDENTITY_SIZE];
	fill_identity(ident);
	CHECK(pw_send_identity(&l, PW_CMD_IDENTITY_SEND, ident) == PW_OK);
	CHECK(memcmp(m.identity_rx, ident, PW_IDENTITY_SIZE) == 0);
	CHECK(pw_walk_start(&l) == PW_OK && m.walk_starts == 1);
	CHECK(pw_walk_start_pairing(&l) == PW_OK && m.walk_starts == 2);
	CHECK(pw_walk_end(&l) == PW_OK && m.walk_ends == 1);
	// One-shot commands are not repeated on a lost reply.
	m.drop_replies = 1;
	CHECK(pw_walk_start(&l) == PW_ERR_TIMEOUT && m.walk_starts == 3);
}

static int silent_recv(void *ctx, uint8_t *buf, size_t cap, uint32_t first_ms)
{
	(void)ctx; (void)buf; (void)cap; (void)first_ms;
	return 0;
}

static void test_errors(void)
{
	static mock m;
	memset(&m, 0, sizeof(m));

	// Nobody there.
	pw_transport quiet = { mock_send, silent_recv, &m };
	pw_link l;
	pw_link_init(&l, &quiet);
	CHECK(pw_connect(&l, 300, 1) == PW_ERR_NO_WALKER);

	// Connected, then the walker goes silent.
	pw_transport io = { mock_send, mock_recv, &m };
	pw_link_init(&l, &io);
	CHECK(pw_connect(&l, 1000, 1) == PW_OK);
	l.io.recv = silent_recv;
	CHECK(pw_ping(&l) == PW_ERR_TIMEOUT);

	// A reply with a different session id is rejected.
	l.io.recv = mock_recv;
	l.session[0] ^= 0xFF;
	m.session[0] ^= 0xFF;     // walker follows, so it answers...
	CHECK(pw_ping(&l) == PW_OK);
	l.session[1] ^= 0xFF;     // ...but now only we change
	CHECK(pw_ping(&l) == PW_ERR_TIMEOUT); // walker ignores a foreign session
}

int main(void)
{
	test_checksum_vectors();
	test_session_and_identity();
	test_eeprom_read();
	test_writes();
	test_errors();
	if (failures) {
		printf("%d check(s) FAILED\n", failures);
		return 1;
	}
	printf("all protocol tests passed\n");
	return 0;
}
