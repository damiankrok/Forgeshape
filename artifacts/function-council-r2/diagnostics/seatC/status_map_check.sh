#!/usr/bin/env bash
# FC2 seat C: compares native CadStatus enumerator order with Java NativeViewport.CAD_* constants.
set -e
R=/home/user/Forgeshape/app/src/main; D=/tmp/claude-0/fc2-scratch/seatC
awk '/^enum class CadStatus/{f=1;next} f&&/^};/{exit} f' $R/cpp/forgeshape_sketch.h | grep -vE "^\s*//" | grep -oE "^\s+[A-Za-z]+," | tr -d ' ,' > $D/enum.txt
grep -oE "static final int CAD_[A-Z0-9_]+ = [0-9]+" $R/java/com/forgeshape/app/NativeViewport.java > $D/java.txt
python3 -I - <<'P'
import re
d='/tmp/claude-0/fc2-scratch/seatC/'
en=[l.strip() for l in open(d+'enum.txt')]
snake=lambda s: re.sub(r'(?<!^)(?=[A-Z])','_',s).upper()
idx={snake(n):i for i,n in enumerate(en)}
matched=bad=0
for l in open(d+'java.txt'):
    n,v=re.match(r'static final int CAD_([A-Z0-9_]+) = (\d+)',l).groups()
    if n in idx:
        matched+=1
        if idx[n]!=int(v): bad+=1; print("MISMATCH",n,v,idx[n])
print(f"enum={len(en)} java_status_constants_matched={matched} mismatches={bad}")
P
