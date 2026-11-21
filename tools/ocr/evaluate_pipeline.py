"""Exercise actual C inference on held-out handwriting and assembled lines.

Requires the host shared library built from production sources. Composed
lines use real EMNIST test glyphs, not computer-font templates. This evaluates
segmentation mechanics, not photographic/UV/real-page accuracy.
"""
import argparse
import subprocess
import json
from pathlib import Path
import cv2
import numpy as np
from train import load


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runner',type=Path,default=Path('build/ocr-recognizer/OCR_Read.exe'))
    args=parser.parse_args()
    vectors=np.load('build/ocr-test-vectors.npz')
    with Path('build/ocr-recognizer/vectors.bin').open('wb') as output:
        for pixels,label in zip(vectors['pixels'],vectors['labels']):
            output.write(bytes([int(label)])+pixels.tobytes())
    # The optional full-line fixture is regenerated below, so do not silently
    # reuse a line selected by an older model when checking the new one.
    alphabet='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
    raw,labels=load(Path('build/ocr-data'),'test')
    # Fixed first 20 test glyphs per class, with no selection by C prediction.
    cases=[]
    for message in ('HELLO','WATER','MIX 9:5:2','AB12'):
        for sample in range(10):
            canvas=np.full((240,320),255,np.uint8); x=20
            for ch in message:
                if ch==' ': x+=20; continue
                if ch==':':
                    canvas[100:103,x:x+3]=0; canvas[109:112,x:x+3]=0; x+=12; continue
                index=np.flatnonzero(labels==alphabet.index(ch))[sample]
                tile=(raw[index]>=64).astype(np.uint8)*255
                canvas[94:122,x:x+28]=255-tile; x+=32
            # Grayscale RGB565 wire encoding, MSB first.
            v=canvas.astype(np.uint16); packed=((v>>3)<<11)|((v>>2)<<5)|(v>>3)
            payload=np.stack((packed>>8,packed&255),axis=-1).astype(np.uint8).ravel()
            frame_path=Path('build/ocr-composed.rgb565'); frame_path.write_bytes(payload.tobytes())
            if message=='WATER' and sample==0:
                Path('build/ocr-recognizer/line.bin').write_bytes(payload.tobytes())
            result=subprocess.run([str(args.runner),str(frame_path)],capture_output=True,text=True)
            lines=result.stdout.strip().splitlines()
            status=0 if result.returncode==0 else 2
            cases.append({'expected':message,'status':status,'text':'\n'.join(lines[1:]),
                          'diagnostics':lines[0] if lines else result.stderr})
            if sample==0: cv2.imwrite(f'build/ocr-composed-{message.split()[0]}.png',canvas)
    accepted_lines=[v for v in cases if v['status']==0]
    report={'c_glyph_count':1024,'composed_cases':len(cases),'composed_accepted':len(accepted_lines),
            'composed_exact':sum(v['text']==v['expected'] for v in accepted_lines),'cases':cases}
    Path('build/ocr-pipeline-evaluation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='cases'},indent=2))
    assert accepted_lines, 'No full line passed; do not claim end-to-end OCR'
    assert report['composed_exact']>=1, 'No exact line passed'
    subprocess.run([str(Path('build/ocr-recognizer/OCR_Check.exe').resolve())],
                   cwd='build/ocr-recognizer',check=True)


if __name__=='__main__': main()
