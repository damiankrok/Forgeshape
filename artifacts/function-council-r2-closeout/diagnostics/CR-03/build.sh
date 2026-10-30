#!/usr/bin/env bash
# Builds the CR-03 diagnostic exactly as scripts/host-native-selftests.sh
# assembles its host build: same COMMON flags, every forgeshape_*.cpp except
# forgeshape_jni.cpp / forgeshape_renderer.cpp, the vendored Manifold archive.
set -euo pipefail
ROOT=/home/user/Forgeshape
CPP=$ROOT/app/src/main/cpp
S=/tmp/claude-0/fc2c-scratch/CR-03
D=$ROOT/artifacts/function-council-r2-closeout/diagnostics/CR-03
CXX=${CXX:-g++}
COMMON=(-std=c++17 -O1 -g0 -I"$CPP" -I"$CPP/third_party/manifold/include"
        -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)
mkdir -p "$S/obj" "$S/kernel"
KLIB="$S/kernel/libmanifold.a"
if [ ! -f "$KLIB" ]; then
  for src in "$CPP"/third_party/manifold/src/*.cpp; do
    "$CXX" "${COMMON[@]}" -O2 -w -c "$src" -o "$S/kernel/$(basename "$src" .cpp).o" &
  done; wait; ar rcs "$KLIB" "$S"/kernel/*.o
fi
OBJS=()
for src in "$CPP"/forgeshape_*.cpp; do
  case "$(basename "$src")" in forgeshape_jni.cpp|forgeshape_renderer.cpp) continue;; esac
  obj="$S/obj/$(basename "$src" .cpp).o"; OBJS+=("$obj")
  [ -f "$obj" ] || { "$CXX" "${COMMON[@]}" -Wall -Wextra -Wno-unused-parameter -c "$src" -o "$obj" & }
  while [ "$(jobs -rp | wc -l)" -ge "$(nproc)" ]; do sleep 0.2; done
done; wait
"$CXX" "${COMMON[@]}" "$D/cr03_import_during_sketch.cpp" "${OBJS[@]}" "$KLIB" -o "$S/cr03"
echo BUILD_OK
