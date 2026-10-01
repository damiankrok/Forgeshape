# Extrude HUD v3 — screen plate vs projected quad vs renderer dock

Task `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1`, read-only. Baseline `bbae765`.

## 1. What exists today

The value is the one HUD piece the OWNER already accepts as attached, and the
reason is structural:

- Its LEADER (extension lines, dimension line, 45° ticks) is WORLD geometry in
  the overlay's `Dimension` range.
- The leader lies in the plane spanned by the extrusion axis and
  `cadExtrudeLeaderSide` = normal × view, signed to the reading-up side
  (`forgeshape_cad_extrude_tool.cpp:301-350`).
- The text stands on that world line: projected slots 32–41, rotated to it.

The action panel has no world part at all:

- It is a rigid Android `LinearLayout` plate (three glyphs: extent, operation,
  Flip).
- It is placed after projection: `CadHudPresentation.layoutPanel`, centred
  `24 + 4 dp` + half its size past the projected arrow point (slots 42–44),
  sliding back along the screen line at an edge.
- Its rotation is 0.35 × the reading angle, capped at 25° and tapered to level
  before vertical. Its scale is clamped to 0.40–1.60.
- It is covered by ONE invisible ≥ 48 dp proxy that opens the palette.

The renderer draws lines only:

- One `LINE_LIST` gizmo pipeline, with depth test and write off, drawn last.
- No triangle overlay pipeline, no dynamic state, no text or glyph path, no
  icon geometry.
- The overlay is rebuilt and re-uploaded on every camera change, because the
  arrowhead size depends on `metersPerPixel`.

Sources: `forgeshape_renderer.cpp:1586-1733, 2652-2777, 3375-3408`;
`forgeshape_sketch_overlay.h:28-93`.

## 2. Option A — keep the screen-space plate

**Advantages:**
- Crisp Android glyphs.
- Trivial accessibility.
- Fully JVM-testable (`CadHudPresentationTest`).
- No renderer or native change.

**Why it keeps reading as detached:**
- It is the one element with NO depth cue. The arrow, leader and preview all
  foreshorten with perspective; the plate does not.
- Its rotation follows the leader only partially (0.35×, capped), so it visibly
  disagrees with the geometry it sits on.
- Its position is a SCREEN function of a projected point. As the arrow
  foreshortens, the plate's "past the tip" offset stays in dp, so it drifts
  relative to the model.

**Why edge-fit fights attachment:** every edge rule moves the plate along a
screen line or toward the base by whatever fits the viewport rectangle. That
is the definition of not being attached to world geometry. Each correction
round (candidate sides → continuous slide → rotation taper) traded one
discontinuity for another.

**Recommendation:** do not continue. Options B and C are both feasible (below).

## 3. The geometry-attached dock frame (shared by B and C)

Native owns it, in `forgeshape_cad_extrude_tool.{h,cpp}`, beside
`cadExtrudeLeaderFor`, as a pure function over (anchors, camera, viewport,
scale). It has no stored state except the optional hysteresis bit in §3.3.

### 3.1 Orientation

```text
a  = unit extrusion axis of the primary side (anchors.axis)
f  = unit view direction at the anchor (eye -> base in perspective,
     camera forward in orthographic; the leader's own `view`)
m  = normalize(-f - a * dot(-f, a))   // toward the eye, perpendicular to a
s  = normalize(cross(a, m))           // == +/- cadExtrudeLeaderSide, unsigned
```

The dock is a quad in the plane spanned by `(a, s)`:
- It CONTAINS the axis and faces the eye as much as a plane containing the
  axis can. This is an axial billboard: the same plane the leader already
  stands in.
- `s` is perpendicular to the view by construction, so its projection is never
  foreshortened.
- `a` foreshortens exactly as the arrow does, so the dock tilts with the arrow
  in perspective.

### 3.2 Degeneracy (view ∥ axis)

`|m|` → 0 as the camera looks down the axis. Then every plane containing the
axis is edge-on, and the arrow itself is a point that cannot be dragged
(`solveAxisParameter`'s limit). Three fallbacks were compared:

| fallback | continuity | determinism | verdict |
| --- | --- | --- | --- |
| sketch-frame axis (`frame.u`) | jumps when it engages | yes | rejected: a teleport at the threshold |
| parallel transport of the previous frame | smooth | no (path-dependent) | rejected: presentation that depends on history is untestable as a pure function |
| camera-right projected ⊥ `a`, with hemisphere continuity | smooth | yes, given one transient sign bit | usable, but needs state |
| **hide the dock below a stated sine** | n/a | yes | **chosen** |

Chosen rule:
- The dock is drawn only while `sin(angle(f, a)) ≥ kCadFeatureViewMinAxisSine`
  (0.35). That is the SAME number that already defines a usable feature view.
- It fades out linearly over [0.35, 0.45]. It is not snapped off.
- Below 0.35 it is ABSENT. The extent, operation and Flip remain one tap away
  on the precision surface's toggle, which already lists New Body / Add / Cut
  and the exact fields.

A control drawn on an edge-on quad would be unreadable and ungrabbable, so
absent is the honest state ("a control that cannot succeed is not drawn").

### 3.3 In-plane orientation (upright glyph)

- Dock "right" = whichever of ±`a` reads left-to-right on screen.
- Dock "up" = whichever of ±`s` reads up.

This is the SAME reading rule `cadExtrudeLeaderSide` and
`readingAngleDegrees` already use, so the badge and the value text turn
together. At the vertical-shaft wrap both flip by a 180° in-plane rotation,
never a mirror.

Because the dock's CENTRE lies on the axis line (§3.4), the wrap rotates the
glyph in place. It does not move the dock: no positional teleport.

Optional transient hysteresis (±8° around vertical), shared by leader, value
and dock, removes flicker when the shaft hovers at vertical. It is presentation
only, never persisted.

### 3.4 Position

```text
centre = cadExtrudeArrowPoint(primary)
       + a * (clearWorld / r + halfLengthWorld)
clearWorld = (kCadExtrudeGrabRadiusUnits + 4) * pxPerUnit * mpp   // the corridor, in world units
r = projectedLength(a) / projectedLength(s)                       // foreshortening, in (0, 1]
```

- The anchor is the arrow's DRAWN point, the one function
  (`cadExtrudeArrowPoint`) the drawing, the hit test and today's panel anchor
  share.
- The offset is along the axis, beyond the head, divided by the
  foreshortening ratio `r`. The on-screen gap past the arrowhead therefore
  equals the grab corridor plus 4 dp, whatever the tilt. The dock never sits
  in the arrow's grab corridor.
- `r` is bounded below by the visibility rule in §3.2, because `r ≥ sin` there.
- No side offset, so nothing depends on the sign of `s`.

### 3.5 Size

- World size = the existing manipulator control scale: `view.scale.world`,
  i.e. `clamp(W / mpp / S_ref, 0.40, 1.60) × S_ref × mpp`, read ONCE per frame
  at `anchors.base`.
- That is the same fact that sizes the arrowhead, the leader and today's panel,
  so the badge shrinks with the arrow when the camera pulls back and saturates
  at both ends.
- Perspective then foreshortens it along `a` only.

### 3.6 Off-viewport

- The dock is computed in world space and projected.
- If ANY projected corner falls outside the viewport, or behind the eye, the
  whole dock is hidden: whole or not at all.
- There is no slide, no clamp and no edge rescue. The value text keeps its own
  existing clip-to-visible-leader rule.
- When the dock is hidden, the precision surface remains the path to the same
  choices.
- The dock does not chase the screen. The arrow is the thing to bring into
  view, and orbit or pan already do that.

### 3.7 Touch

- Hit area = the projected dock quad ∪ a 48 dp square centred on its projected
  centre (the floor). **Nothing else.**
- The proxy view returns `false` from `onTouchEvent` for a Down outside that
  shape, so `ViewGroup` dispatch hands the Down to the next child and finally
  to the SurfaceView. The invisible band around a small, skewed quad no longer
  eats cell taps.
- The proxy is never scaled or rotated (rule kept). Its Android bounds are the
  AABB of the shape, but it CLAIMS only the shape.

## 4. Option B — projected 3D quad, drawn by one custom Android overlay

- **Native** exports the dock as 4 projected corners plus `visible` and
  `alpha`. These are new tool-state slots after 44. Slots 42–44 are also
  missing from the function's doc comment today (`forgeshape_jni.cpp` near
  `:4849`), so the comment should be completed at the same time.
- **Java (`CadExtrudeCanvasView`)** draws the badge plate and glyph into that
  quad with `Matrix.setPolyToPoly(src, 0, dst, 0, 4)`, which is a full
  perspective homography, then `Canvas.concat`.
  - Glyphs are drawn as `Path`s through that matrix, not as rasterised
    `VectorDrawable` bitmaps, so they stay crisp under the transform.
  - Hit test: point-in-quad against the same 4 corners, ∪ the 48 dp floor.
- **Perspective and skew:** exact. The quad is the projection of a world
  rectangle, so the badge foreshortens and tilts with the arrow and the leader.
- **Accessibility:** one proxy, `contentDescription` = the operation's name +
  "Extrude options". TalkBack focus rect = the shape's AABB. Unchanged in kind
  from today.
- **Clipping and offscreen:** §3.6, decided natively, so Java only obeys
  `visible`.
- **Depth:** drawn on top, exactly like the arrow it belongs to. The arrow is
  depth-test-off already, so the badge is no more and no less occluded than
  its arrow.
- **Complexity:** moderate.
  - Native: one pure frame function plus 9–10 slots.
  - Java: one draw path and one hit predicate.
  - `CadHudPresentation` loses `layoutPanel`'s slide, rotation and scale
    policy, about 200 lines of edge-fit.
- **Renderer impact:** none.
- **Testability:**
  - The frame function is host-testable: orbit sweeps, degeneracy, the
    world-anchor drift ≡ 0, the clearance.
  - The hit predicate and the homography are JVM-testable.
  - Device tests by view id.

## 5. Option C — renderer-derived world-space dock

- **Plate:** needs a filled translucent quad.
  - Today's only overlay pipeline is `LINE_LIST` with no dynamic state, so this
    means a SECOND pipeline. It can reuse `gizmo.vert`/`gizmo.frag`, the
    layout and `GizmoPush`.
  - Range topology needs plumbing: a new `SketchOverlayStyle` or a topology
    field, a branch in `recordSketchOverlayDraw`, and create/destroy hooks.
  - It is rebuilt on swapchain resize, since viewport and scissor are fixed in
    the pipeline.
- **Glyphs:** 1 px lines. `wideLines` is not requested, so strokes must be
  gizmo-style bundles. Colour is limited to the push slots (neutral, X/Y/Z
  hue, highlight); there is no per-vertex RGB.
- **Text:** none possible. The value stays Android, as required anyway.
- **Picking:** a native hit test sharing the drawn placement, like the arrow's.
- **Accessibility:** still needs a Java proxy at the projected bounds, which is
  Option B's proxy anyway.
- **Testability:** geometry host-testable; appearance only on device.
- **Cost:** a renderer change (pipeline, style, draw branch, lifecycle). That
  breaks the property every CAD HUD stage so far has kept: "the renderer
  needed no change". It also adds an APK shader-variant cost, though no new
  shader file.
- **Gain over B:** the plate would share the arrow's exact rasteriser and
  antialiasing. Nothing else. Attachment quality is the same, because both use
  the same world frame.

## 6. Decision matrix (5 = best)

| criterion | A screen plate | **B projected quad (Android)** | C renderer dock |
| --- | --- | --- | --- |
| 3D attachment | 1 | **5** (same world frame as C) | 5 |
| touch reliability | 2 (AABB proxy over cells, edge slides) | **5** (shape-exact claim, floor kept) | 4 (native hit + Java proxy, two answers to keep equal) |
| implementation complexity | 5 (exists) | **4** | 2 |
| accessibility | 5 | **5** | 4 (proxy duplicated beside native geometry) |
| renderer impact | none | **none** | new pipeline + style + lifecycle |
| maintainability | 2 (edge-fit policy keeps growing) | **4** | 3 |
| testability | 5 | **5** (host frame + JVM hit/homography) | 3 (visual only on device) |
| failure modes | detaches; slides; covers cells | **hides at axis-on and off-viewport, by rule** | same as B, plus GPU-side faults |

## 7. Recommendation

**Option B.**
- The dock frame is native and world-derived (§3).
- Drawing is one Android overlay through a 4-point homography.
- The touch claim is exactly the visible shape plus the 48 dp floor.
- No renderer text or triangle pipeline. `RESEARCH-18` is satisfied without
  exception.

### Control density: ONE contextual badge

| density | for | against |
| --- | --- | --- |
| 3 glyphs (today) | everything visible | about 96×48–154×58 dp of proxy over the model; extent and Flip duplicate what the arrows already SHOW (one arrow vs two; the arrow's direction IS the side) |
| 2 badges (operation + extent) | extent visible as a glyph | two targets inside one corridor-clear dock; still duplicates the arrows |
| **1 badge (operation)** | Shapr3D's single Boolean badge; Fusion keeps options in the dialog; the arrows already encode extent and direction | extent and Flip are one tap further (in the palette) |

**Adopt 1 badge.**
- It shows the operation by glyph SHAPE (New Body / Add / Cut) and selected
  state, with colour only as a second carrier (rule kept).
- A tap opens the EXISTING palette: extent choices, the operations native
  offers, Flip for One Side.
- When native offers a single operation (a world-plane sketch: New Body only),
  the badge still opens the palette for extent and Flip, so it is never inert.

### Dimension label: unchanged

The leader, the value text above it, Two Sides' second value, tap-to-type and
`annotationCollapsed` all stay as they are. The only HUD-wide change is the
optional shared reading-side hysteresis (§3.3).

### Palette placement

- The palette stays ordinary, readable screen chrome (rule kept).
- It hangs from the badge's projected AABB.
- It flips above or below to fit, which is allowed because it is a transient
  menu, not the attached instrument.
