#include "StateMachine.h"
int main(void) { StateMachine machine; state_machine_init(&machine); return (int)state_machine_state(&machine); }
