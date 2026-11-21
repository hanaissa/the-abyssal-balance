#include <cstdio>
#include <cstdlib>
#include <vector>
#include "mock_sdk/fake_sdk.h"
// Exercise the real driver against the host SDK fake, not a copied algorithm.
#include "../vendor/PicoCamera/src/driver/pio_capture.cpp"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL: %s\n", #x); std::exit(1); } } while (0)
static void reset(Scenario which) {
    scenario = which; ticks = 0; vsync = false;
    dma_started = dma_configured = count_preloaded = samplings_started = false;
    aborted = 0;
}
static void check_sampler(bool first_clock_high, bool blanking_clocks) {
    std::vector<uint32_t> trace;
    std::vector<uint8_t> expected, actual;
    auto hold = [&](unsigned data, bool href, bool pclk, bool sync) {
        for (int i = 0; i < 20; ++i) {
            trace.push_back(data | (unsigned)href << 13 | (unsigned)pclk << 21 | (unsigned)sync << 12);
        }
    };
    hold(0, false, false, false);
    // First HREF may assert at the very same time as VSYNC; do not drop row 1.
    for (unsigned row = 0; row < 2; ++row) {
        for (unsigned byte = 0; byte < 8; ++byte) {
            unsigned value = 1 + row * 8 + byte;
            expected.push_back((uint8_t)value);
            if (!(first_clock_high && byte == 0)) { hold(value, true, false, true); }
            hold(value, true, true, true);
            hold(value, true, false, true);
        }
        hold(0xff, false, false, true);
        if (blanking_clocks) { hold(0xff, false, true, true); hold(0xff, false, false, true); }
    }
    hold(0, false, false, false);
    // A following frame must not append more samples once the count is reached.
    hold(0xee, true, true, true); hold(0xee, true, false, true);
    uint pc = 0, x = 0;
    for (uint32_t pins : trace) {
        uint16_t instruction = s_cap.instr_sized[pc];
        uint next = pc == SIZED_WRAP ? SIZED_WRAP_TARGET : pc + 1;
        switch (instruction >> 13) {
        case 4: break; // PULL: synthetic OSR contains expected length minus one
        case 5: x = (uint)expected.size() - 1; break; // MOV X, OSR
        case 1: // WAIT GPIO
            if (((pins >> (instruction & 31)) & 1) != ((instruction >> 7) & 1)) { next = pc; }
            break;
        case 6: break; // IRQ set (CPU boundary guard tested separately)
        case 2: actual.push_back((uint8_t)pins); break; // IN PINS, 8
        case 0: {
            uint condition = (instruction >> 5) & 7;
            bool jump = condition == 0 || (condition == 6 && (pins & (1u << 13)));
            if (condition == 2) { jump = x != 0; --x; }
            if (jump) { next = instruction & 31; }
            break;
        }
        default: CHECK(false);
        }
        pc = next;
        CHECK(pc < SIZED_PROG_LEN);
    }
    CHECK(actual == expected);
    CHECK(pc == SIZED_WRAP); // halted, rather than wrapping and sampling again
}
int main() {
    // Mock ordering assertions must remain active even for Release builds.
    pio_capture_pins_t pins = {0, 12, 13, 21};
    CHECK(pio_capture_init(&pins) == 0);
    CHECK(!pio0->enabled[0] && !pio0->enabled[1]);
    for (bool first_high : {false, true}) {
        for (bool blanking : {false, true}) { check_sampler(first_high, blanking); }
    }
    uint8_t buffer[38400];
    reset(Scenario::Complete);
    CHECK(pio_capture_frame(buffer, sizeof buffer, 10) == 0);
    CHECK(pio_capture_last_result().bytes_received == sizeof buffer);
    CHECK(!pio0->enabled[0] && !dma_started && aborted == 1);
    reset(Scenario::Short);
    CHECK(pio_capture_frame(buffer, sizeof buffer, 10) == -3);
    CHECK(pio_capture_last_result().bytes_received == 38280);
    CHECK(ticks == 2 && !pio0->enabled[0] && !dma_started); // no second frame
    reset(Scenario::NoSync);
    CHECK(pio_capture_frame(buffer, sizeof buffer, 10) == -2);
    CHECK(pio_capture_last_result().bytes_received == 0);
    CHECK(!pio0->enabled[0] && !dma_started);
    reset(Scenario::PipelineTail);
    CHECK(pio_capture_frame(buffer, sizeof buffer, 10) == 0);
    CHECK(pio_capture_last_result().bytes_received == sizeof buffer);
    reset(Scenario::Complete); // retry after both short-frame and timeout paths
    CHECK(pio_capture_frame(buffer, sizeof buffer, 10) == 0);
    CHECK(pio_capture_frame(nullptr, sizeof buffer, 10) == -1);
    CHECK(pio_capture_frame(buffer, 0, 10) == -1);
    pio_capture_deinit();
    CHECK(!pio0->enabled[0] && !pio0->enabled[1]);
    std::puts("PASS real capture driver: disabled setup, complete/short/timeout/tail, cleanup and retry.");
}
