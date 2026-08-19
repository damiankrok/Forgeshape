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

// Immutable, self-consistent camera state handed to the renderer once per frame.
struct CameraSnapshot {
    Mat4 view;
    Mat4 proj;
    Vec3 eye;
    Vec3 target;
    float yaw;
    float pitch;
    float distance;
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

    CameraSnapshot snapshot() const;

    // --- introspection (logging and self-tests only) ---
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return distance_; }
    Vec3 target() const { return target_; }
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

    // Orthonormal camera basis for the current pose.
    Vec3 orbitDirection() const;  // unit vector target -> eye
    void cameraBasis(Vec3* right, Vec3* up) const;

    Vec3 target_{0.0f, 0.0f, 0.0f};
    float yaw_ = kInitialYaw;
    float pitch_ = kInitialPitch;
    float distance_ = kInitialDistance;

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
