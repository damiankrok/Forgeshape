// ForgeShape native camera / navigation owner.
//
// This module owns the entire camera truth for the viewport:
//   - orbit target, yaw, pitch, distance;
//   - projection parameters and the viewport aspect;
//   - the touch gesture state machine that turns raw pointer positions into
//     orbit / pan / zoom.
//
// It deliberately contains NO JNI, NO Android and NO Vulkan types so that the
// Android layer stays a dumb pointer forwarder and the renderer stays a dumb
// snapshot consumer.
#pragma once

#include <cstdint>

#include "forgeshape_input.h"
#include "forgeshape_math.h"

namespace forgeshape {

// How the camera projects the world onto the viewport.
//
// A closed enum and a switch, deliberately — the same rule the sculpt tools and
// the shading models follow. Two projections do not justify a camera framework,
// a projection registry or a plugin surface.
//
// This is CAMERA/PRESENTATION state, never geometry truth. Changing it mints no
// MeshRevision and no SculptRevision, moves no vertex, touches no Construction
// parameter, no PrimitiveKind, no transform and no ObjectId. It changes which
// pixels a fixed piece of geometry lands on, and nothing else.
enum class ProjectionMode {
    // Standard pinhole projection: near parts of a solid are drawn larger, and
    // parallel edges converge. The product default.
    Perspective,
    // True parallel projection: equal lengths parallel to the image plane are
    // drawn at equal size at every depth, and parallel edges stay parallel. This
    // is the mode in which exact Construction geometry can be judged, because
    // nothing is enlarged merely for being closer.
    Orthographic,
};

constexpr int kProjectionModeCount = 2;

// Perspective, so the viewport comes up in the natural, depth-cued view.
constexpr ProjectionMode kDefaultProjectionMode = ProjectionMode::Perspective;

const char* projectionModeName(ProjectionMode mode);

// Maps the UI's index onto the enum. Out-of-range is REFUSED rather than
// clamped, matching shadingModelFromIndex and sculptToolFromIndex: an unknown
// mode is a caller bug, not a value to repair.
bool projectionModeFromIndex(int index, ProjectionMode* out);
int projectionModeIndex(ProjectionMode mode);

// Immutable, self-consistent camera state handed to the renderer once per frame.
struct CameraSnapshot {
    Mat4 view;
    Mat4 proj;

    // The world-space origin of the view: the point every pick ray starts from
    // and the point `view` translates to the origin.
    //
    // In Perspective this is the pinhole itself, `target + orbitDirection *
    // distance`. In Orthographic it is the centre of the near face of the view
    // slab, which is pulled back to kOrthoViewPlaneDistance so that the whole
    // depth range sits in front of it (see kOrthoViewPlaneDistance). A parallel
    // projection is invariant to translation along the view axis, so that
    // pull-back changes no pixel — it exists so clipping and picking agree with
    // what is drawn. It therefore does NOT equal `target + orbitDirection *
    // distance` in Orthographic, and nothing may assume it does.
    Vec3 eye;

    Vec3 target;
    float yaw;
    float pitch;

    // The ORBIT pose distance, target -> orbit eye. It is the pose radius in
    // both modes and is what orbit must leave alone; in Orthographic it does not
    // set the visible scale (orthoHalfHeightMeters does) and does not place
    // `eye`.
    float distance;

    // Which of the two projections `proj` actually is, so downstream code can
    // branch without re-deriving it from the matrix.
    ProjectionMode projection;

    // Half the world-space height the viewport shows, in METERS, measured at the
    // target plane. Meaningful in Orthographic, where it is the visible scale;
    // in Perspective it carries the equivalent framing (distance * tan(fovY / 2))
    // so the value is always a live, physically interpretable span rather than a
    // stale leftover.
    float orthoHalfHeightMeters;
};

// ---------------------------------------------------------------------------
// Accepted user-visible navigation constants (Stage 004 gesture contract).
// ---------------------------------------------------------------------------

// Orbit sensitivity, radians of rotation per pixel of finger travel.
// 0.005 rad/px means a full-width (1080 px) drag sweeps ~5.4 rad (~310 deg).
constexpr float kOrbitRadiansPerPixel = 0.005f;

// Pitch is clamped just short of +/-90 deg so the lookAt up-vector can never
// become parallel to the view direction.
constexpr float kPitchLimitRadians = 1.52f;  // ~87.1 deg

// Pinch sensitivity. Distance is scaled multiplicatively by
// exp(-deltaSpanPixels * kPinchLogPerPixel), so zooming is scale-invariant:
// the same finger travel always changes distance by the same ratio.
constexpr float kPinchLogPerPixel = 0.0035f;

// Hard camera distance clamps. The minimum keeps the eye from crossing the
// target; the maximum keeps the scene inside the far plane.
constexpr float kMinDistance = 0.35f;
constexpr float kMaxDistance = 400.0f;

// Projection parameters owned by the camera.
constexpr float kFovYRadians = 1.0471976f;  // 60 deg
constexpr float kNearPlane = 0.05f;
constexpr float kFarPlane = 500.0f;

// ---------------------------------------------------------------------------
// Orthographic scale and depth
// ---------------------------------------------------------------------------

// Hard clamps on the orthographic visible half-height, in METERS. The minimum
// shows a 4 cm tall slice of the world, which is enough to work on a small
// detail; the maximum frames a 500 m tall object. The pair brackets the same
// range of apparent sizes the perspective distance clamps do — at the 60 deg
// field of view, kMinDistance and kMaxDistance correspond to half-heights of
// about 0.20 m and 231 m, so neither mode can reach a framing the other cannot.
constexpr float kMinOrthoHalfHeightMeters = 0.02f;
constexpr float kMaxOrthoHalfHeightMeters = 250.0f;

// Where the orthographic view plane sits, in meters in front of the target
// along the orbit direction.
//
// A parallel projection produces the same image from anywhere on the view axis,
// so this is free to be generous, and being generous is the point: with the
// view plane kFarPlane/2 in front of the target, the depth slab [kNearPlane,
// kFarPlane] is centred on the target and reaches 250 m either side of it.
// Nothing the user can frame gets sliced by the near plane, and every drawn
// surface is in front of the pick-ray origin — so what is pickable stays what
// is drawn, which is the same invariant the front-face rule protects.
//
// Ortho depth is linear, so a 500 m slab costs no precision worth naming
// (about 30 um per depth-buffer step at 24 bits) — unlike a perspective frustum,
// where the same range would be ruinous.
constexpr float kOrthoViewPlaneDistance = kFarPlane * 0.5f;

// The default orthographic framing, chosen to match what Perspective shows at
// kInitialDistance so the very first switch does not move the frame.
// Recomputed rather than stored on every real switch; this is only the value a
// process starts with.
constexpr float kInitialOrthoHalfHeightMeters = 4.7343f;  // 8.2 * tan(30 deg)

// Initial framing, chosen so the Stage 003 cube stays fully framed on a tall
// portrait surface.
constexpr float kInitialYaw = 0.7f;
constexpr float kInitialPitch = 0.5f;
constexpr float kInitialDistance = 8.2f;

class CameraController {
public:
    CameraController();

    // Viewport size in pixels. Drives projection aspect and pan scaling.
    void setViewport(int width, int height);

    // The size the camera is currently projecting for.
    //
    // Read-back, not a second truth: the camera already had to keep it for the
    // aspect, and anything that turns a world point into a pixel — the gizmo's
    // hit test and its screen-constant scale — needs the SAME number the
    // projection was built from. A second copy of the viewport size kept
    // anywhere else is a copy that can be one event out of date.
    int viewportWidth() const { return viewportWidth_; }
    int viewportHeight() const { return viewportHeight_; }

    // Places the orbit pose directly, clamped exactly as a gesture would clamp
    // it, and returns whether the request was usable.
    //
    // VERIFICATION ONLY. Nothing in the product calls it and no UI reaches it:
    // its whole purpose is to let a case say "from THIS viewpoint" — an axis
    // nearly edge-on to the viewer, a camera very close, a camera very far —
    // without first having to synthesise an orbit gesture of exactly the right
    // pixel length, which would make the case a test of gesture arithmetic
    // rather than of the thing it is about. The JNI entry point that exposes it
    // is compiled out of a release build.
    //
    // The gesture counters are deliberately NOT advanced: placing the camera is
    // not the user orbiting it, and a case that asserts "this drag did not
    // orbit" must not be defeated by its own setup.
    bool setPose(float yaw, float pitch, float distance);

    // Feeds one complete touch event. `pointers` must describe every pointer
    // currently present in the event, including the one that is lifting on
    // PointerUp / Up (identified by `actionPointerId`, or -1 when unused).
    void onTouch(TouchAction action, int32_t actionPointerId,
                 const TouchPointer* pointers, int count);

    // Drops all gesture tracking without disturbing the camera pose. Called on
    // ACTION_CANCEL and whenever the Surface goes away.
    void resetGesture();

    // Restores the initial pose. Not wired to any gesture; used by self-tests.
    void resetCamera();

    // Everything a sketch view has to put back when it ends: the pose, the
    // projection and the orthographic span. Plain data so the sketch owner can
    // hold it without holding a camera.
    struct Pose {
        Vec3 target{0.0f, 0.0f, 0.0f};
        float yaw = kInitialYaw;
        float pitch = kInitialPitch;
        float distance = kInitialDistance;
        ProjectionMode projection = kDefaultProjectionMode;
        float orthoHalfHeightMeters = kInitialOrthoHalfHeightMeters;
    };

    Pose capturePose() const;

    // Puts a captured pose back exactly. PRODUCT functionality, unlike
    // setPose: it is how leaving a sketch returns the user to the view they
    // had. Clamped as a gesture would be, so a stale pose cannot put the
    // camera anywhere the product cannot reach. Gesture counters are not
    // advanced.
    void restorePose(const Pose& pose);

    // Looks straight at a workplane: the given orbit angles, the target at the
    // world origin, the current orbit distance kept, and the ORTHOGRAPHIC
    // projection, so equal sketch lengths are equal on screen wherever they
    // fall (`CAD-R0-A1A2`). PRODUCT functionality on the same terms as
    // restorePose. The sketch itself never reads the camera: this only decides
    // what the user is looking through.
    void frameWorkplane(float yaw, float pitch);

    // Looks EXACTLY along a sketch frame's normal, orthographically, with no
    // pitch-clamp approximation (`CAD-A3` B3). Unlike frameWorkplane it does
    // not go through the orbit yaw/pitch -- which cannot represent a straight
    // -down view without gimbal lock -- but installs the frame directly: the
    // view is `lookAt(origin + n*D, origin, v)`, so `u` is exactly screen-right
    // and `v` exactly screen-up. Used for BOTH a world-plane sketch (the
    // plane's frame at the origin) and a face sketch (the producer's face frame
    // in world space). Pan slides the target in `u`/`v`; pinch changes the
    // orthographic span; orbit is inert. Presentation only: cleared by
    // restorePose, frameWorkplane or resetCamera.
    void frameSketchView(const Vec3& origin, const Vec3& u, const Vec3& v, const Vec3& n);

    // Whether a sketch-frame view is currently installed.
    bool sketchViewActive() const { return sketchView_; }

    // Switches the projection, PRESERVING THE FRAMING at the target plane.
    //
    // The two modes describe the same visible span in different terms, so the
    // switch converts between them instead of resetting:
    //
    //   Perspective -> Orthographic:  orthoHalfHeight = distance * tan(fovY / 2)
    //   Orthographic -> Perspective:  distance = orthoHalfHeight / tan(fovY / 2)
    //
    // Both are the same identity read in opposite directions, so a round trip
    // returns to where it started (up to the distance clamps). The target, the
    // yaw and the pitch are never touched, so the frame keeps its centre and its
    // viewing direction and the object cannot jump or vanish.
    //
    // Returns true only when the mode actually changed, so a caller can log a
    // real transition rather than reporting a no-op as one.
    bool setProjectionMode(ProjectionMode mode);

    CameraSnapshot snapshot() const;

    // --- introspection (logging and self-tests only) ---
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return distance_; }
    Vec3 target() const { return target_; }
    ProjectionMode projectionMode() const { return projection_; }
    float orthoHalfHeightMeters() const { return orthoHalfHeightMeters_; }
    bool gestureActive() const { return mode_ != Mode::None; }
    int trackedPointerCount() const;

    // Monotonic counters, used to emit a one-shot log token per gesture kind.
    uint32_t orbitCount() const { return orbitCount_; }
    uint32_t panCount() const { return panCount_; }
    uint32_t zoomCount() const { return zoomCount_; }

private:
    enum class Mode { None, Orbit, TwoFinger };

    void applyOrbit(float dx, float dy);
    void applyPan(float dx, float dy);
    void applyZoom(float spanDelta);

    // World meters per screen pixel at the target plane, for the ACTIVE
    // projection. Pan is authored in pixels and has to be resolved in world
    // units, and the two modes resolve it differently: in Perspective the span
    // grows with distance, in Orthographic it is the ortho half-height and
    // distance does not enter. Returns false if the result is not usable.
    bool worldPerPixelAtTargetPlane(float* out) const;

    // Orthonormal camera basis for the current pose.
    Vec3 orbitDirection() const;  // unit vector target -> eye
    void cameraBasis(Vec3* right, Vec3* up) const;

    // CAD-A3 sketch-frame view. When active the snapshot is built from this
    // world frame instead of the orbit pose, so the view is exactly normal to
    // the sketch with no pitch-clamp and no gimbal singularity. `target_` is
    // the in-plane look-at point (pan moves it along svU_/svV_).
    bool sketchView_ = false;
    Vec3 svU_{1.0f, 0.0f, 0.0f};
    Vec3 svV_{0.0f, 1.0f, 0.0f};
    Vec3 svN_{0.0f, 0.0f, 1.0f};

    Vec3 target_{0.0f, 0.0f, 0.0f};
    float yaw_ = kInitialYaw;
    float pitch_ = kInitialPitch;
    float distance_ = kInitialDistance;

    // Projection state. Process-scoped exactly as the pose above is: the one
    // CameraController instance outlives every Surface and every Activity, which
    // is why the chosen projection and its framing survive a HOME/resume and a
    // surface recreation with no save/restore code in the Android layer.
    ProjectionMode projection_ = kDefaultProjectionMode;
    float orthoHalfHeightMeters_ = kInitialOrthoHalfHeightMeters;

    int viewportWidth_ = 1;
    int viewportHeight_ = 1;

    Mode mode_ = Mode::None;
    int32_t idA_ = -1;
    int32_t idB_ = -1;
    float lastX_ = 0.0f;   // single-pointer anchor / two-pointer centroid
    float lastY_ = 0.0f;
    float lastSpan_ = 0.0f;

    uint32_t orbitCount_ = 0;
    uint32_t panCount_ = 0;
    uint32_t zoomCount_ = 0;
};

}  // namespace forgeshape
