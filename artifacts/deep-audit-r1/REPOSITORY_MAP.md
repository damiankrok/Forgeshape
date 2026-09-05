# Repository map — DEEP-AUDIT-R1

Baseline `be5b729c354a9876e24fbef54c630ee21fbe95d3` (clean). 1201 tracked files: 586 png, 113 md, 101 txt, 101 java, 77 xml, 63 h, 61 cpp, 36 forge, 15 ps1, 10 csv, 9 js, 4 svg, 4 glb, 3 vert, 3 frag, 3 gradle, 2 mp4, 2 json, 2 html, 1 jar, 1 bat.

## Layout

| Path | What | Lines (baseline) |
| --- | --- | ---: |
| `app/src/main/cpp/*.{cpp,h}` production (84 files, excludes `*selftest*`) | platform-neutral C++17 domain + Vulkan renderer + the one JNI file | 37 374 |
| `app/src/main/cpp/*selftest*.{cpp,h}` (40 files) | the 20 debug-only native suites | 25 535 |
| `app/src/main/cpp/shaders/*.{vert,frag}` (6) | surface, grid, gizmo/overlay shaders, compiled AOT by `glslc` | — |
| `app/src/main/java/com/forgeshape/app/*.java` (51) | the Android shell: `NativeViewport` (JNI surface), `EditorWorkspaceView` (composition), 40 view/controller classes | 18 733 |
| `app/src/androidTest/java/com/forgeshape/app/*.java` (42) | 39 instrumented classes (519 `@Test`) + 3 support classes | 29 223 |
| `app/src/test/java/com/forgeshape/app/*.java` (8) | JVM suite (70 `@Test`) | 1 151 |
| `app/src/main/res/**` (76) | values (324 strings, 218 ids, dimens, colors, styles), drawables; NO layout XML (views are built in code) | 3 583 |
| `app/src/androidTest/assets/forge/*.forge` (7) | corpus fixtures packaged for on-device load | — |
| `testdata/forge/v1/*.forge` (28) | the golden corpus written by `scripts/build-forge-corpus.ps1` | — |
| `scripts/*.ps1` (12) + `instrumented-sharding.ps1` | device runner, sharding, guards, corpus builder, evidence collectors | 3 105 |
| `artifacts/**` | per-stage evidence (md/png/txt/csv/glb/mp4), historical | — |
| root `*.md` (6) | CLAUDE, README, PROJECT_STATUS, ARCHITECTURE, PRODUCT, DATA_PACKAGE_SPEC | 11 204 |

## Native module dependency direction (headers, include graph)

```
forgeshape_math.h ─┬─ forgeshape_object_id.h ─ forgeshape_mesh.h ─┬─ forgeshape_construction.h ─┐
                   │                                              ├─ forgeshape_imported_mesh.h ─┤
forgeshape_input.h ┤  forgeshape_workplane.h ─ forgeshape_sketch.h ─ forgeshape_cad_body.h ─┬────┤
forgeshape_transform.h ─────────────────────────────────────────── forgeshape_cad_face.h ──┘    │
                                                                                                ▼
forgeshape_camera.h ─ forgeshape_picking.h ─ forgeshape_sculpt_history.h ─ forgeshape_sculpt.h ─ forgeshape_scene.h
                                                                                                │
        forgeshape_history.h ◄──────────────────────────────────────────────────────────────────┤
        forgeshape_body_delete.h, forgeshape_import_commit.h, forgeshape_project_bootstrap.h ◄──┤
        forgeshape_project_document.h + forgeshape_project_bytes.h ─ forgeshape_project_state.h ◄┤
        forgeshape_sketch_session.h, forgeshape_support_chooser.h, forgeshape_gizmo.h, forgeshape_selection.h ◄┤
        forgeshape_gltf_export.h, forgeshape_json.h ─ forgeshape_gltf_import.h ─ forgeshape_glb_roundtrip.h, forgeshape_import_preview.h
                                                                                                │
forgeshape_render_mesh.h, forgeshape_grid.h, forgeshape_matcap.h, forgeshape_display.h, forgeshape_render_recovery.h, forgeshape_selection_pulse.h
                                                                                                │
                                                          forgeshape_renderer.h (Vulkan + ANativeWindow)
                                                                                                │
                                                          forgeshape_jni.cpp  (the ONLY file with jni.h / android types besides the renderer's window seam)
```

Verified by grep: no `jni.h`, `jobject`, `JNIEnv`, `android/` include appears outside `forgeshape_jni.cpp` and `forgeshape_renderer.{h,cpp}` (`android/native_window.h`, `android/log.h`); no domain header includes the renderer. The Java layer reaches native only through `NativeViewport` (155 `static native` methods, 1:1 with the 155 `Java_com_forgeshape_app_NativeViewport_*` definitions — see `NATIVE_CPP_JNI.md`).

## Representation-assumption search (Part A)

| Pattern | Hits | Verdict |
| --- | ---: | --- |
| `isImported()` in production C++ | 3 (`forgeshape_jni.cpp:2162,2169`, `forgeshape_scene.h:119`) | accessor + two JNI queries; no binary branching in the domain |
| `sceneActiveBodyIsImported` / `sceneBodyIsImported` in Java | 5 call sites (`EditorWorkspaceView` ×3, `SculptContextView` ×2) | used for WORDING (Back to Imported Mesh / Reset Sculpt from Imported Mesh) and to withdraw *Shape*; always paired with `sceneActiveBodyIsCad()` where a third kind matters (`syncFromNative`, `showActiveInspectorBody`, `GlobalToolbarView.showContext(…, imported, cad, …)`) |
| Construction-vs-Imported ternaries excluding CAD | 3 (`GlobalToolbarView:519,527,805` label choice) | correct: a CAD body cannot be in Sculpt, so the Back-out-of-Sculpt label only ever has two destinations |
| `bodies_.front()` | 3 (`forgeshape_scene.cpp:98,107,129`) | `activeBody()` fallback when `activeBodyId_` names no body (defensive, unreachable in product: every mutation keeps the id valid, `setActiveBody` validates); line 129 picks the first body after a delete. Not a defect; recorded P3 (silent self-heal would mask a bug — a debug assert would say so) |
| `noProjectBody()` / `g_activeBodyMisuse` | counted null object | `HomeFlowTest` asserts the count stays 0 across every journey (`debugActiveBodyMisuseCount`) |
| raw `ObjectId` misuse (`kDemoCubeObjectId` / `kConstructionBoxObjectId`) | constructor defaults only (`forgeshape_construction.h` ×7, `forgeshape_mesh.h:143`, `forgeshape_jni.cpp:1565` debug stress fixture) | the real identity is minted by `ConstructionScene`; naming debt already recorded in PROJECT_STATUS |
| duplicated geometry extraction by representation | 5 sites: `forgeshape_gltf_export.cpp:213-251`, `forgeshape_glb_roundtrip.cpp:45-65`, `forgeshape_scene.cpp` `publishSceneObject` (353-374), `buildSculptSourceMesh` (403-413), `forgeshape_project_state.cpp` capture (126-149) + fingerprint (480-498) | each is a complete 3-way branch and each has suite coverage; ARCH-HEALTH-01's "third representation is the moment to unify" has passed — reported as P3 debt (`FINDINGS.md` F-13), not refactored (out of audit scope) |
| `sceneBodyRepresentation()` | 1 JNI query, used by test support only (`WorkspaceTestSupport.selectFirstConstructionBody`) | the Java product code never calls it (uses the two booleans instead) — P3 |
