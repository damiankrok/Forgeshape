# SUMMARY — `CAD-UX-S1`

**Baseline:** `b46a33c67c596cd2dbda532da10ef9f913c313f5` · worktree clean at
start · no remote · **no `.forge` byte, section, version, fixture or corpus
digest moved.**

---

## What the user gets

Finish a sketch and the extrusion stops being two fields in a side panel. An
**arrow** is drawn along the extrusion normal, standing on the profile's own
centroid, its shaft the depth it will cut. Beside it, anchored to that geometry,
is a compact camera-attached cluster: the **exact depth** (tap to type one), a
direct **Flip** that reverses which side the solid grows on without ever making
the depth negative, and a **New Body** badge that states what the extrusion
does. After Extrude, the manipulator goes with the session and a single
**Edit Sketch** chip stands on the committed body's own sketch — because the
sketch survived the extrusion and the UI now says so, in one tap instead of
three.

The cluster is **attached to the work, not pinned to the glass**: it shrinks as
the camera pulls back and grows as it comes in, saturating in a bounded band. It
is the first control in the product with that rule; the transform gizmo keeps
its deliberate constant screen size.

## What does not work yet, and exactly why

> **SUPERSEDED by `CAD-UX-S1-C1`.** This section records `CAD-UX-S1` as it was
> delivered. `OQ-CAD-UX-01` is now closed: `Finish Sketch` moves to a
> feature-preview view in which the extrusion axis has a real screen projection,
> and the arrow **is** draggable on the device. The sketch authoring camera was
> not unlocked — see `EVIDENCE_C1.md`.

**The arrow cannot be dragged from inside a sketch (`OQ-CAD-UX-01`).** This is
recorded as a blocker rather than worked around, and it is not a defect in the
manipulator — the drag arithmetic is implemented and proved by eight native
cases under cameras that can see the axis.

The cause is structural and was found on the device, not guessed:

* `CameraController::frameSketchView` aims the camera **exactly along the sketch
  support normal** (`forgeshape_camera.cpp:161`), and
* `CameraController::applyOrbit` **returns early while `sketchView_` is true**
  (`forgeshape_camera.cpp:350`) — *"a sketch view is not orbited: it is locked
  normal to its plane"*.

That normal **is** the extrusion axis. So from the only view a sketch ever has,
the arrow points straight at the eye, its shaft has zero screen extent, and an
axial drag has no direction to resolve against. The manipulator does the one
honest thing the repo's own rule allows — it **holds the last good value** — and
`e2eCaduxs1_03` asserts exactly that on the device, including that the projected
tip and mid-shaft label coincide to within a pixel.

Making the drag reachable means **unlocking the sketch view**, which §4.2 of the
task prompt explicitly forbade (*"nie zmieniaj globalnej kamery"*). It also has
a real consequence worth the owner's attention: an XZ sketch's normal is world
**+Y**, which the orbit cannot express without hitting its own pitch clamp — the
gimbal case `frameSketchView` exists to bypass. That is a camera decision, and
it is returned to the coordinator rather than taken here.

Everything else in the stage — the arrow as an annotation of direction and
depth, Flip, the exact numeric edit, the badge, the camera-attached scale, the
staged Cancel, the one-transaction Apply and the retained-sketch access — works
from the locked sketch view and is proved on the device.

## Two other findings, neither fixed here

**1. `SketchOverlayStyle::Dimension` renders invisible.** The renderer's overlay
switch (`forgeshape_renderer.cpp:1720-1741`) has cases for `GridMinor`,
`GridMajor`, `Axes` and `Entities` and **none for `Dimension`**, with no
`default:`. `GizmoPush push{}` is zero-initialised inside the loop and
`gizmoHighlightColor` writes only rgb, so `push.highlight[3]` — the base alpha
`gizmo.vert` multiplies by — stays `0.0f`, and the gizmo pipeline blends with
`SRC_ALPHA`. Every vertex in a `Dimension` range is therefore fully transparent.
That affects the `SKETCH-UX-R1` E line annotation and Stage 020M's
active-axis leader. **Not fixed**: it changes how two shipped features look,
which is an owner-visible change outside this stage. The extrude arrow
deliberately uses the `Entities` range instead, so it needed no renderer change
and is definitely visible.

**2. The orientation navigator's flip and roll may not move the camera.**
`beginSketchView` (`forgeshape_jni.cpp:302`) reads `sketchSession().frame()` —
the **authoring** frame — not `viewFrame()`, which is where `viewFlipped` and
`viewQuarterTurns` live. The navigator's three acts each call `beginSketchView`.
Recorded as an observation with its two file references; not investigated
further and not changed, because it is `SKETCH-UX-R1` C's business.

## The shape of the implementation

`forgeshape_cad_extrude_tool.{h,cpp}` adds exactly three things and deliberately
not a fourth:

| Adds | Does not add |
| --- | --- |
| `CadExtrudeAnchors` — where the manipulator stands, in world space, from the frame, the profile and the extrusion, with **no camera in it** | a second model of the extrusion. `SketchSession` still owns the profile, the depth and the direction and is still the only writer |
| `CadExtrudeControlScale` — the camera-attached size rule, a new function **beside** `gizmoWorldScale` rather than a change to it | any new renderer path. The arrow is one more `SketchOverlay` producer in a range the renderer already weights |
| `CadExtrudeManipulator` — the drag, on the gizmo's contract verbatim | any persisted value. Nothing here reaches a `.forge` byte, a checkpoint, the fingerprint or a history step |

A dragged depth lands through `SketchSession::setExtrude` — the one door a typed
value already used — so it passes exactly the validation a typed one does.

## Evidence

* **Native**: the sketch-UX suite grew from **52 to 92 checks**; the 40 new ones
  are `CADUXS1-02..11`. Startup: **22/22 `*_SELFTEST_OK`, 0 failures, 0 chatty**
  at a 64 MiB ring buffer.
* **Device**: `CadCanvasExtrudeTest` **OK (8 tests)**; regression
  `SketchExtrudeTest` **OK (9)**, `SketchUxTest` **OK (14)**,
  `SpatialSketchTest` **OK (8)**.
* **Persistence**: `build-forge-corpus.ps1 -VerifyOnly` — **30/30 fixtures
  `OnDisk=True`**, every digest identical to the C++ encoder's and to the
  pre-stage values. `DATA_PACKAGE_SPEC.md` untouched; no testdata byte changed.
* **Release**: both ABIs build; `selftest` dynamic symbols **0** in each, and
  the tool's own 12 symbols present in each.

## Files

`artifacts/cad-ux-s1/` — `SUMMARY.md`, `OWNER_LATER_TEST_PACK.md`,
`EVIDENCE.md`.
