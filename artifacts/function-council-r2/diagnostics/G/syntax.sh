#!/usr/bin/env bash
# usage: syntax.sh <compiler> <tu> ; prints OK/FAIL per TU
CPP=/home/user/Forgeshape/app/src/main/cpp
c="$1"; tu="$2"; shift 2
out=$("$c" -std=c++17 -fsyntax-only -Wall -Wextra -Wpedantic -Wno-unused-parameter "$@" -I"$CPP" -I"$CPP/third_party/manifold/include" -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM "$tu" 2>&1)
rc=$?
nw=$(printf '%s' "$out" | grep -c 'warning:')
ne=$(printf '%s' "$out" | grep -c 'error:')
echo "$(basename $tu) rc=$rc warnings=$nw errors=$ne"
[ -n "$out" ] && printf '%s\n' "$out" > "logs/$(basename $c)_$(basename $tu).log"
exit 0
