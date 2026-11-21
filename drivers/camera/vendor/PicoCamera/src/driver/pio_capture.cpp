/*
 * This file is part of the PicoCamera project.
 * https://github.com/umeiko/PicoCamera
 *
 * Author: umeko <umeko@stu.xmu.edu.cn>
 * License: MIT
 */

#include "pio_capture.h"

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

// ---------------------------------------------------------------------------
// Runtime-assembled PIO programs
//
// "sized" program (exact byte count, for RGB565):
//
//     pull block                 ; byte count - 1 arrives via TX FIFO
//     mov  x, osr
//     wait 0 gpio VSYNC
//     wait 1 gpio VSYNC          ; frame start
//     irq 0                      ; actual frame start, polled by CPU
// loop:                          ; <- wrap target
//     wait 1 gpio HREF           ; line start
//     wait 1 gpio PCLK
//     jmp pin sample             ; HREF must still be high
//     jmp loop                   ; discard blanking clock
// sample:
//     in   pins, 8               ; sample D0..D7
//     wait 0 gpio PCLK
//     jmp  x-- loop
// halt: jmp halt                 ; exact byte count; no wrap into another frame
//
// "continuous" program (streams until disabled, for JPEG):
//
//     wait 0 gpio VSYNC
//     wait 1 gpio VSYNC          ; frame start
// loop:                          ; <- wrap target
//     wait 1 gpio HREF
//     wait 1 gpio PCLK
//     in   pins, 8
//     wait 0 gpio PCLK
//                                ; <- wrap
// ---------------------------------------------------------------------------

#define SIZED_WRAP_TARGET 5
#define SIZED_WRAP        12
#define SIZED_PROG_LEN    13

#define CONT_WRAP_TARGET 2
#define CONT_WRAP        5
#define CONT_PROG_LEN    6

static void build_sized_program(uint16_t *instr, int pin_vsync, int pin_href, int pin_pclk) {
    int i = 0;
    instr[i++] = pio_encode_pull(false, true);            // 0: pull block
    instr[i++] = pio_encode_mov(pio_x, pio_osr);          // 1: mov x, osr
    instr[i++] = pio_encode_wait_gpio(0, pin_vsync);      // 2: wait 0 gpio VSYNC
    instr[i++] = pio_encode_wait_gpio(1, pin_vsync);      // 3: wait 1 gpio VSYNC
    instr[i++] = pio_encode_irq_set(false, 0);           // 4: actual capture frame started
    instr[i++] = pio_encode_wait_gpio(1, pin_href);       // 5: wait 1 gpio HREF
    instr[i++] = pio_encode_wait_gpio(1, pin_pclk);       // 6: wait 1 gpio PCLK
    instr[i++] = pio_encode_jmp_pin(9);                  // 7: HREF still high? sample
    instr[i++] = pio_encode_jmp(SIZED_WRAP_TARGET);       // 8: discard blanking clock
    instr[i++] = pio_encode_in(pio_pins, 8);              // 9: sample
    instr[i++] = pio_encode_wait_gpio(0, pin_pclk);       // 10: wait 0 gpio PCLK
    instr[i++] = pio_encode_jmp_x_dec(SIZED_WRAP_TARGET); // 11: remaining bytes
    instr[i++] = pio_encode_jmp(12);                     // 12: halt after exactly len bytes
}

static void build_continuous_program(uint16_t *instr, int pin_vsync, int pin_href, int pin_pclk) {
    int i = 0;
    instr[i++] = pio_encode_wait_gpio(0, pin_vsync);      // 0: wait 0 gpio VSYNC
    instr[i++] = pio_encode_wait_gpio(1, pin_vsync);      // 1: wait 1 gpio VSYNC
    // CONT_WRAP_TARGET == 2
    instr[i++] = pio_encode_wait_gpio(1, pin_href);       // 2: wait 1 gpio HREF
    instr[i++] = pio_encode_wait_gpio(1, pin_pclk);       // 3: wait 1 gpio PCLK
    instr[i++] = pio_encode_in(pio_pins, 8);              // 4: in pins, 8
    instr[i++] = pio_encode_wait_gpio(0, pin_pclk);       // 5: wait 0 gpio PCLK
    // CONT_WRAP == 5
}

static struct {
    PIO pio;
    uint sm_sized;
    uint sm_cont;
    uint offset_sized;
    uint offset_cont;
    int dma_chan;
    int pin_vsync;
    uint16_t instr_sized[SIZED_PROG_LEN];
    uint16_t instr_cont[CONT_PROG_LEN];
    struct pio_program prog_sized;
    struct pio_program prog_cont;
    bool inited;
    pio_capture_result_t last;
} s_cap;

static void make_program(struct pio_program *prog, uint16_t *instr, uint len) {
    memset(prog, 0, sizeof(*prog));
    prog->instructions = instr;
    prog->length = len;
    prog->origin = -1;
}

static void config_sm(uint sm, uint offset, uint wrap_target, uint wrap, int pin_d0, int pin_href) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + wrap_target, offset + wrap);
    sm_config_set_in_pins(&c, pin_d0);
    sm_config_set_jmp_pin(&c, pin_href);
    sm_config_set_in_shift(&c, false, true, 8);  // shift left, autopush after 8 bits
    pio_sm_init(s_cap.pio, sm, offset, &c);
}

int pio_capture_init(const pio_capture_pins_t *pins) {
    if (!pins || pins->pin_d0 < 0 || pins->pin_vsync < 0 ||
            pins->pin_href < 0 || pins->pin_pclk < 0) {
        return -1;
    }
    if (s_cap.inited) {
        return 0;
    }

    build_sized_program(s_cap.instr_sized, pins->pin_vsync, pins->pin_href, pins->pin_pclk);
    build_continuous_program(s_cap.instr_cont, pins->pin_vsync, pins->pin_href, pins->pin_pclk);
    make_program(&s_cap.prog_sized, s_cap.instr_sized, SIZED_PROG_LEN);
    make_program(&s_cap.prog_cont, s_cap.instr_cont, CONT_PROG_LEN);

    // Find a PIO block that fits both programs and has two free state
    // machines (RP2350 adds a third block, pio2)
    PIO candidates[] = {
        pio0, pio1,
#if NUM_PIOS > 2
        pio2,
#endif
    };
    bool claimed = false;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) && !claimed; i++) {
        PIO p = candidates[i];
        if (!pio_can_add_program(p, &s_cap.prog_sized)) {
            continue;
        }
        int offset_sized = pio_add_program(p, &s_cap.prog_sized);
        if (offset_sized < 0 || !pio_can_add_program(p, &s_cap.prog_cont)) {
            if (offset_sized >= 0) {
                pio_remove_program(p, &s_cap.prog_sized, (uint)offset_sized);
            }
            continue;
        }
        int sm_sized = pio_claim_unused_sm(p, false);
        int sm_cont = pio_claim_unused_sm(p, false);
        if (sm_sized < 0 || sm_cont < 0) {
            if (sm_sized >= 0) {
                pio_sm_unclaim(p, (uint)sm_sized);
            }
            if (sm_cont >= 0) {
                pio_sm_unclaim(p, (uint)sm_cont);
            }
            pio_remove_program(p, &s_cap.prog_sized, (uint)offset_sized);
            continue;
        }
        s_cap.pio = p;
        s_cap.sm_sized = (uint)sm_sized;
        s_cap.sm_cont = (uint)sm_cont;
        s_cap.offset_sized = (uint)offset_sized;
        s_cap.offset_cont = (uint)pio_add_program(p, &s_cap.prog_cont);
        claimed = true;
    }
    if (!claimed) {
        return -1;
    }

    // Route the pins to the chosen PIO block
    for (int i = 0; i < 8; i++) {
        pio_gpio_init(s_cap.pio, pins->pin_d0 + i);
    }
    pio_gpio_init(s_cap.pio, pins->pin_pclk);
    pio_gpio_init(s_cap.pio, pins->pin_href);
    pio_gpio_init(s_cap.pio, pins->pin_vsync);
    gpio_pull_up(pins->pin_vsync);
    s_cap.pin_vsync = pins->pin_vsync;

    config_sm(s_cap.sm_sized, s_cap.offset_sized, SIZED_WRAP_TARGET, SIZED_WRAP, pins->pin_d0, pins->pin_href);
    // Both state machines stay disabled until their DMA and counters are ready.

    config_sm(s_cap.sm_cont, s_cap.offset_cont, CONT_WRAP_TARGET, CONT_WRAP, pins->pin_d0, pins->pin_href);
    // continuous SM stays disabled until a variable capture starts

    s_cap.dma_chan = dma_claim_unused_channel(false);
    if (s_cap.dma_chan < 0) {
        // No free DMA channel: roll back the PIO claims instead of panicking
        pio_sm_set_enabled(s_cap.pio, s_cap.sm_sized, false);
        pio_remove_program(s_cap.pio, &s_cap.prog_sized, s_cap.offset_sized);
        pio_remove_program(s_cap.pio, &s_cap.prog_cont, s_cap.offset_cont);
        pio_sm_unclaim(s_cap.pio, s_cap.sm_sized);
        pio_sm_unclaim(s_cap.pio, s_cap.sm_cont);
        return -1;
    }
    s_cap.inited = true;
    return 0;
}

void pio_capture_deinit(void) {
    if (!s_cap.inited) {
        return;
    }
    dma_channel_unclaim(s_cap.dma_chan);
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_sized, false);
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_cont, false);
    pio_remove_program(s_cap.pio, &s_cap.prog_sized, s_cap.offset_sized);
    pio_remove_program(s_cap.pio, &s_cap.prog_cont, s_cap.offset_cont);
    pio_sm_unclaim(s_cap.pio, s_cap.sm_sized);
    pio_sm_unclaim(s_cap.pio, s_cap.sm_cont);
    s_cap.inited = false;
}

static void config_dma(uint sm, uint8_t *buf, size_t count) {
    dma_channel_config c = dma_channel_get_default_config(s_cap.dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(s_cap.pio, sm, false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);

    dma_channel_configure(
        s_cap.dma_chan, &c,
        buf,                     // write to frame buffer
        &s_cap.pio->rxf[sm],     // read from PIO RX FIFO
        count,
        false
    );
}

// Wait until VSYNC is low. The PIO program's "wait 0 / wait 1" pair only
// works as a rising-edge detector when entered while the signal is LOW;
// if armed mid-frame it falls through and captures a partial frame.
static bool wait_vsync_low(uint32_t timeout_ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (gpio_get(s_cap.pin_vsync)) {
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

int pio_capture_frame(uint8_t *buf, size_t len, uint32_t timeout_ms) {
    s_cap.last = {0, -1};
    if (!s_cap.inited || !buf || len == 0) {
        return -1;
    }

    pio_sm_set_enabled(s_cap.pio, s_cap.sm_sized, false);
    // Reset PC to the program start so every frame begins at the VSYNC wait.
    // pio_sm_restart() alone does NOT touch the program counter (see the
    // hardware_pio docs), so without the forced JMP the SM resumes wherever
    // it stalled - mid-frame - and the capture starts at a random line.
    pio_sm_restart(s_cap.pio, s_cap.sm_sized);
    pio_sm_exec(s_cap.pio, s_cap.sm_sized, pio_encode_jmp(s_cap.offset_sized));
    pio_sm_clear_fifos(s_cap.pio, s_cap.sm_sized);
    pio_interrupt_clear(s_cap.pio, 0);
    config_dma(s_cap.sm_sized, buf, len);
    // Preload while disabled: no reset/setup instruction can race the sampler.
    pio_sm_put_blocking(s_cap.pio, s_cap.sm_sized, (uint32_t)(len - 1));
    if (!wait_vsync_low(timeout_ms)) {
        s_cap.last.status = -2;
        return -2;
    }
    dma_channel_start(s_cap.dma_chan);
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_sized, true);

    uint32_t start = to_ms_since_boot(get_absolute_time());
    int status = 0;
    while (dma_channel_is_busy(s_cap.dma_chan)) {
        // IRQ0 is set by PIO after its own VSYNC wait, so a frame already
        // active during setup is never mistaken for the captured frame.
        if (pio_interrupt_get(s_cap.pio, 0) && !gpio_get(s_cap.pin_vsync)) {
            status = -3;  // first frame ended before its buffer filled
            break;
        }
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            status = -2;
            break;
        }
        tight_loop_contents();
    }
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_sized, false);
    // Allow only bytes already sampled into the FIFO to drain, never a new frame.
    sleep_us(2);
    s_cap.last.bytes_received = len - dma_hw->ch[s_cap.dma_chan].transfer_count;
    if (status == -3 && s_cap.last.bytes_received == len) { status = 0; }
    dma_channel_abort(s_cap.dma_chan);
    pio_sm_clear_fifos(s_cap.pio, s_cap.sm_sized);
    s_cap.last.status = status;
    return status;
}

pio_capture_result_t pio_capture_last_result(void) { return s_cap.last; }

int pio_capture_frame_variable(uint8_t *buf, size_t capacity, size_t *out_len, uint32_t timeout_ms) {
    if (!s_cap.inited || !buf || !out_len || capacity == 0) {
        return -1;
    }

    if (!wait_vsync_low(timeout_ms)) {
        return -2;
    }

    // Enable first, then reset PC to the program start (VSYNC wait) and flush
    // any bytes pushed in between; only then arm the DMA, so nothing stale
    // can land at the head of the buffer. The forced JMP is required because
    // pio_sm_restart() does not reset the program counter.
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_cont, true);
    pio_sm_restart(s_cap.pio, s_cap.sm_cont);
    pio_sm_exec(s_cap.pio, s_cap.sm_cont, pio_encode_jmp(s_cap.offset_cont));
    pio_sm_clear_fifos(s_cap.pio, s_cap.sm_cont);
    config_dma(s_cap.sm_cont, buf, capacity);
    dma_channel_start(s_cap.dma_chan);

    uint32_t start = to_ms_since_boot(get_absolute_time());
    bool ok = true;

    // Wait for the frame to start (VSYNC high)
    while (ok && !gpio_get(s_cap.pin_vsync)) {
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            ok = false;
        }
        tight_loop_contents();
    }

    // Wait for the frame to end (VSYNC low) or the buffer to fill up
    while (ok && gpio_get(s_cap.pin_vsync) && dma_channel_is_busy(s_cap.dma_chan)) {
        if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms) {
            ok = false;
        }
        tight_loop_contents();
    }

    // Stop everything, then count what actually arrived
    pio_sm_set_enabled(s_cap.pio, s_cap.sm_cont, false);
    sleep_us(50);  // let the FIFO/DMA pipeline drain
    size_t received = capacity - dma_hw->ch[s_cap.dma_chan].transfer_count;
    dma_channel_abort(s_cap.dma_chan);
    pio_sm_clear_fifos(s_cap.pio, s_cap.sm_cont);

    if (!ok) {
        return -2;  // timeout
    }

    // Trim at the JPEG EOI marker (0xFFD9); padding may follow it
    size_t len = received;
    for (size_t i = received; i >= 2; i--) {
        if (buf[i - 2] == 0xFF && buf[i - 1] == 0xD9) {
            len = i;
            break;
        }
    }
    *out_len = len;
    return 0;
}
