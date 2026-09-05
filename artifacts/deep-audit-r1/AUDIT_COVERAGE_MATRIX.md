# Audit coverage matrix — DEEP-AUDIT-R1

What was inspected, at what depth, and with what tool. "Read in full" = every line read; "traced" = read for the audited property and its call sites followed.

| Part | Surface | Depth | Tool / evidence |
| --- | --- | --- | --- |
| A | Repository map, dependency direction, representation-assumption searches | full | grep + read; `REPOSITORY_MAP.md` |
| B | C++ ownership, integer/FP safety, containers, error paths | production `.cpp/.h` read in full (37 374 lines); raw-pointer + `new`/`delete` + `reinterpret_cast` grep | `NATIVE_CPP_JNI.md` |
| C | Sanitizers | NOT AVAILABLE (no host build / no sanitizer config); NDK clang `-fsyntax-only` over 35 TUs | recorded |
| D | JNI matrix (155 methods) | full; mechanical Java↔C++ parity | `NATIVE_CPP_JNI.md`, `tools/jni_matrix.js` |
| E | Android lifecycle | `ForgeShapeActivity`/`SurfaceView`/autosave read in full | `ANDROID_LIFECYCLE.md` |
| — | Threading: 190 `g_stateMutex` sites, viewport handshake, MeshStore lock | traced every site | `THREADING_CONCURRENCY.md` |
| F | `.forge` codec (header, sections, CRC, all 28 fixtures, negatives, producer parity) | full read; device + PowerShell + self-test digest parity | `PERSISTENCE_DATA.md`, `CORPUS_VERIFYONLY.txt` |
| G | CAD geometry: sketch, profiles, triangulation, arc/spline, faces, TopoRef, Edit Sketch | full read of `sketch`/`cad_body`/`cad_face`/`sketch_session` | `CAD_GEOMETRY_NUMERICS.md` |
| H | Sculpt + history | full read; aggregate-memory estimate | `SCULPT_HISTORY.md` |
| I/J | GLB import/export + roundtrip + preview | full read | `IMPORT_EXPORT.md` |
| K | Renderer / GPU | `renderer.cpp` (2980) read in 3 pages; shaders read | `RENDERER_GPU.md` |
| L | Performance / memory | device self-test prints + reasoned bounds (no new instrumentation) | `PERFORMANCE_MEMORY.md` |
| M | Security, manifest, file handling, licenses | manifest via `aapt2`; `.so` NEEDED/RELRO via readelf; grep for network/exec | `SECURITY_FILE_HANDLING.md` |
| N | UI / input / accessibility | `EditorWorkspaceView` (4441) + all view classes read; ids/dimens/strings | `UI_INPUT_ACCESSIBILITY.md` |
| O | Build / release / ABI | four `.so`s built + inspected (readelf, nm, size, aapt2) | `BUILD_RELEASE_ABI.md` |
| R | Test quality + TEST_TRUST_MATRIX | inventory read; the five brief-named items revisited | `TEST_QUALITY.md` |
| S | Comment audit | inventory tool, before/after, whitelist | `COMMENT_AUDIT.md` |
| — | Markdown audit (113 files) | every root doc read; artifacts classified | `MARKDOWN_TRUTH_MATRIX.md` |
| — | Future-contract audit | every forward claim classified | `FUTURE_CONTRACT_MATRIX.md` |
| — | Dead code / TODO | method-level + release-symbol scan | `DEAD_CODE_TODO.md` |

## Tools recorded as NOT AVAILABLE (with reason)

| Tool | Reason |
| --- | --- |
| ASan / UBSan / TSan | no sanitizer build type in `CMakeLists.txt`/`build.gradle`; no host C++ compiler on the machine |
| clang-tidy / static analyzer | no configuration present; not installed |
| Frame-time / GPU-memory profiler | not built into the product; brief forbids adding a benchmarking framework |
| Fuzzing framework | brief forbids ("Do not turn this audit into a fuzzing framework project"); negatives are constructed fixtures instead |
| Real GPU device-loss | forbidden on the authoritative emulator; the debug injection seam exercises the real recovery path |
| A physical ARM64 device | not attached this session; ABI proven by build + ELF inspection, runtime proven on x86_64 emulator |

Everything else (Gradle build, NDK clang standalone runner, device launch, instrumented runner, device guards, corpus builder) was available and used.
