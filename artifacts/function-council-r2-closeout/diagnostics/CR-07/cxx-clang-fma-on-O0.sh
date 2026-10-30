#!/bin/sh
# arm64-debug-like: product TUs at -O0 with contraction on (NDK Debug has no -O
# flag; CMake's Debug default is -g, i.e. -O0); the vendored kernel keeps -O2 as
# CMakeLists.txt:179 builds it in every configuration.
case "$*" in
  *third_party/manifold/src*) exec clang++ "$@" -mfma -ffp-contract=on ;;
  *) exec clang++ "$@" -O0 -mfma -ffp-contract=on ;;
esac
