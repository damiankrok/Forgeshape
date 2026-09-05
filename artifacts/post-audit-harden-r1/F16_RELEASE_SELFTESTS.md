# F-16 — self-tests out of the release binary

## Before

`app/src/main/cpp/CMakeLists.txt` listed all 61 translation units in one
`add_library` call. The twenty `*_selftest.cpp` files therefore compiled and
linked into every configuration; only their CALL SITES (`run*SelfTestsAndLog`
in `forgeshape_jni.cpp`) were `#ifndef NDEBUG`. The release `.so` carried the
twenty `run*SelfTests` entry points as exported dynamic symbols, the 181
check-name strings, and roughly a megabyte of dead code per ABI.

## Change (the smallest source-list split)

The one source list became two:

* `FORGESHAPE_PRODUCT_SOURCES` — the 41 product translation units. The two
  debug FIXTURE files (`forgeshape_mesh_fixtures.cpp`,
  `forgeshape_glb_import_fixture.cpp`) stay here deliberately: they are not
  self-tests, and the `#ifndef NDEBUG` debug entry points in
  `forgeshape_jni.cpp` reference them, so they must link in every
  configuration. Not touched, per "do not optimize unrelated binary size".
* `FORGESHAPE_SELFTEST_SOURCES` — the 20 `*_selftest.cpp` files, added with
  `target_sources(... PRIVATE ...)` only when `CMAKE_BUILD_TYPE` is `Debug`.

Debug is the one configuration that leaves `NDEBUG` undefined
(`CMAKE_CXX_FLAGS_RELWITHDEBINFO` = `-O2 -g -DNDEBUG`; AGP configures release
as `RelWithDebInfo`), so the suites are compiled in exactly the configuration
whose call sites can run them. No source file changed for this finding; the
`#include "*_selftest.h"` lines in `forgeshape_jni.cpp` stay (declarations
only, referenced only inside `#ifndef NDEBUG` bodies). Nothing in AGP, Gradle,
the JDK, CMake or the NDK moved.

## Proof

Per-configuration compile databases after the change:

| Configuration | `compile_commands.json` | self-test TUs | files |
| --- | --- | --- | --- |
| Debug | `app/.cxx/Debug/4i492rw6/x86_64/` | 20 | 61 |
| RelWithDebInfo (release) | `app/.cxx/RelWithDebInfo/633b3p38/x86_64/` | 0 | 41 |

Both configurations link for both ABIs (`RELEASE_BUILD.txt`: `BUILD
SUCCESSFUL`; the debug APK was built by the focused runs and the FullSharded
run). The NDK toolchain links shared libraries with `--no-undefined`, so a
product symbol that had lived in a self-test file would have failed the release
link — none did (PAH-R1-08 side: the product runtime APIs the suites use —
`canonical*FixtureSha256`, `projectFixtureSha256Hex`, `canonicalCorpusShape`,
`cad*PerformanceReport`, `sketchUxPerformanceReport` — are declared in the
self-test headers and referenced only from the guarded runners).

Symbols and sizes: `BUILD_SYMBOLS_SIZE.md` (PAH-R1-09/10/11). Debug self-tests
still run: `DEVICE_STARTUP.txt` — twenty `*_SELFTEST_OK` tokens, 2981 checks,
zero `_SELFTEST_FAIL`/`_FAIL:` lines, then `FORGESHAPE_NATIVE_VIEWPORT_OK`
(PAH-R1-08).
