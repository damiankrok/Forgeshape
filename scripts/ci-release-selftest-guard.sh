#!/usr/bin/env bash
# CI-only release guard: the debug self-tests must not reach a release binary.
#
# The self-test translation units are dropped from the release CMake source
# list (deep-audit finding F-16, closed by POST-AUDIT-HARDEN-R1), and the local
# evidence for every stage since has been "0 self-test symbols in the release
# .so on both ABIs". This restates that measurement for a Linux runner with the
# pinned NDK's own llvm tools, on the library bytes actually PACKAGED in each
# APK rather than an intermediate directory.
#
# Two probes per library:
#   - dynamic symbols containing `SelfTests` (the `run*SelfTests` entry points);
#   - strings matching the self-test token and check-name families
#     (`_SELFTEST_`, `FSR1A_`, `CADUXR1_`, `DAR1_`, `CADVS_`), and the debug-only
#     Ready-tap attribution token (`FORGESHAPE_SKETCH_TAP:`), which a release
#     build must not log.
# Release must read 0 and 0. Debug must read MORE than 0 for both, so a probe
# that silently matches nothing — a renamed tool, a wrong path, an empty
# extraction — fails here instead of passing vacuously.
#
# No device, no adb. Reads only the two APKs a `:app:assembleDebug
# :app:assembleRelease` run leaves behind.
set -euo pipefail

NDK_VERSION="${FORGESHAPE_NDK_VERSION:-29.0.14206865}"
SDK_ROOT="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK_ROOT" ]; then
    echo "RELEASE_SELFTEST_GUARD=ERROR ANDROID_HOME/ANDROID_SDK_ROOT is not set" >&2
    exit 2
fi
LLVM_BIN="$SDK_ROOT/ndk/$NDK_VERSION/toolchains/llvm/prebuilt/linux-x86_64/bin"
READELF="$LLVM_BIN/llvm-readelf"
STRINGS="$LLVM_BIN/llvm-strings"
for tool in "$READELF" "$STRINGS"; do
    if [ ! -x "$tool" ]; then
        echo "RELEASE_SELFTEST_GUARD=ERROR missing $tool (NDK $NDK_VERSION)" >&2
        exit 2
    fi
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RELEASE_APK="$ROOT/app/build/outputs/apk/release/app-release-unsigned.apk"
DEBUG_APK="$ROOT/app/build/outputs/apk/debug/app-debug.apk"
LIB="libforgeshape_native.so"
ABIS="x86_64 arm64-v8a"
STRING_PATTERN='_SELFTEST_|FSR1A_|CADUXR1_|DAR1_|CADVS_|FORGESHAPE_SKETCH_TAP:'

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

failures=0
printf '| variant | ABI | packaged bytes | `SelfTests` dynamic symbols | self-test strings | verdict |\n'
printf '| --- | --- | --- | --- | --- | --- |\n'
for variant in release debug; do
    if [ "$variant" = release ]; then apk="$RELEASE_APK"; else apk="$DEBUG_APK"; fi
    if [ ! -f "$apk" ]; then
        echo "RELEASE_SELFTEST_GUARD=ERROR missing $apk" >&2
        exit 2
    fi
    for abi in $ABIS; do
        so="$WORK/$variant-$abi-$LIB"
        if ! unzip -p "$apk" "lib/$abi/$LIB" > "$so" || [ ! -s "$so" ]; then
            printf '| %s | %s | missing | - | - | FAIL |\n' "$variant" "$abi"
            failures=$((failures + 1))
            continue
        fi
        bytes=$(wc -c < "$so" | tr -d ' ')
        syms=$("$READELF" --dyn-syms --wide "$so" | grep -c 'SelfTests' || true)
        strs=$("$STRINGS" "$so" | grep -cE "$STRING_PATTERN" || true)
        if [ "$variant" = release ]; then
            if [ "$syms" -eq 0 ] && [ "$strs" -eq 0 ]; then verdict=PASS; else verdict=FAIL; fi
        else
            # The control: the same probes must find the suites where they belong.
            if [ "$syms" -gt 0 ] && [ "$strs" -gt 0 ]; then verdict=PASS; else verdict=FAIL; fi
        fi
        [ "$verdict" = PASS ] || failures=$((failures + 1))
        printf '| %s | %s | %s | %s | %s | %s |\n' "$variant" "$abi" "$bytes" "$syms" "$strs" "$verdict"
    done
done

if [ "$failures" -eq 0 ]; then
    echo "RELEASE_SELFTEST_GUARD=PASS"
else
    echo "RELEASE_SELFTEST_GUARD=FAIL ($failures row(s))"
    exit 1
fi
