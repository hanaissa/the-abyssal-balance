#ifndef ABYSSAL_RC522_H
#define ABYSSAL_RC522_H
#include "AppTypes.h"
AppStatus rc522_init(void);
AppStatus rc522_poll(RfidUid *uid);
AppStatus rc522_read_target_level(const RfidUid *uid, uint16_t *target_level);
#endif
