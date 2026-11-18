#ifndef ABYSSAL_STATE_MACHINE_H
#define ABYSSAL_STATE_MACHINE_H
#include "AppEvents.h"
typedef struct { SystemState state; uint16_t target_level; } StateMachine;
void state_machine_init(StateMachine *machine);
void state_machine_update(StateMachine *machine, const AppEvent *event);
SystemState state_machine_state(const StateMachine *machine);
#endif
