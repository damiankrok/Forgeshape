#include "forgeshape_transform_selftest.h"

#include <cmath>
#include <limits>

#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    TransformSelfTestResult* out;
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

// Justified tolerance. The matrices are float and every entry is a sine or
// cosine, so an exact 0 or 1 is not achievable: cos(pi/2) in float is about
// 4.4e-8, and a few chained multiplies accumulate a little more. 1e-5 is two
// orders of magnitude above that floor and still far tighter than any real
// convention error, which would be off by whole units or by a sign.
constexpr float kEpsilon = 1e-5f;

bool nearly(float a, float b) { return std::fabs(a - b) <= kEpsilon; }

bool nearlyVec(const Vec3& v, float x, float y, float z) {
    return nearly(v.x, x) && nearly(v.y, y) && nearly(v.z, z);
}

// Identity check for a round-tripped rigid transform.
//
// The 3x3 linear block is products of sines and cosines, so its residual is a
// small multiple of the float epsilon regardless of how far the object is from
// the origin: kEpsilon holds there.
//
// The translation column is different. Undoing a translation of magnitude |p|
// subtracts two numbers of that magnitude, so its absolute residual grows
// linearly with |p|: roughly |p| * 2^-23 (about 1.2e-4 at |p| = 1000). That is a
// property of float arithmetic, not a defect, so the bound is stated as
// kEpsilon + |p| * 1e-6 — about eight times the expected residual, and still far
// tighter than any real convention error, which would be off by whole units or
// by a sign.
bool nearlyIdentity(const Mat4& m, float positionMagnitude = 0.0f) {
    const Mat4 id = mat4Identity();
    const float translationEpsilon = kEpsilon + positionMagnitude * 1e-6f;
    for (int i = 0; i < 16; ++i) {
        const bool isTranslation = (i >= 12 && i <= 14);
        const float limit = isTranslation ? translationEpsilon : kEpsilon;
        if (std::fabs(m.m[i] - id.m[i]) > limit) {
            return false;
        }
    }
    return true;
}

// Largest absolute position component, which is what bounds the round-trip
// residual above.
float positionMagnitudeOf(const TransformValues& v) {
    double largest = std::fabs(v.positionX);
    largest = std::fmax(largest, std::fabs(v.positionY));
    largest = std::fmax(largest, std::fabs(v.positionZ));
    return static_cast<float>(largest);
}

TransformValues makeValues(double px, double py, double pz, double rx, double ry, double rz) {
    TransformValues v;
    v.positionX = px;
    v.positionY = py;
    v.positionZ = pz;
    v.rotationX = rx;
    v.rotationY = ry;
    v.rotationZ = rz;
    return v;
}

TriangleMeshView viewOf(const ConstructionMesh& mesh) {
    TriangleMeshView view{};
    view.positions = mesh.vertices.empty() ? nullptr : mesh.vertices[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    view.indices = mesh.indices.empty() ? nullptr : mesh.indices.data();
    view.indexCount = static_cast<uint32_t>(mesh.indices.size());
    return view;
}

Ray makeRay(const Vec3& origin, const Vec3& direction) {
    Ray r;
    r.origin = origin;
    r.direction = direction;
    return r;
}

// ---------------------------------------------------------------------------
// Domain state
// ---------------------------------------------------------------------------

void testDefaults(Recorder& r) {
    ConstructionTransform t;
    const TransformValues v = t.values();
    r.check("default_position_is_origin",
            v.positionX == 0.0 && v.positionY == 0.0 && v.positionZ == 0.0);
    r.check("default_rotation_is_zero",
            v.rotationX == 0.0 && v.rotationY == 0.0 && v.rotationZ == 0.0);
    r.check("default_is_identity_flag", t.isIdentity());
    r.check("default_counts_zero", t.updateCount() == 0 && t.rejectedUpdateCount() == 0);
    r.check("default_model_is_identity", nearlyIdentity(t.modelMatrix()));
    r.check("default_inverse_is_identity", nearlyIdentity(t.inverseModelMatrix()));

    // The authoritative unit is double: a value with no float representation
    // must survive domain state bit-exactly.
    ConstructionTransform precise;
    const double awkward = 0.1234567890123456;
    r.check("double_position_retained_exactly",
            precise.setValues(makeValues(awkward, 0.0, 0.0, 0.0, 0.0, 0.0)) ==
                    TransformUpdateStatus::Applied &&
                precise.positionXMeters() == awkward);
}

void testUpdateSemantics(Recorder& r) {
    ConstructionTransform t;

    const TransformUpdateStatus applied =
        t.setValues(makeValues(0.75, 0.25, 0.0, 15.0, 30.0, 10.0));
    r.check("valid_change_is_applied", applied == TransformUpdateStatus::Applied);
    r.check("applied_values_are_exact",
            t.positionXMeters() == 0.75 && t.positionYMeters() == 0.25 &&
                t.positionZMeters() == 0.0 && t.rotationXDegrees() == 15.0 &&
                t.rotationYDegrees() == 30.0 && t.rotationZDegrees() == 10.0);
    r.check("applied_counts_once", t.updateCount() == 1 && t.rejectedUpdateCount() == 0);
    r.check("applied_is_not_identity", !t.isIdentity());

    const TransformUpdateStatus repeat =
        t.setValues(makeValues(0.75, 0.25, 0.0, 15.0, 30.0, 10.0));
    r.check("identical_values_are_unchanged", repeat == TransformUpdateStatus::Unchanged);
    r.check("identical_values_do_not_count", t.updateCount() == 1);

    // One differing value out of six is still a change.
    const TransformUpdateStatus oneAxis =
        t.setValues(makeValues(0.75, 0.25, 0.0, 15.0, 30.0, 10.5));
    r.check("single_value_change_is_applied", oneAxis == TransformUpdateStatus::Applied &&
                                                  t.rotationZDegrees() == 10.5 &&
                                                  t.updateCount() == 2);

    // Zero and negative are ordinary places and ordinary angles.
    ConstructionTransform negatives;
    const TransformUpdateStatus negative =
        negatives.setValues(makeValues(-1.5, 0.0, -0.25, -90.0, 0.0, -45.0));
    r.check("negative_position_is_applied",
            negative == TransformUpdateStatus::Applied && negatives.positionXMeters() == -1.5 &&
                negatives.positionZMeters() == -0.25);
    r.check("negative_rotation_is_applied",
            negatives.rotationXDegrees() == -90.0 && negatives.rotationZDegrees() == -45.0);
    r.check("zero_value_is_valid",
            validateTransformValue(0.0) == TransformValidation::Ok &&
                validateTransformValue(-1.0) == TransformValidation::Ok);

    // Rotation is stored exactly as given: 370 is not silently rewritten to 10.
    ConstructionTransform wide;
    wide.setValues(makeValues(0.0, 0.0, 0.0, 370.0, 0.0, 0.0));
    r.check("rotation_is_not_canonicalized", wide.rotationXDegrees() == 370.0);
    // ...but its derived trigonometry is the same as 10 degrees.
    ConstructionTransform narrow;
    narrow.setValues(makeValues(0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
    const Mat4 a = wide.modelMatrix();
    const Mat4 b = narrow.modelMatrix();
    bool sameMatrix = true;
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(a.m[i] - b.m[i]) > kEpsilon) sameMatrix = false;
    }
    r.check("wrapped_rotation_has_same_derived_matrix", sameMatrix);
}

void testRejection(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        TransformValues values;
        TransformValidation expected;
    };
    const Case cases[] = {
        {"reject_nan_position_x", makeValues(nan, 0, 0, 0, 0, 0), TransformValidation::NotFinite},
        {"reject_nan_position_z", makeValues(0, 0, nan, 0, 0, 0), TransformValidation::NotFinite},
        {"reject_inf_position_y", makeValues(0, inf, 0, 0, 0, 0), TransformValidation::NotFinite},
        {"reject_nan_rotation_x", makeValues(0, 0, 0, nan, 0, 0), TransformValidation::NotFinite},
        {"reject_nan_rotation_z", makeValues(0, 0, 0, 0, 0, nan), TransformValidation::NotFinite},
        {"reject_negative_inf_rotation_y", makeValues(0, 0, 0, 0, -inf, 0),
         TransformValidation::NotFinite},
        // Finite as a double, but the derived float matrix could not hold it.
        {"reject_beyond_float_position", makeValues(1e40, 0, 0, 0, 0, 0),
         TransformValidation::NotRepresentable},
        {"reject_beyond_float_rotation", makeValues(0, 0, 0, 0, 0, 1e40),
         TransformValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        ConstructionTransform t;
        t.setValues(makeValues(0.75, 0.25, -0.5, 15.0, 30.0, 10.0));
        const TransformValues before = t.values();

        TransformValidation why = TransformValidation::Ok;
        const TransformUpdateStatus status = t.setValues(c.values, &why);

        const bool rejected = status == TransformUpdateStatus::Rejected && why == c.expected;
        const TransformValues after = t.values();
        const bool preserved =
            after.positionX == before.positionX && after.positionY == before.positionY &&
            after.positionZ == before.positionZ && after.rotationX == before.rotationX &&
            after.rotationY == before.rotationY && after.rotationZ == before.rotationZ;
        r.check(c.name, rejected && preserved);
    }

    // A rejection must not half-apply the five values that were fine.
    ConstructionTransform t;
    t.setValues(makeValues(0.75, 0.25, -0.5, 15.0, 30.0, 10.0));
    TransformValidation why = TransformValidation::Ok;
    const TransformUpdateStatus status =
        t.setValues(makeValues(9.0, 9.0, 9.0, 9.0, 9.0, nan), &why);
    const TransformValues after = t.values();
    r.check("rejection_does_not_partially_apply",
            status == TransformUpdateStatus::Rejected && why == TransformValidation::NotFinite &&
                after.positionX == 0.75 && after.positionY == 0.25 && after.positionZ == -0.5 &&
                after.rotationX == 15.0 && after.rotationY == 30.0 && after.rotationZ == 10.0);
    r.check("rejection_counted", t.rejectedUpdateCount() == 1 && t.updateCount() == 1);
    // A rejected transform still produces the previous valid matrix.
    r.check("matrix_after_rejection_is_previous_valid",
            mat4Finite(t.modelMatrix()) &&
                nearly(t.modelMatrix().m[12], 0.75f) && nearly(t.modelMatrix().m[13], 0.25f));
}

// ---------------------------------------------------------------------------
// The convention: axes, Euler order, inverse
// ---------------------------------------------------------------------------

void testTranslation(Recorder& r) {
    ConstructionTransform t;
    t.setValues(makeValues(0.75, 0.25, -1.5, 0.0, 0.0, 0.0));
    const Mat4 model = t.modelMatrix();
    r.check("translation_lands_in_matrix_column_3",
            nearly(model.m[12], 0.75f) && nearly(model.m[13], 0.25f) &&
                nearly(model.m[14], -1.5f));
    r.check("translation_moves_a_point",
            nearlyVec(mat4TransformPoint(model, Vec3{1.0f, 2.0f, 3.0f}), 1.75f, 2.25f, 1.5f));
    // A direction has no position, so translation must not touch it.
    r.check("translation_does_not_move_a_direction",
            nearlyVec(mat4TransformDirection(model, Vec3{0.0f, 1.0f, 0.0f}), 0.0f, 1.0f, 0.0f));
}

void testRightHandedAxisRotations(Recorder& r) {
    // Right-hand rule: about +X, +Y turns toward +Z.
    ConstructionTransform x;
    x.setValues(makeValues(0, 0, 0, 90.0, 0, 0));
    r.check("rotate_x_plus_90_takes_y_to_z",
            nearlyVec(mat4TransformPoint(x.modelMatrix(), Vec3{0.0f, 1.0f, 0.0f}), 0.0f, 0.0f,
                      1.0f));
    r.check("rotate_x_plus_90_leaves_x_axis",
            nearlyVec(mat4TransformPoint(x.modelMatrix(), Vec3{1.0f, 0.0f, 0.0f}), 1.0f, 0.0f,
                      0.0f));

    // About +Y, +Z turns toward +X.
    ConstructionTransform y;
    y.setValues(makeValues(0, 0, 0, 0, 90.0, 0));
    r.check("rotate_y_plus_90_takes_z_to_x",
            nearlyVec(mat4TransformPoint(y.modelMatrix(), Vec3{0.0f, 0.0f, 1.0f}), 1.0f, 0.0f,
                      0.0f));
    r.check("rotate_y_plus_90_leaves_y_axis",
            nearlyVec(mat4TransformPoint(y.modelMatrix(), Vec3{0.0f, 1.0f, 0.0f}), 0.0f, 1.0f,
                      0.0f));

    // About +Z, +X turns toward +Y.
    ConstructionTransform z;
    z.setValues(makeValues(0, 0, 0, 0, 0, 90.0));
    r.check("rotate_z_plus_90_takes_x_to_y",
            nearlyVec(mat4TransformPoint(z.modelMatrix(), Vec3{1.0f, 0.0f, 0.0f}), 0.0f, 1.0f,
                      0.0f));
    r.check("rotate_z_plus_90_leaves_z_axis",
            nearlyVec(mat4TransformPoint(z.modelMatrix(), Vec3{0.0f, 0.0f, 1.0f}), 0.0f, 0.0f,
                      1.0f));

    // A negative angle must undo the positive one exactly.
    ConstructionTransform negative;
    negative.setValues(makeValues(0, 0, 0, 0, 0, -90.0));
    r.check("rotate_z_minus_90_takes_x_to_minus_y",
            nearlyVec(mat4TransformPoint(negative.modelMatrix(), Vec3{1.0f, 0.0f, 0.0f}), 0.0f,
                      -1.0f, 0.0f));
}

void testEulerOrder(Recorder& r) {
    // Documented order is LOCAL X -> Y -> Z, i.e. Model = T * Rz * Ry * Rx, so
    // Rx acts on the object FIRST.
    //
    // Take (0,1,0). Rx(+90) sends it to (0,0,1); Ry(+90) then sends that to
    // (1,0,0). The opposite order would give Ry(+90) on (0,1,0) = (0,1,0),
    // then Rx(+90) = (0,0,1) — a different answer, so this check actually
    // discriminates between the two conventions.
    ConstructionTransform t;
    t.setValues(makeValues(0, 0, 0, 90.0, 90.0, 0));
    const Vec3 result = mat4TransformPoint(t.modelMatrix(), Vec3{0.0f, 1.0f, 0.0f});
    r.check("euler_order_is_x_then_y_then_z", nearlyVec(result, 1.0f, 0.0f, 0.0f));
    r.check("euler_order_is_not_z_then_y_then_x", !nearlyVec(result, 0.0f, 0.0f, 1.0f));

    // Translation is applied LAST, so it is not rotated by the object's own
    // rotation: a rotated object at (2,0,0) is still centred at (2,0,0).
    ConstructionTransform moved;
    moved.setValues(makeValues(2.0, 0.0, 0.0, 0.0, 0.0, 90.0));
    r.check("translation_applies_after_rotation",
            nearlyVec(mat4TransformPoint(moved.modelMatrix(), Vec3{0.0f, 0.0f, 0.0f}), 2.0f, 0.0f,
                      0.0f));
    r.check("rotation_still_acts_on_local_offset",
            nearlyVec(mat4TransformPoint(moved.modelMatrix(), Vec3{1.0f, 0.0f, 0.0f}), 2.0f, 1.0f,
                      0.0f));
}

void testModelInversePair(Recorder& r) {
    const TransformValues cases[] = {
        makeValues(0, 0, 0, 0, 0, 0),
        makeValues(0.75, 0.25, 0.0, 0, 0, 0),
        makeValues(0, 0, 0, 15.0, 30.0, 10.0),
        makeValues(0.75, 0.25, -1.5, 15.0, 30.0, 10.0),
        makeValues(-3.5, 12.25, 0.125, -90.0, 180.0, -45.0),
        makeValues(1000.0, -1000.0, 0.5, 370.0, -720.5, 33.75),
    };
    const char* finiteNames[] = {
        "model_finite_identity",  "model_finite_translated", "model_finite_rotated",
        "model_finite_combined",  "model_finite_negative",   "model_finite_large",
    };
    const char* inverseNames[] = {
        "inverse_finite_identity", "inverse_finite_translated", "inverse_finite_rotated",
        "inverse_finite_combined", "inverse_finite_negative",   "inverse_finite_large",
    };
    const char* roundTripNames[] = {
        "model_times_inverse_is_identity_identity",
        "model_times_inverse_is_identity_translated",
        "model_times_inverse_is_identity_rotated",
        "model_times_inverse_is_identity_combined",
        "model_times_inverse_is_identity_negative",
        "model_times_inverse_is_identity_large",
    };
    const char* reverseNames[] = {
        "inverse_times_model_is_identity_identity",
        "inverse_times_model_is_identity_translated",
        "inverse_times_model_is_identity_rotated",
        "inverse_times_model_is_identity_combined",
        "inverse_times_model_is_identity_negative",
        "inverse_times_model_is_identity_large",
    };

    for (int i = 0; i < 6; ++i) {
        ConstructionTransform t;
        t.setValues(cases[i]);
        const Mat4 model = t.modelMatrix();
        const Mat4 inverse = t.inverseModelMatrix();
        const float magnitude = positionMagnitudeOf(cases[i]);
        r.check(finiteNames[i], mat4Finite(model));
        r.check(inverseNames[i], mat4Finite(inverse));
        r.check(roundTripNames[i], nearlyIdentity(mat4Multiply(model, inverse), magnitude));
        r.check(reverseNames[i], nearlyIdentity(mat4Multiply(inverse, model), magnitude));
    }

    // The inverse is genuinely rigid: it has no scale, so it preserves lengths.
    ConstructionTransform t;
    t.setValues(makeValues(0.75, 0.25, -1.5, 15.0, 30.0, 10.0));
    const Vec3 d = mat4TransformDirection(t.inverseModelMatrix(), Vec3{0.0f, -1.0f, 0.0f});
    r.check("inverse_preserves_direction_length", nearly(std::sqrt(vec3Dot(d, d)), 1.0f));
}

// ---------------------------------------------------------------------------
// Picking against a transformed object
// ---------------------------------------------------------------------------

void testWorldRayToLocal(Recorder& r) {
    ConstructionTransform t;
    t.setValues(makeValues(5.0, 0.0, 0.0, 0.0, 0.0, 0.0));

    Ray local{};
    const bool ok = transformRayToLocal(
        makeRay(Vec3{5.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), t.inverseModelMatrix(), &local);
    r.check("world_ray_to_local_translated_origin",
            ok && nearlyVec(local.origin, 0.0f, 10.0f, 0.0f));
    r.check("world_ray_to_local_translation_leaves_direction",
            ok && nearlyVec(local.direction, 0.0f, -1.0f, 0.0f));

    // Under a rotation the direction turns by the inverse rotation.
    ConstructionTransform rotated;
    rotated.setValues(makeValues(0.0, 0.0, 0.0, 0.0, 0.0, 90.0));
    Ray localRotated{};
    const bool ok2 =
        transformRayToLocal(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                            rotated.inverseModelMatrix(), &localRotated);
    r.check("world_ray_to_local_rotated_origin",
            ok2 && nearlyVec(localRotated.origin, 10.0f, 0.0f, 0.0f));
    r.check("world_ray_to_local_rotated_direction",
            ok2 && nearlyVec(localRotated.direction, -1.0f, 0.0f, 0.0f));

    Ray rejected{};
    Mat4 broken = mat4Identity();
    broken.m[0] = std::numeric_limits<float>::quiet_NaN();
    r.check("world_ray_to_local_rejects_non_finite_matrix",
            !transformRayToLocal(makeRay(Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), broken,
                                 &rejected));
}

void testTransformedBoxHit(Recorder& r) {
    // The default 2.0 x 1.0 x 0.5 m box, in its own local space.
    ConstructionBox box;
    const ConstructionMesh mesh = box.generateMesh();
    const TriangleMeshView view = viewOf(mesh);

    // Case 1: moved to (0.75, 0.25, 0). A ray fired straight down the box's new
    // centre must hit the top face at world y = 0.25 + 0.5 = 0.75.
    {
        ConstructionTransform t;
        t.setValues(makeValues(0.75, 0.25, 0.0, 0.0, 0.0, 0.0));
        Ray local{};
        const bool ok = transformRayToLocal(
            makeRay(Vec3{0.75f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), t.inverseModelMatrix(),
            &local);
        const TriangleHit hit = pickTriangleMesh(local, view, /*frontFacesOnly=*/true);
        const Vec3 world = mat4TransformPoint(t.modelMatrix(), hit.position);
        r.check("translated_box_is_hit", ok && hit.hit);
        r.check("translated_box_hit_is_at_moved_top_face",
                hit.hit && nearlyVec(world, 0.75f, 0.75f, 0.0f));
        // Rigid transform: the local distance is already the world distance.
        r.check("translated_box_hit_distance_is_world_distance",
                hit.hit && nearly(hit.t, 9.25f));
    }

    // Case 2: rotated 90 degrees about +Z. The box's local +X (half-extent 1.0 m)
    // now points along world +Y, so a ray straight down must hit at y = 1.0 —
    // higher than the unrotated box's 0.5 m top face, which is exactly what
    // proves the rotation reached the picker.
    {
        ConstructionTransform t;
        t.setValues(makeValues(0.0, 0.0, 0.0, 0.0, 0.0, 90.0));
        Ray local{};
        const bool ok = transformRayToLocal(
            makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), t.inverseModelMatrix(),
            &local);
        const TriangleHit hit = pickTriangleMesh(local, view, /*frontFacesOnly=*/true);
        const Vec3 world = mat4TransformPoint(t.modelMatrix(), hit.position);
        r.check("rotated_box_is_hit", ok && hit.hit);
        r.check("rotated_box_hit_uses_rotated_extent",
                hit.hit && nearly(world.y, 1.0f) && nearly(world.x, 0.0f));
        r.check("rotated_box_hit_is_not_unrotated_extent", hit.hit && !nearly(world.y, 0.5f));
    }

    // Case 3: a ray that hit the box at the origin must MISS once the box has
    // been moved far away. Picking that ignored the transform would still hit.
    {
        ConstructionTransform t;
        t.setValues(makeValues(50.0, 0.0, 0.0, 0.0, 0.0, 0.0));
        Ray local{};
        const bool ok = transformRayToLocal(
            makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), t.inverseModelMatrix(),
            &local);
        const TriangleHit hit = pickTriangleMesh(local, view, /*frontFacesOnly=*/true);
        r.check("ray_misses_box_that_moved_away", ok && !hit.hit);
    }
}

// ---------------------------------------------------------------------------
// The boundary that matters: a transform is not a mesh revision
// ---------------------------------------------------------------------------

void testTransformDoesNotTouchMesh(Recorder& r) {
    ConstructionBox box;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision published = publishConstructionBox(box, store);
    const RuntimeMeshPtr before = store.current();
    const uint64_t publishedBefore = store.publishedCount();

    ConstructionTransform t;
    const TransformApplyResult moved =
        applyTransformValues(t, makeValues(0.75, 0.25, -1.5, 15.0, 30.0, 10.0));
    const TransformApplyResult rotated =
        applyTransformValues(t, makeValues(0.75, 0.25, -1.5, 90.0, 0.0, 0.0));

    r.check("transform_applies_are_applied",
            moved.status == TransformUpdateStatus::Applied &&
                rotated.status == TransformUpdateStatus::Applied);
    r.check("transform_does_not_change_mesh_revision", store.currentRevision() == published);
    r.check("transform_does_not_publish", store.publishedCount() == publishedBefore);
    // The very same immutable revision object, not merely an equal one: nothing
    // regenerated the geometry.
    r.check("transform_leaves_runtime_mesh_identical", store.current() == before);
    r.check("transform_leaves_vertex_data_local",
            before != nullptr && before->vertexCount() == kBoxVertexCount &&
                before->indexCount() == kBoxIndexCount &&
                nearly(before->vertices()[0].position[0], -1.0f) &&
                nearly(before->vertices()[0].position[1], -0.5f));

    // ...and the reverse independence: changing the dimensions must not disturb
    // the transform.
    const TransformValues transformBefore = t.values();
    box.setDimensionsMeters(3.0, 4.0, 5.0);
    const MeshRevision republished = publishConstructionBox(box, store);
    const TransformValues transformAfter = t.values();
    r.check("dimension_change_publishes_new_revision", republished > published);
    r.check("dimension_change_leaves_transform_untouched",
            transformAfter.positionX == transformBefore.positionX &&
                transformAfter.positionY == transformBefore.positionY &&
                transformAfter.positionZ == transformBefore.positionZ &&
                transformAfter.rotationX == transformBefore.rotationX &&
                transformAfter.rotationY == transformBefore.rotationY &&
                transformAfter.rotationZ == transformBefore.rotationZ);
    r.check("dimension_change_leaves_transform_update_count",
            t.updateCount() == 2 && t.rejectedUpdateCount() == 0);
}

void testApplyEntryPoint(Recorder& r) {
    ConstructionTransform t;

    const TransformApplyResult applied =
        applyTransformValues(t, makeValues(0.75, 0.25, 0.0, 15.0, 30.0, 10.0));
    r.check("entry_point_reports_applied", applied.status == TransformUpdateStatus::Applied);
    r.check("entry_point_reports_new_values",
            applied.values.positionX == 0.75 && applied.values.rotationY == 30.0);

    const TransformApplyResult unchanged =
        applyTransformValues(t, makeValues(0.75, 0.25, 0.0, 15.0, 30.0, 10.0));
    r.check("entry_point_reports_unchanged", unchanged.status == TransformUpdateStatus::Unchanged);

    const TransformApplyResult rejected = applyTransformValues(
        t, makeValues(0.0, 0.0, 0.0, 0.0, 0.0, std::numeric_limits<double>::quiet_NaN()));
    r.check("entry_point_reports_rejected",
            rejected.status == TransformUpdateStatus::Rejected &&
                rejected.validation == TransformValidation::NotFinite);
    r.check("entry_point_reports_retained_values_on_reject",
            rejected.values.positionX == 0.75 && rejected.values.rotationZ == 10.0);
}

// ---------------------------------------------------------------------------
// Scale: the default, the one positivity rule, and the derived matrices
// ---------------------------------------------------------------------------

void testScaleDomain(Recorder& r) {
    ConstructionTransform t;
    r.check("scale_defaults_to_one",
            t.scaleXFactor() == 1.0 && t.scaleYFactor() == 1.0 && t.scaleZFactor() == 1.0 &&
                t.isUnscaled() && t.isIdentity());

    // Ordinary values, both sides of one.
    TransformValues values;
    values.scaleX = 3.0;
    values.scaleY = 0.25;
    values.scaleZ = 1.0;
    r.check("an_ordinary_scale_applies",
            t.setValues(values) == TransformUpdateStatus::Applied && t.scaleXFactor() == 3.0 &&
                t.scaleYFactor() == 0.25 && !t.isUnscaled() && !t.isIdentity());
    r.check("the_same_scale_again_is_unchanged",
            t.setValues(values) == TransformUpdateStatus::Unchanged);

    // The one positivity rule on this transform, and the whole no-Mirror
    // decision expressed as a refusal rather than as a comment.
    const double refused[4] = {0.0, -1.0, -0.5, kMinScaleFactor};
    bool allRefused = true;
    for (double bad : refused) {
        TransformValues request = values;
        request.scaleY = bad;
        TransformValidation why = TransformValidation::Ok;
        allRefused = allRefused && t.setValues(request, &why) == TransformUpdateStatus::Rejected &&
                     why == TransformValidation::NotPositive;
    }
    r.check("zero_and_negative_scale_are_refused", allRefused);
    r.check("a_refused_scale_leaves_every_previous_value_standing",
            t.scaleXFactor() == 3.0 && t.scaleYFactor() == 0.25 && t.scaleZFactor() == 1.0);

    // A non-finite scale is refused for the shared reason, not the positivity
    // one, so the message a user sees names the right problem.
    {
        TransformValues request = values;
        request.scaleZ = std::numeric_limits<double>::quiet_NaN();
        TransformValidation why = TransformValidation::Ok;
        r.check("a_non_finite_scale_is_refused_as_not_finite",
                t.setValues(request, &why) == TransformUpdateStatus::Rejected &&
                    why == TransformValidation::NotFinite);
    }

    // FAILS CLOSED across the whole nine: a bad scale must not leave a good
    // position half applied.
    {
        TransformValues request;
        request.positionX = 99.0;
        request.rotationZ = 45.0;
        request.scaleX = -2.0;
        const TransformApplyResult result = applyTransformValues(t, request);
        r.check("a_bad_scale_leaves_the_position_untouched",
                result.status == TransformUpdateStatus::Rejected &&
                    t.positionXMeters() == 0.0 && t.rotationZDegrees() == 0.0);
    }

    // Position and rotation keep their freedom: only a scale is constrained.
    r.check("zero_and_negative_coordinates_are_still_ordinary",
            validateTransformValue(0.0) == TransformValidation::Ok &&
                validateTransformValue(-12.5) == TransformValidation::Ok &&
                validateScaleValue(1.0) == TransformValidation::Ok);
}

void testScaledMatrices(Recorder& r) {
    ConstructionTransform t;
    TransformValues values;
    values.positionX = 2.0;
    values.rotationY = 90.0;
    values.scaleX = 4.0;
    values.scaleY = 0.5;
    values.scaleZ = 2.0;
    t.setValues(values);

    // Model = T * R * S: the scale acts on the object FIRST, so the local +X of
    // a body turned 90 degrees about Y is stretched by scaleX and then points
    // along world -Z.
    const Vec3 localX = mat4TransformPoint(t.modelMatrix(), Vec3{1.0f, 0.0f, 0.0f});
    r.check("scale_is_applied_in_object_space_before_the_rotation",
            nearlyVec(vec3Sub(localX, Vec3{2.0f, 0.0f, 0.0f}), 0.0f, 0.0f, -4.0f));
    const Vec3 localY = mat4TransformPoint(t.modelMatrix(), Vec3{0.0f, 1.0f, 0.0f});
    r.check("a_second_scaled_axis_is_scaled_by_its_own_factor",
            nearlyVec(vec3Sub(localY, Vec3{2.0f, 0.0f, 0.0f}), 0.0f, 0.5f, 0.0f));

    // The inverse really is the inverse, built from the values rather than
    // numerically, so it cannot drift from the model it undoes.
    const Mat4 round = mat4Multiply(t.inverseModelMatrix(), t.modelMatrix());
    r.check("the_scaled_inverse_undoes_the_scaled_model", nearlyIdentity(round, 2.0f));

    // A world ray carried into local space keeps its PARAMETER, which is what
    // makes a hit distance still mean world meters on a stretched body. See the
    // note in transformRayToLocal.
    {
        Ray world{};
        world.origin = Vec3{5.0f, 3.0f, 1.0f};
        world.direction = vec3Normalize(Vec3{-1.0f, -0.4f, 0.2f});
        Ray local{};
        const bool moved = transformRayToLocal(world, t.inverseModelMatrix(), &local);
        const float parameter = 2.75f;
        const Vec3 localPoint = vec3Add(local.origin, vec3Scale(local.direction, parameter));
        const Vec3 backToWorld = mat4TransformPoint(t.modelMatrix(), localPoint);
        const Vec3 alongWorld = vec3Add(world.origin, vec3Scale(world.direction, parameter));
        r.check("a_local_ray_parameter_is_still_world_distance_under_scale",
                moved && nearlyVec(vec3Sub(backToWorld, alongWorld), 0.0f, 0.0f, 0.0f));
    }

    // The normal matrix is R * S^-1, not R * S. The test that separates them:
    // a face whose object-space normal is +X on a body stretched along X must
    // still come out perpendicular to that face, and the two matrices disagree
    // about that the moment the scale is non-uniform.
    {
        const Mat4 normal = t.normalMatrix();
        const Vec3 carried = mat4TransformDirection(normal, Vec3{1.0f, 0.0f, 0.0f});
        const Vec3 expected{0.0f, 0.0f, -0.25f};  // 1/4 along the rotated +X
        r.check("the_normal_matrix_uses_the_inverse_scale",
                nearlyVec(vec3Sub(carried, expected), 0.0f, 0.0f, 0.0f));

        // And the property that actually matters: a normal stays perpendicular
        // to a tangent it was perpendicular to in object space.
        const Vec3 tangent = mat4TransformDirection(t.modelMatrix(), Vec3{0.0f, 1.0f, 0.0f});
        r.check("a_carried_normal_stays_perpendicular_to_a_carried_tangent",
                std::fabs(vec3Dot(vec3Normalize(carried), vec3Normalize(tangent))) < kEpsilon);
    }

    // An unscaled body is untouched by any of it: the normal matrix IS the
    // rotation, which is why nothing about the existing product changes.
    {
        ConstructionTransform plain;
        TransformValues rotated;
        rotated.rotationX = 21.0;
        rotated.rotationY = -33.0;
        rotated.rotationZ = 57.0;
        plain.setValues(rotated);
        const Mat4 normal = plain.normalMatrix();
        const Mat4 rotation = plain.rotationMatrix();
        bool same = true;
        for (int i = 0; i < 16; ++i) {
            same = same && nearly(normal.m[i], rotation.m[i]);
        }
        r.check("an_unscaled_normal_matrix_is_the_rotation", same && plain.isUnscaled());
    }
}

// ---------------------------------------------------------------------------
// Euler decomposition: the branch-continuous bridge the gizmo rotates through
// ---------------------------------------------------------------------------

bool sameRotation(const Mat4& a, const Mat4& b, float tolerance) {
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            if (std::fabs(a.m[column * 4 + row] - b.m[column * 4 + row]) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

void testEulerDecomposition(Recorder& r) {
    // Round trip: whatever branch is chosen, rebuilding it must give the matrix
    // back. That is the ONLY property a decomposition owes, and it is what makes
    // every other case below a statement about continuity rather than about
    // correctness.
    const EulerDegrees samples[6] = {
        {0.0, 0.0, 0.0},      {37.0, -52.0, 24.0}, {170.0, 12.0, -95.0},
        {-140.0, 61.0, 33.0}, {5.0, 89.0, -7.0},   {12.0, -89.5, 44.0},
    };
    bool roundTrips = true;
    for (const EulerDegrees& sample : samples) {
        const Mat4 rotation = rotationMatrixFromEuler(sample);
        EulerDegrees back{};
        roundTrips = roundTrips && eulerFromRotationMatrix(rotation, sample, &back) &&
                     sameRotation(rotationMatrixFromEuler(back), rotation, 1e-4f);
    }
    r.check("every_decomposition_rebuilds_its_own_matrix", roundTrips);

    // Given the same triple it started from, the decomposition returns it
    // unchanged rather than an equivalent one — which is what stops the
    // exact-value editors rewriting themselves when nothing happened.
    {
        const EulerDegrees start{37.0, -52.0, 24.0};
        EulerDegrees back{};
        r.check("a_decomposition_prefers_the_triple_it_was_given",
                eulerFromRotationMatrix(rotationMatrixFromEuler(start), start, &back) &&
                    nearly(static_cast<float>(back.x), 37.0f) &&
                    nearly(static_cast<float>(back.y), -52.0f) &&
                    nearly(static_cast<float>(back.z), 24.0f));
    }

    // Continuity through +/-180, past 360 and past 720. Ten degrees at a time
    // about world Z from an unturned start, exactly as a slow drag samples it:
    // the Z field has to climb monotonically through every one of those
    // boundaries rather than wrapping.
    {
        EulerDegrees previous{};
        bool monotone = true;
        bool reached = false;
        double last = 0.0;
        for (int step = 1; step <= 80; ++step) {
            const double angle = static_cast<double>(step) * 10.0;
            const Mat4 target = mat4Multiply(elementaryRotationMatrix(2, angle),
                                             rotationMatrixFromEuler(EulerDegrees{}));
            EulerDegrees next{};
            if (!eulerFromRotationMatrix(target, previous, &next)) {
                monotone = false;
                break;
            }
            // A hundredth of a degree. The matrix is float and the angle comes
            // back through an atan2 of two of its entries, so the residual is a
            // few thousandths of a degree at these magnitudes — far above an
            // exact match and far below anything a wrapped answer could look
            // like, which would be wrong by a whole 360.
            monotone = monotone && next.z > last - 1e-3 && std::fabs(next.z - angle) < 1e-2;
            last = next.z;
            previous = next;
        }
        reached = last > 720.0;
        r.check("a_stepped_turn_climbs_past_720_without_wrapping", monotone && reached);
    }

    // The same, downward, so the negative direction is not a separate accident.
    {
        EulerDegrees previous{};
        bool monotone = true;
        double last = 0.0;
        for (int step = 1; step <= 60; ++step) {
            const double angle = static_cast<double>(step) * -10.0;
            const Mat4 target = mat4Multiply(elementaryRotationMatrix(0, angle),
                                             rotationMatrixFromEuler(EulerDegrees{}));
            EulerDegrees next{};
            if (!eulerFromRotationMatrix(target, previous, &next)) {
                monotone = false;
                break;
            }
            monotone = monotone && next.x < last + 1e-3;
            last = next.x;
            previous = next;
        }
        r.check("a_stepped_turn_the_other_way_falls_past_minus_360",
                monotone && last < -360.0);
    }

    // The branch choice. From a `previous` that sits on the SECOND branch, the
    // decomposition must return that branch rather than the principal one: a
    // swap mid-drag would flip all three fields for no motion the user made.
    {
        const EulerDegrees principal{20.0, 30.0, -40.0};
        const Mat4 rotation = rotationMatrixFromEuler(principal);
        const EulerDegrees other{20.0 + 180.0, 180.0 - 30.0, -40.0 + 180.0};
        EulerDegrees back{};
        const bool decomposed = eulerFromRotationMatrix(rotation, other, &back);
        // A thousandth of a degree, for the float-matrix reason above. What is
        // being asserted is which BRANCH came back, and the two branches are 180
        // degrees apart in every component — there is no way to confuse them at
        // this tolerance.
        r.check("the_branch_nearest_the_previous_answer_wins",
                decomposed && std::fabs(back.x - other.x) < 1e-3 &&
                    std::fabs(back.y - other.y) < 1e-3 && std::fabs(back.z - other.z) < 1e-3);
        r.check("the_chosen_branch_is_still_the_same_orientation",
                decomposed && sameRotation(rotationMatrixFromEuler(back), rotation, 1e-4f));
    }

    // A component the rotation never touched comes back EXACTLY as it was, not
    // as a few times 1e-14 — see kEulerStickyDegrees. This is what keeps the
    // exact-value editors from showing a rotation of -0.00000000000006.
    {
        const EulerDegrees start{0.0, 0.0, 0.0};
        const Mat4 target = mat4Multiply(elementaryRotationMatrix(1, 100.0),
                                         rotationMatrixFromEuler(start));
        EulerDegrees back{};
        r.check("an_untouched_euler_component_comes_back_exactly",
                eulerFromRotationMatrix(target, start, &back) && back.x == 0.0 &&
                    back.z == 0.0 && nearly(static_cast<float>(back.y), 100.0f));
    }

    // Gimbal lock: pitch at exactly +/-90. Only the sum or difference of the
    // outer two is determined, so the only assertable property is that the
    // matrix comes back — and that nothing is non-finite.
    {
        bool singularOk = true;
        const double pitches[2] = {90.0, -90.0};
        for (double pitch : pitches) {
            const EulerDegrees at{15.0, pitch, -25.0};
            const Mat4 rotation = rotationMatrixFromEuler(at);
            EulerDegrees back{};
            singularOk = singularOk && eulerFromRotationMatrix(rotation, at, &back) &&
                         std::isfinite(back.x) && std::isfinite(back.y) &&
                         std::isfinite(back.z) &&
                         sameRotation(rotationMatrixFromEuler(back), rotation, 1e-3f);
        }
        r.check("a_singular_orientation_is_finite_and_orientation_correct", singularOk);
    }

    // Refusals: nothing is written and the caller holds its last good value.
    {
        Mat4 bad = mat4Identity();
        bad.m[5] = std::numeric_limits<float>::quiet_NaN();
        EulerDegrees back{1.0, 2.0, 3.0};
        const EulerDegrees previous{};
        r.check("a_non_finite_matrix_has_no_decomposition",
                !eulerFromRotationMatrix(bad, previous, &back) && back.x == 1.0);
        EulerDegrees badPrevious{std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0};
        r.check("a_non_finite_previous_has_no_decomposition",
                !eulerFromRotationMatrix(mat4Identity(), badPrevious, &back) && back.x == 1.0);
    }

    // The elementary rotations are the ones the convention names, and an
    // unknown axis is the identity rather than a silent X.
    {
        r.check("elementary_rotations_match_the_convention",
                sameRotation(elementaryRotationMatrix(0, 30.0),
                             rotationMatrixFromEuler(EulerDegrees{30.0, 0.0, 0.0}), 1e-6f) &&
                    sameRotation(elementaryRotationMatrix(1, 30.0),
                                 rotationMatrixFromEuler(EulerDegrees{0.0, 30.0, 0.0}), 1e-6f) &&
                    sameRotation(elementaryRotationMatrix(2, 30.0),
                                 rotationMatrixFromEuler(EulerDegrees{0.0, 0.0, 30.0}), 1e-6f));
        r.check("an_unknown_elementary_axis_is_the_identity",
                sameRotation(elementaryRotationMatrix(7, 30.0), mat4Identity(), 1e-6f));
    }
}

}  // namespace

int runTransformSelfTests(TransformSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testDefaults(r);
    testUpdateSemantics(r);
    testRejection(r);
    testTranslation(r);
    testRightHandedAxisRotations(r);
    testEulerOrder(r);
    testModelInversePair(r);
    testWorldRayToLocal(r);
    testTransformedBoxHit(r);
    testTransformDoesNotTouchMesh(r);
    testApplyEntryPoint(r);
    testScaleDomain(r);
    testScaledMatrices(r);
    testEulerDecomposition(r);
    return r.n;
}

}  // namespace forgeshape
