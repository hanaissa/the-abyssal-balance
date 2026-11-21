"""Replay photographed development frames through production C OCR.

Regression checks only: these frames informed development/calibration and
must never be reported as independent recognition accuracy.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--runner',type=Path,default=Path('build/ocr-recognizer/OCR_Read.exe'))
parser.add_argument('--manifest',type=Path,default=Path('tests/OCR_Test/fixtures/manifest.json'))
args=parser.parse_args()
cases=json.loads(args.manifest.read_text())
with tempfile.TemporaryDirectory(prefix='ocr-regression-') as temp:
    for case in cases:
        raw=gzip.decompress((args.manifest.parent/case['fixture']).read_bytes())
        assert len(raw)==153600 and hashlib.sha256(raw).hexdigest()==case['sha256']
        frame=Path(temp)/'frame.rgb565'; frame.write_bytes(raw)
        result=subprocess.run([str(args.runner.resolve()),str(frame)],text=True,capture_output=True)
        lines=result.stdout.strip().splitlines()
        assert result.returncode==0 and '\n'.join(lines[1:])==case['expected'], result.stdout+result.stderr
        print('PASS full-frame camera regression:',case['fixture'])
