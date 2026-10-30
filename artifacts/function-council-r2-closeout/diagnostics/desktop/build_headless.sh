#!/usr/bin/env bash
# Builds the product's platform-neutral objects (the host-native-selftests.sh source rule and
# COMMON flags, minus self-test TUs) + the vendored kernel into scratch, then links the two
# headless probes copied from FUNCTION-COUNCIL-R2 seat G. Nothing is written to the repo.
set -euo pipefail
CPP=/home/user/Forgeshape/app/src/main/cpp
S=${S:-/tmp/claude-0/fc2c-scratch/desktop}; D=$(cd "$(dirname "$0")" && pwd)
CXX=${CXX:-g++}
COMMON=(-std=c++17 -O1 -g0 -I"$CPP" -I"$CPP/third_party/manifold/include" -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)
mkdir -p "$S/obj" "$S/kernel"
ls "$CPP"/third_party/manifold/src/*.cpp | xargs -P"$(nproc)" -I{} sh -c '"$0" "$@" -O2 -w -c {} -o '"$S"'/kernel/$(basename {} .cpp).o' "$CXX" "${COMMON[@]}"
ar rcs "$S/kernel/libmanifold.a" "$S"/kernel/*.o
ls "$CPP"/forgeshape_*.cpp | grep -v -e '/forgeshape_jni.cpp$' -e '/forgeshape_renderer.cpp$' -e '_selftest.cpp$' > "$S/product_tus.txt"
xargs -P"$(nproc)" -I{} sh -c '"$0" "$@" -c {} -o '"$S"'/obj/$(basename {} .cpp).o' "$CXX" "${COMMON[@]}" < "$S/product_tus.txt"
echo "product objects: $(ls $S/obj/*.o | wc -l)"
for p in headless_shell_probe seq_reencode; do
  "$CXX" "${COMMON[@]}" "$D/$p.cpp" "$S"/obj/*.o "$S/kernel/libmanifold.a" -o "$S/$p"
done
echo built
