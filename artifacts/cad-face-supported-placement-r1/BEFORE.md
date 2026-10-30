# CAD-FACE-SUPPORTED-PLACEMENT-R1 — BEFORE (preflight, before any product edit)

## Refs (verified by `git fetch`, 2026-10-30)

| Ref | Full SHA |
| --- | --- |
| Base `audit/function-council-r2-closeout-r1` | `7c2fc913d8c7081ab36b2b0385904ee02d7e7245` (matches the prompt) |
| Product parent `feature/cad-r2-c2-picking-seam-r1` | `70a6c14f703bfda2ab689b855126c1d12a541e41` (matches) |
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` (matches) |
| New branch | `feature/cad-face-supported-placement-r1` (name was free locally and on origin), created from exactly `7c2fc91` |

`git status --short` was empty. `70a6c14` is an ancestor of the base, and
`git diff --name-only 70a6c14 7c2fc91 -- app scripts testdata .github` is
empty: the base product is byte-identical to C2.

## Source map (the one root cause and its two symptoms)

**Truth.** `ConstructionScene::resolveWorldModel` (`forgeshape_scene.cpp:277-326`)
is the one owner of a body's world model. For an independent body it returns
`transform().modelMatrix()` (`:293-297`). For a face-supported CAD Body
(`SceneObject::isFaceSupportedCad`, `forgeshape_scene.h:165-168`) it returns
`producerWorldModel · cadFaceFrameMatrix(face)` after a lineage check, and
**never reads the dependent's stored transform**. `snapshot()` (`:359`) feeds
that model to the renderer and to CPU picking.

**Symptom 1 — export (CR-01).** `captureGlbExportScene`
(`forgeshape_gltf_export.cpp:156-318`) splits every body's placement from
`body.transform().modelMatrix()/localMatrix()/normalMatrix()` (`:178-187`).
For a dependent that stored transform is the inert identity, so the file puts
its sketch-local geometry at the origin. JNI `exportGlb`
(`forgeshape_jni.cpp:8506-8534`) carries **no status across JNI** (it logs
`FORGESHAPE_GLB_EXPORT_FAIL:<name>` and returns null), and Java
`onExportGlbRequested` (`EditorWorkspaceView.java:5749-5770`) shows one generic
failure for every refusal. So a new appended `GlbExportStatus` value needs no
JNI or Java mapping, only `glbExportStatusName`.

**Symptom 2 — transform / gizmo (CR-02).** Every door into a dependent's
stored transform:

| Door | Code | Guard today |
| --- | --- | --- |
| Typed Apply (precision surface placement editor) | Java `ConstructionPlacementEditorView.java:269` → JNI `applyBoxTransform` (`forgeshape_jni.cpp:2538-2556`) → `applyBoxTransform(label, …)` (`:1333-1386`) → `applyConstructionTransform` (`forgeshape_transform.cpp:407`) | NoProject, Locked only |
| Gizmo activation | JNI `setGizmoActive` (`forgeshape_jni.cpp:3827-3864`) | Sculpt, Locked only |
| Gizmo drawing / hit | `GizmoSession::snapshot` (`forgeshape_gizmo.cpp:1197-1232`) pivots on the STORED position | none |
| Gizmo drag | touch path begins on `gizmo.active()`; `GizmoSession::beginDrag` (`forgeshape_gizmo.cpp:1428+`) → `applyTarget` (`:1415-1426`) writes `body->transform()` | none |
| Java offering | `EditorWorkspaceView.java:3028-3033` (gizmo `offered`), `:3046-3049` (`transformOffered`), `:3713-3715` (placement editor), `WorkspaceTrailingHostView.java:605-622` (Transform rail entry) | Locked / sketch / sculpt / Dimensions only |
| Native read-only query that exists | `sceneActiveBodyIsFaceSupportedCad` (`forgeshape_jni.cpp:7676-7680`), used today only by androidTests | — |

Not doors (already refused for a CAD Body or the act copies, not moves):
Body Dimensions / Relative Scale refuse a CAD Body (`BodySizeStatus`,
`APPLY_REJECTED_UNAVAILABLE`); Mirror refuses CAD (`RefusedNotMirrorable`);
Duplicate refuses a dependent (`RefusedFaceSupportedCad`). Undo/Redo restore
whatever a step recorded (`forgeshape_history.cpp:348`); load restores the
stored transform (`forgeshape_project_state.cpp:339`) — OD-16 untouched.

**Status codes.** Java `APPLY_*` codes 0–10 are taken (`NativeViewport.java:148-376`;
7 = CAD, 8 = LOCKED, 9 = DEGENERATE_AXIS, 10 = UNAVAILABLE). A new refusal
code must be appended (11).

## Test coverage before this stage, and why it misses both symptoms

- `GLTF_EXPORT` (93/128): `forgeshape_gltf_export_selftest.cpp` has no
  `addCadBody`; `CADR0_37` exports only a world-plane CAD body.
- `GlbExportTest` (24/24 green in `CI DEVICE 40979514270`): no CAD Body, no
  face support.
- `GIZMO` (176/256) and `EditorWorkspaceGizmoTest` (46/46 green in `40983965648`):
  no face-supported body.
- `CAD_A3` (66/128): tests follow-the-producer and Duplicate's refusal, never a
  transform write on the dependent.
- Suites that must NOT gain checks: `PROJECT` is at its 256/256 device
  recorder cap (closeout CR-07).

## Baseline numbers (closeout, same product tree)

Host `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites,
`CAD_R2_ORACLE_PASS=17`; JVM 196/196; corpus 72/72; release guard PASS.
`CI DEVICE 40979514270` 63/63. `CI DEVICE 40983965648` **72/73 — a FAILURE**,
the one failure being the stale `EditorWorkspaceMobileTest#uir4a04`
(`expected:<8> but was:<12>`).

## Out of scope (stated before any edit)

Recover / Save flow / Activity recreation (OD-05/06), Sculpt NotRetained
(OD-22), import during a sketch or the chooser, navigator orientation, decimal
comma, System Back, 48 dp remediation, second root sketches / `CADB` v9
(OD-01), an "offset from face" placement concept, Lock on a dependent (OD-29),
any change to a stored non-identity dependent transform or its decoding
(OD-16), `resolveWorldModel` / face identity / lineage / topo tokens, `.forge`,
`CADB`, fixtures, `DATA_PACKAGE_SPEC.md`, renderer, kernel, desktop, new
libraries, feature flags. The full Master Plan v4.1.61 is still absent; no
plan-compliance claim is made.
