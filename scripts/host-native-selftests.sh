#!/usr/bin/env bash
# Builds ForgeShape's platform-neutral native domain and every self-test suite
# for the HOST (Linux, g++ or clang++) and runs them -- the same suites a debug
# launch runs from `NativeViewport.start()`, without an emulator.
#
# Developer evidence only. It proves the domain arithmetic, the codec and the
# kernel on the host ABI; it does NOT replace the on-device startup tokens
# (`CI DEVICE`), which are the gate, and it compiles nothing Android-specific:
# `forgeshape_jni.cpp` and `forgeshape_renderer.cpp` are the only two
# translation units that are not platform-neutral, and both are left out.
#
# Usage: scripts/host-native-selftests.sh [suite-filter]
#   suite-filter  optional substring; only suites whose name contains it run.
# Env:   CXX (default g++), HOST_SELFTEST_OUT (default build/host-selftests)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/app/src/main/cpp"
OUT="${HOST_SELFTEST_OUT:-$ROOT/build/host-selftests}"
CXX="${CXX:-g++}"
FILTER="${1:-}"
mkdir -p "$OUT/obj" "$OUT/kernel"

COMMON=(-std=c++17 -O1 -g0 -I"$CPP" -I"$CPP/third_party/manifold/include"
        -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM)

# The vendored kernel, built once and cached: it never changes under a task.
KLIB="$OUT/kernel/libmanifold.a"
if [ ! -f "$KLIB" ]; then
    pids=()
    for src in "$CPP"/third_party/manifold/src/*.cpp; do
        "$CXX" "${COMMON[@]}" -O2 -w -c "$src" -o "$OUT/kernel/$(basename "$src" .cpp).o" &
        pids+=($!)
    done
    for p in "${pids[@]}"; do wait "$p"; done
    ar rcs "$KLIB" "$OUT"/kernel/*.o
fi

# Every product and self-test source except the two platform-bound ones.
SOURCES=()
for src in "$CPP"/forgeshape_*.cpp; do
    case "$(basename "$src")" in
        forgeshape_jni.cpp|forgeshape_renderer.cpp|forgeshape_render_surface.cpp|forgeshape_viewport_frame.cpp|forgeshape_renderer_ui.cpp) continue ;;
    esac
    SOURCES+=("$src")
done

# Incremental: an object is rebuilt when its source or any header is newer.
NEWEST_HEADER="$(ls -t "$CPP"/forgeshape_*.h | head -1)"
pids=()
OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="$OUT/obj/$(basename "$src" .cpp).o"
    OBJECTS+=("$obj")
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ] || [ "$NEWEST_HEADER" -nt "$obj" ]; then
        "$CXX" "${COMMON[@]}" -Wall -Wextra -Wno-unused-parameter -c "$src" -o "$obj" &
        pids+=($!)
        if [ "${#pids[@]}" -ge "$(nproc)" ]; then
            wait "${pids[0]}"; pids=("${pids[@]:1}")
        fi
    fi
done
for p in "${pids[@]}"; do wait "$p"; done

# The runner is a committed file the desktop build compiles too.
MAIN="$ROOT/scripts/host_selftest_main.cpp"
"$CXX" "${COMMON[@]}" -c "$MAIN" -o "$OUT/host_selftest_main.o"
"$CXX" -o "$OUT/host-selftests" "$OUT/host_selftest_main.o" "${OBJECTS[@]}" "$KLIB"
cd "$ROOT"
"$OUT/host-selftests" "$FILTER"
