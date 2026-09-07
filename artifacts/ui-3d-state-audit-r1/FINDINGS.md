# UI-3D-STATE-AUDIT-R1 — Findings

**Start HEAD:** `dfcab1afd1361d37b6dfe4607772a5597f31d043`
**Device:** `ForgeShape_Stage006` / `emulator-5580`, 1080 × 2400, density 2.625
**No product code was changed by this audit.** Every finding below is recorded,
not repaired.

Findings are grouped by ROOT CAUSE, as §6 requires. Two root-cause families
account for every product finding; one fix per family closes every symptom
under it.

---

## Root-cause family R1 — the world-anchored overlay is placed in a padded container

### UI3D-F-001 — every world-/feature-anchored viewport surface stands one system-bar inset below its anchor

| | |
| --- | --- |
| **Severity** | **P1** |
| **Root-cause family** | `ANDROID_LAYOUT` |
| **Affected surfaces** | S19 dimension labels, S20 dimension editor, S31 sketch line dimension label, S35 canvas extrude cluster, S36 Side B value, S37 distance editors, S38 retained `Edit Sketch` chip — **every** anchored chrome surface in the product |
| **Layer** | Android-shell-only. The domain, the projection and the renderer are correct. |
| **Reproduces after a clean app restart** | **Yes** — both instrumentation runs are separate processes and both measure it |

**Action sequence.** Any state in which one of those surfaces is drawn. The
shortest: select a Construction Body, Transform, *Dimensions*, look at where the
three numbers stand relative to the leaders the renderer draws.

**Expected contract.** `UI-OWNER-50`: a world-anchored surface must remain
attached to the intended semantic geometry. The intended attachment point is the
surface's centre — `placeAt` in all three placing views subtracts half the
measured box from the anchor — and there is no authored offset policy for any of
them.

**Actual result.** The surface's centre stands **+128 px in Y and +0 px in X**
from the anchor native reports, on every measurement where the anchor was fresh.
**30 of the 60 measured spatial rows carry exactly this and nothing else**
(|dy − 128| < 1 px and |dx| < 1 px); the median error over all measured rows is
**48.90 dp**, which is 128 px at this device's density.

**Cause (verified in-session).** The placing views are children of
`overlayRoot`, and `EditorWorkspaceView.applyChromeInsets` gives that container
the chrome's window-inset padding:

```
app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java:1090
    overlayRoot.setPadding(padding.left, padding.top, padding.right, padding.bottom);
```

A `FrameLayout` lays its children out at `(paddingLeft, paddingTop)`, so a
`setTranslationY(y)` computed from a **viewport-pixel** anchor is drawn at
`y + paddingTop` in the window. The audit read that padding back on the device
at every measurement: `overlayPadding=0,128,0,63` (see `NOTES.tsv`), and 128 is
exactly the measured offset. The X error is 0 only because this window has no
left inset; a window with a left cutout or a landscape navigation bar would
carry the same defect horizontally.

The anchors themselves are correct: they are the same viewport pixels
`SketchTestSupport` sends synthetic touches to, and those touches land on the
geometry they name.

**Evidence.** `SPATIAL_ATTACHMENTS.tsv` (every row), `NOTES.tsv`
(`overlayPadding`), and the mechanical overlays in
`screenshots/ui3d10_01_committed.png` (green ring = the anchor on the body's
retained sketch, red cross = where the `Edit Sketch` chip actually attached) and
`screenshots/ui3d07_01_line_selected.png`.

---

### UI3D-F-006 — the extrude cluster's edge clamp is computed against the padded container

| | |
| --- | --- |
| **Severity** | **P3** |
| **Root-cause family** | `ANDROID_LAYOUT` (a consequence of UI3D-F-001) |
| **Affected surfaces** | S35, and in principle S19/S31 whenever their box is near an edge |
| **Layer** | Android-shell-only |

`CadExtrudeCanvasView.placeAt` clamps into `getWidth()/getHeight()` of the
canvas view, which is the **padded** content box rather than the viewport. The
cluster therefore reaches its clamp earlier than the window requires and is
pushed further from its anchor: 10 of its measurements are `CLAMPED`, with
errors up to **105.15 dp**. This is not a second defect so much as the first one
compounding; it is recorded separately only because a fix for F-001 that moved
the padding without revisiting the clamp bound would leave it behind.

**Evidence.** `SPATIAL_ATTACHMENTS.tsv`, rows with verdict `CLAMPED`.

---

## Root-cause family R2 — world-anchored chrome has no refresh driver tied to the frame

The domain state these surfaces mirror is decided **on the render thread, every
frame** (`forgeshape_jni.cpp:1635-1657` builds the dimension overlay and closes
the session the moment its body stops being measurable;
`bodyDimensionLabelPoint` answers only once `anchors.valid` is set there). The
shell reads that state **once per chrome act** — there are exactly three refresh
drivers, and only one of them fires for a viewport gesture, refreshing only the
CAD extrude canvas
(`EditorWorkspaceView.java:465-476`, `:493-529`, `:3180-3203`).

Four distinct symptoms follow, and one frame-driven refresh for world-anchored
chrome closes all four.

### UI3D-F-002 — the Construction dimension labels are never re-placed for camera motion

| | |
| --- | --- |
| **Severity** | **P1** |
| **Root-cause family** | `CAMERA_PROJECTION` (missing refresh driver) |
| **Affected surfaces** | S19, S20 |
| **Layer** | Android-shell-only |
| **Reproduces after a clean app restart** | **Yes** |

**Action sequence.** Construction Body selected → Transform → *Dimensions* →
(one chrome act, so the labels appear — see UI3D-F-003) → one-finger drag in the
viewport to orbit, or two fingers to pan, or a pinch to zoom.

**Expected contract.** `UI-OWNER-50`: correctly attached while the camera or
viewport orbits, pans or zooms, using the current semantic state rather than
stale cached screen coordinates.

**Actual result.** The labels stay where they were. Measured against the anchor
read back **after** the gesture:

| action | X label error | Y label error | Z label error |
| --- | --- | --- | --- |
| after_pan | 42.71 dp | 44.19 dp | 43.18 dp |
| after_zoom_out | 70.78 dp | 98.39 dp | **119.32 dp** |
| after_zoom_in | 34.21 dp | 49.30 dp | 39.01 dp |
| after_scale (UI3D-04, body resized under the labels) | 77.08 dp | 93.04 dp | 89.56 dp |

An unrelated later chrome act repairs the placement completely: every
`*_then_chrome_sync` row falls back to exactly the constant UI3D-F-001 offset
(48.6–48.9 dp) and nothing more. That is what makes this a missing refresh
driver rather than a wrong projection.

**Cause (verified in-session).** `onViewportGestureMoved` refreshes
`cadExtrudeCanvas` and nothing else, deliberately. `onViewportGestureSettled`
calls `onNativeStateChanged()` only when the sketch state, the active
`ObjectId`, the sculpt undo depth or the gizmo committed-drag count changed — a
camera change is none of those, and the comment there says so
("an orbit changed nothing").

**Evidence.** `screenshots/ui3d05_05_after_zoom_out.png` — the body is a small
sliver near the centre while `2 m`, `1 m` and `0.5 m` sit far below and to the
left of it, with the mechanical overlay joining the anchor to the chip.

### UI3D-F-003 — the dimension labels are absent on the frame the mode opens

| | |
| --- | --- |
| **Severity** | **P2** |
| **Root-cause family** | `STATE_VISIBILITY` (same missing refresh driver) |
| **Affected surfaces** | S19, S20 |
| **Layer** | shell + neutral-domain boundary (the anchor is only valid after a frame) |
| **Reproduces after a clean app restart** | **Yes**, in 4 of the 5 places the audit opens the mode; the fifth passed in run 1 and failed in run 2, so it is a race rather than a constant |

**Action sequence.** Transform → *Dimensions*. Nothing else.

**Expected contract.** C4 Stage 020M: the mode's numbers are its whole point.

**Actual result.** All three labels are `GONE` although the mode is open, the
body is measurable and the anchor projects. `NOTES.tsv` records the state at
that instant:

```
UI3D-04  dimensions_open: modeActive=1.0 measurable=1.0 anchorProjectsXYZ=111
         container=VISIBLE containerShown=true labelX=GONE overlayPadding=0,128,0,63
```

They appear on the next unrelated chrome act (a selection change, an Apply, a
panel opening).

**Cause (verified in-session).** `bodyDimensionLabelPoint` returns false until
`BodyDimensionSession::labelAnchors().valid` is set, which happens when the
**render thread** builds the overlay (`forgeshape_jni.cpp:1646`). The shell's
one post-open `syncFromNative()` runs before that frame, reads false for all
three axes, sets each chip `GONE`, and never reads again. The JNI comment
already names this case: *"False when the mode is closed, the overlay has not
been built or the anchor does not project."*

**Evidence.** `VISIBILITY.tsv` (`UI3D-04`, `UI3D-05`, `UI3D-06`, `UI3D-13` rows
for `dimensions_open`), `NOTES.tsv`, `screenshots/ui3d04_01_dimensions_open.png`.

### UI3D-F-004 — a dimension label survives as ghost UI into Sculpt and over a hidden body

| | |
| --- | --- |
| **Severity** | **P1** |
| **Root-cause family** | `STATE_VISIBILITY` / `CROSS_REPRESENTATION_LEAK` (same missing refresh driver) |
| **Affected surfaces** | S19, S20 |
| **Layer** | Android-shell-only |
| **Reproduces after a clean app restart** | **Yes** for the Sculpt case (both runs); the hidden-body case appeared in run 2 |

**Action sequence A.** Construction Body → Transform → *Dimensions* → **Start
Sculpting**.
**Action sequence B.** Construction Body → Transform → *Dimensions* → hide the
active body from the Objects row.

**Expected contract.** `UI-OWNER-50`: a contextual surface must disappear when
its owning context is switched away from, and must not survive as ghost UI from
a previous mode. C1: Dimensions is refused in Sculpt and over a hidden body —
`setBodyDimensionsMode` refuses to OPEN in both.

**Actual result.** The label chips are still drawn, over the sculpt session and
over a body that is not rendered. The divergence is visible in one frame: the
**renderer** has correctly dropped the leaders (its session is closed), while
the **shell** still draws all three numbers — one of them squarely on top of the
sphere the user is now sculpting
(`screenshots/ui3d11_01_sculpt.png`). Native agrees the mode is shut:

```
UI3D-11  sculpt_entered: modeActive=0.0 measurable=1.0 anchorProjectsXYZ=000
         container=VISIBLE containerShown=true labelX=VISIBLE
```

`modeActive=0.0` is native saying the session is closed while the chrome is
still showing it.

**Cause (verified in-session).** The session closes **itself on the render
thread** the moment its body stops being measurable
(`forgeshape_jni.cpp:1650-1656`, whose comment states the design: *"the shell's
next refresh simply finds the mode shut"*). But the shell's next refresh is the
one it already ran, inside `onFreezeToSculpt` / the visibility command, before
that frame — and nothing refreshes afterwards. The design assumes a later
refresh that no driver guarantees.

### UI3D-F-007 — after switching the active body, the labels stand at the PREVIOUS body's anchor

| | |
| --- | --- |
| **Severity** | **P1** |
| **Root-cause family** | `STALE_OWNER_OBJECT` (same missing refresh driver) |
| **Affected surfaces** | S19, S20 |
| **Layer** | Android-shell-only |
| **Reproduces after a clean app restart** | **Yes** (both runs) |

**Action sequence.** Two Construction Bodies, A and B, standing apart. Select B →
Transform → *Dimensions* → select A from the Objects list.

**Expected contract.** `UI-OWNER-50`, verbatim: a contextual surface must not
survive as ghost UI from a previous object/body, and must use the current
semantic state rather than stale cached screen coordinates.

**Actual result.** The three numbers are drawn at **body B's** dimension-line
midpoints while the mode now measures body A. This is the **largest measured
error in the audit**:

| label | error |
| --- | --- |
| X | 555.49 px = **211.61 dp** |
| Y | 737.40 px = **280.91 dp** |
| Z | 741.65 px = **282.53 dp** |

**Cause (verified in-session).** `sceneSelectBody` is followed by
`onNativeStateChanged()`, and that refresh reads `bodyDimensionLabelPoint`
before the render thread has rebuilt the overlay for the new body — so
`BodyDimensionSession::labelAnchors()` still holds the previous body's anchors
and reports them as valid. The labels are placed from them, and nothing reads
again. It is UI3D-F-003's race with the opposite outcome: there the anchors were
not yet valid and the labels vanished; here they are valid but belong to the
wrong body.

**Evidence.** `SPATIAL_ATTACHMENTS.tsv` rows `UI3D-06 / switched_to_body_A`,
`screenshots/ui3d06_02_body_a.png`.

---

## Adjudicated existing debt (provenance preserved, not newly discovered)

### UI3D-F-005 — `SketchOverlayStyle::Dimension` renders fully transparent

| | |
| --- | --- |
| **Severity** | **P2** |
| **Root-cause family** | `OVERLAY_RENDER_STYLE` |
| **Affected surfaces** | S32 (the sketch line annotation, entirely) and S21 (the ACTIVE-axis dimension leader only) |
| **Layer** | renderer-only |
| **Provenance** | **Already recorded**, unfixed, in `PROJECT_STATUS.md` → *Known Issues*, found beside `CAD-UX-S1`. This audit **adjudicates it at runtime**; it does not claim to have found it. |

**What this audit adds.** The existing record is a source argument. The audit
confirms all four links and then shows the consequence in a frame:

1. **Producers emit it.** `forgeshape_sketch_session.cpp:1606` styles the whole
   line annotation `Dimension`; `forgeshape_body_dimension_overlay.cpp:157`
   styles the **active-axis** range `Dimension` while the other two axes stay
   `Entities`.
2. **The renderer has no mapping.** `forgeshape_renderer.cpp:1720-1739` has
   cases for `GridMinor`, `GridMajor`, `Axes` and `Entities`, and **no
   `Dimension` case and no `default:`**.
3. **The alpha is therefore zero.** `GizmoPush push{}` is zero-initialised
   inside the loop and `gizmoHighlightColor` writes rgb only
   (`forgeshape_gizmo.cpp:1118-1126`), so `push.highlight[3]` stays `0.0f`.
4. **The shader multiplies by it.** `app/src/main/cpp/shaders/gizmo.vert:81`:
   `fragColor = vec4(rgb, pc.highlight.w * weight)`.

**Runtime adjudication.** `screenshots/ui3d07_01_line_selected.png`: a straight
Line is selected, native reports its dimension (`sketchLineDimension` → true,
length 1.6 m), the numeric chip `1.6 m` is drawn — and there are **no extension
lines, no dimension line and no end ticks anywhere in the frame**. By contrast
`screenshots/ui3d05_05_after_zoom_out.png` shows the Stage 020M leaders clearly,
because no axis is active there and all three ranges take the `Entities` style;
the invisibility bites only on the axis the user is editing.

**Not fixed here**, per §3 and per the existing record: the four-line repair
changes how two shipped features look, which is the owner's call.

---

## Corrections to this audit's own matrix (not product findings)

Recorded for honesty, because run 1 produced rows that looked like defects and
were not.

| what looked wrong in run 1 | what it actually was |
| --- | --- |
| `S28 sketch_editor` reported HIDDEN in three states with `MUST_SHOW` | `sketch_editor`, `cad_editor`, `sculpt_mesh_summary`/`clear_mask` and `relative_scale_editor` are **bodies of the one precision surface** (`inspector.setBody`), not surfaces of their own. Asking whether one is on screen while the panel is closed asks the wrong question. Run 2 opens the panel from its own toggle and then asks which body is in it; all such rows PASS. |
| `S41 clear_mask` reported HIDDEN after painting a mask | The same. With the precision surface open, Clear Mask is present exactly when `sculptCanClearMask()` says it can succeed. |

---

## Explicitly verified as CORRECT (no finding)

These were measured and are recorded here so the audit is not read as a list of
only the things that failed.

- **The CAD staged extrusion tracks its geometry through everything.** S35, S36
  and S38 carry the constant UI3D-F-001 offset and **nothing else** through a
  real arrow drag, an orbit, a zoom out and back, a body Move, Rotate, Scale and
  a real gizmo drag: 7/7 `Edit Sketch` chip rows and 4/4 Side B rows are within
  49.0 dp, i.e. the inset alone. The `onViewportGestureMoved` driver that only
  this surface has is exactly why.
- **Side A and Side B stay on their own sides.** Dragging each in turn moves
  only its own distance (`negative` 1.51 → 2.12 while `positive` is untouched),
  and each value stays at its own anchor.
- **The camera-attached scale behaves as `CAD-UX-S1` states**: 0.9505 near,
  saturating at exactly **0.800** when the camera pulls back — the authored
  floor, and the arithmetic that keeps the smallest live control at 48 dp.
- **Every extent-mode transition withdraws what it should.** Flip is absent in
  Symmetric and Two Sides; the Side B value is absent in One Side and Symmetric;
  no stale second value or second editor survives a mode change. 0 failures.
- **Sculpt leaks nothing else in.** The gizmo, the transform selectors, Add
  Primitive, the extrude cluster and the `Edit Sketch` chip are all absent in
  Sculpt, and Back → Resume resurrects none of them. The only leak is
  UI3D-F-004.
- **One primary surface at a time holds**, System Back closes the topmost, and
  Relative Scale closes Dimensions — all as contracted.
- **The sketch line dimension label tracks a pan correctly** (48.90 dp, i.e. the
  inset alone), because a settled gesture inside a sketch does refresh it.
- **Home withdraws the whole editor**, and no anchored surface survives behind
  it.

---

## Summary table

| id | severity | family | surfaces | layer | reproduces after restart |
| --- | --- | --- | --- | --- | --- |
| UI3D-F-001 | P1 | `ANDROID_LAYOUT` | S19, S20, S31, S35, S36, S37, S38 | shell | yes |
| UI3D-F-002 | P1 | `CAMERA_PROJECTION` | S19, S20 | shell | yes |
| UI3D-F-003 | P2 | `STATE_VISIBILITY` | S19, S20 | shell + domain boundary | yes (race) |
| UI3D-F-004 | P1 | `STATE_VISIBILITY` / `CROSS_REPRESENTATION_LEAK` | S19, S20 | shell | yes |
| UI3D-F-005 | P2 | `OVERLAY_RENDER_STYLE` | S32, S21 | renderer | yes (pre-existing debt) |
| UI3D-F-006 | P3 | `ANDROID_LAYOUT` | S35 | shell | yes |
| UI3D-F-007 | P1 | `STALE_OWNER_OBJECT` | S19, S20 | shell | yes |

**No P0.** Nothing found risks data loss or corrupts project truth: every
finding is presentation. No `.forge` byte, section, version, fixture, revision,
history step or fingerprint is involved in any of them, and the audit made no
change to any of those either.
