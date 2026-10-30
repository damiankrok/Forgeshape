#!/usr/bin/env bash
# FUNCTION-COUNCIL-R2-CLOSEOUT desktop seat: platform-neutral compile probe.
# Re-run of FUNCTION-COUNCIL-R2 seat G probes P1-P6 (diagnostics/G/COMMANDS.txt) on the current tree.
# Scratch only: everything is written under $S; nothing is built into the product.
# usage: probe.sh  (env: S scratch dir, NDK path)
set -u
ROOT=/home/user/Forgeshape
CPP=$ROOT/app/src/main/cpp
S=${S:-/tmp/claude-0/fc2c-scratch/desktop}
NDK=${NDK:-/root/android-sdk/ndk/29.0.14206865}
GLSLC=$NDK/shader-tools/linux-x86_64/glslc
SPVVAL=$NDK/shader-tools/linux-x86_64/spirv-val
LIBCXX=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1
DEFS=(-DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)
INC=(-I"$CPP" -I"$CPP/third_party/manifold/include")
mkdir -p "$S/logs" "$S/gen"
cd "$S"
# TU list = exactly the host-native-selftests.sh rule: forgeshape_*.cpp minus jni/renderer
ls "$CPP"/forgeshape_*.cpp | grep -v -e '/forgeshape_jni.cpp$' -e '/forgeshape_renderer.cpp$' > tus.txt
ls "$CPP"/forgeshape_*.h > hdrs.txt
echo "TUs: $(wc -l < tus.txt)  headers: $(wc -l < hdrs.txt)"
one() { # compiler tag tu extra...
  local c=$1 tag=$2 tu=$3; shift 3
  out=$("$c" -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -Wno-unused-parameter "$@" "${INC[@]}" "${DEFS[@]}" "$tu" 2>&1); rc=$?
  echo "$(basename $tu) rc=$rc warnings=$(printf '%s' "$out" | grep -c 'warning:') errors=$(printf '%s' "$out" | grep -c 'error:')"
  [ -n "$out" ] && printf '%s\n' "$out" > "logs/${tag}_$(basename $tu).log"; return 0; }
export -f one; export INC DEFS
summ() { echo "$1: pass=$(grep -c ' rc=0 ' $2) fail=$(grep -vc ' rc=0 ' $2) warnings=$(awk -F'warnings=' '{split($2,a," ");s+=a[1]}END{print s+0}' $2) errors=$(awk -F'errors=' '{s+=$2}END{print s+0}' $2)"; }
# P1a g++ libstdc++ ; P1b clang++ libstdc++ ; P2 clang++ NDK libc++
while read tu; do one g++ gxx "$tu"; done < tus.txt > p1_gxx.txt
while read tu; do one clang++ clang "$tu"; done < tus.txt > p1_clang.txt
while read tu; do one clang++ libcxx "$tu" -nostdinc++ -isystem "$LIBCXX"; done < tus.txt > p2_clang_libcxx.txt
summ "P1a g++ libstdc++" p1_gxx.txt; summ "P1b clang++ libstdc++" p1_clang.txt; summ "P2 clang++ NDK-libc++" p2_clang_libcxx.txt
# warning categories
for f in p1_gxx p1_clang; do echo "$f warning categories:"; cat logs/$( [ $f = p1_gxx ] && echo gxx || echo clang )_*.log 2>/dev/null | grep -o 'warning:.*\[-W[a-z-]*\]' | grep -o '\[-W[a-z-]*\]' | sort | uniq -c | sort -rn; done
# P3 header self-containment, g++ and clang++
for c in g++ clang++; do
  while read h; do out=$($c -std=c++17 -fsyntax-only -x c++ "${INC[@]}" "${DEFS[@]}" "$h" 2>&1); rc=$?
    echo "$(basename $h) rc=$rc $(printf '%s' "$out" | grep -m1 'error:' | sed -E 's|.*/||' | cut -c1-120)"; done < hdrs.txt > p3_hdr_$c.txt
  echo "P3 headers $c: pass=$(grep -c ' rc=0' p3_hdr_$c.txt) fail=$(grep -vc ' rc=0' p3_hdr_$c.txt)"; grep -v ' rc=0' p3_hdr_$c.txt
done
# P5 shaders -> gen/*.spv.inc (needed by P4) + spirv-val
ok=0; n=0
for sh in "$CPP"/shaders/*; do n=$((n+1)); b=$(basename $sh)
  "$GLSLC" -O --target-env=vulkan1.0 -mfmt=c "$sh" -o gen/$b.spv.inc && "$GLSLC" -O --target-env=vulkan1.0 "$sh" -o gen/$b.spv && "$SPVVAL" --target-env vulkan1.0 gen/$b.spv && ok=$((ok+1)) || echo "SHADER FAIL $b"
done; echo "P5 shaders: $ok/$n glslc+spirv-val OK"
# P4 renderer with stubs (5 declarations) + NDK Khronos Vulkan headers
for c in g++ clang++; do
  out=$($c -std=c++17 -fsyntax-only -Wall -Wextra -Wno-unused-parameter -Istub -Istubvk -Igen "${INC[@]}" "${DEFS[@]}" "$CPP/forgeshape_renderer.cpp" 2>&1); rc=$?
  printf '%s\n' "$out" > logs/p4_renderer_$c.log
  echo "P4 renderer $c: rc=$rc warnings=$(printf '%s' "$out" | grep -c 'warning:') errors=$(printf '%s' "$out" | grep -c 'error:')"
done
# P4b renderer WITHOUT stubs (control: proves the Android dependency)
out=$(clang++ -std=c++17 -fsyntax-only -Istubvk -Igen "${INC[@]}" "${DEFS[@]}" "$CPP/forgeshape_renderer.cpp" 2>&1); rc=$?
echo "P4b renderer clang++ without android stubs: rc=$rc first-error: $(printf '%s' "$out" | grep -m1 'error:' | sed -E 's|.*/||')"
# P4c JNI TU (control): needs jni.h and android headers
out=$(clang++ -std=c++17 -fsyntax-only -Istub -Istubvk -Igen "${INC[@]}" "${DEFS[@]}" "$CPP/forgeshape_jni.cpp" 2>&1); rc=$?
echo "P4c jni clang++ with the same stubs: rc=$rc errors=$(printf '%s' "$out" | grep -c 'error:') first-error: $(printf '%s' "$out" | grep -m1 'error:' | sed -E 's|.*/||')"
# P6 product CMake configure on host (glslc on PATH)
rm -rf cmake_host; PATH="$(dirname $GLSLC):$PATH" cmake -S "$CPP" -B cmake_host -G Ninja -DCMAKE_BUILD_TYPE=Debug > logs/p6_cmake.log 2>&1; rc=$?
echo "P6 cmake configure rc=$rc; NOTFOUND lines:"; grep -E 'NOTFOUND|CMake Error' logs/p6_cmake.log | sort -u | head
