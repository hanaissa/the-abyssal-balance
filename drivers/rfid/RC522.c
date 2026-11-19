#include "RC522.h"

#if defined(ABYSSAL_USE_PICO_SDK) && ABYSSAL_USE_PICO_SDK
#include "mfrc522.h"

static MFRC522Ptr_t reader;
static bool initialized;
static uint8_t reader_version;

/* Use the library's register API only to distinguish SPI faults from no card. */
static bool reader_healthy(void) {
    return reader && PCD_ReadRegister(reader, VersionReg) == reader_version &&
           reader_version != 0 && reader_version != 0xFF &&
           PCD_ReadRegister(reader, TModeReg) == 0x80 &&
           (PCD_ReadRegister(reader, TxControlReg) & 3) == 3;
}

static bool card_timeout(void) {
    // The library uses STATUS_TIMEOUT for both RF silence and a stuck reader.
    return reader_healthy() && (PCD_ReadRegister(reader, ComIrqReg) & 1) &&
           !(PCD_ReadRegister(reader, ErrorReg) & 0xDB);
}

AppStatus rc522_init(void) {
    initialized = false;
    if (!reader) { reader = MFRC522_Init(); } // Allocate once, even on re-init.
    if (!reader) { return APP_ERROR; }
    PCD_Init(reader, spi0); // Library uses the project's AppConfig.h pins.
    reader_version = PCD_ReadRegister(reader, VersionReg);
    initialized = reader_healthy();
    return initialized ? APP_OK : APP_ERROR;
}

AppStatus rc522_poll(RfidUid *uid) {
    if (!uid) { return APP_ERROR; }
    *uid = (RfidUid){0};
    if (!initialized || !reader_healthy()) { return APP_ERROR; }

    uint8_t atqa[2], length = sizeof(atqa);
    // Status-returning form of PICC_IsNewCardPresent(): preserves errors.
    StatusCode status = PICC_RequestA(reader, atqa, &length);
    if (status == STATUS_TIMEOUT) {
        return card_timeout() ? APP_NOT_READY : APP_ERROR;
    }
    if (status != STATUS_OK || !PICC_ReadCardSerial(reader)) { return APP_ERROR; }
    uint8_t size = reader->uid.size;
    if (size != 4 && size != 7 && size != 10) { return APP_ERROR; }

    // HALTed cards ignore new-card requests until removed from the RF field.
    if (PICC_HaltA(reader) != STATUS_OK || !card_timeout()) { return APP_ERROR; }
    uid->length = size;
    for (uint8_t i = 0; i < size; ++i) { uid->uid[i] = reader->uid.uidByte[i]; }
    return APP_OK;
}

#else

AppStatus rc522_init(void) { return APP_UNSUPPORTED; }
AppStatus rc522_poll(RfidUid *uid) {
    if (!uid) { return APP_ERROR; }
    *uid = (RfidUid){0};
    return APP_UNSUPPORTED;
}

#endif

AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level) {
    if (!uid || !target_level) { return APP_ERROR; }
    return APP_UNSUPPORTED;
}
