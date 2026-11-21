"""Development-only mask inspection; production recognition remains C."""
from pathlib import Path
import cv2
import numpy as np
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('image')
args=parser.parse_args()
gray=cv2.cvtColor(cv2.imread(args.image),cv2.COLOR_BGR2GRAY)
results=[]
for bias in (5,7,10):
    mean=cv2.boxFilter(gray.astype(np.float32),-1,(25,25),normalize=True,borderType=cv2.BORDER_CONSTANT)
    mask=((gray.astype(float)+bias)<mean).astype(np.uint8)*255
    lines=cv2.HoughLinesP(mask,1,np.pi/360,threshold=90,minLineLength=100,maxLineGap=30)
    if lines is not None:
        for a in lines[:,0]:
            x0,y0,x1,y1=map(int,a)
            if abs(y1-y0)>.15*abs(x1-x0): continue
            cv2.line(mask,(x0,y0),(x1,y1),0,5)
    mask=cv2.morphologyEx(mask,cv2.MORPH_CLOSE,np.ones((2,2),np.uint8))
    n,lab,stats,cent=cv2.connectedComponentsWithStats(mask)
    display=cv2.cvtColor(255-mask,cv2.COLOR_GRAY2BGR)
    boxes=[]
    for x,y,w,h,area in stats[1:]:
        if h>=8 and w<h*3 and area>=10:
            boxes.append([int(x),int(y),int(w),int(h),int(area)])
            cv2.rectangle(display,(x,y),(x+w,y+h),(0,0,255),1)
    print(bias,boxes)
    results.append(display)
cv2.imwrite('build/ocr-mask-comparison.png',np.hstack(results))
