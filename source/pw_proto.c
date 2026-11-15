// Pokewalker IR protocol, master (game) side. See pw_proto.h.

#include "pw_proto.h"

#include <string.h>

// --- packet helpers --------------------------------------------------------

// Checksum over header + payload with bytes 2..3 treated as zero:
// seed 2, even-index bytes added to the high byte, odd-index bytes to the low
// byte, carries folded back in. Stored little-endian at bytes 2..3.
uint16_t pw_checksum(const uint8_t *packet, size_t len)
{
	uint32_t sum = 0x0002;
	for (size_t i = 0; i < len; i++) {
		uint8_t b = (i == 2 || i == 3) ? 0 : packet[i];
		sum += (i & 1) ? b : ((uint32_t)b << 8);
	}
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return (uint16_t)sum;
}

size_t pw_build_packet(uint8_t *out, uint8_t cmd, uint8_t extra, const uint8_t session[4],
                       const uint8_t *payload, size_t payload_len)
{
	out[0] = cmd;
	out[1] = extra;
	out[2] = 0;
	out[3] = 0;
	memcpy(out + 4, session, 4);
	if (payload_len)
		memcpy(out + PW_HEADER_SIZE, payload, payload_len);
	size_t len = PW_HEADER_SIZE + payload_len;
	uint16_t sum = pw_checksum(out, len);
	out[2] = (uint8_t)(sum & 0xFF);
	out[3] = (uint8_t)(sum >> 8);
	return len;
}

static void xor_buf(uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
		buf[i] ^= PW_XOR_KEY;
}

// --- link ------------------------------------------------------------------

void pw_link_init(pw_link *l, const pw_transport *io)
{
	memset(l, 0, sizeof(*l));
	l->io = *io;
	// Replies come 5-40 ms after our packet (hardware tests), and a real walker
	// drops a link that is silent for well under 0.4 s - so a lost packet must
	// be retried quickly or the whole link is lost (test 7).
	l->reply_ms = 100;
	l->read_attempts = 6;
}

static void trace(pw_link *l, int dir, const uint8_t *data, size_t len, int status)
{
	if (l->trace)
		l->trace(l->user, dir, data, len, status);
}

static int send_packet(pw_link *l, uint8_t cmd, uint8_t extra, const uint8_t session[4],
                       const uint8_t *payload, size_t payload_len)
{
	uint8_t pkt[PW_MAX_PACKET];
	if (payload_len > PW_MAX_PAYLOAD)
		return PW_ERR_ARG;
	size_t len = pw_build_packet(pkt, cmd, extra, session, payload, payload_len);
	trace(l, 0, pkt, len, 0);
	xor_buf(pkt, len);
	if (l->io.send(l->io.ctx, pkt, len) < 0)
		return PW_ERR_TRANSPORT;
	return PW_OK;
}

// Receive one burst into l->rx (decoded). Returns its length or an error.
static int recv_burst(pw_link *l, uint32_t first_ms)
{
	int n = l->io.recv(l->io.ctx, l->rx, sizeof(l->rx), first_ms);
	if (n < 0) {
		l->rx_len = 0;
		trace(l, 1, l->rx, 0, PW_ERR_TRANSPORT);
		return PW_ERR_TRANSPORT;
	}
	l->rx_len = n;
	xor_buf(l->rx, (size_t)n);
	if (n == 0)
		return PW_ERR_TIMEOUT;
	return n;
}

// Check the packet in l->rx: size, checksum and (optionally) session.
static int validate_rx(pw_link *l, int check_session)
{
	if (l->rx_len < PW_HEADER_SIZE)
		return PW_ERR_SHORT;
	if (l->rx_len > PW_MAX_PACKET)
		return PW_ERR_LENGTH;
	uint16_t want = pw_checksum(l->rx, (size_t)l->rx_len);
	uint16_t got = (uint16_t)(l->rx[2] | (l->rx[3] << 8));
	if (want != got)
		return PW_ERR_CHECKSUM;
	if (check_session && memcmp(l->rx + 4, l->session, 4) != 0)
		return PW_ERR_SESSION;
	return PW_OK;
}

int pw_connect(pw_link *l, uint32_t wait_ms, uint32_t local_nonce)
{
	const uint32_t slice_ms = 100;
	uint32_t tries = wait_ms / slice_ms + 1;
	uint8_t nonce[4] = {
		(uint8_t)(local_nonce >> 24), (uint8_t)(local_nonce >> 16),
		(uint8_t)(local_nonce >> 8), (uint8_t)local_nonce,
	};
	int last = PW_ERR_NO_WALKER;

	l->connected = 0;
	for (uint32_t t = 0; t < tries; t++) {
		if (l->cancel && l->cancel(l->user))
			return PW_ERR_CANCELLED;

		int n = recv_burst(l, slice_ms);
		if (n == PW_ERR_TRANSPORT)
			return n;
		if (n <= 0)
			continue;
		trace(l, 1, l->rx, (size_t)n, 0);

		// The walker's only non-packet transmission is the bare advert byte.
		// Accept a burst whose last byte is the advert (several may have run together).
		if (l->rx[n - 1] != PW_ADVERT_BYTE)
			continue;

		int r = send_packet(l, PW_CMD_SYN, PW_EXTRA_MASTER, nonce, NULL, 0);
		if (r < 0)
			return r;

		n = recv_burst(l, l->reply_ms);
		if (n == PW_ERR_TRANSPORT)
			return n;
		if (n <= 0) {
			trace(l, 1, l->rx, 0, PW_ERR_TIMEOUT);
			last = PW_ERR_TIMEOUT;
			continue;
		}
		r = validate_rx(l, 0);
		if (r == PW_OK && l->rx[0] != PW_CMD_SYNACK)
			r = PW_ERR_UNEXPECTED;
		trace(l, 1, l->rx, (size_t)n, r);
		if (r < 0) {
			last = r;
			continue;
		}
		for (int i = 0; i < 4; i++)
			l->session[i] = (uint8_t)(nonce[i] ^ l->rx[4 + i]);
		l->connected = 1;
		return PW_OK;
	}
	return last;
}

int pw_command(pw_link *l, uint8_t cmd, uint8_t extra, const uint8_t *payload, size_t payload_len,
               int expect_cmd)
{
	int r = send_packet(l, cmd, extra, l->session, payload, payload_len);
	if (r < 0)
		return r;

	int n = recv_burst(l, l->reply_ms);
	if (n <= 0) {
		if (n != PW_ERR_TRANSPORT)
			trace(l, 1, l->rx, 0, PW_ERR_TIMEOUT);
		return n ? n : PW_ERR_TIMEOUT;
	}
	r = validate_rx(l, 1);
	if (r == PW_OK && expect_cmd >= 0 && l->rx[0] != (uint8_t)expect_cmd)
		r = PW_ERR_UNEXPECTED;
	trace(l, 1, l->rx, (size_t)n, r);
	if (r < 0)
		return r;
	return n - PW_HEADER_SIZE;
}

int pw_get_identity(pw_link *l, pw_identity *out, uint8_t raw[PW_IDENTITY_SIZE])
{
	// Safe to repeat (the game itself sends 0x20 at every connection), so a
	// damaged reply is simply asked for again, like an EEPROM read.
	int n = PW_ERR_TIMEOUT;
	int attempts = l->read_attempts > 0 ? l->read_attempts : 1;
	for (int attempt = 0; attempt < attempts; attempt++) {
		if (attempt) {
			l->read_retries++;
			if (l->cancel && l->cancel(l->user))
				return PW_ERR_CANCELLED;
		}
		n = pw_command(l, PW_CMD_IDENTITY_REQ, PW_EXTRA_MASTER, NULL, 0, PW_CMD_IDENTITY_REPLY);
		if (n >= 0 && n != PW_IDENTITY_SIZE)
			n = PW_ERR_LENGTH;
		if (n >= 0)
			break;
	}
	if (n < 0)
		return n;
	if (raw)
		memcpy(raw, l->rx + PW_HEADER_SIZE, PW_IDENTITY_SIZE);
	if (out)
		pw_identity_parse(l->rx + PW_HEADER_SIZE, out);
	return PW_OK;
}

int pw_ping(pw_link *l)
{
	int n = PW_ERR_TIMEOUT;
	for (int attempt = 0; attempt < 3; attempt++) {
		n = pw_command(l, PW_CMD_PING, PW_EXTRA_MASTER, NULL, 0, PW_CMD_PONG);
		if (n >= 0)
			break;
	}
	return n < 0 ? n : PW_OK;
}

int pw_eeprom_read(pw_link *l, uint16_t addr, uint8_t *out, size_t len)
{
	size_t done = 0;
	while (done < len) {
		if (l->cancel && l->cancel(l->user))
			return PW_ERR_CANCELLED;

		size_t chunk = len - done;
		if (chunk > PW_MAX_PAYLOAD)
			chunk = PW_MAX_PAYLOAD;
		uint16_t a = (uint16_t)(addr + done);
		uint8_t req[3] = { (uint8_t)(a >> 8), (uint8_t)a, (uint8_t)chunk };

		// Reads have no side effects, so a garbled reply, a lost reply or a
		// failed send (3DS-side bus error) can simply be tried again.
		int n = PW_ERR_TIMEOUT;
		int attempts = l->read_attempts > 0 ? l->read_attempts : 1;
		for (int attempt = 0; attempt < attempts; attempt++) {
			if (attempt) {
				l->read_retries++;
				if (l->cancel && l->cancel(l->user))
					return PW_ERR_CANCELLED;
			}
			n = pw_command(l, PW_CMD_EEPROM_READ, PW_EXTRA_MASTER, req, sizeof(req),
			               PW_CMD_EEPROM_READ_REPLY);
			if (n >= 0 && (size_t)n != chunk)
				n = PW_ERR_LENGTH;
			if (n >= 0)
				break;
		}
		if (n < 0)
			return n;
		memcpy(out + done, l->rx + PW_HEADER_SIZE, chunk);
		done += chunk;
	}
	return PW_OK;
}

int pw_eeprom_write(pw_link *l, uint16_t addr, const uint8_t *data, size_t len, int verify)
{
	if ((addr & 0x7F) || (len & 0x7F) || (size_t)addr + len > PW_EEPROM_SIZE)
		return PW_ERR_ARG;
	int attempts = l->read_attempts > 0 ? l->read_attempts : 1;
	uint8_t back[PW_MAX_PAYLOAD];

	for (size_t done = 0; done < len; done += PW_MAX_PAYLOAD) {
		uint16_t a = (uint16_t)(addr + done);
		uint8_t cmd = (a & 0x80) ? PW_CMD_EEPROM_WRITE_RAW_HI : PW_CMD_EEPROM_WRITE_RAW_LO;
		int r = PW_ERR_TIMEOUT;
		for (int attempt = 0; attempt < attempts; attempt++) {
			if (attempt) {
				l->read_retries++;
				if (l->cancel && l->cancel(l->user))
					return PW_ERR_CANCELLED;
			}
			int n = pw_command(l, cmd, (uint8_t)(a >> 8), data + done, PW_MAX_PAYLOAD,
			                   PW_CMD_EEPROM_WRITE_ACK);
			if (n < 0) {
				r = n;
				continue;
			}
			if (!verify) {
				r = PW_OK;
				break;
			}
			r = pw_eeprom_read(l, a, back, PW_MAX_PAYLOAD);
			if (r < 0)
				return r; // reads already retried; the link is gone
			if (memcmp(back, data + done, PW_MAX_PAYLOAD) == 0)
				break;
			r = PW_ERR_VERIFY;
		}
		if (r < 0)
			return r;
	}
	return PW_OK;
}

int pw_send_identity(pw_link *l, uint8_t cmd, const uint8_t identity[0x68])
{
	int n = PW_ERR_TIMEOUT;
	for (int attempt = 0; attempt < 3; attempt++) {
		n = pw_command(l, cmd, PW_EXTRA_MASTER, identity, PW_IDENTITY_SIZE, cmd + 2);
		if (n >= 0)
			break;
	}
	return n < 0 ? n : PW_OK;
}

static int one_shot(pw_link *l, uint8_t cmd, uint8_t expect)
{
	// Never repeated, and the walker may do work before answering: wait longer.
	uint32_t saved = l->reply_ms;
	l->reply_ms = 1500;
	int n = pw_command(l, cmd, PW_EXTRA_MASTER, NULL, 0, expect);
	l->reply_ms = saved;
	return n < 0 ? n : PW_OK;
}

int pw_walk_start(pw_link *l)         { return one_shot(l, PW_CMD_WALK_START, PW_CMD_WALK_START); }
int pw_walk_start_pairing(pw_link *l) { return one_shot(l, PW_CMD_WALK_START_PAIR, PW_CMD_WALK_START_PAIR); }
int pw_walk_end(pw_link *l)           { return one_shot(l, PW_CMD_WALK_END, 0x50); }

int pw_walker_reset(pw_link *l, const uint8_t unique_id[0x28])
{
	uint32_t saved = l->reply_ms;
	l->reply_ms = 1500;
	int n = pw_command(l, 0x2A, PW_EXTRA_MASTER, unique_id, 0x28, -1); // never repeated
	l->reply_ms = saved;
	return n < 0 ? n : PW_OK;
}

int pw_disconnect(pw_link *l)
{
	int r = send_packet(l, PW_CMD_DISCONNECT, PW_EXTRA_MASTER, l->session, NULL, 0);
	l->connected = 0;
	return r;
}

// --- decoding helpers ------------------------------------------------------

static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t rd32be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void pw_identity_parse(const uint8_t raw[PW_IDENTITY_SIZE], pw_identity *out)
{
	memset(out, 0, sizeof(*out));
	out->unk0 = rd32le(raw + 0x00);
	out->unk1 = rd32le(raw + 0x04);
	out->unk2 = rd16le(raw + 0x08);
	out->unk3 = rd16le(raw + 0x0A);
	out->tid = rd16le(raw + 0x0C);
	out->sid = rd16le(raw + 0x0E);
	memcpy(out->unique_id, raw + 0x10, 0x28);
	memcpy(out->event_bitmap, raw + 0x38, 0x10);
	for (int i = 0; i < 8; i++)
		out->trainer_name[i] = rd16le(raw + 0x48 + 2 * i);
	out->event_index = raw[0x5A];
	out->flags = raw[0x5B];
	out->proto_ver = raw[0x5C];
	out->proto_subver = raw[0x5E];
	out->last_sync = rd32be(raw + 0x60);
	out->total_steps = rd32be(raw + 0x64);
}

void pw_poke_summary_parse(const uint8_t raw[16], pw_poke_summary *out)
{
	out->species = rd16le(raw + 0);
	out->held_item = rd16le(raw + 2);
	for (int i = 0; i < 4; i++)
		out->moves[i] = rd16le(raw + 4 + 2 * i);
	out->level = raw[0x0C];
	out->variant = raw[0x0D];
	out->flags = raw[0x0E];
}

void pw_text_to_ascii(const uint16_t *text, size_t max_chars, char *out, size_t out_size)
{
	size_t o = 0;
	if (out_size == 0)
		return;
	for (size_t i = 0; i < max_chars && o + 1 < out_size; i++) {
		uint16_t c = text[i];
		if (c == 0xFFFF || c == 0x0000)
			break;
		char ch = '?';
		if (c >= 0x0121 && c <= 0x012A)
			ch = (char)('0' + (c - 0x0121));
		else if (c >= 0x012B && c <= 0x0144)
			ch = (char)('A' + (c - 0x012B));
		else if (c >= 0x0145 && c <= 0x015E)
			ch = (char)('a' + (c - 0x0145));
		else if (c == 0x01DE)
			ch = ' ';
		out[o++] = ch;
	}
	out[o] = '\0';
}

size_t pw_text_from_ascii(const char *in, uint16_t *text, size_t max_chars)
{
	size_t n = 0;
	for (; *in && n < max_chars; in++) {
		char c = *in;
		uint16_t code;
		if (c >= '0' && c <= '9')
			code = (uint16_t)(0x0121 + (c - '0'));
		else if (c >= 'A' && c <= 'Z')
			code = (uint16_t)(0x012B + (c - 'A'));
		else if (c >= 'a' && c <= 'z')
			code = (uint16_t)(0x0145 + (c - 'a'));
		else if (c == ' ')
			code = 0x01DE;
		else
			continue; // characters we cannot map are left out
		text[n++] = code;
	}
	size_t len = n;
	while (n < max_chars)
		text[n++] = 0xFFFF;
	return len;
}

const char *pw_strerror(int err)
{
	switch (err) {
	case PW_OK:             return "ok";
	case PW_ERR_TRANSPORT:  return "IR hardware error";
	case PW_ERR_TIMEOUT:    return "no reply (timeout)";
	case PW_ERR_SHORT:      return "reply too short";
	case PW_ERR_CHECKSUM:   return "bad checksum";
	case PW_ERR_SESSION:    return "wrong session id";
	case PW_ERR_UNEXPECTED: return "unexpected reply command";
	case PW_ERR_LENGTH:     return "wrong reply length";
	case PW_ERR_ARG:        return "bad argument";
	case PW_ERR_NO_WALKER:  return "no walker found";
	case PW_ERR_CANCELLED:  return "cancelled";
	case PW_ERR_VERIFY:     return "read-back did not match";
	default:                return "unknown error";
	}
}
