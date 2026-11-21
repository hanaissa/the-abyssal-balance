# RFID identification, target reading and controlled provisioning

`main.c` initializes the reader once, polls for a card and prints its UID/length,
ATQA bytes in received order, SAK, PICC family and library memory-read capability
over USB serial. It calls `rc522_poll()` then `rc522_get_card_info()`; metadata
comes from that same successful selection and expires on the next poll/init.
The test uses only `RC522.h`; the driver wraps the C pico-mfrc522
library in `third_party/`. Its README records the source, license and local fixes.

For a four-byte UID with SAK 08, it then calls the production
`rc522_read_block4()` while the card remains in range. That function wakes and
reselects the same UID, tries factory Key A `FF FF FF FF FF FF` for sector 1,
and reads data block 4. It prints the 16 data bytes as hex and printable ASCII.
Authentication can fail if the key differs, access is denied, or communication
fails. No other sector/block is scanned. Remove and re-tap after each attempt.

Normal scans remain read-only. Writing requires this exact USB serial command,
followed by Enter, with the displayed four-byte UID as eight hex digits:

```text
SET 1 3D91DFEE
```

This provisions Card 1 with 750 raw ADC counts. Use `SET 2 <actual UID>` for
Card 2 (1000), and `SET 3 <actual UID>` for Card 3 (500), substituting each
displayed UID without spaces or angle brackets. Scan just one card and keep it
in range until `VERIFIED` appears. Remove and label it before scanning the next.
Commands have no local echo by default in miniterm; Enter submits the line.
Malformed/overlong commands and a UID different from the last scan never write.
Each operation also reselects and matches the actual card UID before memory access.

The production `rc522_provision_target_level()` writes ONLY block 4 when its
previous contents are all 00 or all FF. Unknown contents, corrupt records, and
different existing targets are refused. Repeating the identical target verifies
without another EEPROM write. Manufacturer blocks, sector trailers, access bits
and keys are never written. Authentication uses factory Key A only.
All 16 written bytes are read back and compared; the diagnostic then separately
calls production `rc522_read_target_level()` and prints the value read from memory.
If writing/readback fails, inspect the block before retrying; there is no automatic
retry or recovery overwrite. A partial write can leave an invalid record.

## Target record, version 1

| Block 4 bytes | Meaning |
|---|---|
| 0..3 | ASCII `ABWL` |
| 4 | Version 1 |
| 5 | Units 0: raw ADC counts |
| 6..7 | Target unsigned 16-bit little-endian, range 0..4095 |
| 8..9 | Bytewise complement of bytes 6..7 |
| 10..15 | Reserved zero bytes |

These checks reject uninitialized/malformed records; they are not cryptographic
authentication of the target value. The transport library also checks read CRC.
The reader never invents a target or looks it up from a UID. Targets listed in
this diagnostic are write inputs only; production reads decode stored bytes.

## Wiring

GPIO assignments come from `include/AppConfig.h`:

| RC522 | Pico |
|---|---|
| MISO | GP16 |
| SDA / SS / NSS | GP17 |
| SCK | GP18 |
| MOSI | GP19 |
| RST | GP20 |
| 3.3V | 3V3 OUT |
| GND | GND |
| IRQ | Unconnected |

Use 3.3V power/logic. Remove any MOSI-to-MISO loopback jumper before connecting
this reader. SPI0 uses 1 MHz, mode 0. Present one Type A card at a time.

## Build and run

With the Pico SDK, ARM toolchain and Ninja available:

```powershell
cmake -S . -B build-pico -G Ninja -DABYSSAL_USE_PICO_SDK=ON -DPICO_BOARD=pico -DPICO_SDK_PATH="C:/path/to/pico-sdk"
cmake --build build-pico --target RFID_Test
```

For an already configured build, only the second command is needed. The firmware
is `build-pico/tests/RFID_Test.uf2`. Hold BOOTSEL while connecting USB and copy
that file to RPI-RP2. Open the Pico's USB serial port at 115200; the demo waits
for the serial connection before initializing the reader.

Example format; the actual UID comes from your card:

```text
UID (4 bytes): 04 00 A1 2B
ATQA bytes (received order): 04 00
SAK: 08
Detected PICC family (SAK): MIFARE 1KB
```

All 4, 7 or 10 bytes are printed. The library HALTs the card after reading it,
so holding it in place does not repeat the UID. Remove it fully and tap again
for another read. A healthy reader with no new card returns `APP_NOT_READY`;
reader/protocol errors return `APP_ERROR` and are printed separately.

The LCD/indicator drivers are not integrated into this demo; output is USB serial.
`rc522_read_target_level()` is implemented in the production driver and only reads.
It returns `APP_NOT_READY` for blank/invalid records and clears the output on
failure. Uniform 00/FF data is not interpreted as a target. A read of block 4 does
not establish whether another block contains an existing target.
The explicit SET command is the only write entry point in this diagnostic.
There are no format, key-change or access-condition commands. Reader-chip
register configuration is not a card-memory write. Capability text describes
library APIs, not a successful access test or proof of exact chip identity.
In particular SAK 00 does not distinguish Ultralight from NTAG variants.
Only four-byte-UID, SAK-08 cards are accepted by the block read/provisioning APIs.
Other card types need separate identification/layout decisions.

The owner's identification run reported a four-byte UID, ATQA bytes `04 00`,
and SAK `08` (Classic 1K-compatible), then successfully read sixteen 00 bytes
from block 4 with the factory key. Provisioning and persistent target readback
still require physical testing. The temporary SPI loopback and desktop
simulator scaffolding have been removed; this folder retains the hardware demo.
