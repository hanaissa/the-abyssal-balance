"""Receive one verified RGB565 QVGA/QQVGA frame and save/display it with OpenCV."""
import argparse
from pathlib import Path
import re
import struct
import time
import zlib


def read_exact(stream, count, deadline):
    result = bytearray()
    while len(result) < count:
        if time.monotonic() > deadline:
            raise TimeoutError('Image receive deadline expired')
        block = stream.read(count - len(result))
        if not block:
            raise TimeoutError('Serial read timed out or image was truncated')
        result.extend(block)
    return bytes(result)


def receive_frame(stream, deadline, log=print, on_ready=None):
    while time.monotonic() < deadline:
        line = bytearray()
        while not line.endswith(b'\n'):
            line.extend(read_exact(stream, 1, deadline))
            if len(line) > 512:
                raise ValueError('Oversized header; wrong firmware or serial stream')
        if line.startswith(b'ERROR'):
            raise RuntimeError(line.decode('ascii', errors='replace').strip())
        match = re.fullmatch(rb'ABYSSAL_RGB565 1 (\d+) (\d+) (\d+) ([0-9A-Fa-f]{8})\r?\n', line)
        if match:
            width, height, size = map(int, match.groups()[:3])
            if (width, height, size) not in ((160, 120, 38400), (320, 240, 153600)):
                raise ValueError('Unexpected dimensions/length; refusing malformed frame')
            payload = read_exact(stream, size, deadline)
            if zlib.crc32(payload) != int(match[4], 16):
                raise ValueError('CRC32 mismatch; image was not saved')
            if read_exact(stream, 12, deadline) != b'\nEND_RGB565\n':
                raise ValueError('Invalid frame trailer')
            return width, height, payload
        text = line.decode('ascii', errors='replace').strip()
        if text:
            log(text)
        if text.startswith('Send c after') and on_ready:
            on_ready()
    raise TimeoutError('No image header received')


def rgb565_to_bgr(payload, byte_order='big'):
    if len(payload) % 2:
        raise ValueError('RGB565 payload has an odd byte count')
    result = bytearray(len(payload) // 2 * 3)
    for i, (pixel,) in enumerate(struct.iter_unpack('>H' if byte_order == 'big' else '<H', payload)):
        result[i*3:i*3+3] = bytes(((pixel & 31) * 255 // 31,
                                 ((pixel >> 5) & 63) * 255 // 63,
                                 ((pixel >> 11) & 31) * 255 // 31))
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM3')
    parser.add_argument('--output', type=Path, default=Path('build/picocamera.png'))
    parser.add_argument('--byte-order', choices=('big', 'little'), default='big',
                        help='Default big; verify pixel alignment and colors using known objects')
    parser.add_argument('--no-display', action='store_true')
    parser.add_argument('--color-bars', action='store_true', help='Capture the sensor internal pattern, not a photo')
    parser.add_argument('--ocr-roi',nargs=4,type=int,metavar=('X','Y','WIDTH','HEIGHT'),
                        help='Recognize only this rectangle; still save the complete image')
    args = parser.parse_args()
    if args.ocr_roi and (min(args.ocr_roi)<0 or args.ocr_roi[2]<3 or args.ocr_roi[3]<3 or
                         args.ocr_roi[0]+args.ocr_roi[2]>320 or args.ocr_roi[1]+args.ocr_roi[3]>240):
        parser.error('OCR ROI must fit inside the 320x240 image')
    try:
        import serial
        import cv2
        import numpy as np
    except ImportError as error:
        parser.exit(1, f'Missing dependency: {error}. Install requirements.txt in your camera venv.\n')
    print('Keep pump supply OFF. Before running: verify RESET/PWDN and shared-bus voltage/pull-ups.')
    try:
        # Pico OCR uses software floating point; permit bounded recognition
        # work before the next diagnostic/header line is emitted.
        with serial.Serial(args.port, 115200, timeout=60, write_timeout=3) as port:
            port.dtr = True
            command=b't' if args.color_bars else b'c'
            if args.ocr_roi:
                command+=(' '+ ' '.join(map(str,args.ocr_roi))+'\n').encode('ascii')
            width, height, payload = receive_frame(port, time.monotonic()+120,
                                                   on_ready=lambda: port.write(command))
        bgr = np.frombuffer(rgb565_to_bgr(payload, args.byte_order), dtype=np.uint8).reshape(height, width, 3)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        raw_path = args.output.with_suffix('.rgb565')
        raw_path.write_bytes(payload)
        if not cv2.imwrite(str(args.output), bgr):
            raise OSError('OpenCV failed to write image')
        print(f'Saved {width}x{height} RGB565, {len(payload)} bytes, CRC32 verified: {args.output.resolve()}')
        print(f'Raw sensor bytes retained: {raw_path.resolve()} (decoded {args.byte_order}-endian)')
        print('Transport checks passed; inspect focus/colors before claiming hardware image quality.')
        if not args.no_display:
            cv2.namedWindow('Abyssal Balance - OV7670 (press any key)', cv2.WINDOW_NORMAL)
            cv2.imshow('Abyssal Balance - OV7670 (press any key)', bgr)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
    except (OSError, ValueError, RuntimeError, TimeoutError, serial.SerialException, cv2.error) as error:
        parser.exit(1, f'Camera test failed: {error}\nClose other serial readers; reconnect Pico normally to repeat.\n')


if __name__ == '__main__':
    main()
