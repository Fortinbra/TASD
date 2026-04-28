/*
 * tasd.c — TASD file format library implementation
 *
 * See tasd.h for the public API and documentation.
 */

#include "tasd.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Internal helpers — endian-safe integer I/O
 * -------------------------------------------------------------------------- */

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
            (uint32_t)p[3];
}

static uint64_t read_be64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56) |
           ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) |
           ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) |
           ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] <<  8) |
            (uint64_t)p[7];
}

/* Read n-byte big-endian unsigned integer (n <= 8). */
static uint64_t read_be_n(const uint8_t *p, uint8_t n)
{
    uint64_t v = 0;
    uint8_t  i;
    for (i = 0; i < n; i++) {
        v = (v << 8) | p[i];
    }
    return v;
}

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

static void write_be64(uint8_t *p, uint64_t v)
{
    p[0] = (uint8_t)(v >> 56);
    p[1] = (uint8_t)(v >> 48);
    p[2] = (uint8_t)(v >> 40);
    p[3] = (uint8_t)(v >> 32);
    p[4] = (uint8_t)(v >> 24);
    p[5] = (uint8_t)(v >> 16);
    p[6] = (uint8_t)(v >>  8);
    p[7] = (uint8_t)(v);
}

/* Write value into n bytes, big-endian (n <= 4). */
static void write_be_n(uint8_t *p, uint32_t v, uint8_t n)
{
    uint8_t i;
    for (i = n; i-- > 0; ) {
        p[i] = (uint8_t)(v & 0xFFu);
        v >>= 8;
    }
}

/* Return the minimum number of bytes needed to represent value. */
static uint8_t min_pexp(uint32_t v)
{
    if (v <= 0xFFu)       return 1u;
    if (v <= 0xFFFFu)     return 2u;
    if (v <= 0xFFFFFFu)   return 3u;
    return 4u;
}

/* --------------------------------------------------------------------------
 * Header
 * -------------------------------------------------------------------------- */

tasd_result_t tasd_read_header(const uint8_t *buf, size_t len,
                                tasd_header_t *out)
{
    if (!buf || !out) {
        return TASD_ERR_INVALID_PAYLOAD;
    }
    if (len < TASD_HEADER_SIZE) {
        return TASD_ERR_TRUNCATED;
    }
    if (buf[0] != TASD_MAGIC_0 || buf[1] != TASD_MAGIC_1 ||
        buf[2] != TASD_MAGIC_2 || buf[3] != TASD_MAGIC_3) {
        return TASD_ERR_INVALID_MAGIC;
    }
    out->version  = read_be16(buf + 4);
    out->g_keylen = buf[6];

    if (out->version == 0 || out->g_keylen == 0) {
        return TASD_ERR_UNSUPPORTED;
    }
    return TASD_OK;
}

tasd_result_t tasd_write_header(uint8_t *buf, size_t len)
{
    if (!buf) {
        return TASD_ERR_INVALID_PAYLOAD;
    }
    if (len < TASD_HEADER_SIZE) {
        return TASD_ERR_BUFFER_SMALL;
    }
    buf[0] = TASD_MAGIC_0;
    buf[1] = TASD_MAGIC_1;
    buf[2] = TASD_MAGIC_2;
    buf[3] = TASD_MAGIC_3;
    write_be16(buf + 4, (uint16_t)TASD_VERSION);
    buf[6] = (uint8_t)TASD_G_KEYLEN;
    return TASD_OK;
}

/* --------------------------------------------------------------------------
 * Reader
 * -------------------------------------------------------------------------- */

void tasd_reader_init(tasd_reader_t *r, const uint8_t *buf, size_t len,
                      const tasd_header_t *hdr)
{
    r->buf      = buf;
    r->len      = len;
    r->pos      = TASD_HEADER_SIZE;
    r->g_keylen = hdr ? hdr->g_keylen : (uint8_t)TASD_G_KEYLEN;
}

tasd_result_t tasd_reader_next(tasd_reader_t *r, tasd_packet_t *out)
{
    uint8_t  pexp;
    uint64_t plen64;
    uint32_t plen;
    size_t   needed;

    if (!r || !out) {
        return TASD_ERR_INVALID_PAYLOAD;
    }
    if (r->pos >= r->len) {
        return TASD_ERR_END;
    }

    /* Need at least g_keylen + 1 bytes for key and PEXP. */
    needed = (size_t)r->g_keylen + 1u;
    if (r->len - r->pos < needed) {
        return TASD_ERR_TRUNCATED;
    }

    /* Key */
    if (r->g_keylen == 2u) {
        out->key = read_be16(r->buf + r->pos);
    } else {
        /* Future-proof: read g_keylen bytes as uint16 (truncate upper bits) */
        out->key = (uint16_t)(read_be_n(r->buf + r->pos, r->g_keylen) & 0xFFFFu);
    }
    r->pos += r->g_keylen;

    /* PEXP */
    pexp = r->buf[r->pos++];
    if (pexp == 0u) {
        return TASD_ERR_INVALID_PEXP;
    }

    /* PLEN */
    if (r->len - r->pos < (size_t)pexp) {
        return TASD_ERR_TRUNCATED;
    }
    plen64 = read_be_n(r->buf + r->pos, pexp);
    r->pos += pexp;

    /* Guard against extremely large lengths on platforms with small size_t */
    if (plen64 > (uint64_t)UINT32_MAX) {
        return TASD_ERR_TRUNCATED;
    }
    plen = (uint32_t)plen64;

    /* Payload */
    if (r->len - r->pos < (size_t)plen) {
        return TASD_ERR_TRUNCATED;
    }
    out->payload     = r->buf + r->pos;
    out->payload_len = plen;
    r->pos          += plen;

    return TASD_OK;
}

/* --------------------------------------------------------------------------
 * Writer
 * -------------------------------------------------------------------------- */

void tasd_writer_init(tasd_writer_t *w, uint8_t *buf, size_t len)
{
    w->buf = buf;
    w->len = len;
    w->pos = 0;
}

tasd_result_t tasd_writer_write_header(tasd_writer_t *w)
{
    tasd_result_t rc;
    if (!w || !w->buf) {
        return TASD_ERR_INVALID_PAYLOAD;
    }
    if (w->len - w->pos < TASD_HEADER_SIZE) {
        return TASD_ERR_BUFFER_SMALL;
    }
    rc = tasd_write_header(w->buf + w->pos, w->len - w->pos);
    if (rc == TASD_OK) {
        w->pos += TASD_HEADER_SIZE;
    }
    return rc;
}

tasd_result_t tasd_writer_append(tasd_writer_t *w, uint16_t key,
                                  const uint8_t *payload,
                                  uint32_t payload_len)
{
    uint8_t pexp;
    size_t  frame_size;

    if (!w || !w->buf) {
        return TASD_ERR_INVALID_PAYLOAD;
    }

    pexp       = min_pexp(payload_len);
    frame_size = (size_t)TASD_G_KEYLEN + 1u + (size_t)pexp + (size_t)payload_len;

    if (w->len - w->pos < frame_size) {
        return TASD_ERR_BUFFER_SMALL;
    }

    write_be16(w->buf + w->pos, key);
    w->pos += TASD_G_KEYLEN;

    w->buf[w->pos++] = pexp;

    write_be_n(w->buf + w->pos, payload_len, pexp);
    w->pos += pexp;

    if (payload_len > 0u && payload != NULL) {
        memcpy(w->buf + w->pos, payload, payload_len);
    }
    w->pos += payload_len;

    return TASD_OK;
}

size_t tasd_writer_size(const tasd_writer_t *w)
{
    return w ? w->pos : 0u;
}

/* --------------------------------------------------------------------------
 * Controller input size table
 * -------------------------------------------------------------------------- */

uint32_t tasd_controller_input_size(uint16_t controller_type)
{
    switch (controller_type) {
        case TASD_CTRL_NES_STANDARD:     return 1u;
        case TASD_CTRL_NES_FOUR_SCORE:   return 3u;
        case TASD_CTRL_SNES_STANDARD:    return 2u;
        case TASD_CTRL_SNES_MULTITAP:    return 5u;
        case TASD_CTRL_SNES_MOUSE:       return 4u;
        case TASD_CTRL_N64_STANDARD:     return 4u;
        case TASD_CTRL_N64_RUMBLE:       return 4u;
        case TASD_CTRL_N64_CTRL_PAK:     return 4u;
        case TASD_CTRL_N64_TRANSFER_PAK: return 4u;
        case TASD_CTRL_N64_MOUSE:        return 4u;
        case TASD_CTRL_N64_DENSHA_DE_GO: return 4u;
        case TASD_CTRL_GC_STANDARD:      return 8u;
        case TASD_CTRL_GB_STANDARD:      return 1u;
        case TASD_CTRL_GBC_STANDARD:     return 1u;
        case TASD_CTRL_GBA_STANDARD:     return 2u;
        case TASD_CTRL_GENESIS_3BTN:     return 1u;
        case TASD_CTRL_GENESIS_6BTN:     return 2u;
        case TASD_CTRL_A2600_JOYSTICK:   return 1u;
        case TASD_CTRL_A2600_KEYBOARD:   return 1u;
        default:                         return 0u;
    }
}

/* --------------------------------------------------------------------------
 * Payload decode helpers
 * -------------------------------------------------------------------------- */

tasd_result_t tasd_decode_console_type(const tasd_packet_t *p,
                                        tasd_pkt_console_type_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;

    out->console  = p->payload[0];
    out->name     = p->payload + 1u;
    out->name_len = p->payload_len - 1u;
    return TASD_OK;
}

tasd_result_t tasd_decode_console_region(const tasd_packet_t *p, uint8_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;
    *out = p->payload[0];
    return TASD_OK;
}

tasd_result_t tasd_decode_attribution(const tasd_packet_t *p,
                                       tasd_pkt_attribution_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;

    out->type     = p->payload[0];
    out->name     = p->payload + 1u;
    out->name_len = p->payload_len - 1u;
    return TASD_OK;
}

tasd_result_t tasd_decode_timestamp(const tasd_packet_t *p, int64_t *out)
{
    uint64_t raw;
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 8u) return TASD_ERR_INVALID_PAYLOAD;
    raw  = read_be64(p->payload);
    /* Reinterpret bit pattern as signed — avoids UB on C99. */
    memcpy(out, &raw, sizeof(*out));
    return TASD_OK;
}

tasd_result_t tasd_decode_uint32(const tasd_packet_t *p, uint32_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 4u) return TASD_ERR_INVALID_PAYLOAD;
    *out = read_be32(p->payload);
    return TASD_OK;
}

tasd_result_t tasd_decode_uint16(const tasd_packet_t *p, uint16_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 2u) return TASD_ERR_INVALID_PAYLOAD;
    *out = read_be16(p->payload);
    return TASD_OK;
}

tasd_result_t tasd_decode_uint8(const tasd_packet_t *p, uint8_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;
    *out = p->payload[0];
    return TASD_OK;
}

tasd_result_t tasd_decode_int16(const tasd_packet_t *p, int16_t *out)
{
    uint16_t raw;
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 2u) return TASD_ERR_INVALID_PAYLOAD;
    raw = read_be16(p->payload);
    memcpy(out, &raw, sizeof(*out));
    return TASD_OK;
}

tasd_result_t tasd_decode_bool(const tasd_packet_t *p, uint8_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;
    *out = p->payload[0] ? 1u : 0u;
    return TASD_OK;
}

tasd_result_t tasd_decode_memory_init(const tasd_packet_t *p,
                                       tasd_pkt_memory_init_t *out)
{
    uint8_t nlen;

    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    /* Minimum: data_type(1) + device(2) + required(1) + nlen(1) = 5 bytes */
    if (p->payload_len < 5u) return TASD_ERR_INVALID_PAYLOAD;

    out->data_type = p->payload[0];
    out->device    = read_be16(p->payload + 1u);
    out->required  = p->payload[3] ? 1u : 0u;
    nlen           = p->payload[4];
    out->name_len  = nlen;

    if (p->payload_len < (uint32_t)(5u + nlen)) {
        return TASD_ERR_INVALID_PAYLOAD;
    }

    out->name     = p->payload + 5u;
    out->data     = p->payload + 5u + nlen;
    out->data_len = p->payload_len - 5u - nlen;
    return TASD_OK;
}

tasd_result_t tasd_decode_game_identifier(const tasd_packet_t *p,
                                           tasd_pkt_game_identifier_t *out)
{
    uint8_t nlen;

    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    /* Minimum: type(1) + encoding(1) + nlen(1) = 3 bytes */
    if (p->payload_len < 3u) return TASD_ERR_INVALID_PAYLOAD;

    out->type     = p->payload[0];
    out->encoding = p->payload[1];
    nlen          = p->payload[2];
    out->name_len = nlen;

    if (p->payload_len < (uint32_t)(3u + nlen)) {
        return TASD_ERR_INVALID_PAYLOAD;
    }

    out->name           = p->payload + 3u;
    out->identifier     = p->payload + 3u + nlen;
    out->identifier_len = p->payload_len - 3u - nlen;
    return TASD_OK;
}

tasd_result_t tasd_decode_movie_file(const tasd_packet_t *p,
                                      tasd_pkt_movie_file_t *out)
{
    uint8_t nlen;

    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;

    nlen          = p->payload[0];
    out->name_len = nlen;

    if (p->payload_len < (uint32_t)(1u + nlen)) {
        return TASD_ERR_INVALID_PAYLOAD;
    }

    out->name     = p->payload + 1u;
    out->data     = p->payload + 1u + nlen;
    out->data_len = p->payload_len - 1u - nlen;
    return TASD_OK;
}

tasd_result_t tasd_decode_port_controller(const tasd_packet_t *p,
                                           tasd_pkt_port_controller_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 3u) return TASD_ERR_INVALID_PAYLOAD;

    out->port = p->payload[0];
    out->type = read_be16(p->payload + 1u);
    return TASD_OK;
}

tasd_result_t tasd_decode_port_overread(const tasd_packet_t *p,
                                         tasd_pkt_port_overread_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 2u) return TASD_ERR_INVALID_PAYLOAD;

    out->port = p->payload[0];
    out->high = p->payload[1] ? 1u : 0u;
    return TASD_OK;
}

tasd_result_t tasd_decode_input_chunk(const tasd_packet_t *p,
                                       tasd_pkt_input_chunk_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 1u) return TASD_ERR_INVALID_PAYLOAD;

    out->port       = p->payload[0];
    out->inputs     = p->payload + 1u;
    out->inputs_len = p->payload_len - 1u;
    return TASD_OK;
}

tasd_result_t tasd_decode_input_moment(const tasd_packet_t *p,
                                        tasd_pkt_input_moment_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    /* port(1) + hold(1) + index_type(1) + index(8) = 11 bytes minimum */
    if (p->payload_len < 11u) return TASD_ERR_INVALID_PAYLOAD;

    out->port       = p->payload[0];
    out->hold       = p->payload[1] ? 1u : 0u;
    out->index_type = p->payload[2];
    out->index      = read_be64(p->payload + 3u);
    out->inputs     = p->payload + 11u;
    out->inputs_len = p->payload_len - 11u;
    return TASD_OK;
}

tasd_result_t tasd_decode_transition(const tasd_packet_t *p,
                                      tasd_pkt_transition_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    /* port(1) + index_type(1) + index(8) + type(1) = 11 bytes minimum */
    if (p->payload_len < 11u) return TASD_ERR_INVALID_PAYLOAD;

    out->port             = p->payload[0];
    out->index_type       = p->payload[1];
    out->index            = read_be64(p->payload + 2u);
    out->type             = p->payload[10];
    out->inner_packet     = p->payload + 11u;
    out->inner_packet_len = p->payload_len - 11u;
    return TASD_OK;
}

tasd_result_t tasd_decode_lag_frame_chunk(const tasd_packet_t *p,
                                           tasd_pkt_lag_frame_chunk_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    if (p->payload_len < 8u) return TASD_ERR_INVALID_PAYLOAD;

    out->movie_frame = read_be32(p->payload);
    out->count       = read_be32(p->payload + 4u);
    return TASD_OK;
}

tasd_result_t tasd_decode_movie_transition(const tasd_packet_t *p,
                                            tasd_pkt_movie_transition_t *out)
{
    if (!p || !out) return TASD_ERR_INVALID_PAYLOAD;
    /* movie_frame(4) + type(1) = 5 bytes minimum */
    if (p->payload_len < 5u) return TASD_ERR_INVALID_PAYLOAD;

    out->movie_frame      = read_be32(p->payload);
    out->type             = p->payload[4];
    out->inner_packet     = p->payload + 5u;
    out->inner_packet_len = p->payload_len - 5u;
    return TASD_OK;
}

/* --------------------------------------------------------------------------
 * Payload encode helpers
 *
 * All functions write only the *payload* bytes (not the packet framing).
 * Return value: bytes written (>= 0) or TASD_ERR_BUFFER_SMALL.
 * -------------------------------------------------------------------------- */

int tasd_encode_console_type(uint8_t console, const uint8_t *name,
                              uint8_t name_len, uint8_t *buf, size_t len)
{
    size_t need = 1u + (size_t)name_len;
    if (!buf || len < need) return TASD_ERR_BUFFER_SMALL;
    buf[0] = console;
    if (name_len > 0u && name != NULL) {
        memcpy(buf + 1, name, name_len);
    }
    return (int)need;
}

int tasd_encode_console_region(uint8_t region, uint8_t *buf, size_t len)
{
    if (!buf || len < 1u) return TASD_ERR_BUFFER_SMALL;
    buf[0] = region;
    return 1;
}

int tasd_encode_attribution(uint8_t type, const uint8_t *name,
                             uint32_t name_len, uint8_t *buf, size_t len)
{
    size_t need = 1u + (size_t)name_len;
    if (!buf || len < need) return TASD_ERR_BUFFER_SMALL;
    buf[0] = type;
    if (name_len > 0u && name != NULL) {
        memcpy(buf + 1, name, name_len);
    }
    return (int)need;
}

int tasd_encode_timestamp(int64_t unix_ts, uint8_t *buf, size_t len)
{
    uint64_t raw;
    if (!buf || len < 8u) return TASD_ERR_BUFFER_SMALL;
    memcpy(&raw, &unix_ts, sizeof(raw));
    write_be64(buf, raw);
    return 8;
}

int tasd_encode_uint32(uint32_t value, uint8_t *buf, size_t len)
{
    if (!buf || len < 4u) return TASD_ERR_BUFFER_SMALL;
    write_be32(buf, value);
    return 4;
}

int tasd_encode_uint16(uint16_t value, uint8_t *buf, size_t len)
{
    if (!buf || len < 2u) return TASD_ERR_BUFFER_SMALL;
    write_be16(buf, value);
    return 2;
}

int tasd_encode_uint8(uint8_t value, uint8_t *buf, size_t len)
{
    if (!buf || len < 1u) return TASD_ERR_BUFFER_SMALL;
    buf[0] = value;
    return 1;
}

int tasd_encode_int16(int16_t value, uint8_t *buf, size_t len)
{
    uint16_t raw;
    if (!buf || len < 2u) return TASD_ERR_BUFFER_SMALL;
    memcpy(&raw, &value, sizeof(raw));
    write_be16(buf, raw);
    return 2;
}

int tasd_encode_bool(uint8_t value, uint8_t *buf, size_t len)
{
    if (!buf || len < 1u) return TASD_ERR_BUFFER_SMALL;
    buf[0] = value ? 1u : 0u;
    return 1;
}

int tasd_encode_memory_init(const tasd_pkt_memory_init_t *p,
                             uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 5u + (size_t)p->name_len + (size_t)p->data_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    buf[0] = p->data_type;
    write_be16(buf + 1u, p->device);
    buf[3] = p->required ? 1u : 0u;
    buf[4] = p->name_len;
    if (p->name_len > 0u && p->name != NULL) {
        memcpy(buf + 5u, p->name, p->name_len);
    }
    if (p->data_len > 0u && p->data != NULL) {
        memcpy(buf + 5u + p->name_len, p->data, p->data_len);
    }
    return (int)need;
}

int tasd_encode_game_identifier(const tasd_pkt_game_identifier_t *p,
                                 uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 3u + (size_t)p->name_len + (size_t)p->identifier_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    buf[0] = p->type;
    buf[1] = p->encoding;
    buf[2] = p->name_len;
    if (p->name_len > 0u && p->name != NULL) {
        memcpy(buf + 3u, p->name, p->name_len);
    }
    if (p->identifier_len > 0u && p->identifier != NULL) {
        memcpy(buf + 3u + p->name_len, p->identifier, p->identifier_len);
    }
    return (int)need;
}

int tasd_encode_movie_file(const tasd_pkt_movie_file_t *p,
                            uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 1u + (size_t)p->name_len + (size_t)p->data_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    buf[0] = p->name_len;
    if (p->name_len > 0u && p->name != NULL) {
        memcpy(buf + 1u, p->name, p->name_len);
    }
    if (p->data_len > 0u && p->data != NULL) {
        memcpy(buf + 1u + p->name_len, p->data, p->data_len);
    }
    return (int)need;
}

int tasd_encode_port_controller(uint8_t port, uint16_t type,
                                 uint8_t *buf, size_t len)
{
    if (!buf || len < 3u) return TASD_ERR_BUFFER_SMALL;
    buf[0] = port;
    write_be16(buf + 1u, type);
    return 3;
}

int tasd_encode_port_overread(uint8_t port, uint8_t high,
                               uint8_t *buf, size_t len)
{
    if (!buf || len < 2u) return TASD_ERR_BUFFER_SMALL;
    buf[0] = port;
    buf[1] = high ? 1u : 0u;
    return 2;
}

int tasd_encode_input_chunk(uint8_t port, const uint8_t *inputs,
                             uint32_t inputs_len, uint8_t *buf, size_t len)
{
    size_t need = 1u + (size_t)inputs_len;
    if (!buf || len < need) return TASD_ERR_BUFFER_SMALL;
    buf[0] = port;
    if (inputs_len > 0u && inputs != NULL) {
        memcpy(buf + 1u, inputs, inputs_len);
    }
    return (int)need;
}

int tasd_encode_input_moment(const tasd_pkt_input_moment_t *p,
                              uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 11u + (size_t)p->inputs_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    buf[0] = p->port;
    buf[1] = p->hold ? 1u : 0u;
    buf[2] = p->index_type;
    write_be64(buf + 3u, p->index);
    if (p->inputs_len > 0u && p->inputs != NULL) {
        memcpy(buf + 11u, p->inputs, p->inputs_len);
    }
    return (int)need;
}

int tasd_encode_transition(const tasd_pkt_transition_t *p,
                            uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 11u + (size_t)p->inner_packet_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    buf[0] = p->port;
    buf[1] = p->index_type;
    write_be64(buf + 2u, p->index);
    buf[10] = p->type;
    if (p->inner_packet_len > 0u && p->inner_packet != NULL) {
        memcpy(buf + 11u, p->inner_packet, p->inner_packet_len);
    }
    return (int)need;
}

int tasd_encode_lag_frame_chunk(uint32_t movie_frame, uint32_t count,
                                 uint8_t *buf, size_t len)
{
    if (!buf || len < 8u) return TASD_ERR_BUFFER_SMALL;
    write_be32(buf,      movie_frame);
    write_be32(buf + 4u, count);
    return 8;
}

int tasd_encode_movie_transition(const tasd_pkt_movie_transition_t *p,
                                  uint8_t *buf, size_t len)
{
    size_t need;
    if (!p || !buf) return TASD_ERR_BUFFER_SMALL;

    need = 5u + (size_t)p->inner_packet_len;
    if (len < need) return TASD_ERR_BUFFER_SMALL;

    write_be32(buf, p->movie_frame);
    buf[4] = p->type;
    if (p->inner_packet_len > 0u && p->inner_packet != NULL) {
        memcpy(buf + 5u, p->inner_packet, p->inner_packet_len);
    }
    return (int)need;
}
