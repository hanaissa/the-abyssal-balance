# Integration plan

1. Confirm wiring and electrical levels, especially the PCF8574T LCD backpack and OV7670 camera.
2. Implement and demonstrate each driver through its independent test target.
3. Keep pump output disabled until L298N supply, grounds, flyback protection, and emergency-stop behavior are verified.
4. Integrate initialization in a defined order, then add cooperative periodic tasks using Pico SDK timing.
5. Connect state-machine events only after module contracts and tests are stable.
6. Treat camera/OCR as a feasibility track: the RP2040 may capture data, but OCR may require a second processor or host service.

Risks: camera GPIO/timing budget is unknown; LCD voltage compatibility is unverified; sensor calibration is unknown; current stubs do not claim hardware functionality.
