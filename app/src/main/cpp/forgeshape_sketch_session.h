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
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_overlay.h"

namespace forgeshape {

enum class SketchSessionState : uint8_t {
    Inactive,
    Editing,
    Ready,
};

const char* sketchSessionStateName(SketchSessionState state);

// The rail's five entries. Select is a tool so that a tap in the viewport has
// exactly one meaning at a time: with a drawing tool held it places geometry,
// with Select held it chooses an entity.
enum class SketchTool : uint8_t {
    Select,
    Line,
    Polyline,
    Rectangle,
    Circle,
};

constexpr int kSketchToolCount = 5;

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
    CadStatus beginOnFace(const SketchFrame& worldFrame, const TopoRef& support);

    // The world authoring frame the camera should look normal to. Valid while
    // active; the plane's frame at the origin for a world-plane sketch, the
    // producer's face frame for a face sketch.
    const SketchFrame& frame() const { return frame_; }

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

    const ProfileExtraction& profiles() const { return profiles_; }
    CadStatus selectProfile(SketchEntityId anchorEntityId);
    SketchEntityId selectedProfileId() const { return extrude_.profileEntityId; }

    CadStatus setExtrude(Meters depth, ExtrudeDirection direction);
    const ExtrudeFeature& extrude() const { return extrude_; }

    // THE commit. Ready -> Inactive on success, with the new body's id in
    // `outId`. One transaction; a refusal changes nothing and stays in Ready.
    CadStatus commit(ConstructionScene& scene, ConstructionHistory& history, ObjectId* outId);

    // The state a commit WOULD build, for the extrude preview and the tests.
    CadBodyState candidateState() const;

    // --- overlay ---------------------------------------------------------

    // The current overlay, rebuilt only when something changed. Empty while
    // inactive. `worldPerUnit` sizes the snap marker; it is the world length
    // of one reference unit at the plane, which the caller derives from the
    // camera exactly as the gizmo does.
    SketchOverlayPtr overlay(float worldPerUnit);
    uint64_t overlayRevision() const { return overlayRevision_; }

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

    bool pointerToSketch(const CameraSnapshot& camera, float x, float y, int viewportWidth,
                         int viewportHeight, SnapResult* out);
    SnapResult snap(const SketchPoint& raw, double snapWorld) const;
    SketchEntityId hitTest(const SketchPoint& point, double toleranceWorld) const;
    void placeFromDrag();
    void placePolylineVertex(const SnapResult& at);
    void touchOverlay() { ++overlayRevision_; overlayDirty_ = true; }
    CadStatus fail(CadStatus why) { lastStatus_ = why; return why; }
    void buildOverlay(float worldPerUnit);

    SketchSessionState state_ = SketchSessionState::Inactive;
    SketchTool tool_ = SketchTool::Rectangle;
    CadSketch sketch_;
    // The world frame the sketch is authored on. See SketchFrame.
    SketchFrame frame_;
    // The current adaptive minor grid step, in metres. Updated from the camera
    // at pointer-down and when the overlay is rebuilt; never persisted.
    double gridStep_ = kSketchGridSpacingMeters;
    SketchEntityId selectedEntityId_ = kNoSketchEntity;

    ProfileExtraction profiles_;
    ExtrudeFeature extrude_;

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

    bool polylineInProgress_ = false;
    std::vector<SketchPoint> polylineVertices_;

    CadStatus lastStatus_ = CadStatus::Ok;
    SketchSnapKind lastSnapKind_ = SketchSnapKind::None;
    uint32_t entitiesPlaced_ = 0;

    uint64_t overlayRevision_ = 0;
    bool overlayDirty_ = true;
    float overlayWorldPerUnit_ = 0.0f;
    std::shared_ptr<SketchOverlay> overlay_;
};

// The one process-scoped sketch session. Like `gizmoSession()`: exactly one
// sketch can be in progress in the product, and the JNI layer holds no
// sketch state of its own.
SketchSession& sketchSession();

}  // namespace forgeshape
