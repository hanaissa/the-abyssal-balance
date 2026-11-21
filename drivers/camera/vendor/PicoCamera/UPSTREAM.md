# Vendored PicoCamera

- Source: https://github.com/umeiko/PicoCamera
- Commit: `281d9ff6de5d5f6bd68c2ce75f245cc9cbadb0ea`
- Version in header: 0.4.2
- License: [MIT](LICENSE), copyright notices retained in every source file.
- Initially copied unmodified: `src/`, `CMakeLists.txt`, `LICENSE`, `readme.md`.
- Local fix v2 changes `src/driver/pio_capture.cpp` and `pio_capture.h`:
  keep the sized state machine disabled during reset/FIFO/DMA/count setup;
  recheck HREF after waiting for PCLK to exclude blanking bytes; retain the
  first line when HREF asserts with VSYNC; halt after exactly the requested
  bytes; signal actual PIO frame start using private bench IRQ0; stop/reject
  a short first frame rather than continuing into another frame; expose the
  last byte count/status and stop/clean up after completion or timeout.
  The JPEG sampler and sensor register tables remain unchanged.
- No Arduino core or handwritten capture backend is used by the new test.
- API/example reviewed: `src/pico_camera.h`, `src/sensor.h`,
  `examples/pico_sdk_capture/main.c`, its CMake file, `docs/user_guide.md`,
  `src/driver/sccb.cpp`, `src/driver/pio_capture.cpp`, `src/sensors/ov7670.cpp`.

The production wrapper lives in `../../Camera.c` and `../../CameraSetup.h`;
the optional USB receiver/preview live in `../../../../tools/camera/`.
Neither depends on the old test tree. The checked sensor
settings are bench-local (480-row window, divided internal clock, normal PCLK,
zero scaler delay and optional DSP bars). Current v5 selects QVGA 320x240.
Fix v3 restores HREF[7:6] to the documented default 0x80 through the wrapper,
after upstream frame_control() clears them; its six horizontal-window bits
are preserved. References: [OV7670 revision 1.4, Table 5](https://web.mit.edu/6.111/www/f2016/tools/OV7670_2006.pdf)
(HREF 0x32 offset/default) and the [Linux sensor driver](https://github.com/torvalds/linux/blob/master/drivers/media/i2c/ov7670.c)
(default HREF 0xB6; set-window preserves the top two bits). The v2 hardware run
still received 38280/38400 bytes in one frame; restoring HREF alone did not fix
it. V4's physical timing comparison passed with normal PCLK and scaler delay
zero, repeatedly. V5 uses those settings directly and passed three physical
320x240 captures: 153600 bytes, one frame, 240 observed HREF rows, valid CRC.
Host SDK-fake checks exercise the actual capture driver and emitted sampler
instructions; they do not establish electrical timing or real FIFO/DMA behavior.
Updating this snapshot requires reviewing the API and repeating build/transport
checks; hardware success is separate.
