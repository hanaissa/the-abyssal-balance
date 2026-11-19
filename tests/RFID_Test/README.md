# RFID UID demo

`main.c` initializes the reader once, polls for a card and prints its UID over
USB serial. The test uses only `RC522.h`; the driver wraps the C pico-mfrc522
library in `third_party/`. Its README records the source, license and local fixes.

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
```

All 4, 7 or 10 bytes are printed. The library HALTs the card after reading it,
so holding it in place does not repeat the UID. Remove it fully and tap again
for another read. A healthy reader with no new card returns `APP_NOT_READY`;
reader/protocol errors return `APP_ERROR` and are printed separately.

The LCD/indicator drivers are not integrated into this demo; output is USB serial.
No card memory is written. `rc522_read_target_level()` returns `APP_UNSUPPORTED`
and leaves its output unchanged; target storage format is still undecided.
Hardware operation remains unverified. The temporary SPI loopback and desktop
simulator scaffolding have been removed; this folder retains the hardware demo.
