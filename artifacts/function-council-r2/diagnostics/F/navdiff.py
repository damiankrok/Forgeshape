import sys
from PIL import Image
import numpy as np
d=sys.argv[1]
a=np.asarray(Image.open(d+'/04_navigator_default_plus_z.png').convert('RGB')).astype(int)
for f in ['05_navigator_other_plane.png','06_navigator_rotated_plus_90.png']:
    b=np.asarray(Image.open(d+'/'+f).convert('RGB')).astype(int)
    diff=np.abs(a-b).sum(axis=2)>30
    mask=np.zeros_like(diff); mask[336:691,442:859]=True
    print(f,'changed px total',int(diff.sum()),'inside navigator',int((diff&mask).sum()),'outside',int((diff&~mask).sum()))
    ys,xs=np.nonzero(diff&~mask)
    if len(ys): print('  outside bbox x',xs.min(),xs.max(),'y',ys.min(),ys.max())
