#include "RC522.h"
AppStatus rc522_init(void) { return APP_NOT_READY; }
AppStatus rc522_poll(RfidUid *uid) { (void)uid; return APP_NOT_READY; }
AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level) { (void)uid; (void)target_level; return APP_NOT_READY; }
