# D1 before the fix: reproduction on the baseline

**Result: D1 REPRODUCED on the device.** Import GLB in Sculpt changes the
active body and the scene, adds a Construction step, drops every sculpt reader
to zero, and leaves a project that **cannot be encoded**. All of it happens
while the product mode stays Sculpt.

## Run

| Item | Value |
| --- | --- |
| Commit | `6ee8557` — `origin/main` `a1b50f2` plus ONE test file (`ImportGlbSculptGuardTest`), no product change |
| Run | `CI DEVICE` `36641798987`, `workflow_dispatch`, `test_class=com.forgeshape.app.ImportGlbSculptGuardTest` |
| Startup | 23/23 self-test tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure lines (capture 3 of 3) |
| Instrumentation | run 3, failed 3, 25 s: `FAIL-CI-CLOUD-DEVICE-PRODUCT`, the expected red |
| Evidence kept | `before-run-36641798987/` — `instrumentation-raw.txt`, `summary.json`, and a logcat excerpt of the `ForgeShapeD1`, import, encode and mode lines |

## The journey each case drives

1. A Construction sphere (r = 2 m) stands at the origin as the active body A,
   with `ObjectId` 1.
2. The real `Start Sculpting` control is tapped. The mode is Sculpt, and A is
   both the active body and the sculpt target.
3. One real stroke is made through the viewport, so sculpt revision 8,
   has-edits 1 and sculpt undo 1 are all non-trivial.
4. A valid six-node GLB (`glb/construction_sentinel.glb`) is delivered through
   one of three paths.

## What changed (observed values, `D1_OBSERVED_*` log lines)

### Picker-result path (`onOpenGlbDocumentChosen`, what the Activity calls when the system picker answers)

| Field | Before | After |
| --- | --- | --- |
| product mode | 1 (Sculpt) | **1 (Sculpt)** |
| active body | 1 | **2** (the first imported body) |
| sculpt target | 1 | **0** |
| body ids | `[1]` | **`[1, 2, 3, 4, 5, 6, 7]`** |
| sculpt revision | 8 | **0** |
| sculpt has-edits | 1 | **0** |
| sculpt undo depth | 1 | **0** |
| Construction undo depth | 1 | **2**, a Construction step recorded in Sculpt |
| project fingerprint | `ff200df59bcfa549` | `8fca7db6c72043a6` |
| encoded project | `17643:eb48fc482ae3fceb` | **`null`**: `FORGESHAPE_PROJECT_ENCODE_FAIL:UnresolvedReference` |
| active visible / locked | true / false | true / false |

Native log for the same moment:
`FORGESHAPE_IMPORTED:59884 objects=6 … firstObjectId=2 activeObjectId=2`.
No refusal was logged.

### Bytes path (`applyImportedGlbBytes`), then the JNI entry (`NativeViewport.importGlbDurable`) directly

- **Bytes path.** Ids went from `[1]` to `[1, 8 … 13]`, and the active body
  from 1 to 8. The sculpt target, revision and undo all fell to 0, and
  Construction undo went from 1 to 2.
- **JNI entry.** It then answered **`0` (`Ok`)** and imported six more bodies
  (ids to 19, active 14, Construction undo 3). The mode stayed Sculpt
  throughout, and the project could not be encoded after either import.

### Chrome

`D1_OBSERVED_UI importOfferedInSculpt=true importLabelShownInSculpt=true`.
The Project surface offers `Import GLB…` while sculpting.

## Root cause (source)

Nothing between the control and the scene asks about the mode:

- **Chrome.** `ProjectActionsPopoverView` always draws the import row, and
  `EditorWorkspaceView.syncFromNative` never withdraws it.
- **Java.** `onImportGlbRequested`, `onOpenGlbDocumentChosen` and
  `applyImportedGlbBytes` have no mode check.
- **JNI.** `importGlbDurable` takes the state lock and calls
  `commitImportedGlbScene` without the `sculptSession().inSculptMode()`
  question. Every other scene-mutating entry asks it under the same lock:
  add, delete, rename, visibility, lock, duplicate, mirror, and Construction
  Undo/Redo.
- **Domain.** `commitImportedGlbScene` is platform-neutral and does not know
  the mode. It opens a `ScopedConstructionEdit` and makes the first created
  body active (`forgeshape_import_commit.cpp`). This is correct in
  Construction and is why the sculpt target moves in Sculpt.

The encode failure follows from that state. The mode is Sculpt, while the
active body is now an Imported Mesh with no Frozen Sculpt Mesh, so the
Sculpt-kind document the writer assembles is one its own validation refuses
(`FORGESHAPE_PROJECT_ENCODE_FAIL:UnresolvedReference`). The codec has several
`UnresolvedReference` rules (`forgeshape_project_document.cpp`), and which one
fires here was not isolated. The fix removes the state, so it was not pursued.
What is observed is that Save and the autosave checkpoint cannot encode the
project while the state lasts.
