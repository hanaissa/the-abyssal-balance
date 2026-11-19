# Vendored Pico C RC522 library

Upstream: https://github.com/BenjaminModica/pico-mfrc522

Pinned revision: `70c0d8deda9479e18113bd854bdd1afa262bef37`.

License: Unlicense / public domain; see the unchanged `UNLICENSE.md`.
Only `mfrc522.c`, `mfrc522.h` and the license are copied from upstream.

Actual upstream API used by the application wrapper:

```c
MFRC522Ptr_t MFRC522_Init();
void PCD_Init(MFRC522Ptr_t mfrc, spi_inst_t *spi);
StatusCode PICC_RequestA(MFRC522Ptr_t mfrc, uint8_t *bufferATQA, uint8_t *bufferSize);
bool PICC_ReadCardSerial(MFRC522Ptr_t mfrc);
StatusCode PICC_HaltA(MFRC522Ptr_t mfrc);
```

`PICC_IsNewCardPresent()` is also available, but returns only a boolean. The
wrapper uses `PICC_RequestA()` to retain timeout/error information. The UID is in
`mfrc->uid.uidByte[0..mfrc->uid.size-1]`. `PCD_ReadRegister()` is used for basic
reader-health and hardware-timer checks; the library conflates software and RF
timeouts into `STATUS_TIMEOUT`.

Local integration fixes, kept in this dependency rather than reimplementing
the protocol in `drivers/rfid/RC522.c`:

- Source GPIOs from `AppConfig.h` instead of duplicating numeric pin assignments.
- Initialize the reset pin as an output before toggling it.
- Honor the SPI instance passed to `PCD_Init()` (SPI0, 1 MHz in this project).
- Wait 50 ms after software reset before writing configuration registers.
- Fix nested chip select and retain partial-byte alignment in `PCD_ReadNRegister()`.
- Keep private chip-select declarations in the `.c` file rather than the public header.

The library contains optional card-memory write functions. The project wrapper
and UID demonstration do not call any of them. Builds require no network fetch.
