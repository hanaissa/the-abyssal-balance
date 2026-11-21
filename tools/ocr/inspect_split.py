"""Development inspection of alternate cuts; never supplies runtime text."""
import re
from pathlib import Path
import cv2
import numpy as np
from train import normalize,probabilities,ALPHABET
source=Path('drivers/camera/ocr/OCRModel.c').read_text()
arrays=[]
for body in re.findall(r'= \{(.*?)\};',source,re.S):
    arrays.append(np.fromstring(body.replace('f','').replace('\n',''),sep=',',dtype=np.float32))
arrays[0]=arrays[0].reshape(576,192); arrays[2]=arrays[2].reshape(192,36)
mask=(cv2.imread('build/ocr-mask.pgm',0)<128).astype(np.uint8)*255
crop=mask[74:105,72:137]
n=crop.shape[1]; dp=[(-1e9,[]) for _ in range(n+1)]; dp[0]=(0,[])
for end in range(8,n+1):
    for start in range(max(0,end-28),end-7):
        if dp[start][0]<-1e8: continue
        glyph=normalize(crop[:,start:end]); p=probabilities(glyph.reshape(1,-1).astype(np.float32)/255,arrays)[0]
        k=int(p.argmax()); width=end-start
        score=float(np.log(max(float(p[k]),1e-9)))-.5-max(0,width/31-.75)*4
        score-=float((crop[:,end-1]>0).sum())/31*.2
        value=dp[start][0]+score
        if value>dp[end][0]: dp[end]=(value,dp[start][1]+[(start,end,ALPHABET[k],float(p[k]))])
print(dp[n])
