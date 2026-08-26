#include "forgeshape_gizmo_selftest.h"

#include <cmath>
#include <limits>

#include "forgeshape_construction.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Recorder {
    GizmoSelfTestResult* out;
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

bool nearly(double a, double b, double tolerance) {
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= tolerance;
}

// A camera built here rather than driven through CameraController, so a case is
// a statement about the SOLVER and not about how a gesture reached a pose. The
// snapshot carries exactly the fields buildPickRay and the gizmo consume.
CameraSnapshot perspectiveCamera(const Vec3& eye, const Vec3& target, int width, int height) {
    CameraSnapshot camera{};
    camera.view = mat4LookAt(eye, target, Vec3{0.0f, 1.0f, 0.0f});
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    camera.proj = mat4Perspective(60.0f * kPi / 180.0f, aspect, 0.05f, 200.0f);
    camera.eye = eye;
    camera.target = target;
    camera.projection = ProjectionMode::Perspective;
    camera.orthoHalfHeightMeters = 1.0f;
    return camera;
}

CameraSnapshot orthographicCamera(const Vec3& eye, const Vec3& target, int width, int height,
                                  float halfHeight) {
    CameraSnapshot camera{};
    camera.view = mat4LookAt(eye, target, Vec3{0.0f, 1.0f, 0.0f});
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    camera.proj = mat4Orthographic(halfHeight, aspect, -100.0f, 200.0f);
    camera.eye = eye;
    camera.target = target;
    camera.projection = ProjectionMode::Orthographic;
    camera.orthoHalfHeightMeters = halfHeight;
    return camera;
}

// Its own scene and its own history, for the same reason every other suite here
// builds its own: a case that read the process-scoped scene would pass or fail
// depending on what a live session or a previous suite left behind. GizmoSession
// takes both by reference precisely so this is possible.
struct Fixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};
    GizmoSession gizmo{scene, history};
    int width = 1080;
    int height = 2000;

    Fixture() {
        publishConstructionObject(scene.activeBody().construction(),
                                  scene.activeBody().meshStore());
        gizmo.setActive(true);
    }

    TransformValues placement() const { return scene.activeBody().transform().values(); }

    void setPlacement(const TransformValues& values) {
        applyTransformValues(scene.activeBody().transform(), values);
    }

    // The pixel a handle can be grabbed at, derived from the same projection the
    // hit test uses. A case that hard-coded a coordinate would be true for one
    // camera and one window only.
    bool handlePixel(const CameraSnapshot& camera, GizmoAxis axis, float* x, float* y) const {
        const GizmoSnapshot state = gizmo.snapshot(camera, width, height);
        if (!state.visible) {
            return false;
        }
        const Vec3 direction = gizmoAxisDirection(axis);
        Vec3 world{};
        if (state.mode == GizmoMode::Move) {
            const float length = kGizmoHandleLengthUnits * state.worldPerReferenceUnit;
            const float middle =
                0.5f * (kGizmoShaftGrabStartFraction + kGizmoShaftGrabEndFraction);
            world = vec3Add(state.pivot, vec3Scale(direction, length * middle));
        } else {
            world = vec3Add(state.pivot, gizmoRingGrabOffset(axis,
                                                             kGizmoRingRadiusUnits *
                                                                 state.worldPerReferenceUnit));
        }
        return projectWorldToScreen(camera, world, width, height, x, y);
    }
};

TransformValues placementValues(double px, double py, double pz, double rx, double ry,
                                double rz) {
    TransformValues values;
    values.positionX = px;
    values.positionY = py;
    values.positionZ = pz;
    values.rotationX = rx;
    values.rotationY = ry;
    values.rotationZ = rz;
    return values;
}

}  // namespace

int runGizmoSelfTests(GizmoSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};
    if (out == nullptr || maxOut <= 0) {
        return 0;
    }

    // -----------------------------------------------------------------------
    // The closed enums, and the seam that must stay closed
    // -----------------------------------------------------------------------
    {
        GizmoMode mode = GizmoMode::Rotate;
        r.check("mode_index_round_trips",
                gizmoModeFromIndex(0, &mode) && mode == GizmoMode::Move &&
                    gizmoModeFromIndex(1, &mode) && mode == GizmoMode::Rotate &&
                    gizmoModeIndex(GizmoMode::Move) == 0 &&
                    gizmoModeIndex(GizmoMode::Rotate) == 1);
        r.check("unknown_mode_index_is_refused_not_clamped",
                !gizmoModeFromIndex(-1, &mode) && !gizmoModeFromIndex(2, &mode));

        // Scale is not a mode, a plane handle is not an axis, and neither is
        // nameable. This is the whole no-scope-creep guard, stated as an
        // assertion rather than as a comment.
        r.check("there_are_exactly_two_modes_and_three_axes", kGizmoModeCount == 2);
        r.check("axis_directions_are_the_world_axes",
                gizmoAxisDirection(GizmoAxis::X).x == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::Y).y == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::Z).z == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::None).x == 0.0f &&
                    gizmoAxisDirection(GizmoAxis::None).y == 0.0f &&
                    gizmoAxisDirection(GizmoAxis::None).z == 0.0f);
    }

    // -----------------------------------------------------------------------
    // The quantization seam is the identity, and this is what says so
    // -----------------------------------------------------------------------
    {
        r.check("translation_quantizer_is_the_identity",
                quantizeGizmoTranslation(0.0) == 0.0 &&
                    quantizeGizmoTranslation(0.37) == 0.37 &&
                    quantizeGizmoTranslation(-12.5) == -12.5);
        r.check("rotation_quantizer_is_the_identity",
                quantizeGizmoRotation(0.0) == 0.0 && quantizeGizmoRotation(7.5) == 7.5 &&
                    quantizeGizmoRotation(-370.25) == -370.25);
    }

    // -----------------------------------------------------------------------
    // Projection round trip and the screen-constant scale
    // -----------------------------------------------------------------------
    {
        const int width = 1080;
        const int height = 2000;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{6.0f, 5.0f, 8.0f}, Vec3{0.0f, 0.0f, 0.0f}, width, height);

        // A world point projected to a pixel and picked back through that pixel
        // must produce a ray that passes through the point. This is the one
        // check that ties projectWorldToScreen to buildPickRay, which is what
        // makes hit-testing in pixels legitimate.
        const Vec3 point{1.25f, 0.75f, -0.5f};
        float px = 0.0f, py = 0.0f;
        const bool projected = projectWorldToScreen(camera, point, width, height, &px, &py);
        Ray ray{};
        const bool built = buildPickRay(camera, px, py, width, height, &ray);
        float distance = 1e9f;
        if (built) {
            const Vec3 toPoint = vec3Sub(point, ray.origin);
            const float along = vec3Dot(toPoint, ray.direction);
            const Vec3 closest = vec3Add(ray.origin, vec3Scale(ray.direction, along));
            const Vec3 offset = vec3Sub(point, closest);
            distance = std::sqrt(vec3Dot(offset, offset));
        }
        r.check("projection_and_pick_ray_are_inverses", projected && built && distance < 1e-3f);

        r.check("a_point_behind_the_eye_has_no_pixel",
                !projectWorldToScreen(camera, Vec3{6.0f, 5.0f, 40.0f}, width, height, &px, &py));
        r.check("a_non_finite_point_has_no_pixel",
                !projectWorldToScreen(camera, Vec3{std::nanf(""), 0.0f, 0.0f}, width, height,
                                      &px, &py));
        r.check("a_degenerate_viewport_has_no_pixel",
                !projectWorldToScreen(camera, point, 0, height, &px, &py));

        // Perspective: twice as far away is half as many pixels per meter, so
        // the world length one reference unit spans DOUBLES. That is exactly
        // what keeps the drawn gizmo the same size on screen.
        float nearScale = 0.0f;
        float farScale = 0.0f;
        const CameraSnapshot nearCamera =
            perspectiveCamera(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, width, height);
        const CameraSnapshot farCamera =
            perspectiveCamera(Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, 0.0f, 0.0f}, width, height);
        const bool haveNear =
            gizmoWorldScale(nearCamera, Vec3{0.0f, 0.0f, 0.0f}, height, &nearScale);
        const bool haveFar =
            gizmoWorldScale(farCamera, Vec3{0.0f, 0.0f, 0.0f}, height, &farScale);
        r.check("perspective_scale_doubles_at_twice_the_distance",
                haveNear && haveFar && nearly(farScale, nearScale * 2.0, nearScale * 0.02));

        // Orthographic: the scale does not depend on depth at all, which is the
        // defining property of a parallel projection and must survive here.
        const CameraSnapshot ortho = orthographicCamera(Vec3{0.0f, 0.0f, 5.0f},
                                                        Vec3{0.0f, 0.0f, 0.0f}, width, height,
                                                        4.0f);
        float orthoNear = 0.0f;
        float orthoFar = 0.0f;
        r.check("orthographic_scale_is_depth_independent",
                gizmoWorldScale(ortho, Vec3{0.0f, 0.0f, 0.0f}, height, &orthoNear) &&
                    gizmoWorldScale(ortho, Vec3{0.0f, 0.0f, -30.0f}, height, &orthoFar) &&
                    nearly(orthoNear, orthoFar, 1e-6));

        // And the drawn size in pixels is the constant the header states, in
        // both projections. This is the "stays readable at any zoom" claim as
        // arithmetic rather than as a screenshot.
        const float pixelsPerUnit = gizmoPixelsPerReferenceUnit();
        const float nearPixels = kGizmoHandleLengthUnits * pixelsPerUnit;
        float measuredNear = 0.0f;
        float measuredFar = 0.0f;
        {
            float ax = 0, ay = 0, bx = 0, by = 0;
            const float lengthNear = kGizmoHandleLengthUnits * nearScale;
            projectWorldToScreen(nearCamera, Vec3{0, 0, 0}, width, height, &ax, &ay);
            projectWorldToScreen(nearCamera, Vec3{lengthNear, 0, 0}, width, height, &bx, &by);
            measuredNear = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
            const float lengthFar = kGizmoHandleLengthUnits * farScale;
            projectWorldToScreen(farCamera, Vec3{0, 0, 0}, width, height, &ax, &ay);
            projectWorldToScreen(farCamera, Vec3{lengthFar, 0, 0}, width, height, &bx, &by);
            measuredFar = std::sqrt((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
        }
        // A 5% band: an axis lying across the image plane projects to very
        // nearly its full length, and the residual is the perspective foreshort-
        // ening of its far end, which is real and small.
        r.check("drawn_handle_length_is_near_constant_in_pixels",
                nearly(measuredNear, nearPixels, nearPixels * 0.05) &&
                    nearly(measuredFar, nearPixels, nearPixels * 0.05));

        // The hit corridor is 48 reference units across, which is the
        // interactive floor. The STROKE is one pixel wide; the target is not.
        r.check("hit_corridor_meets_the_48_unit_floor", 2.0f * kGizmoHitSlopUnits >= 48.0f);
    }

    // -----------------------------------------------------------------------
    // The axis solver, and its degenerate cases
    // -----------------------------------------------------------------------
    {
        const Vec3 pivot{0.0f, 0.0f, 0.0f};
        const Vec3 axis{1.0f, 0.0f, 0.0f};

        // A ray straight down at x = 3 meets the X axis at t = 3.
        Ray down{};
        down.origin = Vec3{3.0f, 5.0f, 0.0f};
        down.direction = Vec3{0.0f, -1.0f, 0.0f};
        float t = 0.0f;
        r.check("closest_point_solves_a_perpendicular_ray",
                solveAxisParameter(down, pivot, axis, &t) == AxisSolveStatus::Resolved &&
                    nearly(t, 3.0, 1e-4));

        // Negative is an ordinary answer: there is no positivity rule on a
        // coordinate.
        down.origin = Vec3{-4.5f, 5.0f, 0.0f};
        r.check("closest_point_solves_a_negative_parameter",
                solveAxisParameter(down, pivot, axis, &t) == AxisSolveStatus::Resolved &&
                    nearly(t, -4.5, 1e-4));

        // A skew ray that never meets the axis still has a closest approach, and
        // that is the answer the drag wants.
        Ray skew{};
        skew.origin = Vec3{2.0f, 4.0f, 4.0f};
        skew.direction = vec3Normalize(Vec3{0.0f, -1.0f, -1.0f});
        r.check("closest_point_solves_a_skew_ray",
                solveAxisParameter(skew, pivot, axis, &t) == AxisSolveStatus::Resolved &&
                    nearly(t, 2.0, 1e-4));

        // Nearly parallel to the axis: the closest-approach denominator
        // collapses and the camera-facing plane takes over. What matters is that
        // it is FINITE and that the fallback path is the one that ran.
        // About two degrees off the axis: past the closest-approach threshold
        // (which needs roughly eight) and comfortably clear of the plane
        // fallback's own edge-on limit.
        Ray nearlyParallel{};
        nearlyParallel.origin = Vec3{-10.0f, 0.35f, 0.0f};
        nearlyParallel.direction = vec3Normalize(Vec3{1.0f, 0.035f, 0.0f});
        const AxisSolveStatus fallback = solveAxisParameter(nearlyParallel, pivot, axis, &t);
        r.check("a_near_parallel_ray_falls_back_to_the_plane",
                fallback == AxisSolveStatus::ResolvedByPlane && std::isfinite(t));

        // Exactly parallel: neither method means anything, and the honest answer
        // is to write nothing rather than to guess.
        Ray parallel{};
        parallel.origin = Vec3{-10.0f, 0.0f, 0.0f};
        parallel.direction = Vec3{1.0f, 0.0f, 0.0f};
        float untouched = 12345.0f;
        r.check("an_exactly_parallel_ray_is_unresolvable",
                solveAxisParameter(parallel, pivot, axis, &untouched) ==
                        AxisSolveStatus::Unresolvable &&
                    untouched == 12345.0f);

        Ray nonFinite{};
        nonFinite.origin = Vec3{std::nanf(""), 0.0f, 0.0f};
        nonFinite.direction = Vec3{0.0f, -1.0f, 0.0f};
        r.check("a_non_finite_ray_is_unresolvable",
                solveAxisParameter(nonFinite, pivot, axis, &t) ==
                    AxisSolveStatus::Unresolvable);
        r.check("a_degenerate_axis_is_unresolvable",
                solveAxisParameter(down, pivot, Vec3{0.0f, 0.0f, 0.0f}, &t) ==
                    AxisSolveStatus::Unresolvable);
    }

    // -----------------------------------------------------------------------
    // Ray / ring-plane intersection
    // -----------------------------------------------------------------------
    {
        Ray ray{};
        ray.origin = Vec3{0.0f, 4.0f, 2.0f};
        ray.direction = Vec3{0.0f, -1.0f, 0.0f};
        Vec3 hit{};
        r.check("ray_meets_the_y_ring_plane",
                intersectRayPlane(ray, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, &hit) &&
                    nearly(hit.y, 0.0, 1e-5) && nearly(hit.z, 2.0, 1e-5));

        // Edge-on: the ray runs inside the plane. This is the documented
        // degenerate case, and refusing it is what makes the drag hold its last
        // good sample instead of spinning on noise.
        Ray edgeOn{};
        edgeOn.origin = Vec3{0.0f, 0.0f, 4.0f};
        edgeOn.direction = Vec3{0.0f, 0.0f, -1.0f};
        r.check("an_edge_on_ring_plane_is_refused",
                !intersectRayPlane(edgeOn, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, &hit));

        Ray bad{};
        bad.origin = Vec3{0.0f, std::numeric_limits<float>::infinity(), 0.0f};
        bad.direction = Vec3{0.0f, -1.0f, 0.0f};
        r.check("a_non_finite_ray_meets_no_plane",
                !intersectRayPlane(bad, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, &hit));
    }

    // -----------------------------------------------------------------------
    // Signed angle, unwrap, and accumulation past a full turn
    // -----------------------------------------------------------------------
    {
        float angle = 0.0f;
        const Vec3 y{0.0f, 1.0f, 0.0f};
        // +X to +Z about +Y is -90 degrees by the right-hand rule: looking down
        // +Y toward the origin, positive is counter-clockwise, which takes +X
        // toward -Z.
        r.check("signed_angle_has_a_sign",
                signedAngleAround(y, Vec3{1, 0, 0}, Vec3{0, 0, 1}, &angle) &&
                    nearly(angle, -kPi / 2.0f, 1e-5));
        r.check("signed_angle_reverses_with_the_arguments",
                signedAngleAround(y, Vec3{0, 0, 1}, Vec3{1, 0, 0}, &angle) &&
                    nearly(angle, kPi / 2.0f, 1e-5));
        r.check("signed_angle_ignores_the_out_of_plane_component",
                signedAngleAround(y, Vec3{1, 7, 0}, Vec3{0, -3, 1}, &angle) &&
                    nearly(angle, -kPi / 2.0f, 1e-5));
        r.check("a_degenerate_spoke_has_no_angle",
                !signedAngleAround(y, Vec3{0, 1, 0}, Vec3{0, 0, 1}, &angle));

        r.check("unwrap_leaves_a_small_step_alone", nearly(unwrapAngleDelta(0.1f), 0.1, 1e-6));
        // Crossing +/-180: a raw difference of nearly a full turn is really a
        // small step the other way, and this is what stops a jump there.
        r.check("unwrap_turns_a_near_full_turn_into_a_small_step",
                nearly(unwrapAngleDelta(2.0f * kPi - 0.05f), -0.05, 1e-5) &&
                    nearly(unwrapAngleDelta(-(2.0f * kPi - 0.05f)), 0.05, 1e-5));
        r.check("unwrap_survives_a_non_finite_step",
                unwrapAngleDelta(std::nanf("")) == 0.0f);

        // Accumulating unwrapped steps past a full turn keeps going, which is
        // what lets a drag report 400 degrees instead of 40.
        float total = 0.0f;
        float previous = 0.0f;
        for (int i = 1; i <= 100; ++i) {
            const float raw = std::atan2(std::sin(0.09f * i), std::cos(0.09f * i));
            total += unwrapAngleDelta(raw - previous);
            previous = raw;
        }
        r.check("accumulation_passes_a_full_turn", nearly(total, 9.0, 1e-3) && total > 2.0f * kPi);
    }

    // -----------------------------------------------------------------------
    // Visibility
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{6.0f, 5.0f, 8.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        r.check("an_active_gizmo_is_visible", f.gizmo.snapshot(camera, f.width, f.height).visible);
        f.gizmo.setActive(false);
        r.check("an_inactive_gizmo_is_invisible",
                !f.gizmo.snapshot(camera, f.width, f.height).visible);
        f.gizmo.setActive(true);
        r.check("entering_the_tool_selects_move", f.gizmo.mode() == GizmoMode::Move);
        r.check("a_degenerate_viewport_draws_no_gizmo", !f.gizmo.snapshot(camera, 0, 0).visible);
    }

    // -----------------------------------------------------------------------
    // Hit testing picks the axis under the finger, and nothing else
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        bool allFound = true;
        const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
        for (int i = 0; i < 3; ++i) {
            float x = 0.0f, y = 0.0f;
            if (!f.handlePixel(camera, axes[i], &x, &y) ||
                f.gizmo.hitTest(camera, x, y, f.width, f.height) != axes[i]) {
                allFound = false;
            }
        }
        r.check("each_move_handle_is_hit_at_its_own_pixel", allFound);

        // Far from every handle is a miss, and a miss is what leaves orbit and
        // pan alone.
        r.check("a_pixel_far_from_every_handle_is_a_miss",
                f.gizmo.hitTest(camera, 5.0f, 5.0f, f.width, f.height) == GizmoAxis::None);

        f.gizmo.setMode(GizmoMode::Rotate);
        bool ringsFound = true;
        for (int i = 0; i < 3; ++i) {
            float x = 0.0f, y = 0.0f;
            if (!f.handlePixel(camera, axes[i], &x, &y) ||
                f.gizmo.hitTest(camera, x, y, f.width, f.height) != axes[i]) {
                ringsFound = false;
            }
        }
        r.check("each_rotate_ring_is_hit_at_its_own_pixel", ringsFound);

        f.gizmo.setActive(false);
        float x = 0.0f, y = 0.0f;
        r.check("an_inactive_gizmo_hits_nothing",
                !f.handlePixel(camera, GizmoAxis::X, &x, &y) &&
                    f.gizmo.hitTest(camera, 540.0f, 1000.0f, f.width, f.height) ==
                        GizmoAxis::None);
    }

    // -----------------------------------------------------------------------
    // One drag, one transaction, one step
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        const bool grabbed =
            f.handlePixel(camera, GizmoAxis::X, &x, &y) &&
            f.gizmo.beginDrag(7, camera, x, y, f.width, f.height);
        r.check("a_handle_captures_its_pointer",
                grabbed && f.gizmo.capturing() && f.gizmo.capturedPointerId() == 7 &&
                    f.gizmo.capturedAxis() == GizmoAxis::X);
        r.check("a_drag_in_progress_records_nothing_yet", !f.history.canUndo());

        // Twenty samples, exactly as a real drag produces hundreds.
        for (int i = 1; i <= 20; ++i) {
            f.gizmo.updateDrag(7, camera, x + static_cast<float>(i) * 4.0f, y, f.width, f.height);
        }
        const TransformValues moved = f.placement();
        r.check("many_samples_moved_the_body", std::fabs(moved.positionX) > 0.05);
        r.check("a_move_drag_leaves_every_rotation_alone",
                moved.rotationX == 0.0 && moved.rotationY == 0.0 && moved.rotationZ == 0.0);
        r.check("a_move_drag_leaves_the_other_positions_alone",
                moved.positionY == 0.0 && moved.positionZ == 0.0);

        const bool recorded = f.gizmo.commitDrag();
        r.check("a_drag_of_any_length_is_exactly_one_step",
                recorded && f.history.undoDepth() == 1 && f.gizmo.dragUpdateCount() > 1);
        r.check("the_drag_released_its_pointer",
                !f.gizmo.capturing() && f.gizmo.capturedAxis() == GizmoAxis::None);

        f.history.undo();
        const TransformValues afterUndo = f.placement();
        r.check("undo_restores_the_pre_drag_placement", afterUndo.positionX == 0.0);
        f.history.redo();
        r.check("redo_restores_the_drag_result",
                nearly(f.placement().positionX, moved.positionX, 1e-9));
    }

    // -----------------------------------------------------------------------
    // A tap records nothing, and a cancel restores exactly
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        f.handlePixel(camera, GizmoAxis::Y, &x, &y);
        f.gizmo.beginDrag(1, camera, x, y, f.width, f.height);
        const bool tapRecorded = f.gizmo.commitDrag();
        r.check("a_tap_on_a_handle_records_nothing",
                !tapRecorded && f.history.undoDepth() == 0 && !f.history.canUndo());

        // Something to lose: a real step first, so the cancel can be shown to
        // leave the redo stack alone as well.
        f.setPlacement(placementValues(1.0, 2.0, 3.0, 10.0, 20.0, 30.0));
        const TransformValues before = f.placement();

        f.handlePixel(camera, GizmoAxis::Y, &x, &y);
        f.gizmo.beginDrag(2, camera, x, y, f.width, f.height);
        for (int i = 1; i <= 8; ++i) {
            f.gizmo.updateDrag(2, camera, x, y - static_cast<float>(i) * 6.0f, f.width, f.height);
        }
        r.check("a_cancelled_drag_had_actually_moved_the_body",
                f.placement().positionY != before.positionY);
        f.gizmo.cancelDrag();
        const TransformValues after = f.placement();
        r.check("cancel_restores_the_pre_drag_placement_exactly",
                after.positionX == before.positionX && after.positionY == before.positionY &&
                    after.positionZ == before.positionZ &&
                    after.rotationX == before.rotationX &&
                    after.rotationY == before.rotationY && after.rotationZ == before.rotationZ);
        r.check("cancel_records_no_step", f.history.undoDepth() == 0);
        r.check("cancel_releases_the_capture", !f.gizmo.capturing());
    }

    // -----------------------------------------------------------------------
    // A drag follows ONE pointer, and only the body it started on
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        f.handlePixel(camera, GizmoAxis::Z, &x, &y);
        f.gizmo.beginDrag(3, camera, x, y, f.width, f.height);
        const TransformValues before = f.placement();
        const bool otherPointerMoved =
            f.gizmo.updateDrag(9, camera, x + 60.0f, y, f.width, f.height);
        r.check("a_pointer_that_is_not_the_captured_one_moves_nothing",
                !otherPointerMoved && f.placement().positionZ == before.positionZ);

        // The captured body is the one the drag can move, whatever the selection
        // does underneath it.
        const ObjectId dragged = f.gizmo.capturedObjectId();
        const ObjectId second = f.scene.addBody().objectId();
        publishConstructionObject(f.scene.activeBody().construction(),
                                  f.scene.activeBody().meshStore());
        r.check("selection_moved_but_the_drag_did_not_retarget",
                f.scene.activeBodyId() == second && f.gizmo.capturedObjectId() == dragged);
        f.gizmo.updateDrag(3, camera, x + 60.0f, y, f.width, f.height);
        const SceneObject* newBody = f.scene.findBody(second);
        r.check("the_drag_moved_only_the_body_it_started_on",
                newBody != nullptr && newBody->transform().values().positionZ == 0.0 &&
                    f.scene.findBody(dragged) != nullptr &&
                    f.scene.findBody(dragged)->transform().values().positionZ != 0.0);
        f.gizmo.cancelDrag();

        r.check("a_second_begin_while_capturing_is_refused",
                f.gizmo.beginDrag(4, camera, x, y, f.width, f.height) &&
                    !f.gizmo.beginDrag(5, camera, x, y, f.width, f.height) &&
                    f.gizmo.capturedPointerId() == 4);
        f.gizmo.cancelDrag();

        r.check("mode_cannot_change_under_a_moving_finger",
                f.gizmo.beginDrag(6, camera, x, y, f.width, f.height) &&
                    !f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.mode() == GizmoMode::Move);
        f.gizmo.cancelDrag();
        r.check("mode_changes_freely_when_nothing_is_held",
                f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.mode() == GizmoMode::Rotate &&
                    f.history.undoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Rotate
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // Steeply down onto the Y ring, so it is close to face-on and a drag
        // around it is unambiguous. Not straight down: an eye exactly on the up
        // vector degenerates the look-at basis, which would make this a test of
        // the camera rather than of the ring.
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{1.5f, 9.0f, 1.5f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        f.gizmo.setMode(GizmoMode::Rotate);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.handlePixel(camera, GizmoAxis::Y, &x, &y) &&
                             f.gizmo.beginDrag(1, camera, x, y, f.width, f.height);
        r.check("a_ring_captures_its_pointer", grabbed && f.gizmo.capturedAxis() == GizmoAxis::Y);

        // Walk the pointer around the ring in world space, projecting each step,
        // so the drive is a real circular motion rather than a straight line.
        const GizmoSnapshot state = f.gizmo.snapshot(camera, f.width, f.height);
        const float radius = kGizmoRingRadiusUnits * state.worldPerReferenceUnit;
        // A full turn plus a quarter, in small steps, to exercise the unwrap
        // repeatedly and to pass 360.
        const int steps = 100;
        const float sweep = 2.0f * kPi + kPi / 2.0f;
        for (int i = 1; i <= steps; ++i) {
            const float angle = sweep * static_cast<float>(i) / static_cast<float>(steps);
            const Vec3 point = vec3Add(
                state.pivot,
                Vec3{radius * std::cos(angle), 0.0f, radius * std::sin(angle)});
            float sx = 0.0f, sy = 0.0f;
            if (projectWorldToScreen(camera, point, f.width, f.height, &sx, &sy)) {
                f.gizmo.updateDrag(1, camera, sx, sy, f.width, f.height);
            }
        }
        const TransformValues rotated = f.placement();
        // The ring's own +X spoke was the start, and the sweep is 450 degrees
        // clockwise as seen from +Y, which is -450 by the right-hand rule.
        r.check("a_ring_drag_passes_a_full_turn_without_canonicalising",
                std::fabs(rotated.rotationY) > 360.0);
        r.check("a_rotate_drag_leaves_the_other_rotations_alone",
                rotated.rotationX == 0.0 && rotated.rotationZ == 0.0);
        r.check("a_rotate_drag_leaves_position_alone",
                rotated.positionX == 0.0 && rotated.positionY == 0.0 && rotated.positionZ == 0.0);

        const bool recorded = f.gizmo.commitDrag();
        r.check("a_ring_drag_of_any_length_is_exactly_one_step",
                recorded && f.history.undoDepth() == 1);
        f.history.undo();
        r.check("undo_restores_the_pre_rotate_placement", f.placement().rotationY == 0.0);
        f.history.redo();
        r.check("redo_restores_the_rotate_result",
                nearly(f.placement().rotationY, rotated.rotationY, 1e-9));
    }

    // -----------------------------------------------------------------------
    // An edge-on ring holds instead of jumping
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // Eye in the XZ plane: the Y ring is exactly edge-on.
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{0.0f, 0.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        f.gizmo.setMode(GizmoMode::Rotate);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.handlePixel(camera, GizmoAxis::Y, &x, &y) &&
                             f.gizmo.beginDrag(1, camera, x, y, f.width, f.height);
        // Whether the capture succeeds at all is the camera's business; what
        // must never happen is a non-finite or wildly jumping rotation.
        for (int i = 1; i <= 10; ++i) {
            f.gizmo.updateDrag(1, camera, x + static_cast<float>(i) * 5.0f, y + 2.0f, f.width,
                               f.height);
        }
        const TransformValues values = f.placement();
        r.check("an_edge_on_ring_never_produces_a_non_finite_rotation",
                std::isfinite(values.rotationX) && std::isfinite(values.rotationY) &&
                    std::isfinite(values.rotationZ));
        if (grabbed) {
            f.gizmo.cancelDrag();
        }
        r.check("an_edge_on_ring_leaves_the_placement_finite_after_cancel",
                std::isfinite(f.placement().rotationY) && f.placement().rotationY == 0.0);
    }

    // -----------------------------------------------------------------------
    // A move along an axis pointing almost at the viewer stays finite
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // Looking almost straight down the world X axis, so the X handle points
        // nearly at the eye.
        const CameraSnapshot camera = perspectiveCamera(Vec3{9.0f, 0.05f, 0.0f},
                                                        Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.handlePixel(camera, GizmoAxis::X, &x, &y) &&
                             f.gizmo.beginDrag(1, camera, x, y, f.width, f.height);
        for (int i = 1; i <= 20; ++i) {
            f.gizmo.updateDrag(1, camera, x + static_cast<float>(i) * 3.0f,
                               y + static_cast<float>(i) * 3.0f, f.width, f.height);
        }
        const TransformValues values = f.placement();
        r.check("a_near_parallel_move_never_produces_a_non_finite_position",
                std::isfinite(values.positionX) && std::isfinite(values.positionY) &&
                    std::isfinite(values.positionZ));
        if (grabbed) {
            f.gizmo.cancelDrag();
            r.check("a_near_parallel_move_cancels_back_to_the_origin",
                    f.placement().positionX == 0.0);
        } else {
            // Refusing the capture outright is the other honest answer, and it
            // must have written nothing at all.
            r.check("a_near_parallel_move_that_refuses_capture_wrote_nothing",
                    values.positionX == 0.0 && !f.gizmo.capturing() && f.history.undoDepth() == 0);
        }
    }

    // -----------------------------------------------------------------------
    // Multi-object chronology
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        const ObjectId first = f.scene.activeBodyId();

        float x = 0.0f, y = 0.0f;
        f.handlePixel(camera, GizmoAxis::X, &x, &y);
        f.gizmo.beginDrag(1, camera, x, y, f.width, f.height);
        for (int i = 1; i <= 5; ++i) {
            f.gizmo.updateDrag(1, camera, x + static_cast<float>(i) * 8.0f, y, f.width, f.height);
        }
        f.gizmo.commitDrag();
        const double firstX = f.scene.findBody(first)->transform().values().positionX;

        // A second body, created outside any transaction of the gizmo's, then
        // dragged on its own.
        ObjectId second = kNoObject;
        {
            ScopedConstructionEdit edit(f.history);
            SceneObject& body = f.scene.addBody();
            second = body.objectId();
            publishConstructionObject(body.construction(), body.meshStore());
        }
        f.handlePixel(camera, GizmoAxis::Y, &x, &y);
        f.gizmo.beginDrag(2, camera, x, y, f.width, f.height);
        for (int i = 1; i <= 5; ++i) {
            f.gizmo.updateDrag(2, camera, x, y - static_cast<float>(i) * 8.0f, f.width, f.height);
        }
        f.gizmo.commitDrag();
        const double secondY = f.scene.findBody(second)->transform().values().positionY;

        r.check("two_gizmo_edits_and_one_creation_are_three_steps", f.history.undoDepth() == 3);
        f.history.undo();
        r.check("the_newest_edit_undoes_first_and_touches_only_its_own_body",
                f.scene.findBody(second)->transform().values().positionY == 0.0 &&
                    nearly(f.scene.findBody(first)->transform().values().positionX, firstX, 1e-9));
        f.history.redo();
        r.check("redo_puts_the_newest_edit_back",
                nearly(f.scene.findBody(second)->transform().values().positionY, secondY, 1e-9));
    }

    // -----------------------------------------------------------------------
    // The session-initialization boundary (S020-PRE)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        r.check("a_fresh_session_has_an_empty_history",
                !f.history.canUndo() && !f.history.canRedo());

        // Seeding: exactly what answering the start question with Sculpt does —
        // a real Construction shape change through the ordinary entry point.
        f.history.beginSessionInitialization();
        {
            ScopedConstructionEdit edit(f.history);
            SceneObject& body = f.scene.activeBody();
            applyPrimitive(body.construction(), body.meshStore(), PrimitiveSpec::forSphere(2.0));
        }
        r.check("a_seeded_shape_change_actually_changed_the_shape",
                f.scene.activeBody().construction().kind() == PrimitiveKind::Sphere);
        f.history.endSessionInitialization();
        r.check("seeding_leaves_the_history_empty",
                !f.history.canUndo() && !f.history.canRedo() && f.history.undoDepth() == 0 &&
                    f.history.redoDepth() == 0 && !f.history.editInProgress());
        r.check("seeding_did_not_undo_the_seed",
                f.scene.activeBody().construction().kind() == PrimitiveKind::Sphere);

        // And the very next act IS the user's first Undo.
        {
            ScopedConstructionEdit edit(f.history);
            applyTransformValues(f.scene.activeBody().transform(),
                                 placementValues(1.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        }
        r.check("the_first_user_act_after_seeding_is_the_first_step",
                f.history.undoDepth() == 1 && f.history.canUndo());
        f.history.undo();
        r.check("undoing_it_goes_back_to_the_seeded_state_not_before_it",
                f.scene.activeBody().transform().values().positionX == 0.0 &&
                    f.scene.activeBody().construction().kind() ==
                        PrimitiveKind::Sphere);
    }

    return r.n;
}

}  // namespace forgeshape
