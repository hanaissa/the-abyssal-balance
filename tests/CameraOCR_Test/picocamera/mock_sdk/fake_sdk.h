#pragma once
/* Host-only SDK fake. Checks capture ordering and frame-end cleanup; it does
 * not simulate electrical timing, PIO execution, FIFO stalls or real DMA. */
#include <cassert>
#include <cstddef>
#include <cstdint>
using uint = unsigned;
struct MockPIO { uint32_t rxf[4]; bool enabled[4]; bool irq; };
using PIO = MockPIO *;
static MockPIO mock_pio[2] = {};
static PIO pio0 = &mock_pio[0], pio1 = &mock_pio[1];
struct pio_program { const uint16_t *instructions; uint length; int origin; };
struct pio_sm_config {};
enum { pio_x = 1, pio_osr = 7, pio_pins = 0, DMA_SIZE_8 = 0 };
struct dma_channel_config {};
struct MockDMA { struct { uint32_t transfer_count; } ch[1]; };
static MockDMA mock_dma = {};
static MockDMA *dma_hw = &mock_dma;
enum class Scenario { Complete, Short, NoSync, PipelineTail };
static Scenario scenario;
static uint ticks, next_sm, loaded_programs;
static bool dma_started, dma_configured, count_preloaded, samplings_started;
static bool vsync;
static unsigned aborted;
static PIO selected;
static uint active_sm;
inline uint16_t pio_encode_pull(bool, bool) { return 0x80a0; }
inline uint16_t pio_encode_mov(int dest, int src) { return 0xa000 | dest << 5 | src; }
inline uint16_t pio_encode_wait_gpio(uint level, uint pin) { return 0x2000 | level << 7 | pin; }
inline uint16_t pio_encode_irq_set(bool, uint irq) { return 0xc000 | irq; }
inline uint16_t pio_encode_in(int src, uint n) { return 0x4000 | src << 5 | n; }
inline uint16_t pio_encode_jmp_x_dec(uint dest) { return 0x0040 | dest; }
inline uint16_t pio_encode_jmp(uint dest) { return dest; }
inline uint16_t pio_encode_jmp_pin(uint dest) { return 0x00c0 | dest; }
inline pio_sm_config pio_get_default_sm_config() { return {}; }
inline void sm_config_set_wrap(pio_sm_config *, uint, uint) {}
inline void sm_config_set_in_pins(pio_sm_config *, uint) {}
inline void sm_config_set_jmp_pin(pio_sm_config *, uint) {}
inline void sm_config_set_in_shift(pio_sm_config *, bool, bool, uint) {}
inline void pio_sm_init(PIO, uint, uint, const pio_sm_config *) {}
inline bool pio_can_add_program(PIO, const pio_program *) { return true; }
inline int pio_add_program(PIO, const pio_program *) { return loaded_programs++ ? 13 : 0; }
inline int pio_claim_unused_sm(PIO, bool) { return next_sm++; }
inline void pio_sm_unclaim(PIO, uint) {}
inline void pio_remove_program(PIO, const pio_program *, uint) {}
inline void pio_gpio_init(PIO, int) {}
inline void gpio_pull_up(int) {}
inline void pio_sm_set_enabled(PIO p, uint sm, bool enabled) {
    if (enabled) {
        assert(dma_started && dma_configured && count_preloaded);
        samplings_started = true; selected = p; active_sm = sm;
    }
    p->enabled[sm] = enabled;
}
inline void pio_sm_restart(PIO p, uint sm) { assert(!p->enabled[sm]); }
inline void pio_sm_exec(PIO p, uint sm, uint16_t) { assert(!p->enabled[sm]); }
inline void pio_sm_clear_fifos(PIO p, uint sm) { assert(!p->enabled[sm]); }
inline void pio_sm_put_blocking(PIO p, uint sm, uint32_t) {
    assert(!p->enabled[sm]); count_preloaded = true;
}
inline void pio_interrupt_clear(PIO p, uint) { p->irq = false; }
inline bool pio_interrupt_get(PIO p, uint) { return p->irq; }
inline uint pio_get_dreq(PIO, uint, bool) { return 0; }
inline int dma_claim_unused_channel(bool) { return 0; }
inline void dma_channel_unclaim(int) {}
inline dma_channel_config dma_channel_get_default_config(int) { return {}; }
inline void channel_config_set_read_increment(dma_channel_config *, bool) {}
inline void channel_config_set_write_increment(dma_channel_config *, bool) {}
inline void channel_config_set_dreq(dma_channel_config *, uint) {}
inline void channel_config_set_transfer_data_size(dma_channel_config *, int) {}
inline void dma_channel_configure(int, const dma_channel_config *, void *, const void *, size_t len, bool) {
    assert(!mock_pio[0].enabled[0]);
    mock_dma.ch[0].transfer_count = (uint32_t)len;
    dma_configured = true;
}
inline void dma_channel_start(int) { assert(dma_configured); dma_started = true; }
inline bool dma_channel_is_busy(int) { return dma_started && mock_dma.ch[0].transfer_count; }
inline void dma_channel_abort(int) { dma_started = false; ++aborted; }
inline uint32_t get_absolute_time() { return ticks; }
inline uint32_t to_ms_since_boot(uint32_t t) { return t; }
inline bool gpio_get(int) { return vsync; }
inline void tight_loop_contents() {
    ++ticks;
    if (!samplings_started || scenario == Scenario::NoSync) { return; }
    if (ticks == 1) { vsync = true; selected->irq = true; }
    if (ticks == 2) {
        mock_dma.ch[0].transfer_count = scenario == Scenario::Complete ? 0 :
                                      scenario == Scenario::PipelineTail ? 2 : 120;
        vsync = false;
    }
}
inline void sleep_us(uint) {
    if (scenario == Scenario::PipelineTail && samplings_started) { mock_dma.ch[0].transfer_count = 0; }
}
