#!/usr/bin/env bash
# For each committed fixture: is its SHA-256 (first 58 hex chars, so a literal
# split across two C++ string pieces still matches) present in a *_selftest.cpp?
cd "$(dirname "$0")/../../../.."
n=0; m=0
for f in $(find testdata/forge -name '*.forge' | sort); do
  h=$(sha256sum "$f" | cut -c1-58); n=$((n+1))
  if grep -qi "$h" app/src/main/cpp/*selftest*.cpp; then m=$((m+1)); else echo "NO_DIGEST_LITERAL $(basename "$f")"; fi
done
echo "fixtures=$n digest_literal_in_selftest_source=$m"
