# UI-3D-STATE-C1 — Finding closure

One row per finding the correction owns. `UI3D-F-005` is listed last and is
**not closed**.

Every "after" figure is from the strict re-run of the original
`Ui3dStateAuditTest` on `emulator-5580` at the final tree, plus the new
`Ui3dStateCorrectionTest`, which asserts what the audit only recorded.

---

## UI3D-F-001 — every anchored surface stood one system-bar inset below its anchor

| | |
| --- | --- |
| **Severity / family** | P1, `ANDROID_LAYOUT` |
| **Status** | **CLOSED** |

**Before.** 30 of 60 measured rows carried exactly `+128 px` in Y and `0` in X;
median over all measured rows **48.90 dp**. Affected S19, S20, S31, S35, S36,
S37, S38 — every anchored surface in the product.

**Root cause.** The projected anchor is in viewport-content pixels, whose origin
is the window because the viewport is full-bleed. The placing views are children
of `overlayRoot`, which `EditorWorkspaceView.applyChromeInsets` gives the
chrome's window-inset padding, and a `FrameLayout` lays a child out at
`(paddingLeft, paddingTop)`. A translation computed from the anchor was therefore
drawn one inset lower. Each of the three placing views wrote that arithmetic
itself.

**Fix.** `ViewportAnchorSpace` — one conversion, shared by all three, derived
from runtime geometry with no constant of any kind:
`translation = anchor + (viewportOriginInWindow − parentOriginInWindow −
placedLayoutPositionInParent)`. Correct for an arbitrary left, right, top or
bottom inset, for a view that translates itself and for a container that
translates its children.

**After.** 60 measured rows, all PASS. Median **0.16 dp**, max **0.25 dp**.
`UI3DC1-03` asserts the per-axis error separately, so an offset that had merely
shrunk could not pass, and `UI3DC1-02` asserts the overlay is genuinely padded at
the moment of measurement, so the conversion is proved to be doing work.

---

## UI3D-F-006 — the extrude cluster clamped against the padded container

| | |
| --- | --- |
| **Severity / family** | P3, `ANDROID_LAYOUT` (a consequence of F-001) |
| **Status** | **CLOSED** |

**Before.** `CadExtrudeCanvasView.placeAt` clamped into `getWidth()/getHeight()`
of the canvas view — the padded content box — so the cluster reached its clamp an
inset earlier than the window required. Ten rows `CLAMPED`, residuals up to
**105.15 dp**.

**Root cause.** The same one: a bound expressed in the container's space while
the anchor was expressed in the viewport's.

**Fix.** The clamp rectangle is the real viewport carried through the same
offset. Two related placement errors were fixed with it: the box centred on the
anchor is now measured under the constraint its parent will impose (an
unconstrained measure of a cluster wider than the window answered a size layout
never produces, shifting the control sideways by half the difference), and the
clamp is guarded both ways so a box wider than the viewport pins to the near edge
instead of jumping to the far one.

**After.** Every CAD clamped residual fell 60–75 %: `after_orbit`, `symmetric`
and `two_sides` all 99.53 → **26.36 dp**; `two_sides_after_side_b_drag`
105.15 → **37.28 dp**; `finish_again` 63.32 → **11.38 dp**. `UI3DC1-04` asserts
the surface stays inside the true viewport rectangle, and `UI3DC1-11` asserts the
same for the cluster on every clamped row.

---

## UI3D-F-002 — the dimension labels never followed the camera

| | |
| --- | --- |
| **Severity / family** | P1, `CAMERA_PROJECTION` (missing refresh driver) |
| **Status** | **CLOSED** |

**Before.** `after_pan` 42.7–44.2 dp, `after_zoom_out` 70.8–**119.3** dp,
`after_zoom_in` 34.2–49.3 dp, `after_scale` 77.1–93.0 dp. An unrelated later
chrome act repaired the placement completely, which is what identified it as a
missing driver rather than a wrong projection.

**Root cause.** `onViewportGestureMoved` refreshed `cadExtrudeCanvas` and nothing
else, and `onViewportGestureSettled` called `onNativeStateChanged()` only when
the sketch state, the active `ObjectId`, the sculpt undo depth or the gizmo
commit count changed — a camera move is none of those.

**Fix.** `refreshWorldAnchoredUi()` is the one path for every world-anchored
surface, and both gesture callbacks drive it: `moved` on every pointer sample,
`settled` unconditionally and first, because the last sample of a fling or a
lifted pinch can land after the final move callback. It is deliberately cheaper
than `syncFromNative()` and rewrites no exact-value editor, so a half-typed draft
still survives a pointer sample.

**After.** `UI3DC1-06` orbits, pans, zooms out and zooms back in, asserting
attachment after each; every audit camera row is now PASS at ≤ 0.25 dp.

---

## UI3D-F-003 — the labels were absent on the frame the mode opened

| | |
| --- | --- |
| **Severity / family** | P2, `STATE_VISIBILITY` |
| **Status** | **CLOSED** |

**Before.** All three labels `GONE` with `modeActive=1`, `measurable=1` and
`anchorProjectsXYZ=111`, in 4 of the 5 places the audit opened the mode; the
fifth flipped between runs, so it was reported as a race. Four of the six failing
lifecycle assertions are this. Twelve spatial rows were `NOT_MEASURED` because
there was nothing on screen to measure.

**Root cause — two independent halves, both real.**
*Native:* `bodyDimensionLabelPoint` answered only once
`BodyDimensionSession::labelAnchors().valid` had been set by the **render
thread** building the overlay. The shell's one post-open refresh ran before that
frame, read false for all three axes, set each chip `GONE`, and never read again.
*Android:* a container the refresh has just made visible has not been laid out at
its own position — a `GONE` view is skipped by its parent's layout — so even a
correct anchor was placed against a container reporting position 0.

**Fix.** *Native:* `bodyDimensionLabelAnchors` derives the anchors from the
active body's current bounds, placement and camera scale, through the **same
builder** so there is no second copy of the midpoint arithmetic. It is a read: no
session, no cache, no revision. *Android:* the pass records that it read a
pending layout and repeats once after layout has run — capped at three, reset by
any settled pass, and narrowed to the container's own position so an ordinary
text change does not trigger it.

**After.** `UI3DC1-05` asserts all three labels are on screen with no second
event. All four lifecycle rows PASS, and all twelve previously `NOT_MEASURED`
rows are now measured.

---

## UI3D-F-004 — a label survived as ghost UI into Sculpt and over a hidden body

| | |
| --- | --- |
| **Severity / family** | P1, `STATE_VISIBILITY` / `CROSS_REPRESENTATION_LEAK` |
| **Status** | **CLOSED** |

**Before.** `UI3D-11 sculpt_entered: modeActive=0.0 … labelX=VISIBLE` — native
saying the session was closed while the chrome was still showing it. The same
over a hidden body. Two of the six failing lifecycle assertions.

**Root cause.** The session closed **itself on the render thread** when its body
stopped being measurable, and the design comment said the shell's next refresh
would find the mode shut. But the shell's next refresh is the one it already ran,
inside `onFreezeToSculpt` or the visibility command, *before* that frame — and
nothing refreshed afterwards. The measurability rule existed in four separate
copies.

**Fix.** `activeBodyDimensionsEditable` is that rule, named once, and asked by
the render thread, `bodyDimensionsState`, `setBodyDimensionsMode` and the anchor
read alike. `settledBodyDimensionSession()` applies the self-closing rule at
**every observation** rather than only on a frame, so the shell's refresh at the
instant of the transition already sees the mode shut. Above JNI,
`BodyDimensionLabelsView` clears its cached owner when it withdraws, so no later
refresh can resurrect it.

**After.** `UI3DC1-09a/b/c` assert no label, and no editor, survives hiding the
owner, entering Sculpt, returning to Construction, or closing the mode — and that
an unrelated later refresh (an orbit) does not bring one back. Both lifecycle
rows PASS.

---

## UI3D-F-007 — after a body switch the labels stood at the PREVIOUS body's anchors

| | |
| --- | --- |
| **Severity / family** | P1, `STALE_OWNER_OBJECT` |
| **Status** | **CLOSED** |

**Before.** The largest measurement in the audit: X **211.61 dp**, Y **280.91
dp**, Z **282.53 dp**.

**Root cause.** `sceneSelectBody` is followed by `onNativeStateChanged()`, and
that refresh read `bodyDimensionLabelPoint` before the render thread had rebuilt
the overlay for the new body — so `labelAnchors()` still held the previous body's
anchors and reported them valid. F-003's race with the opposite outcome.

**Fix.** The same one: the anchors are derived for the instant the chrome asks,
from the body being measured now. The cached `labelAnchors_` field and its
accessor were **deleted**, so the class can no longer hold an anchor belonging to
a body it does not measure. `BodyDimensionLabelsView` additionally tracks its
owner `ObjectId` and closes an open editor when it changes, so a typed size can
never reach the wrong body.

**After.** `UI3DC1-08` selects B, opens Dimensions, selects A, and asserts every
number on screen stands on the current owner's anchor. Every audit owner-switch
row is PASS or correctly `CLAMPED`; **0 stale-anchor rows** in 72.

---

## UI3D-F-005 — `SketchOverlayStyle::Dimension` renders fully transparent

| | |
| --- | --- |
| **Severity / family** | P2, `OVERLAY_RENDER_STYLE` |
| **Status** | **OPEN — not fixed, not touched, provenance preserved** |

A renderer root cause in a different layer, explicitly out of scope for this
correction. `forgeshape_renderer.cpp` still has cases for `GridMinor`,
`GridMajor`, `Axes` and `Entities`, no `Dimension` case and no `default:`, so
`push.highlight[3]` stays `0.0f` and `gizmo.vert`'s
`vec4(rgb, pc.highlight.w * weight)` makes such a range invisible.

`git diff` proves neutrality: `forgeshape_renderer.cpp`, `forgeshape_gizmo.cpp`,
`forgeshape_sketch_overlay.h` and every file under `app/src/main/cpp/shaders/`
are **byte unchanged**. `after_ui3d07_01_line_selected.png` still shows the
selected Line's numeric chip with no extension lines, dimension line or ticks —
the chip is now on the annotation instead of one inset below it, which is
F-001 closing and is not F-005 changing.

It remains recorded in `PROJECT_STATUS.md` → *Known Issues* and in
`artifacts/ui-3d-state-audit-r1/FINDINGS.md`.
