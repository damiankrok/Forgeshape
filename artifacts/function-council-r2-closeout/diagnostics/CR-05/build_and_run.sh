#!/usr/bin/env bash
# CR-05: compile the REAL product classes (copied byte-identical, hash-checked)
# with the sandbox shims, run the harness. Binaries stay in the scratch dir.
set -u
R=/home/user/Forgeshape
D=$R/artifacts/function-council-r2-closeout/diagnostics/CR-05
S=/tmp/claude-0/fc2c-scratch/CR-05
P=$R/app/src/main/java/com/forgeshape/app
rm -rf /tmp/claude-0/fc2c-scratch/CR-05/src /tmp/claude-0/fc2c-scratch/CR-05/classes /tmp/claude-0/fc2c-scratch/CR-05/run
mkdir -p $S/src/com/forgeshape/app $S/classes $S/run
REAL="AutosaveController ProjectCheckpoint ProjectSlot EditorUiState LengthUnit DiagnosticLog"
for c in $REAL; do cp $P/$c.java $S/src/com/forgeshape/app/; done
echo "# product sources copied unchanged (sha256 prefix: product copy verdict)"
for c in $REAL; do
  a=$(sha256sum $P/$c.java | cut -c1-16); b=$(sha256sum $S/src/com/forgeshape/app/$c.java | cut -c1-16)
  echo "$c $a $b $([ "$a" = "$b" ] && echo SAME || echo DIFF)"
done
javac -nowarn -d $S/classes $(find $D/shim $D/harness -name '*.java') $S/src/com/forgeshape/app/*.java
echo "javac_exit=$?"
java -cp $S/classes com.forgeshape.app.Cr05Repro $S/run
echo "java_exit=$?"
