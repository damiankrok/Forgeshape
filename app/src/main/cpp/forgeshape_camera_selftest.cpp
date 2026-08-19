#include "forgeshape_camera_selftest.h"

#include <cmath>

#include "forgeshape_camera.h"

namespace forgeshape {
namespace {

struct Recorder {
    CameraSelfTestResult* out;
    int max;
    int n = 0;

    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

// Convenience wrappers so each test reads like the gesture it simulates.
void down(CameraController& c, int32_t id, float x, float y) {
    TouchPointer p{id, x, y};
    c.onTouch(TouchAction::Down, -1, &p, 1);
}

void move1(CameraController& c, int32_t id, float x, float y) {
    TouchPointer p{id, x, y};
    c.onTouch(TouchAction::Move, -1, &p, 1);
}

void up1(CameraController& c, int32_t id, float x, float y) {
    TouchPointer p{id, x, y};
    c.onTouch(TouchAction::Up, id, &p, 1);
}

void pointerDown2(CameraController& c, const TouchPointer& a, const TouchPointer& b) {
    TouchPointer p[2]{a, b};
    c.onTouch(TouchAction::PointerDown, -1, p, 2);
}

void move2(CameraController& c, const TouchPointer& a, const TouchPointer& b) {
    TouchPointer p[2]{a, b};
    c.onTouch(TouchAction::Move, -1, p, 2);
}

void pointerUp2(CameraController& c, const TouchPointer& a, const TouchPointer& b,
                int32_t lifting) {
    TouchPointer p[2]{a, b};
    c.onTouch(TouchAction::PointerUp, lifting, p, 2);
}

CameraController makeController() {
    CameraController c;
    c.setViewport(1080, 2400);
    return c;
}

bool snapshotFinite(const CameraController& c) {
    const CameraSnapshot s = c.snapshot();
    return mat4Finite(s.view) && mat4Finite(s.proj) && vec3Finite(s.eye) &&
           vec3Finite(s.target) && std::isfinite(s.yaw) && std::isfinite(s.pitch) &&
           std::isfinite(s.distance);
}

}  // namespace

int runCameraSelfTests(CameraSelfTestResult* out, int maxOut) {
    Recorder rec{out, maxOut};

    // 1. A freshly constructed camera must already produce usable matrices.
    {
        CameraController c = makeController();
        rec.check("initial_matrices_finite", snapshotFinite(c));
        const CameraSnapshot s = c.snapshot();
        rec.check("initial_distance_matches_eye",
                  std::fabs(std::sqrt((s.eye.x - s.target.x) * (s.eye.x - s.target.x) +
                                      (s.eye.y - s.target.y) * (s.eye.y - s.target.y) +
                                      (s.eye.z - s.target.z) * (s.eye.z - s.target.z)) -
                            s.distance) < 1e-3f);
    }

    // 2. Horizontal drag direction: dragging right decreases yaw.
    {
        CameraController c = makeController();
        const float before = c.yaw();
        down(c, 0, 540.0f, 1200.0f);
        move1(c, 0, 740.0f, 1200.0f);
        const float expected = before - 200.0f * kOrbitRadiansPerPixel;
        rec.check("orbit_drag_right_decreases_yaw", c.yaw() < before - 0.5f);
        rec.check("orbit_yaw_magnitude", std::fabs(c.yaw() - expected) < 1e-3f);
        rec.check("orbit_target_unchanged",
                  c.target().x == 0.0f && c.target().y == 0.0f && c.target().z == 0.0f);
    }

    // 3. Vertical drag direction: dragging down increases pitch.
    {
        CameraController c = makeController();
        const float before = c.pitch();
        down(c, 0, 540.0f, 1000.0f);
        move1(c, 0, 540.0f, 1120.0f);
        rec.check("orbit_drag_down_increases_pitch", c.pitch() > before + 0.4f);
    }

    // 4. Pitch clamps at both extremes and never degenerates the up-vector.
    {
        CameraController c = makeController();
        down(c, 0, 540.0f, 100.0f);
        for (int i = 0; i < 40; ++i) {
            move1(c, 0, 540.0f, 100.0f + static_cast<float>(i + 1) * 200.0f);
        }
        rec.check("pitch_clamp_upper", c.pitch() <= kPitchLimitRadians + 1e-4f &&
                                           c.pitch() >= kPitchLimitRadians - 1e-4f);
        rec.check("pitch_clamp_upper_finite", snapshotFinite(c));

        CameraController d = makeController();
        down(d, 0, 540.0f, 9000.0f);
        for (int i = 0; i < 40; ++i) {
            move1(d, 0, 540.0f, 9000.0f - static_cast<float>(i + 1) * 200.0f);
        }
        rec.check("pitch_clamp_lower", d.pitch() >= -kPitchLimitRadians - 1e-4f &&
                                           d.pitch() <= -kPitchLimitRadians + 1e-4f);
        rec.check("pitch_clamp_lower_finite", snapshotFinite(d));
    }

    // 5. Yaw wraps rather than growing without bound.
    {
        CameraController c = makeController();
        down(c, 0, 0.0f, 1200.0f);
        for (int i = 0; i < 60; ++i) {
            move1(c, 0, static_cast<float>(i + 1) * 400.0f, 1200.0f);
        }
        rec.check("yaw_wraps_bounded", std::fabs(c.yaw()) <= 3.1416f);
    }

    // 6. Pinch: spreading zooms in (distance shrinks), pinching zooms out.
    {
        CameraController c = makeController();
        down(c, 0, 440.0f, 1200.0f);
        pointerDown2(c, TouchPointer{0, 440.0f, 1200.0f}, TouchPointer{1, 640.0f, 1200.0f});
        const float before = c.distance();
        move2(c, TouchPointer{0, 340.0f, 1200.0f}, TouchPointer{1, 740.0f, 1200.0f});
        rec.check("pinch_spread_decreases_distance", c.distance() < before - 0.1f);
        const float spread = c.distance();
        move2(c, TouchPointer{0, 440.0f, 1200.0f}, TouchPointer{1, 640.0f, 1200.0f});
        rec.check("pinch_close_increases_distance", c.distance() > spread + 0.1f);
        rec.check("pinch_finite", snapshotFinite(c));
    }

    // 7. Distance clamps at both ends.
    {
        CameraController c = makeController();
        down(c, 0, 100.0f, 1200.0f);
        pointerDown2(c, TouchPointer{0, 100.0f, 1200.0f}, TouchPointer{1, 140.0f, 1200.0f});
        for (int i = 0; i < 60; ++i) {
            move2(c, TouchPointer{0, 100.0f, 1200.0f},
                  TouchPointer{1, 140.0f + static_cast<float>(i + 1) * 300.0f, 1200.0f});
        }
        rec.check("distance_clamp_min", c.distance() >= kMinDistance - 1e-4f &&
                                            c.distance() <= kMinDistance + 1e-3f);

        CameraController d = makeController();
        down(d, 0, 100.0f, 1200.0f);
        pointerDown2(d, TouchPointer{0, 100.0f, 1200.0f}, TouchPointer{1, 20000.0f, 1200.0f});
        for (int i = 0; i < 60; ++i) {
            move2(d, TouchPointer{0, 100.0f, 1200.0f},
                  TouchPointer{1, 20000.0f - static_cast<float>(i + 1) * 300.0f, 1200.0f});
        }
        rec.check("distance_clamp_max", d.distance() <= kMaxDistance + 1e-2f &&
                                            d.distance() >= kMaxDistance - 1e-1f);
        rec.check("distance_clamp_finite", snapshotFinite(c) && snapshotFinite(d));
    }

    // 8. Two-finger centroid translation pans the target in the camera plane.
    {
        CameraController c = makeController();
        down(c, 0, 400.0f, 1000.0f);
        pointerDown2(c, TouchPointer{0, 400.0f, 1000.0f}, TouchPointer{1, 600.0f, 1000.0f});
        const Vec3 before = c.target();
        const float distBefore = c.distance();
        // Pure translation: both pointers move together so span is unchanged.
        move2(c, TouchPointer{0, 550.0f, 1000.0f}, TouchPointer{1, 750.0f, 1000.0f});
        const Vec3 after = c.target();
        const float moved = std::sqrt((after.x - before.x) * (after.x - before.x) +
                                      (after.y - before.y) * (after.y - before.y) +
                                      (after.z - before.z) * (after.z - before.z));
        rec.check("pan_moves_target", moved > 0.05f);
        rec.check("pan_pure_translation_keeps_distance",
                  std::fabs(c.distance() - distBefore) < 1e-3f);
        rec.check("pan_result_finite", vec3Finite(after) && snapshotFinite(c));
        // Pan must not change the orbit angles.
        rec.check("pan_does_not_orbit",
                  std::fabs(c.yaw() - kInitialYaw) < 1e-5f &&
                      std::fabs(c.pitch() - kInitialPitch) < 1e-5f);
    }

    // 9. Pan scaling follows camera distance: farther camera pans farther.
    {
        CameraController nearCam = makeController();
        down(nearCam, 0, 400.0f, 1000.0f);
        pointerDown2(nearCam, TouchPointer{0, 400.0f, 1000.0f}, TouchPointer{1, 600.0f, 1000.0f});
        move2(nearCam, TouchPointer{0, 500.0f, 1000.0f}, TouchPointer{1, 700.0f, 1000.0f});
        const Vec3 nearT = nearCam.target();
        const float nearMoved = std::sqrt(nearT.x * nearT.x + nearT.y * nearT.y + nearT.z * nearT.z);

        CameraController farCam = makeController();
        // Zoom out first, then perform the identical pan.
        down(farCam, 0, 100.0f, 1200.0f);
        pointerDown2(farCam, TouchPointer{0, 100.0f, 1200.0f}, TouchPointer{1, 900.0f, 1200.0f});
        move2(farCam, TouchPointer{0, 400.0f, 1200.0f}, TouchPointer{1, 600.0f, 1200.0f});
        up1(farCam, 0, 400.0f, 1200.0f);
        farCam.resetGesture();
        const Vec3 farBase = farCam.target();
        down(farCam, 0, 400.0f, 1000.0f);
        pointerDown2(farCam, TouchPointer{0, 400.0f, 1000.0f}, TouchPointer{1, 600.0f, 1000.0f});
        move2(farCam, TouchPointer{0, 500.0f, 1000.0f}, TouchPointer{1, 700.0f, 1000.0f});
        const Vec3 farT = farCam.target();
        const float farMoved = std::sqrt((farT.x - farBase.x) * (farT.x - farBase.x) +
                                         (farT.y - farBase.y) * (farT.y - farBase.y) +
                                         (farT.z - farBase.z) * (farT.z - farBase.z));
        rec.check("pan_scales_with_distance", farCam.distance() > nearCam.distance() &&
                                                  farMoved > nearMoved * 1.5f);
    }

    // 10. 1 -> 2 pointer transition must not move the camera at all.
    {
        CameraController c = makeController();
        down(c, 0, 300.0f, 1000.0f);
        move1(c, 0, 360.0f, 1000.0f);
        const float yaw = c.yaw(), pitch = c.pitch(), dist = c.distance();
        const Vec3 tgt = c.target();
        // Second finger lands far away: a naive centroid/span implementation
        // would jump here.
        pointerDown2(c, TouchPointer{0, 360.0f, 1000.0f}, TouchPointer{1, 1000.0f, 2000.0f});
        rec.check("transition_1_to_2_no_jump",
                  c.yaw() == yaw && c.pitch() == pitch && c.distance() == dist &&
                      tgt.x == c.target().x && tgt.y == c.target().y && tgt.z == c.target().z);
    }

    // 11. 2 -> 1 pointer transition must not move the camera, and the following
    //     single-finger move must produce a bounded delta.
    {
        CameraController c = makeController();
        down(c, 0, 300.0f, 1000.0f);
        pointerDown2(c, TouchPointer{0, 300.0f, 1000.0f}, TouchPointer{1, 900.0f, 1600.0f});
        move2(c, TouchPointer{0, 320.0f, 1010.0f}, TouchPointer{1, 920.0f, 1610.0f});
        const float yaw = c.yaw(), pitch = c.pitch(), dist = c.distance();
        const Vec3 tgt = c.target();
        pointerUp2(c, TouchPointer{0, 320.0f, 1010.0f}, TouchPointer{1, 920.0f, 1610.0f}, 1);
        rec.check("transition_2_to_1_no_jump",
                  c.yaw() == yaw && c.pitch() == pitch && c.distance() == dist &&
                      tgt.x == c.target().x && tgt.y == c.target().y && tgt.z == c.target().z);
        // The surviving pointer is re-anchored at 320,1010 so a 10 px move must
        // orbit by exactly 10 px worth of rotation, not by the 600 px gap.
        move1(c, 0, 330.0f, 1010.0f);
        rec.check("transition_2_to_1_bounded_delta",
                  std::fabs(c.yaw() - (yaw - 10.0f * kOrbitRadiansPerPixel)) < 1e-4f);
    }

    // 12. Pointer index reordering must not disturb the gesture.
    {
        CameraController c = makeController();
        down(c, 0, 300.0f, 1000.0f);
        pointerDown2(c, TouchPointer{0, 300.0f, 1000.0f}, TouchPointer{1, 700.0f, 1000.0f});
        move2(c, TouchPointer{0, 350.0f, 1000.0f}, TouchPointer{1, 750.0f, 1000.0f});
        const Vec3 orderedTarget = c.target();
        const float orderedDist = c.distance();

        CameraController d = makeController();
        down(d, 0, 300.0f, 1000.0f);
        pointerDown2(d, TouchPointer{0, 300.0f, 1000.0f}, TouchPointer{1, 700.0f, 1000.0f});
        // Same physical gesture, pointers delivered in the opposite index order.
        move2(d, TouchPointer{1, 750.0f, 1000.0f}, TouchPointer{0, 350.0f, 1000.0f});
        const Vec3 swappedTarget = d.target();

        rec.check("pointer_index_reorder_stable",
                  std::fabs(orderedTarget.x - swappedTarget.x) < 1e-5f &&
                      std::fabs(orderedTarget.y - swappedTarget.y) < 1e-5f &&
                      std::fabs(orderedTarget.z - swappedTarget.z) < 1e-5f &&
                      std::fabs(orderedDist - d.distance()) < 1e-5f);
    }

    // 13. CANCEL drops gesture tracking but keeps the camera pose.
    {
        CameraController c = makeController();
        down(c, 0, 300.0f, 1000.0f);
        move1(c, 0, 400.0f, 1000.0f);
        const float yaw = c.yaw();
        rec.check("gesture_active_during_drag", c.gestureActive());
        TouchPointer p{0, 400.0f, 1000.0f};
        c.onTouch(TouchAction::Cancel, -1, &p, 1);
        rec.check("cancel_clears_gesture_state",
                  !c.gestureActive() && c.trackedPointerCount() == 0);
        rec.check("cancel_preserves_camera", c.yaw() == yaw);
        // A stale delta must not leak into the next gesture.
        down(c, 0, 900.0f, 1000.0f);
        rec.check("cancel_no_stale_delta", c.yaw() == yaw);
        move1(c, 0, 910.0f, 1000.0f);
        rec.check("post_cancel_delta_bounded",
                  std::fabs(c.yaw() - (yaw - 10.0f * kOrbitRadiansPerPixel)) < 1e-4f);
    }

    // 14. Lifting the last pointer clears tracking and leaves no stale anchor.
    {
        CameraController c = makeController();
        down(c, 0, 300.0f, 1000.0f);
        move1(c, 0, 400.0f, 1000.0f);
        const float yaw = c.yaw();
        up1(c, 0, 400.0f, 1000.0f);
        rec.check("last_pointer_up_clears_gesture", !c.gestureActive());
        down(c, 0, 50.0f, 1000.0f);
        rec.check("new_gesture_no_stale_delta", c.yaw() == yaw);
    }

    // 15. Aspect follows the viewport, and a degenerate viewport is survivable.
    {
        CameraController c = makeController();
        const CameraSnapshot portrait = c.snapshot();
        c.setViewport(2400, 1080);
        const CameraSnapshot landscape = c.snapshot();
        rec.check("projection_follows_aspect",
                  std::fabs(portrait.proj.m[0] - landscape.proj.m[0]) > 1e-3f &&
                      mat4Finite(landscape.proj));
        c.setViewport(0, 0);
        rec.check("degenerate_viewport_finite", snapshotFinite(c));
    }

    // 16. Everything stays finite after a long mixed gesture stream.
    {
        CameraController c = makeController();
        for (int i = 0; i < 200; ++i) {
            const float f = static_cast<float>(i);
            down(c, 0, 100.0f + f, 200.0f + f * 3.0f);
            move1(c, 0, 100.0f + f * 7.0f, 200.0f - f * 5.0f);
            pointerDown2(c, TouchPointer{0, 100.0f + f, 200.0f},
                         TouchPointer{1, 900.0f - f, 1800.0f});
            move2(c, TouchPointer{0, 120.0f + f, 260.0f},
                  TouchPointer{1, 700.0f - f, 1500.0f});
            pointerUp2(c, TouchPointer{0, 120.0f + f, 260.0f},
                       TouchPointer{1, 700.0f - f, 1500.0f}, 1);
            up1(c, 0, 120.0f + f, 260.0f);
        }
        rec.check("mixed_gesture_stream_finite", snapshotFinite(c));
        rec.check("mixed_gesture_stream_distance_in_range",
                  c.distance() >= kMinDistance - 1e-4f && c.distance() <= kMaxDistance + 1e-2f);
        rec.check("mixed_gesture_stream_pitch_in_range",
                  std::fabs(c.pitch()) <= kPitchLimitRadians + 1e-4f);
    }

    return rec.n;
}

}  // namespace forgeshape
