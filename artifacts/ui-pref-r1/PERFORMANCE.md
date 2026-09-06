# Performance / recomposition check — UI-PREF-R1 Part O

No benchmark framework was created. What each preference change does, and
does not do, is stated from the code paths and confirmed by the tokens native
code logs on the device.

## Palette switch

- Path: `SettingsPageView` row → `EditorWorkspaceView.onPaletteChosen` →
  `AppPreferencesStore.update` (one `SharedPreferences.commit()`) →
  `ForgeShapeActivity.requestTheme` → `EditorUiState.carryAcrossRecreation`
  → `Activity.recreate()`. The new Activity calls `setTheme` and
  `NativeViewport.setViewportBackground(index)`, then builds the view tree.
- Native: `DisplaySettingsStore.setViewportBackground` is one atomic
  exchange; the render pass reads the clear colour and the grid/gizmo push
  constants per frame anyway. **No project encode/decode, no CAD
  regeneration, no Sculpt republish, no import/export, no GPU buffer
  touched.** `onDestroy` skips `NativeViewport.stop()` during a configuration
  change, so the render thread, device and every uploaded mesh survive the
  recreation (`ARCHITECTURE.md`, *Appearance*).
- Evidence: `FORGESHAPE_VIEWPORT_BACKGROUND:<name> … changed=1` is the only
  native token a switch emits; `FORGESHAPE_GIZMO_UPLOAD_OK` appears once per
  device and never per switch (`focused/*.log`, `startup.log`);
  `SettingsPreferencesTest.uiprefr1_09_10_11_37` records
  `constructionMeshRevision()` and the whole native snapshot before two palette
  switches and asserts them identical after — a regeneration or republish
  would move the revision.

## Handedness switch

- Path: row → `onHandednessChosen` → store → `applyHandedness`, which sets one
  flag on the trailing host, swaps four margins, removes and re-adds the five
  members of ONE `LinearLayout` (the middle row) and calls `requestLayout()`.
  One layout pass of the chrome; the `SurfaceView` is not measured differently
  (the row is inside `chromeRoot`, which never pads or resizes the viewport).
- Native: nothing. No JNI call is made by a handedness change.
- Evidence: `uiprefr1_16_18` — the X handle's screen pixel, the camera pose and
  the native snapshot are bit-identical across the switch; `uiprefr1_09_10_11_37`
  — the mesh revision and the bytes unchanged.

## Gizmo visual size

- Path: row → `onGizmoVisualScaleChosen` → store → `NativeViewport.setGizmoVisualScale`
  → `GizmoSession::setVisualScale` under the state mutex: one float. The
  renderer's model matrix multiplies it in per frame (`gizmoPlacementScale`).
- **No re-upload**: the canonical vertex list is unchanged, so `syncGizmoGeometry`
  finds the uploaded weight equal and returns; `FORGESHAPE_GIZMO_UPLOAD_OK` is
  not logged by a size change.

## Gizmo thickness

- Path: row → `onGizmoStrokeWeightChosen` → store → `NativeViewport.setGizmoStrokeWeight`
  → `DisplaySettingsStore.setGizmoStrokeWeight` (one atomic exchange). Before
  the next frame `syncGizmoGeometry` notices the weight differs from the
  uploaded one, waits for in-flight frames on the shared rule every mesh
  upload follows, regenerates the canonical list on the stack
  (≤ 2268 vertices, ≤ 45 KB) and copies it through the existing staging
  path into the ONE gizmo buffer, which was sized to the widest recipe at
  device creation — a copy, never a reallocation.
- Evidence: `FORGESHAPE_GIZMO_UPLOAD_OK weight=<Thin|Regular|Bold>` once per
  change and at no other time — plus, on a COLD start whose stored weight is not
  Regular, one extra upload at device creation before the stored weight is
  pushed (`RELAUNCH.md`), a few kilobytes once per process; no body's `syncScene` runs for it (the scene
  revision gate is separate and no revision moved).

## What none of the four does

Encode or decode a project, regenerate a CAD mesh, republish a Sculpt mesh,
import or export, reset renderer project resources, mint a `MeshRevision` or
`SculptRevision`, or record a history step. Proven together by
`uiprefr1_09_10_11_37` (bytes, fingerprint, dirty flag, both depths, mesh
revision, native snapshot) after every preference was changed.
