"""Fine-tune glyph weights on explicitly labelled camera handwriting.

Camera development examples are training data, never held-out evidence.
Replay of NIST training data limits catastrophic forgetting. A fresh physical
capture is required before claiming improved camera recognition.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','4')
import argparse
import json
from pathlib import Path
import cv2
import numpy as np
from train import load,features,probabilities,export,ALPHABET
import hashlib

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,required=True)
    parser.add_argument('--epochs',type=int,default=18)
    args=parser.parse_args()
    entries=json.loads(args.manifest.read_text())
    camera=[(cv2.imread(e['image'],cv2.IMREAD_GRAYSCALE),ALPHABET.index(e['label'])) for e in entries]
    assert all(im is not None and im.shape==(24,24) for im,_ in camera)
    rng=np.random.default_rng(702)
    original=np.load('build/ocr-model.npz'); params=[original[f'arr_{i}'].copy() for i in range(4)]
    raw,y=load(Path('build/ocr-data'),'train')
    # This subset is training only. Official test data is not used to tune.
    ids=rng.permutation(len(y))[:12000]; x=features(raw[ids]); y=y[ids]
    first=[np.zeros_like(p) for p in params]; second=[np.zeros_like(p) for p in params]; step=0
    for epoch in range(args.epochs):
        order=rng.permutation(len(y))
        for start in range(0,len(order),128):
            replay=order[start:start+128]; cx=[]; cy=[]
            for _ in range(48):
                im,label=camera[int(rng.integers(len(camera)))]; im=im.copy()
                transform=cv2.getRotationMatrix2D((11.5,11.5),float(rng.uniform(-8,8)),float(rng.uniform(.9,1.1)))
                transform[:,2]+=rng.uniform(-1,1,size=2)
                im=cv2.warpAffine(im,transform,(24,24))
                if rng.random()<.25: im=cv2.GaussianBlur(im,(3,3),.5)
                cx.append(im.ravel()); cy.append(label)
            batch=np.vstack((x[replay],np.array(cx,dtype=np.float32)/255)); labels=np.r_[y[replay],cy]
            w1,b1,w2,b2=params
            hidden=np.maximum(0,batch@w1+b1); scores=hidden@w2+b2; scores-=scores.max(1,keepdims=True)
            prob=np.exp(scores); prob/=prob.sum(1,keepdims=True)
            prob[np.arange(len(labels)),labels]-=1; prob/=len(labels)
            dh=prob@w2.T; dh[hidden<=0]=0
            gradients=(batch.T@dh+1e-5*w1,dh.sum(0),hidden.T@prob+1e-5*w2,prob.sum(0))
            step+=1
            for p,g,m,v in zip(params,gradients,first,second):
                m*=.9; m+=.1*g; v*=.999; v+=.001*g*g
                p-=.00015*(m/(1-.9**step))/(np.sqrt(v/(1-.999**step))+1e-7)
        pred=probabilities(np.array([im.ravel() for im,_ in camera],np.float32)/255,params)
        print(f'Calibration {epoch+1}/{args.epochs}: development glyph probabilities '+
              ','.join(f'{ALPHABET[label]}={pred[i,label]:.2f}' for i,(_,label) in enumerate(camera)),flush=True)
    dest=Path('drivers/camera/ocr/OCRModel.c'); export(params,dest)
    test,ty=load(Path('build/ocr-data'),'test'); pred=probabilities(features(test),params)
    top=np.sort(pred,axis=1)[:,-2:]; accepted=(top[:,-1]>=.8)&(top[:,-1]-top[:,-2]>=.35)
    report={'calibration_seed':702,'calibration_glyph_count':len(camera),'calibration_epochs':args.epochs,
            'development_source':'Explicitly labelled camera glyphs from manifest; not held out',
            'manifest':str(args.manifest),'manifest_sha256':hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
            'training_samples':entries,
            'test_count':len(ty),'test_accuracy':float((pred.argmax(1)==ty).mean()),
            'test_acceptance':float(accepted.mean()),
            'accepted_test_accuracy':float((pred.argmax(1)[accepted]==ty[accepted]).mean()),
            'model_sha256':hashlib.sha256(dest.read_bytes()).hexdigest()}
    Path('drivers/camera/ocr/calibration.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__': main()
