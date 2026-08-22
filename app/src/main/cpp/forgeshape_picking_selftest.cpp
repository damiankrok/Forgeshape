#include "forgeshape_picking_selftest.h"

#include <cmath>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_demo_mesh.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
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

    // This suite can rerun within one process after Activity recreation (the
    // render thread is stopped and restarted between NativeViewport.stop()/
    // start() calls), by which point the live process-scoped MeshStore may
    // hold whatever primitive/transform a prior UI test last applied, not the
    // demo cube these checks are written against. Publish the known demo cube
    // fixture directly and pick with an explicit identity transform, so this
    // test is self-contained and its result cannot depend on ConstructionObject
    // or ConstructionTransform state left over from anything else.
    meshStore().publish(demoCubeVertices(), demoCubeVertexCount(), demoCubeIndices(),
                        demoCubeIndexCount());
    const Mat4 identity = mat4Identity();

    const SceneHit center = pickScene(c.snapshot(), kCenterX, kCenterY, kTestViewportWidth,
                                      kTestViewportHeight, identity, identity, true);
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

    const SceneHit background = pickScene(c.snapshot(), 5.0f, 5.0f, kTestViewportWidth,
                                          kTestViewportHeight, identity, identity, true);
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
                                          kTestViewportHeight, identity, identity, true);
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
                                         kTestViewportHeight, identity, identity, true);
    r.check("scene_hits_cube_after_zoom", afterZoom.hit);
    r.check("scene_object_id_stable_after_zoom",
            afterZoom.hit && afterZoom.objectId == kDemoCubeObjectId);

    r.check("scene_zero_viewport_safe",
            !pickScene(c.snapshot(), kCenterX, kCenterY, 0, 0, identity, identity, true).hit);
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

// ---------------------------------------------------------------------------
// CAMPROJ-09 / CAMPROJ-10 — picking in both projections
//
// Part of the CAMPROJ series that begins in the camera suite; these two live
// here because the code they exercise is buildPickRay.
//
// The decisive measurement is a ROUND TRIP: pick at a pixel, then project the
// resulting world hit back through the very matrices the renderer draws with and
// check it lands on the pixel it started from. That single statement is what
// "the hit matches the visible surface" actually means, it is true of both
// projections, and it cannot be satisfied by a ray that is merely plausible —
// an orthographic image picked with a perspective ray agrees only at the screen
// centre and drifts further out toward every edge, which is exactly what the
// off-centre pixels below would catch.
// ---------------------------------------------------------------------------

// The known box this stage is specified against: 2.0 x 1.0 x 0.5 m.
constexpr float kProbeBoxWidth = 2.0f;
constexpr float kProbeBoxHeight = 1.0f;
constexpr float kProbeBoxDepth = 0.5f;

// Yaw 0 / pitch 0: the eye sits on +Z looking down -Z, world +X right, +Y up.
// The algebra a check asserts is then algebra a reader can redo by hand.
CameraController makeAxisAlignedController(int width, int height) {
    CameraController c;
    c.setViewport(width, height);
    TouchPointer p{0, 0.0f, 0.0f};
    c.onTouch(TouchAction::Down, -1, &p, 1);
    TouchPointer q{0, kInitialYaw / kOrbitRadiansPerPixel,
                   -kInitialPitch / kOrbitRadiansPerPixel};
    c.onTouch(TouchAction::Move, -1, &q, 1);
    c.resetGesture();
    return c;
}

// Projects a world point to a view-local pixel: the exact inverse of what
// buildPickRay is asked to do, built from the same snapshot.
bool screenOf(const CameraSnapshot& s, const Vec3& world, int width, int height, float* outX,
              float* outY) {
    const Mat4 vp = mat4Multiply(s.proj, s.view);
    const float x =
        vp.m[0] * world.x + vp.m[4] * world.y + vp.m[8] * world.z + vp.m[12];
    const float y =
        vp.m[1] * world.x + vp.m[5] * world.y + vp.m[9] * world.z + vp.m[13];
    const float w =
        vp.m[3] * world.x + vp.m[7] * world.y + vp.m[11] * world.z + vp.m[15];
    if (!std::isfinite(w) || std::fabs(w) < 1e-9f) {
        return false;
    }
    *outX = ((x / w) + 1.0f) * 0.5f * static_cast<float>(width);
    *outY = ((y / w) + 1.0f) * 0.5f * static_cast<float>(height);
    return std::isfinite(*outX) && std::isfinite(*outY);
}

void testProjectionPicking(Recorder& r) {
    ConstructionBox box;
    box.setDimensionsMeters(kProbeBoxWidth, kProbeBoxHeight, kProbeBoxDepth);
    const ConstructionMesh mesh = box.generateMesh();

    TriangleMeshView view{};
    view.positions = mesh.vertices.empty() ? nullptr : mesh.vertices[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    view.indices = mesh.indices.empty() ? nullptr : mesh.indices.data();
    view.indexCount = static_cast<uint32_t>(mesh.indices.size());

    r.check("camproj_probe_box_is_2x1x0.5",
            nearly(box.widthMeters(), kProbeBoxWidth, 1e-6f) &&
                nearly(box.heightMeters(), kProbeBoxHeight, 1e-6f) &&
                nearly(box.depthMeters(), kProbeBoxDepth, 1e-6f) && view.indexCount == 36);

    // Non-square viewport, deliberately: a projection bug that cancels out on a
    // square viewport is the one worth catching.
    constexpr int kW = 1080;
    constexpr int kH = 2400;

    // Five pixels spread across the box's silhouette, including well off-centre
    // in both axes, because the centre pixel is where the two projections agree
    // and therefore where a wrong ray hides.
    struct Probe {
        float u, v;  // fraction of the box's projected half-extent
    };
    const Probe probes[5] = {{0.0f, 0.0f}, {0.6f, 0.0f}, {-0.6f, 0.0f},
                             {0.0f, 0.6f}, {0.55f, -0.55f}};

    for (int mode = 0; mode < 2; ++mode) {
        const bool orthographic = (mode == 1);
        CameraController c = makeAxisAlignedController(kW, kH);
        if (orthographic) {
            c.setProjectionMode(ProjectionMode::Orthographic);
        }
        const CameraSnapshot s = c.snapshot();

        bool allHit = true;
        bool allOnFrontFace = true;
        bool allRoundTrip = true;
        bool allInsideBox = true;
        bool originsDiffer = false;
        bool directionsShared = true;
        Ray firstRay{};

        for (int i = 0; i < 5; ++i) {
            // Aim at a point known to be on the box's near (+Z) face, then ask
            // the picker for that pixel back.
            const Vec3 aim{probes[i].u * kProbeBoxWidth * 0.5f,
                           probes[i].v * kProbeBoxHeight * 0.5f, kProbeBoxDepth * 0.5f};
            float px = 0.0f, py = 0.0f;
            if (!screenOf(s, aim, kW, kH, &px, &py)) {
                allHit = false;
                continue;
            }

            Ray ray{};
            if (!buildPickRay(s, px, py, kW, kH, &ray)) {
                allHit = false;
                continue;
            }
            if (i == 0) {
                firstRay = ray;
            } else {
                if (length(vec3Sub(ray.origin, firstRay.origin)) > 1e-4f) {
                    originsDiffer = true;
                }
                if (length(vec3Sub(ray.direction, firstRay.direction)) > 1e-5f) {
                    directionsShared = false;
                }
            }

            const TriangleHit hit = pickTriangleMesh(ray, view, /*frontFacesOnly=*/true);
            if (!hit.hit) {
                allHit = false;
                continue;
            }

            // Front-face-only picking must return the NEAR face (+Z at 0.25),
            // never the far one at -0.25.
            if (!nearly(hit.position.z, kProbeBoxDepth * 0.5f, 1e-3f)) {
                allOnFrontFace = false;
            }
            // And the hit must lie within the box's exact extents.
            if (std::fabs(hit.position.x) > kProbeBoxWidth * 0.5f + 1e-3f ||
                std::fabs(hit.position.y) > kProbeBoxHeight * 0.5f + 1e-3f) {
                allInsideBox = false;
            }

            // The round trip: back to the pixel it came from, inside a pixel.
            float bx = 0.0f, by = 0.0f;
            if (!screenOf(s, hit.position, kW, kH, &bx, &by) ||
                std::fabs(bx - px) > 1.0f || std::fabs(by - py) > 1.0f) {
                allRoundTrip = false;
            }
        }

        if (orthographic) {
            r.check("camproj10_ortho_all_probe_pixels_hit", allHit);
            r.check("camproj10_ortho_hits_front_face_only", allOnFrontFace);
            r.check("camproj10_ortho_hit_inside_exact_box_extents", allInsideBox);
            r.check("camproj10_ortho_hit_reprojects_to_source_pixel", allRoundTrip);
            // The structural signature of a parallel pick: the origin slides
            // with the pixel and the direction does not.
            r.check("camproj10_ortho_ray_origin_moves_with_pixel", originsDiffer);
            r.check("camproj10_ortho_ray_direction_is_shared", directionsShared);
            // The shared direction is the view axis itself.
            const Vec3 forward{-s.view.m[2], -s.view.m[6], -s.view.m[10]};
            r.check("camproj10_ortho_direction_is_the_view_axis",
                    length(vec3Sub(firstRay.direction, forward)) < 1e-5f);
        } else {
            r.check("camproj09_perspective_all_probe_pixels_hit", allHit);
            r.check("camproj09_perspective_hits_front_face_only", allOnFrontFace);
            r.check("camproj09_perspective_hit_inside_exact_box_extents", allInsideBox);
            r.check("camproj09_perspective_hit_reprojects_to_source_pixel", allRoundTrip);
            // The structural signature of a pinhole pick: one origin, fanning
            // directions — the exact opposite of the orthographic case.
            r.check("camproj09_perspective_ray_origin_is_the_eye",
                    !originsDiffer && length(vec3Sub(firstRay.origin, s.eye)) < 1e-4f);
            r.check("camproj09_perspective_ray_directions_fan", !directionsShared);
        }
    }

    // The two modes must agree about the SAME visible surface: a pick at the
    // centre lands on the same face at the same point in both, because the
    // centre pixel is the one ray the two projections share.
    {
        CameraController p = makeAxisAlignedController(kW, kH);
        CameraController o = makeAxisAlignedController(kW, kH);
        o.setProjectionMode(ProjectionMode::Orthographic);

        Ray pr{}, orr{};
        const bool built = buildPickRay(p.snapshot(), kW * 0.5f, kH * 0.5f, kW, kH, &pr) &&
                           buildPickRay(o.snapshot(), kW * 0.5f, kH * 0.5f, kW, kH, &orr);
        const TriangleHit ph = pickTriangleMesh(pr, view, true);
        const TriangleHit oh = pickTriangleMesh(orr, view, true);
        r.check("camproj_centre_pick_agrees_across_projections",
                built && ph.hit && oh.hit &&
                    length(vec3Sub(ph.position, oh.position)) < 1e-3f &&
                    ph.triangleIndex == oh.triangleIndex);
    }

    // A pixel clear of the silhouette misses in both modes: an orthographic ray
    // that had kept a perspective origin would still sweep inward and could hit.
    {
        CameraController o = makeAxisAlignedController(kW, kH);
        o.setProjectionMode(ProjectionMode::Orthographic);
        Ray corner{};
        const bool built = buildPickRay(o.snapshot(), 4.0f, 4.0f, kW, kH, &corner);
        r.check("camproj10_ortho_background_pixel_misses",
                built && !pickTriangleMesh(corner, view, true).hit);
    }

    // Degenerate input is refused in Orthographic exactly as in Perspective —
    // the guards precede the projection branch, so neither mode can produce a
    // NaN ray.
    {
        CameraController o = makeAxisAlignedController(kW, kH);
        o.setProjectionMode(ProjectionMode::Orthographic);
        const CameraSnapshot s = o.snapshot();
        Ray unused{};
        r.check("camproj10_ortho_rejects_degenerate_input",
                !buildPickRay(s, kW * 0.5f, kH * 0.5f, 0, kH, &unused) &&
                    !buildPickRay(s, kW * 0.5f, kH * 0.5f, kW, 0, &unused) &&
                    !buildPickRay(s, NAN, kH * 0.5f, kW, kH, &unused) &&
                    !buildPickRay(s, kW * 0.5f, INFINITY, kW, kH, &unused));
        Ray ok{};
        r.check("camproj10_ortho_ray_is_finite_and_unit",
                buildPickRay(s, 12.0f, 2380.0f, kW, kH, &ok) && vec3Finite(ok.origin) &&
                    vec3Finite(ok.direction) && nearly(length(ok.direction), 1.0f, 1e-5f));
    }
}

// ---------------------------------------------------------------------------
// PLN-11..16 — the bounded two-sided Plane picking exception
//
// A Construction Plane is a flat, open, zero-thickness sheet: unlike a closed
// solid there is no interior a back-face hit could wrongly reach, so it is the
// one primitive `pickScene` picks from BOTH sides (see forgeshape_selection.h/
// .cpp). These checks exercise that exception directly, at both the
// TriangleMeshView level (proving the raw exception mechanics) and through
// `pickScene`'s explicit-frontFacesOnly overload (proving the actual function
// the product calls behaves correctly), in both camera projections.
// ---------------------------------------------------------------------------

constexpr float kProbePlaneWidth = 2.0f;
constexpr float kProbePlaneDepth = 1.5f;

TriangleMeshView planeView(const ConstructionMesh& mesh) {
    TriangleMeshView view{};
    view.positions = mesh.vertices.empty() ? nullptr : mesh.vertices[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    view.indices = mesh.indices.empty() ? nullptr : mesh.indices.data();
    view.indexCount = static_cast<uint32_t>(mesh.indices.size());
    return view;
}

// PLN-11/12: a ray from the canonical front (+Y) and a ray from the canonical
// back (-Y) both hit the plane once the two-sided exception is requested;
// front-face-only picking (the ordinary closed-solid rule) hits the front but
// MISSES the back, which is exactly the defect the exception exists to fix.
void testPlaneFrontAndBackPicking(Recorder& r) {
    ConstructionPlane plane;
    plane.setDimensionsMeters(kProbePlaneWidth, kProbePlaneDepth);
    const ConstructionMesh mesh = plane.generateMesh();
    const TriangleMeshView view = planeView(mesh);

    const Ray fromAbove{{0.3f, 5.0f, 0.2f}, {0.0f, -1.0f, 0.0f}};
    const Ray fromBelow{{0.3f, -5.0f, 0.2f}, {0.0f, 1.0f, 0.0f}};

    const TriangleHit frontOrdinary = pickTriangleMesh(fromAbove, view, /*frontFacesOnly=*/true);
    r.check("plane_front_hits_under_ordinary_front_face_rule",
            frontOrdinary.hit && nearly(frontOrdinary.position.y, 0.0f, 1e-5f));

    const TriangleHit backOrdinary = pickTriangleMesh(fromBelow, view, /*frontFacesOnly=*/true);
    r.check("plane_back_misses_under_ordinary_front_face_rule", !backOrdinary.hit);

    const TriangleHit frontTwoSided = pickTriangleMesh(fromAbove, view, /*frontFacesOnly=*/false);
    r.check("plane_front_hits_with_two_sided_exception",
            frontTwoSided.hit && nearly(frontTwoSided.position.y, 0.0f, 1e-5f));

    const TriangleHit backTwoSided = pickTriangleMesh(fromBelow, view, /*frontFacesOnly=*/false);
    r.check("plane_back_hits_with_two_sided_exception",
            backTwoSided.hit && nearly(backTwoSided.position.y, 0.0f, 1e-5f) &&
                nearly(backTwoSided.position.x, 0.3f, 1e-5f) &&
                nearly(backTwoSided.position.z, 0.2f, 1e-5f));
}

// PLN-13: a ray clear of the finite rectangle misses from either side, even
// with the two-sided exception on — the exception widens WHICH SIDE can be
// hit, never the finite extent of the sheet.
void testPlaneOutsideRectangleMisses(Recorder& r) {
    ConstructionPlane plane;
    plane.setDimensionsMeters(kProbePlaneWidth, kProbePlaneDepth);
    const ConstructionMesh mesh = plane.generateMesh();
    const TriangleMeshView view = planeView(mesh);

    // Just outside the half-width (1.0 m) on X.
    const Ray besideAbove{{1.2f, 5.0f, 0.0f}, {0.0f, -1.0f, 0.0f}};
    const Ray besideBelow{{1.2f, -5.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    // Just outside the half-depth (0.75 m) on Z.
    const Ray beyondAbove{{0.0f, 5.0f, 0.9f}, {0.0f, -1.0f, 0.0f}};

    r.check("plane_outside_width_misses_from_front",
            !pickTriangleMesh(besideAbove, view, false).hit);
    r.check("plane_outside_width_misses_from_back",
            !pickTriangleMesh(besideBelow, view, false).hit);
    r.check("plane_outside_depth_misses", !pickTriangleMesh(beyondAbove, view, false).hit);

    // Just inside both bounds still hits, proving the misses above are really
    // about the rectangle's edge and not a broken ray.
    const Ray justInside{{0.99f, 5.0f, 0.74f}, {0.0f, -1.0f, 0.0f}};
    r.check("plane_just_inside_rectangle_hits", pickTriangleMesh(justInside, view, false).hit);
}

// PLN-14/15/16: the two-sided exception survives a representative rotation and
// works in both Perspective and Orthographic, mirroring CAMPROJ-09/10's probe
// technique (aim a known world point, ask the camera for its pixel, pick at
// that pixel, and check the hit round-trips back to the point it was aimed
// at). Rotating 90 degrees about local X takes the plane's canonical front
// (local +Y) to world +Z — the direction `makeAxisAlignedController`'s eye
// looks toward — so a local point (lx, 0, lz) lands at world (lx, -lz, 0).
void testPlaneRotatedPickingBothProjections(Recorder& r) {
    ConstructionPlane plane;
    plane.setDimensionsMeters(kProbePlaneWidth, kProbePlaneDepth);
    const ConstructionMesh mesh = plane.generateMesh();
    const TriangleMeshView view = planeView(mesh);

    ConstructionTransform transform;
    TransformValues rotated;
    rotated.rotationX = 90.0;
    transform.setValues(rotated);
    const Mat4 model = transform.modelMatrix();
    const Mat4 inverseModel = transform.inverseModelMatrix();

    constexpr int kW = 1080;
    constexpr int kH = 2400;
    const Vec3 worldAim{0.4f, -0.3f, 0.0f};  // (lx, -lz, 0) for local (0.4, 0, 0.3)

    for (int mode = 0; mode < 2; ++mode) {
        const bool orthographic = (mode == 1);
        CameraController c = makeAxisAlignedController(kW, kH);
        if (orthographic) {
            c.setProjectionMode(ProjectionMode::Orthographic);
        }
        const CameraSnapshot s = c.snapshot();

        float px = 0.0f, py = 0.0f;
        const bool projected = screenOf(s, worldAim, kW, kH, &px, &py);

        Ray worldRay{};
        const bool built = projected && buildPickRay(s, px, py, kW, kH, &worldRay);
        Ray localRay{};
        const bool moved = built && transformRayToLocal(worldRay, inverseModel, &localRay);
        const TriangleHit front =
            moved ? pickTriangleMesh(localRay, view, /*frontFacesOnly=*/false) : TriangleHit{};
        const Vec3 worldHit =
            front.hit ? mat4TransformPoint(model, front.position) : Vec3{0.0f, 0.0f, 0.0f};

        const bool ok = moved && front.hit && nearly(worldHit.x, worldAim.x, 1e-3f) &&
                        nearly(worldHit.y, worldAim.y, 1e-3f) && nearly(worldHit.z, worldAim.z, 1e-3f);
        if (orthographic) {
            r.check("plane_ortho_rotated_front_pick_round_trips", ok);
        } else {
            r.check("plane_persp_rotated_front_pick_round_trips", ok);
        }
    }

    // Back: a ray from world -Z toward +Z meets the plane's BACK once rotated
    // this way — a hit only with the two-sided exception, a miss under the
    // ordinary closed-solid rule. Projection-independent (the exception is a
    // property of the pick, not of the camera), so checked once.
    Ray fromBehindWorld{{worldAim.x, worldAim.y, -5.0f}, {0.0f, 0.0f, 1.0f}};
    Ray localBack{};
    const bool builtBack = transformRayToLocal(fromBehindWorld, inverseModel, &localBack);
    const TriangleHit backTwoSided = pickTriangleMesh(localBack, view, /*frontFacesOnly=*/false);
    r.check("plane_rotated_back_pick_hits_finite_plane", builtBack && backTwoSided.hit);
    const TriangleHit backOrdinary = pickTriangleMesh(localBack, view, /*frontFacesOnly=*/true);
    r.check("plane_rotated_back_misses_without_exception", !backOrdinary.hit);
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
    testProjectionPicking(r);
    testPlaneFrontAndBackPicking(r);
    testPlaneOutsideRectangleMisses(r);
    testPlaneRotatedPickingBothProjections(r);
    return r.n;
}

}  // namespace forgeshape
