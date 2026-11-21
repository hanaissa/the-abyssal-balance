"""Fetch only the balanced split from NIST's ZIP using verified HTTP ranges.

No third-party mirror, whole-archive download or Python dependency required.
zipfile verifies each member's CRC before its contents are saved.
"""
import argparse
import io
from pathlib import Path
import urllib.request
import zipfile

URL = 'https://biometrics.nist.gov/cs_links/EMNIST/gzip.zip'
HEADERS = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) EMNISTResearch/1.0'}


class RemoteZip(io.RawIOBase):
    def __init__(self):
        with urllib.request.urlopen(urllib.request.Request(URL, headers=HEADERS, method='HEAD'), timeout=60) as response:
            self.length = int(response.headers['Content-Length'])
        self.position = 0
        self.transferred = 0

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=0):
        self.position = offset if whence == 0 else self.position + offset if whence == 1 else self.length + offset
        if self.position < 0:
            raise ValueError('Negative ZIP seek')
        return self.position

    def read(self, size=-1):
        end = self.length if size < 0 else min(self.length, self.position + size)
        if end <= self.position:
            return b''
        # A single member is at most ~35 MB. Never silently download 562 MB.
        if end - self.position > 50_000_000:
            raise ValueError('Oversized range; refusing whole archive')
        requested = f'bytes={self.position}-{end-1}'
        request = urllib.request.Request(URL, headers={**HEADERS, 'Range': requested})
        with urllib.request.urlopen(request, timeout=120) as response:
            expected = f'bytes {self.position}-{end-1}/{self.length}'
            if response.status != 206 or response.headers.get('Content-Range') != expected:
                raise RuntimeError('NIST server did not honor exact HTTP byte range')
            result = response.read(end - self.position + 1)
        if len(result) != end - self.position:
            raise RuntimeError('Truncated ZIP range')
        self.position = end
        self.transferred += len(result)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/ocr-data'))
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    remote = RemoteZip()
    with zipfile.ZipFile(remote) as archive:
        for name in archive.namelist():
            base = Path(name).name
            if not base.startswith('emnist-balanced-'):
                continue
            target = args.output / base
            if target.exists():
                print('Already present:', target, flush=True)
                continue
            data = archive.read(name)
            target.write_bytes(data)
            print('Verified ZIP member:', base, len(data), 'bytes', flush=True)
    print('Downloaded', remote.transferred, 'bytes from', URL)


if __name__ == '__main__':
    main()
