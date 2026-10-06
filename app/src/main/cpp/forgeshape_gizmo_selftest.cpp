#include "forgeshape_gizmo_selftest.h"

#include <cmath>
#include <cstring>
#include <limits>

#include "forgeshape_construction.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_mesh.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch_overlay.h"
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

// Orientation equality, which is the ONLY correct way to state a rotation
// result: a ZYX Euler triple is not unique, so "the Y field grew by 30" is a
// statement about a representation and "the body faces this way" is a statement
// about the body. Every mixed-rotation case below compares matrices.
bool sameOrientation(const Mat4& a, const Mat4& b, float tolerance) {
    if (!mat4Finite(a) || !mat4Finite(b)) {
        return false;
    }
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            if (std::fabs(a.m[column * 4 + row] - b.m[column * 4 + row]) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

// A camera built here rather than driven through CameraController, so a case is
// a statement about the SOLVER and not about how a gesture reached a pose. The
// snapshot carries exactly the fields buildPickRay and the gizmo consume.
CameraSnapshot perspectiveCamera(const Vec3& eye, const Vec3& target, int width, int height) {
    CameraSnapshot camera{};
    // The world up, unless the view runs almost straight along it — a look-at
    // whose up is parallel to its direction has no basis at all, and a case that
    // hit one would be testing the camera rather than the gizmo.
    Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 direction = vec3Normalize(vec3Sub(target, eye));
    if (std::fabs(vec3Dot(direction, up)) > 0.95f) {
        up = Vec3{0.0f, 0.0f, 1.0f};
    }
    camera.view = mat4LookAt(eye, target, up);
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

    Mat4 orientation() const { return rotationMatrixFromEuler(eulerOf(placement())); }

    // The pixel a handle can be grabbed at, derived from the same projection the
    // hit test uses. A case that hard-coded a coordinate would be true for one
    // camera and one window only.
    bool handlePixel(const CameraSnapshot& camera, GizmoHandle handle, float* x, float* y) const {
        const GizmoSnapshot state = gizmo.snapshot(camera, width, height);
        Vec3 world{};
        if (!gizmoHandleGrabPoint(state, handle, &world)) {
            return false;
        }
        return projectWorldToScreen(camera, world, width, height, x, y);
    }

    // Grabs a handle at its own pixel.
    //
    // The hit test has to AGREE that this pixel names this handle before the
    // drag starts. Without that check a handle pointing nearly at the camera
    // projects almost onto the pivot, another handle wins the pixel, and the
    // case then measures the wrong drag while looking like it passed. Returning
    // false instead makes an unreachable handle a visible failure.
    bool grab(int32_t pointerId, const CameraSnapshot& camera, GizmoHandle handle, float* x,
              float* y) {
        return handlePixel(camera, handle, x, y) &&
               gizmo.hitTest(camera, *x, *y, width, height) == handle &&
               gizmo.beginDrag(pointerId, camera, *x, *y, width, height);
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

// A deliberately MIXED orientation: no component is zero and none is a multiple
// of 90, so nothing about a result can be true by an accidental alignment. This
// is the orientation on which "add delta to one Euler field" is wrong, which is
// exactly why every world/local case starts here.
TransformValues mixedPlacement() {
    return placementValues(0.0, 0.0, 0.0, 37.0, -52.0, 24.0);
}

// A viewpoint from which one basis axis is fully ACROSS the image plane.
//
// Needed because a mixed orientation puts at least one local axis close to any
// fixed eye, and a handle pointing at the camera projects to almost nothing:
// it cannot be grabbed by a case any more than by a user, and a case that tried
// would silently measure whichever handle won the pixel instead. Looking along
// the BISECTOR of the other two axes is the viewpoint that maximises this one:
// it is exactly perpendicular to `axisIndex` and leaves the other two equally
// oblique, so no handle is degenerate and none is hidden behind another.
CameraSnapshot cameraAcrossBasisAxis(const GizmoBasis& basis, int axisIndex, int width,
                                     int height) {
    const Vec3 other0 = basis.axis[(axisIndex + 1) % 3];
    const Vec3 other1 = basis.axis[(axisIndex + 2) % 3];
    const Vec3 viewDirection = vec3Normalize(vec3Add(other0, other1));
    const Vec3 eye = vec3Scale(viewDirection, -9.0f);
    return perspectiveCamera(eye, Vec3{0.0f, 0.0f, 0.0f}, width, height);
}

// A viewpoint looking straight DOWN one basis axis, so the plane perpendicular
// to it is face-on and its square handle is at its largest. Nudged off the exact
// axis so the two in-plane handles are not perfectly symmetric about the pixel.
CameraSnapshot cameraAlongBasisAxis(const GizmoBasis& basis, int axisIndex, int width,
                                    int height) {
    Vec3 eye = vec3Scale(basis.axis[axisIndex], 9.0f);
    eye = vec3Add(eye, vec3Scale(basis.axis[(axisIndex + 1) % 3], 1.5f));
    return perspectiveCamera(eye, Vec3{0.0f, 0.0f, 0.0f}, width, height);
}

// Walks the pointer around a ring in WORLD space and feeds every projected step
// through the ordinary drag path, so a case exercises the real solver rather
// than a straight line across the screen.
//
// The grab point sits 45 degrees between the ring plane own two basis
// directions (see gizmoRingGrabOffset), and the (u, v) plane is right-handed
// about the ring normal, so sweeping the angle POSITIVE turns the body
// positively about that normal.
void driveRingSweep(Fixture& f, const CameraSnapshot& camera, int32_t pointerId, int axisIndex,
                    float sweepRadians, int steps) {
    const GizmoSnapshot state = f.gizmo.snapshot(camera, f.width, f.height);
    if (!state.visible) {
        return;
    }
    int ui = 0;
    int vi = 0;
    gizmoPerpendicularIndices(axisIndex, &ui, &vi);
    const Mat4& basis = state.orientation;
    const Vec3 u{basis.m[ui * 4 + 0], basis.m[ui * 4 + 1], basis.m[ui * 4 + 2]};
    const Vec3 v{basis.m[vi * 4 + 0], basis.m[vi * 4 + 1], basis.m[vi * 4 + 2]};
    const float radius = kGizmoRingRadiusUnits * state.worldPerReferenceUnit;
    const float start = kPi / 4.0f;  // where gizmoHandleGrabPoint put the finger
    for (int i = 1; i <= steps; ++i) {
        const float angle = start + sweepRadians * static_cast<float>(i) /
                                        static_cast<float>(steps);
        const Vec3 point = vec3Add(state.pivot,
                                   vec3Add(vec3Scale(u, radius * std::cos(angle)),
                                           vec3Scale(v, radius * std::sin(angle))));
        float sx = 0.0f, sy = 0.0f;
        if (projectWorldToScreen(camera, point, f.width, f.height, &sx, &sy)) {
            f.gizmo.updateDrag(pointerId, camera, sx, sy, f.width, f.height);
        }
    }
}

const GizmoHandle kAxisHandles[3] = {GizmoHandle::AxisX, GizmoHandle::AxisY, GizmoHandle::AxisZ};
const GizmoHandle kPlaneHandles[3] = {GizmoHandle::PlaneXY, GizmoHandle::PlaneXZ,
                                      GizmoHandle::PlaneYZ};

}  // namespace

int runGizmoSelfTests(GizmoSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};
    if (out == nullptr || maxOut <= 0) {
        return 0;
    }

    // -----------------------------------------------------------------------
    // The closed enums  (S020R2-01, S020R2-16)
    // -----------------------------------------------------------------------
    {
        GizmoMode mode = GizmoMode::Rotate;
        bool modesRoundTrip = kGizmoModeCount == 3;
        for (int i = 0; i < kGizmoModeCount; ++i) {
            modesRoundTrip = modesRoundTrip && gizmoModeFromIndex(i, &mode) &&
                             gizmoModeIndex(mode) == i;
        }
        r.check("mode_index_round_trips", modesRoundTrip);
        r.check("unknown_mode_index_is_refused_not_clamped",
                !gizmoModeFromIndex(-1, &mode) && !gizmoModeFromIndex(3, &mode));

        GizmoSpace space = GizmoSpace::Local;
        bool spacesRoundTrip = kGizmoSpaceCount == 2;
        for (int i = 0; i < kGizmoSpaceCount; ++i) {
            spacesRoundTrip = spacesRoundTrip && gizmoSpaceFromIndex(i, &space) &&
                              gizmoSpaceIndex(space) == i;
        }
        r.check("space_index_round_trips", spacesRoundTrip);
        r.check("unknown_space_index_is_refused_not_clamped",
                !gizmoSpaceFromIndex(-1, &space) && !gizmoSpaceFromIndex(2, &space));

        bool handlesRoundTrip = true;
        for (int code = 0; code < kGizmoHandleCodeCount; ++code) {
            GizmoHandle handle = GizmoHandle::None;
            handlesRoundTrip = handlesRoundTrip && gizmoHandleFromCode(code, &handle) &&
                               gizmoHandleCode(handle) == code;
        }
        r.check("handle_code_round_trips", handlesRoundTrip);
        {
            GizmoHandle handle = GizmoHandle::None;
            r.check("unknown_handle_code_is_refused",
                    !gizmoHandleFromCode(-1, &handle) &&
                        !gizmoHandleFromCode(kGizmoHandleCodeCount, &handle));
        }
        // The three axis codes are unchanged from the axis-only gizmo, so a
        // reader of an older log and a reader of a new one agree what a 2 means.
        r.check("axis_handle_codes_are_unchanged",
                gizmoHandleCode(GizmoHandle::AxisX) == 1 &&
                    gizmoHandleCode(GizmoHandle::AxisY) == 2 &&
                    gizmoHandleCode(GizmoHandle::AxisZ) == 3);

        r.check("axis_directions_are_the_world_axes",
                gizmoAxisDirection(GizmoAxis::X).x == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::Y).y == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::Z).z == 1.0f &&
                    gizmoAxisDirection(GizmoAxis::None).x == 0.0f &&
                    gizmoAxisDirection(GizmoAxis::None).y == 0.0f &&
                    gizmoAxisDirection(GizmoAxis::None).z == 0.0f);

        // A plane handle names two basis directions and borrows the hue of the
        // third; a uniform handle names none and is drawn neutral.
        int a = 0, b = 0;
        r.check("plane_handles_name_their_two_axes",
                gizmoPlaneAxisIndices(GizmoHandle::PlaneXY, &a, &b) && a == 0 && b == 1 &&
                    gizmoPlaneAxisIndices(GizmoHandle::PlaneXZ, &a, &b) && a == 0 && b == 2 &&
                    gizmoPlaneAxisIndices(GizmoHandle::PlaneYZ, &a, &b) && a == 1 && b == 2 &&
                    !gizmoPlaneAxisIndices(GizmoHandle::AxisX, &a, &b));
        r.check("plane_handles_borrow_the_perpendicular_hue",
                gizmoHandleColorAxis(GizmoHandle::PlaneXY) == GizmoAxis::Z &&
                    gizmoHandleColorAxis(GizmoHandle::PlaneXZ) == GizmoAxis::Y &&
                    gizmoHandleColorAxis(GizmoHandle::PlaneYZ) == GizmoAxis::X &&
                    gizmoHandleColorAxis(GizmoHandle::Uniform) == GizmoAxis::None);

        GizmoHandle handles[kGizmoMaxHandles];
        r.check("move_offers_axes_and_planes_and_no_uniform",
                gizmoHandlesForMode(GizmoMode::Move, handles, kGizmoMaxHandles) == 6);
        r.check("rotate_offers_three_rings",
                gizmoHandlesForMode(GizmoMode::Rotate, handles, kGizmoMaxHandles) == 3);
        r.check("scale_offers_axes_planes_and_uniform",
                gizmoHandlesForMode(GizmoMode::Scale, handles, kGizmoMaxHandles) == 7 &&
                    handles[0] == GizmoHandle::Uniform);
    }

    // -----------------------------------------------------------------------
    // The quantization and placement seam is the identity  (S020R2-01)
    // -----------------------------------------------------------------------
    {
        r.check("translation_quantizer_is_the_identity",
                quantizeGizmoTranslation(0.0) == 0.0 &&
                    quantizeGizmoTranslation(0.37) == 0.37 &&
                    quantizeGizmoTranslation(-12.5) == -12.5);
        r.check("rotation_quantizer_is_the_identity",
                quantizeGizmoRotation(0.0) == 0.0 && quantizeGizmoRotation(7.5) == 7.5 &&
                    quantizeGizmoRotation(-370.25) == -370.25);
        r.check("scale_quantizer_is_the_identity",
                quantizeGizmoScale(1.0) == 1.0 && quantizeGizmoScale(0.125) == 0.125 &&
                    quantizeGizmoScale(17.5) == 17.5);

        // The whole-placement seam. It exists so an approved Surface Snap lands
        // in one function; today it must be provably transparent.
        TransformValues target = mixedPlacement();
        target.positionX = 3.5;
        target.scaleY = 2.25;
        const TransformValues start = placementValues(0, 0, 0, 0, 0, 0);
        const TransformValues passed = applyGizmoPlacementModifier(
            target, start, GizmoMode::Move, GizmoHandle::PlaneXZ, GizmoSpace::Local);
        r.check("placement_modifier_is_the_identity",
                passed.positionX == target.positionX && passed.positionY == target.positionY &&
                    passed.positionZ == target.positionZ &&
                    passed.rotationX == target.rotationX &&
                    passed.rotationY == target.rotationY &&
                    passed.rotationZ == target.rotationZ && passed.scaleX == target.scaleX &&
                    passed.scaleY == target.scaleY && passed.scaleZ == target.scaleZ);
    }

    // -----------------------------------------------------------------------
    // The constrained basis  (S020R2-04, S020R2-15)
    // -----------------------------------------------------------------------
    {
        const TransformValues world = mixedPlacement();
        const GizmoBasis worldBasis = gizmoBasisFor(GizmoSpace::World, world);
        r.check("the_world_basis_is_the_world_axes",
                worldBasis.axis[0].x == 1.0f && worldBasis.axis[1].y == 1.0f &&
                    worldBasis.axis[2].z == 1.0f);

        const GizmoBasis localBasis = gizmoBasisFor(GizmoSpace::Local, world);
        const Mat4 rotation = rotationMatrixFromEuler(eulerOf(world));
        bool columnsMatch = true;
        for (int i = 0; i < 3; ++i) {
            const Vec3 column{rotation.m[i * 4 + 0], rotation.m[i * 4 + 1], rotation.m[i * 4 + 2]};
            columnsMatch = columnsMatch && nearly(localBasis.axis[i].x, column.x, 1e-5) &&
                           nearly(localBasis.axis[i].y, column.y, 1e-5) &&
                           nearly(localBasis.axis[i].z, column.z, 1e-5);
        }
        r.check("the_local_basis_is_the_body_rotation_columns", columnsMatch);
        r.check("a_mixed_local_basis_is_not_the_world_basis",
                std::fabs(localBasis.axis[0].x - 1.0f) > 0.05f);

        // And the basis is SCALE-FREE. A stretched body still has a local X that
        // points one way, and a handle direction that depended on how large the
        // body was would make a drag mean something different after every scale.
        TransformValues stretched = world;
        stretched.scaleX = 4.0;
        stretched.scaleY = 0.25;
        stretched.scaleZ = 2.0;
        const GizmoBasis stretchedBasis = gizmoBasisFor(GizmoSpace::Local, stretched);
        bool unaffected = true;
        for (int i = 0; i < 3; ++i) {
            unaffected = unaffected && nearly(stretchedBasis.axis[i].x, localBasis.axis[i].x, 1e-6) &&
                         nearly(stretchedBasis.axis[i].y, localBasis.axis[i].y, 1e-6) &&
                         nearly(stretchedBasis.axis[i].z, localBasis.axis[i].z, 1e-6);
        }
        r.check("body_scale_does_not_tilt_the_local_basis", unaffected);
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

        // Every interactive target meets the 48-unit floor. The STROKE is one
        // pixel wide; the target is not.
        r.check("every_hit_target_meets_the_48_unit_floor",
                2.0f * kGizmoHitSlopUnits >= 48.0f &&
                    2.0f * kGizmoPlaneHitRadiusUnits >= 48.0f &&
                    2.0f * kGizmoUniformHitRadiusUnits >= 48.0f);
        // And the drawn plane square never overlaps the drawn axis corridor, so
        // priority is a tie-break rather than a way of hiding an overlap.
        r.check("the_plane_square_clears_the_axis_corridor",
                kGizmoPlaneInnerUnits > kGizmoHitSlopUnits);
    }

    // -----------------------------------------------------------------------
    // UILR1-12 / UILR1-13 — what the instrument LOOKS like
    //
    // Legibility, stated as arithmetic. None of it looks at a pixel of a
    // screenshot, and none of it changes what a handle MEANS: the hit radii,
    // the handle set per mode, the grab points and the solvers are untouched
    // above and are asserted untouched by the cases around this block.
    // -----------------------------------------------------------------------
    {
        static GizmoVertex vertices[kGizmoVertexCount];
        const int written = generateGizmoVertices(vertices, kGizmoVertexCount);
        r.check("the_buffer_holds_exactly_the_vertices_it_declares",
                written == kGizmoVertexCount);

        // Each mode's range is exactly the vertices that mode declares, so a
        // change to one mode's drawing cannot silently shift another's.
        int first = 0;
        int count = 0;
        bool rangesAgree = gizmoVertexRange(GizmoMode::Move, &first, &count) &&
                           first == kGizmoMoveFirstVertex && count == kGizmoMoveVertexCount;
        rangesAgree = rangesAgree && gizmoVertexRange(GizmoMode::Rotate, &first, &count) &&
                      first == kGizmoRotateFirstVertex && count == kGizmoRotateVertexCount;
        rangesAgree = rangesAgree && gizmoVertexRange(GizmoMode::Scale, &first, &count) &&
                      first == kGizmoScaleFirstVertex && count == kGizmoScaleVertexCount;
        r.check("every_mode_draws_exactly_its_own_range", rangesAgree);

        // The pivot mark: neutral, part of no handle, and inside the disc that
        // grabs nothing — so the one mark that is not a control cannot be
        // mistaken for one, in either mode that draws it.
        auto neutralPivotVertices = [&](int first, int count) {
            int found = 0;
            for (int i = first; i < first + count; ++i) {
                if (vertices[i].handle != 0.0f) {
                    continue;
                }
                ++found;
                if (vertices[i].axis != 0.0f) {
                    return -1;  // a mark with no handle must have no hue either
                }
                const float x = vertices[i].position[0];
                const float y = vertices[i].position[1];
                const float z = vertices[i].position[2];
                if (std::sqrt(x * x + y * y + z * z) > kGizmoPivotDeadRadiusUnits) {
                    return -1;
                }
            }
            return found;
        };
        r.check("move_draws_a_neutral_pivot_mark_inside_the_dead_disc",
                neutralPivotVertices(kGizmoMoveFirstVertex, kGizmoMoveVertexCount) ==
                    2 * kGizmoPivotMarkLineCount);
        r.check("rotate_draws_the_same_pivot_mark",
                neutralPivotVertices(kGizmoRotateFirstVertex, kGizmoRotateVertexCount) ==
                    2 * kGizmoPivotMarkLineCount);
        // And Scale draws NONE, because the uniform cube stands on that point
        // and is a handle there: a reference mark and a control sharing one
        // point is what made the control unreadable.
        int scaleUnhandled = 0;
        for (int i = kGizmoScaleFirstVertex; i < kGizmoScaleFirstVertex + kGizmoScaleVertexCount;
             ++i) {
            if (vertices[i].handle == 0.0f) {
                ++scaleUnhandled;
            }
        }
        r.check("scale_puts_a_handle_on_the_pivot_rather_than_a_mark", scaleUnhandled == 0);

        // The uniform cube is the largest mark on the instrument and is drawn
        // twice, a stroke apart. It acts on all three axes and stands where
        // every shaft converges; at the size of a shaft cube it was the faintest
        // thing in the drawing while carrying the widest consequence.
        int uniformVertices = 0;
        float uniformExtent = 0.0f;
        for (int i = kGizmoScaleFirstVertex; i < kGizmoScaleFirstVertex + kGizmoScaleVertexCount;
             ++i) {
            if (vertices[i].handle != static_cast<float>(gizmoHandleCode(GizmoHandle::Uniform))) {
                continue;
            }
            ++uniformVertices;
            for (int axis = 0; axis < 3; ++axis) {
                const float extent = std::fabs(vertices[i].position[axis]);
                uniformExtent = extent > uniformExtent ? extent : uniformExtent;
            }
        }
        r.check("the_uniform_cube_is_drawn_as_a_double_outline",
                uniformVertices == 2 * 2 * kGizmoCubeLineCount);
        r.check("the_uniform_cube_is_larger_than_a_shaft_cube",
                kGizmoUniformCubeHalfUnits > kGizmoScaleCubeHalfUnits &&
                    nearly(uniformExtent,
                           kGizmoUniformCubeHalfUnits + kGizmoStrokeOffsetUnits, 1e-3));

        // A plane handle is a different KIND of mark from an axis handle: the
        // square is crossed. Told by counting the lines that belong to one
        // plane handle rather than by looking at them.
        int planeVertices = 0;
        for (int i = kGizmoMoveFirstVertex; i < kGizmoMoveFirstVertex + kGizmoMoveVertexCount;
             ++i) {
            if (vertices[i].handle == static_cast<float>(gizmoHandleCode(GizmoHandle::PlaneXY))) {
                ++planeVertices;
            }
        }
        r.check("a_plane_handle_is_a_crossed_square_not_a_bare_outline",
                planeVertices == 2 * kGizmoPlaneSquareLineCount &&
                    kGizmoPlaneSquareLineCount > 2 * 4);

        // The held handle is stated in a colour NO axis owns, and the others
        // drop away rather than the held one merely brightening. Both halves,
        // because a reader who cannot separate the hues still has the weight.
        float highlight[3] = {0.0f, 0.0f, 0.0f};
        gizmoHighlightColor(ViewportBackground::WarmGraphite, highlight);
        bool highlightIsItsOwn = true;
        for (GizmoAxis axis : {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z}) {
            float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            gizmoAxisColor(ViewportBackground::WarmGraphite, axis, rgba);
            float distance = 0.0f;
            for (int i = 0; i < 3; ++i) {
                distance += (rgba[i] - highlight[i]) * (rgba[i] - highlight[i]);
            }
            highlightIsItsOwn = highlightIsItsOwn && std::sqrt(distance) > 0.25f;
        }
        r.check("a_held_handle_is_drawn_in_a_colour_no_axis_owns", highlightIsItsOwn);
        r.check("and_the_handles_that_are_not_held_drop_away",
                kGizmoIdleAxisAlphaScale < kGizmoHeldAxisAlphaScale &&
                    kGizmoIdleAxisAlphaScale > 0.0f);
    }

    // -----------------------------------------------------------------------
    // The sketch-overlay style mapping, EXHAUSTIVELY (`UI-3D-STATE-C2`)
    // -----------------------------------------------------------------------
    //
    // The overlay borrows this instrument's pipeline, palette and vertex
    // layout, so its per-style weights are checked here rather than in a suite
    // of their own. Exhaustive by construction: a style with no mapping draws at
    // alpha 0, which is invisible with nothing failing -- `UI3D-F-005`, where
    // `Dimension` went unmapped through two shipped features.
    {
        const ViewportBackground grounds[kViewportBackgroundCount] = {
            ViewportBackground::WarmGraphite, ViewportBackground::NeutralCharcoal,
            ViewportBackground::LightCharcoal, ViewportBackground::WarmLight,
            ViewportBackground::CoolLight};

        // A deliberate tripwire: it names the LAST value of the enum, so adding
        // one lands a reader in this block, where the exhaustive walk below is
        // what actually proves the new style is drawn. Verified by adding a
        // sixth value: this check and the walk both fail, and the mapping's own
        // switch warns at compile time.
        r.check("overlay_the_style_count_states_the_whole_enum",
                static_cast<int>(SketchOverlayStyle::DimensionReference) + 1
                        == kSketchOverlayStyleCount);

        // EVERY value the enum can take, on EVERY ground: mapped, finite, and
        // drawn at an alpha something can actually be seen at.
        bool allMapped = true;
        bool allVisible = true;
        bool allFinite = true;
        for (int s = 0; s < kSketchOverlayStyleCount; ++s) {
            const SketchOverlayStyle style = static_cast<SketchOverlayStyle>(s);
            for (int g = 0; g < kViewportBackgroundCount; ++g) {
                SketchOverlayStyleWeights w;
                if (!sketchOverlayStyleWeights(style, grounds[g], &w)) {
                    allMapped = false;
                    continue;
                }
                allVisible = allVisible && w.alpha > 0.05f && w.alpha <= 1.0f;
                allFinite = allFinite && std::isfinite(w.alpha) &&
                            std::isfinite(w.neutralLevel) && w.neutralLevel >= 0.0f &&
                            w.neutralLevel <= 1.0f;
            }
        }
        r.check("overlay_every_style_has_a_mapping_on_every_ground", allMapped);
        r.check("overlay_no_style_is_drawn_at_an_invisible_alpha", allVisible);
        r.check("overlay_every_style_weight_is_finite_and_in_range", allFinite);

        // The finding itself, named: the annotation both the selected-Line
        // dimension and the Stage 020M active-axis leader are drawn in.
        bool dimensionVisible = true;
        for (int g = 0; g < kViewportBackgroundCount; ++g) {
            SketchOverlayStyleWeights w;
            dimensionVisible =
                dimensionVisible &&
                sketchOverlayStyleWeights(SketchOverlayStyle::Dimension, grounds[g], &w) &&
                w.alpha > 0.5f;
        }
        r.check("overlay_the_dimension_annotation_is_not_transparent", dimensionVisible);

        // The four styles that already had a mapping keep it to the value, on
        // every ground: this correction is one case added, not a re-weighting.
        bool legacyUnchanged = true;
        for (int g = 0; g < kViewportBackgroundCount; ++g) {
            const float neutral = gizmoNeutralLevel(grounds[g]);
            float highlightRgb[3] = {0.0f, 0.0f, 0.0f};
            gizmoHighlightColor(grounds[g], highlightRgb);
            const float entityLevel = neutral * 0.35f + highlightRgb[0] * 0.65f;
            const SketchOverlayStyle styles[4] = {
                SketchOverlayStyle::GridMinor, SketchOverlayStyle::GridMajor,
                SketchOverlayStyle::Axes, SketchOverlayStyle::Entities};
            const float alphas[4] = {kGizmoAxisAlpha * 0.22f, kGizmoAxisAlpha * 0.45f,
                                     kGizmoAxisAlpha * 0.9f, kGizmoAxisAlpha};
            const float levels[4] = {neutral, neutral, neutral, entityLevel};
            for (int i = 0; i < 4; ++i) {
                SketchOverlayStyleWeights w;
                legacyUnchanged = legacyUnchanged &&
                                  sketchOverlayStyleWeights(styles[i], grounds[g], &w) &&
                                  w.alpha == alphas[i] && w.neutralLevel == levels[i];
            }
        }
        r.check("overlay_the_four_older_styles_keep_their_exact_weights", legacyUnchanged);

        // An annotation is read AGAINST the geometry it measures, so it stands
        // clear of the grid and under the entities. Stated as an ordering
        // rather than as a number, because the number is a look and the
        // ordering is the rule.
        bool ordered = true;
        for (int g = 0; g < kViewportBackgroundCount; ++g) {
            SketchOverlayStyleWeights minor, major, dimension, entities;
            ordered = ordered &&
                      sketchOverlayStyleWeights(SketchOverlayStyle::GridMinor, grounds[g], &minor) &&
                      sketchOverlayStyleWeights(SketchOverlayStyle::GridMajor, grounds[g], &major) &&
                      sketchOverlayStyleWeights(SketchOverlayStyle::Dimension, grounds[g],
                                                &dimension) &&
                      sketchOverlayStyleWeights(SketchOverlayStyle::Entities, grounds[g],
                                                &entities) &&
                      minor.alpha < major.alpha && major.alpha < dimension.alpha &&
                      dimension.alpha < entities.alpha &&
                      dimension.neutralLevel == entities.neutralLevel;
        }
        r.check("overlay_an_annotation_stands_off_the_grid_and_under_the_entities", ordered);

        // A code that is not a style at all is refused, writing nothing -- the
        // renderer skips such a range rather than recording an unseeable draw.
        SketchOverlayStyleWeights untouched;
        untouched.neutralLevel = 0.125f;
        untouched.alpha = 0.375f;
        const bool refusedOutOfEnum =
            !sketchOverlayStyleWeights(static_cast<SketchOverlayStyle>(kSketchOverlayStyleCount),
                                       ViewportBackground::WarmGraphite, &untouched) &&
            !sketchOverlayStyleWeights(static_cast<SketchOverlayStyle>(200),
                                       ViewportBackground::WarmGraphite, &untouched) &&
            untouched.neutralLevel == 0.125f && untouched.alpha == 0.375f;
        r.check("overlay_a_code_outside_the_enum_is_refused_and_writes_nothing", refusedOutOfEnum);
        r.check("overlay_a_null_destination_is_refused",
                !sketchOverlayStyleWeights(SketchOverlayStyle::Dimension,
                                           ViewportBackground::WarmGraphite, nullptr));
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
        // it is FINITE and that the fallback path is the one that ran. About two
        // degrees off the axis: past the closest-approach threshold (which needs
        // roughly eight) and comfortably clear of the plane fallback own
        // edge-on limit.
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
    // Ray / plane intersection
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
    // Visibility, mode and space  (S020R2-18, S020R2-19)
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
        r.check("entering_the_tool_selects_move_in_world",
                f.gizmo.mode() == GizmoMode::Move && f.gizmo.space() == GizmoSpace::World &&
                    f.gizmo.spaceIsSelectable());
        r.check("a_degenerate_viewport_draws_no_gizmo", !f.gizmo.snapshot(camera, 0, 0).visible);

        // Space is the user choice in Move and in Rotate.
        r.check("move_and_rotate_take_either_space",
                f.gizmo.setSpace(GizmoSpace::Local) && f.gizmo.space() == GizmoSpace::Local &&
                    f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.spaceIsSelectable() &&
                    f.gizmo.setSpace(GizmoSpace::World) && f.gizmo.space() == GizmoSpace::World);

        // Entering Scale forces Local and withdraws the choice.
        f.gizmo.setSpace(GizmoSpace::Local);
        r.check("scale_is_local_only",
                f.gizmo.setMode(GizmoMode::Scale) && f.gizmo.space() == GizmoSpace::Local &&
                    !f.gizmo.spaceIsSelectable() && !f.gizmo.setSpace(GizmoSpace::World) &&
                    f.gizmo.space() == GizmoSpace::Local);
        // And leaving it puts the remembered space back, so a round trip through
        // Scale does not quietly change what a Move handle means.
        r.check("leaving_scale_restores_the_remembered_space",
                f.gizmo.setMode(GizmoMode::Move) && f.gizmo.space() == GizmoSpace::Local);
        f.gizmo.setSpace(GizmoSpace::World);
        f.gizmo.setMode(GizmoMode::Scale);
        r.check("leaving_scale_restores_world_too",
                f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.space() == GizmoSpace::World);

        // None of it is an edit.
        r.check("mode_and_space_changes_record_nothing",
                f.history.undoDepth() == 0 && f.history.redoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Hit testing picks the handle under the finger  (S020R2-16)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);

        const GizmoMode modes[3] = {GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Scale};
        const char* names[3] = {"each_move_handle_is_hit_at_its_own_pixel",
                                "each_rotate_ring_is_hit_at_its_own_pixel",
                                "each_scale_handle_is_hit_at_its_own_pixel"};
        for (int m = 0; m < 3; ++m) {
            f.gizmo.setMode(modes[m]);
            GizmoHandle handles[kGizmoMaxHandles];
            const int count = gizmoHandlesForMode(modes[m], handles, kGizmoMaxHandles);
            bool allFound = count > 0;
            for (int i = 0; i < count; ++i) {
                float x = 0.0f, y = 0.0f;
                if (!f.handlePixel(camera, handles[i], &x, &y) ||
                    f.gizmo.hitTest(camera, x, y, f.width, f.height) != handles[i]) {
                    allFound = false;
                }
            }
            r.check(names[m], allFound);
        }

        // Far from every handle is a miss, and a miss is what leaves orbit and
        // pan alone.
        f.gizmo.setMode(GizmoMode::Move);
        r.check("a_pixel_far_from_every_handle_is_a_miss",
                f.gizmo.hitTest(camera, 5.0f, 5.0f, f.width, f.height) == GizmoHandle::None);

        // The pivot itself is not a Move handle: the three shafts converge there
        // and a touch would be a coin toss. In Scale it IS a handle — the
        // uniform cube — and the tiers must resolve to it rather than to a shaft.
        float px = 0.0f, py = 0.0f;
        const GizmoSnapshot moveState = f.gizmo.snapshot(camera, f.width, f.height);
        projectWorldToScreen(camera, moveState.pivot, f.width, f.height, &px, &py);
        r.check("the_pivot_is_not_a_move_handle",
                f.gizmo.hitTest(camera, px, py, f.width, f.height) == GizmoHandle::None);
        f.gizmo.setMode(GizmoMode::Scale);
        r.check("the_pivot_is_the_uniform_scale_handle",
                f.gizmo.hitTest(camera, px, py, f.width, f.height) == GizmoHandle::Uniform);

        f.gizmo.setActive(false);
        float x = 0.0f, y = 0.0f;
        r.check("an_inactive_gizmo_hits_nothing",
                !f.handlePixel(camera, GizmoHandle::AxisX, &x, &y) &&
                    f.gizmo.hitTest(camera, 540.0f, 1000.0f, f.width, f.height) ==
                        GizmoHandle::None);
    }

    // -----------------------------------------------------------------------
    // Move: world axes and world planes  (S020R2-03)
    // -----------------------------------------------------------------------
    {
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1080, 2000);
        bool axesMoved = true;
        bool othersHeld = true;
        for (int i = 0; i < 3; ++i) {
            Fixture f;
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kAxisHandles[i], &x, &y)) {
                axesMoved = false;
                continue;
            }
            for (int s = 1; s <= 12; ++s) {
                f.gizmo.updateDrag(1, camera, x + static_cast<float>(s) * 5.0f,
                                   y - static_cast<float>(s) * 3.0f, f.width, f.height);
            }
            const TransformValues moved = f.placement();
            axesMoved = axesMoved && std::fabs(transformPositionAt(moved, i)) > 0.05;
            // Constrained: the OTHER two world components are untouched, exactly.
            for (int other = 0; other < 3; ++other) {
                if (other != i) {
                    othersHeld = othersHeld && transformPositionAt(moved, other) == 0.0;
                }
            }
            othersHeld = othersHeld && moved.rotationX == 0.0 && moved.rotationY == 0.0 &&
                         moved.rotationZ == 0.0 && moved.scaleX == 1.0 && moved.scaleY == 1.0 &&
                         moved.scaleZ == 1.0;
            f.gizmo.cancelDrag();
        }
        r.check("every_world_move_axis_moves_its_own_component", axesMoved);
        r.check("a_world_axis_move_touches_nothing_else", othersHeld);

        bool planesMoved = true;
        bool normalHeld = true;
        for (int p = 0; p < 3; ++p) {
            Fixture f;
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kPlaneHandles[p], &x, &y)) {
                planesMoved = false;
                continue;
            }
            for (int s = 1; s <= 12; ++s) {
                f.gizmo.updateDrag(1, camera, x + static_cast<float>(s) * 6.0f,
                                   y + static_cast<float>(s) * 4.0f, f.width, f.height);
            }
            const TransformValues moved = f.placement();
            int a = 0, b = 0;
            gizmoPlaneAxisIndices(kPlaneHandles[p], &a, &b);
            const int normal = gizmoPlaneNormalIndex(kPlaneHandles[p]);
            planesMoved = planesMoved && (std::fabs(transformPositionAt(moved, a)) > 0.01 ||
                                          std::fabs(transformPositionAt(moved, b)) > 0.01);
            // The third component is untouched BY CONSTRUCTION — the delta is
            // projected onto two directions, never corrected afterwards.
            normalHeld = normalHeld && transformPositionAt(moved, normal) == 0.0;
            f.gizmo.cancelDrag();
        }
        r.check("every_world_move_plane_moves_in_its_plane", planesMoved);
        r.check("a_world_plane_move_never_leaves_its_plane", normalHeld);
    }

    // -----------------------------------------------------------------------
    // Move: local axes and local planes on a MIXED body  (S020R2-04)
    // -----------------------------------------------------------------------
    {
        const GizmoBasis localBasis = gizmoBasisFor(GizmoSpace::Local, mixedPlacement());
        bool alongLocalAxis = true;
        for (int i = 0; i < 3; ++i) {
            Fixture f;
            f.setPlacement(mixedPlacement());
            f.gizmo.setSpace(GizmoSpace::Local);
            // A viewpoint chosen against THIS axis, not a generic one — see
            // cameraAcrossBasisAxis. A mixed orientation always leaves some
            // local axis pointing near any fixed eye.
            const CameraSnapshot camera =
                cameraAcrossBasisAxis(localBasis, i, f.width, f.height);
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kAxisHandles[i], &x, &y)) {
                alongLocalAxis = false;
                continue;
            }
            for (int s = 1; s <= 12; ++s) {
                f.gizmo.updateDrag(1, camera, x + static_cast<float>(s) * 5.0f,
                                   y - static_cast<float>(s) * 3.0f, f.width, f.height);
            }
            const TransformValues moved = f.placement();
            const Vec3 delta{static_cast<float>(moved.positionX),
                             static_cast<float>(moved.positionY),
                             static_cast<float>(moved.positionZ)};
            const GizmoBasis basis = gizmoBasisFor(GizmoSpace::Local, mixedPlacement());
            const float along = vec3Dot(delta, basis.axis[i]);
            const Vec3 residue = vec3Sub(delta, vec3Scale(basis.axis[i], along));
            const float length = std::sqrt(vec3Dot(delta, delta));
            // The displacement is PARALLEL to the body own axis, which for a
            // mixed orientation is a genuinely different direction from the
            // world axis of the same name — and every world component moves.
            alongLocalAxis = alongLocalAxis && length > 0.05f &&
                             std::sqrt(vec3Dot(residue, residue)) < length * 1e-3f;
            f.gizmo.cancelDrag();
        }
        r.check("a_local_move_axis_moves_along_the_body_own_axis", alongLocalAxis);

        bool inLocalPlane = true;
        for (int p = 0; p < 3; ++p) {
            Fixture f;
            f.setPlacement(mixedPlacement());
            f.gizmo.setSpace(GizmoSpace::Local);
            // Looking down the plane own normal, so the square is face-on —
            // the viewpoint from which a plane handle is meant to be used.
            const CameraSnapshot camera = cameraAlongBasisAxis(
                localBasis, gizmoPlaneNormalIndex(kPlaneHandles[p]), f.width, f.height);
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kPlaneHandles[p], &x, &y)) {
                inLocalPlane = false;
                continue;
            }
            for (int s = 1; s <= 12; ++s) {
                f.gizmo.updateDrag(1, camera, x + static_cast<float>(s) * 6.0f,
                                   y + static_cast<float>(s) * 4.0f, f.width, f.height);
            }
            const TransformValues moved = f.placement();
            const Vec3 delta{static_cast<float>(moved.positionX),
                             static_cast<float>(moved.positionY),
                             static_cast<float>(moved.positionZ)};
            const GizmoBasis basis = gizmoBasisFor(GizmoSpace::Local, mixedPlacement());
            const int normal = gizmoPlaneNormalIndex(kPlaneHandles[p]);
            const float outOfPlane = std::fabs(vec3Dot(delta, basis.axis[normal]));
            const float length = std::sqrt(vec3Dot(delta, delta));
            inLocalPlane = inLocalPlane && length > 0.02f && outOfPlane < length * 1e-3f;
            f.gizmo.cancelDrag();
        }
        r.check("a_local_move_plane_stays_in_the_body_own_plane", inLocalPlane);

        // The two spaces are genuinely different answers from the same start:
        // the same body, the same handle name, the same drag.
        //
        // Each fixture gets the viewpoint its OWN X handle is usable from, and
        // they are different viewpoints because the two X handles point
        // different ways — which is the very thing being demonstrated. The
        // assertion below is about the world-space result and not about a
        // shared pixel, so a shared camera would only be a way of making one of
        // the two ungrabbable.
        Fixture worldFixture;
        Fixture localFixture;
        worldFixture.setPlacement(mixedPlacement());
        localFixture.setPlacement(mixedPlacement());
        localFixture.gizmo.setSpace(GizmoSpace::Local);
        const GizmoBasis worldBasis = gizmoBasisFor(GizmoSpace::World, mixedPlacement());
        const CameraSnapshot worldCamera =
            cameraAcrossBasisAxis(worldBasis, 0, worldFixture.width, worldFixture.height);
        const CameraSnapshot localCamera =
            cameraAcrossBasisAxis(localBasis, 0, localFixture.width, localFixture.height);
        float wx = 0.0f, wy = 0.0f, lx = 0.0f, ly = 0.0f;
        const bool bothGrabbed =
            worldFixture.grab(1, worldCamera, GizmoHandle::AxisX, &wx, &wy) &&
            localFixture.grab(1, localCamera, GizmoHandle::AxisX, &lx, &ly);
        if (bothGrabbed) {
            for (int s = 1; s <= 10; ++s) {
                worldFixture.gizmo.updateDrag(1, worldCamera, wx + static_cast<float>(s) * 6.0f,
                                              wy, worldFixture.width, worldFixture.height);
                localFixture.gizmo.updateDrag(1, localCamera, lx + static_cast<float>(s) * 6.0f,
                                              ly, localFixture.width, localFixture.height);
            }
        }
        r.check("world_and_local_move_are_different_answers",
                bothGrabbed && worldFixture.placement().positionY == 0.0 &&
                    std::fabs(localFixture.placement().positionY) > 0.01);
    }

    // -----------------------------------------------------------------------
    // Rotate: world is pre-multiplication  (S020R2-05)
    // -----------------------------------------------------------------------
    {
        // Steeply down and to one side, so every ring is reasonably face-on and
        // a drag around one is unambiguous. Not straight down: an eye exactly on
        // the up vector degenerates the look-at basis, which would make this a
        // test of the camera rather than of the composition.
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{5.0f, 6.5f, 7.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1080, 2000);
        const float sweep = 0.6f;  // radians, comfortably inside one ring pass
        bool worldMatches = true;
        for (int i = 0; i < 3; ++i) {
            Fixture f;
            f.setPlacement(mixedPlacement());
            f.gizmo.setMode(GizmoMode::Rotate);
            f.gizmo.setSpace(GizmoSpace::World);
            const Mat4 start = f.orientation();
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kAxisHandles[i], &x, &y)) {
                worldMatches = false;
                continue;
            }
            driveRingSweep(f, camera, 1, i, sweep, 40);
            const Mat4 expected = mat4Multiply(
                elementaryRotationMatrix(i, static_cast<double>(sweep) * 180.0 / kPi), start);
            worldMatches = worldMatches && sameOrientation(f.orientation(), expected, 5e-3f);
            f.gizmo.cancelDrag();
        }
        r.check("world_rotate_pre_multiplies_the_start_orientation", worldMatches);

        bool localMatches = true;
        for (int i = 0; i < 3; ++i) {
            Fixture f;
            f.setPlacement(mixedPlacement());
            f.gizmo.setMode(GizmoMode::Rotate);
            f.gizmo.setSpace(GizmoSpace::Local);
            const Mat4 start = f.orientation();
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kAxisHandles[i], &x, &y)) {
                localMatches = false;
                continue;
            }
            driveRingSweep(f, camera, 1, i, sweep, 40);
            const Mat4 expected = mat4Multiply(
                start, elementaryRotationMatrix(i, static_cast<double>(sweep) * 180.0 / kPi));
            localMatches = localMatches && sameOrientation(f.orientation(), expected, 5e-3f);
            f.gizmo.cancelDrag();
        }
        r.check("local_rotate_post_multiplies_the_start_orientation", localMatches);

        // From the same mixed start the two produce DIFFERENT but individually
        // correct orientations. That difference is the whole point of the space
        // selector, and a build in which the two agreed would be one in which
        // one of them was wrong.
        Fixture worldFixture;
        Fixture localFixture;
        worldFixture.setPlacement(mixedPlacement());
        localFixture.setPlacement(mixedPlacement());
        worldFixture.gizmo.setMode(GizmoMode::Rotate);
        localFixture.gizmo.setMode(GizmoMode::Rotate);
        localFixture.gizmo.setSpace(GizmoSpace::Local);
        float wx = 0.0f, wy = 0.0f, lx = 0.0f, ly = 0.0f;
        const bool bothGrabbed = worldFixture.grab(1, camera, GizmoHandle::AxisX, &wx, &wy) &&
                                 localFixture.grab(1, camera, GizmoHandle::AxisX, &lx, &ly);
        if (bothGrabbed) {
            driveRingSweep(worldFixture, camera, 1, 0, sweep, 40);
            driveRingSweep(localFixture, camera, 1, 0, sweep, 40);
        }
        r.check("world_and_local_rotate_are_different_answers",
                bothGrabbed &&
                    !sameOrientation(worldFixture.orientation(), localFixture.orientation(),
                                     0.05f));
        // Neither touched anything but the orientation.
        r.check("a_rotate_drag_leaves_position_and_scale_alone",
                worldFixture.placement().positionX == 0.0 &&
                    worldFixture.placement().positionY == 0.0 &&
                    worldFixture.placement().positionZ == 0.0 &&
                    worldFixture.placement().scaleX == 1.0 &&
                    worldFixture.placement().scaleY == 1.0 &&
                    worldFixture.placement().scaleZ == 1.0);
    }

    // -----------------------------------------------------------------------
    // Rotate: past a full turn, and the fields it does not move  (S020R2-08)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{1.5f, 9.0f, 1.5f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        f.gizmo.setMode(GizmoMode::Rotate);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(1, camera, GizmoHandle::AxisY, &x, &y);
        r.check("a_ring_captures_its_pointer",
                grabbed && f.gizmo.capturedHandle() == GizmoHandle::AxisY);

        // Two full turns and a quarter, in small steps, to exercise the unwrap
        // repeatedly and to pass both 360 and 720.
        driveRingSweep(f, camera, 1, 1, 2.0f * (2.0f * kPi) + kPi / 2.0f, 200);
        const TransformValues rotated = f.placement();
        r.check("a_ring_drag_passes_two_full_turns_without_canonicalising",
                std::fabs(rotated.rotationY) > 720.0);
        // The world Y ring from an UNROTATED start is the one case where a
        // single Euler field is the whole answer, so it is also the one case
        // where "the others did not move" is a meaningful assertion.
        r.check("an_unmixed_ring_drag_leaves_the_other_rotations_alone",
                rotated.rotationX == 0.0 && rotated.rotationZ == 0.0);
        r.check("a_ring_drag_leaves_position_alone",
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
    // Rotate near the pitch singularity stays finite and correct  (S020R2-08)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // Started just short of straight up, so a Z ring drag walks the body
        // THROUGH the gimbal degeneracy rather than near it.
        f.setPlacement(placementValues(0.0, 0.0, 0.0, 0.0, 0.0, 88.0));
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{2.0f, 3.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        f.gizmo.setMode(GizmoMode::Rotate);
        f.gizmo.setSpace(GizmoSpace::Local);
        const Mat4 start = f.orientation();
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(1, camera, GizmoHandle::AxisY, &x, &y);
        const float sweep = 0.15f;
        if (grabbed) {
            driveRingSweep(f, camera, 1, 1, sweep, 60);
        }
        const TransformValues values = f.placement();
        r.check("a_drag_through_the_singularity_stays_finite",
                std::isfinite(values.rotationX) && std::isfinite(values.rotationY) &&
                    std::isfinite(values.rotationZ));
        const Mat4 expected = mat4Multiply(
            start, elementaryRotationMatrix(1, static_cast<double>(sweep) * 180.0 / kPi));
        // The ONLY property that can be asserted at a singularity: the
        // orientation is right, whatever triple was chosen to express it.
        r.check("a_drag_through_the_singularity_is_orientation_correct",
                grabbed && sameOrientation(f.orientation(), expected, 1e-2f));
        if (grabbed) {
            f.gizmo.cancelDrag();
        }
        r.check("cancelling_a_singular_drag_restores_exactly",
                f.placement().rotationZ == 88.0 && f.placement().rotationX == 0.0 &&
                    f.placement().rotationY == 0.0);
    }

    // -----------------------------------------------------------------------
    // Scale: axis, plane and uniform  (S020R2-09, -10, -11)
    // -----------------------------------------------------------------------
    {
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, 1080, 2000);

        bool axisScaled = true;
        bool axisOthersHeld = true;
        for (int i = 0; i < 3; ++i) {
            Fixture f;
            f.gizmo.setMode(GizmoMode::Scale);
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kAxisHandles[i], &x, &y)) {
                axisScaled = false;
                continue;
            }
            r.check("scale_starts_at_a_factor_of_one",
                    f.placement().scaleX == 1.0 && f.placement().scaleY == 1.0 &&
                        f.placement().scaleZ == 1.0);
            // Along the handle own screen direction, which the drag read at the
            // down and which this case does not have to know.
            float hx = 0.0f, hy = 0.0f, pxPivot = 0.0f, pyPivot = 0.0f;
            const GizmoSnapshot state = f.gizmo.snapshot(camera, f.width, f.height);
            Vec3 handleWorld{};
            gizmoHandleGrabPoint(state, kAxisHandles[i], &handleWorld);
            projectWorldToScreen(camera, handleWorld, f.width, f.height, &hx, &hy);
            projectWorldToScreen(camera, state.pivot, f.width, f.height, &pxPivot, &pyPivot);
            const float dx = hx - pxPivot;
            const float dy = hy - pyPivot;
            const float length = std::sqrt(dx * dx + dy * dy);
            for (int s = 1; s <= 10; ++s) {
                const float step = static_cast<float>(s) * 4.0f;
                f.gizmo.updateDrag(1, camera, x + dx / length * step, y + dy / length * step,
                                   f.width, f.height);
            }
            const TransformValues scaled = f.placement();
            axisScaled = axisScaled && transformScaleAt(scaled, i) > 1.05;
            for (int other = 0; other < 3; ++other) {
                if (other != i) {
                    axisOthersHeld = axisOthersHeld && transformScaleAt(scaled, other) == 1.0;
                }
            }
            axisOthersHeld = axisOthersHeld && scaled.positionX == 0.0 &&
                             scaled.positionY == 0.0 && scaled.positionZ == 0.0 &&
                             scaled.rotationX == 0.0 && scaled.rotationY == 0.0 &&
                             scaled.rotationZ == 0.0;
            f.gizmo.cancelDrag();
            r.check("cancelling_a_scale_drag_restores_exactly",
                    f.placement().scaleX == 1.0 && f.placement().scaleY == 1.0 &&
                        f.placement().scaleZ == 1.0 && f.history.undoDepth() == 0);
        }
        r.check("every_scale_axis_stretches_its_own_component", axisScaled);
        r.check("an_axis_scale_touches_nothing_else", axisOthersHeld);

        bool planeScaled = true;
        for (int p = 0; p < 3; ++p) {
            Fixture f;
            f.gizmo.setMode(GizmoMode::Scale);
            float x = 0.0f, y = 0.0f;
            if (!f.grab(1, camera, kPlaneHandles[p], &x, &y)) {
                planeScaled = false;
                continue;
            }
            const GizmoSnapshot state = f.gizmo.snapshot(camera, f.width, f.height);
            float hx = 0.0f, hy = 0.0f, pxPivot = 0.0f, pyPivot = 0.0f;
            Vec3 handleWorld{};
            gizmoHandleGrabPoint(state, kPlaneHandles[p], &handleWorld);
            projectWorldToScreen(camera, handleWorld, f.width, f.height, &hx, &hy);
            projectWorldToScreen(camera, state.pivot, f.width, f.height, &pxPivot, &pyPivot);
            const float dx = hx - pxPivot;
            const float dy = hy - pyPivot;
            const float length = std::sqrt(dx * dx + dy * dy);
            for (int s = 1; s <= 10; ++s) {
                const float step = static_cast<float>(s) * 4.0f;
                f.gizmo.updateDrag(1, camera, x + dx / length * step, y + dy / length * step,
                                   f.width, f.height);
            }
            const TransformValues scaled = f.placement();
            int a = 0, b = 0;
            gizmoPlaneAxisIndices(kPlaneHandles[p], &a, &b);
            const int normal = gizmoPlaneNormalIndex(kPlaneHandles[p]);
            // One factor on TWO components, and the third untouched.
            planeScaled = planeScaled && transformScaleAt(scaled, a) > 1.02 &&
                          nearly(transformScaleAt(scaled, a), transformScaleAt(scaled, b), 1e-12) &&
                          transformScaleAt(scaled, normal) == 1.0;
            f.gizmo.cancelDrag();
        }
        r.check("every_scale_plane_stretches_its_two_components_together", planeScaled);

        // Uniform: one factor on all three, so an already non-uniform body keeps
        // its proportions exactly.
        Fixture f;
        TransformValues start;
        start.scaleX = 3.0;
        start.scaleY = 1.5;
        start.scaleZ = 0.5;
        f.setPlacement(start);
        f.gizmo.setMode(GizmoMode::Scale);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(1, camera, GizmoHandle::Uniform, &x, &y);
        if (grabbed) {
            for (int s = 1; s <= 10; ++s) {
                const float step = static_cast<float>(s) * 5.0f;
                // Right and up on the screen, which is the direction the uniform
                // handle reads and the only one it has.
                f.gizmo.updateDrag(1, camera, x + step, y - step, f.width, f.height);
            }
        }
        const TransformValues scaled = f.placement();
        const double factor = scaled.scaleX / start.scaleX;
        r.check("uniform_scale_grows_every_component", grabbed && factor > 1.05);
        r.check("uniform_scale_preserves_the_existing_ratios",
                nearly(scaled.scaleY / start.scaleY, factor, 1e-12) &&
                    nearly(scaled.scaleZ / start.scaleZ, factor, 1e-12));

        // And pulling the pointer far past the pivot pins the body at a sliver
        // rather than passing through zero into a mirror.
        for (int s = 1; s <= 40; ++s) {
            const float step = static_cast<float>(s) * 30.0f;
            f.gizmo.updateDrag(1, camera, x - step, y + step, f.width, f.height);
        }
        const TransformValues shrunk = f.placement();
        r.check("scale_never_reaches_or_passes_zero",
                shrunk.scaleX > 0.0 && shrunk.scaleY > 0.0 && shrunk.scaleZ > 0.0 &&
                    shrunk.scaleX < start.scaleX);
        f.gizmo.commitDrag();
        r.check("a_scale_drag_of_any_length_is_exactly_one_step", f.history.undoDepth() == 1);
        f.history.undo();
        r.check("undo_restores_the_pre_scale_scale",
                f.placement().scaleX == start.scaleX && f.placement().scaleY == start.scaleY &&
                    f.placement().scaleZ == start.scaleZ);
        f.history.redo();
        r.check("redo_restores_the_scale_result",
                f.placement().scaleX == shrunk.scaleX && f.placement().scaleY == shrunk.scaleY &&
                    f.placement().scaleZ == shrunk.scaleZ);
    }

    // -----------------------------------------------------------------------
    // A scaled body does not scale its own gizmo  (S020R2-15)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        const GizmoSnapshot before = f.gizmo.snapshot(camera, f.width, f.height);
        TransformValues stretched;
        stretched.scaleX = 6.0;
        stretched.scaleY = 0.2;
        stretched.scaleZ = 3.0;
        f.setPlacement(stretched);
        const GizmoSnapshot after = f.gizmo.snapshot(camera, f.width, f.height);
        r.check("the_gizmo_size_ignores_the_body_scale",
                before.visible && after.visible &&
                    before.worldPerReferenceUnit == after.worldPerReferenceUnit);
        // The BASIS ignores it too — the orientation matrix stays orthonormal,
        // so the handles stay perpendicular however the body is stretched.
        const Vec3 firstColumn{after.orientation.m[0], after.orientation.m[1],
                               after.orientation.m[2]};
        r.check("the_gizmo_basis_stays_unit_length_under_scale",
                nearly(std::sqrt(vec3Dot(firstColumn, firstColumn)), 1.0, 1e-5));
    }

    // -----------------------------------------------------------------------
    // One drag, one transaction, one step  (S020R2-12)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(7, camera, GizmoHandle::AxisX, &x, &y);
        r.check("a_handle_captures_its_pointer",
                grabbed && f.gizmo.capturing() && f.gizmo.capturedPointerId() == 7 &&
                    f.gizmo.capturedHandle() == GizmoHandle::AxisX);
        r.check("a_drag_in_progress_records_nothing_yet", !f.history.canUndo());

        for (int i = 1; i <= 200; ++i) {
            f.gizmo.updateDrag(7, camera, x + static_cast<float>(i) * 0.5f, y, f.width, f.height);
        }
        const TransformValues moved = f.placement();
        r.check("many_samples_moved_the_body", std::fabs(moved.positionX) > 0.05);

        const bool recorded = f.gizmo.commitDrag();
        r.check("a_drag_of_any_length_is_exactly_one_step",
                recorded && f.history.undoDepth() == 1 && f.gizmo.dragUpdateCount() > 100);
        r.check("the_drag_released_its_pointer",
                !f.gizmo.capturing() && f.gizmo.capturedHandle() == GizmoHandle::None);

        f.history.undo();
        r.check("undo_restores_the_pre_drag_placement", f.placement().positionX == 0.0);
        f.history.redo();
        r.check("redo_restores_the_drag_result",
                nearly(f.placement().positionX, moved.positionX, 1e-9));
    }

    // -----------------------------------------------------------------------
    // A tap records nothing, and a cancel restores exactly  (S020R2-12)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        f.grab(1, camera, GizmoHandle::AxisY, &x, &y);
        const bool tapRecorded = f.gizmo.commitDrag();
        r.check("a_tap_on_a_handle_records_nothing",
                !tapRecorded && f.history.undoDepth() == 0 && !f.history.canUndo());

        // Something to lose: a real placement first, including a scale, so the
        // cancel can be shown to put all nine values back.
        TransformValues before = placementValues(1.0, 2.0, 3.0, 10.0, 20.0, 30.0);
        before.scaleX = 2.0;
        before.scaleY = 0.5;
        before.scaleZ = 1.75;
        f.setPlacement(before);

        f.grab(2, camera, GizmoHandle::AxisY, &x, &y);
        for (int i = 1; i <= 8; ++i) {
            f.gizmo.updateDrag(2, camera, x, y - static_cast<float>(i) * 6.0f, f.width, f.height);
        }
        r.check("a_cancelled_drag_had_actually_moved_the_body",
                f.placement().positionY != before.positionY);
        f.gizmo.cancelDrag();
        const TransformValues after = f.placement();
        r.check("cancel_restores_all_nine_values_exactly",
                after.positionX == before.positionX && after.positionY == before.positionY &&
                    after.positionZ == before.positionZ &&
                    after.rotationX == before.rotationX &&
                    after.rotationY == before.rotationY &&
                    after.rotationZ == before.rotationZ && after.scaleX == before.scaleX &&
                    after.scaleY == before.scaleY && after.scaleZ == before.scaleZ);
        r.check("cancel_records_no_step", f.history.undoDepth() == 0);
        r.check("cancel_releases_the_capture", !f.gizmo.capturing());
    }

    // -----------------------------------------------------------------------
    // A drag follows ONE pointer, and only the body it started on  (S020R2-17)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        f.grab(3, camera, GizmoHandle::AxisZ, &x, &y);
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
                f.grab(4, camera, GizmoHandle::AxisZ, &x, &y) &&
                    !f.gizmo.beginDrag(5, camera, x, y, f.width, f.height) &&
                    f.gizmo.capturedPointerId() == 4);
        f.gizmo.cancelDrag();

        r.check("mode_and_space_cannot_change_under_a_moving_finger",
                f.grab(6, camera, GizmoHandle::AxisZ, &x, &y) &&
                    !f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.mode() == GizmoMode::Move &&
                    !f.gizmo.setSpace(GizmoSpace::Local) &&
                    f.gizmo.space() == GizmoSpace::World);
        f.gizmo.cancelDrag();
        r.check("mode_changes_freely_when_nothing_is_held",
                f.gizmo.setMode(GizmoMode::Rotate) && f.gizmo.mode() == GizmoMode::Rotate &&
                    f.history.undoDepth() == 0);
    }

    // -----------------------------------------------------------------------
    // Degenerate viewpoints hold instead of jumping
    // -----------------------------------------------------------------------
    {
        Fixture f;
        // Eye in the XZ plane: the Y ring is exactly edge-on.
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{0.0f, 0.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        f.gizmo.setMode(GizmoMode::Rotate);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(1, camera, GizmoHandle::AxisY, &x, &y);
        // Whether the capture succeeds at all is the camera business; what must
        // never happen is a non-finite or wildly jumping rotation.
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
    {
        Fixture f;
        // Looking almost straight down the world X axis, so the X handle points
        // nearly at the eye.
        const CameraSnapshot camera = perspectiveCamera(Vec3{9.0f, 0.05f, 0.0f},
                                                        Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        float x = 0.0f, y = 0.0f;
        const bool grabbed = f.grab(1, camera, GizmoHandle::AxisX, &x, &y);
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

        // A SCALE handle pointing at the viewer has no usable screen direction,
        // and refusing the capture is the honest answer rather than anchoring on
        // rounding error.
        f.gizmo.setMode(GizmoMode::Scale);
        float sx = 0.0f, sy = 0.0f;
        const bool scaleGrabbed = f.grab(2, camera, GizmoHandle::AxisX, &sx, &sy);
        r.check("an_edge_on_scale_handle_refuses_or_stays_finite",
                !scaleGrabbed ? (f.history.undoDepth() == 0 && !f.gizmo.capturing())
                              : std::isfinite(f.placement().scaleX));
        if (scaleGrabbed) {
            f.gizmo.cancelDrag();
        }
    }

    // -----------------------------------------------------------------------
    // Multi-object chronology  (S020R2-13)
    // -----------------------------------------------------------------------
    {
        Fixture f;
        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        const ObjectId first = f.scene.activeBodyId();

        float x = 0.0f, y = 0.0f;
        f.grab(1, camera, GizmoHandle::AxisX, &x, &y);
        for (int i = 1; i <= 5; ++i) {
            f.gizmo.updateDrag(1, camera, x + static_cast<float>(i) * 8.0f, y, f.width, f.height);
        }
        f.gizmo.commitDrag();
        const double firstX = f.scene.findBody(first)->transform().values().positionX;

        // A second body, created outside any transaction of the gizmo, then
        // scaled on its own.
        ObjectId second = kNoObject;
        {
            ScopedConstructionEdit edit(f.history);
            SceneObject& body = f.scene.addBody();
            second = body.objectId();
            publishConstructionObject(body.construction(), body.meshStore());
        }
        f.gizmo.setMode(GizmoMode::Scale);
        f.grab(2, camera, GizmoHandle::Uniform, &x, &y);
        for (int i = 1; i <= 5; ++i) {
            f.gizmo.updateDrag(2, camera, x + static_cast<float>(i) * 8.0f,
                               y - static_cast<float>(i) * 8.0f, f.width, f.height);
        }
        f.gizmo.commitDrag();
        const double secondScale = f.scene.findBody(second)->transform().values().scaleX;

        r.check("two_gizmo_edits_and_one_creation_are_three_steps", f.history.undoDepth() == 3);
        r.check("the_second_edit_actually_scaled", secondScale != 1.0);
        f.history.undo();
        r.check("the_newest_edit_undoes_first_and_touches_only_its_own_body",
                f.scene.findBody(second)->transform().values().scaleX == 1.0 &&
                    nearly(f.scene.findBody(first)->transform().values().positionX, firstX, 1e-9));
        f.history.redo();
        r.check("redo_puts_the_newest_edit_back",
                nearly(f.scene.findBody(second)->transform().values().scaleX, secondScale, 1e-12));
    }

    // -----------------------------------------------------------------------
    // The session-initialization boundary (S020-PRE, S020R2-21)
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

        // And the very next act IS the user first Undo.
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

    // -----------------------------------------------------------------------
    // UI-PREF-R1 E: the visual size preference
    //
    // A bounded multiplier on where the handles STAND, never on what a touch
    // grabs or how far a drag moves. Each claim below is arithmetic against the
    // same snapshot the renderer draws from.
    // -----------------------------------------------------------------------
    {
        Fixture f;
        r.check("uipref_visual_scale_defaults_to_one",
                f.gizmo.visualScale() == kGizmoDefaultVisualScale &&
                    kGizmoDefaultVisualScale == 1.0f);
        r.check("uipref_visual_scale_bounds_are_the_documented_ones",
                kGizmoMinVisualScale == 0.9f && kGizmoMaxVisualScale == 1.5f);
        r.check("uipref_visual_scale_refuses_out_of_range_and_non_finite_not_clamps",
                !f.gizmo.setVisualScale(0.75f) && !f.gizmo.setVisualScale(2.0f) &&
                    !f.gizmo.setVisualScale(0.0f) && !f.gizmo.setVisualScale(-1.0f) &&
                    !f.gizmo.setVisualScale(std::numeric_limits<float>::quiet_NaN()) &&
                    !f.gizmo.setVisualScale(std::numeric_limits<float>::infinity()) &&
                    f.gizmo.visualScale() == 1.0f);
        r.check("uipref_visual_scale_accepts_both_bounds",
                f.gizmo.setVisualScale(kGizmoMinVisualScale) &&
                    f.gizmo.visualScale() == kGizmoMinVisualScale &&
                    f.gizmo.setVisualScale(kGizmoMaxVisualScale) &&
                    f.gizmo.visualScale() == kGizmoMaxVisualScale);
        r.check("uipref_visual_scale_records_nothing",
                f.history.undoDepth() == 0 && f.history.redoDepth() == 0);

        const CameraSnapshot camera =
            perspectiveCamera(Vec3{7.0f, 6.0f, 9.0f}, Vec3{0.0f, 0.0f, 0.0f}, f.width, f.height);
        const float bounds[2] = {kGizmoMinVisualScale, kGizmoMaxVisualScale};
        const GizmoMode modes[3] = {GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Scale};

        // The snapshot carries the preference and the placement scale is the
        // product of the two; the camera-derived unit itself is untouched.
        f.gizmo.setVisualScale(kGizmoMinVisualScale);
        const GizmoSnapshot atMin = f.gizmo.snapshot(camera, f.width, f.height);
        f.gizmo.setVisualScale(kGizmoMaxVisualScale);
        const GizmoSnapshot atMax = f.gizmo.snapshot(camera, f.width, f.height);
        r.check("uipref_snapshot_carries_the_visual_scale_beside_the_camera_scale",
                atMin.visible && atMax.visible && atMin.visualScale == kGizmoMinVisualScale &&
                    atMax.visualScale == kGizmoMaxVisualScale &&
                    atMin.worldPerReferenceUnit == atMax.worldPerReferenceUnit &&
                    nearly(gizmoPlacementScale(atMax) / gizmoPlacementScale(atMin),
                           kGizmoMaxVisualScale / kGizmoMinVisualScale, 1e-5));

        // Every handle of every mode is hit at its own pixel at BOTH bounds:
        // the smallest instrument is still fully pickable and the largest still
        // resolves each handle rather than losing one behind another.
        bool allFoundAtBounds = true;
        bool handlesMoveWithScale = true;
        for (int b = 0; b < 2; ++b) {
            f.gizmo.setVisualScale(bounds[b]);
            for (int m = 0; m < 3; ++m) {
                f.gizmo.setMode(modes[m]);
                GizmoHandle handles[kGizmoMaxHandles];
                const int count = gizmoHandlesForMode(modes[m], handles, kGizmoMaxHandles);
                for (int i = 0; i < count; ++i) {
                    float x = 0.0f, y = 0.0f;
                    if (!f.handlePixel(camera, handles[i], &x, &y) ||
                        f.gizmo.hitTest(camera, x, y, f.width, f.height) != handles[i]) {
                        allFoundAtBounds = false;
                    }
                }
            }
        }
        r.check("uipref_every_handle_is_pickable_at_both_visual_bounds", allFoundAtBounds);

        // The handles genuinely move: the X shaft's grab point stands twice as
        // far from the pivot at 1.5 as at 0.75, which is what "size" means.
        {
            f.gizmo.setMode(GizmoMode::Move);
            Vec3 nearGrab{}, farGrab{};
            f.gizmo.setVisualScale(kGizmoMinVisualScale);
            const GizmoSnapshot sMin = f.gizmo.snapshot(camera, f.width, f.height);
            f.gizmo.setVisualScale(kGizmoMaxVisualScale);
            const GizmoSnapshot sMax = f.gizmo.snapshot(camera, f.width, f.height);
            handlesMoveWithScale = gizmoHandleGrabPoint(sMin, GizmoHandle::AxisX, &nearGrab) &&
                                   gizmoHandleGrabPoint(sMax, GizmoHandle::AxisX, &farGrab);
            const Vec3 nearOffset = vec3Sub(nearGrab, sMin.pivot);
            const float nearDistance = std::sqrt(vec3Dot(nearOffset, nearOffset));
            const Vec3 farOffset = vec3Sub(farGrab, sMax.pivot);
            const float farDistance = std::sqrt(vec3Dot(farOffset, farOffset));
            handlesMoveWithScale = handlesMoveWithScale &&
                                   nearly(farDistance / nearDistance,
                                          kGizmoMaxVisualScale / kGizmoMinVisualScale, 1e-4);
        }
        r.check("uipref_handles_stand_further_out_at_a_larger_visual_size", handlesMoveWithScale);

        // The hit CORRIDOR does not shrink with the instrument: at the smallest
        // size a touch 20 reference units off the X shaft's grab point, across
        // the shaft, still lands on that shaft. The corridor is 24 units wide
        // either side, in density pixels, whatever the visual scale.
        {
            f.gizmo.setVisualScale(kGizmoMinVisualScale);
            f.gizmo.setMode(GizmoMode::Move);
            const GizmoSnapshot s = f.gizmo.snapshot(camera, f.width, f.height);
            float gx = 0.0f, gy = 0.0f, px = 0.0f, py = 0.0f;
            bool ok = f.handlePixel(camera, GizmoHandle::AxisX, &gx, &gy) &&
                      projectWorldToScreen(camera, s.pivot, f.width, f.height, &px, &py);
            if (ok) {
                // Perpendicular, on screen, to the projected shaft direction.
                float dx = gx - px, dy = gy - py;
                const float length = std::sqrt(dx * dx + dy * dy);
                ok = length > 1.0f;
                dx /= length;
                dy /= length;
                const float off = 20.0f * gizmoPixelsPerReferenceUnit();
                ok = ok && f.gizmo.hitTest(camera, gx - dy * off, gy + dx * off, f.width,
                                           f.height) == GizmoHandle::AxisX;
            }
            r.check("uipref_the_hit_corridor_keeps_its_floor_at_the_smallest_size", ok);
        }

        // NO EFFECT ON THE AMOUNT. The scale mapping's reference is the
        // canonical handle length, so the same pixel drag on the X cube
        // stretches the body by exactly the same factor at both visual sizes.
        {
            double factors[2] = {0.0, 0.0};
            bool ran = true;
            for (int b = 0; b < 2; ++b) {
                Fixture g;
                g.gizmo.setVisualScale(bounds[b]);
                g.gizmo.setMode(GizmoMode::Scale);
                float x = 0.0f, y = 0.0f;
                if (!g.grab(1, camera, GizmoHandle::AxisX, &x, &y)) {
                    ran = false;
                    continue;
                }
                float hx = 0.0f, hy = 0.0f, ppx = 0.0f, ppy = 0.0f;
                const GizmoSnapshot state = g.gizmo.snapshot(camera, g.width, g.height);
                Vec3 handleWorld{};
                gizmoHandleGrabPoint(state, GizmoHandle::AxisX, &handleWorld);
                projectWorldToScreen(camera, handleWorld, g.width, g.height, &hx, &hy);
                projectWorldToScreen(camera, state.pivot, g.width, g.height, &ppx, &ppy);
                float dx = hx - ppx, dy = hy - ppy;
                const float length = std::sqrt(dx * dx + dy * dy);
                dx /= length;
                dy /= length;
                g.gizmo.updateDrag(1, camera, x + dx * 60.0f, y + dy * 60.0f, g.width, g.height);
                factors[b] = g.placement().scaleX;
                g.gizmo.cancelDrag();
            }
            r.check("uipref_the_same_pixel_drag_scales_by_the_same_factor_at_every_visual_size",
                    ran && factors[0] > 1.0 && nearly(factors[0], factors[1], 1e-6));
        }

        // And the same for the uniform handle, whose reference is a constant.
        {
            double factors[2] = {0.0, 0.0};
            bool ran = true;
            for (int b = 0; b < 2; ++b) {
                Fixture g;
                g.gizmo.setVisualScale(bounds[b]);
                g.gizmo.setMode(GizmoMode::Scale);
                float x = 0.0f, y = 0.0f;
                if (!g.grab(1, camera, GizmoHandle::Uniform, &x, &y)) {
                    ran = false;
                    continue;
                }
                g.gizmo.updateDrag(1, camera, x + 40.0f, y - 40.0f, g.width, g.height);
                factors[b] = g.placement().scaleX;
                g.gizmo.cancelDrag();
            }
            r.check("uipref_a_uniform_drag_is_the_same_factor_at_every_visual_size",
                    ran && factors[0] > 1.0 && nearly(factors[0], factors[1], 1e-9));
        }

        // A move is solved from the RAY, so the same two pixels move the body
        // by the same world distance whatever size the instrument is drawn at.
        {
            double moved[2] = {0.0, 0.0};
            bool ran = true;
            float startX = 0.0f, startY = 0.0f;
            for (int b = 0; b < 2; ++b) {
                Fixture g;
                g.gizmo.setVisualScale(bounds[b]);
                g.gizmo.setMode(GizmoMode::Move);
                float x = 0.0f, y = 0.0f;
                if (b == 0) {
                    if (!g.grab(1, camera, GizmoHandle::AxisX, &x, &y)) {
                        ran = false;
                        continue;
                    }
                    startX = x;
                    startY = y;
                } else {
                    // The SAME pixel the small instrument was grabbed at: it is
                    // inside the large instrument's grab span too.
                    x = startX;
                    y = startY;
                    if (g.gizmo.hitTest(camera, x, y, g.width, g.height) != GizmoHandle::AxisX ||
                        !g.gizmo.beginDrag(1, camera, x, y, g.width, g.height)) {
                        ran = false;
                        continue;
                    }
                }
                g.gizmo.updateDrag(1, camera, x + 45.0f, y + 10.0f, g.width, g.height);
                moved[b] = g.placement().positionX;
                g.gizmo.cancelDrag();
            }
            r.check("uipref_the_same_two_pixels_move_the_body_the_same_distance_at_every_size",
                    ran && moved[0] != 0.0 && nearly(moved[0], moved[1], 1e-9));
        }
    }

    // -----------------------------------------------------------------------
    // UI-PREF-R1 F: the stroke weight recipes
    //
    // Regular IS the pre-preference gizmo: the two-argument generator and the
    // Regular recipe produce the same bytes, and the counts every existing
    // reader names are the Regular counts. The other two recipes are
    // bounded, finite and drawn in their own contiguous ranges.
    // -----------------------------------------------------------------------
    {
        static GizmoVertex regularA[kGizmoVertexCountMax];
        static GizmoVertex regularB[kGizmoVertexCountMax];
        static GizmoVertex thin[kGizmoVertexCountMax];
        static GizmoVertex bold[kGizmoVertexCountMax];
        const int wroteA = generateGizmoVertices(regularA, kGizmoVertexCountMax);
        const int wroteB =
            generateGizmoVertices(regularB, kGizmoVertexCountMax, GizmoStrokeWeight::Regular);
        const int wroteThin =
            generateGizmoVertices(thin, kGizmoVertexCountMax, GizmoStrokeWeight::Thin);
        const int wroteBold =
            generateGizmoVertices(bold, kGizmoVertexCountMax, GizmoStrokeWeight::Bold);

        r.check("uipref_regular_is_the_pre_preference_gizmo_byte_for_byte",
                wroteA == kGizmoVertexCount && wroteB == kGizmoVertexCount &&
                    std::memcmp(regularA, regularB, sizeof(GizmoVertex) * kGizmoVertexCount) == 0);
        r.check("uipref_the_regular_count_is_the_accepted_1116",
                kGizmoVertexCount == 1116 && gizmoVertexCountFor(GizmoStrokeWeight::Regular) == 1116);
        r.check("uipref_thin_has_the_regular_count_and_different_bytes",
                wroteThin == kGizmoVertexCount &&
                    std::memcmp(regularA, thin, sizeof(GizmoVertex) * kGizmoVertexCount) != 0);
        r.check("uipref_bold_is_the_widest_recipe_and_sizes_the_buffer",
                wroteBold == gizmoVertexCountFor(GizmoStrokeWeight::Bold) &&
                    gizmoVertexCountFor(GizmoStrokeWeight::Bold) == 2268 &&
                    kGizmoVertexCountMax == 2268 && wroteBold > wroteThin);
        r.check("uipref_a_too_small_capacity_writes_nothing",
                generateGizmoVertices(bold, kGizmoVertexCount, GizmoStrokeWeight::Bold) == 0);

        // The centre line of every shaft is the same line in every recipe: a
        // weight widens the band around the stroke and never moves the stroke.
        // The first shaft line written after the pivot mark is that centre.
        const int centre = 2 * kGizmoPivotMarkLineCount;  // vertex index
        r.check("uipref_the_shaft_centre_line_is_identical_in_every_recipe",
                std::memcmp(&regularA[centre], &thin[centre], 2 * sizeof(GizmoVertex)) == 0 &&
                    std::memcmp(&regularA[centre], &bold[centre], 2 * sizeof(GizmoVertex)) == 0);

        // Every recipe's ranges are contiguous, ordered Move/Rotate/Scale and
        // end exactly at its own count; every vertex is finite.
        const GizmoStrokeWeight weights[3] = {GizmoStrokeWeight::Thin, GizmoStrokeWeight::Regular,
                                              GizmoStrokeWeight::Bold};
        const GizmoVertex* lists[3] = {thin, regularA, bold};
        bool rangesHold = true;
        bool allFinite = true;
        for (int w = 0; w < 3; ++w) {
            int first = 0, count = 0, expectedFirst = 0;
            const GizmoMode modes[3] = {GizmoMode::Move, GizmoMode::Rotate, GizmoMode::Scale};
            for (int m = 0; m < 3; ++m) {
                if (!gizmoVertexRange(modes[m], weights[w], &first, &count) ||
                    first != expectedFirst || count <= 0) {
                    rangesHold = false;
                }
                expectedFirst = first + count;
            }
            if (expectedFirst != gizmoVertexCountFor(weights[w])) {
                rangesHold = false;
            }
            for (int i = 0; i < gizmoVertexCountFor(weights[w]); ++i) {
                for (int c = 0; c < 3; ++c) {
                    if (!std::isfinite(lists[w][i].position[c])) {
                        allFinite = false;
                    }
                }
            }
        }
        r.check("uipref_every_recipe_draws_contiguous_ordered_ranges", rangesHold);
        r.check("uipref_every_recipe_is_finite", allFinite);
        r.check("uipref_the_two_argument_range_is_the_regular_range", [] {
            int a = 0, b = 0, c = 0, d = 0;
            return gizmoVertexRange(GizmoMode::Scale, &a, &b) &&
                   gizmoVertexRange(GizmoMode::Scale, GizmoStrokeWeight::Regular, &c, &d) &&
                   a == c && b == d && a == kGizmoScaleFirstVertex;
        }());

        // The bold band is genuinely wider: its widest shaft offset is twice
        // the regular spread, and the thin band is half of it.
        bool spreads = true;
        {
            const GizmoStrokeStyle t = gizmoStrokeStyle(GizmoStrokeWeight::Thin);
            const GizmoStrokeStyle g = gizmoStrokeStyle(GizmoStrokeWeight::Regular);
            const GizmoStrokeStyle b = gizmoStrokeStyle(GizmoStrokeWeight::Bold);
            spreads = nearly(t.spread, 0.5f * g.spread, 1e-6) && g.spread == kGizmoStrokeOffsetUnits &&
                      b.spread == g.spread && b.shaftBundle == 13 && g.shaftBundle == 5 &&
                      t.shaftBundle == 5 && b.ringPasses == 4 && g.ringPasses == 2;
        }
        r.check("uipref_stroke_recipes_are_the_documented_bundles", spreads);
    }

    return r.n;
}

}  // namespace forgeshape
