#include "forgeshape_construction_selftest.h"

#include <cmath>
#include <limits>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_selection.h"

namespace forgeshape {
namespace {

constexpr int kTestViewportWidth = 1080;
constexpr int kTestViewportHeight = 2400;

struct Recorder {
    ConstructionSelfTestResult* out;
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

// Justified tolerance for "does the generated mesh have the dimension that was
// asked for". The parameters are double meters; the mesh is float. One float
// rounding of the half-extent plus the doubling back to a full extent bounds the
// error at roughly value * 2^-23, so a relative 1e-6 with a 1e-6 absolute floor
// is generous by more than an order of magnitude and still far tighter than any
// real defect would be.
double extentTolerance(double value) { return value * 1e-6 + 1e-6; }

bool nearlyEqual(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

struct Bounds {
    double minAxis[3];
    double maxAxis[3];
};

bool boundsOf(const MeshVertex* vertices, uint32_t count, Bounds* out) {
    if (vertices == nullptr || count == 0) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        out->minAxis[axis] = std::numeric_limits<double>::infinity();
        out->maxAxis[axis] = -std::numeric_limits<double>::infinity();
    }
    for (uint32_t i = 0; i < count; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            const double p = vertices[i].position[axis];
            if (!std::isfinite(p)) {
                return false;
            }
            if (p < out->minAxis[axis]) out->minAxis[axis] = p;
            if (p > out->maxAxis[axis]) out->maxAxis[axis] = p;
        }
    }
    return true;
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

Ray axisRay(const Vec3& origin, const Vec3& direction) {
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;
    return ray;
}

// ---------------------------------------------------------------------------
// Domain state: the parameters ARE the truth
// ---------------------------------------------------------------------------

void testDefaults(Recorder& r) {
    ConstructionBox box;
    r.check("default_width_is_2m", box.widthMeters() == kDefaultBoxWidthMeters);
    r.check("default_height_is_1m", box.heightMeters() == kDefaultBoxHeightMeters);
    r.check("default_depth_is_0_5m", box.depthMeters() == kDefaultBoxDepthMeters);
    r.check("default_is_non_cubic",
            box.widthMeters() != box.heightMeters() && box.heightMeters() != box.depthMeters());
    r.check("default_object_id_is_construction_box",
            box.objectId() == kConstructionBoxObjectId && box.objectId() != kNoObject);
    r.check("default_update_count_zero", box.updateCount() == 0 && box.rejectedUpdateCount() == 0);

    // The authoritative unit is double: a value with no float representation
    // must be retained bit-exactly in domain state.
    ConstructionBox precise;
    const Meters awkward = 3.3330000000000002;
    r.check("double_parameter_retained_exactly",
            precise.setDimensionsMeters(awkward, 1.0, 1.0) == PrimitiveUpdateStatus::Applied &&
                precise.widthMeters() == awkward);
}

// ---------------------------------------------------------------------------
// Generation: dimensions -> mesh
// ---------------------------------------------------------------------------

void testGeneratedCounts(Recorder& r) {
    ConstructionBox box;
    const ConstructionMesh mesh = box.generateMesh();
    r.check("generated_vertex_count_is_8", mesh.vertices.size() == kBoxVertexCount);
    r.check("generated_index_count_is_36", mesh.indices.size() == kBoxIndexCount);
    r.check("generated_triangle_count_is_12", (mesh.indices.size() / 3) == 12);

    bool indicesInRange = true;
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) {
            indicesInRange = false;
        }
    }
    r.check("generated_indices_in_range", indicesInRange);

    // Every one of the 8 corners must be referenced, or the box is not closed.
    bool used[kBoxVertexCount] = {false, false, false, false, false, false, false, false};
    for (uint32_t i : mesh.indices) {
        if (i < kBoxVertexCount) used[i] = true;
    }
    bool allUsed = true;
    for (bool u : used) {
        if (!u) allUsed = false;
    }
    r.check("generated_uses_all_8_corners", allUsed);
}

void testGeneratedExtentsMatchDimensions(Recorder& r) {
    struct Case {
        const char* nameX;
        const char* nameY;
        const char* nameZ;
        const char* nameCenter;
        Meters w;
        Meters h;
        Meters d;
    };
    const Case cases[] = {
        {"default_extent_x", "default_extent_y", "default_extent_z", "default_center_at_origin",
         kDefaultBoxWidthMeters, kDefaultBoxHeightMeters, kDefaultBoxDepthMeters},
        {"stateB_extent_x", "stateB_extent_y", "stateB_extent_z", "stateB_center_at_origin",
         1.25, 2.5, 0.75},
        {"stateC_extent_x", "stateC_extent_y", "stateC_extent_z", "stateC_center_at_origin",
         3.333, 0.42, 1.125},
        {"tiny_extent_x", "tiny_extent_y", "tiny_extent_z", "tiny_center_at_origin",
         0.001, 0.002, 0.004},
        {"large_extent_x", "large_extent_y", "large_extent_z", "large_center_at_origin",
         1000.0, 250.5, 12.75},
    };

    for (const Case& c : cases) {
        ConstructionBox box;
        const bool applied =
            box.setDimensionsMeters(c.w, c.h, c.d, nullptr) != PrimitiveUpdateStatus::Rejected;
        const ConstructionMesh mesh = box.generateMesh();
        Bounds b{};
        const bool ok = applied && boundsOf(mesh.vertices.data(),
                                            static_cast<uint32_t>(mesh.vertices.size()), &b);

        const Meters requested[3] = {c.w, c.h, c.d};
        r.check(c.nameX, ok && nearlyEqual(b.maxAxis[0] - b.minAxis[0], requested[0],
                                           extentTolerance(requested[0])));
        r.check(c.nameY, ok && nearlyEqual(b.maxAxis[1] - b.minAxis[1], requested[1],
                                           extentTolerance(requested[1])));
        r.check(c.nameZ, ok && nearlyEqual(b.maxAxis[2] - b.minAxis[2], requested[2],
                                           extentTolerance(requested[2])));
        // Centred at the origin: min and max are exact negatives on every axis.
        bool centered = ok;
        for (int axis = 0; axis < 3; ++axis) {
            if (b.minAxis[axis] != -b.maxAxis[axis]) {
                centered = false;
            }
        }
        r.check(c.nameCenter, centered);
    }
}

void testAxesAreIndependent(Recorder& r) {
    ConstructionBox box;
    box.setDimensionsMeters(1.25, 2.5, 0.75);
    const ConstructionMesh mesh = box.generateMesh();
    Bounds b{};
    const bool ok = boundsOf(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()), &b);
    const double ex = b.maxAxis[0] - b.minAxis[0];
    const double ey = b.maxAxis[1] - b.minAxis[1];
    const double ez = b.maxAxis[2] - b.minAxis[2];
    r.check("non_cubic_axes_are_distinct", ok && ex != ey && ey != ez && ex != ez);
    // Changing one axis must move only that axis.
    ConstructionBox other;
    other.setDimensionsMeters(1.25, 2.5, 0.75);
    other.setDimensionsMeters(1.25, 2.5, 4.0);
    const ConstructionMesh changed = other.generateMesh();
    Bounds c{};
    const bool ok2 =
        boundsOf(changed.vertices.data(), static_cast<uint32_t>(changed.vertices.size()), &c);
    r.check("single_axis_change_is_isolated",
            ok && ok2 && c.maxAxis[0] == b.maxAxis[0] && c.maxAxis[1] == b.maxAxis[1] &&
                c.maxAxis[2] != b.maxAxis[2]);
}

void testWindingAndValidation(Recorder& r) {
    const Meters cases[][3] = {
        {kDefaultBoxWidthMeters, kDefaultBoxHeightMeters, kDefaultBoxDepthMeters},
        {1.25, 2.5, 0.75},
        {3.333, 0.42, 1.125},
    };
    const char* windingNames[] = {"winding_canonical_default", "winding_canonical_stateB",
                                  "winding_canonical_stateC"};
    const char* validationNames[] = {"runtime_validation_default", "runtime_validation_stateB",
                                     "runtime_validation_stateC"};
    const char* buildNames[] = {"runtime_mesh_builds_default", "runtime_mesh_builds_stateB",
                                "runtime_mesh_builds_stateC"};

    for (int i = 0; i < 3; ++i) {
        ConstructionBox box;
        box.setDimensionsMeters(cases[i][0], cases[i][1], cases[i][2]);
        const ConstructionMesh mesh = box.generateMesh();
        const TriangleMeshView view = viewOf(mesh);
        r.check(windingNames[i], meshObeysCanonicalWinding(view, Vec3{0.0f, 0.0f, 0.0f}));

        const MeshValidation why =
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size()));
        r.check(validationNames[i], why == MeshValidation::Ok);

        MeshValidation buildWhy = MeshValidation::Ok;
        const RuntimeMeshPtr runtime = createRuntimeMesh(
            box.objectId(), 1, mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
            mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size()), &buildWhy);
        r.check(buildNames[i], runtime != nullptr && buildWhy == MeshValidation::Ok &&
                                   runtime->objectId() == kConstructionBoxObjectId &&
                                   runtime->vertexCount() == kBoxVertexCount &&
                                   runtime->indexCount() == kBoxIndexCount);
    }

    // A ray fired from inside the box must miss under front-face-only picking:
    // that is the same proof the demo cube gets, applied to generated geometry.
    ConstructionBox box;
    box.setDimensionsMeters(3.333, 0.42, 1.125);
    const ConstructionMesh mesh = box.generateMesh();
    const TriangleHit inside = pickTriangleMesh(
        axisRay(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}), viewOf(mesh), true);
    r.check("front_face_only_ray_from_inside_misses", !inside.hit);
}

// ---------------------------------------------------------------------------
// Update semantics
// ---------------------------------------------------------------------------

void testUpdateSemantics(Recorder& r) {
    ConstructionBox box;
    MeshStore store(kConstructionBoxObjectId);

    const MeshRevision rev0 = publishConstructionBox(box, store);
    r.check("initial_publish_succeeds", rev0 != kNoMeshRevision && store.currentRevision() == rev0);

    const ObjectId idBefore = box.objectId();
    const PrimitiveUpdateStatus applied = box.setDimensionsMeters(1.25, 2.5, 0.75);
    const MeshRevision rev1 = publishConstructionBox(box, store);
    r.check("valid_change_is_applied", applied == PrimitiveUpdateStatus::Applied);
    r.check("valid_change_publishes_new_revision", rev1 != kNoMeshRevision && rev1 > rev0 &&
                                                       store.currentRevision() == rev1);
    r.check("object_id_stable_across_change", box.objectId() == idBefore &&
                                                  box.objectId() == kConstructionBoxObjectId);
    r.check("published_mesh_carries_object_id",
            store.current() != nullptr && store.current()->objectId() == kConstructionBoxObjectId);

    // Same-topology: the published counts must not move, which is what lets the
    // GPU capacity be reused.
    r.check("dimension_change_keeps_topology",
            store.current() != nullptr && store.current()->vertexCount() == kBoxVertexCount &&
                store.current()->indexCount() == kBoxIndexCount);

    // Identical dimensions are a no-op: no parameter change, no new revision.
    const uint64_t updatesBefore = box.updateCount();
    const PrimitiveUpdateStatus repeat = box.setDimensionsMeters(1.25, 2.5, 0.75);
    r.check("identical_dimensions_are_unchanged", repeat == PrimitiveUpdateStatus::Unchanged);
    r.check("identical_dimensions_do_not_count_as_update",
            box.updateCount() == updatesBefore);
    r.check("identical_dimensions_leave_revision_alone", store.currentRevision() == rev1);

    // A third distinct state, to prove the path is not single-shot.
    const PrimitiveUpdateStatus third = box.setDimensionsMeters(3.333, 0.42, 1.125);
    const MeshRevision rev2 = publishConstructionBox(box, store);
    r.check("second_valid_change_is_applied", third == PrimitiveUpdateStatus::Applied);
    r.check("second_valid_change_publishes_newer_revision", rev2 > rev1);
    r.check("object_id_stable_across_all_changes", box.objectId() == kConstructionBoxObjectId);
    r.check("update_count_tracks_real_changes", box.updateCount() == 2);
}

void testRejection(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        Meters w;
        Meters h;
        Meters d;
        DimensionValidation expected;
    };
    const Case cases[] = {
        {"reject_zero_width", 0.0, 1.0, 0.5, DimensionValidation::NotPositive},
        {"reject_zero_height", 1.0, 0.0, 0.5, DimensionValidation::NotPositive},
        {"reject_zero_depth", 1.0, 1.0, 0.0, DimensionValidation::NotPositive},
        {"reject_negative_width", -2.0, 1.0, 0.5, DimensionValidation::NotPositive},
        {"reject_negative_height", 2.0, -1.0, 0.5, DimensionValidation::NotPositive},
        {"reject_negative_depth", 2.0, 1.0, -0.5, DimensionValidation::NotPositive},
        {"reject_nan_width", nan, 1.0, 0.5, DimensionValidation::NotFinite},
        {"reject_nan_depth", 2.0, 1.0, nan, DimensionValidation::NotFinite},
        {"reject_inf_width", inf, 1.0, 0.5, DimensionValidation::NotFinite},
        {"reject_negative_inf_height", 2.0, -inf, 0.5, DimensionValidation::NotFinite},
        // Finite and positive as a double, but the derived float half-extent is
        // either infinite or flushes to zero.
        {"reject_beyond_float_range", 1e40, 1.0, 0.5, DimensionValidation::NotRepresentable},
        {"reject_below_float_range", 1e-60, 1.0, 0.5, DimensionValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        ConstructionBox box;
        MeshStore store(kConstructionBoxObjectId);
        const MeshRevision before = publishConstructionBox(box, store);
        const BoxDimensionsMeters dimensionsBefore = box.dimensionsMeters();

        DimensionValidation why = DimensionValidation::Ok;
        const PrimitiveUpdateStatus status = box.setDimensionsMeters(c.w, c.h, c.d, &why);

        const bool rejected = status == PrimitiveUpdateStatus::Rejected && why == c.expected;
        const bool preserved = box.widthMeters() == dimensionsBefore.width &&
                               box.heightMeters() == dimensionsBefore.height &&
                               box.depthMeters() == dimensionsBefore.depth;
        // Nothing partial: the store still holds exactly the revision it had.
        const bool revisionHeld = store.currentRevision() == before;
        r.check(c.name, rejected && preserved && revisionHeld);
    }

    // A rejection must not half-apply the dimensions that were valid.
    ConstructionBox box;
    box.setDimensionsMeters(1.25, 2.5, 0.75);
    DimensionValidation why = DimensionValidation::Ok;
    const PrimitiveUpdateStatus status = box.setDimensionsMeters(9.0, 9.0, -1.0, &why);
    r.check("rejection_does_not_partially_apply",
            status == PrimitiveUpdateStatus::Rejected && why == DimensionValidation::NotPositive &&
                box.widthMeters() == 1.25 && box.heightMeters() == 2.5 &&
                box.depthMeters() == 0.75);
    r.check("rejection_counted", box.rejectedUpdateCount() == 1 && box.updateCount() == 1);

    // The mesh generated after a rejection is still the previous valid one.
    const ConstructionMesh mesh = box.generateMesh();
    Bounds b{};
    const bool ok = boundsOf(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()), &b);
    r.check("mesh_after_rejection_is_previous_valid",
            ok && nearlyEqual(b.maxAxis[0] - b.minAxis[0], 1.25, extentTolerance(1.25)) &&
                nearlyEqual(b.maxAxis[2] - b.minAxis[2], 0.75, extentTolerance(0.75)));

    // A rejected value must never be validated as usable on its own either.
    r.check("validate_rejects_zero",
            validateDimensionMeters(0.0) == DimensionValidation::NotPositive);
    r.check("validate_accepts_typical",
            validateDimensionMeters(0.5) == DimensionValidation::Ok);
}

// ---------------------------------------------------------------------------
// The one apply entry point: update AND publish, or neither
// ---------------------------------------------------------------------------

void testApplyEntryPoint(Recorder& r) {
    // Driven through the active object, which is what the product uses since
    // Stage 010; the box sub-object is read back to prove the parameters landed.
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    const ConstructionBox& box = object.box();

    // Applied: parameters move and exactly one revision is published.
    const PrimitiveApplyResult applied = applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("apply_valid_reports_applied", applied.status == PrimitiveUpdateStatus::Applied);
    r.check("apply_valid_publishes_once",
            applied.published && applied.revision == rev0 + 1 &&
                store.currentRevision() == applied.revision && store.publishedCount() == 2);
    r.check("apply_valid_reports_new_dimensions",
            applied.spec.box() != nullptr && applied.spec.box()->width == 1.25 &&
                applied.spec.box()->height == 2.5 && applied.spec.box()->depth == 0.75);
    r.check("apply_valid_matches_box_state",
            box.widthMeters() == 1.25 && box.heightMeters() == 2.5 && box.depthMeters() == 0.75);
    r.check("apply_valid_keeps_topology",
            store.current() != nullptr && store.current()->vertexCount() == kBoxVertexCount &&
                store.current()->indexCount() == kBoxIndexCount);
    r.check("apply_valid_keeps_object_id",
            store.current() != nullptr && store.current()->objectId() == kConstructionBoxObjectId);

    // Unchanged: identical values publish nothing at all.
    const uint64_t publishedAfterApply = store.publishedCount();
    const PrimitiveApplyResult unchanged = applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("apply_identical_reports_unchanged", unchanged.status == PrimitiveUpdateStatus::Unchanged);
    r.check("apply_identical_publishes_nothing",
            !unchanged.published && store.publishedCount() == publishedAfterApply &&
                store.currentRevision() == applied.revision &&
                unchanged.revision == applied.revision);
    r.check("apply_identical_does_not_count_as_update", box.updateCount() == 1);

    // Rejected: nothing is written and nothing is published, and the reason is
    // reported rather than repaired.
    const PrimitiveApplyResult rejected = applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, -0.75));
    r.check("apply_invalid_reports_rejected", rejected.status == PrimitiveUpdateStatus::Rejected &&
                                                  rejected.validation ==
                                                      DimensionValidation::NotPositive);
    r.check("apply_invalid_publishes_nothing",
            !rejected.published && store.publishedCount() == publishedAfterApply &&
                store.currentRevision() == applied.revision &&
                rejected.revision == applied.revision);
    r.check("apply_invalid_preserves_parameters",
            rejected.spec.box() != nullptr && rejected.spec.box()->width == 1.25 &&
                rejected.spec.box()->height == 2.5 && rejected.spec.box()->depth == 0.75 &&
                box.depthMeters() == 0.75);
    r.check("apply_invalid_does_not_partially_apply", box.updateCount() == 1);

    // A blank/NaN request arrives from the UI as a non-finite double.
    const PrimitiveApplyResult notFinite = applyPrimitive(
        object, store,
        PrimitiveSpec::forBox(std::numeric_limits<double>::quiet_NaN(), 2.5, 0.75));
    r.check("apply_nan_reports_not_finite",
            notFinite.status == PrimitiveUpdateStatus::Rejected &&
                notFinite.validation == DimensionValidation::NotFinite &&
                store.currentRevision() == applied.revision);

    // The exact decimal states the mm/cm/m UI produces must survive as doubles.
    const PrimitiveApplyResult decimals = applyPrimitive(object, store, PrimitiveSpec::forBox(3.333, 0.42, 1.125));
    r.check("apply_decimal_state_exact",
            decimals.status == PrimitiveUpdateStatus::Applied && decimals.published &&
                box.widthMeters() == 3.333 && box.heightMeters() == 0.42 &&
                box.depthMeters() == 1.125);
    // 125 cm / 250 cm / 75 cm is the same state as 1.25 / 2.5 / 0.75 m: the unit
    // a value was typed in must never survive into the domain.
    const PrimitiveApplyResult fromCentimeters = applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("apply_unit_independent_state",
            fromCentimeters.status == PrimitiveUpdateStatus::Applied &&
                box.widthMeters() == 1.25 && box.heightMeters() == 2.5 &&
                box.depthMeters() == 0.75);
    const PrimitiveApplyResult repeatFromMeters = applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("apply_same_state_from_any_unit_is_unchanged",
            repeatFromMeters.status == PrimitiveUpdateStatus::Unchanged && !repeatFromMeters.published);
}

// ---------------------------------------------------------------------------
// Picking coherence: the picker must see the GENERATED box, not a fixture
// ---------------------------------------------------------------------------

void testPickingUsesGeneratedBox(Recorder& r) {
    // Dimensions no debug fixture can produce: the baseline cube is 1.8 across,
    // the deformed fixture 3.33 and the spherified box 2.56 in diameter, so a
    // 5.5 m width and a 0.42 m height are unmistakably the Construction box.
    ConstructionBox box;
    box.setDimensionsMeters(5.5, 0.42, 1.125);

    MeshStore& store = meshStore();
    const MeshRevision before = store.currentRevision();
    MeshValidation why = MeshValidation::Ok;
    const MeshRevision published = publishConstructionBox(box, store, &why);
    r.check("box_publishes_into_process_store",
            published != kNoMeshRevision && published > before && why == MeshValidation::Ok);

    const RuntimeMeshPtr current = store.current();
    r.check("process_store_holds_box_topology",
            current != nullptr && current->vertexCount() == kBoxVertexCount &&
                current->indexCount() == kBoxIndexCount);

    Bounds b{};
    const bool ok = current != nullptr &&
                    boundsOf(current->vertices(), current->vertexCount(), &b);
    r.check("process_store_holds_box_extents",
            ok && nearlyEqual(b.maxAxis[0] - b.minAxis[0], 5.5, extentTolerance(5.5)) &&
                nearlyEqual(b.maxAxis[1] - b.minAxis[1], 0.42, extentTolerance(0.42)) &&
                nearlyEqual(b.maxAxis[2] - b.minAxis[2], 1.125, extentTolerance(1.125)));

    // Axis rays hit exactly at the generated half-extents. This is the numeric
    // proof that picking geometry follows the authoritative dimensions.
    const TriangleMeshView view =
        current != nullptr ? current->triangleView() : TriangleMeshView{};
    const TriangleHit hitX =
        pickTriangleMesh(axisRay(Vec3{10.0f, 0.0f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("pick_ray_hits_generated_x_half_extent",
            hitX.hit && nearlyEqual(hitX.position.x, 2.75, extentTolerance(5.5)));
    const TriangleHit hitY =
        pickTriangleMesh(axisRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("pick_ray_hits_generated_y_half_extent",
            hitY.hit && nearlyEqual(hitY.position.y, 0.21, extentTolerance(0.42)));
    const TriangleHit hitZ =
        pickTriangleMesh(axisRay(Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, 0.0f, -1.0f}), view, true);
    r.check("pick_ray_hits_generated_z_half_extent",
            hitZ.hit && nearlyEqual(hitZ.position.z, 0.5625, extentTolerance(1.125)));

    // Nothing of the baseline debug fixture survives in what the picker sees.
    r.check("pick_geometry_is_not_debug_fixture",
            hitX.hit && std::fabs(hitX.position.x - 0.9) > 0.5 &&
                std::fabs(hitX.position.x - 1.665) > 0.5);

    // And the full scene path — camera ray, picking, object identity — resolves
    // to the Construction box's stable id. Picks with an explicit identity
    // transform rather than the implicit process-global ConstructionTransform:
    // this self-test suite can rerun within one process after Activity
    // recreation, by which point that global transform may no longer be
    // identity, and this test's "on the generated surface" bounds are only
    // valid in the local space the box above was built in.
    CameraController camera;
    camera.setViewport(kTestViewportWidth, kTestViewportHeight);
    const Mat4 identity = mat4Identity();
    const SceneHit scene =
        pickScene(camera.snapshot(), kTestViewportWidth * 0.5f, kTestViewportHeight * 0.5f,
                  kTestViewportWidth, kTestViewportHeight, identity, identity, true);
    r.check("pick_scene_hits_construction_box",
            scene.hit && scene.objectId == kConstructionBoxObjectId);
    // The reported hit point must lie on the generated surface.
    const bool onSurface =
        scene.hit && std::fabs(scene.position.x) <= 2.75 + 1e-3f &&
        std::fabs(scene.position.y) <= 0.21 + 1e-3f && std::fabs(scene.position.z) <= 0.5625 + 1e-3f;
    r.check("pick_scene_hit_lies_on_generated_box", onSurface);
}

}  // namespace

int runConstructionSelfTests(ConstructionSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testDefaults(r);
    testGeneratedCounts(r);
    testGeneratedExtentsMatchDimensions(r);
    testAxesAreIndependent(r);
    testWindingAndValidation(r);
    testUpdateSemantics(r);
    testRejection(r);
    testApplyEntryPoint(r);
    testPickingUsesGeneratedBox(r);
    return r.n;
}

}  // namespace forgeshape
