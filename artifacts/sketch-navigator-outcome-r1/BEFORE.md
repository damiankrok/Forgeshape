# SKETCH-NAVIGATOR-OUTCOME-R1 — BEFORE (written before any product edit)

NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM. The canonical v4.1.61 and the
current OWNER_DECISIONS_* are absent.

## Refs

Checked 2026-11-02 by `git fetch`.

| Ref | SHA |
| --- | --- |
| Base `feature/import-glb-sketch-guard-r1` | `8a797ff0f57af0b01caf5cc7608c09209273a71a` (matches) |
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` |
| New branch `feature/sketch-navigator-outcome-r1` | created from exactly `8a797ff`. No `feature/sketch-navigator-*` existed on origin. `8eb94ce` is an ancestor. Tree clean. |

## Sources and their authority

| Source | Level | What it says |
| --- | --- | --- |
| `artifacts/function-council-r2-closeout/sources/FORGESHAPE_MASTER_PLAN_v4.1.0.md:319` | Historical OWNER record (authentic archive copy, SHA-256 verified in the closeout) | `UI-OWNER-48` "immediate flat Sketch + orientation navigator + on-canvas line length", APPROVED, IMPLEMENTED_OWNER_ACCEPTED (`5cf981a`). `UI-OWNER-46` (spatial support selection, OWNER real-device retest pending), `UI-OWNER-47`, `ARCH-OWNER-14` are at `:317-333`. |
| `…/FORGESHAPE_MASTER_PLAN_v4.0.58.md`, `…v4.1.58.md`, addenda v4.1.62–66, PROJECT_STATE v4.1.66 | Historical | v4.0.58 lists UI-OWNER-01…46 only. v4.1.58 and the addenda do not mention `UI-OWNER-48` or the navigator. **No source contradicts v4.1.0's status.** |
| `CLAUDE.md:343-356` (SKETCH-UX-R1 C) | Current repo rule | "The orientation navigator … owns both … `SketchSession::viewFrame` derives the camera's frame from the authoring frame and is ALWAYS right-handed, so no view state can mirror a sketch." Also: plane change only while empty and world-supported (`SketchNotEmpty`); a face sketch refuses a world plane (`InvalidWorkplane`). |
| `CLAUDE.md` (CAD-UX-S1-C1) | Current repo rule | Every path back to the authored sketch (`backToEditing`, Edit Sketch) reinstalls the exact aligned view, and `endSketchView` returns the pre-sketch view on cancel and commit. |
| `PRODUCT.md:153-160` | Current repo claim | "one control flips to look at the plane from the other side; and −90° and +90° turn the drawing on screen." **Not true on this baseline** (RED below). |
| `artifacts/function-council-r2-closeout/CRITICAL_REPROS.md` CR-06-S3 (`:94`, `:754+`) | Audit evidence | E1: Flip and ±90 do not move the sketch camera (`camera_view_identical_to_B0=1`). |
| `DECISION_GATES.md` row 9 (`:69`) and OD-09 (`:172`) | Audit gate | "**none** for the reset-on-reentry form; **OD-09** only if the user's flip/roll should survive Back to Sketch / Edit Sketch". |

What cannot be confirmed without v4.1.61 and the current decisions: whether
any later OWNER decision changed `UI-OWNER-48`'s navigator scope, and OD-09's
answer. This stage takes no position on either beyond the reset-on-re-entry
form, which the gate says needs no OWNER decision.

## The real path (lines at `8a797ff`)

**Gesture to state.** `SketchOrientationNavigatorView`:
- `sketch_navigator_rotate_ccw` sends −1 and `sketch_navigator_rotate_cw`
  sends +1;
- `sketch_navigator_flip` toggles;
- the plane chips `sketch_navigator_plane_{xy,xz,yz}` choose the plane;
- Flip is disabled on a face (`:199-201`).

`EditorWorkspaceView.java:4081/4087` forward to JNI `sketchSetViewFlipped`,
`sketchRotateView` and `sketchSetSupportPlane` (`forgeshape_jni.cpp:6638-6696`).
Those update `SketchSession`, which holds `viewFlipped_` and
`viewQuarterTurns_`, and then call `beginSketchView()`.

**State to camera.** `beginSketchView()` (`forgeshape_jni.cpp:313-320`) does:

```cpp
const SketchFrame& f = sketchSession().frame();
g_camera.frameSketchView(f.origin, f.u, f.v, f.n);
```

It frames the camera on the AUTHORING frame. `SketchSession::viewFrame()`
(`forgeshape_sketch_session.cpp:648-668`) applies the flip and the roll, and
has **no production caller**. So the navigator changes its state and its label,
and the camera never moves.

**Camera to render and pick.** `CameraController::snapshot()`
(`forgeshape_camera.cpp:461-462`) is the eye on `target + svN·d` with up
`svV`, so the camera honours whatever frame it is given. Picking
(`screenToSketch`, `:893-915`) intersects the ray with the AUTHORING plane
(two-sided) and reads (u, v) off the authoring axes. It is independent of the
view frame, so it stays correct from either side.

**Way back.**
- `endSketchView()` restores the pose saved at the first `beginSketchView`, on
  cancel and on commit.
- `begin()` and `cancel()` reset flip and roll (`:225-226`, `:270-271`). Every
  commit and every Edit Sketch start passes through them.
- `backToEditing()` (`:2275`) KEEPS flip and roll, while JNI re-frames on
  `frame()`. Today that is invisible, because the camera ignores the state.

## RED on this baseline

**Host (E1).** The closeout CR-06 native diagnostic, rebuilt unchanged:
- The `beginSketchView` body it copies is byte-identical between `70a6c14`
  and `8a797ff` (`diff` of the extracted function is empty).
- `HOST_SUITE_OK SKETCH_UX (118 checks, 0 failed)` objects; link exit 0, run
  exit 0. Output: `evidence/red/cr06_native_on_8a797ff.out`.

| Act | `camera_view_identical_to_B0` | The view matrix framed on `viewFrame()` |
| --- | --- | --- |
| B1 Flip | 1 | differs (`control_differs_from_B0=1`) |
| B2 +90 | 1 | differs |
| B3 −90 (3 turns) | 1 | differs |

**Device.** `SketchNavigatorCameraTest` (new) is committed before any product
edit and run on `CI DEVICE` against this product. The run id and results are in
`RED.md` and `TESTS.md`.
- Every navigator act is a real tap through the window.
- Every view assertion reads where the asymmetric sketch points U=(1,0) and
  V=(0,1) land on screen through the live camera.

## Invariants the fix must keep

- Authoring frame, entity ids, (u, v), dimensions, profiles, support /
  `TopoRef` / lineage.
- Scene, Undo/Redo, fingerprint, autosave and every `.forge` / `CADB` byte.
- The pre-sketch camera on cancel and commit.
- Picking agrees with what is drawn.
- Import refusals 1005 / 1006 and every earlier stage's guards.

## Out of scope (stated before the edit)

- OD-09 option (b): the flip / roll surviving re-entry.
- 48 dp of `sketch_navigator_flip`: 36.19 dp measured historically, CR-06-S6
  F-01. Reported, not changed.
- Decimal comma, System Back, A2 / `CADB` v9, the chooser-only import,
  Recover / Save, Sculpt NotRetained.
- Renderer / kernel, formats, `.gitattributes`, CI, build.
