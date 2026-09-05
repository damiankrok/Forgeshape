# Dead code, TODOs and unreferenced declarations — DEEP-AUDIT-R1

## TODO / FIXME / HACK

One match in the whole tree: `forgeshape_json.cpp:90` — a `// A \uXXXX escape is decoded…` comment containing the substring "XXXX", not a task marker. **There is no TODO, FIXME, HACK or XXX task marker anywhere in `app/src/main`, `androidTest`, `test` or `scripts`.**

## Unreferenced / dead declarations (kept, reported)

| Symbol | File | State | Recommendation |
| --- | --- | --- | --- |
| `EditorControlStyles.setChipReserved` + `res/drawable/bg_capsule_reserved.xml` | Java + res | dead since E2E-R1C removed the reserved Export chip (its only caller); ARCH-HEALTH-01 recorded it, left in place to avoid re-taking the aggregate | remove in the next stage that touches `EditorControlStyles`, or reuse if a reserved control is re-approved (F-18, P3) |
| `runProjectSelfTests` + 19 other `run*SelfTests` and all check-name strings | native, **release `.so`** | present in the release binary (exported dynamic symbol; `FSR1A_01`/`CADUXR1_25`/`DAR1_0*` strings in `app-release-unsigned.apk`); never called in release (call site is `#ifndef NDEBUG`) | move the `*selftest*` translation units behind a CMake condition so a release `.so` drops them (F-16, P2; ≈ 500 KB) |
| `buildSpherifiedBox` | `forgeshape_mesh_fixtures.cpp` | defined outside the anonymous namespace with no header declaration → accidental external linkage; used only within the file | move into the anonymous namespace (F-15, P3) |
| `ParsedGlbScene::totalTriangles()` | `forgeshape_gltf_import.{h,cpp}` | declared + defined, referenced twice (decl + def) — only used by the diagnostic report path; live, not dead | keep |
| `SculptHistory::recordOutcomeName` | `forgeshape_sculpt_history.{h,cpp}` | decl + def, used by the log line only | keep (diagnostic) |

Method-level scan (production C++ `Class::name` and Java `private`/package methods): no truly orphaned function found beyond the above — the low-reference-count candidates all resolve to JNI entry points (one caller by design, the JNI bridge), diagnostic reporters, or single-file helpers. Java private/package methods: none unreferenced.

## Debug-only surfaces (guarded, correct — not dead code, but not product)

`debugMeshCommand` (keyevents 1–9, A–G), `debugCameraPose`/`debugSetCameraPose`, `debugInjectDeviceLoss`, `debugRendererDeviceRebuilds`, `debugResetConstructionHistory`, `debugActiveBodyMisuseCount`, `debugLastPointerEvent`, `debugPreviewRendersBothSides`, `nomadLikeGlbFixture`, the whole `ImportedMeshPreview`. All are `#ifndef NDEBUG`-guarded at the CALL SITE and return false/no-op in release — but see F-16: their code still links into the release binary.

## Reserved / inert controls

None. `EditorControlStyles.setChipReserved` is the only reserved-styling path and it has no caller. No OBJ/FBX menu entry, no drawn-but-unimplemented tool (verified against CLAUDE.md "Nothing unimplemented is drawn as a tool").
