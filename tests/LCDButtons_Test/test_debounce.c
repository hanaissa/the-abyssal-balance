#include <stdio.h>
#include <stdlib.h>
#include "Buttons.h"
#include "AppConfig.h"
static unsigned samples[2], phase;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); exit(1); } } while (0)
void gpio_init(unsigned pin) { CHECK(pin == PIN_SUBMIT_BUTTON || pin == PIN_RESET_BUTTON); }
void gpio_set_dir(unsigned pin, bool output) { (void)pin; CHECK(!output); }
void gpio_pull_up(unsigned pin) { (void)pin; }
bool gpio_get(unsigned pin) { return (samples[phase] & (1u << pin)) != 0; }
void sleep_ms(uint32_t ms) { CHECK(ms == 20); phase = 1; }
static void check_samples(bool submit1, bool submit2, bool reset1, bool reset2,
                          bool expected_submit, bool expected_reset) {
    samples[0] = (!submit1 << PIN_SUBMIT_BUTTON) | (!reset1 << PIN_RESET_BUTTON);
    samples[1] = (!submit2 << PIN_SUBMIT_BUTTON) | (!reset2 << PIN_RESET_BUTTON);
    phase = 0;
    ButtonEvents events;
    CHECK(buttons_poll_debounced(&events) == APP_OK);
    CHECK(events.submit_pressed == expected_submit && events.reset_pressed == expected_reset);
}
int main(void) {
    ButtonEvents events = {true, true};
    CHECK(buttons_poll_debounced(NULL) == APP_ERROR);
    CHECK(buttons_poll_debounced(&events) == APP_NOT_READY);
    CHECK(!events.submit_pressed && !events.reset_pressed);
    CHECK(buttons_init() == APP_OK);
    check_samples(false,false,false,false,false,false);
    check_samples(true,false,false,false,false,false); // bounce rejected
    check_samples(true,true,false,false,true,false);   // first press
    check_samples(true,true,false,false,false,false);  // held: no repeat
    check_samples(false,true,false,false,false,false); // release bounce
    check_samples(false,false,false,false,false,false);
    check_samples(true,true,true,true,true,true);      // independent buttons
    CHECK(buttons_init() == APP_OK);                   // reset edge state
    check_samples(true,true,true,true,true,true);
    puts("PASS actual button driver: debounce, held key, bounce, release/repress, NULL and reinit.");
    return 0;
}
