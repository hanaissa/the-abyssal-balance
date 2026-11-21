"""Train a small handwriting MLP and export flash-resident C parameters.

Training runs on a computer; inference is plain C on the Pico. NIST EMNIST
balanced training/test partitions stay separate. Requires existing NumPy/OpenCV.
"""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS', '4')
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct
import time
import cv2
import numpy as np

ALPHABET = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
BALANCED = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabdefghnqrt'
SIDE=24
EXTENT=20
HIDDEN=192


def normalize(image):
    """Same binary box-area normalization as OCR.c; max extent 12 in 16x16."""
    ys, xs = np.nonzero(image)
    result = np.zeros((SIDE, SIDE), np.uint8)
    if len(xs) == 0:
        return result
    image = image[ys.min():ys.max()+1, xs.min():xs.max()+1]
    h, w = image.shape
    nw, nh = (EXTENT, max(1, h*EXTENT//w)) if w >= h else (max(1, w*EXTENT//h), EXTENT)
    integral = np.pad(image.astype(np.int32).cumsum(0).cumsum(1), ((1,0),(1,0)))
    x0 = np.arange(nw)*w//nw
    x1 = np.minimum(w, (np.arange(1,nw+1)*w+nw-1)//nw)
    y0 = np.arange(nh)*h//nh
    y1 = np.minimum(h, (np.arange(1,nh+1)*h+nh-1)//nh)
    sums = integral[y1[:,None],x1] - integral[y0[:,None],x1] - integral[y1[:,None],x0] + integral[y0[:,None],x0]
    result[(SIDE-nh)//2:(SIDE-nh)//2+nh,(SIDE-nw)//2:(SIDE-nw)//2+nw] = sums // ((y1-y0)[:,None]*(x1-x0))
    return result


def load(data, split):
    images = gzip.decompress((data / f'emnist-balanced-{split}-images-idx3-ubyte.gz').read_bytes())
    labels = gzip.decompress((data / f'emnist-balanced-{split}-labels-idx1-ubyte.gz').read_bytes())
    magic, n, h, w = struct.unpack('>4I', images[:16])
    label_magic, count = struct.unpack('>2I', labels[:8])
    assert (magic, label_magic, h, w, n) == (2051, 2049, 28, 28, count)
    # EMNIST IDX raster coordinates are transposed relative to upright glyphs.
    x = np.frombuffer(images, np.uint8, offset=16).reshape(n,28,28).transpose(0,2,1).copy()
    y = np.frombuffer(labels, np.uint8, offset=8).astype(np.int64)
    # Scope is separated uppercase block letters/digits: do not map lowercase
    # r/t/g handwriting shapes onto their differently shaped capital classes.
    keep=y<36
    return x[keep], y[keep]


def features(images):
    return np.stack([normalize((im >= 64).astype(np.uint8)*255).ravel() for im in images]).astype(np.float32)/255


def augment(images, rng):
    result = []
    for image in images:
        transform = cv2.getRotationMatrix2D((13.5,13.5), float(rng.uniform(-12,12)), float(rng.uniform(.85,1.15)))
        transform[0,1] += rng.uniform(-.12,.12)
        im = cv2.warpAffine(image, transform, (28,28))
        if rng.random() < .3:
            side = int(rng.integers(10,20))
            im = cv2.resize(cv2.resize(im,(side,side),interpolation=cv2.INTER_AREA),(28,28))
        if rng.random() < .3:
            im = cv2.dilate(im, np.ones((2,2),np.uint8))
        if rng.random() < .25:
            im = cv2.GaussianBlur(im,(3,3),.7)
        result.append(normalize((im >= 64).astype(np.uint8)*255).ravel())
    return np.asarray(result,np.float32)/255


def probabilities(x, params):
    w1,b1,w2,b2 = params
    hidden = np.maximum(0,x@w1+b1)
    scores = hidden@w2+b2
    scores -= scores.max(1,keepdims=True)
    result = np.exp(scores)
    result /= result.sum(1,keepdims=True)
    return result


def export(params, destination):
    destination.parent.mkdir(parents=True,exist_ok=True)
    parts = ['/* Trained EMNIST handwriting parameters; see MODEL.md. */', '#include "OCRModel.h"']
    for name, value in zip(('ocr_w1','ocr_b1','ocr_w2','ocr_b2'), params):
        # Row-major [input][hidden] / [hidden][class], matching C inference.
        flat = value.astype(np.float32).ravel()
        parts.append(f'const float {name}[{len(flat)}] = {{')
        for start in range(0,len(flat),10):
            parts.append(','.join(f'{float(v):.8e}f' for v in flat[start:start+10])+',')
        parts.append('};')
    destination.write_text('\n'.join(parts)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=Path('build/ocr-data'))
    parser.add_argument('--epochs',type=int,default=60)
    parser.add_argument('--output',type=Path,default=Path('drivers/camera/ocr/OCRModel.c'))
    args = parser.parse_args()
    rng = np.random.default_rng(701)
    raw, y = load(args.data,'train')
    # Validation is carved from training. Official test data is evaluated once at end.
    indices = rng.permutation(len(y)); valid, train = indices[:8000], indices[8000:]
    x = features(raw)
    params = [(rng.standard_normal((SIDE*SIDE,HIDDEN))*np.sqrt(2/(SIDE*SIDE))).astype(np.float32), np.zeros(HIDDEN,np.float32),
              (rng.standard_normal((HIDDEN,36))*np.sqrt(2/HIDDEN)).astype(np.float32),np.zeros(36,np.float32)]
    first = [np.zeros_like(p) for p in params]; second = [np.zeros_like(p) for p in params]
    best = 0; best_params = None; step = 0
    started = time.monotonic()
    for epoch in range(args.epochs):
        order = rng.permutation(train)
        learning_rate = .0015 * (.5 if epoch >= 25 else 1) * (.5 if epoch >= 38 else 1)
        for start in range(0,len(order),256):
            ids = order[start:start+256]
            batch = x[ids] if rng.random() < .5 else augment(raw[ids],rng)
            labels = y[ids]; w1,b1,w2,b2 = params
            hidden = np.maximum(0,batch@w1+b1)
            logits = hidden@w2+b2; logits -= logits.max(1,keepdims=True)
            prob = np.exp(logits); prob /= prob.sum(1,keepdims=True)
            prob[np.arange(len(ids)),labels] -= 1; prob /= len(ids)
            dh = prob@w2.T; dh[hidden <= 0] = 0
            gradients = (batch.T@dh+1e-5*w1,dh.sum(0),hidden.T@prob+1e-5*w2,prob.sum(0))
            step += 1
            for p,g,m,v in zip(params,gradients,first,second):
                m *= .9; m += .1*g; v *= .999; v += .001*g*g
                p -= learning_rate*(m/(1-.9**step))/(np.sqrt(v/(1-.999**step))+1e-7)
        pred = probabilities(x[valid],params)
        accuracy = float((pred.argmax(1)==y[valid]).mean())
        if accuracy > best:
            best = accuracy; best_params = [p.copy() for p in params]
        print(f'Epoch {epoch+1}/{args.epochs}: validation {accuracy:.4f}, best {best:.4f}, elapsed {time.monotonic()-started:.0f}s',flush=True)
    test_raw,test_y = load(args.data,'test')
    test_x = features(test_raw)
    pred = probabilities(test_x,best_params)
    best_two = np.sort(pred,axis=1)[:,-2:]
    accepted = (best_two[:,-1] >= .80) & ((best_two[:,-1]-best_two[:,-2]) >= .35)
    report = {'source':'NIST EMNIST balanced','seed':701,'output_alphabet':ALPHABET,
              'train_count':len(train),'validation_count':len(valid),'test_count':len(test_y),
              'test_accuracy':float((pred.argmax(1)==test_y).mean()),
              'test_acceptance':float(accepted.mean()),
              'accepted_test_accuracy':float((pred.argmax(1)[accepted]==test_y[accepted]).mean()),
              'epochs':args.epochs,'architecture':[SIDE*SIDE,HIDDEN,36],
              'scope':'Uppercase block letters and digits only', 'normalization_side':SIDE,'normalization_extent':EXTENT}
    export(best_params,args.output)
    report['model_sha256'] = hashlib.sha256(args.output.read_bytes()).hexdigest()
    (args.output.parent/'evaluation.json').write_text(json.dumps(report,indent=2)+'\n')
    np.savez('build/ocr-model.npz',*best_params)
    # Held-out real handwriting fixtures for C inference parity, not synthetic fonts.
    np.savez('build/ocr-test-vectors.npz',pixels=(test_x[:1024]*255).astype(np.uint8),labels=test_y[:1024])
    print(json.dumps(report,indent=2),flush=True)


if __name__ == '__main__':
    main()
