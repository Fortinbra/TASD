/*
 * tasd.h — TASD file format library
 *
 * Platform-agnostic, zero-allocation C99 library for reading and writing
 * Tool Assisted Speedrun Dump (TASD) files as specified at https://tasd.io/
 *
 * Compatible with the Raspberry Pi Pico SDK and any hosted C/C++ environment.
 *
 * Usage pattern (reading):
 *   tasd_header_t hdr;
 *   tasd_result_t rc = tasd_read_header(buf, len, &hdr);
 *   tasd_reader_t r;
 *   tasd_reader_init(&r, buf, len, &hdr);
 *   tasd_packet_t pkt;
 *   while (tasd_reader_next(&r, &pkt) == TASD_OK) { ... }
 *
 * Usage pattern (writing):
 *   uint8_t buf[4096];
 *   tasd_writer_t w;
 *   tasd_writer_init(&w, buf, sizeof(buf));
 *   tasd_writer_write_header(&w);
 *   uint8_t payload[1] = { TASD_CONSOLE_NES };
 *   tasd_writer_append(&w, TASD_KEY_CONSOLE_TYPE, payload, 1);
 */

#ifndef TASD_H
#define TASD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * File-level constants
 * -------------------------------------------------------------------------- */

#define TASD_MAGIC_0     0x54u   /* 'T' */
#define TASD_MAGIC_1     0x41u   /* 'A' */
#define TASD_MAGIC_2     0x53u   /* 'S' */
#define TASD_MAGIC_3     0x44u   /* 'D' */
#define TASD_VERSION     1u
#define TASD_G_KEYLEN    2u
#define TASD_HEADER_SIZE 7u

/* --------------------------------------------------------------------------
 * Result codes  (tasd_result_t is a signed int; 0 = OK, negative = error)
 * -------------------------------------------------------------------------- */

typedef int tasd_result_t;

#define TASD_OK                  0
#define TASD_ERR_INVALID_MAGIC   (-1)  /* magic bytes do not match */
#define TASD_ERR_UNSUPPORTED     (-2)  /* version or keylen not supported */
#define TASD_ERR_BUFFER_SMALL    (-3)  /* destination buffer too small */
#define TASD_ERR_TRUNCATED       (-4)  /* source data ends unexpectedly */
#define TASD_ERR_INVALID_PEXP    (-5)  /* PEXP == 0 */
#define TASD_ERR_INVALID_PAYLOAD (-6)  /* payload does not match expected format */
#define TASD_ERR_END             (-7)  /* no more packets (not an error per se) */

/* --------------------------------------------------------------------------
 * Packet keys (2 bytes, big-endian in the binary stream)
 * -------------------------------------------------------------------------- */

/* General */
#define TASD_KEY_CONSOLE_TYPE        0x0001u
#define TASD_KEY_CONSOLE_REGION      0x0002u
#define TASD_KEY_GAME_TITLE          0x0003u
#define TASD_KEY_ROM_NAME            0x0004u
#define TASD_KEY_ATTRIBUTION         0x0005u
#define TASD_KEY_CATEGORY            0x0006u
#define TASD_KEY_EMULATOR_NAME       0x0007u
#define TASD_KEY_EMULATOR_VERSION    0x0008u
#define TASD_KEY_EMULATOR_CORE       0x0009u
#define TASD_KEY_TAS_LAST_MODIFIED   0x000Au
#define TASD_KEY_DUMP_CREATED        0x000Bu
#define TASD_KEY_DUMP_LAST_MODIFIED  0x000Cu
#define TASD_KEY_TOTAL_FRAMES        0x000Du
#define TASD_KEY_RERECORDS           0x000Eu
#define TASD_KEY_SOURCE_LINK         0x000Fu
#define TASD_KEY_BLANK_FRAMES        0x0010u
#define TASD_KEY_VERIFIED            0x0011u
#define TASD_KEY_MEMORY_INIT         0x0012u
#define TASD_KEY_GAME_IDENTIFIER     0x0013u
#define TASD_KEY_MOVIE_LICENSE       0x0014u
#define TASD_KEY_MOVIE_FILE          0x0015u
#define TASD_KEY_PORT_CONTROLLER     0x00F0u
#define TASD_KEY_PORT_OVERREAD       0x00F1u

/* NES specific */
#define TASD_KEY_NES_LATCH_FILTER    0x0101u
#define TASD_KEY_NES_CLOCK_FILTER    0x0102u
#define TASD_KEY_NES_GAME_GENIE_CODE 0x0104u

/* SNES specific */
#define TASD_KEY_SNES_LATCH_FILTER    0x0201u
#define TASD_KEY_SNES_CLOCK_FILTER    0x0202u
#define TASD_KEY_SNES_GAME_GENIE_CODE 0x0204u
#define TASD_KEY_SNES_LATCH_TRAIN     0x0205u

/* Genesis specific */
#define TASD_KEY_GENESIS_GAME_GENIE_CODE 0x0804u

/* Input / timing */
#define TASD_KEY_INPUT_CHUNK      0xFE01u
#define TASD_KEY_INPUT_MOMENT     0xFE02u
#define TASD_KEY_TRANSITION       0xFE03u
#define TASD_KEY_LAG_FRAME_CHUNK  0xFE04u
#define TASD_KEY_MOVIE_TRANSITION 0xFE05u

/* Extraneous */
#define TASD_KEY_COMMENT      0xFF01u
#define TASD_KEY_EXPERIMENTAL 0xFFFEu
#define TASD_KEY_UNSPECIFIED  0xFFFFu

/* --------------------------------------------------------------------------
 * Console type values  (CONSOLE_TYPE packet, byte 0)
 * -------------------------------------------------------------------------- */

#define TASD_CONSOLE_NES     0x01u
#define TASD_CONSOLE_SNES    0x02u
#define TASD_CONSOLE_N64     0x03u
#define TASD_CONSOLE_GC      0x04u
#define TASD_CONSOLE_GB      0x05u
#define TASD_CONSOLE_GBC     0x06u
#define TASD_CONSOLE_GBA     0x07u
#define TASD_CONSOLE_GENESIS 0x08u
#define TASD_CONSOLE_A2600   0x09u
#define TASD_CONSOLE_CUSTOM  0xFFu

/* --------------------------------------------------------------------------
 * Console region values  (CONSOLE_REGION packet)
 * -------------------------------------------------------------------------- */

#define TASD_REGION_NTSC  0x01u
#define TASD_REGION_PAL   0x02u
#define TASD_REGION_OTHER 0xFFu

/* --------------------------------------------------------------------------
 * Attribution type values  (ATTRIBUTION packet, byte 0)
 * -------------------------------------------------------------------------- */

#define TASD_ATTR_AUTHOR       0x01u
#define TASD_ATTR_VERIFIER     0x02u
#define TASD_ATTR_FILE_CREATOR 0x03u
#define TASD_ATTR_FILE_EDITOR  0x04u
#define TASD_ATTR_OTHER        0xFFu

/* --------------------------------------------------------------------------
 * Controller type values  (PORT_CONTROLLER packet bytes 1-2; uint16_t)
 * -------------------------------------------------------------------------- */

#define TASD_CTRL_NES_STANDARD      0x0101u
#define TASD_CTRL_NES_FOUR_SCORE    0x0102u
#define TASD_CTRL_SNES_STANDARD     0x0201u
#define TASD_CTRL_SNES_MULTITAP     0x0202u
#define TASD_CTRL_SNES_MOUSE        0x0203u
#define TASD_CTRL_N64_STANDARD      0x0301u
#define TASD_CTRL_N64_RUMBLE        0x0302u
#define TASD_CTRL_N64_CTRL_PAK      0x0303u
#define TASD_CTRL_N64_TRANSFER_PAK  0x0304u
#define TASD_CTRL_N64_MOUSE         0x0305u
#define TASD_CTRL_N64_DENSHA_DE_GO  0x0308u
#define TASD_CTRL_GC_STANDARD       0x0401u
#define TASD_CTRL_GB_STANDARD       0x0501u
#define TASD_CTRL_GBC_STANDARD      0x0601u
#define TASD_CTRL_GBA_STANDARD      0x0701u
#define TASD_CTRL_GENESIS_3BTN      0x0801u
#define TASD_CTRL_GENESIS_6BTN      0x0802u
#define TASD_CTRL_A2600_JOYSTICK    0x0901u
#define TASD_CTRL_A2600_KEYBOARD    0x0903u
#define TASD_CTRL_OTHER             0xFFFFu

/* --------------------------------------------------------------------------
 * Transition type values  (TRANSITION / MOVIE_TRANSITION packet)
 * -------------------------------------------------------------------------- */

#define TASD_TRANSITION_SOFT_RESET   0x01u
#define TASD_TRANSITION_POWER_RESET  0x02u
#define TASD_TRANSITION_RESTART_TASD 0x03u
#define TASD_TRANSITION_PACKET       0xFFu

/* --------------------------------------------------------------------------
 * Index type values  (INPUT_MOMENT / TRANSITION packets)
 * -------------------------------------------------------------------------- */

#define TASD_INDEX_FRAME        0x01u
#define TASD_INDEX_CYCLE_COUNT  0x02u
#define TASD_INDEX_MILLISECONDS 0x03u
#define TASD_INDEX_MICROSECONDS 0x04u
#define TASD_INDEX_NANOSECONDS  0x05u
#define TASD_INDEX_CHUNK_BYTE   0x06u  /* TRANSITION only */

/* --------------------------------------------------------------------------
 * Memory-init type values  (MEMORY_INIT packet, byte 0)
 * -------------------------------------------------------------------------- */

#define TASD_MEMINIT_NONE    0x01u  /* no init required */
#define TASD_MEMINIT_ALL_00  0x02u
#define TASD_MEMINIT_ALL_FF  0x03u
#define TASD_MEMINIT_ALT     0x04u  /* 00 00 00 00 FF FF FF FF repeating */
#define TASD_MEMINIT_RANDOM  0x05u
#define TASD_MEMINIT_CUSTOM  0xFFu

/* Memory-init device values (two bytes) */
#define TASD_MEMINIT_DEV_NES_RAM     0x0101u
#define TASD_MEMINIT_DEV_NES_SAVE    0x0102u
#define TASD_MEMINIT_DEV_SNES_RAM    0x0201u
#define TASD_MEMINIT_DEV_SNES_SAVE   0x0202u
#define TASD_MEMINIT_DEV_GB_RAM      0x0501u
#define TASD_MEMINIT_DEV_GB_SAVE     0x0502u
#define TASD_MEMINIT_DEV_GBC_RAM     0x0601u
#define TASD_MEMINIT_DEV_GBC_SAVE    0x0602u
#define TASD_MEMINIT_DEV_GBA_RAM     0x0701u
#define TASD_MEMINIT_DEV_GBA_SAVE    0x0702u
#define TASD_MEMINIT_DEV_GENESIS_RAM 0x0801u
#define TASD_MEMINIT_DEV_GENESIS_SAVE 0x0802u
#define TASD_MEMINIT_DEV_A2600_RAM   0x0901u
#define TASD_MEMINIT_DEV_A2600_SAVE  0x0902u
#define TASD_MEMINIT_DEV_CUSTOM      0xFFFFu

/* --------------------------------------------------------------------------
 * Game-identifier type values  (GAME_IDENTIFIER packet, byte 0)
 * -------------------------------------------------------------------------- */

#define TASD_IDENT_MD5        0x01u
#define TASD_IDENT_SHA1       0x02u
#define TASD_IDENT_SHA224     0x03u
#define TASD_IDENT_SHA256     0x04u
#define TASD_IDENT_SHA384     0x05u
#define TASD_IDENT_SHA512     0x06u
#define TASD_IDENT_SHA512_224 0x07u
#define TASD_IDENT_SHA512_256 0x08u
#define TASD_IDENT_SHA3_224   0x09u
#define TASD_IDENT_SHA3_256   0x0Au
#define TASD_IDENT_SHA3_384   0x0Bu
#define TASD_IDENT_SHA3_512   0x0Cu
#define TASD_IDENT_SHAKE128   0x0Du
#define TASD_IDENT_SHAKE256   0x0Eu
#define TASD_IDENT_OTHER      0xFFu

/* Game-identifier encoding values  (GAME_IDENTIFIER packet, byte 1) */
#define TASD_ENCODING_RAW    0x01u
#define TASD_ENCODING_BASE16 0x02u
#define TASD_ENCODING_BASE32 0x03u
#define TASD_ENCODING_BASE64 0x04u

/* --------------------------------------------------------------------------
 * Core data structures
 * -------------------------------------------------------------------------- */

/* Parsed file header. */
typedef struct {
    uint16_t version;
    uint8_t  g_keylen;
} tasd_header_t;

/*
 * A single decoded packet.  The payload pointer references data inside the
 * caller-supplied buffer and is therefore only valid as long as that buffer
 * remains unchanged.
 */
typedef struct {
    uint16_t       key;
    uint32_t       payload_len;
    const uint8_t *payload;
} tasd_packet_t;

/* Forward-only packet iterator over a const buffer. */
typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         pos;
    uint8_t        g_keylen;
} tasd_reader_t;

/* Sequential packet writer into a mutable buffer. */
typedef struct {
    uint8_t *buf;
    size_t   len;
    size_t   pos;
} tasd_writer_t;

/* --------------------------------------------------------------------------
 * Payload structs — decoded from raw packet payload bytes
 * -------------------------------------------------------------------------- */

/* CONSOLE_TYPE */
typedef struct {
    uint8_t        console;
    const uint8_t *name;      /* UTF-8, not NUL-terminated */
    uint32_t       name_len;
} tasd_pkt_console_type_t;

/* ATTRIBUTION */
typedef struct {
    uint8_t        type;
    const uint8_t *name;
    uint32_t       name_len;
} tasd_pkt_attribution_t;

/* MEMORY_INIT */
typedef struct {
    uint8_t        data_type;
    uint16_t       device;
    uint8_t        required;
    const uint8_t *name;
    uint8_t        name_len;
    const uint8_t *data;
    uint32_t       data_len;
} tasd_pkt_memory_init_t;

/* GAME_IDENTIFIER */
typedef struct {
    uint8_t        type;
    uint8_t        encoding;
    const uint8_t *name;
    uint8_t        name_len;
    const uint8_t *identifier;
    uint32_t       identifier_len;
} tasd_pkt_game_identifier_t;

/* MOVIE_FILE */
typedef struct {
    const uint8_t *name;
    uint8_t        name_len;
    const uint8_t *data;
    uint32_t       data_len;
} tasd_pkt_movie_file_t;

/* PORT_CONTROLLER */
typedef struct {
    uint8_t  port;
    uint16_t type;
} tasd_pkt_port_controller_t;

/* PORT_OVERREAD */
typedef struct {
    uint8_t port;
    uint8_t high;
} tasd_pkt_port_overread_t;

/* INPUT_CHUNK */
typedef struct {
    uint8_t        port;
    const uint8_t *inputs;
    uint32_t       inputs_len;
} tasd_pkt_input_chunk_t;

/* INPUT_MOMENT */
typedef struct {
    uint8_t        port;
    uint8_t        hold;
    uint8_t        index_type;
    uint64_t       index;
    const uint8_t *inputs;
    uint32_t       inputs_len;
} tasd_pkt_input_moment_t;

/* TRANSITION */
typedef struct {
    uint8_t        port;
    uint8_t        index_type;
    uint64_t       index;
    uint8_t        type;
    const uint8_t *inner_packet;
    uint32_t       inner_packet_len;
} tasd_pkt_transition_t;

/* LAG_FRAME_CHUNK */
typedef struct {
    uint32_t movie_frame;
    uint32_t count;
} tasd_pkt_lag_frame_chunk_t;

/* MOVIE_TRANSITION */
typedef struct {
    uint32_t       movie_frame;
    uint8_t        type;
    const uint8_t *inner_packet;
    uint32_t       inner_packet_len;
} tasd_pkt_movie_transition_t;

/* --------------------------------------------------------------------------
 * Core API
 * -------------------------------------------------------------------------- */

/*
 * Parse the 7-byte TASD header at buf[0..6].
 * Returns TASD_OK on success.
 */
tasd_result_t tasd_read_header(const uint8_t *buf, size_t len,
                                tasd_header_t *out);

/*
 * Write the 7-byte TASD header to buf[0..6].
 * Returns TASD_OK on success.
 */
tasd_result_t tasd_write_header(uint8_t *buf, size_t len);

/*
 * Initialise a reader.  Call after tasd_read_header().
 * The reader starts at the first packet (byte 7 of the file).
 */
void tasd_reader_init(tasd_reader_t *r, const uint8_t *buf, size_t len,
                      const tasd_header_t *hdr);

/*
 * Advance to and decode the next packet.
 * Returns TASD_OK when a packet was successfully read, TASD_ERR_END when
 * there are no more packets, or a negative error code on failure.
 * out->payload points into the buffer passed to tasd_reader_init().
 */
tasd_result_t tasd_reader_next(tasd_reader_t *r, tasd_packet_t *out);

/*
 * Initialise a writer.  The writer starts at position 0.
 * Call tasd_writer_write_header() before appending packets.
 */
void tasd_writer_init(tasd_writer_t *w, uint8_t *buf, size_t len);

/*
 * Write the 7-byte TASD header into the writer buffer and advance the
 * position by TASD_HEADER_SIZE.
 * Returns TASD_OK on success.
 */
tasd_result_t tasd_writer_write_header(tasd_writer_t *w);

/*
 * Append a packet (key + framing + payload) to the writer buffer.
 * payload may be NULL when payload_len == 0.
 * Returns TASD_OK on success.
 */
tasd_result_t tasd_writer_append(tasd_writer_t *w, uint16_t key,
                                  const uint8_t *payload,
                                  uint32_t payload_len);

/*
 * Return the number of bytes so far written into the writer buffer.
 * This is the total file size after writing the header and all packets.
 */
size_t tasd_writer_size(const tasd_writer_t *w);

/*
 * Return the number of bytes per single controller input for the given
 * controller type.  Returns 0 for unknown or reserved types.
 */
uint32_t tasd_controller_input_size(uint16_t controller_type);

/* --------------------------------------------------------------------------
 * Payload decode helpers
 *
 * Each function validates the payload length and fills the output struct.
 * The pointers in the output structs reference the original packet payload
 * and are therefore only valid while the source buffer is unchanged.
 * -------------------------------------------------------------------------- */

tasd_result_t tasd_decode_console_type(const tasd_packet_t *p,
                                        tasd_pkt_console_type_t *out);

/* CONSOLE_REGION — 1-byte value written to *out */
tasd_result_t tasd_decode_console_region(const tasd_packet_t *p,
                                          uint8_t *out);

/* ATTRIBUTION */
tasd_result_t tasd_decode_attribution(const tasd_packet_t *p,
                                       tasd_pkt_attribution_t *out);

/* Timestamps (TAS_LAST_MODIFIED, DUMP_CREATED, DUMP_LAST_MODIFIED) */
tasd_result_t tasd_decode_timestamp(const tasd_packet_t *p, int64_t *out);

/* TOTAL_FRAMES / RERECORDS — 4-byte big-endian uint32 */
tasd_result_t tasd_decode_uint32(const tasd_packet_t *p, uint32_t *out);

/* NES_LATCH_FILTER / SNES_LATCH_FILTER — 2-byte big-endian uint16 */
tasd_result_t tasd_decode_uint16(const tasd_packet_t *p, uint16_t *out);

/* NES_CLOCK_FILTER / SNES_CLOCK_FILTER — 1-byte uint8 */
tasd_result_t tasd_decode_uint8(const tasd_packet_t *p, uint8_t *out);

/* BLANK_FRAMES — 2-byte big-endian int16 */
tasd_result_t tasd_decode_int16(const tasd_packet_t *p, int16_t *out);

/* VERIFIED / EXPERIMENTAL — 1-byte boolean */
tasd_result_t tasd_decode_bool(const tasd_packet_t *p, uint8_t *out);

/* MEMORY_INIT */
tasd_result_t tasd_decode_memory_init(const tasd_packet_t *p,
                                       tasd_pkt_memory_init_t *out);

/* GAME_IDENTIFIER */
tasd_result_t tasd_decode_game_identifier(const tasd_packet_t *p,
                                           tasd_pkt_game_identifier_t *out);

/* MOVIE_FILE */
tasd_result_t tasd_decode_movie_file(const tasd_packet_t *p,
                                      tasd_pkt_movie_file_t *out);

/* PORT_CONTROLLER */
tasd_result_t tasd_decode_port_controller(const tasd_packet_t *p,
                                           tasd_pkt_port_controller_t *out);

/* PORT_OVERREAD */
tasd_result_t tasd_decode_port_overread(const tasd_packet_t *p,
                                         tasd_pkt_port_overread_t *out);

/* INPUT_CHUNK */
tasd_result_t tasd_decode_input_chunk(const tasd_packet_t *p,
                                       tasd_pkt_input_chunk_t *out);

/* INPUT_MOMENT */
tasd_result_t tasd_decode_input_moment(const tasd_packet_t *p,
                                        tasd_pkt_input_moment_t *out);

/* TRANSITION */
tasd_result_t tasd_decode_transition(const tasd_packet_t *p,
                                      tasd_pkt_transition_t *out);

/* LAG_FRAME_CHUNK */
tasd_result_t tasd_decode_lag_frame_chunk(const tasd_packet_t *p,
                                           tasd_pkt_lag_frame_chunk_t *out);

/* MOVIE_TRANSITION */
tasd_result_t tasd_decode_movie_transition(const tasd_packet_t *p,
                                            tasd_pkt_movie_transition_t *out);

/*
 * SNES_LATCH_TRAIN — the payload is a packed array of 8-byte big-endian
 * uint64 values.  Use p->payload_len / 8 to get the count; decode each
 * value with tasd_read_u64() if available, or read 8 bytes manually.
 * No separate decode function is needed beyond accessing p->payload directly.
 */

/* --------------------------------------------------------------------------
 * Payload encode helpers
 *
 * Each function serialises the given values into buf and returns the number
 * of bytes written, or TASD_ERR_BUFFER_SMALL if buf is too small.
 * These helpers produce only the *payload* bytes; the caller is responsible
 * for passing the result to tasd_writer_append() with the correct key.
 * -------------------------------------------------------------------------- */

int tasd_encode_console_type(uint8_t console, const uint8_t *name,
                              uint8_t name_len, uint8_t *buf, size_t len);

int tasd_encode_console_region(uint8_t region, uint8_t *buf, size_t len);

int tasd_encode_attribution(uint8_t type, const uint8_t *name,
                             uint32_t name_len, uint8_t *buf, size_t len);

int tasd_encode_timestamp(int64_t unix_ts, uint8_t *buf, size_t len);

int tasd_encode_uint32(uint32_t value, uint8_t *buf, size_t len);

int tasd_encode_uint16(uint16_t value, uint8_t *buf, size_t len);

int tasd_encode_uint8(uint8_t value, uint8_t *buf, size_t len);

int tasd_encode_int16(int16_t value, uint8_t *buf, size_t len);

int tasd_encode_bool(uint8_t value, uint8_t *buf, size_t len);

int tasd_encode_memory_init(const tasd_pkt_memory_init_t *p,
                             uint8_t *buf, size_t len);

int tasd_encode_game_identifier(const tasd_pkt_game_identifier_t *p,
                                 uint8_t *buf, size_t len);

int tasd_encode_movie_file(const tasd_pkt_movie_file_t *p,
                            uint8_t *buf, size_t len);

int tasd_encode_port_controller(uint8_t port, uint16_t type,
                                 uint8_t *buf, size_t len);

int tasd_encode_port_overread(uint8_t port, uint8_t high,
                               uint8_t *buf, size_t len);

int tasd_encode_input_chunk(uint8_t port, const uint8_t *inputs,
                             uint32_t inputs_len, uint8_t *buf, size_t len);

int tasd_encode_input_moment(const tasd_pkt_input_moment_t *p,
                              uint8_t *buf, size_t len);

int tasd_encode_transition(const tasd_pkt_transition_t *p,
                            uint8_t *buf, size_t len);

int tasd_encode_lag_frame_chunk(uint32_t movie_frame, uint32_t count,
                                 uint8_t *buf, size_t len);

int tasd_encode_movie_transition(const tasd_pkt_movie_transition_t *p,
                                  uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TASD_H */
