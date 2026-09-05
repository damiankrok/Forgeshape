# Import / export — DEEP-AUDIT-R1 (Parts I, J)

## What was read

`forgeshape_gltf_export.{h,cpp}`, `forgeshape_json.{h,cpp}`, `forgeshape_gltf_import.{h,cpp}`, `forgeshape_glb_roundtrip.{h,cpp}`, `forgeshape_import_commit.{h,cpp}`, `forgeshape_import_preview.{h,cpp}`, `forgeshape_imported_mesh.{h,cpp}`, `forgeshape_glb_import_fixture.cpp`, the JNI import/export entries, `forgeshape_gltf_export_selftest.cpp` (93), `forgeshape_gltf_import_selftest.cpp` (189), the instrumented `GlbExportTest`, `GlbImportPreviewTest`, `GlbImportExternalR1Test`, `ImportedMeshDurableTest`, `ImportedMeshSculptTest`.

## Export (confirmed)

`exportSceneAsGlb` bakes `L = Rz·Ry·Rx·S` into each body's vertices (`position = L·p`, `normal = normalize(transpose(inverse(L))·n)`), leaves `T` as a translation-only node, writes no rotation/scale/matrix. Refuses `SingularTransform`/`MirroredTransform` (`det(L) ≤ 0`), never compensates by reversing triangles. No axis conversion, no scale factor, no merge, no second file. Exports EVERY body's effective geometry (Frozen Sculpt Mesh if present, else imported arrays, else regenerated Construction/CAD mesh). Read-only: no revision, no history, neither slot moves. The `.forge` still stores the nine authored values. Reads only — `runGlbRoundtripDiagnostic` re-derives the expected side from domain truth (not the exporter's own arrays) and the R0 roundtrip is `ROUNDTRIP_EQUIVALENT` at `max_position_delta_m=0`.

## Import (confirmed)

`importGlb` reads a bounded STATIC subset with `forgeshape_json` (depth 15, 1M values). Fails closed by name on a required extension, sparse accessor, interleaved view, external buffer, animation, skinning, morph targets, non-triangle mode, a node with children, a node stating both `matrix` and TRS, and any unknown attribute. `commitImportedGlbScene` is atomic (build+validate off the scene, then one `ScopedConstructionEdit`), splits the node transform (linear part baked, translation → placement), refuses `RefusedEditInProgress`. Every accessor read is bounds-checked. The diagnostic `ImportedMeshPreview` is session-only, keyed from `1<<60`, and has no user-facing control since IMPORT-01A. The `nomadLikeGlbFixture` is a synthetic structural stand-in, deterministic (integer/power-of-two coordinates), never described as an owner asset.

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| F-03 | P1 | `sceneBodyName`, `glbRoundtripReport`, `glbCompareReport` handed `NewStringUTF` a name that may carry a 4-byte UTF-8 sequence (a legal sanitized name, e.g. an emoji from another tool). Illegal Modified UTF-8 → CheckJNI aborts a debuggable process on GLB import of such a file. | FIXED (UTF-16 via `newJavaString`/`utf8ToUtf16`) + `DAR1_03` |
| — | OK | `importGlbDurable` refuses only `editInProgress`, not Sculpt mode. Verified the Java product path never offers Import while sculpting (`ProjectActionsPopoverView`/`syncFromNative`): the project surface's Import row is reachable in Construction, and a durable import while a sculpt mesh is active would simply add a body — no corruption. No fix needed. |
| — | OK | `sanitizeImportedMeshName` is idempotent, drops malformed bytes, cuts on a UTF-8 boundary; `importedMeshNameIsStorable` is defined as "== sanitize(self)", so the decoder cannot admit a name the importer could not have made. Read line by line; `DAR1_03` pins the 4-byte case. |
| — | OK | Roundtrip diagnostic's expected side comes from domain truth, not the exporter — a captured-wrong-geometry bug would still fail. Correct by construction. |
| — | OK | The five representation branches in export + roundtrip are complete (Construction/Imported/CAD, else refuse); part of F-13 unification debt, not a defect. |
