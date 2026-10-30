#!/usr/bin/env bash
# Prints the exact product lines WorkspaceTranscript.java and the lifecycle model copy,
# so the transcription can be compared by eye. Read-only.
R=/home/user/Forgeshape/app/src/main
W=$R/java/com/forgeshape/app/EditorWorkspaceView.java
A=$R/java/com/forgeshape/app/ForgeShapeActivity.java
echo "git HEAD of product tree: $(git -C /home/user/Forgeshape rev-parse --short HEAD) ; last product commit: $(git -C /home/user/Forgeshape log -1 --format=%h -- app/)"
for r in 127,127 271,272 1019,1030 2597,2614 2623,2691 5458,5472 5483,5513 5579,5604 5652,5701 5709,5726 5778,5800 6004,6004 6047,6064 6104,6111; do
  echo "----- EditorWorkspaceView.java:$r"; sed -n "${r}p" $W | nl -ba -v ${r%,*}
done
for r in 268,280 353,359 513,537 569,584; do
  echo "----- ForgeShapeActivity.java:$r"; sed -n "${r}p" $A | nl -ba -v ${r%,*}
done
echo "----- AndroidManifest.xml:31"; sed -n 31p $R/AndroidManifest.xml
echo "----- ProjectTransfer.java:95-100,122-126"; sed -n '95,100p;122,126p' $R/java/com/forgeshape/app/ProjectTransfer.java
