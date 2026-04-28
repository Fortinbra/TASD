/*
 * tests/test_tasd.c — basic self-tests for the TASD library
 *
 * Compile and run:
 *   cmake -S . -B build && cmake --build build && ctest --test-dir build -V
 */

#include "tasd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Minimal test harness ------------------------------------------------ */

static int g_failures = 0;

#define CHECK(expr)                                                     \
    do {                                                                \
        if (!(expr)) {                                                  \
            fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__, __LINE__,   \
                    #expr);                                             \
            g_failures++;                                               \
        }                                                               \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

/* ---- Helpers ------------------------------------------------------------- */

static void test_header_roundtrip(void)
{
    uint8_t       buf[TASD_HEADER_SIZE + 4];
    tasd_header_t hdr;

    memset(buf, 0xAA, sizeof(buf));

    CHECK_EQ(tasd_write_header(buf, sizeof(buf)), TASD_OK);

    /* Magic */
    CHECK_EQ(buf[0], 0x54u);
    CHECK_EQ(buf[1], 0x41u);
    CHECK_EQ(buf[2], 0x53u);
    CHECK_EQ(buf[3], 0x44u);

    /* Sentinel byte after header must be untouched */
    CHECK_EQ(buf[TASD_HEADER_SIZE], 0xAAu);

    CHECK_EQ(tasd_read_header(buf, sizeof(buf), &hdr), TASD_OK);
    CHECK_EQ(hdr.version,  (uint16_t)TASD_VERSION);
    CHECK_EQ(hdr.g_keylen, (uint8_t)TASD_G_KEYLEN);
}

static void test_header_bad_magic(void)
{
    uint8_t       buf[TASD_HEADER_SIZE] = {0};
    tasd_header_t hdr;

    CHECK_EQ(tasd_read_header(buf, sizeof(buf), &hdr), TASD_ERR_INVALID_MAGIC);
}

static void test_header_too_short(void)
{
    uint8_t       buf[3] = {0x54, 0x41, 0x53};
    tasd_header_t hdr;

    CHECK_EQ(tasd_read_header(buf, sizeof(buf), &hdr), TASD_ERR_TRUNCATED);
    CHECK_EQ(tasd_write_header(buf, sizeof(buf)), TASD_ERR_BUFFER_SMALL);
}

/* ---- Writer / reader roundtrip ------------------------------------------- */

static void test_writer_reader_roundtrip(void)
{
    uint8_t       buf[256];
    tasd_writer_t w;
    tasd_reader_t r;
    tasd_header_t hdr;
    tasd_packet_t pkt;

    uint8_t payload_console[1]  = { TASD_CONSOLE_NES };
    uint8_t payload_region[1]   = { TASD_REGION_NTSC };
    uint8_t payload_frames[4];
    uint8_t payload_title[]     = { 'T', 'e', 's', 't' };

    /* TOTAL_FRAMES = 1000 */
    payload_frames[0] = 0x00;
    payload_frames[1] = 0x00;
    payload_frames[2] = 0x03;
    payload_frames[3] = 0xE8;

    tasd_writer_init(&w, buf, sizeof(buf));
    CHECK_EQ(tasd_writer_write_header(&w), TASD_OK);
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_CONSOLE_TYPE,   payload_console, 1u),  TASD_OK);
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_CONSOLE_REGION, payload_region,  1u),  TASD_OK);
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_TOTAL_FRAMES,   payload_frames,  4u),  TASD_OK);
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_GAME_TITLE,     payload_title,   4u),  TASD_OK);

    /* Read back */
    CHECK_EQ(tasd_read_header(buf, tasd_writer_size(&w), &hdr), TASD_OK);
    tasd_reader_init(&r, buf, tasd_writer_size(&w), &hdr);

    /* Packet 1: CONSOLE_TYPE */
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_OK);
    CHECK_EQ(pkt.key, (uint16_t)TASD_KEY_CONSOLE_TYPE);
    CHECK_EQ(pkt.payload_len, 1u);
    {
        tasd_pkt_console_type_t ct;
        CHECK_EQ(tasd_decode_console_type(&pkt, &ct), TASD_OK);
        CHECK_EQ(ct.console, (uint8_t)TASD_CONSOLE_NES);
        CHECK_EQ(ct.name_len, 0u);
    }

    /* Packet 2: CONSOLE_REGION */
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_OK);
    CHECK_EQ(pkt.key, (uint16_t)TASD_KEY_CONSOLE_REGION);
    {
        uint8_t region = 0;
        CHECK_EQ(tasd_decode_console_region(&pkt, &region), TASD_OK);
        CHECK_EQ(region, (uint8_t)TASD_REGION_NTSC);
    }

    /* Packet 3: TOTAL_FRAMES */
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_OK);
    CHECK_EQ(pkt.key, (uint16_t)TASD_KEY_TOTAL_FRAMES);
    {
        uint32_t frames = 0;
        CHECK_EQ(tasd_decode_uint32(&pkt, &frames), TASD_OK);
        CHECK_EQ(frames, 1000u);
    }

    /* Packet 4: GAME_TITLE — raw string */
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_OK);
    CHECK_EQ(pkt.key, (uint16_t)TASD_KEY_GAME_TITLE);
    CHECK_EQ(pkt.payload_len, 4u);
    CHECK_EQ(memcmp(pkt.payload, "Test", 4), 0);

    /* End */
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_ERR_END);
}

/* ---- Encode helpers ------------------------------------------------------- */

static void test_encode_helpers(void)
{
    uint8_t buf[64];
    int     n;

    /* console_type */
    n = tasd_encode_console_type(TASD_CONSOLE_SNES, NULL, 0, buf, sizeof(buf));
    CHECK_EQ(n, 1);
    CHECK_EQ(buf[0], (uint8_t)TASD_CONSOLE_SNES);

    /* attribution */
    {
        const uint8_t name[] = { 'A', 'l', 'i', 'c', 'e' };
        n = tasd_encode_attribution(TASD_ATTR_AUTHOR, name, 5, buf, sizeof(buf));
        CHECK_EQ(n, 6);
        CHECK_EQ(buf[0], (uint8_t)TASD_ATTR_AUTHOR);
        CHECK_EQ(memcmp(buf + 1, name, 5), 0);
    }

    /* timestamp */
    {
        int64_t ts = (int64_t)0x0102030405060708LL;
        n = tasd_encode_timestamp(ts, buf, sizeof(buf));
        CHECK_EQ(n, 8);
        CHECK_EQ(buf[0], 0x01u);
        CHECK_EQ(buf[7], 0x08u);
    }

    /* uint32 */
    n = tasd_encode_uint32(0xDEADBEEFu, buf, sizeof(buf));
    CHECK_EQ(n, 4);
    CHECK_EQ(buf[0], 0xDEu);
    CHECK_EQ(buf[3], 0xEFu);

    /* int16 negative */
    {
        int16_t val = -1;
        n = tasd_encode_int16(val, buf, sizeof(buf));
        CHECK_EQ(n, 2);
        CHECK_EQ(buf[0], 0xFFu);
        CHECK_EQ(buf[1], 0xFFu);
    }

    /* bool */
    n = tasd_encode_bool(1, buf, sizeof(buf));
    CHECK_EQ(n, 1);
    CHECK_EQ(buf[0], 1u);

    n = tasd_encode_bool(42, buf, sizeof(buf));  /* any non-zero → 1 */
    CHECK_EQ(buf[0], 1u);

    /* port_controller */
    n = tasd_encode_port_controller(1, TASD_CTRL_NES_STANDARD, buf, sizeof(buf));
    CHECK_EQ(n, 3);
    CHECK_EQ(buf[0], 1u);
    CHECK_EQ(buf[1], 0x01u);
    CHECK_EQ(buf[2], 0x01u);

    /* lag_frame_chunk */
    n = tasd_encode_lag_frame_chunk(100u, 3u, buf, sizeof(buf));
    CHECK_EQ(n, 8);
    {
        tasd_packet_t p;
        tasd_pkt_lag_frame_chunk_t lfc;
        p.key         = TASD_KEY_LAG_FRAME_CHUNK;
        p.payload     = buf;
        p.payload_len = 8u;
        CHECK_EQ(tasd_decode_lag_frame_chunk(&p, &lfc), TASD_OK);
        CHECK_EQ(lfc.movie_frame, 100u);
        CHECK_EQ(lfc.count,       3u);
    }
}

/* ---- INPUT_CHUNK decode --------------------------------------------------- */

static void test_input_chunk(void)
{
    uint8_t raw[]  = { 0x01, 0xFF, 0xFE, 0xFD };  /* port=1, 3 input bytes */
    tasd_packet_t p;
    tasd_pkt_input_chunk_t ic;

    p.key         = TASD_KEY_INPUT_CHUNK;
    p.payload     = raw;
    p.payload_len = sizeof(raw);

    CHECK_EQ(tasd_decode_input_chunk(&p, &ic), TASD_OK);
    CHECK_EQ(ic.port,       1u);
    CHECK_EQ(ic.inputs_len, 3u);
    CHECK_EQ(ic.inputs[0],  0xFFu);
    CHECK_EQ(ic.inputs[2],  0xFDu);
}

/* ---- INPUT_MOMENT decode -------------------------------------------------- */

static void test_input_moment(void)
{
    uint8_t raw[12];
    tasd_packet_t p;
    tasd_pkt_input_moment_t im;

    raw[0] = 2;                     /* port */
    raw[1] = 1;                     /* hold = TRUE */
    raw[2] = TASD_INDEX_FRAME;      /* index_type */
    /* index = 500 (big-endian uint64) */
    raw[3] = 0; raw[4] = 0; raw[5] = 0; raw[6] = 0;
    raw[7] = 0; raw[8] = 0; raw[9] = 0x01; raw[10] = 0xF4;
    raw[11] = 0xAB;                 /* 1 input byte */

    p.key         = TASD_KEY_INPUT_MOMENT;
    p.payload     = raw;
    p.payload_len = sizeof(raw);

    CHECK_EQ(tasd_decode_input_moment(&p, &im), TASD_OK);
    CHECK_EQ(im.port,       2u);
    CHECK_EQ(im.hold,       1u);
    CHECK_EQ(im.index_type, (uint8_t)TASD_INDEX_FRAME);
    CHECK_EQ(im.index,      (uint64_t)500u);
    CHECK_EQ(im.inputs_len, 1u);
    CHECK_EQ(im.inputs[0],  0xABu);
}

/* ---- MEMORY_INIT decode --------------------------------------------------- */

static void test_memory_init(void)
{
    uint8_t raw[] = {
        TASD_MEMINIT_ALL_00,      /* data_type */
        0x01, 0x01,               /* device = NES CPU RAM */
        0x01,                     /* required = TRUE */
        0x00,                     /* nlen = 0 (no name) */
        /* no data (all-zeros is implicit) */
    };
    tasd_packet_t p;
    tasd_pkt_memory_init_t mi;

    p.key         = TASD_KEY_MEMORY_INIT;
    p.payload     = raw;
    p.payload_len = sizeof(raw);

    CHECK_EQ(tasd_decode_memory_init(&p, &mi), TASD_OK);
    CHECK_EQ(mi.data_type, (uint8_t)TASD_MEMINIT_ALL_00);
    CHECK_EQ(mi.device,    (uint16_t)TASD_MEMINIT_DEV_NES_RAM);
    CHECK_EQ(mi.required,  1u);
    CHECK_EQ(mi.name_len,  0u);
    CHECK_EQ(mi.data_len,  0u);
}

/* ---- TRANSITION decode ---------------------------------------------------- */

static void test_transition(void)
{
    uint8_t raw[11];
    tasd_packet_t p;
    tasd_pkt_transition_t tr;

    raw[0] = 1;                             /* port */
    raw[1] = TASD_INDEX_FRAME;              /* index_type */
    /* index = 1000 */
    raw[2] = 0; raw[3] = 0; raw[4] = 0; raw[5] = 0;
    raw[6] = 0; raw[7] = 0; raw[8] = 0x03; raw[9] = 0xE8;
    raw[10] = TASD_TRANSITION_SOFT_RESET;   /* type */

    p.key         = TASD_KEY_TRANSITION;
    p.payload     = raw;
    p.payload_len = sizeof(raw);

    CHECK_EQ(tasd_decode_transition(&p, &tr), TASD_OK);
    CHECK_EQ(tr.port,             1u);
    CHECK_EQ(tr.index_type,       (uint8_t)TASD_INDEX_FRAME);
    CHECK_EQ(tr.index,            (uint64_t)1000u);
    CHECK_EQ(tr.type,             (uint8_t)TASD_TRANSITION_SOFT_RESET);
    CHECK_EQ(tr.inner_packet_len, 0u);
}

/* ---- Controller input sizes ---------------------------------------------- */

static void test_controller_input_sizes(void)
{
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_NES_STANDARD),  1u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_NES_FOUR_SCORE), 3u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_SNES_STANDARD), 2u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_SNES_MULTITAP), 5u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_SNES_MOUSE),    4u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_N64_STANDARD),  4u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_GC_STANDARD),   8u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_GB_STANDARD),   1u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_GBA_STANDARD),  2u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_GENESIS_3BTN),  1u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_GENESIS_6BTN),  2u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_A2600_JOYSTICK),1u);
    CHECK_EQ(tasd_controller_input_size(TASD_CTRL_OTHER),         0u);
    CHECK_EQ(tasd_controller_input_size(0x0000u),                 0u);
}

/* ---- Empty-payload packet ------------------------------------------------ */

static void test_empty_payload(void)
{
    uint8_t       buf[32];
    tasd_writer_t w;
    tasd_reader_t r;
    tasd_header_t hdr;
    tasd_packet_t pkt;

    tasd_writer_init(&w, buf, sizeof(buf));
    CHECK_EQ(tasd_writer_write_header(&w), TASD_OK);
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_UNSPECIFIED, NULL, 0u), TASD_OK);

    CHECK_EQ(tasd_read_header(buf, tasd_writer_size(&w), &hdr), TASD_OK);
    tasd_reader_init(&r, buf, tasd_writer_size(&w), &hdr);

    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_OK);
    CHECK_EQ(pkt.key,         (uint16_t)TASD_KEY_UNSPECIFIED);
    CHECK_EQ(pkt.payload_len, 0u);
    CHECK_EQ(tasd_reader_next(&r, &pkt), TASD_ERR_END);
}

/* ---- Buffer-too-small writer --------------------------------------------- */

static void test_writer_overflow(void)
{
    uint8_t       buf[TASD_HEADER_SIZE + 4];  /* room for header + tiny packet */
    tasd_writer_t w;
    uint8_t       payload[16];

    memset(payload, 0, sizeof(payload));

    tasd_writer_init(&w, buf, sizeof(buf));
    CHECK_EQ(tasd_writer_write_header(&w), TASD_OK);
    /* This packet is too large to fit */
    CHECK_EQ(tasd_writer_append(&w, TASD_KEY_GAME_TITLE, payload, sizeof(payload)),
             TASD_ERR_BUFFER_SMALL);
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    test_header_roundtrip();
    test_header_bad_magic();
    test_header_too_short();
    test_writer_reader_roundtrip();
    test_encode_helpers();
    test_input_chunk();
    test_input_moment();
    test_memory_init();
    test_transition();
    test_controller_input_sizes();
    test_empty_payload();
    test_writer_overflow();

    if (g_failures == 0) {
        printf("All tests passed.\n");
        return 0;
    }
    fprintf(stderr, "%d test(s) failed.\n", g_failures);
    return 1;
}
