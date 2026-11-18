#include "StateMachine.h"
void state_machine_init(StateMachine *machine) { if (machine) { machine->state = STATE_WAITING_FOR_RFID; machine->target_level = 0; } }
void state_machine_update(StateMachine *machine, const AppEvent *event) {
    if (!machine || !event) return;
    switch (event->type) {
        case APP_EVENT_RFID_AUTHORIZED: machine->target_level = event->target_level; machine->state = STATE_BALANCING; break;
        case APP_EVENT_SUBMIT: machine->state = STATE_CHECKING; break;
        case APP_EVENT_RESET: machine->state = STATE_RESETTING; break;
        case APP_EVENT_OCR_READY: machine->state = STATE_REWARD_SEQUENCE; break;
        case APP_EVENT_ERROR: machine->state = STATE_FAILURE_RECOVERY; break;
        default: break;
    }
}
SystemState state_machine_state(const StateMachine *machine) { return machine ? machine->state : STATE_RESETTING; }
