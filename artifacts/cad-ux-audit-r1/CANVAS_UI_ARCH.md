# CANVAS_UI_ARCH — a neutral active-feature model, and the canvas seam

**Task:** `CAD-UX-AUDIT-R1` · **Baseline:** `42314cbfece5a27f163045f090394603ab80246b`
Design proposal. **Nothing here is implemented.** Names are working names for the
coordinator to accept, rename or reject.

---

## 1. The one new platform-neutral type

A single session-scoped value, in a new `forgeshape_cad_feature_tool.{h,cpp}`,
beside `SketchSession` and on its terms: **volatile, native-owned, never
serialized, never in a history step, never in a checkpoint, never in the
fingerprint.** It holds no camera, no viewport, no pixel, no Android type.

```cpp
namespace forgeshape {

enum class CadFeatureOperation : uint8_t { NewBody, Add, Cut };   // Add/Cut kernel-gated
enum class CadExtrudeExtent   : uint8_t { OneSide, Symmetric, TwoSides };

// What is being extruded, and how. The COMMITTABLE half: every field here
// becomes authored truth on commit and reaches a .forge byte.
struct CadExtrudeIntent {
    SketchEntityId      profileEntityId = kNoSketchEntity;  // a reference, never geometry
    CadExtrudeExtent    extent    = CadExtrudeExtent::OneSide;
    ExtrudeDirection    direction = ExtrudeDirection::AlongNormal;  // OneSide only
    Meters              distanceA = kDefaultExtrudeDepthMeters;
    Meters              distanceB = kDefaultExtrudeDepthMeters;     // TwoSides only
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
    ObjectId            targetBodyId = kNoObject;    // Add/Cut only; kNoObject for NewBody
};

// The whole live tool. The VOLATILE half is everything below the intent.
class CadFeatureTool {
public:
    bool active() const;
    const CadExtrudeIntent& intent() const;

    // Every mutator is REFUSED BY NAME and changes nothing on refusal, exactly
    // as SketchSession::setExtrude already is.
    CadStatus setExtent(CadExtrudeExtent);
    CadStatus setDirection(ExtrudeDirection);
    CadStatus setDistance(int side /*0=A,1=B*/, Meters);   // typed: never snapped
    CadStatus setOperation(CadFeatureOperation);           // refuses Add/Cut until the kernel exists
    CadStatus setTarget(ObjectId);

    // Whether the intent would commit right now, and why not.
    CadStatus previewStatus() const;

    // WORLD-space anchors the presentation layer needs. Derived every read from
    // the sketch frame and the intent; nothing about a camera enters.
    struct Anchors {
        bool  valid = false;
        Vec3  base;        // profile centroid on the support plane
        Vec3  normal;      // unit, the frame normal (NOT flipped by direction)
        Vec3  tipA;        // base + normal * distanceA   (or the far cap)
        Vec3  tipB;        // base - normal * distanceB   (TwoSides / Symmetric)
        Vec3  cluster;     // where the badges belong, frozen at gesture start
    };
    Anchors anchors() const;
};

CadFeatureTool& cadFeatureTool();   // process-scoped, like sketchSession()

}  // namespace forgeshape
```

**Why a separate type rather than growing `SketchSession`:** the tool is alive in
two situations the session is not — over a committed body (Edit Extrude) and,
later, over a target body for Add/Cut — and the session's whole contract is that
it dies at commit. Keeping them apart is also what lets `SketchSession` stay the
sketch's owner and nothing else.

**Why the intent is one struct:** it is exactly what a future `CadFeature`
record persists, so the commit path is a copy rather than a translation, and the
codec has one shape to encode.

## 2. Layer split

| Concern | Owner | Why |
| --- | --- | --- |
| profile id, operation, extent, direction, distances, target, validity | **C++ domain/session** (`CadFeatureTool`) | it is semantics, and it becomes `.forge` bytes |
| world anchors of the manipulator | **C++** (`CadFeatureTool::anchors`) | derived from the frame and the intent; the same rule that makes `BodyDimensionLabelAnchors` native |
| the arrow, ticks and extent marks as drawable lines | **C++** → a third `SketchOverlayPtr` producer | proven pattern; renderer needs no change (`renderer.cpp:3416-3420`) |
| world→screen projection of an anchor | **C++**, exposed through JNI | `projectWorldToScreen` already exists (`gizmo.h:367`) |
| screen scale of the control cluster | **C++** (§4) | drawing and hit testing must share one number, the gizmo's own rule |
| hit test of the manipulator | **C++**, from a `CameraSnapshot` | so what is drawn and what can be grabbed cannot differ |
| badge views, numeric field, IME, fonts, insets, handedness | **Android shell** | presentation; `EditorUiState`'s existing definition of what Java may hold |
| pointer → semantic sample | **`PointerSemantics` → `TouchPointer`** | already neutral, already carries `Mouse` |

Java holds **no** operation, extent, direction or distance. The existing
`draftDirection` int in `SketchEditorView` and `CadFeatureEditorView` should be
retired into this tool at the same time, or it becomes the seed of the
Android-owned semantics the contract forbids.

## 3. Renderer and picking seam

**Draw.** One new `SketchOverlayPtr` producer emitting, in world space:

* a shaft along `normal` from `base` to `tipA` (and `tipB` for two-sided);
* an arrowhead as short lines, not a solid — the overlay is a line list by
  renderer contract, exactly as the gizmo is;
* end ticks in the technical-drawing convention already used by both dimension
  overlays (`kSketchDimensionTickUnits` and friends), so the CAD annotations read
  as one drawing language rather than three.

Reuse `SketchOverlayStyle::Dimension` for the measured part and `Entities` for
the manipulator, or add exactly one style; the renderer switch is small and
closed.

**Occlusion.** The overlay path is depth-test-off, depth-write-off, drawn last.
That gives "always on top" for free. For an **Add** preview that is right; for a
**Cut** preview that reaches into a solid it is an owner question, because a
control that never occludes cannot show that it is inside the material. Record
it; do not default it silently.

**Pick.** Manipulator hit testing belongs in C++ from the same `CameraSnapshot`
the draw used, following the gizmo's contract verbatim:

* one captured `pointerId` per drag, a second pointer or a Cancel restores the
  pre-drag intent and records nothing;
* the drag basis (anchor, normal, screen scale) is **frozen at pointer-down** and
  is scale-free, so the preview growing under the finger cannot move the control
  out from under it;
* a degenerate viewpoint holds the last good value rather than guessing —
  `AxisSolveStatus::Unresolvable` is the existing precedent and the same solver
  applies, because dragging a length along a world axis *is* the axis solver.

**Badges and the numeric field** are Android chrome positioned from a projected
native anchor — the `BodyDimensionLabelsView` / `bodyDimensionLabelPoint` pattern
(`NativeViewport.java:474`) a third time. That gives real text, real IME, real
accessibility and real theming, with no billboard vertex path and no font in the
renderer. A badge is still 48 dp of hit area with a smaller glyph, per the floor
rule.

## 4. Billboard and scaling math — the ownership question, stated not answered

### 4a. What exists, and why it is not the answer

`gizmoWorldScale(camera, pivot, viewportHeight, &s)` returns the world length of
one reference unit at the pivot; the gizmo multiplies its canonical geometry by
it. Because `s` is proportional to `worldMetersPerPixel` at that depth, the
instrument occupies a **constant number of pixels at every distance**. That is
correct for a placement gizmo — reachability must not depend on zoom — and it is
the *opposite* of the owner's request in §3.4, which is that the control visibly
belongs to the work: smaller when you pull back, larger when you come in.

So this is a **new rule beside the gizmo's**, not a change to it. Both are built
on the same primitive, `worldMetersPerPixel` (`gizmo.h:378`), which reads the
camera's own projection matrix and therefore cannot drift from what is drawn.

### 4b. Proposed rule (recommended form: attach, then clamp)

Let `A` be the world anchor and `m = worldMetersPerPixel(camera, A, viewportH)`.

1. Author the cluster at a **world reference size** `W` metres — this is what
   makes it feel attached to the work rather than pinned to the glass.
2. Its unclamped screen size is `S_world = W / m` pixels.
3. Clamp: `S = clamp(S_world, S_min, S_max)`.
4. Draw and hit-test at world scale `s = S * m`.

At `S_min < S_world < S_max` the control is a rigid world object and scales
exactly with distance. Outside that band it saturates: unreadable at a distance
and screen-swallowing up close are both impossible. Two constants and one
reference size, all in units the project already speaks (metres and reference
units), and **one** number `s` shared by drawing and hit testing.

### 4c. Alternative if hard clamps read as "sticking"

A soft blend between the two behaviours, with one exponent:

```
S = S0 * (S_world / S0) ^ beta        beta in [0, 1]
S = clamp(S, S_min, S_max)
```

`beta = 0` is the gizmo (constant screen size), `beta = 1` is §4b. A value near
0.6-0.75 gives visible attenuation without the control ever getting small enough
to lose. **The exponent is a feel decision and belongs to the owner**, exactly as
`kMaskGain` and the Crease fractions were put to the owner rather than settled.

### 4d. Constants are deliberately NOT chosen here

The prompt forbids picking final px/dp "by eye". What can be said from the repo
without measuring on a device:

* the interactive floor is **48 dp of hit area** and it is not negotiable, so
  `S_min` must leave every badge and the arrow tip at or above it;
* the gizmo's own screen budget is a useful upper reference: `kGizmoHandleLengthUnits`
  96 and `kGizmoRingRadiusUnits` 78 reference units, and 1.5× of that "still leaves
  a 360-unit-wide window a margin on both sides" (`gizmo.h:341-353`). An extrude
  cluster that is larger than the rotate rings would dominate the viewport;
* the sketch annotation offsets (30 / 6 / 8 / 7 reference units) are the drawing
  language this control should match.

**Recommendation to the coordinator:** the implementing stage picks candidate
values inside those bounds, renders them, and puts the *choice* to the owner with
screenshots at near, mid and far zoom — the same shape `SCULPT-FCM-R1` used for
its two feel constants.

### 4e. Camera-facing, and what must NOT face the camera

* **Badges face the camera.** Built in the camera's right/up basis at the anchor:
  `p = A + (rx·R + ry·U)·s`. As Android chrome (recommended), this is automatic.
* **The arrow does not.** It points along the extrusion normal in world space —
  that is the whole information it carries, and billboarding it would destroy it.
* **Nothing may roll with the camera.** A cluster that spins as the user orbits
  is unreadable; use the camera's up projected into the screen plane, which is
  what a chrome view over the viewport gives for free.

### 4f. Anchor stability during a drag

Anchoring the badges to the moving far cap makes them chase the finger. Split:

* the **arrow** is anchored at `base` (on the support plane) and its tip tracks
  the live distance — that is the feedback;
* the **badge cluster** is anchored at `Anchors::cluster`, computed once at
  pointer-down and held for the gesture, so operation and extent controls stay
  where the user left them;
* on pointer-up the cluster re-derives, so it settles at the new geometry.

That is one sentence of policy and it removes the entire class of "the control
ran away from me".

## 5. The user-edit transaction

```
pointer-down on the arrow
    ScopedConstructionEdit opens (or, before a body exists, nothing opens —
    the sketch session is already volatile)
    -> tool intent updated per sample, authoritatively
    -> preview regenerated through the ONE path, generateCadMesh
pointer-up
    commitEdit()   -> one step, or none if nothing differed
second pointer / Cancel / Back
    cancelEdit()   -> pre-drag state restored, nothing recorded
```

The typed field submits the same way and is one transaction of its own; a typed
value is **never snapped**, exactly as `applyLineLength` and every precision
field already promise.

## 6. What the preview may and may not be

May: a regenerated `RuntimeMesh` published for the target body inside the open
edit (the live-state-is-authoritative rule the gizmo already follows), and an
overlay.

May not: a second hidden `SceneObject`, a renderer-only merged mesh, a triangle
list stitched from two bodies, or anything that would make Add/Cut *look*
implemented before the kernel exists. Until the kernel lands, `setOperation(Add)`
and `setOperation(Cut)` must **refuse by name** and the controls must be absent
or visibly unavailable — the repo's rule is that a control which cannot succeed
is not drawn.
