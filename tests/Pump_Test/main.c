#include <stdio.h>
#include <stdint.h>

#include "../../drivers/motor/L298N.h"
#include "../../drivers/motor/Pump.h"

typedef enum {
    CALL_NONE = 0,
    CALL_INIT,
    CALL_APPLY,
    CALL_STOP
} FakeCall;

typedef struct {
    AppStatus init_status;
    AppStatus apply_status;
    FakeCall calls[8];
    int call_count;
    PumpDirection last_apply_dir;
    uint8_t last_apply_speed;
    PumpDirection active_dir;
    uint8_t active_speed;
} FakeL298N;

typedef struct {
    const char *name;
    AppStatus backend_status;
    int expected_calls;
    FakeCall c0;
    FakeCall c1;
    FakeCall c2;
} InitCase;

typedef struct {
    const char *name;
    PumpCommand cmd;
    AppStatus backend_status;
    AppStatus expected_status;
    int expected_calls;
    FakeCall c0;
    FakeCall c1;
    PumpDirection expected_last_dir;
    uint8_t expected_last_speed;
    PumpDirection expected_active_dir;
    uint8_t expected_active_speed;
} ApplyCase;

static FakeL298N g_fake;
static int g_failures = 0;

#define CHECK(label, expr)                                                    \
    do {                                                                      \
        if (!(expr)) {                                                        \
            fprintf(stderr, "FAIL: %s:%d [%s]: %s\n",                         \
                    __FILE__, __LINE__, (label), #expr);                      \
            g_failures++;                                                     \
        }                                                                     \
    } while (0)

static void fake_record(FakeCall call) {
    if (g_fake.call_count < (int)(sizeof(g_fake.calls) / sizeof(g_fake.calls[0]))) {
        g_fake.calls[g_fake.call_count] = call;
    }
    g_fake.call_count++;
}

static void fake_reset(void) {
    g_fake.init_status = APP_OK;
    g_fake.apply_status = APP_OK;
    g_fake.call_count = 0;
    g_fake.last_apply_dir = PUMP_STOP;
    g_fake.last_apply_speed = 0u;
    g_fake.active_dir = PUMP_STOP;
    g_fake.active_speed = 0u;
}

AppStatus l298n_init(void) {
    fake_record(CALL_INIT);
    return g_fake.init_status;
}

AppStatus l298n_apply(PumpDirection direction, uint8_t speed_percent) {
    fake_record(CALL_APPLY);
    g_fake.last_apply_dir = direction;
    g_fake.last_apply_speed = speed_percent;
    if (g_fake.apply_status == APP_OK) {
        g_fake.active_dir = direction;
        g_fake.active_speed = speed_percent;
    }
    return g_fake.apply_status;
}

void l298n_emergency_stop(void) {
    fake_record(CALL_STOP);
    g_fake.active_dir = PUMP_STOP;
    g_fake.active_speed = 0u;
}

static void check_calls(const char *label, int expected_count,
                        FakeCall c0, FakeCall c1, FakeCall c2) {
    const FakeCall expected[3] = { c0, c1, c2 };
    int i;

    CHECK(label, g_fake.call_count == expected_count);
    for (i = 0; i < expected_count && i < g_fake.call_count; i++) {
        CHECK(label, g_fake.calls[i] == expected[i]);
    }
}

static void arm_running_output(const char *label, PumpDirection dir, uint8_t speed) {
    PumpCommand cmd = { dir, speed };
    g_fake.apply_status = APP_OK;
    CHECK(label, pump_apply(cmd) == APP_OK);
    CHECK(label, g_fake.active_dir == dir);
    CHECK(label, g_fake.active_speed == speed);
    g_fake.call_count = 0;
}

static void run_init_tests(void) {
    const InitCase cases[] = {
        { "init_ok",          APP_OK,          2, CALL_STOP, CALL_INIT, CALL_NONE },
        { "init_not_ready",   APP_NOT_READY,   3, CALL_STOP, CALL_INIT, CALL_STOP },
        { "init_error",       APP_ERROR,       3, CALL_STOP, CALL_INIT, CALL_STOP },
        { "init_unsupported", APP_UNSUPPORTED, 3, CALL_STOP, CALL_INIT, CALL_STOP }
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fake_reset();
        g_fake.active_dir = PUMP_FILL;
        g_fake.active_speed = 80u;
        g_fake.init_status = cases[i].backend_status;

        CHECK(cases[i].name, pump_init() == cases[i].backend_status);
        check_calls(cases[i].name, cases[i].expected_calls,
                    cases[i].c0, cases[i].c1, cases[i].c2);
        CHECK(cases[i].name, g_fake.active_dir == PUMP_STOP);
        CHECK(cases[i].name, g_fake.active_speed == 0u);
    }
}

static void run_apply_tests(void) {
    const ApplyCase cases[] = {
        /* Valid positive FILL/DRAIN including 1 and 100 endpoints */
        { "fill_min",   { PUMP_FILL, 1u },    APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_FILL,  1u,   PUMP_FILL,  1u },
        { "fill_mid",   { PUMP_FILL, 55u },   APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_FILL,  55u,  PUMP_FILL,  55u },
        { "fill_max",   { PUMP_FILL, 100u },  APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_FILL,  100u, PUMP_FILL,  100u },
        { "drain_min",  { PUMP_DRAIN, 1u },   APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_DRAIN, 1u,   PUMP_DRAIN, 1u },
        { "drain_mid",  { PUMP_DRAIN, 42u },  APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_DRAIN, 42u,  PUMP_DRAIN, 42u },
        { "drain_max",  { PUMP_DRAIN, 100u }, APP_OK, APP_OK, 1, CALL_APPLY, CALL_NONE, PUMP_DRAIN, 100u, PUMP_DRAIN, 100u },

        /* Canonicalization: STOP (any speed) and zero-speed FILL/DRAIN -> (PUMP_STOP, 0) */
        { "stop_zero",     { PUMP_STOP, 0u },    APP_OK,        APP_OK,        1, CALL_APPLY, CALL_NONE, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "stop_nonzero",  { PUMP_STOP, 75u },   APP_OK,        APP_OK,        1, CALL_APPLY, CALL_NONE, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "stop_over_100", { PUMP_STOP, 255u },  APP_OK,        APP_OK,        1, CALL_APPLY, CALL_NONE, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "fill_zero",     { PUMP_FILL, 0u },    APP_OK,        APP_OK,        1, CALL_APPLY, CALL_NONE, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "drain_zero",    { PUMP_DRAIN, 0u },   APP_OK,        APP_OK,        1, CALL_APPLY, CALL_NONE, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "stop_not_rdy",  { PUMP_STOP, 75u },   APP_NOT_READY, APP_NOT_READY, 2, CALL_APPLY, CALL_STOP, PUMP_STOP, 0u, PUMP_STOP, 0u },
        { "fill0_not_rdy", { PUMP_FILL, 0u },    APP_NOT_READY, APP_NOT_READY, 2, CALL_APPLY, CALL_STOP, PUMP_STOP, 0u, PUMP_STOP, 0u },

        /* Invalid speed > 100 and invalid direction: stop without calling apply; last_apply stays armed (PUMP_FILL, 80) */
        { "fill_101",      { PUMP_FILL, 101u },        APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "fill_255",      { PUMP_FILL, 255u },        APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "drain_101",     { PUMP_DRAIN, 101u },       APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "drain_255",     { PUMP_DRAIN, 255u },       APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "dir_99_mid",    { (PumpDirection)99, 50u }, APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "dir_99_zero",   { (PumpDirection)99, 0u },  APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },
        { "dir_neg1",      { (PumpDirection)-1, 10u }, APP_OK, APP_ERROR, 1, CALL_STOP, CALL_NONE, PUMP_FILL, 80u, PUMP_STOP, 0u },

        /* Backend rejection on valid movement: apply then stop, exact status propagated, stale output cleared */
        { "rej_not_ready",   { PUMP_DRAIN, 40u }, APP_NOT_READY,   APP_NOT_READY,   2, CALL_APPLY, CALL_STOP, PUMP_DRAIN, 40u, PUMP_STOP, 0u },
        { "rej_error",       { PUMP_DRAIN, 40u }, APP_ERROR,       APP_ERROR,       2, CALL_APPLY, CALL_STOP, PUMP_DRAIN, 40u, PUMP_STOP, 0u },
        { "rej_unsupported", { PUMP_DRAIN, 40u }, APP_UNSUPPORTED, APP_UNSUPPORTED, 2, CALL_APPLY, CALL_STOP, PUMP_DRAIN, 40u, PUMP_STOP, 0u }
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fake_reset();
        arm_running_output(cases[i].name, PUMP_FILL, 80u);
        g_fake.apply_status = cases[i].backend_status;

        CHECK(cases[i].name, pump_apply(cases[i].cmd) == cases[i].expected_status);
        check_calls(cases[i].name, cases[i].expected_calls,
                    cases[i].c0, cases[i].c1, CALL_NONE);
        CHECK(cases[i].name, g_fake.last_apply_dir == cases[i].expected_last_dir);
        CHECK(cases[i].name, g_fake.last_apply_speed == cases[i].expected_last_speed);
        CHECK(cases[i].name, g_fake.active_dir == cases[i].expected_active_dir);
        CHECK(cases[i].name, g_fake.active_speed == cases[i].expected_active_speed);
    }
}

static void run_explicit_stop_test(void) {
    fake_reset();
    arm_running_output("explicit_stop", PUMP_FILL, 100u);

    pump_stop();
    check_calls("explicit_stop", 1, CALL_STOP, CALL_NONE, CALL_NONE);
    CHECK("explicit_stop", g_fake.active_dir == PUMP_STOP);
    CHECK("explicit_stop", g_fake.active_speed == 0u);
}

int main(void) {
    run_init_tests();
    run_apply_tests();
    run_explicit_stop_test();

    if (g_failures != 0) {
        fprintf(stderr, "Pump_Test: %d check(s) failed\n", g_failures);
        return 1;
    }
    printf("Pump_Test: all checks passed\n");
    return 0;
}
