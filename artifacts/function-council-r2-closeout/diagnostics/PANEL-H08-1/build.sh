#!/usr/bin/env bash
# Builds the platform-neutral product objects exactly as scripts/host-native-selftests.sh
# does (same source list, same COMMON flags), then links h08_diag.cpp against them.
set -euo pipefail
ROOT=/home/user/Forgeshape
CPP="$ROOT/app/src/main/cpp"
OUT=/tmp/claude-0/fc2c-scratch/PANEL-H08-1
DIAG="$ROOT/artifacts/function-council-r2-closeout/diagnostics/PANEL-H08-1"
CXX=g++
mkdir -p "$OUT/obj" "$OUT/kernel"
COMMON=(-std=c++17 -O1 -g0 -I"$CPP" -I"$CPP/third_party/manifold/include"
        -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)
KLIB="$OUT/kernel/libmanifold.a"
if [ ! -f "$KLIB" ]; then
  for src in "$CPP"/third_party/manifold/src/*.cpp; do
    "$CXX" "${COMMON[@]}" -O2 -w -c "$src" -o "$OUT/kernel/$(basename "$src" .cpp).o" & done; wait
  ar rcs "$KLIB" "$OUT"/kernel/*.o
fi
OBJS=()
for src in "$CPP"/forgeshape_*.cpp; do
  case "$(basename "$src")" in forgeshape_jni.cpp|forgeshape_renderer.cpp) continue;; esac
  obj="$OUT/obj/$(basename "$src" .cpp).o"; OBJS+=("$obj")
  [ -f "$obj" ] || { "$CXX" "${COMMON[@]}" -w -c "$src" -o "$obj" & }
  while [ "$(jobs -r | wc -l)" -ge "$(nproc)" ]; do sleep 0.2; done
done; wait
"$CXX" "${COMMON[@]}" -c "$DIAG/h08_diag.cpp" -o "$OUT/h08_diag.o"
"$CXX" -o "$OUT/h08_diag" "$OUT/h08_diag.o" "${OBJS[@]}" "$KLIB"
