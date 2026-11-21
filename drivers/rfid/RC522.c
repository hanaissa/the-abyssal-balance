#include "RC522.h"
#include <string.h>

/* Block 4 record: ABWL, version 1, raw-ADC units 0, uint16 LE target,
 * bytewise complement of target, then six reserved zero bytes. */
static bool decode_target(const uint8_t data[16], uint16_t *target) {
    if (memcmp(data, "ABWL", 4) != 0 || data[4] != 1 || data[5] != 0 ||
        (unsigned)data[6] + data[8] != 255 || (unsigned)data[7] + data[9] != 255) {
        return false;
    }
    for (unsigned i = 10; i < 16; ++i) { if (data[i]) { return false; } }
    uint16_t value = (uint16_t)((uint16_t)data[6] | (uint16_t)data[7] << 8);
    if (value > 4095) { return false; }
    *target = value;
    return true;
}

#if defined(ABYSSAL_USE_PICO_SDK) && ABYSSAL_USE_PICO_SDK
#include "mfrc522.h"

static MFRC522Ptr_t reader;
static bool initialized;
static uint8_t reader_version;
static RfidCardInfo last_card;
static bool last_card_valid;

static const char *memory_read_support(PICC_Type type) {
    switch (type) {
        case PICC_TYPE_MIFARE_MINI:
        case PICC_TYPE_MIFARE_1K:
        case PICC_TYPE_MIFARE_4K:
            return "Classic block read API available; correct sector key/access required. Not tested.";
        case PICC_TYPE_MIFARE_UL:
            return "Type 2/Ultralight read API available; exact variant/protection must be identified. Not tested.";
        default:
            return "No generic memory-read support established for this classification.";
    }
}

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
    memset(&last_card, 0, sizeof last_card);
    last_card_valid = false;
    initialized = false;
    if (!reader) { reader = MFRC522_Init(); } // Allocate once, even on re-init.
    if (!reader) { return APP_ERROR; }
    PCD_Init(reader, spi0); // Library uses the project's AppConfig.h pins.
    reader_version = PCD_ReadRegister(reader, VersionReg);
    initialized = reader_healthy();
    return initialized ? APP_OK : APP_ERROR;
}

AppStatus rc522_poll(RfidUid *uid) {
    memset(&last_card, 0, sizeof last_card);
    last_card_valid = false;
    if (!uid) { return APP_ERROR; }
    *uid = (RfidUid){0};
    if (!initialized || !reader_healthy()) { return APP_ERROR; }

    uint8_t atqa[2], length = sizeof(atqa);
    // Status-returning form of PICC_IsNewCardPresent(): preserves errors.
    StatusCode status = PICC_RequestA(reader, atqa, &length);
    if (status == STATUS_TIMEOUT) {
        return card_timeout() ? APP_NOT_READY : APP_ERROR;
    }
    if (status != STATUS_OK || length != sizeof atqa || !PICC_ReadCardSerial(reader)) { return APP_ERROR; }
    uint8_t size = reader->uid.size;
    if (size != 4 && size != 7 && size != 10) { return APP_ERROR; }

    // HALTed cards ignore new-card requests until removed from the RF field.
    if (PICC_HaltA(reader) != STATUS_OK || !card_timeout()) { return APP_ERROR; }
    uid->length = size;
    for (uint8_t i = 0; i < size; ++i) { uid->uid[i] = reader->uid.uidByte[i]; }
    PICC_Type type = PICC_GetType(reader->uid.sak);
    last_card.uid = *uid;
    memcpy(last_card.atqa, atqa, sizeof atqa);
    last_card.sak = reader->uid.sak;
    last_card.type_name = PICC_GetTypeName(type);
    last_card.memory_read_support = memory_read_support(type);
    last_card_valid = true;
    return APP_OK;
}

AppStatus rc522_get_card_info(RfidCardInfo *info) {
    if (!info) { return APP_ERROR; }
    memset(info, 0, sizeof *info);
    if (!last_card_valid) { return APP_NOT_READY; }
    *info = last_card;
    return APP_OK;
}

static AppStatus block4_transaction(const RfidUid *uid, uint8_t data[16],
                                    const uint8_t *provision) {
    if (!data) { return APP_ERROR; }
    memset(data, 0, 16);
    if (!uid || uid->length != 4) { return APP_ERROR; }
    for (unsigned i = 0; i < uid->length; ++i) {
        if (uid->uid[i] > 255) { return APP_ERROR; }
    }
    if (!initialized || !reader_healthy()) { return APP_ERROR; }

    /* poll() HALTs cards. Wake and select again; never trust an old UID or
     * read a different card that replaced it. Four-byte UIDs avoid the
     * vendored authentication routine's seven-byte UID limitation. */
    PCD_StopCrypto1(reader);
    uint8_t atqa[2], atqa_length = sizeof atqa;
    StatusCode status = PICC_WakeupA(reader, atqa, &atqa_length);
    if (status != STATUS_OK || atqa_length != sizeof atqa) {
        return status == STATUS_TIMEOUT && card_timeout() ? APP_NOT_READY : APP_ERROR;
    }
    if (!PICC_ReadCardSerial(reader)) { return APP_ERROR; }

    AppStatus result = APP_ERROR;
    uint8_t buffer[18], length = sizeof buffer;
    if (reader->uid.size != uid->length) { goto cleanup; }
    for (unsigned i = 0; i < uid->length; ++i) {
        if (reader->uid.uidByte[i] != uid->uid[i]) { goto cleanup; }
    }
    if ((reader->uid.sak & 0x7f) != 0x08) {
        result = APP_UNSUPPORTED;
        goto cleanup;
    }
    MIFARE_Key key = {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
    status = PCD_Authenticate(reader, PICC_CMD_MF_AUTH_KEY_A, 4, &key, &reader->uid);
    if (status != STATUS_OK) { goto cleanup; }
    status = MIFARE_Read(reader, 4, buffer, &length);
    if (status != STATUS_OK || length != sizeof buffer) { goto cleanup; }
    if (provision && memcmp(buffer, provision, 16) != 0) {
        bool zero = true, ff = true;
        for (unsigned i = 0; i < 16; ++i) {
            zero = zero && buffer[i] == 0;
            ff = ff && buffer[i] == 0xff;
        }
        /* Never overwrite unknown contents, another target or a corrupt
         * record. Identical records above are idempotent (no EEPROM write). */
        if (!zero && !ff) { goto cleanup; }
        uint8_t record[16];
        memcpy(record, provision, sizeof record);
        status = MIFARE_Write(reader, 4, record, sizeof record);
        if (status != STATUS_OK) { goto cleanup; }
        length = sizeof buffer;
        status = MIFARE_Read(reader, 4, buffer, &length);
        if (status != STATUS_OK || length != sizeof buffer ||
            memcmp(buffer, provision, 16) != 0) { goto cleanup; }
    }
    result = APP_OK;

cleanup:
    /* HALT while authentication is still active, then always stop Crypto1.
     * A successful read is not returned if cleanup/reader health failed. */
    status = PICC_HaltA(reader);
    PCD_StopCrypto1(reader);
    if (status != STATUS_OK || !reader_healthy()) { result = APP_ERROR; }
    if (result == APP_OK) { memcpy(data, buffer, 16); }
    return result;
}

AppStatus rc522_read_block4(const RfidUid *uid, uint8_t data[16]) {
    return block4_transaction(uid, data, NULL);
}

AppStatus rc522_provision_target_level(const RfidUid *uid, uint16_t target_level) {
    if (target_level > 4095) { return APP_ERROR; }
    uint8_t record[16] = {'A', 'B', 'W', 'L', 1, 0};
    record[6] = (uint8_t)target_level;
    record[7] = (uint8_t)(target_level >> 8);
    record[8] = (uint8_t)~record[6];
    record[9] = (uint8_t)~record[7];
    uint8_t verified[16];
    return block4_transaction(uid, verified, record);
}

#else

AppStatus rc522_init(void) { return APP_UNSUPPORTED; }
AppStatus rc522_poll(RfidUid *uid) {
    if (!uid) { return APP_ERROR; }
    *uid = (RfidUid){0};
    return APP_UNSUPPORTED;
}
AppStatus rc522_get_card_info(RfidCardInfo *info) {
    if (!info) { return APP_ERROR; }
    memset(info, 0, sizeof *info);
    return APP_UNSUPPORTED;
}
AppStatus rc522_read_block4(const RfidUid *uid, uint8_t data[16]) {
    if (!data) { return APP_ERROR; }
    memset(data, 0, 16);
    if (!uid || uid->length != 4) { return APP_ERROR; }
    for (unsigned i = 0; i < uid->length; ++i) {
        if (uid->uid[i] > 255) { return APP_ERROR; }
    }
    return APP_UNSUPPORTED;
}
AppStatus rc522_provision_target_level(const RfidUid *uid, uint16_t target_level) {
    if (!uid || uid->length != 4 || target_level > 4095) { return APP_ERROR; }
    for (unsigned i = 0; i < uid->length; ++i) {
        if (uid->uid[i] > 255) { return APP_ERROR; }
    }
    return APP_UNSUPPORTED;
}

#endif

AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level) {
    if (!target_level) { return APP_ERROR; }
    *target_level = 0;
    if (!uid) { return APP_ERROR; }
    uint8_t data[16];
    AppStatus status = rc522_read_block4(uid, data);
    if (status != APP_OK) { return status; }
    return decode_target(data, target_level) ? APP_OK : APP_NOT_READY;
}
