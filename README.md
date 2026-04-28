# TASD

A minimal, platform-agnostic C99 library for reading and writing
[TASD](https://tasd.io/) (Tool Assisted Speedrun Dump) files.

The library is designed to be:

* **Portable** — pure C99, no platform-specific headers beyond `<stdint.h>` and `<string.h>`
* **Allocation-free** — every operation works on caller-supplied buffers; `malloc` is never called
* **Lightweight** — two files (`include/tasd.h`, `src/tasd.c`); no external dependencies
* **Compatible with the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)** and any hosted desktop toolchain

---

## File format

TASD is a key-based binary packet format defined at <https://tasd.io/> (spec
source: [tasd-org/tasd-spec](https://github.com/tasd-org/tasd-spec)).

A TASD file begins with a 7-byte header followed by zero or more
variable-length packets.  Each packet carries a 2-byte key, a length prefix,
and an arbitrary payload.

---

## Quick start

### Reading a TASD file

```c
#include "tasd.h"

/* buf / len come from wherever you loaded the file */
void process(const uint8_t *buf, size_t len) {
    tasd_header_t hdr;
    if (tasd_read_header(buf, len, &hdr) != TASD_OK) { /* bad file */ return; }

    tasd_reader_t r;
    tasd_reader_init(&r, buf, len, &hdr);

    tasd_packet_t pkt;
    while (tasd_reader_next(&r, &pkt) == TASD_OK) {
        switch (pkt.key) {
            case TASD_KEY_CONSOLE_TYPE: {
                tasd_pkt_console_type_t ct;
                tasd_decode_console_type(&pkt, &ct);
                /* use ct.console, ct.name, ct.name_len */
                break;
            }
            case TASD_KEY_TOTAL_FRAMES: {
                uint32_t frames = 0;
                tasd_decode_uint32(&pkt, &frames);
                break;
            }
            case TASD_KEY_INPUT_CHUNK: {
                tasd_pkt_input_chunk_t ic;
                tasd_decode_input_chunk(&pkt, &ic);
                /* ic.port, ic.inputs, ic.inputs_len */
                break;
            }
            default:
                /* unknown or unhandled packets are safely skipped */
                break;
        }
    }
}
```

### Writing a TASD file

```c
#include "tasd.h"

void build_file(uint8_t *buf, size_t len) {
    tasd_writer_t w;
    tasd_writer_init(&w, buf, len);
    tasd_writer_write_header(&w);

    /* CONSOLE_TYPE = NES */
    uint8_t ct_payload[1] = { TASD_CONSOLE_NES };
    tasd_writer_append(&w, TASD_KEY_CONSOLE_TYPE, ct_payload, 1);

    /* TOTAL_FRAMES = 5000 */
    uint8_t frames_payload[4];
    tasd_encode_uint32(5000, frames_payload, sizeof(frames_payload));
    tasd_writer_append(&w, TASD_KEY_TOTAL_FRAMES, frames_payload, 4);

    /* INPUT_CHUNK for port 1 */
    uint8_t ic_payload[128];
    /* fill ic_payload with NES input bytes … */
    uint8_t ic_buf[129];
    int n = tasd_encode_input_chunk(1, ic_payload, sizeof(ic_payload),
                                    ic_buf, sizeof(ic_buf));
    tasd_writer_append(&w, TASD_KEY_INPUT_CHUNK, ic_buf, (uint32_t)n);

    size_t file_size = tasd_writer_size(&w);
    /* write buf[0..file_size-1] to storage */
    (void)file_size;
}
```

---

## Building

### Desktop (CMake)

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

### As part of a Pico SDK project

Add this repository as a subdirectory in your project's `CMakeLists.txt`:

```cmake
add_subdirectory(path/to/TASD)
target_link_libraries(your_target PRIVATE tasd)
```

No additional configuration is required; the library does not depend on any
Pico SDK APIs.

---

## API overview

| Function | Description |
|---|---|
| `tasd_read_header` | Parse the 7-byte file header |
| `tasd_write_header` | Write the 7-byte file header |
| `tasd_reader_init` | Initialise a forward-only packet iterator |
| `tasd_reader_next` | Advance to the next packet |
| `tasd_writer_init` | Initialise a packet writer |
| `tasd_writer_write_header` | Write the header through the writer |
| `tasd_writer_append` | Append a packet (key + framing + payload) |
| `tasd_writer_size` | Bytes written so far |
| `tasd_controller_input_size` | Bytes per input frame for a controller type |
| `tasd_decode_*` | Typed payload decoders for each packet type |
| `tasd_encode_*` | Typed payload encoders for each packet type |

All constants (`TASD_KEY_*`, `TASD_CONSOLE_*`, `TASD_CTRL_*`, etc.) are
defined in `include/tasd.h`.

---

## License

See [LICENSE](LICENSE).