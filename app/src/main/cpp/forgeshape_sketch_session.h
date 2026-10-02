// The sketch edit session: the bounded, VOLATILE state between "New Sketch"
// and the one commit that turns a sketch into a CAD Body.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type.
// Pointer samples arrive as the same platform-neutral `TouchPointer` the
// camera and the gizmo consume; what leaves is a sketch, an overlay the
// renderer can draw, and -- once, at commit -- one project transaction.
//
// Lifecycle
// ---------
//     Inactive --begin(plane)--> Editing --finish()--> Ready --commit()--> Inactive
//                                   ^                    |
//                                   +---backToEditing()--+
//         any state --cancel()--> Inactive
//
// `beginEdit(body)` enters the same Editing state over a STAGED copy of a
// committed CAD Body's state, and `commitEdit()` replaces `commit()` for it
// (`SKETCH-UX-R1` F): one transaction into the existing body, no second body.
//
// Nothing before `commit` is project truth. A half-drawn line, a deleted
// entity, a chosen profile and a typed depth all live here, and the scene,
// the history, the autosave fingerprint and the `.forge` document know
// nothing of them: `cancel` costs the project nothing, and process death
// during a sketch loses the sketch and nothing else. R0's rule is that an
// uncommitted sketch is volatile; edit-session recovery is not a feature.
//
// `commit` is ONE `ScopedConstructionEdit` around ONE `addCadBody`, so
// creating a body from a sketch is exactly one Undo, and a refused commit
// mints no ObjectId and records nothing.
//
// What a touch means
// ------------------
// The session owns ONE pointer at a time, by id. A second pointer cancels the
// entity in progress and hands the gesture to the camera -- pan and pinch
// stay available while sketching, and a stroke can never become a pan half
// way through. Pixels are turned into sketch coordinates ONCE, at the moment
// they are used, by intersecting the camera's pick ray with the workplane;
// the pixel itself is never stored.
//
// Snapping: an endpoint snap wins over a grid snap, both are always on, and
// both produce EXACT coordinates -- a snapped point is the other entity's own
// stored value or an exact multiple of the grid spacing, never a rounded
// pixel. A typed value is never snapped.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_overlay.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

// What the session's current candidate WOULD produce, evaluated on demand and
// only for the newest authored state (`CAD-VERTICAL-SLICE-R1`).
//
// It is the preview and the commit's proof at once: the same candidate state a
// commit would apply, regenerated through the same path. It is never project
// truth -- no history step, no fingerprint, no `.forge` byte, no publish -- and
// it is LATEST-ONLY by construction: it is keyed by the candidate revision, so a
// result computed for an older set of parameters is simply never the answer to
// a newer question, and a drag that moves forty times between two frames costs
// at most one regeneration per frame, never one per MotionEvent.
struct CadCandidateEvaluation {
    uint64_t revision = 0;
    // False until an evaluation for `revision` exists.
    bool valid = false;
    CadStatus status = CadStatus::NotSketching;
    // The later feature that refused, when the refusal is one's (an edit to an
    // earlier feature that makes a later Add disjoint, say); 0 otherwise.
    uint32_t failedFeatureId = 0;
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
    // The body whose drawn mesh the preview REPLACES (Add, Cut, or an edit), or
    // kNoObject for a New Body, which is drawn beside the scene.
    ObjectId targetBodyId = kNoObject;
    // The regenerated candidate, when `status` is Ok.
    std::shared_ptr<const CadBodyMesh> mesh;
    // Microseconds the regeneration took, and how much of it was the kernel.
    double micros = 0.0;
    double kernelMicros = 0.0;
};

enum class SketchSessionState : uint8_t {
    Inactive,
    Editing,
    Ready,
};

const char* sketchSessionStateName(SketchSessionState state);

// The rail's seven entries. Select is a tool so that a tap in the viewport has
// exactly one meaning at a time: with a drawing tool held it places geometry,
// with Select held it chooses an entity.
//
// Arc and Spline are APPENDED (`SKETCH-UX-R1` D): the existing five keep their
// indices, so a stored tool index, a test and the rail's own order do not move
// under them.
enum class SketchTool : uint8_t {
    Select,
    Line,
    Polyline,
    Rectangle,
    Circle,
    Arc,
    Spline,
};

constexpr int kSketchToolCount = 7;

const char* sketchToolName(SketchTool tool);
bool sketchToolFromIndex(int index, SketchTool* out);
int sketchToolIndex(SketchTool tool);

// What the last snap landed on, for the overlay's marker.
enum class SketchSnapKind : uint8_t {
    None,
    Grid,
    Endpoint,
};

// ---------------------------------------------------------------------------
// Sizes
// ---------------------------------------------------------------------------

// The sketch grid: a bounded, fixed rhythm. A quarter of a metre, so the
// default orthographic framing (about nine metres tall) shows roughly forty
// cells and a finger can land on one; every fourth line is major, so the
// major rhythm IS the world grid's one-metre rhythm. Fixed rather than
// zoom-derived: an adaptive multi-decade grid is a CAD grid system this stage
// does not build. The grid never alters a typed value.
constexpr double kSketchGridSpacingMeters = 0.25;  // the fallback / minimum minor step
constexpr int kSketchGridMajorEveryNMinor = 4;
constexpr double kSketchGridHalfExtentMeters = 8.0;

// CAD-A3 adaptive grid. The minor step is chosen so a grid square is at least
// this many screen pixels wide, and the grid is drawn as a bounded number of
// lines each side of the frame origin rather than to a fixed world extent, so
// it stays readable and cheap at every zoom.
constexpr double kSketchGridMinPixels = 26.0;
constexpr int kSketchGridLinesPerSide = 40;
constexpr double kSketchGridMinStepMeters = 1.0e-4;   // 0.1 mm
constexpr double kSketchGridMaxStepMeters = 100.0;

// A deterministic view-adaptive minor grid step: the smallest 1/2/5 x 10^k
// metres whose on-screen spacing is at least kSketchGridMinPixels. `worldPerPixel`
// is metres per screen pixel at the sketch plane. Never persisted -- the grid is
// display and snap only, and a typed value is never re-snapped to it (`CAD-A3`
// J1). Falls back to kSketchGridSpacingMeters on a non-usable input.
double adaptiveSketchGridStep(double worldPerPixel);

// The world authoring frame a sketch is drawn on: a right-handed orthonormal
// basis (u x v = n) at a world origin. For a world-plane sketch it is the
// plane's frame at the world origin; for a face sketch it is the producer's
// resolved face frame.
struct SketchFrame {
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 u{1.0f, 0.0f, 0.0f};
    Vec3 v{0.0f, 1.0f, 0.0f};
    Vec3 n{0.0f, 0.0f, 1.0f};
};

// Snap and hit tolerances, in reference units (dp). Both reach the 48-unit
// interactive floor as a DIAMETER: a target 24 units either side of an
// endpoint or an edge is what a finger can hit.
constexpr float kSketchSnapToleranceUnits = 24.0f;
constexpr float kSketchHitToleranceUnits = 24.0f;

// A single-pointer gesture that travels less than this is a TAP. The same
// radius the selection controller uses.
constexpr float kSketchTapSlopPixels = 24.0f;

// What became of one Ready-state tap candidate (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`).
// DIAGNOSTIC only: the debug build logs it as `FORGESHAPE_SKETCH_TAP:<name>` so a
// missed tap on a physical device is attributable from one logcat capture. It
// decides nothing, is never stored and never crosses JNI as a value.
enum class SketchTapOutcome : uint8_t {
    None,
    // A face (or loop region) under the Down pixel toggled.
    Resolved,
    // The Down pixel's ray met the sketch plane outside every bounded area.
    Exterior,
    // A still tap on the drawn arrow HEAD: the manipulator's, toggling nothing.
    ArrowHead,
    // The finger travelled past the tap slop: an orbit or a drag, not a tap.
    Travel,
    // The Down pixel's ray is parallel to the sketch plane (or its hit is not
    // finite): the existing ray-plane math answered no point.
    RayParallel,
    // The area under the finger could not be toggled (no face, a refused
    // region, a session not in Ready).
    InvalidFace,
    // The selection is at its bound (`TooManyRegions`).
    SelectionCap,
};

const char* sketchTapOutcomeName(SketchTapOutcome outcome);

// THE one statement of whether the camera is handed a sketch-session event
// after the session saw it (`forgeshape_jni.cpp` asks nothing else). A
// consumed event never reaches the camera; while a sketch is being DRAWN no
// single pointer does; in READY a single pointer navigates -- EXCEPT while a
// tap is still armed (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`), because a finger
// that jitters inside the tap slop must neither orbit the view nor be
// resolved against a camera that moved under it. The caller resets the
// camera's gesture whenever this is false, so the first event after a tap
// disarms re-anchors the orbit at the CURRENT point: no jump from the Down.
// Two pointers are never held back.
bool sketchEventReachesCamera(SketchSessionState state, bool consumed, int pointerCount,
                              bool readyTapArmed);

// The dimension annotation's proportions, in reference units (dp), so it reads
// the same at any zoom (`SKETCH-UX-R1` E1). The dimension line stands this far
// off the stroke it measures -- far enough that the label never sits on the
// geometry -- the extension lines start a small gap away from the endpoints and
// overshoot the dimension line, and the end ticks are the technical-drawing
// slash rather than a filled arrowhead, which reads better at phone sizes.
constexpr double kSketchDimensionOffsetUnits = 30.0;
constexpr double kSketchDimensionExtensionGapUnits = 6.0;
constexpr double kSketchDimensionOvershootUnits = 8.0;
constexpr double kSketchDimensionTickUnits = 7.0;

// The snap marker's half size, in reference units.
constexpr float kSketchSnapMarkerUnits = 8.0f;

// ---------------------------------------------------------------------------
// The session
// ---------------------------------------------------------------------------

class SketchSession {
public:
    SketchSession() = default;

    // --- lifecycle -------------------------------------------------------

    // Opens a fresh sketch on `plane`. Refused (NotSketching) while a session
    // is already open; the caller cancels first. The authoring frame is the
    // plane's own frame at the world origin -- a new world-plane CAD body
    // starts at the identity placement.
    CadStatus begin(Workplane plane);

    // Opens a fresh FACE-supported sketch (`CAD-A3`). `worldFrame` is the
    // producer's chosen face resolved into WORLD space (origin, right-handed
    // orthonormal u/v/n, n outward); `support` is the TopoRef the committed
    // body carries. The sketch authors on that frame -- exactly as a world
    // plane, but placed on the face -- and its canonical basis is XY. Refused
    // (NotSketching) while a session is open; the caller cancels first.
    //
    // `producerState` (`CAD-VERTICAL-SLICE-R1`): the producer body's current
    // authored state. When it is given the sketch may also be applied to that
    // body as an Add or a Cut -- the SAME SceneObject grows or loses material --
    // instead of becoming a new face-supported body. Held as a staged copy;
    // nothing is written until the one commit.
    CadStatus beginOnFace(const SketchFrame& worldFrame, const TopoRef& support,
                          const CadBodyState* producerState = nullptr);

    // Opens a session that EDITS an existing CAD body's sketch
    // (`SKETCH-UX-R1` F).
    //
    // The session takes a STAGED COPY of the body's authored state and the
    // project keeps its own until `commitEdit`. Nothing here regenerates the
    // body, records a history step or moves a `.forge` byte, so `cancel` from
    // an edit costs the project exactly what `cancel` from a new sketch costs:
    // nothing. `worldFrame` is where the body's sketch is authored in world
    // space, which the caller resolves the same way it does for a new sketch.
    //
    // Refused (`NotSketching`) while a session is already open.
    CadStatus beginEdit(ObjectId bodyId, const CadBodyState& state, const SketchFrame& worldFrame);

    // Opens a session that EDITS one feature of a CAD body's chain
    // (`CAD-VERTICAL-SLICE-R1`). `featureId` kCadFeatureId is the base (what
    // `beginEdit` does); a later feature stages ITS sketch, extrusion and
    // operation. `worldFrame` is where that feature's sketch stands in the
    // world. With `startReady` the session opens directly on the extrusion
    // (the "edit the Extrude step" act); without it, on the sketch.
    //
    // Refused (`NotSketching`) while a session is open, `ProfileNotFound` for a
    // feature the chain does not have, and whatever the state's own validation
    // says when it could not be regenerated.
    CadStatus beginEditFeature(ObjectId bodyId, const CadBodyState& state, uint32_t featureId,
                               const SketchFrame& worldFrame, bool startReady);

    // The feature an edit session edits, or 0 when it authors a new one.
    uint32_t editingFeatureId() const { return editingFeatureId_; }

    // Which body this session is editing, or `kNoObject` when it is authoring a
    // new one. The ONE answer to "is this an edit?" -- no shell flag mirrors it.
    ObjectId editingBodyId() const { return editingBodyId_; }
    bool editingExistingBody() const { return editingBodyId_ != kNoObject; }

    // THE edit commit: Ready -> Inactive, applying the staged state to the body
    // as ONE `ScopedConstructionEdit` and so exactly one Undo. A refusal changes
    // nothing at all and stays in Ready with the reason named. Dependent
    // face-supported bodies re-resolve as the derived consequence they are; no
    // follow-state is written here.
    CadStatus commitEdit(ConstructionScene& scene, ConstructionHistory& history);

    // The world authoring frame the camera should look normal to. Valid while
    // active; the plane's frame at the origin for a world-plane sketch, the
    // producer's face frame for a face sketch.
    //
    // AUTHORING TRUTH: `(u, v)` mean what this frame says they mean, and the
    // orientation navigator never touches it. See `viewFrame`.
    const SketchFrame& frame() const { return frame_; }

    // --- the orientation navigator (`SKETCH-UX-R1` C) --------------------
    //
    // Two PRESENTATION facts, and they are presentation in the strongest sense
    // the project has: neither is persisted, neither reaches a history step, a
    // checkpoint, the fingerprint or a `.forge` byte, and neither changes one
    // authored coordinate. They decide only which way the camera is pointed and
    // which way up the sketch is drawn.

    // Whether the camera looks along the frame's NEGATIVE normal (the "back" of
    // the plane) rather than its positive one.
    bool viewFlipped() const { return viewFlipped_; }
    // How many quarter turns the view is rolled about the normal, 0..3.
    int viewQuarterTurns() const { return viewQuarterTurns_; }

    // The frame the CAMERA should be framed on: `frame()` with the flip and the
    // roll applied. Always right-handed, so no view state can mirror anything.
    SketchFrame viewFrame() const;

    // Rolls the view by `quarterTurns` (any integer; +1 makes the drawing
    // appear to turn 90 degrees counter-clockwise). Never refused, because it
    // cannot fail and cannot change the sketch.
    void rotateView(int quarterTurns);
    void setViewFlipped(bool flipped);

    // Changes a WORLD-plane sketch's support plane, re-basing the authoring
    // frame (`SKETCH-UX-R1` C4).
    //
    // Refused, changing nothing:
    //   * `SketchNotEmpty`   -- the sketch already carries an entity. The
    //                           authored `(u, v)` values would come to mean a
    //                           different place in the world, and silently
    //                           reinterpreting them is exactly what this
    //                           refuses to do. The user empties the sketch, or
    //                           cancels.
    //   * `InvalidWorkplane` -- the sketch is FACE-supported. Its support is a
    //                           TopoRef and switching to a world plane would
    //                           detach it from the producer it follows; that
    //                           takes a Cancel and a new support choice.
    CadStatus setSupportPlane(Workplane plane);

    // --- the selected line's dimension (`SKETCH-UX-R1` E) ----------------

    // The selected entity's length, when it is a straight Line. False for any
    // other selection, which is the whole condition for drawing the dimension.
    bool selectedLineLength(Meters* outLength) const;

    // Sets a straight Line's length EXACTLY, keeping its first endpoint fixed
    // and its direction unchanged: `P1' = P0 + normalize(P1 - P0) * length`.
    //
    // Deliberately not a constraint solver: no neighbouring entity moves, no
    // angle is preserved for anything else, and nothing is re-snapped. A chain
    // this opens becomes an open profile, which `finish` then refuses by name --
    // the honest outcome, rather than dragging the rest of the sketch along.
    //
    // Refused, changing nothing, on a non-finite, zero, negative or
    // out-of-range length, and on an id that is not a Line's.
    CadStatus applyLineLength(SketchEntityId id, Meters length);

    // Where the dimension's numeric LABEL belongs, in sketch coordinates: the
    // midpoint of the dimension line, offset clear of the stroke. The shell
    // turns it into a screen position through `sketchToScreen` and draws the
    // editable field there. False when no straight Line is selected.
    //
    // `worldPerUnit` is the same camera-derived scale the overlay is built
    // with, so the label sits exactly on the annotation whatever the zoom.
    bool selectedLineDimensionAnchor(double worldPerUnit, SketchPoint* out) const;

    // The current adaptive minor grid step, in metres. What a grid snap rounds
    // to, and what the overlay draws. Sampled from the camera at pointer-down.
    double gridStep() const { return gridStep_; }

    // Drops everything. Never a project mutation.
    void cancel();

    SketchSessionState state() const { return state_; }
    bool active() const { return state_ != SketchSessionState::Inactive; }
    Workplane plane() const { return sketch_.plane; }

    // --- tools -----------------------------------------------------------

    bool setTool(SketchTool tool);
    SketchTool tool() const { return tool_; }

    // --- pointer ---------------------------------------------------------

    // Feeds one complete touch event with the camera it was made under.
    // Returns true when the session CONSUMED the event -- the caller then
    // hands it to neither the camera nor the selection. Returns false, having
    // dropped any entity in progress, when the gesture belongs to navigation.
    bool onTouch(TouchAction action, int32_t actionPointerId, const TouchPointer* pointers,
                 int count, const CameraSnapshot& camera, int viewportWidth, int viewportHeight);

    // Drops any gesture in progress without touching the sketch. For Cancel
    // and for the Surface going away.
    void resetGesture();

    // --- the sketch ------------------------------------------------------

    const CadSketch& sketch() const { return sketch_; }

    SketchEntityId selectedEntityId() const { return selectedEntityId_; }
    bool select(SketchEntityId id);
    void clearSelection();

    // Removes the selected entity. An edit-session act, not a history step.
    CadStatus deleteSelected();

    // Replaces one entity's geometry with EXACT typed values. Never snapped.
    // Refused, changing nothing, when the geometry is invalid.
    CadStatus replaceEntity(SketchEntityId id, SketchEntity::Payload payload);

    // Ends a polyline being placed vertex by vertex: kept as an open polyline
    // when it has at least two vertices, discarded otherwise. Called by the
    // tool change and by finish; harmless when none is in progress.
    void endPolylineInProgress();

    // --- finish and extrude ----------------------------------------------

    // Editing -> Ready. Validates the sketch and extracts its closed
    // profiles. Refused, staying in Editing, when the sketch is invalid or
    // closes no profile; the reason names the first rejection when there is
    // one. With exactly one profile it is selected; with several the choice
    // is left open and `commit` refuses AmbiguousProfile until one is made.
    CadStatus finish();

    // Ready -> Editing, keeping the sketch, so a profile problem can be fixed.
    void backToEditing();

    const ProfileExtraction& profiles() const { return regions_.loops; }
    // The REGIONS the finished sketch encloses (`CAD-VERTICAL-SLICE-R1`): one
    // per closed loop, an outer boundary minus its direct holes. Valid in Ready.
    const SketchRegionExtraction& regions() const { return regions_; }

    // Selects exactly the region whose outer loop is `anchorEntityId`,
    // replacing any other selection: the panel's list row.
    CadStatus selectProfile(SketchEntityId anchorEntityId);
    SketchEntityId selectedProfileId() const { return extrude_.profileEntityId; }

    // Adds the region to the selection or, when it is selected, removes it.
    // Adding a region DROPS every selected region it would overlap, touch or
    // share a loop with -- so one tap on the disk while the ring around it is
    // selected SWITCHES to the disk -- while disjoint regions accumulate.
    // Refused (`OverlappingHoles`) for a region that cannot be selected.
    CadStatus toggleRegion(SketchEntityId outerAnchorId);

    // The tap in the canvas: the region under the pixel, toggled. False,
    // changing nothing, when the tap lands in no region. Records the tap's
    // diagnostic outcome. The touch path hands it the camera captured at the
    // tap's DOWN, never a later one.
    bool toggleRegionAt(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                        int viewportHeight);

    // Whether a Ready-state tap is armed right now: a single finger is down,
    // has not travelled past the slop, and is not the arrow head's.
    bool readyTapArmed() const {
        return state_ == SketchSessionState::Ready && regionTapArmed_;
    }
    // The last tap candidate's diagnostic outcome, and a serial that advances
    // every time one is recorded, so a caller can tell a new one from an old.
    SketchTapOutcome lastTapOutcome() const { return lastTapOutcome_; }
    uint32_t tapOutcomeSerial() const { return tapOutcomeSerial_; }

    // Whether the region is part of the current selection.
    bool regionSelected(SketchEntityId outerAnchorId) const;

    // --- planar faces (`CAD-V6-S2`) ---------------------------------------
    //
    // When a finished sketch's areas need the planar arrangement
    // (`sketchRequiresPlanarFaces`: a crossing or T-junction cut an area the
    // loop-region model cannot name), the session selects ATOMIC FACES instead
    // of regions: `extrude().selection` is `PlanarFaces`, a tap toggles exactly
    // the face under the finger, several faces extrude as their union, and the
    // stored selection is the canonical `PlanarFaceRef` list -- never an index.
    // Every other sketch stays on `LoopRegions`, byte for byte as before. The
    // indices below are TRANSIENT (into the arrangement derived at Finish, in
    // its canonical face order) and are re-read on every refresh.
    CadSelectionKind selectionKind() const { return extrude_.selection; }
    const SketchArrangement& arrangement() const { return arrangement_; }
    size_t planarFaceCount() const { return faceShapes_.size(); }
    bool planarFaceSelected(size_t index) const;
    // A point strictly inside the face and outside its holes, and its area.
    bool planarFaceInfo(size_t index, SketchPoint* outInterior, double* outArea) const;
    // Adds the face or, when selected, removes it: a pure SET toggle that no
    // other selected face can gate. Refused by name, the selection standing
    // as it was, only outside Ready, for an index that names no face, or past
    // `kMaxPlanarFaceSelection` (`TooManyRegions`). Whether the set extrudes
    // is the candidate's verdict, never a tap's.
    CadStatus togglePlanarFace(size_t index);
    // Selects exactly this face: the panel's list row.
    CadStatus selectPlanarFace(size_t index);
    // Whether anything is chosen, whichever the selection kind.
    bool selectionChosen() const;
    // How many regions or faces are chosen.
    size_t selectedAreaCount() const;
    // True after Finish found that a stored face selection (an edited feature)
    // no longer resolves exactly: nothing is re-bound, the candidate reports
    // `PlanarFaceUnresolved`, and the user chooses again or cancels.
    bool selectionLost() const { return selectionLost_; }

    // --- the operation (`CAD-VERTICAL-SLICE-R1`) --------------------------
    //
    // New Body, Add or Cut: what the extrusion does to material. New Body makes
    // a new SceneObject; Add and Cut modify the body the sketch stands on, in
    // place, as one more feature of its chain.

    // Whether an operation can be chosen now. New Body: a new sketch or an
    // edit of a body's first feature. Add and Cut: a sketch standing on a face
    // of a CAD body (its target), or an edit of a later feature.
    bool operationAvailable(CadFeatureOperation operation) const;

    // Refused, changing nothing: `OperationNeedsTarget` for Add/Cut with no
    // body to apply them to, `InvalidFeatureOperation` for New Body over a later
    // feature or Add/Cut over a first one.
    CadStatus setOperation(CadFeatureOperation operation);
    CadFeatureOperation operation() const { return operation_; }

    // The body an Add or a Cut modifies, or an edit rewrites; kNoObject when
    // the candidate is a new body.
    ObjectId operationTargetId() const;

    // THE evaluation of the current candidate, computed at most once per
    // candidate revision (see `CadCandidateEvaluation`). Only in Ready with a
    // selection; otherwise an invalid evaluation.
    const CadCandidateEvaluation& evaluateCandidate();

    // Where the candidate stands in the world: the target body's own derived
    // model for an Add, a Cut or an edit; for a New Body the placement the
    // committed body will have -- identity on a world plane, the support face's
    // frame on a face. Presentation for the preview; never stored.
    bool candidateWorldModel(ConstructionScene& scene, Mat4* out) const;
    uint64_t candidateRevision() const { return candidateRevision_; }

    // Writes the PRIMARY distance, and in One Side alone the side it is on.
    // The extent MODE is preserved.
    CadStatus setExtrude(Meters depth, ExtrudeDirection direction);
    const ExtrudeFeature& extrude() const { return extrude_; }

    // --- the extent (`CAD-EXT-R1`) ---------------------------------------
    //
    // One Side, Symmetric and Two Sides are three ways of authoring the SAME
    // two distances, and `ExtrudeFeature` is still the only model of them:
    // nothing below adds a draft extent, a second depth or a copy of either.

    // Changes which combinations the controls author, carrying the distances
    // across by the deterministic policy in `extrudeFeatureWithExtent`. Refused
    // (`NotSketching`) outside Ready, and refused by name when the resulting
    // extrusion is one the domain would not accept.
    CadStatus setExtrudeExtent(ExtrudeExtentMode extent);

    // Writes ONE side's distance -- what a drag on that side's arrow and its own
    // exact field both land through. In Symmetric either side writes the one
    // shared distance; in One Side a write to the empty side is refused
    // (`InvalidExtrudeExtent`), because there is no handle there to have moved.
    CadStatus setExtrudeSide(bool positiveSide, Meters distance);

    // The user's last One Side choice, held as VOLATILE intent so a mode round
    // trip gives the side back. Never persisted, never in a history step, never
    // in the fingerprint and never a second answer to which side the solid is
    // on -- `extrude_.direction` remains that, and this is only what a
    // transition out of a two-sided mode consults.
    ExtrudeDirection preferredOneSideDirection() const { return oneSideDirection_; }

    // --- the canvas extrude manipulator (`CAD-UX-S1`) --------------------
    //
    // The arrow, the exact length beside it and the Flip that reverses it are
    // three views of the ONE `extrude_` this session already owned. Nothing
    // below adds a second copy of the profile, the depth or the direction.

    // Where the manipulator stands in WORLD space, for the drawing, the hit
    // test and the chrome anchor alike. False in any state but Ready, without a
    // chosen profile, or when the geometry cannot produce anchors.
    bool extrudeAnchors(CadExtrudeAnchors* out) const;

    // The camera-derived facts the manipulator is DRAWN with for one frame
    // (`CAD-FOUNDATION-C1`), read once through `cadExtrudeManipulatorScale` at
    // the manipulator's own base. The frame hands the SAME value to
    // `overlay`, and the chrome reads it through here too, so the drawn head,
    // the hit test and the HUD glyphs are one number. Invalid (and false)
    // whenever there are no anchors.
    bool extrudeViewFacts(const CameraSnapshot& camera, int viewportWidth, int viewportHeight,
                          CadExtrudeViewFacts* out) const;

    // Reverses which side of the sketch plane the solid grows on, keeping the
    // exact depth and the same profile. A One Side control ALONE: Symmetric
    // reaches both sides already, and Two Sides states both explicitly, so in
    // either it is refused by name (`InvalidExtrudeExtent`) and withdrawn above
    // JNI rather than shown and then refused.
    //
    // Flip is a DIRECTION change and never a negative depth: an extrusion
    // stores a positive length and a two-valued direction, and encoding the
    // other side as a negative number would make every consumer of the depth
    // -- the mesh generator, the codec, the panel field -- learn about a sign
    // it has never had to carry. Refused (`NotSketching`) outside Ready.
    CadStatus flipExtrudeDirection();

    // The live drag, for the shell diagnostics and the tests. The session owns
    // it because the session owns what it writes.
    const CadExtrudeManipulator& extrudeManipulator() const { return extrudeDrag_; }

    // THE commit. Ready -> Inactive on success, with the new body's id in
    // `outId`. One transaction; a refusal changes nothing and stays in Ready.
    //
    // For Add and Cut (`CAD-VERTICAL-SLICE-R1`) no body is created: the
    // feature is appended to the TARGET body's chain inside ONE
    // `ScopedConstructionEdit`, its id is written to `outId`, and the
    // Objects list does not grow. A candidate the evaluation refused -- a
    // disjoint Add, a Cut that misses, a Cut that would remove everything -- is
    // refused by that name and never falls back to a New Body.
    CadStatus commit(ConstructionScene& scene, ConstructionHistory& history, ObjectId* outId);

    // The state a commit WOULD build, for the extrude preview and the tests.
    CadBodyState candidateState() const;

    // --- overlay ---------------------------------------------------------

    // The current overlay, rebuilt only when something changed. Empty while
    // inactive. `worldPerUnit` sizes the snap marker; it is the world length
    // of one reference unit at the plane, which the caller derives from the
    // camera exactly as the gizmo does.
    //
    // `view` is the manipulator's camera facts for this frame
    // (`extrudeViewFacts`); without them no arrow is drawn, because an arrow
    // sized by any other scale would disagree with what the hit test grabs.
    // A rebuild the CAMERA causes -- another `worldPerUnit` or other view
    // facts -- is a new overlay revision, so the renderer's revision-gated
    // upload can never keep the previous zoom's vertices.
    SketchOverlayPtr overlay(float worldPerUnit);
    SketchOverlayPtr overlay(float worldPerUnit, const CadExtrudeViewFacts& view);
    uint64_t overlayRevision() const { return overlayRevision_; }
    // The manipulator facts the current overlay was BUILT with; verification.
    const CadExtrudeViewFacts& overlayViewFacts() const { return overlayView_; }

    // --- mapping, for the shell's verification --------------------------

    // Pixel -> sketch point, unsnapped, through the current camera. False
    // when the ray misses the plane.
    bool screenToSketch(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                        int viewportHeight, SketchPoint* out) const;
    // The authoring frame's (u, v) -> world, and with an out-of-plane offset
    // along the frame normal. The one place a sketch point becomes world space.
    Vec3 sketchToWorld(const SketchPoint& point) const;
    Vec3 sketchToWorldAt(const SketchPoint& point, double offset) const;

    bool sketchToScreen(const CameraSnapshot& camera, const SketchPoint& point,
                        int viewportWidth, int viewportHeight, float* outX, float* outY) const;

    // The orbit angles that look straight at a plane's positive normal with
    // U to the right and V up. The camera owner applies them; the session
    // stores nothing about the camera.
    static void sketchViewAngles(Workplane plane, float* outYaw, float* outPitch);

    // --- diagnostics -----------------------------------------------------

    CadStatus lastStatus() const { return lastStatus_; }
    // The CAD bootstrap's first commit (`forgeshape_project_bootstrap.h`)
    // reports through the same diagnostic the ordinary commit does, so the
    // shell reads one `lastStatus` whichever commit it asked for.
    void recordLastStatus(CadStatus why) { lastStatus_ = why; }
    SketchSnapKind lastSnapKind() const { return lastSnapKind_; }
    uint32_t entitiesPlaced() const { return entitiesPlaced_; }
    bool gestureActive() const { return pointerId_ >= 0; }
    bool polylineInProgress() const { return polylineInProgress_; }
    // The snapped point the pointer is currently at, valid while a gesture
    // is active.
    const SketchPoint& cursor() const { return cursor_; }

private:
    struct SnapResult {
        SketchPoint point;
        SketchSnapKind kind = SketchSnapKind::None;
    };

    // The Ready-state half of onTouch: the extrude arrow, on the gizmo one-
    // pointer contract. Returns true when the manipulator consumed the event.
    bool onExtrudeTouch(TouchAction action, int32_t actionPointerId, const TouchPointer* pointers,
                        int count, const CameraSnapshot& camera, int viewportWidth,
                        int viewportHeight);

    // THE one writer of `extrude_`: every typed value, drag sample, Flip and
    // extent change lands here and passes one validation.
    CadStatus applyExtrudeFeature(const ExtrudeFeature& requested);

    // Puts a cancelled drag's own SIDE back to the distance it started from.
    bool restoreCancelledExtrudeDrag();

    bool pointerToSketch(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                         int viewportHeight, SnapResult* out);
    SnapResult snap(const SketchPoint& raw, double snapWorld) const;
    SketchEntityId hitTest(const SketchPoint& point, double toleranceWorld) const;
    void placeFromDrag();
    void placeArcThrough(const SketchPoint& through);
    void placePolylineVertex(const SnapResult& at);
    void touchOverlay() { ++overlayRevision_; overlayDirty_ = true; }
    // An authored change: the overlay AND the candidate are stale.
    void touchCandidate() {
        ++candidateRevision_;
        touchOverlay();
    }
    // The selection, finished: keep a still-valid one, otherwise auto-select
    // the single selectable region, otherwise none.
    void reconcileRegionSelection();
    // The base point of the arrow for the current selection, in sketch (u, v).
    bool selectionAnchorPoint(SketchPoint* out) const;
    CadStatus commitIntoTarget(ConstructionScene& scene, ConstructionHistory& history,
                               ObjectId* outId);
    CadStatus checkDependentsKeepTheirFaces(const ConstructionScene& scene, ObjectId producerId,
                                            const CadBodyState& candidate,
                                            const CadBodyMesh& candidateMesh) const;
    CadStatus fail(CadStatus why) { lastStatus_ = why; return why; }
    void reconcilePlanarSelection();
    CadStatus unchosenStatus() const;
    void setPlanarSelection(std::vector<PlanarFaceRef> faces);
    void clearPlanarSelection();
    std::vector<size_t> selectedPlanarFaceIndices() const;
    bool planarSelectionAnchor(SketchPoint* out) const;
    void buildOverlay(float worldPerUnit, const CadExtrudeViewFacts& view);

    SketchSessionState state_ = SketchSessionState::Inactive;
    SketchTool tool_ = SketchTool::Rectangle;
    CadSketch sketch_;
    // The world frame the sketch is authored on. See SketchFrame.
    SketchFrame frame_;
    // The current adaptive minor grid step, in metres. Updated from the camera
    // at pointer-down and when the overlay is rebuilt; never persisted.
    double gridStep_ = kSketchGridSpacingMeters;
    SketchEntityId selectedEntityId_ = kNoSketchEntity;

    SketchRegionExtraction regions_;
    // `CAD-V6-S2`: the arrangement derived at Finish and, when the session is
    // selecting planar faces, one shape per atomic face (its own one-face union)
    // for hit testing, labels and the hatch. Derived; cleared with `regions_`.
    SketchArrangement arrangement_;
    std::vector<PlanarProfileComponent> faceShapes_;
    bool selectionLost_ = false;
    ExtrudeFeature extrude_;
    // `CAD-VERTICAL-SLICE-R1`. What the extrusion does; see `setOperation`.
    CadFeatureOperation operation_ = CadFeatureOperation::NewBody;
    // The body an Add/Cut applies to (a new face sketch's producer) or an edit
    // rewrites, and its authored state as staged when the session opened.
    ObjectId targetBodyId_ = kNoObject;
    CadBodyState targetBaseState_;
    bool hasTargetState_ = false;
    // An edit session's feature (0 when authoring a new one). The sketch's
    // placement is not staged here: it belongs to the sketch RECORD in the
    // staged state, which an edit rewrites the entities of and nothing else.
    uint32_t editingFeatureId_ = 0;
    // Bumped by every authored change that could change the candidate.
    uint64_t candidateRevision_ = 1;
    CadCandidateEvaluation evaluation_;
    // A Ready-state tap that may pick a region: armed on a single-pointer Down
    // that misses the arrow, disarmed by travel or a second pointer.
    bool regionTapArmed_ = false;
    int32_t regionTapPointer_ = -1;
    float regionTapX_ = 0.0f;
    float regionTapY_ = 0.0f;
    // The camera as it stood at the tap's Down: the tap resolves its cell
    // against THIS and the Down pixel, so nothing the camera did since can
    // move which cell the finger meant (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`).
    CameraSnapshot regionTapCamera_{};
    // The Down landed on the drawn arrow HEAD: a still tap there is the
    // manipulator's and is recorded as such on Up.
    bool regionTapOnHead_ = false;
    SketchTapOutcome lastTapOutcome_ = SketchTapOutcome::None;
    uint32_t tapOutcomeSerial_ = 0;
    void recordTapOutcome(SketchTapOutcome outcome) {
        lastTapOutcome_ = outcome;
        ++tapOutcomeSerial_;
    }
    // Volatile transition intent (`CAD-EXT-R1`). See the accessor.
    ExtrudeDirection oneSideDirection_ = ExtrudeDirection::AlongNormal;
    // The canvas manipulator (`CAD-UX-S1`). Volatile like everything else here,
    // and alive only in Ready: while the sketch is being DRAWN the single
    // finger belongs to the drawing, and an arrow that competed with it would
    // be a second meaning for one gesture.
    CadExtrudeManipulator extrudeDrag_;

    // The one captured pointer, or -1.
    int32_t pointerId_ = -1;
    float downX_ = 0.0f;
    float downY_ = 0.0f;
    bool travelled_ = false;
    bool dragValid_ = false;
    SketchPoint anchor_;   // where the gesture started, snapped
    SketchPoint cursor_;   // where it is now, snapped
    // World metres per reference unit at the plane, sampled at pointer down,
    // so a drag's tolerances are fixed for its whole life.
    double worldPerUnit_ = 0.0;

    // The multi-tap point run. ONE buffer, shared by the Polyline and the
    // Spline tools, because only one multi-tap entity can be in progress at a
    // time and two buffers would be two answers to what is being placed. Which
    // entity it becomes is decided by the tool held when the run ENDS, and a
    // tool change ends the run before it changes the tool.
    bool polylineInProgress_ = false;
    std::vector<SketchPoint> polylineVertices_;

    // The Arc tool's two-step gesture: a drag sets the chord, then one tap sets
    // the point the arc passes through. Dropped by a tool change, a cancel and
    // a second pointer, exactly as a drag is.
    bool arcPending_ = false;
    SketchPoint arcStart_;
    SketchPoint arcEnd_;

    // The orientation navigator's PRESENTATION state. Never persisted; see the
    // accessors.
    bool viewFlipped_ = false;
    int viewQuarterTurns_ = 0;

    // The body this session is editing, or kNoObject when it is authoring a new
    // one. See beginEdit.
    ObjectId editingBodyId_ = kNoObject;

    CadStatus lastStatus_ = CadStatus::Ok;
    SketchSnapKind lastSnapKind_ = SketchSnapKind::None;
    uint32_t entitiesPlaced_ = 0;

    uint64_t overlayRevision_ = 0;
    bool overlayDirty_ = true;
    float overlayWorldPerUnit_ = 0.0f;
    CadExtrudeViewFacts overlayView_{};
    std::shared_ptr<SketchOverlay> overlay_;
};

// The one process-scoped sketch session. Like `gizmoSession()`: exactly one
// sketch can be in progress in the product, and the JNI layer holds no
// sketch state of its own.
SketchSession& sketchSession();

}  // namespace forgeshape
