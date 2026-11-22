"""Run compiled sampler instructions against synthetic line/frame waveforms.

Subset simulator only: WAIT GPIO, JMP unconditional/PIN, IN PINS and IRQ.
It does not model electrical timing, input synchronizer delay, DMA or FIFO stalls.
"""
import argparse
from pathlib import Path
import re


def waveform(first_clock_high, blanking_clocks):
    def hold(data=0, href=0, pclk=0, vsync=0, cycles=20):
        return [(data | href << 13 | pclk << 21 | vsync << 12)] * cycles

    trace = hold(vsync=1) + hold()
    expected = []
    for row in range(3):
        for x in range(4):
            y = 16 * (row * 4 + x) + 7
            expected.append(y)
            for byte in (y, 0xC3):
                # Boundary case: HREF asserts with PCLK already high on first Y.
                if not (first_clock_high and x == 0 and byte == y):
                    trace += hold(byte, href=1)
                trace += hold(byte, href=1, pclk=1)
                trace += hold(byte, href=1)
        trace += hold()
        if blanking_clocks:
            trace += hold(0xFF, pclk=1) + hold()
    trace += hold(vsync=1)
    # A second frame must never be included in the first image.
    trace += hold() + hold(0xFF, href=1, pclk=1)
    return trace, expected


def simulate(code, trace, bits):
    pc = shift = shifted = 0
    started = False
    output = []
    for pins in trace:
        # This models firmware stopping at the first VSYNC after start IRQ.
        if started and pins & (1 << 12):
            return output
        instruction = code[pc]
        operation = instruction >> 13
        next_pc = (pc + 1) % len(code)
        if operation == 1:  # WAIT GPIO
            assert (instruction >> 5) & 3 == 0
            level = bool(pins & (1 << (instruction & 31)))
            if level != bool(instruction & 0x80):
                next_pc = pc
        elif operation == 0:  # JMP
            condition = (instruction >> 5) & 7
            assert condition in (0, 6)
            if condition == 0 or pins & (1 << 13):
                next_pc = instruction & 31
        elif operation == 2:  # IN PINS, left-shift/autopush=8
            count = instruction & 31
            assert (instruction >> 5) & 7 == 0 and count == bits
            value = (pins >> (4 if bits == 4 else 0)) & ((1 << bits) - 1)
            shift = ((shift << bits) | value) & 0xFFFFFFFF
            shifted += bits
            if shifted == 8:
                output.append(shift & 255)
                shift = shifted = 0
        elif operation == 6:  # IRQ nowait 0
            assert instruction == 0xC000
            started = True
        else:
            raise AssertionError(f'Unsupported instruction {instruction:04x}')
        pc = next_pc
    raise AssertionError('No end-of-frame observed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--header', type=Path, default=Path('build/camera-bench/camera_capture.pio.h'))
    args = parser.parse_args()
    header = args.header.read_text()
    for name, bits in [('camera_capture', 8), ('camera_capture_vga', 4), ('camera_capture_raw', 8)]:
        body = re.search(rf'{name}_program_instructions\[\] = \{{(.*?)\}};', header, re.S)
        assert body, f'Missing assembled {name}'
        code = [int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{4})', body[1])]
        for high in (False, True):
            for blanking in (False, True):
                trace, expected = waveform(high, blanking)
                actual = simulate(code, trace, bits)
                if bits == 4:
                    expected = [(expected[i] & 0xF0) | (expected[i+1] >> 4)
                                for i in range(0, len(expected), 2)]
                elif name == 'camera_capture_raw':
                    expected = [byte for y in expected for byte in (y, 0xC3)]
                assert actual == expected, (name, high, blanking, actual, expected)
    print('PASS: compiled PIO, first-clock boundary, blanking clocks, Y/chroma separation,')
    print('      VGA nibble packing and first-frame termination (synthetic, not hardware).')


if __name__ == '__main__':
    main()
