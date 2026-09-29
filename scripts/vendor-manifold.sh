#!/usr/bin/env bash
# Reproduces app/src/main/cpp/third_party/manifold/ byte for byte from the
# pinned upstream source release (`CAD-VERTICAL-SLICE-R1`, GATE-KERNEL).
#
# The kernel is Manifold (https://github.com/elalish/manifold), Apache-2.0,
# version 3.5.4, taken from the source distribution its maintainers publish to
# PyPI as `manifold3d` -- the same C++ tree the upstream tag builds from, with a
# SHA-256 that pins it. Nothing in the product fetches anything at build or run
# time: the vendored copy is committed, and this script exists only so the copy
# can be regenerated and audited.
#
# What is taken: the core library (include/manifold/*.h except the optional
# 2D cross-section API, and src/*.{h,cpp}), the LICENSE and the AUTHORS file.
# What is left out: the optional Clipper2-backed cross-section module, the
# language bindings, the build scripts and the test suite. No vendored file is
# modified; ForgeShape's own flags live in app/src/main/cpp/CMakeLists.txt.
#
# Usage: scripts/vendor-manifold.sh [--verify]
#   --verify   regenerate into a scratch directory and compare with the
#              committed copy instead of overwriting it.
set -euo pipefail

VERSION=3.5.4
SDIST_URL="https://files.pythonhosted.org/packages/f3/0f/1f7e51a8f9a6f25e837159c5bf7c95a3ae948aa12e1c5fdeee0fd90ca5f9/manifold3d-${VERSION}.tar.gz"
SDIST_SHA256=5bd88c482c6fd7fc01aa7b715b3e31c2585ee98343d5559d286ba30db0d0b6f0

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/app/src/main/cpp/third_party/manifold"
# The Apache-2.0 text the APK ships with the object form (Apache-2.0 §4).
LICENSE_ASSET="$ROOT/app/src/main/assets/licenses/manifold-${VERSION}-LICENSE.txt"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

curl -sSfL -o "$WORK/sdist.tar.gz" "$SDIST_URL"
echo "$SDIST_SHA256  $WORK/sdist.tar.gz" | sha256sum -c -
tar -xzf "$WORK/sdist.tar.gz" -C "$WORK"
SRC="$WORK/manifold3d-${VERSION}"

OUT="$WORK/out"
mkdir -p "$OUT/include/manifold" "$OUT/src"
cp "$SRC/LICENSE" "$SRC/AUTHORS" "$OUT/"
for h in "$SRC"/include/manifold/*.h; do
    [ "$(basename "$h")" = "cross_section.h" ] && continue
    cp "$h" "$OUT/include/manifold/"
done
cp "$SRC"/src/*.h "$SRC"/src/*.cpp "$OUT/src/"

if [ "${1:-}" = "--verify" ]; then
    # FORGESHAPE_VENDOR.md is ForgeShape's own provenance note, not upstream.
    diff -r -x FORGESHAPE_VENDOR.md "$OUT" "$DEST"
    cmp "$OUT/LICENSE" "$LICENSE_ASSET"
    echo "VENDOR_MANIFOLD_VERIFY_OK ${VERSION}"
else
    mkdir -p "$DEST"
    find "$DEST" -mindepth 1 ! -name FORGESHAPE_VENDOR.md -delete
    cp -r "$OUT"/. "$DEST"/
    mkdir -p "$(dirname "$LICENSE_ASSET")"
    cp "$OUT/LICENSE" "$LICENSE_ASSET"
    echo "VENDOR_MANIFOLD_WRITTEN ${VERSION}"
fi
