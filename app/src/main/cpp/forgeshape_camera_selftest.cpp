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
           std::isfinite(s.distance) && std::isfinite(s.orthoHalfHeightMeters);
}

bool nearly(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// ---------------------------------------------------------------------------
// Projection: CAMPROJ-01 .. CAMPROJ-08, CAMPROJ-12 .. CAMPROJ-14
//
// CAMPROJ-09 (perspective picking) and CAMPROJ-10 (orthographic picking) live in
// the picking suite, and CAMPROJ-11 (sculpt hit and radius) lives in the sculpt
// suite, because that is where the code they exercise lives. The ids are
// deliberately kept as one series across the three files: they are one question
// asked of three modules.
// ---------------------------------------------------------------------------

// Projects a world point through a snapshot the same way the vertex stage does,
// including the perspective divide, and reports NORMALIZED DEVICE coordinates.
//
// This is the measurement every projection check below is built on. It applies
// the divide unconditionally rather than branching on the mode, which is the
// point: an orthographic matrix must produce w = 1 on its own, and if it ever
// did not, dividing anyway is what would expose it.
struct NdcPoint {
    float x, y, z, w;
};

NdcPoint projectPoint(const CameraSnapshot& s, const Vec3& world) {
    const Mat4 viewProj = mat4Multiply(s.proj, s.view);
    const float x = viewProj.m[0] * world.x + viewProj.m[4] * world.y +
                    viewProj.m[8] * world.z + viewProj.m[12];
    const float y = viewProj.m[1] * world.x + viewProj.m[5] * world.y +
                    viewProj.m[9] * world.z + viewProj.m[13];
    const float z = viewProj.m[2] * world.x + viewProj.m[6] * world.y +
                    viewProj.m[10] * world.z + viewProj.m[14];
    const float w = viewProj.m[3] * world.x + viewProj.m[7] * world.y +
                    viewProj.m[11] * world.z + viewProj.m[15];
    NdcPoint p{x, y, z, w};
    if (std::isfinite(w) && std::fabs(w) > 1e-9f) {
        p.x = x / w;
        p.y = y / w;
        p.z = z / w;
    }
    return p;
}

// Places the camera straight down -Z at the origin, so the algebra a check
// asserts is the algebra a reader can do by hand. Yaw 0 / pitch 0 puts the eye
// on +Z looking toward -Z, with world +X to the right and world +Y up.
CameraController makeAxisAlignedController(int width, int height) {
    CameraController c;
    c.setViewport(width, height);
    // Drive yaw and pitch to zero through the public gesture path rather than
    // reaching into the class: an initial pose of (0.7, 0.5) rotated to (0, 0)
    // by an exact pixel count keeps this test on the same surface the product
    // uses.
    TouchPointer p{0, 0.0f, 0.0f};
    c.onTouch(TouchAction::Down, -1, &p, 1);
    const float dx = kInitialYaw / kOrbitRadiansPerPixel;    // yaw -= dx * k
    const float dy = -kInitialPitch / kOrbitRadiansPerPixel; // pitch += dy * k
    TouchPointer q{0, dx, dy};
    c.onTouch(TouchAction::Move, -1, &q, 1);
    c.resetGesture();
    return c;
}

void switchTo(CameraController& c, ProjectionMode mode) { c.setProjectionMode(mode); }

void pinch(CameraController& c, float fromSpan, float toSpan) {
    const float y = 1000.0f;
    TouchPointer a{0, 500.0f, y};
    c.onTouch(TouchAction::Down, -1, &a, 1);
    TouchPointer downPair[2]{TouchPointer{0, 500.0f, y}, TouchPointer{1, 500.0f + fromSpan, y}};
    c.onTouch(TouchAction::PointerDown, -1, downPair, 2);
    TouchPointer movePair[2]{TouchPointer{0, 500.0f, y}, TouchPointer{1, 500.0f + toSpan, y}};
    c.onTouch(TouchAction::Move, -1, movePair, 2);
    c.resetGesture();
}

void runProjectionSelfTests(Recorder& rec) {
    const float tanHalfFov = std::tan(kFovYRadians * 0.5f);

    // -----------------------------------------------------------------------
    // CAMPROJ-01 — the EXISTING perspective projection maps known points to the
    // values the matrix says it should, and is a real pinhole projection.
    //
    // This is an audit of what already shipped, not a change to it: the point of
    // the stage is to add a second projection without disturbing the first.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        const CameraSnapshot s = c.snapshot();
        const float aspect = 1080.0f / 2400.0f;

        rec.check("camproj01_default_mode_is_perspective",
                  s.projection == ProjectionMode::Perspective &&
                      c.projectionMode() == ProjectionMode::Perspective);

        // The declared field of view is 60 deg and nothing else may claim to set
        // it. Reading it back off the matrix proves the constant reaches clip
        // space unmodified.
        rec.check("camproj01_fov_is_60_degrees", nearly(kFovYRadians, 1.0471976f, 1e-6f));
        rec.check("camproj01_proj_m5_is_minus_cot_half_fov",
                  nearly(s.proj.m[5], -1.0f / tanHalfFov, 1e-5f));
        rec.check("camproj01_proj_m0_divides_by_aspect",
                  nearly(s.proj.m[0], (1.0f / tanHalfFov) / aspect, 1e-5f));
        rec.check("camproj01_is_a_pinhole_projection", nearly(s.proj.m[11], -1.0f, 1e-6f));

        // The target sits at the centre of the frame.
        const NdcPoint centre = projectPoint(s, s.target);
        rec.check("camproj01_target_projects_to_centre",
                  nearly(centre.x, 0.0f, 1e-5f) && nearly(centre.y, 0.0f, 1e-5f));

        // A point exactly at the top edge of the view at the target plane must
        // land on the top of NDC. The eye is `distance` from the target, so the
        // half-height there is distance * tan(fovY / 2). Vulkan NDC Y grows
        // DOWNWARD, so world +Y maps to NDC -1.
        const float halfHeight = s.distance * tanHalfFov;
        const NdcPoint top = projectPoint(s, Vec3{0.0f, halfHeight, 0.0f});
        rec.check("camproj01_top_of_frame_maps_to_minus_one", nearly(top.y, -1.0f, 1e-4f));
        const NdcPoint right = projectPoint(s, Vec3{halfHeight * aspect, 0.0f, 0.0f});
        rec.check("camproj01_right_of_frame_maps_to_plus_one", nearly(right.x, 1.0f, 1e-4f));

        // Depth: the near plane maps to 0 and the far plane to 1.
        const NdcPoint atNear = projectPoint(s, Vec3{0.0f, 0.0f, s.eye.z - kNearPlane});
        const NdcPoint atFar = projectPoint(s, Vec3{0.0f, 0.0f, s.eye.z - kFarPlane});
        rec.check("camproj01_near_plane_maps_to_zero", nearly(atNear.z, 0.0f, 1e-4f));
        rec.check("camproj01_far_plane_maps_to_one", nearly(atFar.z, 1.0f, 1e-4f));

        // No screen-axis non-uniform scale: the ratio of the two scale terms is
        // exactly the aspect ratio, so a square in the world is a square on
        // screen. This is the check that would fail if anything ever stretched
        // one axis to "fix" a rotated display.
        rec.check("camproj01_no_non_uniform_screen_scale",
                  nearly(std::fabs(s.proj.m[0] / s.proj.m[5]), 1.0f / aspect, 1e-5f));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-02 — the orthographic matrix maps X, Y and depth correctly.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        switchTo(c, ProjectionMode::Orthographic);
        const CameraSnapshot s = c.snapshot();
        const float aspect = 1080.0f / 2400.0f;
        const float h = s.orthoHalfHeightMeters;

        rec.check("camproj02_mode_is_orthographic",
                  s.projection == ProjectionMode::Orthographic);
        // The defining property: no perspective divide term at all.
        rec.check("camproj02_is_a_parallel_projection", s.proj.m[11] == 0.0f);
        rec.check("camproj02_w_is_one_everywhere",
                  nearly(projectPoint(s, Vec3{0.0f, 0.0f, 0.0f}).w, 1.0f, 1e-6f) &&
                      nearly(projectPoint(s, Vec3{3.0f, -2.0f, 7.0f}).w, 1.0f, 1e-6f));

        const NdcPoint centre = projectPoint(s, s.target);
        rec.check("camproj02_target_projects_to_centre",
                  nearly(centre.x, 0.0f, 1e-5f) && nearly(centre.y, 0.0f, 1e-5f));

        // Y: +h in the world is the top of the frame, which is NDC -1.
        const NdcPoint top = projectPoint(s, Vec3{0.0f, h, 0.0f});
        const NdcPoint bottom = projectPoint(s, Vec3{0.0f, -h, 0.0f});
        rec.check("camproj02_y_top_maps_to_minus_one", nearly(top.y, -1.0f, 1e-4f));
        rec.check("camproj02_y_bottom_maps_to_plus_one", nearly(bottom.y, 1.0f, 1e-4f));

        // X: the visible half-width is the half-height times the aspect.
        const NdcPoint right = projectPoint(s, Vec3{h * aspect, 0.0f, 0.0f});
        const NdcPoint left = projectPoint(s, Vec3{-h * aspect, 0.0f, 0.0f});
        rec.check("camproj02_x_right_maps_to_plus_one", nearly(right.x, 1.0f, 1e-4f));
        rec.check("camproj02_x_left_maps_to_minus_one", nearly(left.x, -1.0f, 1e-4f));

        // Depth uses the SAME Vulkan [0, 1] convention as perspective, measured
        // from the orthographic view plane (camera.eye).
        const NdcPoint atNear = projectPoint(s, Vec3{0.0f, 0.0f, s.eye.z - kNearPlane});
        const NdcPoint atFar = projectPoint(s, Vec3{0.0f, 0.0f, s.eye.z - kFarPlane});
        rec.check("camproj02_near_maps_to_zero", nearly(atNear.z, 0.0f, 1e-4f));
        rec.check("camproj02_far_maps_to_one", nearly(atFar.z, 1.0f, 1e-4f));
        // Linear in depth, unlike perspective — the midpoint lands at 0.5.
        const float mid = 0.5f * (kNearPlane + kFarPlane);
        rec.check("camproj02_depth_is_linear",
                  nearly(projectPoint(s, Vec3{0.0f, 0.0f, s.eye.z - mid}).z, 0.5f, 1e-4f));

        // The target must sit INSIDE the slab, or the object would be clipped
        // away entirely. This is what kOrthoViewPlaneDistance buys.
        rec.check("camproj02_target_inside_depth_slab",
                  centre.z > 0.0f && centre.z < 1.0f);
        rec.check("camproj02_no_non_uniform_screen_scale",
                  nearly(std::fabs(s.proj.m[0] / s.proj.m[5]), 1.0f / aspect, 1e-5f));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-03 — in Orthographic, two equal segments parallel to the image
    // plane have the SAME screen size regardless of depth. This is the property
    // the whole stage exists to provide.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        switchTo(c, ProjectionMode::Orthographic);
        const CameraSnapshot s = c.snapshot();

        // Two 1.0 m segments along world X, one 3 m nearer the camera than the
        // other. Both are parallel to the image plane.
        const float zNear = 3.0f, zFar = 0.0f;
        const float nearLen =
            projectPoint(s, Vec3{0.5f, 0.0f, zNear}).x - projectPoint(s, Vec3{-0.5f, 0.0f, zNear}).x;
        const float farLen =
            projectPoint(s, Vec3{0.5f, 0.0f, zFar}).x - projectPoint(s, Vec3{-0.5f, 0.0f, zFar}).x;

        rec.check("camproj03_equal_segments_measure_nonzero",
                  nearLen > 1e-5f && farLen > 1e-5f);
        rec.check("camproj03_no_depth_foreshortening", nearly(nearLen, farLen, 1e-6f));

        // Same again vertically, and at a much larger depth separation, so the
        // property is not an artefact of a small offset.
        const float nearH =
            projectPoint(s, Vec3{0.0f, 0.5f, 40.0f}).y - projectPoint(s, Vec3{0.0f, -0.5f, 40.0f}).y;
        const float farH = projectPoint(s, Vec3{0.0f, 0.5f, -40.0f}).y -
                           projectPoint(s, Vec3{0.0f, -0.5f, -40.0f}).y;
        rec.check("camproj03_no_depth_foreshortening_vertical", nearly(nearH, farH, 1e-6f));

        // Parallel edges stay parallel: the two X-extents of a box are the same
        // width at both of its Z faces, which is what "no convergence" means.
        const float frontWidth = projectPoint(s, Vec3{1.0f, 0.0f, 0.25f}).x -
                                 projectPoint(s, Vec3{-1.0f, 0.0f, 0.25f}).x;
        const float backWidth = projectPoint(s, Vec3{1.0f, 0.0f, -0.25f}).x -
                                projectPoint(s, Vec3{-1.0f, 0.0f, -0.25f}).x;
        rec.check("camproj03_box_faces_do_not_converge", nearly(frontWidth, backWidth, 1e-6f));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-04 — in Perspective the SAME two segments do foreshorten, by the
    // ratio the pinhole model predicts. The counterpart to CAMPROJ-03: without
    // it, a projection that collapsed to a constant would pass CAMPROJ-03.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        const CameraSnapshot s = c.snapshot();

        const float zNear = 3.0f, zFar = 0.0f;
        const float nearLen =
            projectPoint(s, Vec3{0.5f, 0.0f, zNear}).x - projectPoint(s, Vec3{-0.5f, 0.0f, zNear}).x;
        const float farLen =
            projectPoint(s, Vec3{0.5f, 0.0f, zFar}).x - projectPoint(s, Vec3{-0.5f, 0.0f, zFar}).x;

        rec.check("camproj04_nearer_segment_is_larger", nearLen > farLen + 1e-4f);

        // The exact prediction: screen size scales as 1 / depth, so the ratio is
        // the inverse ratio of the two depths from the eye.
        const float depthNear = s.eye.z - zNear;
        const float depthFar = s.eye.z - zFar;
        rec.check("camproj04_foreshortening_matches_pinhole_model",
                  nearly(nearLen / farLen, depthFar / depthNear, 1e-3f));

        // And parallel edges DO converge, which is the visible difference.
        const float frontWidth = projectPoint(s, Vec3{1.0f, 0.0f, 0.25f}).x -
                                 projectPoint(s, Vec3{-1.0f, 0.0f, 0.25f}).x;
        const float backWidth = projectPoint(s, Vec3{1.0f, 0.0f, -0.25f}).x -
                                projectPoint(s, Vec3{-1.0f, 0.0f, -0.25f}).x;
        rec.check("camproj04_box_faces_converge", frontWidth > backWidth + 1e-4f);
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-05 — Perspective -> Orthographic preserves the framing at the
    // target plane. The frame must not jump and the object must not vanish.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        const CameraSnapshot before = c.snapshot();

        // A 2.0 x 1.0 x 0.5 m box's top corner, measured at the target plane.
        const Vec3 probe{1.0f, 0.5f, 0.0f};
        const NdcPoint pBefore = projectPoint(before, probe);

        rec.check("camproj05_switch_reports_a_change",
                  c.setProjectionMode(ProjectionMode::Orthographic));
        const CameraSnapshot after = c.snapshot();
        const NdcPoint pAfter = projectPoint(after, probe);

        rec.check("camproj05_ortho_half_height_matches_perspective_span",
                  nearly(after.orthoHalfHeightMeters, before.distance * tanHalfFov, 1e-4f));
        // The scale at the target plane is preserved to well inside a pixel.
        rec.check("camproj05_target_plane_scale_preserved",
                  nearly(pAfter.x, pBefore.x, 1e-4f) && nearly(pAfter.y, pBefore.y, 1e-4f));
        // The centre does not move and the pose is untouched.
        const NdcPoint centre = projectPoint(after, after.target);
        rec.check("camproj05_centre_does_not_jump",
                  nearly(centre.x, 0.0f, 1e-5f) && nearly(centre.y, 0.0f, 1e-5f));
        rec.check("camproj05_pose_untouched",
                  after.yaw == before.yaw && after.pitch == before.pitch &&
                      after.distance == before.distance && after.target.x == before.target.x &&
                      after.target.y == before.target.y && after.target.z == before.target.z);
        // The object is still inside the frame and inside the depth slab.
        rec.check("camproj05_object_still_visible",
                  std::fabs(pAfter.x) < 1.0f && std::fabs(pAfter.y) < 1.0f && pAfter.z > 0.0f &&
                      pAfter.z < 1.0f);
        rec.check("camproj05_switching_again_is_a_no_op",
                  !c.setProjectionMode(ProjectionMode::Orthographic));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-06 — Orthographic -> Perspective preserves it too, including
    // after the ortho span has been changed by a pinch, and a round trip
    // returns to the original framing.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        switchTo(c, ProjectionMode::Orthographic);
        pinch(c, 200.0f, 420.0f);  // spread: zoom in, ortho span shrinks
        const CameraSnapshot before = c.snapshot();
        const Vec3 probe{0.4f, 0.25f, 0.0f};
        const NdcPoint pBefore = projectPoint(before, probe);

        rec.check("camproj06_switch_reports_a_change",
                  c.setProjectionMode(ProjectionMode::Perspective));
        const CameraSnapshot after = c.snapshot();
        const NdcPoint pAfter = projectPoint(after, probe);

        rec.check("camproj06_distance_matches_ortho_span",
                  nearly(after.distance, before.orthoHalfHeightMeters / tanHalfFov, 1e-3f));
        rec.check("camproj06_target_plane_scale_preserved",
                  nearly(pAfter.x, pBefore.x, 1e-3f) && nearly(pAfter.y, pBefore.y, 1e-3f));
        rec.check("camproj06_centre_does_not_jump",
                  nearly(projectPoint(after, after.target).x, 0.0f, 1e-5f) &&
                      nearly(projectPoint(after, after.target).y, 0.0f, 1e-5f));
        rec.check("camproj06_object_still_visible",
                  std::fabs(pAfter.x) < 1.0f && std::fabs(pAfter.y) < 1.0f && pAfter.z > 0.0f &&
                      pAfter.z < 1.0f);

        // Round trip: the two conversions are the same identity read in opposite
        // directions, so going back must land where it started.
        const float spanBefore = before.orthoHalfHeightMeters;
        c.setProjectionMode(ProjectionMode::Orthographic);
        rec.check("camproj06_round_trip_restores_span",
                  nearly(c.orthoHalfHeightMeters(), spanBefore, 1e-3f));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-07 — orbit changes the viewing DIRECTION and nothing else, in
    // whichever mode is active. It must never move the distance, the FOV or the
    // orthographic span.
    // -----------------------------------------------------------------------
    {
        // Perspective.
        CameraController c = makeAxisAlignedController(1080, 2400);
        const CameraSnapshot before = c.snapshot();
        down(c, 0, 300.0f, 1000.0f);
        move1(c, 0, 460.0f, 1120.0f);
        const CameraSnapshot after = c.snapshot();
        rec.check("camproj07_perspective_orbit_keeps_distance",
                  after.distance == before.distance);
        rec.check("camproj07_perspective_orbit_keeps_fov",
                  after.proj.m[5] == before.proj.m[5] && after.proj.m[0] == before.proj.m[0]);
        rec.check("camproj07_perspective_orbit_keeps_target",
                  after.target.x == before.target.x && after.target.y == before.target.y &&
                      after.target.z == before.target.z);
        rec.check("camproj07_perspective_orbit_changes_direction",
                  after.yaw != before.yaw && after.pitch != before.pitch);
        rec.check("camproj07_perspective_orbit_keeps_mode",
                  after.projection == ProjectionMode::Perspective);

        // Orthographic.
        CameraController o = makeAxisAlignedController(1080, 2400);
        switchTo(o, ProjectionMode::Orthographic);
        const CameraSnapshot oBefore = o.snapshot();
        down(o, 0, 300.0f, 1000.0f);
        move1(o, 0, 460.0f, 1120.0f);
        const CameraSnapshot oAfter = o.snapshot();
        rec.check("camproj07_ortho_orbit_keeps_span",
                  oAfter.orthoHalfHeightMeters == oBefore.orthoHalfHeightMeters);
        rec.check("camproj07_ortho_orbit_keeps_projection_matrix",
                  oAfter.proj.m[0] == oBefore.proj.m[0] && oAfter.proj.m[5] == oBefore.proj.m[5] &&
                      oAfter.proj.m[10] == oBefore.proj.m[10]);
        rec.check("camproj07_ortho_orbit_keeps_distance", oAfter.distance == oBefore.distance);
        rec.check("camproj07_ortho_orbit_changes_direction",
                  oAfter.yaw != oBefore.yaw && oAfter.pitch != oBefore.pitch);
        // The orthographic eye is the pulled-back view plane, so it stays at a
        // fixed radius from the target while the direction turns.
        const Vec3 d = vec3Sub(oAfter.eye, oAfter.target);
        rec.check("camproj07_ortho_eye_stays_on_view_plane_radius",
                  nearly(std::sqrt(vec3Dot(d, d)), kOrthoViewPlaneDistance, 1e-2f));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-08 — an orthographic pinch changes the ortho span, is bounded,
    // and touches nothing else about the camera pose.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        switchTo(c, ProjectionMode::Orthographic);
        const float spanBefore = c.orthoHalfHeightMeters();
        const float distBefore = c.distance();

        pinch(c, 200.0f, 420.0f);  // spread = zoom in
        rec.check("camproj08_spread_shrinks_span", c.orthoHalfHeightMeters() < spanBefore - 1e-3f);
        rec.check("camproj08_pinch_does_not_fake_zoom_with_distance",
                  c.distance() == distBefore);
        const float spanIn = c.orthoHalfHeightMeters();
        pinch(c, 420.0f, 200.0f);  // close = zoom out
        rec.check("camproj08_close_grows_span", c.orthoHalfHeightMeters() > spanIn + 1e-3f);

        // The span is genuinely what is drawn: halving it must double the
        // on-screen size of a fixed segment.
        CameraController m = makeAxisAlignedController(1080, 2400);
        switchTo(m, ProjectionMode::Orthographic);
        const CameraSnapshot wide = m.snapshot();
        const float wideLen =
            projectPoint(wide, Vec3{0.5f, 0.0f, 0.0f}).x - projectPoint(wide, Vec3{-0.5f, 0.0f, 0.0f}).x;
        // Five spreads, deliberately short of the clamp, so the ratio below is
        // measuring the scale law rather than the clamp.
        for (int i = 0; i < 5; ++i) {
            pinch(m, 200.0f, 260.0f);
        }
        const CameraSnapshot tight = m.snapshot();
        const float tightLen = projectPoint(tight, Vec3{0.5f, 0.0f, 0.0f}).x -
                               projectPoint(tight, Vec3{-0.5f, 0.0f, 0.0f}).x;
        const float drawnRatio = tightLen / wideLen;
        const float spanRatio = wide.orthoHalfHeightMeters / tight.orthoHalfHeightMeters;
        rec.check("camproj08_smaller_span_draws_larger",
                  drawnRatio > 1.5f && tight.orthoHalfHeightMeters > kMinOrthoHalfHeightMeters &&
                      nearly(drawnRatio / spanRatio, 1.0f, 1e-3f));

        // Clamps: finite, positive, never inverted, at both ends.
        CameraController lo = makeAxisAlignedController(1080, 2400);
        switchTo(lo, ProjectionMode::Orthographic);
        for (int i = 0; i < 200; ++i) {
            pinch(lo, 100.0f, 900.0f);
        }
        rec.check("camproj08_span_clamps_at_minimum",
                  nearly(lo.orthoHalfHeightMeters(), kMinOrthoHalfHeightMeters, 1e-4f));
        rec.check("camproj08_min_span_still_finite_and_positive",
                  lo.orthoHalfHeightMeters() > 0.0f && snapshotFinite(lo) &&
                      lo.snapshot().proj.m[0] > 0.0f && lo.snapshot().proj.m[5] < 0.0f);

        CameraController hi = makeAxisAlignedController(1080, 2400);
        switchTo(hi, ProjectionMode::Orthographic);
        for (int i = 0; i < 200; ++i) {
            pinch(hi, 900.0f, 100.0f);
        }
        rec.check("camproj08_span_clamps_at_maximum",
                  nearly(hi.orthoHalfHeightMeters(), kMaxOrthoHalfHeightMeters, 1e-2f));
        rec.check("camproj08_max_span_still_finite", snapshotFinite(hi));

        // Pan in Orthographic moves the target and is scaled by the SPAN, not by
        // the distance — the same "one pixel of finger is one pixel of world"
        // contract Perspective has.
        CameraController p = makeAxisAlignedController(1080, 2400);
        switchTo(p, ProjectionMode::Orthographic);
        const float spanBeforePan = p.orthoHalfHeightMeters();
        const Vec3 t0 = p.target();
        down(p, 0, 400.0f, 1000.0f);
        pointerDown2(p, TouchPointer{0, 400.0f, 1000.0f}, TouchPointer{1, 600.0f, 1000.0f});
        move2(p, TouchPointer{0, 500.0f, 1000.0f}, TouchPointer{1, 700.0f, 1000.0f});
        const Vec3 t1 = p.target();
        const float moved = std::sqrt(vec3Dot(vec3Sub(t1, t0), vec3Sub(t1, t0)));
        const float expected = 100.0f * (2.0f * p.orthoHalfHeightMeters()) / 2400.0f;
        const float spanAfterPan = p.orthoHalfHeightMeters();
        rec.check("camproj08_ortho_pan_moves_target", moved > 1e-4f);
        rec.check("camproj08_ortho_pan_scaled_by_span", nearly(moved, expected, 1e-3f));
        // Pan slides the target only: it must not zoom and must not orbit.
        rec.check("camproj08_ortho_pan_keeps_span_and_angles",
                  spanAfterPan == spanBeforePan && nearly(p.yaw(), 0.0f, 1e-4f) &&
                      nearly(p.pitch(), 0.0f, 1e-4f) && p.distance() == kInitialDistance);
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-12 — the projection is PRESENTATION. Switching it, pinching in
    // ortho and orbiting must leave every piece of geometry truth alone.
    //
    // The camera owns no mesh, so the strongest statement this suite can make is
    // structural: the camera module exposes no way to reach a mesh, a revision,
    // a Construction parameter or a transform, and the snapshot it hands out is
    // a pure function of the pose. The revision side of CAMPROJ-12 is asserted
    // where revisions live — see the sculpt and render-shading suites, which
    // already prove a presentation change mints nothing.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        const CameraSnapshot base = c.snapshot();

        // A projection switch is a pure camera act: the same pose in, the same
        // pose out.
        c.setProjectionMode(ProjectionMode::Orthographic);
        c.setProjectionMode(ProjectionMode::Perspective);
        const CameraSnapshot after = c.snapshot();
        rec.check("camproj12_round_trip_restores_pose_exactly",
                  nearly(after.distance, base.distance, 1e-3f) && after.yaw == base.yaw &&
                      after.pitch == base.pitch && after.target.x == base.target.x &&
                      after.target.y == base.target.y && after.target.z == base.target.z);
        rec.check("camproj12_round_trip_restores_projection_matrix",
                  nearly(after.proj.m[0], base.proj.m[0], 1e-4f) &&
                      nearly(after.proj.m[5], base.proj.m[5], 1e-4f) &&
                      after.proj.m[11] == base.proj.m[11]);

        // The snapshot is a pure function of the pose: taking it twice with no
        // intervening input yields identical matrices, so no frame can observe
        // the camera drifting on its own.
        const CameraSnapshot a = c.snapshot();
        const CameraSnapshot b = c.snapshot();
        bool identical = true;
        for (int i = 0; i < 16; ++i) {
            identical = identical && a.proj.m[i] == b.proj.m[i] && a.view.m[i] == b.view.m[i];
        }
        rec.check("camproj12_snapshot_is_pure", identical);
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-13 — the projection mode and its framing are process-scoped, so a
    // HOME/resume and a Surface recreation preserve them.
    //
    // The Android layer stores nothing: the one CameraController outlives every
    // Surface. What a resume actually does to the camera is setViewport() and
    // resetGesture(), so that is what this drives — the real sequence, not a
    // stand-in for it.
    // -----------------------------------------------------------------------
    {
        CameraController c = makeAxisAlignedController(1080, 2400);
        switchTo(c, ProjectionMode::Orthographic);
        pinch(c, 200.0f, 380.0f);
        down(c, 0, 300.0f, 1000.0f);
        move1(c, 0, 380.0f, 1080.0f);
        c.resetGesture();

        const ProjectionMode mode = c.projectionMode();
        const float span = c.orthoHalfHeightMeters();
        const float yaw = c.yaw(), pitch = c.pitch(), dist = c.distance();
        const Vec3 target = c.target();

        // surfaceDestroyed -> surfaceCreated -> surfaceChanged, exactly as JNI
        // drives it, including the redundant second setViewport a resume makes.
        c.resetGesture();
        c.setViewport(1080, 2400);
        c.setViewport(1080, 2400);
        c.resetGesture();

        rec.check("camproj13_resume_preserves_projection_mode", c.projectionMode() == mode);
        rec.check("camproj13_resume_preserves_ortho_span",
                  c.orthoHalfHeightMeters() == span);
        rec.check("camproj13_resume_preserves_pose",
                  c.yaw() == yaw && c.pitch() == pitch && c.distance() == dist &&
                      c.target().x == target.x && c.target().y == target.y &&
                      c.target().z == target.z);
        rec.check("camproj13_resume_snapshot_finite", snapshotFinite(c));
    }

    // -----------------------------------------------------------------------
    // CAMPROJ-14 — the P2 orientation convention holds for BOTH projections on
    // a non-square viewport, in portrait and in rotated landscape.
    //
    // The convention is that there is ONE coordinate space: the projection takes
    // its aspect from the window and no display rotation is folded into any
    // matrix. The invariant that proves it is that a shape's proportions are
    // preserved — a square in the world stays square on screen, in both
    // orientations and in both modes.
    // -----------------------------------------------------------------------
    {
        struct Case {
            int w, h;
        };
        // Portrait and rotated landscape, the two the P2 convention had to fix.
        const Case cases[2] = {{1080, 2400}, {2400, 1080}};

        bool perspectiveSquareOk = true;
        bool orthoSquareOk = true;
        bool aspectFollowsWindow = true;
        bool allFinite = true;

        for (int i = 0; i < 2; ++i) {
            const float aspect = static_cast<float>(cases[i].w) / static_cast<float>(cases[i].h);

            for (int mode = 0; mode < 2; ++mode) {
                CameraController c = makeAxisAlignedController(cases[i].w, cases[i].h);
                if (mode == 1) {
                    switchTo(c, ProjectionMode::Orthographic);
                }
                const CameraSnapshot s = c.snapshot();
                allFinite = allFinite && snapshotFinite(c);

                // The projection's aspect IS the window's aspect. No transpose,
                // no rotation, no second convention.
                aspectFollowsWindow =
                    aspectFollowsWindow &&
                    nearly(std::fabs(s.proj.m[0] / s.proj.m[5]), 1.0f / aspect, 1e-4f);

                // A 1 m x 1 m world square at the target plane must occupy the
                // same number of PIXELS horizontally and vertically. NDC is
                // normalized per axis, so the pixel conversion is where a
                // non-uniform scale would show up.
                const float ndcW = projectPoint(s, Vec3{0.5f, 0.0f, 0.0f}).x -
                                   projectPoint(s, Vec3{-0.5f, 0.0f, 0.0f}).x;
                const float ndcH = projectPoint(s, Vec3{0.0f, 0.5f, 0.0f}).y -
                                   projectPoint(s, Vec3{0.0f, -0.5f, 0.0f}).y;
                const float pixelW = std::fabs(ndcW) * 0.5f * static_cast<float>(cases[i].w);
                const float pixelH = std::fabs(ndcH) * 0.5f * static_cast<float>(cases[i].h);
                const bool square = nearly(pixelW / pixelH, 1.0f, 1e-3f);
                if (mode == 0) {
                    perspectiveSquareOk = perspectiveSquareOk && square;
                } else {
                    orthoSquareOk = orthoSquareOk && square;
                }
            }
        }

        rec.check("camproj14_perspective_square_stays_square_both_orientations",
                  perspectiveSquareOk);
        rec.check("camproj14_orthographic_square_stays_square_both_orientations", orthoSquareOk);
        rec.check("camproj14_aspect_follows_window_both_modes", aspectFollowsWindow);
        rec.check("camproj14_both_modes_finite_both_orientations", allFinite);

        // A rotated-landscape ortho view must still frame the same world height
        // as portrait does: the half-height is the vertical span in both, which
        // is what makes the one-convention rule observable here.
        CameraController portrait = makeAxisAlignedController(1080, 2400);
        switchTo(portrait, ProjectionMode::Orthographic);
        CameraController landscape = makeAxisAlignedController(2400, 1080);
        switchTo(landscape, ProjectionMode::Orthographic);
        rec.check("camproj14_ortho_half_height_is_orientation_independent",
                  portrait.orthoHalfHeightMeters() == landscape.orthoHalfHeightMeters());
        rec.check("camproj14_ortho_landscape_is_wider_not_taller",
                  landscape.snapshot().proj.m[0] < portrait.snapshot().proj.m[0] &&
                      landscape.snapshot().proj.m[5] == portrait.snapshot().proj.m[5]);
    }

    // The default ortho framing constant must stay in step with the perspective
    // one it was derived from. It is a literal because std::tan is not
    // constexpr, so this is the check that stops the two drifting apart.
    rec.check("camproj_initial_ortho_span_matches_initial_distance",
              nearly(kInitialOrthoHalfHeightMeters, kInitialDistance * tanHalfFov, 1e-3f));
    rec.check("camproj_mode_index_mapping_is_closed",
              projectionModeIndex(ProjectionMode::Perspective) == 0 &&
                  projectionModeIndex(ProjectionMode::Orthographic) == 1);
    {
        ProjectionMode m = ProjectionMode::Perspective;
        rec.check("camproj_unknown_mode_index_is_refused",
                  projectionModeFromIndex(0, &m) && m == ProjectionMode::Perspective &&
                      projectionModeFromIndex(1, &m) && m == ProjectionMode::Orthographic &&
                      !projectionModeFromIndex(2, &m) && !projectionModeFromIndex(-1, &m) &&
                      m == ProjectionMode::Orthographic);
    }
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

    runProjectionSelfTests(rec);

    return rec.n;
}

}  // namespace forgeshape
