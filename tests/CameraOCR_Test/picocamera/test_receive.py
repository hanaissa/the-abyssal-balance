import io
import time
import unittest
import zlib
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools' / 'camera'))
from receive import receive_frame, rgb565_to_bgr


class TransferTests(unittest.TestCase):
    def packet(self, payload=None, checksum=None, dimensions=b'160 120 38400', trailer=b'\nEND_RGB565\n'):
        payload = bytes(range(256)) * 150 if payload is None else payload
        checksum = zlib.crc32(payload) if checksum is None else checksum
        return io.BytesIO(b'boot\r\nABYSSAL_RGB565 1 ' + dimensions +
                          f' {checksum:08X}\r\n'.encode() + payload + trailer)

    def read(self, stream):
        return receive_frame(stream, time.monotonic()+1, log=lambda _: None)

    def test_binary_all_values(self):
        width, height, data = self.read(self.packet())
        self.assertEqual((width, height, len(data)), (160, 120, 38400))
        self.assertEqual(data[:256], bytes(range(256)))

    def test_qvga_full_frame(self):
        payload = bytes(range(256)) * 600
        width, height, data = self.read(self.packet(payload=payload, dimensions=b'320 240 153600'))
        self.assertEqual((width, height, len(data)), (320, 240, 153600))
        self.assertEqual(data, payload)

    def test_qvga_reject_bad_packets(self):
        payload = bytes(range(256)) * 600
        for packet in (
                self.packet(payload=payload, dimensions=b'320 240 38400'),
                self.packet(payload=payload[:-1], dimensions=b'320 240 153600', trailer=b''),
                self.packet(payload=payload, dimensions=b'320 240 153600', checksum=0),
                self.packet(payload=payload, dimensions=b'640 480 614400')):
            with self.subTest(packet=packet), self.assertRaises((ValueError, TimeoutError)):
                self.read(packet)

    def test_reject_bad_packets(self):
        for packet in (self.packet(checksum=0), self.packet(payload=b'cut'),
                       self.packet(dimensions=b'160 119 38400'), self.packet(trailer=b'bad')):
            with self.subTest(packet=packet), self.assertRaises((ValueError, TimeoutError)):
                self.read(packet)

    def test_device_error(self):
        with self.assertRaises(RuntimeError):
            self.read(io.BytesIO(b'ERROR initialization: no sensor\n'))

    def test_rgb_colors_and_order(self):
        expected = bytes((0,0,255, 0,255,0, 255,0,0, 255,255,255, 0,0,0))
        self.assertEqual(rgb565_to_bgr(bytes.fromhex('f80007e0001fffff0000')), expected)
        self.assertEqual(rgb565_to_bgr(bytes.fromhex('00f8e0071f00ffff0000'), 'little'), expected)
        with self.assertRaises(ValueError):
            rgb565_to_bgr(b'\0')


if __name__ == '__main__':
    unittest.main()
