#include "forgeshape_primitive_selftest.h"

#include <cmath>
#include <limits>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_scene.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    PrimitiveSelfTestResult* out;
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

// The generated positions are float, so a hit coordinate carries one float
// rounding of the half-extent plus the ray arithmetic. 1e-5 absolute is two
// orders of magnitude above that floor at these sizes and far tighter than any
// real topology or winding error, which would be off by a whole extent.
constexpr float kEpsilon = 1e-5f;

bool nearly(float a, float b) { return std::fabs(a - b) <= kEpsilon; }

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

struct Bounds {
    float minAxis[3];
    float maxAxis[3];
};

bool boundsOf(const ConstructionMesh& mesh, Bounds* out) {
    if (mesh.vertices.empty()) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        out->minAxis[axis] = std::numeric_limits<float>::infinity();
        out->maxAxis[axis] = -std::numeric_limits<float>::infinity();
    }
    for (const MeshVertex& v : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            const float p = v.position[axis];
            if (!std::isfinite(p)) {
                return false;
            }
            if (p < out->minAxis[axis]) out->minAxis[axis] = p;
            if (p > out->maxAxis[axis]) out->maxAxis[axis] = p;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The one active object
// ---------------------------------------------------------------------------

void testActiveObjectDefaults(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    r.check("active_object_defaults_to_box", object.kind() == PrimitiveKind::Box);
    r.check("active_object_keeps_box_defaults",
            object.box().widthMeters() == kDefaultBoxWidthMeters &&
                object.box().heightMeters() == kDefaultBoxHeightMeters &&
                object.box().depthMeters() == kDefaultBoxDepthMeters);
    r.check("active_object_carries_cylinder_defaults",
            object.cylinder().diameterMeters() == kDefaultCylinderDiameterMeters &&
                object.cylinder().heightMeters() == kDefaultCylinderHeightMeters);
    r.check("active_object_id_is_stable_value",
            object.objectId() == kConstructionBoxObjectId && object.objectId() != kNoObject);
    r.check("active_object_counts_start_at_zero",
            object.updateCount() == 0 && object.rejectedUpdateCount() == 0);
    r.check("active_object_transform_starts_identity", placement_.isIdentity());
    r.check("active_object_default_mesh_is_the_box",
            object.generateMesh().vertices.size() == kBoxVertexCount);

    // The process-scoped transform IS the active BODY's transform, not a second
    // singleton. Since IMPORT-01A it belongs to the body rather than to its
    // Construction Source, which is what lets an Imported Mesh have one too --
    // and it is still exactly one placement per body.
    r.check("process_transform_belongs_to_the_active_body",
            &constructionTransform() == &constructionScene().activeBody().transform());
}

void testIdentityAndTransformSurviveKindChanges(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    publishConstructionObject(object, store);

    const ObjectId idBefore = object.objectId();
    TransformValues placement;
    placement.positionX = 0.75;
    placement.positionY = -1.25;
    placement.positionZ = 0.5;
    placement.rotationX = 15.0;
    placement.rotationY = 30.0;
    placement.rotationZ = 90.0;
    placement_.setValues(placement);
    const uint64_t transformUpdatesBefore = placement_.updateCount();

    const PrimitiveApplyResult toCylinder =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("box_to_cylinder_is_applied", toCylinder.status == PrimitiveUpdateStatus::Applied);
    r.check("kind_is_cylinder_after_switch", object.kind() == PrimitiveKind::Cylinder);
    r.check("object_id_stable_box_to_cylinder", object.objectId() == idBefore);

    const TransformValues after = placement_.values();
    r.check("transform_survives_box_to_cylinder",
            after.positionX == 0.75 && after.positionY == -1.25 && after.positionZ == 0.5 &&
                after.rotationX == 15.0 && after.rotationY == 30.0 && after.rotationZ == 90.0);
    r.check("kind_change_does_not_count_as_a_transform_update",
            placement_.updateCount() == transformUpdatesBefore);

    const PrimitiveApplyResult backToBox =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.5, 3.0, 0.4));
    r.check("cylinder_to_box_is_applied", backToBox.status == PrimitiveUpdateStatus::Applied);
    r.check("kind_is_box_after_switch_back", object.kind() == PrimitiveKind::Box);
    r.check("object_id_stable_cylinder_to_box", object.objectId() == idBefore);
    const TransformValues afterBack = placement_.values();
    r.check("transform_survives_cylinder_to_box",
            afterBack.positionX == 0.75 && afterBack.rotationZ == 90.0);

    // The inactive primitive's parameters are remembered, so a round trip does
    // not silently forget them.
    r.check("cylinder_parameters_remembered_while_box_is_active",
            object.cylinder().diameterMeters() == 1.2 && object.cylinder().heightMeters() == 2.4);

    // Mesh identity follows the object, not the primitive.
    r.check("published_mesh_object_id_stable",
            store.current() != nullptr && store.current()->objectId() == kConstructionBoxObjectId);
}

void testApplySemantics(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);

    // Box -> Cylinder publishes exactly one revision.
    const uint64_t publishedBefore = store.publishedCount();
    const PrimitiveApplyResult toCylinder =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("box_to_cylinder_publishes_exactly_one_revision",
            toCylinder.published && toCylinder.revision == rev0 + 1 &&
                store.publishedCount() == publishedBefore + 1 &&
                store.currentRevision() == toCylinder.revision);
    r.check("box_to_cylinder_reports_cylinder_topology",
            toCylinder.vertexCount == kCylinderVertexCount &&
                toCylinder.indexCount == kCylinderIndexCount);
    r.check("box_to_cylinder_store_holds_cylinder_topology",
            store.current() != nullptr && store.current()->vertexCount() == kCylinderVertexCount &&
                store.current()->indexCount() == kCylinderIndexCount);
    r.check("cylinder_parameters_are_exact_doubles",
            object.cylinder().diameterMeters() == 1.2 && object.cylinder().heightMeters() == 2.4);
    r.check("cylinder_radius_is_derived_not_stored",
            object.cylinder().radiusMeters() == 0.6);

    // Identical request is a no-op.
    const MeshRevision afterSwitch = store.currentRevision();
    const uint64_t publishedAfterSwitch = store.publishedCount();
    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("identical_cylinder_is_unchanged", repeat.status == PrimitiveUpdateStatus::Unchanged);
    r.check("identical_cylinder_publishes_nothing",
            !repeat.published && store.currentRevision() == afterSwitch &&
                store.publishedCount() == publishedAfterSwitch);
    r.check("identical_cylinder_does_not_count_as_update", object.updateCount() == 1);

    // A parameter change on the active cylinder publishes exactly one revision.
    const PrimitiveApplyResult resized =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(0.8, 3.5));
    r.check("cylinder_parameter_change_publishes_one_revision",
            resized.status == PrimitiveUpdateStatus::Applied && resized.published &&
                resized.revision == afterSwitch + 1 &&
                store.publishedCount() == publishedAfterSwitch + 1);
    r.check("cylinder_parameter_change_keeps_topology",
            resized.vertexCount == kCylinderVertexCount &&
                resized.indexCount == kCylinderIndexCount);

    // A kind change is a change even when the target already holds these values.
    applyPrimitive(object, store, PrimitiveSpec::forBox(2.0, 1.0, 0.5));
    const MeshRevision beforeReswitch = store.currentRevision();
    const PrimitiveApplyResult reswitch =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(0.8, 3.5));
    r.check("kind_change_with_identical_parameters_is_applied",
            reswitch.status == PrimitiveUpdateStatus::Applied && reswitch.published &&
                reswitch.revision == beforeReswitch + 1);
}

void testRejection(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        PrimitiveSpec spec;
        DimensionValidation expected;
    };
    const Case cases[] = {
        {"reject_zero_diameter", PrimitiveSpec::forCylinder(0.0, 2.0),
         DimensionValidation::NotPositive},
        {"reject_negative_diameter", PrimitiveSpec::forCylinder(-1.0, 2.0),
         DimensionValidation::NotPositive},
        {"reject_zero_cylinder_height", PrimitiveSpec::forCylinder(1.0, 0.0),
         DimensionValidation::NotPositive},
        {"reject_negative_cylinder_height", PrimitiveSpec::forCylinder(1.0, -2.0),
         DimensionValidation::NotPositive},
        {"reject_nan_diameter", PrimitiveSpec::forCylinder(nan, 2.0),
         DimensionValidation::NotFinite},
        {"reject_inf_cylinder_height", PrimitiveSpec::forCylinder(1.0, inf),
         DimensionValidation::NotFinite},
        {"reject_unrepresentable_diameter", PrimitiveSpec::forCylinder(1e40, 2.0),
         DimensionValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        ConstructionObject object;
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here. The
        // invariant under test is unchanged: a primitive change must not disturb it.
        ConstructionTransform placement_;
        MeshStore store(kConstructionBoxObjectId);
        // Start from a known non-default cylinder so a rejection has something
        // real to preserve.
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
        TransformValues placement;
        placement.positionY = -1.25;
        placement.rotationZ = 90.0;
        placement_.setValues(placement);

        const MeshRevision before = store.currentRevision();
        const uint64_t publishedBefore = store.publishedCount();
        const PrimitiveApplyResult rejected = applyPrimitive(object, store, c.spec);

        const bool refused = rejected.status == PrimitiveUpdateStatus::Rejected &&
                             rejected.validation == c.expected;
        const bool kindHeld = object.kind() == PrimitiveKind::Cylinder;
        const bool parametersHeld = object.cylinder().diameterMeters() == 1.2 &&
                                    object.cylinder().heightMeters() == 2.4;
        const bool revisionHeld = store.currentRevision() == before &&
                                  store.publishedCount() == publishedBefore && !rejected.published;
        const TransformValues t = placement_.values();
        const bool transformHeld = t.positionY == -1.25 && t.rotationZ == 90.0;
        r.check(c.name, refused && kindHeld && parametersHeld && revisionHeld && transformHeld);
    }

    // A rejected BOX request must not switch the kind either.
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    DimensionValidation why = DimensionValidation::Ok;
    const PrimitiveUpdateStatus status =
        object.setPrimitive(PrimitiveSpec::forBox(2.0, 1.0, -0.5), &why);
    r.check("rejected_kind_change_does_not_switch_kind",
            status == PrimitiveUpdateStatus::Rejected &&
                why == DimensionValidation::NotPositive &&
                object.kind() == PrimitiveKind::Cylinder);
    r.check("rejection_counted_on_the_object",
            object.rejectedUpdateCount() == 1 && object.updateCount() == 1);
}

// ---------------------------------------------------------------------------
// Cylinder geometry
// ---------------------------------------------------------------------------

void testCylinderTopology(Recorder& r) {
    ConstructionCylinder cylinder;
    cylinder.setDimensionsMeters(1.2, 2.4);
    const ConstructionMesh mesh = cylinder.generateMesh();
    const uint32_t n = kCylinderRadialSegments;

    r.check("cylinder_vertex_count_is_2n_plus_2",
            mesh.vertices.size() == kCylinderVertexCount && kCylinderVertexCount == 2 * n + 2);
    r.check("cylinder_index_count_is_12n",
            mesh.indices.size() == kCylinderIndexCount && kCylinderIndexCount == 12 * n);
    r.check("cylinder_triangle_count_is_4n", (mesh.indices.size() / 3) == 4 * n);

    bool finitePositions = true;
    for (const MeshVertex& v : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(v.position[axis])) finitePositions = false;
        }
    }
    r.check("cylinder_positions_all_finite", finitePositions);

    bool indicesInRange = true;
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) indicesInRange = false;
    }
    r.check("cylinder_indices_in_range", indicesInRange);

    // Every vertex must be referenced, or the surface has a hole.
    std::vector<uint32_t> uses(mesh.vertices.size(), 0);
    for (uint32_t i : mesh.indices) {
        if (i < uses.size()) ++uses[i];
    }
    bool allUsed = true;
    for (uint32_t count : uses) {
        if (count == 0) allUsed = false;
    }
    r.check("cylinder_uses_every_vertex", allUsed);

    // Cap sanity: each cap centre anchors exactly one fan triangle per segment,
    // which is what makes both caps closed.
    r.check("cylinder_bottom_cap_is_a_closed_fan", uses[2 * n] == n);
    r.check("cylinder_top_cap_is_a_closed_fan", uses[2 * n + 1] == n);
    // Every ring vertex is referenced exactly 5 times: 3 by the two side
    // triangles either side of it (it is one corner of one segment's quad and
    // the opposite corner of its neighbour's), and 2 by the cap fan. That
    // accounts for the whole index buffer: 2N ring vertices x 5 plus 2 centres
    // x N is 12N, which is exactly kCylinderIndexCount.
    bool ringSharing = true;
    for (uint32_t i = 0; i < 2 * n; ++i) {
        if (uses[i] != 5) ringSharing = false;
    }
    r.check("cylinder_ring_vertices_are_shared_by_sides_and_caps", ringSharing);
    uint32_t totalUses = 0;
    for (uint32_t count : uses) {
        totalUses += count;
    }
    r.check("cylinder_vertex_uses_account_for_every_index",
            totalUses == kCylinderIndexCount && totalUses == 2 * n * 5 + 2 * n);

    r.check("cylinder_mesh_passes_runtime_validation",
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(),
                             static_cast<uint32_t>(mesh.indices.size())) == MeshValidation::Ok);
}

void testCylinderBoundsAndWinding(Recorder& r) {
    struct Case {
        const char* boundsName;
        const char* windingName;
        Meters diameter;
        Meters height;
    };
    const Case cases[] = {
        {"cylinder_bounds_default", "cylinder_winding_default", kDefaultCylinderDiameterMeters,
         kDefaultCylinderHeightMeters},
        {"cylinder_bounds_1_2x2_4", "cylinder_winding_1_2x2_4", 1.2, 2.4},
        {"cylinder_bounds_wide_flat", "cylinder_winding_wide_flat", 5.0, 0.25},
        {"cylinder_bounds_tall_thin", "cylinder_winding_tall_thin", 0.05, 4.0},
    };

    for (const Case& c : cases) {
        ConstructionCylinder cylinder;
        cylinder.setDimensionsMeters(c.diameter, c.height);
        const ConstructionMesh mesh = cylinder.generateMesh();

        Bounds b{};
        const bool ok = boundsOf(mesh, &b);
        // Because the four cardinal directions are written down exactly rather
        // than computed, the X and Z bounds are EXACTLY the radius, not the
        // radius times cos(pi/32).
        const float radius = static_cast<float>(c.diameter * 0.5);
        const float halfY = static_cast<float>(c.height * 0.5);
        r.check(c.boundsName, ok && b.maxAxis[0] == radius && b.minAxis[0] == -radius &&
                                  b.maxAxis[2] == radius && b.minAxis[2] == -radius &&
                                  b.maxAxis[1] == halfY && b.minAxis[1] == -halfY);

        // Canonical winding: every outward normal points away from the centre,
        // which is what makes the sides AND both caps survive back-face culling.
        r.check(c.windingName, meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, 0.0f, 0.0f}));
    }

    // A ray fired from inside must miss under front-face-only picking, exactly
    // as it does for the box: that is the proof the data, the rasterizer and the
    // picker agree about which way the surface faces.
    ConstructionCylinder cylinder;
    cylinder.setDimensionsMeters(1.2, 2.4);
    const ConstructionMesh mesh = cylinder.generateMesh();
    const TriangleHit inside = pickTriangleMesh(
        makeRay(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}), viewOf(mesh), true);
    r.check("cylinder_front_face_only_ray_from_inside_misses", !inside.hit);
    const TriangleHit insideUp = pickTriangleMesh(
        makeRay(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}), viewOf(mesh), true);
    r.check("cylinder_front_face_only_ray_from_inside_misses_cap", !insideUp.hit);
}

void testCylinderPicking(Recorder& r) {
    // Diameter 1.2 m (radius 0.6), height 2.4 m (half 1.2).
    ConstructionCylinder cylinder;
    cylinder.setDimensionsMeters(1.2, 2.4);
    const ConstructionMesh mesh = cylinder.generateMesh();
    const TriangleMeshView view = viewOf(mesh);

    // Side hit along a cardinal direction: exactly the radius.
    const TriangleHit side =
        pickTriangleMesh(makeRay(Vec3{10.0f, 0.0f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("cylinder_side_ray_hits_exact_radius",
            side.hit && nearly(side.position.x, 0.6f) && nearly(side.position.y, 0.0f));
    const TriangleHit sideZ =
        pickTriangleMesh(makeRay(Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, 0.0f, -1.0f}), view, true);
    r.check("cylinder_side_ray_hits_exact_radius_on_z",
            sideZ.hit && nearly(sideZ.position.z, 0.6f));

    // Cap hit: exactly half the height.
    const TriangleHit top =
        pickTriangleMesh(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("cylinder_top_cap_ray_hits_half_height",
            top.hit && nearly(top.position.y, 1.2f));
    const TriangleHit bottom =
        pickTriangleMesh(makeRay(Vec3{0.0f, -10.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}), view, true);
    r.check("cylinder_bottom_cap_ray_hits_negative_half_height",
            bottom.hit && nearly(bottom.position.y, -1.2f));

    // Just outside the radius must MISS: a ray at x = 0.7 passes by a 0.6 m
    // radius cylinder, and would only hit if the shape were really a box.
    const TriangleHit beside =
        pickTriangleMesh(makeRay(Vec3{0.7f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("cylinder_ray_outside_radius_misses", !beside.hit);
    // ...and a ray inside the radius but past the top still hits the cap.
    const TriangleHit within =
        pickTriangleMesh(makeRay(Vec3{0.5f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("cylinder_ray_inside_radius_hits_cap",
            within.hit && nearly(within.position.y, 1.2f));
    // A corner a box would occupy is empty on a cylinder.
    const TriangleHit corner = pickTriangleMesh(
        makeRay(Vec3{0.5f, 10.0f, 0.5f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("cylinder_is_round_not_square", !corner.hit);

    // The same cylinder under a transform: the ray goes to local space and the
    // hit comes back to world space.
    ConstructionTransform transform;
    TransformValues placement;
    placement.positionX = 5.0;
    placement.positionY = 0.5;
    transform.setValues(placement);
    Ray local{};
    const bool moved = transformRayToLocal(
        makeRay(Vec3{5.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), transform.inverseModelMatrix(),
        &local);
    const TriangleHit transformed = pickTriangleMesh(local, view, true);
    const Vec3 world = mat4TransformPoint(transform.modelMatrix(), transformed.position);
    r.check("transformed_cylinder_is_hit", moved && transformed.hit);
    r.check("transformed_cylinder_hit_is_at_moved_cap",
            transformed.hit && nearly(world.x, 5.0f) && nearly(world.y, 1.7f));

    // Rotated 90 degrees about +Z the cylinder lies on its side, so a ray
    // straight down now meets the SIDE at the radius, not the cap.
    ConstructionTransform rotated;
    TransformValues laid;
    laid.rotationZ = 90.0;
    rotated.setValues(laid);
    Ray localRotated{};
    const bool ok = transformRayToLocal(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                                        rotated.inverseModelMatrix(), &localRotated);
    const TriangleHit onSide = pickTriangleMesh(localRotated, view, true);
    const Vec3 worldSide = mat4TransformPoint(rotated.modelMatrix(), onSide.position);
    r.check("rotated_cylinder_presents_its_side",
            ok && onSide.hit && nearly(worldSide.y, 0.6f));
}

// ---------------------------------------------------------------------------
// Plane geometry (Stage 016)
// ---------------------------------------------------------------------------

// PLN-01: exactly 4 source vertices, 6 indices (2 triangles), independent of
// the requested dimensions, with no duplicate vertex, no degenerate triangle
// and every value finite.
void testPlaneTopology(Recorder& r) {
    ConstructionPlane plane;
    plane.setDimensionsMeters(3.0, 0.4);
    const ConstructionMesh mesh = plane.generateMesh();

    r.check("plane_vertex_count_is_exactly_4",
            mesh.vertices.size() == 4 && kPlaneVertexCount == 4);
    r.check("plane_index_count_is_exactly_6",
            mesh.indices.size() == 6 && kPlaneIndexCount == 6);
    r.check("plane_triangle_count_is_exactly_2", (mesh.indices.size() / 3) == 2);

    bool finitePositions = true;
    for (const MeshVertex& v : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(v.position[axis])) finitePositions = false;
        }
    }
    r.check("plane_positions_all_finite", finitePositions);

    bool indicesInRange = true;
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) indicesInRange = false;
    }
    r.check("plane_indices_in_range", indicesInRange);

    // No duplicate source vertices: all 4 corners are distinct positions.
    bool noDuplicates = true;
    for (uint32_t i = 0; i < mesh.vertices.size(); ++i) {
        for (uint32_t j = i + 1; j < mesh.vertices.size(); ++j) {
            const MeshVertex& a = mesh.vertices[i];
            const MeshVertex& b = mesh.vertices[j];
            if (a.position[0] == b.position[0] && a.position[1] == b.position[1] &&
                a.position[2] == b.position[2]) {
                noDuplicates = false;
            }
        }
    }
    r.check("plane_no_duplicate_source_vertices", noDuplicates);

    // No degenerate triangle: each triangle's area is strictly positive.
    bool noDegenerate = true;
    for (uint32_t tri = 0; tri < mesh.indices.size() / 3; ++tri) {
        const MeshVertex& a = mesh.vertices[mesh.indices[tri * 3 + 0]];
        const MeshVertex& b = mesh.vertices[mesh.indices[tri * 3 + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[tri * 3 + 2]];
        const float ux = b.position[0] - a.position[0], uy = b.position[1] - a.position[1],
                    uz = b.position[2] - a.position[2];
        const float vx = c.position[0] - a.position[0], vy = c.position[1] - a.position[1],
                    vz = c.position[2] - a.position[2];
        const float cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
        if ((cx * cx + cy * cy + cz * cz) <= 0.0f) noDegenerate = false;
    }
    r.check("plane_no_degenerate_triangle", noDegenerate);

    // Every vertex is referenced, so the two triangles really do share a
    // diagonal rather than leaving a corner unused.
    std::vector<uint32_t> uses(mesh.vertices.size(), 0);
    for (uint32_t i : mesh.indices) {
        if (i < uses.size()) ++uses[i];
    }
    bool allUsed = true;
    for (uint32_t count : uses) {
        if (count == 0) allUsed = false;
    }
    r.check("plane_uses_every_vertex", allUsed);

    r.check("plane_mesh_passes_runtime_validation",
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(),
                             static_cast<uint32_t>(mesh.indices.size())) == MeshValidation::Ok);

    // PLN-topology-independent-of-dimensions: a very different width/depth
    // still produces exactly 4:6.
    ConstructionPlane other;
    other.setDimensionsMeters(0.02, 40.0);
    const ConstructionMesh otherMesh = other.generateMesh();
    r.check("plane_topology_independent_of_dimensions",
            otherMesh.vertices.size() == 4 && otherMesh.indices.size() == 6);
}

// PLN-02/03: exact X/Z bounds, Y always exactly 0, CCW winding and an outward
// +Y normal seen from the canonical front, across several representative
// sizes.
void testPlaneBoundsAndWinding(Recorder& r) {
    struct Case {
        const char* boundsName;
        const char* windingName;
        const char* normalName;
        Meters width;
        Meters depth;
    };
    const Case cases[] = {
        {"plane_bounds_default", "plane_winding_default", "plane_normal_default",
         kDefaultPlaneWidthMeters, kDefaultPlaneDepthMeters},
        {"plane_bounds_3x0_4", "plane_winding_3x0_4", "plane_normal_3x0_4", 3.0, 0.4},
        {"plane_bounds_wide_flat", "plane_winding_wide_flat", "plane_normal_wide_flat", 8.0, 0.1},
        {"plane_bounds_narrow_tall", "plane_winding_narrow_tall", "plane_normal_narrow_tall", 0.05,
         6.0},
    };

    for (const Case& c : cases) {
        ConstructionPlane plane;
        plane.setDimensionsMeters(c.width, c.depth);
        const ConstructionMesh mesh = plane.generateMesh();

        Bounds b{};
        const bool ok = boundsOf(mesh, &b);
        const float halfX = static_cast<float>(c.width * 0.5);
        const float halfZ = static_cast<float>(c.depth * 0.5);
        r.check(c.boundsName, ok && b.maxAxis[0] == halfX && b.minAxis[0] == -halfX &&
                                  b.maxAxis[2] == halfZ && b.minAxis[2] == -halfZ &&
                                  b.maxAxis[1] == 0.0f && b.minAxis[1] == 0.0f);

        // A point strictly below the plane (negative Y) is "outside" it, so
        // canonical winding here means every triangle's normal points toward
        // +Y, exactly what the canonical front (+Y) contract requires.
        r.check(c.windingName,
                meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, -1.0f, 0.0f}));

        // The geometric normal of both triangles is exactly +Y (not merely
        // outward-ish): the plane is flat, so there is exactly one direction
        // to get right.
        const TriangleMeshView view = viewOf(mesh);
        bool bothNormalsExactlyPlusY = true;
        for (uint32_t tri = 0; tri < view.indexCount / 3; ++tri) {
            const uint32_t i0 = view.indices[tri * 3 + 0];
            const uint32_t i1 = view.indices[tri * 3 + 1];
            const uint32_t i2 = view.indices[tri * 3 + 2];
            const MeshVertex& v0 = mesh.vertices[i0];
            const MeshVertex& v1 = mesh.vertices[i1];
            const MeshVertex& v2 = mesh.vertices[i2];
            const float ux = v1.position[0] - v0.position[0], uz = v1.position[2] - v0.position[2];
            const float vx = v2.position[0] - v0.position[0], vz = v2.position[2] - v0.position[2];
            // Cross product's Y component for two vectors lying in the XZ
            // plane: (u x v).y = uz*vx - ux*vz.
            const float ny = uz * vx - ux * vz;
            if (!(ny > 0.0f)) bothNormalsExactlyPlusY = false;
        }
        r.check(c.normalName, bothNormalsExactlyPlusY);
    }
}

// PLN-04: non-finite, zero and negative width/depth are all rejected, and a
// rejection leaves kind, parameters, transform and revision exactly as they
// were (the same fail-closed contract every primitive has).
void testPlaneRejection(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        PrimitiveSpec spec;
        DimensionValidation expected;
    };
    const Case cases[] = {
        {"reject_zero_plane_width", PrimitiveSpec::forPlane(0.0, 1.0),
         DimensionValidation::NotPositive},
        {"reject_negative_plane_width", PrimitiveSpec::forPlane(-1.0, 1.0),
         DimensionValidation::NotPositive},
        {"reject_zero_plane_depth", PrimitiveSpec::forPlane(1.0, 0.0),
         DimensionValidation::NotPositive},
        {"reject_negative_plane_depth", PrimitiveSpec::forPlane(1.0, -2.0),
         DimensionValidation::NotPositive},
        {"reject_nan_plane_width", PrimitiveSpec::forPlane(nan, 1.0),
         DimensionValidation::NotFinite},
        {"reject_inf_plane_depth", PrimitiveSpec::forPlane(1.0, inf),
         DimensionValidation::NotFinite},
        {"reject_unrepresentable_plane_width", PrimitiveSpec::forPlane(1e40, 1.0),
         DimensionValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        ConstructionObject object;
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here. The
        // invariant under test is unchanged: a primitive change must not disturb it.
        ConstructionTransform placement_;
        MeshStore store(kConstructionBoxObjectId);
        applyPrimitive(object, store, PrimitiveSpec::forPlane(2.5, 1.5));
        TransformValues placement;
        placement.positionY = 0.75;
        placement.rotationX = 90.0;
        placement_.setValues(placement);

        const MeshRevision before = store.currentRevision();
        const uint64_t publishedBefore = store.publishedCount();
        const PrimitiveApplyResult rejected = applyPrimitive(object, store, c.spec);

        const bool refused = rejected.status == PrimitiveUpdateStatus::Rejected &&
                             rejected.validation == c.expected;
        const bool kindHeld = object.kind() == PrimitiveKind::Plane;
        const bool parametersHeld =
            object.plane().widthMeters() == 2.5 && object.plane().depthMeters() == 1.5;
        const bool revisionHeld = store.currentRevision() == before &&
                                  store.publishedCount() == publishedBefore && !rejected.published;
        const TransformValues t = placement_.values();
        const bool transformHeld = t.positionY == 0.75 && t.rotationX == 90.0;
        r.check(c.name, refused && kindHeld && parametersHeld && revisionHeld && transformHeld);
    }
}

// PLN-05/06: an unchanged Apply publishes nothing; a changed Apply publishes
// exactly one revision and preserves ObjectId and transform.
void testPlaneApplySemantics(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    const ObjectId idBefore = object.objectId();

    TransformValues placement;
    placement.positionX = 1.5;
    placement.rotationY = 45.0;
    placement_.setValues(placement);

    const uint64_t publishedBefore = store.publishedCount();
    const PrimitiveApplyResult toPlane =
        applyPrimitive(object, store, PrimitiveSpec::forPlane(2.0, 1.25));
    r.check("box_to_plane_publishes_exactly_one_revision",
            toPlane.published && toPlane.revision == rev0 + 1 &&
                store.publishedCount() == publishedBefore + 1);
    r.check("box_to_plane_reports_plane_topology",
            toPlane.vertexCount == kPlaneVertexCount && toPlane.indexCount == kPlaneIndexCount);
    r.check("box_to_plane_preserves_object_id", object.objectId() == idBefore);
    const TransformValues afterToPlane = placement_.values();
    r.check("box_to_plane_preserves_transform",
            afterToPlane.positionX == 1.5 && afterToPlane.rotationY == 45.0);

    // Identical request is a no-op: same status, no revision, no publish.
    const MeshRevision afterApply = store.currentRevision();
    const uint64_t publishedAfterApply = store.publishedCount();
    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forPlane(2.0, 1.25));
    r.check("identical_plane_is_unchanged", repeat.status == PrimitiveUpdateStatus::Unchanged);
    r.check("identical_plane_publishes_nothing",
            !repeat.published && store.currentRevision() == afterApply &&
                store.publishedCount() == publishedAfterApply);

    // A genuine parameter change publishes exactly one more revision and keeps
    // the fixed 4:6 topology, ObjectId and transform.
    const PrimitiveApplyResult resized =
        applyPrimitive(object, store, PrimitiveSpec::forPlane(5.0, 0.3));
    r.check("plane_parameter_change_publishes_one_revision",
            resized.status == PrimitiveUpdateStatus::Applied && resized.published &&
                resized.revision == afterApply + 1);
    r.check("plane_parameter_change_keeps_topology",
            resized.vertexCount == kPlaneVertexCount && resized.indexCount == kPlaneIndexCount);
    r.check("plane_parameter_change_preserves_object_id", object.objectId() == idBefore);
    const TransformValues afterResize = placement_.values();
    r.check("plane_parameter_change_preserves_transform",
            afterResize.positionX == 1.5 && afterResize.rotationY == 45.0);
}

// PLN-07: all six primitive payloads survive a full round trip of kind
// switches, each remembering its own last-applied values independently.
void testAllSixPrimitivesRoundTrip(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const ObjectId idBefore = object.objectId();

    applyPrimitive(object, store, PrimitiveSpec::forBox(1.1, 2.2, 3.3));
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.5, 2.5));
    applyPrimitive(object, store, PrimitiveSpec::forSphere(0.9));
    applyPrimitive(object, store, PrimitiveSpec::forCone(1.2, 1.8));
    applyPrimitive(object, store, PrimitiveSpec::forCapsule(0.6, 2.4));
    applyPrimitive(object, store, PrimitiveSpec::forPlane(2.0, 1.25));

    r.check("round_trip_kind_ends_on_plane", object.kind() == PrimitiveKind::Plane);
    r.check("round_trip_object_id_stable", object.objectId() == idBefore);
    r.check("round_trip_remembers_box",
            object.box().widthMeters() == 1.1 && object.box().heightMeters() == 2.2 &&
                object.box().depthMeters() == 3.3);
    r.check("round_trip_remembers_cylinder",
            object.cylinder().diameterMeters() == 1.5 && object.cylinder().heightMeters() == 2.5);
    r.check("round_trip_remembers_sphere", object.sphere().diameterMeters() == 0.9);
    r.check("round_trip_remembers_cone",
            object.cone().bottomDiameterMeters() == 1.2 && object.cone().heightMeters() == 1.8);
    r.check("round_trip_remembers_capsule",
            object.capsule().diameterMeters() == 0.6 && object.capsule().totalHeightMeters() == 2.4);
    r.check("round_trip_remembers_plane",
            object.plane().widthMeters() == 2.0 && object.plane().depthMeters() == 1.25);

    // Switching all the way back to Box brings its remembered values back
    // rather than resetting them.
    const PrimitiveApplyResult backToBox =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.1, 2.2, 3.3));
    r.check("round_trip_back_to_box_is_applied",
            backToBox.status == PrimitiveUpdateStatus::Applied);
    r.check("round_trip_back_to_box_object_id_stable", object.objectId() == idBefore);
    r.check("round_trip_plane_still_remembered_while_box_active",
            object.plane().widthMeters() == 2.0 && object.plane().depthMeters() == 1.25);
}

// ---------------------------------------------------------------------------
// Box regression through the new owner
// ---------------------------------------------------------------------------

void testBoxStillWorksThroughTheObject(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    r.check("object_publishes_box_topology",
            store.current() != nullptr && store.current()->vertexCount() == kBoxVertexCount &&
                store.current()->indexCount() == kBoxIndexCount);

    const PrimitiveApplyResult applied =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("box_apply_through_object_is_applied",
            applied.status == PrimitiveUpdateStatus::Applied && applied.published &&
                applied.revision == rev0 + 1);
    r.check("box_apply_through_object_is_exact",
            object.box().widthMeters() == 1.25 && object.box().heightMeters() == 2.5 &&
                object.box().depthMeters() == 0.75);
    r.check("box_apply_through_object_keeps_topology",
            applied.vertexCount == kBoxVertexCount && applied.indexCount == kBoxIndexCount);

    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("box_repeat_through_object_is_unchanged",
            repeat.status == PrimitiveUpdateStatus::Unchanged && !repeat.published &&
                store.currentRevision() == applied.revision);
}

}  // namespace

int runPrimitiveSelfTests(PrimitiveSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testActiveObjectDefaults(r);
    testIdentityAndTransformSurviveKindChanges(r);
    testApplySemantics(r);
    testRejection(r);
    testCylinderTopology(r);
    testCylinderBoundsAndWinding(r);
    testCylinderPicking(r);
    testPlaneTopology(r);
    testPlaneBoundsAndWinding(r);
    testPlaneRejection(r);
    testPlaneApplySemantics(r);
    testAllSixPrimitivesRoundTrip(r);
    testBoxStillWorksThroughTheObject(r);
    return r.n;
}

}  // namespace forgeshape
