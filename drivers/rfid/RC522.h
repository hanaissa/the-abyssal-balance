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

/* Not implemented: APP_UNSUPPORTED, with *target_level left unchanged.
 * NULL arguments return APP_ERROR. No card memory is read or written.
 */
AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level);
#endif
