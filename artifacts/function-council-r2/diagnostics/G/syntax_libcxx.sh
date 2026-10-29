#!/usr/bin/env bash
# host clang++ 18 syntax-only against the NDK's libc++ headers (a third standard library), x86_64-linux-gnu target
CPP=/home/user/Forgeshape/app/src/main/cpp
tu="$1"
out=$(clang++ -std=c++17 -fsyntax-only -nostdinc++ -isystem /root/android-sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/c++/v1 -D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS -I"$CPP" -I"$CPP/third_party/manifold/include" -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM "$tu" 2>&1)
rc=$?
echo "$(basename $tu) rc=$rc errors=$(printf '%s' "$out" | grep -c 'error:')"
[ -n "$out" ] && printf '%s\n' "$out" > "logs/libcxx_$(basename $tu).log"
exit 0
