#include "forgeshape_picking_selftest.h"

#include <cmath>

#include "forgeshape_camera.h"
#include "forgeshape_demo_mesh.h"
#include "forgeshape_math.h"
#include "forgeshape_picking.h"
#include "forgeshape_selection.h"

namespace forgeshape {
namespace {

constexpr int kTestViewportWidth = 1080;
constexpr int kTestViewportHeight = 2400;
constexpr float kCenterX = kTestViewportWidth * 0.5f;
constexpr float kCenterY = kTestViewportHeight * 0.5f;

struct Recorder {
    PickingSelfTestResult* out;
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

bool nearly(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

float length(const Vec3& v) { return std::sqrt(vec3Dot(v, v)); }

CameraController makeController() {
    CameraController c;
    c.setViewport(kTestViewportWidth, kTestViewportHeight);
    return c;
}

// --- touch helpers, mirroring what the JNI layer feeds both controllers ------

bool sendDown(SelectionController& s, int32_t id, float x, float y, float* px, float* py) {
    TouchPointer p{id, x, y};
    return s.onTouch(TouchAction::Down, -1, &p, 1, px, py);
}

bool sendMove(SelectionController& s, int32_t id, float x, float y, float* px, float* py) {
    TouchPointer p{id, x, y};
    return s.onTouch(TouchAction::Move, -1, &p, 1, px, py);
}

bool sendUp(SelectionController& s, int32_t id, float x, float y, float* px, float* py) {
    TouchPointer p{id, x, y};
    return s.onTouch(TouchAction::Up, id, &p, 1, px, py);
}

bool sendPointerDown(SelectionController& s, const TouchPointer& a, const TouchPointer& b) {
    TouchPointer p[2]{a, b};
    return s.onTouch(TouchAction::PointerDown, -1, p, 2, nullptr, nullptr);
}

bool sendMove2(SelectionController& s, const TouchPointer& a, const TouchPointer& b) {
    TouchPointer p[2]{a, b};
    return s.onTouch(TouchAction::Move, -1, p, 2, nullptr, nullptr);
}

bool sendPointerUp(SelectionController& s, const TouchPointer& a, const TouchPointer& b,
                   int32_t lifting) {
    TouchPointer p[2]{a, b};
    return s.onTouch(TouchAction::PointerUp, lifting, p, 2, nullptr, nullptr);
}

// ---------------------------------------------------------------------------
// Ray construction
// ---------------------------------------------------------------------------
void testRayConstruction(Recorder& r) {
    CameraController c = makeController();
    const CameraSnapshot cam = c.snapshot();

    Ray center{};
    const bool centerOk = buildPickRay(cam, kCenterX, kCenterY, kTestViewportWidth,
                                       kTestViewportHeight, &center);
    r.check("ray_center_builds", centerOk);
    r.check("ray_center_origin_finite", centerOk && vec3Finite(center.origin));
    r.check("ray_center_direction_finite", centerOk && vec3Finite(center.direction));
    r.check("ray_center_direction_normalized",
            centerOk && nearly(length(center.direction), 1.0f, 1e-4f));
    r.check("ray_origin_is_camera_eye",
            centerOk && nearly(center.origin.x, cam.eye.x, 1e-5f) &&
                nearly(center.origin.y, cam.eye.y, 1e-5f) &&
                nearly(center.origin.z, cam.eye.z, 1e-5f));

    // The centre pixel must look straight at the orbit target.
    const Vec3 toTarget = vec3Normalize(vec3Sub(cam.target, cam.eye));
    r.check("ray_center_points_at_target",
            centerOk && vec3Dot(center.direction, toTarget) > 0.9999f);

    // Camera basis rows of the view matrix, used for the corner sign checks.
    const Vec3 camRight{cam.view.m[0], cam.view.m[4], cam.view.m[8]};
    const Vec3 camUp{cam.view.m[1], cam.view.m[5], cam.view.m[9]};

    Ray topLeft{};
    const bool tlOk = buildPickRay(cam, 0.0f, 0.0f, kTestViewportWidth, kTestViewportHeight,
                                   &topLeft);
    r.check("ray_top_left_builds", tlOk);
    r.check("ray_top_left_points_left", tlOk && vec3Dot(topLeft.direction, camRight) < 0.0f);
    r.check("ray_top_left_points_up", tlOk && vec3Dot(topLeft.direction, camUp) > 0.0f);

    Ray bottomRight{};
    const bool brOk = buildPickRay(cam, static_cast<float>(kTestViewportWidth),
                                   static_cast<float>(kTestViewportHeight),
                                   kTestViewportWidth, kTestViewportHeight, &bottomRight);
    r.check("ray_bottom_right_builds", brOk);
    r.check("ray_bottom_right_points_right",
            brOk && vec3Dot(bottomRight.direction, camRight) > 0.0f);
    r.check("ray_bottom_right_points_down",
            brOk && vec3Dot(bottomRight.direction, camUp) < 0.0f);
    r.check("ray_corners_are_distinct",
            tlOk && brOk && vec3Dot(topLeft.direction, bottomRight.direction) < 0.9999f);

    // Same NDC point, different aspect -> different ray.
    CameraController portrait = makeController();
    Ray portraitRay{};
    const bool pOk = buildPickRay(portrait.snapshot(), kTestViewportWidth * 0.75f,
                                  kCenterY, kTestViewportWidth, kTestViewportHeight,
                                  &portraitRay);
    CameraController landscape;
    landscape.setViewport(kTestViewportHeight, kTestViewportWidth);
    Ray landscapeRay{};
    const bool lOk = buildPickRay(landscape.snapshot(), kTestViewportHeight * 0.75f,
                                  kTestViewportWidth * 0.5f, kTestViewportHeight,
                                  kTestViewportWidth, &landscapeRay);
    r.check("ray_aspect_change_changes_direction",
            pOk && lOk && vec3Dot(portraitRay.direction, landscapeRay.direction) < 0.9999f);

    // Invalid inputs must fail closed, never produce a NaN ray.
    Ray unused{};
    r.check("ray_zero_width_rejected",
            !buildPickRay(cam, kCenterX, kCenterY, 0, kTestViewportHeight, &unused));
    r.check("ray_zero_height_rejected",
            !buildPickRay(cam, kCenterX, kCenterY, kTestViewportWidth, 0, &unused));
    r.check("ray_negative_viewport_rejected",
            !buildPickRay(cam, kCenterX, kCenterY, -8, -8, &unused));
    r.check("ray_nonfinite_screen_point_rejected",
            !buildPickRay(cam, NAN, kCenterY, kTestViewportWidth, kTestViewportHeight, &unused));
    r.check("ray_null_out_rejected",
            !buildPickRay(cam, kCenterX, kCenterY, kTestViewportWidth, kTestViewportHeight,
                          nullptr));

    CameraSnapshot degenerate = cam;
    degenerate.proj = Mat4{};
    r.check("ray_degenerate_projection_rejected",
            !buildPickRay(degenerate, kCenterX, kCenterY, kTestViewportWidth,
                          kTestViewportHeight, &unused));
}

// ---------------------------------------------------------------------------
// Ray / triangle
// ---------------------------------------------------------------------------
void testTriangleIntersection(Recorder& r) {
    // CCW seen from +Z, so its canonical outward normal is +Z.
    const Vec3 a{-1.0f, -1.0f, 0.0f};
    const Vec3 b{1.0f, -1.0f, 0.0f};
    const Vec3 c{0.0f, 1.0f, 0.0f};

    const Ray down{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}};

    float t = 0.0f;
    const bool frontHit = intersectRayTriangle(down, a, b, c, true, &t);
    r.check("tri_front_face_hit", frontHit);
    r.check("tri_front_hit_distance", frontHit && nearly(t, 5.0f, 1e-4f));
    r.check("tri_front_hit_distance_positive_finite",
            frontHit && std::isfinite(t) && t > 0.0f);

    // Same triangle wound the other way is back-facing to this ray.
    float tb = 0.0f;
    r.check("tri_back_face_rejected_when_culling",
            !intersectRayTriangle(down, a, c, b, true, &tb));
    r.check("tri_back_face_accepted_without_culling",
            intersectRayTriangle(down, a, c, b, false, &tb) && nearly(tb, 5.0f, 1e-4f));

    // Miss: same plane, ray far outside the triangle.
    const Ray offset{{20.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}};
    float tm = 0.0f;
    r.check("tri_miss_outside_triangle", !intersectRayTriangle(offset, a, b, c, true, &tm));

    // Behind the origin.
    const Ray away{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 1.0f}};
    float tbehind = 0.0f;
    r.check("tri_behind_origin_rejected", !intersectRayTriangle(away, a, b, c, false, &tbehind));

    // Parallel to the plane.
    const Ray parallel{{0.0f, 0.0f, 5.0f}, {1.0f, 0.0f, 0.0f}};
    float tp = 0.0f;
    r.check("tri_parallel_rejected", !intersectRayTriangle(parallel, a, b, c, false, &tp));

    // Degenerate triangles.
    float td = 0.0f;
    r.check("tri_degenerate_point_safe", !intersectRayTriangle(down, a, a, a, false, &td));
    r.check("tri_degenerate_collinear_safe",
            !intersectRayTriangle(down, Vec3{-1.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f},
                                  Vec3{1.0f, 0.0f, 0.0f}, false, &td));

    // Non-finite input.
    const Ray bad{{0.0f, 0.0f, NAN}, {0.0f, 0.0f, -1.0f}};
    float tn = 0.0f;
    r.check("tri_nonfinite_ray_rejected", !intersectRayTriangle(bad, a, b, c, false, &tn));

    // Nearest of several hits wins, regardless of mesh order.
    const MeshVertex stacked[9] = {
        {{-1.0f, -1.0f, 0.0f}, {0, 0, 0}}, {{1.0f, -1.0f, 0.0f}, {0, 0, 0}},
        {{0.0f, 1.0f, 0.0f}, {0, 0, 0}},
        {{-1.0f, -1.0f, 2.0f}, {0, 0, 0}}, {{1.0f, -1.0f, 2.0f}, {0, 0, 0}},
        {{0.0f, 1.0f, 2.0f}, {0, 0, 0}},
        {{-1.0f, -1.0f, -3.0f}, {0, 0, 0}}, {{1.0f, -1.0f, -3.0f}, {0, 0, 0}},
        {{0.0f, 1.0f, -3.0f}, {0, 0, 0}},
    };
    const uint32_t stackedIndices[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    TriangleMeshView view{};
    view.positions = stacked[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = 9;
    view.indices = stackedIndices;
    view.indexCount = 9;

    const TriangleHit nearest = pickTriangleMesh(down, view, true);
    r.check("mesh_nearest_hit_found", nearest.hit);
    r.check("mesh_nearest_hit_is_closest", nearest.hit && nearly(nearest.t, 3.0f, 1e-4f));
    r.check("mesh_nearest_hit_triangle_index", nearest.hit && nearest.triangleIndex == 1);
    r.check("mesh_hit_position_on_ray",
            nearest.hit && nearly(nearest.position.z, 2.0f, 1e-4f) &&
                nearly(nearest.position.x, 0.0f, 1e-4f));

    // Malformed views must miss rather than read out of bounds.
    TriangleMeshView empty{};
    r.check("mesh_null_view_safe", !pickTriangleMesh(down, empty, true).hit);

    TriangleMeshView ragged = view;
    ragged.indexCount = 8;  // not a multiple of three
    r.check("mesh_ragged_index_count_safe", !pickTriangleMesh(down, ragged, true).hit);

    const uint32_t outOfRange[3] = {0, 1, 400};
    TriangleMeshView bogus = view;
    bogus.indices = outOfRange;
    bogus.indexCount = 3;
    r.check("mesh_out_of_range_index_safe", !pickTriangleMesh(down, bogus, true).hit);
}

// ---------------------------------------------------------------------------
// Canonical winding / facing
// ---------------------------------------------------------------------------
void testWinding(Recorder& r) {
    const TriangleMeshView cube = demoCubeMeshView();
    r.check("cube_mesh_view_valid",
            cube.positions != nullptr && cube.indices != nullptr && cube.indexCount == 36 &&
                cube.vertexCount == 8);
    r.check("cube_winding_is_canonical",
            meshObeysCanonicalWinding(cube, Vec3{0.0f, 0.0f, 0.0f}));

    // A reversed copy must be rejected, proving the check has teeth.
    uint32_t reversed[36];
    for (uint32_t tri = 0; tri < 12; ++tri) {
        reversed[tri * 3 + 0] = cube.indices[tri * 3 + 0];
        reversed[tri * 3 + 1] = cube.indices[tri * 3 + 2];
        reversed[tri * 3 + 2] = cube.indices[tri * 3 + 1];
    }
    TriangleMeshView flipped = cube;
    flipped.indices = reversed;
    r.check("cube_reversed_winding_detected",
            !meshObeysCanonicalWinding(flipped, Vec3{0.0f, 0.0f, 0.0f}));

    // Front-facing agreement: from outside, along each axis, the cube is hit
    // with front-face-only picking.
    const Vec3 axes[6] = {{5.0f, 0.0f, 0.0f},  {-5.0f, 0.0f, 0.0f}, {0.0f, 5.0f, 0.0f},
                          {0.0f, -5.0f, 0.0f}, {0.0f, 0.0f, 5.0f},  {0.0f, 0.0f, -5.0f}};
    bool allAxesHit = true;
    for (const Vec3& origin : axes) {
        Ray ray{origin, vec3Normalize(vec3Scale(origin, -1.0f))};
        const TriangleHit hit = pickTriangleMesh(ray, cube, true);
        if (!hit.hit || !nearly(hit.t, 5.0f - kDemoCubeHalfExtent, 1e-3f)) {
            allAxesHit = false;
        }
    }
    r.check("cube_front_faces_hit_from_all_six_sides", allAxesHit);

    // From inside the cube only back faces are visible, so front-face-only
    // picking must miss while unculled picking still hits. This is the check
    // that ties the picking convention to VK_CULL_MODE_BACK_BIT.
    const Ray inside{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    r.check("cube_interior_ray_misses_front_faces", !pickTriangleMesh(inside, cube, true).hit);
    r.check("cube_interior_ray_hits_without_culling",
            pickTriangleMesh(inside, cube, false).hit);
}

// ---------------------------------------------------------------------------
// Demo cube through the real camera
// ---------------------------------------------------------------------------
void testDemoCubePicking(Recorder& r) {
    CameraController c = makeController();

    const SceneHit center = pickScene(c.snapshot(), kCenterX, kCenterY, kTestViewportWidth,
                                      kTestViewportHeight);
    r.check("scene_center_hits_cube", center.hit);
    r.check("scene_center_object_id", center.hit && center.objectId == kDemoCubeObjectId);
    r.check("scene_center_triangle_index_valid",
            center.hit && center.triangleIndex >= 0 && center.triangleIndex < 12);
    r.check("scene_center_distance_positive_finite",
            center.hit && std::isfinite(center.distance) && center.distance > 0.0f);
    r.check("scene_center_hit_in_front_of_target",
            center.hit && center.distance < kInitialDistance);
    r.check("scene_center_position_on_cube_surface",
            center.hit && std::fabs(center.position.x) <= kDemoCubeHalfExtent + 1e-3f &&
                std::fabs(center.position.y) <= kDemoCubeHalfExtent + 1e-3f &&
                std::fabs(center.position.z) <= kDemoCubeHalfExtent + 1e-3f);

    const SceneHit background =
        pickScene(c.snapshot(), 5.0f, 5.0f, kTestViewportWidth, kTestViewportHeight);
    r.check("scene_background_corner_misses", !background.hit);
    r.check("scene_miss_has_no_object_id", background.objectId == kNoObject);

    // Deterministically move the camera through the real gesture path, then
    // pick again: the same object id must come back.
    TouchPointer p{1, 400.0f, 1200.0f};
    c.onTouch(TouchAction::Down, -1, &p, 1);
    p.x = 900.0f;
    p.y = 1400.0f;
    c.onTouch(TouchAction::Move, -1, &p, 1);
    c.onTouch(TouchAction::Up, 1, &p, 1);

    const bool poseChanged = !nearly(c.yaw(), kInitialYaw, 1e-3f) ||
                             !nearly(c.pitch(), kInitialPitch, 1e-3f);
    r.check("scene_camera_pose_actually_changed", poseChanged);

    const SceneHit afterOrbit = pickScene(c.snapshot(), kCenterX, kCenterY, kTestViewportWidth,
                                          kTestViewportHeight);
    r.check("scene_hits_cube_after_camera_move", afterOrbit.hit);
    r.check("scene_object_id_stable_after_camera_move",
            afterOrbit.hit && afterOrbit.objectId == center.objectId);
    r.check("scene_distance_valid_after_camera_move",
            afterOrbit.hit && std::isfinite(afterOrbit.distance) && afterOrbit.distance > 0.0f);

    // Zoomed in, the centre pixel still lands on the cube.
    TouchPointer a{1, 400.0f, 1000.0f};
    TouchPointer b2{2, 700.0f, 1000.0f};
    c.onTouch(TouchAction::Down, -1, &a, 1);
    TouchPointer pair[2]{a, b2};
    c.onTouch(TouchAction::PointerDown, -1, pair, 2);
    pair[0].x = 300.0f;
    pair[1].x = 800.0f;
    c.onTouch(TouchAction::Move, -1, pair, 2);
    c.onTouch(TouchAction::Cancel, -1, pair, 2);
    const SceneHit afterZoom = pickScene(c.snapshot(), kCenterX, kCenterY, kTestViewportWidth,
                                         kTestViewportHeight);
    r.check("scene_hits_cube_after_zoom", afterZoom.hit);
    r.check("scene_object_id_stable_after_zoom",
            afterZoom.hit && afterZoom.objectId == kDemoCubeObjectId);

    r.check("scene_zero_viewport_safe",
            !pickScene(c.snapshot(), kCenterX, kCenterY, 0, 0).hit);
}

// ---------------------------------------------------------------------------
// Tap candidacy and selection ownership
// ---------------------------------------------------------------------------
void testTapAndSelection(Recorder& r) {
    float x = 0.0f, y = 0.0f;

    {   // Small down/up is a tap, and reports the release position.
        SelectionController s;
        r.check("tap_down_does_not_pick", !sendDown(s, 3, 500.0f, 1200.0f, &x, &y));
        const bool tapped = sendUp(s, 3, 504.0f, 1203.0f, &x, &y);
        r.check("tap_small_down_up_picks", tapped);
        r.check("tap_reports_release_position",
                tapped && nearly(x, 504.0f, 1e-3f) && nearly(y, 1203.0f, 1e-3f));
    }

    {   // Movement past the slop radius permanently cancels the tap.
        SelectionController s;
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        sendMove(s, 1, 500.0f + kTapSlopPixels + 10.0f, 1200.0f, &x, &y);
        r.check("tap_cancelled_by_drag", !s.tapCandidateActive());
        r.check("tap_drag_release_does_not_pick", !sendUp(s, 1, 500.0f, 1200.0f, &x, &y));
    }

    {   // Total displacement from DOWN is what counts, not per-move deltas: many
        // small moves that never individually exceed the slop still cancel.
        SelectionController s;
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        for (int i = 1; i <= 10; ++i) {
            sendMove(s, 1, 500.0f + static_cast<float>(i) * 5.0f, 1200.0f, &x, &y);
        }
        r.check("tap_uses_total_displacement_from_down", !s.tapCandidateActive());
        r.check("tap_creeping_drag_does_not_pick", !sendUp(s, 1, 550.0f, 1200.0f, &x, &y));
    }

    {   // Movement inside the slop radius keeps the tap alive.
        SelectionController s;
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        sendMove(s, 1, 505.0f, 1205.0f, &x, &y);
        sendMove(s, 1, 502.0f, 1198.0f, &x, &y);
        r.check("tap_survives_small_jitter", s.tapCandidateActive());
        r.check("tap_jitter_release_picks", sendUp(s, 1, 503.0f, 1201.0f, &x, &y));
    }

    {   // A release beyond the slop radius never picks, even with no MOVE.
        SelectionController s;
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        r.check("tap_far_release_does_not_pick",
                !sendUp(s, 1, 500.0f + kTapSlopPixels + 50.0f, 1200.0f, &x, &y));
    }

    {   // Multi-touch permanently cancels, even after dropping back to one finger.
        SelectionController s;
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        TouchPointer a{1, 500.0f, 1200.0f};
        TouchPointer b{2, 700.0f, 1200.0f};
        sendPointerDown(s, a, b);
        r.check("tap_cancelled_by_second_pointer", !s.tapCandidateActive());
        sendMove2(s, a, b);
        sendPointerUp(s, a, b, 2);
        r.check("tap_multitouch_release_does_not_pick",
                !sendUp(s, 1, 500.0f, 1200.0f, &x, &y));
    }

    {   // Pointer identity is authoritative: an event carrying a different id
        // must not be mistaken for the tracked pointer.
        SelectionController s;
        sendDown(s, 7, 500.0f, 1200.0f, &x, &y);
        r.check("tap_foreign_pointer_move_cancels",
                !sendMove(s, 9, 500.0f, 1200.0f, &x, &y) && !s.tapCandidateActive());
        r.check("tap_foreign_pointer_release_does_not_pick",
                !sendUp(s, 9, 500.0f, 1200.0f, &x, &y));
    }

    {   // A high, non-zero pointer id taps just as well as id 0.
        SelectionController s;
        sendDown(s, 42, 300.0f, 900.0f, &x, &y);
        r.check("tap_nonzero_pointer_id_picks", sendUp(s, 42, 302.0f, 902.0f, &x, &y));
    }

    {   // ACTION_CANCEL kills the tap and never changes selection.
        SelectionController s;
        s.setSelected(kDemoCubeObjectId);
        const uint32_t before = s.changeCount();
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        TouchPointer p{1, 500.0f, 1200.0f};
        s.onTouch(TouchAction::Cancel, -1, &p, 1, &x, &y);
        r.check("tap_cancel_clears_candidate", !s.tapCandidateActive());
        r.check("tap_cancel_release_does_not_pick", !sendUp(s, 1, 500.0f, 1200.0f, &x, &y));
        r.check("tap_cancel_preserves_selection",
                s.selected() == kDemoCubeObjectId && s.changeCount() == before);
    }

    {   // A cancelled gesture leaves no stale state for the next one.
        SelectionController s;
        sendDown(s, 1, 100.0f, 100.0f, &x, &y);
        sendMove(s, 1, 900.0f, 1800.0f, &x, &y);
        sendUp(s, 1, 900.0f, 1800.0f, &x, &y);
        const bool tapped = sendDown(s, 2, 500.0f, 1200.0f, &x, &y) ||
                            sendUp(s, 2, 501.0f, 1201.0f, &x, &y);
        r.check("tap_next_gesture_has_no_stale_state", tapped);
        r.check("tap_next_gesture_reports_own_position",
                nearly(x, 501.0f, 1e-3f) && nearly(y, 1201.0f, 1e-3f));
    }

    {   // resetGesture (Surface swap) drops candidacy but not selection.
        SelectionController s;
        s.setSelected(kDemoCubeObjectId);
        sendDown(s, 1, 500.0f, 1200.0f, &x, &y);
        s.resetGesture();
        r.check("reset_gesture_clears_candidate", !s.tapCandidateActive());
        r.check("reset_gesture_preserves_selection", s.selected() == kDemoCubeObjectId);
        r.check("reset_gesture_release_does_not_pick", !sendUp(s, 1, 500.0f, 1200.0f, &x, &y));
    }

    {   // A malformed event never picks and never crashes.
        SelectionController s;
        r.check("tap_null_pointers_safe",
                !s.onTouch(TouchAction::Down, -1, nullptr, 0, &x, &y));
        TouchPointer nan{1, NAN, NAN};
        r.check("tap_nonfinite_down_rejected",
                !s.onTouch(TouchAction::Down, -1, &nan, 1, &x, &y) && !s.tapCandidateActive());
    }

    {   // Selection identity semantics.
        SelectionController s;
        r.check("selection_starts_empty", !s.hasSelection() && s.selected() == kNoObject);

        SceneHit hit{};
        hit.hit = true;
        hit.objectId = kDemoCubeObjectId;
        r.check("selection_pick_hit_selects", s.applyPick(hit));
        r.check("selection_holds_stable_id", s.selected() == kDemoCubeObjectId);
        r.check("selection_is_selected_query", s.isSelected(kDemoCubeObjectId));
        r.check("selection_no_object_never_selected", !s.isSelected(kNoObject));
        r.check("selection_reselect_is_not_a_change", !s.applyPick(hit));

        const SceneHit miss{};
        r.check("selection_pick_miss_clears", s.applyPick(miss));
        r.check("selection_cleared", !s.hasSelection() && s.selected() == kNoObject);
        r.check("selection_clear_when_empty_is_not_a_change", !s.applyPick(miss));
        r.check("selection_change_count_counts_changes", s.changeCount() == 2);
    }
}

}  // namespace

int runPickingSelfTests(PickingSelfTestResult* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) {
        return 0;
    }
    Recorder r{out, maxOut};
    testRayConstruction(r);
    testTriangleIntersection(r);
    testWinding(r);
    testDemoCubePicking(r);
    testTapAndSelection(r);
    return r.n;
}

}  // namespace forgeshape
