#!/usr/bin/env bash
# compile one header standalone (self-containment) with host clang++ 18
CPP=/home/user/Forgeshape/app/src/main/cpp
h="$1"
out=$(clang++ -std=c++17 -fsyntax-only -x c++ -I"$CPP" -I"$CPP/third_party/manifold/include" -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM "$h" 2>&1)
rc=$?
echo "$(basename $h) rc=$rc $(printf '%s' "$out" | grep -m1 'error:' | sed -E 's|.*/||' | cut -c1-120)"
