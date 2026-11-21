#ifndef ABYSSAL_RC522_H
#define ABYSSAL_RC522_H
#include "AppTypes.h"

/* Configure SPI0 using AppConfig.h and check reader register communication.
 * APP_OK: ready; APP_ERROR: reader/configuration failure.
 * A build without the Pico SDK returns APP_UNSUPPORTED.
 */
AppStatus rc522_init(void);

/* Present one ISO/IEC 14443 Type A card at a time.
 * APP_OK: a complete 4-, 7- or 10-byte UID is returned once per tap.
 * APP_NOT_READY: no new card (also includes a previously read, held card).
 * APP_ERROR: uninitialized reader, bad argument, SPI/protocol error or collision.
 * length is cleared on failure. Each uint16_t uid[] element holds one UID byte.
 * Remove the card fully from the antenna field before tapping it again.
 */
AppStatus rc522_poll(RfidUid *uid);

typedef struct {
    RfidUid uid;
    uint8_t atqa[2]; /* Bytes in received order, not a host-endian integer. */
    uint8_t sak;
    const char *type_name; /* SAK-based family, not proof of the exact IC. */
    const char *memory_read_support; /* Library capability; no access attempted. */
} RfidCardInfo;

/* Copy metadata from the last successful rc522_poll without any RF operation.
 * Static description strings remain valid; do not modify/free them.
 * Invalidated by init or the next poll (including no card/error).
 * NULL -> APP_ERROR; no successful poll -> APP_NOT_READY; host -> APP_UNSUPPORTED.
 * Output is cleared on failure. No authentication or card-memory access. */
AppStatus rc522_get_card_info(RfidCardInfo *info);

/* Explicit read-only diagnostic for the identified 4-byte UID, SAK 08
 * Classic 1K-compatible card. Re-selects/verifies uid, tries factory Key A
 * FF FF FF FF FF FF for sector 1, and reads only data block 4 (16 bytes).
 * No writes, key changes, memory scan or target-format assumptions.
 * Caller supplies 16 bytes; output is cleared on failure. NULL/bad UID ->
 * APP_ERROR; absent card -> APP_NOT_READY; other card types -> APP_UNSUPPORTED.
 * Authentication/read/cleanup failures -> APP_ERROR. Remove/re-tap on failure.
 * Call from the same control loop as rc522_poll; card must remain in range. */
AppStatus rc522_read_block4(const RfidUid *uid, uint8_t data[16]);

/* Read actual version-1 ABWL raw-ADC target record from block 4. Uses the
 * same card/key restrictions as rc522_read_block4. Never writes.
 * APP_NOT_READY: absent card or no valid target record; APP_ERROR: bad args,
 * hardware/authentication/CRC/cleanup failure; APP_UNSUPPORTED: other type/host.
 * A valid output pointer is cleared on failure.
 */
AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level);

/* Explicit provisioning ONLY: write a version-1 raw-ADC target (0..4095)
 * to blank (all 00 or all FF) block 4 of the same verified card, then read
 * back and compare every byte. An identical record succeeds without writing;
 * all other existing contents are refused. Same card/key/status restrictions
 * as rc522_read_block4. Never touches manufacturer/trailer blocks or keys.
 * APP_ERROR after a write may mean partial/unverified contents: inspect the
 * card before retrying. Ordinary poll/read APIs never call this function. */
AppStatus rc522_provision_target_level(const RfidUid *uid, uint16_t target_level);
#endif
