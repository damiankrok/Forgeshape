#include "forgeshape_camera.h"

#include <cmath>

namespace forgeshape {
namespace {

constexpr float kTwoPi = 6.2831853f;

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Keeps yaw in [-pi, pi] so it can wrap forever without losing precision.
float wrapAngle(float a) {
    if (!std::isfinite(a)) {
        return 0.0f;
    }
    while (a > 3.14159265f) a -= kTwoPi;
    while (a < -3.14159265f) a += kTwoPi;
    return a;
}

// Sorted-by-id copy of the pointers that are still down. The lifting pointer of
// a PointerUp / Up event is excluded, so the controller always sees the set that
// will still exist after the event. Sorting by id makes the controller immune to
// MotionEvent pointer-index reordering.
int collectActive(TouchAction action, int32_t actionPointerId,
                  const TouchPointer* pointers, int count, TouchPointer* out) {
    const bool dropAction = (action == TouchAction::PointerUp || action == TouchAction::Up);
    int n = 0;
    for (int i = 0; i < count && n < kMaxTrackedPointers; ++i) {
        if (dropAction && pointers[i].id == actionPointerId) {
            continue;
        }
        if (!std::isfinite(pointers[i].x) || !std::isfinite(pointers[i].y)) {
            continue;
        }
        // Insertion sort by pointer id.
        int j = n++;
        while (j > 0 && out[j - 1].id > pointers[i].id) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = pointers[i];
    }
    return n;
}

float spanOf(const TouchPointer& a, const TouchPointer& b) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

CameraController::CameraController() = default;

void CameraController::setViewport(int width, int height) {
    viewportWidth_ = width > 0 ? width : 1;
    viewportHeight_ = height > 0 ? height : 1;
}

void CameraController::resetGesture() {
    mode_ = Mode::None;
    idA_ = -1;
    idB_ = -1;
    lastX_ = 0.0f;
    lastY_ = 0.0f;
    lastSpan_ = 0.0f;
}

void CameraController::resetCamera() {
    target_ = Vec3{0.0f, 0.0f, 0.0f};
    yaw_ = kInitialYaw;
    pitch_ = kInitialPitch;
    distance_ = kInitialDistance;
    orbitCount_ = 0;
    panCount_ = 0;
    zoomCount_ = 0;
    resetGesture();
}

int CameraController::trackedPointerCount() const {
    switch (mode_) {
        case Mode::None: return 0;
        case Mode::Orbit: return 1;
        case Mode::TwoFinger: return 2;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Gesture state machine
//
// Every event recomputes the active pointer set. Whenever that set differs from
// the tracked one the controller RE-ANCHORS and applies no delta, which is what
// makes 1->2 and 2->1 transitions jump-free. Deltas are only ever applied on
// Move events whose pointer set is unchanged.
// ---------------------------------------------------------------------------
void CameraController::onTouch(TouchAction action, int32_t actionPointerId,
                               const TouchPointer* pointers, int count) {
    if (action == TouchAction::Cancel) {
        resetGesture();
        return;
    }
    if (pointers == nullptr || count <= 0) {
        resetGesture();
        return;
    }

    TouchPointer active[kMaxTrackedPointers];
    const int n = collectActive(action, actionPointerId, pointers, count, active);

    if (n == 0) {
        resetGesture();
        return;
    }

    if (n == 1) {
        const TouchPointer& p = active[0];
        if (mode_ != Mode::Orbit || idA_ != p.id) {
            mode_ = Mode::Orbit;
            idA_ = p.id;
            idB_ = -1;
            lastX_ = p.x;
            lastY_ = p.y;
            lastSpan_ = 0.0f;
            return;  // re-anchor only, no camera change
        }
        if (action == TouchAction::Move) {
            applyOrbit(p.x - lastX_, p.y - lastY_);
            lastX_ = p.x;
            lastY_ = p.y;
        }
        return;
    }

    // Two or more pointers: the two lowest ids drive pan + pinch together.
    const TouchPointer& a = active[0];
    const TouchPointer& b = active[1];
    const float cx = (a.x + b.x) * 0.5f;
    const float cy = (a.y + b.y) * 0.5f;
    const float span = spanOf(a, b);

    if (mode_ != Mode::TwoFinger || idA_ != a.id || idB_ != b.id) {
        mode_ = Mode::TwoFinger;
        idA_ = a.id;
        idB_ = b.id;
        lastX_ = cx;
        lastY_ = cy;
        lastSpan_ = span;
        return;  // re-anchor only, no camera change
    }

    if (action == TouchAction::Move) {
        applyPan(cx - lastX_, cy - lastY_);
        applyZoom(span - lastSpan_);
        lastX_ = cx;
        lastY_ = cy;
        lastSpan_ = span;
    }
}

// ---------------------------------------------------------------------------
// Camera math
// ---------------------------------------------------------------------------

Vec3 CameraController::orbitDirection() const {
    const float cp = std::cos(pitch_);
    return Vec3{cp * std::sin(yaw_), std::sin(pitch_), cp * std::cos(yaw_)};
}

void CameraController::cameraBasis(Vec3* right, Vec3* up) const {
    const Vec3 dir = orbitDirection();               // target -> eye
    const Vec3 forward = vec3Scale(dir, -1.0f);      // eye -> target
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};
    // Safe because pitch is clamped strictly inside +/-90 deg, so `forward` is
    // never parallel to worldUp.
    const Vec3 r = vec3Normalize(vec3Cross(forward, worldUp));
    *right = r;
    *up = vec3Cross(r, forward);
}

// Grab-the-object convention: drag right spins the model right (yaw decreases),
// drag down tips the model toward the viewer (pitch increases / camera rises).
void CameraController::applyOrbit(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) {
        return;
    }
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }
    yaw_ = wrapAngle(yaw_ - dx * kOrbitRadiansPerPixel);
    pitch_ = clampf(pitch_ + dy * kOrbitRadiansPerPixel, -kPitchLimitRadians, kPitchLimitRadians);
    ++orbitCount_;
}

// The target slides in the camera plane so the world appears to follow the
// fingers. One pixel of finger travel maps to exactly one pixel of world travel
// measured at the target plane, which is what makes pan feel identical at every
// zoom level and on every viewport size.
void CameraController::applyPan(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) {
        return;
    }
    if (dx == 0.0f && dy == 0.0f) {
        return;
    }
    const float worldPerPixel =
        (2.0f * distance_ * std::tan(kFovYRadians * 0.5f)) / static_cast<float>(viewportHeight_);
    if (!std::isfinite(worldPerPixel)) {
        return;
    }

    Vec3 right, up;
    cameraBasis(&right, &up);

    // Screen +x is world -right and screen +y (downward) is world +up, so the
    // scene tracks the fingers instead of running away from them.
    Vec3 moved = vec3Add(target_, vec3Scale(right, -dx * worldPerPixel));
    moved = vec3Add(moved, vec3Scale(up, dy * worldPerPixel));
    if (vec3Finite(moved)) {
        target_ = moved;
        ++panCount_;
    }
}

// Multiplicative, so the distance can never reach or cross zero and the same
// finger travel always produces the same zoom ratio.
void CameraController::applyZoom(float spanDelta) {
    if (!std::isfinite(spanDelta) || spanDelta == 0.0f) {
        return;
    }
    const float scale = std::exp(-spanDelta * kPinchLogPerPixel);
    if (!std::isfinite(scale) || scale <= 0.0f) {
        return;
    }
    const float next = distance_ * scale;
    if (!std::isfinite(next)) {
        return;
    }
    distance_ = clampf(next, kMinDistance, kMaxDistance);
    ++zoomCount_;
}

CameraSnapshot CameraController::snapshot() const {
    CameraSnapshot s{};
    s.yaw = yaw_;
    s.pitch = pitch_;
    s.distance = distance_;
    s.target = target_;
    s.eye = vec3Add(target_, vec3Scale(orbitDirection(), distance_));
    s.view = mat4LookAt(s.eye, s.target, Vec3{0.0f, 1.0f, 0.0f});

    const float aspect =
        static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
    s.proj = mat4Perspective(kFovYRadians, aspect > 0.0f ? aspect : 1.0f, kNearPlane, kFarPlane);
    return s;
}

}  // namespace forgeshape
