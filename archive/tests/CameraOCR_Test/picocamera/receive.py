"""Compatibility entry point; the maintained viewer lives outside tests."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parents[4] / 'tools/camera/receive.py'), run_name='__main__')
