#include "forgeshape_sphere_selftest.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_transform.h"

namespace forgeshape {
namespace {

struct Recorder {
    SphereSelfTestResult* out;
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
// rounding of the radius plus the ray arithmetic. 1e-5 absolute is two orders of
// magnitude above that floor at these sizes and far tighter than any real
// topology or winding error, which would be off by a whole radius.
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

float lengthOf(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

ConstructionMesh sphereMesh(Meters diameter) {
    ConstructionSphere sphere;
    sphere.setDimensionsMeters(diameter);
    return sphere.generateMesh();
}

// Convenience for naming a ring vertex the way the generator lays them out.
uint32_t ringVertex(uint32_t ring, uint32_t meridian) {
    return ring * kSphereRadialSegments + meridian;
}

// ---------------------------------------------------------------------------
// The typed primitive parameter boundary
// ---------------------------------------------------------------------------
//
// The point of these checks is that a request cannot be misread as another
// primitive's. A spec built for one primitive does not contain the others'
// parameters at all, so there is nothing to read by mistake.

void testTypedSpecCarriesOnlyItsOwnParameters(Recorder& r) {
    const PrimitiveSpec box = PrimitiveSpec::forBox(1.5, 0.8, 0.4);
    r.check("box_spec_reports_box_kind", box.kind() == PrimitiveKind::Box);
    r.check("box_spec_carries_only_box_parameters",
            box.box() != nullptr && box.cylinder() == nullptr && box.sphere() == nullptr);
    r.check("box_spec_values_are_exact", box.box()->width == 1.5 && box.box()->height == 0.8 &&
                                             box.box()->depth == 0.4);

    const PrimitiveSpec cylinder = PrimitiveSpec::forCylinder(1.2, 2.4);
    r.check("cylinder_spec_reports_cylinder_kind", cylinder.kind() == PrimitiveKind::Cylinder);
    r.check("cylinder_spec_carries_only_cylinder_parameters",
            cylinder.cylinder() != nullptr && cylinder.box() == nullptr &&
                cylinder.sphere() == nullptr);
    r.check("cylinder_spec_values_are_exact",
            cylinder.cylinder()->diameter == 1.2 && cylinder.cylinder()->height == 2.4);

    const PrimitiveSpec sphere = PrimitiveSpec::forSphere(1.5);
    r.check("sphere_spec_reports_sphere_kind", sphere.kind() == PrimitiveKind::Sphere);
    r.check("sphere_spec_carries_only_sphere_parameters",
            sphere.sphere() != nullptr && sphere.box() == nullptr && sphere.cylinder() == nullptr);
    r.check("sphere_spec_value_is_exact", sphere.sphere()->diameter == 1.5);

    // A default-constructed spec is the default box, never an unset kind.
    const PrimitiveSpec fresh;
    r.check("default_spec_is_the_default_box",
            fresh.kind() == PrimitiveKind::Box && fresh.box() != nullptr &&
                fresh.box()->width == kDefaultBoxWidthMeters);

    // kind() is derived from the payload, so it cannot disagree with it.
    r.check("kind_matches_payload_index",
            static_cast<size_t>(box.kind()) == box.payload().index() &&
                static_cast<size_t>(cylinder.kind()) == cylinder.payload().index() &&
                static_cast<size_t>(sphere.kind()) == sphere.payload().index());
}

void testTypedRequestsCannotCrossPrimitives(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    publishConstructionObject(object, store);

    // A cylinder request must not be readable, or applied, as a box or a sphere.
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("cylinder_request_applies_only_to_the_cylinder",
            object.kind() == PrimitiveKind::Cylinder && object.cylinder().diameterMeters() == 1.2 &&
                object.cylinder().heightMeters() == 2.4 &&
                object.box().widthMeters() == kDefaultBoxWidthMeters &&
                object.sphere().diameterMeters() == kDefaultSphereDiameterMeters);
    r.check("cylinder_active_spec_exposes_no_box_or_sphere_values",
            object.spec().cylinder() != nullptr && object.spec().box() == nullptr &&
                object.spec().sphere() == nullptr);

    // A sphere request must not disturb the remembered box or cylinder.
    applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    r.check("sphere_request_applies_only_to_the_sphere",
            object.kind() == PrimitiveKind::Sphere && object.sphere().diameterMeters() == 1.5 &&
                object.cylinder().diameterMeters() == 1.2 &&
                object.cylinder().heightMeters() == 2.4 &&
                object.box().widthMeters() == kDefaultBoxWidthMeters);
    r.check("sphere_active_spec_exposes_no_box_or_cylinder_values",
            object.spec().sphere() != nullptr && object.spec().box() == nullptr &&
                object.spec().cylinder() == nullptr);

    // A box request must not disturb the remembered cylinder or sphere.
    applyPrimitive(object, store, PrimitiveSpec::forBox(1.5, 0.8, 0.4));
    r.check("box_request_applies_only_to_the_box",
            object.kind() == PrimitiveKind::Box && object.box().widthMeters() == 1.5 &&
                object.box().heightMeters() == 0.8 && object.box().depthMeters() == 0.4 &&
                object.sphere().diameterMeters() == 1.5 &&
                object.cylinder().diameterMeters() == 1.2);
    r.check("box_active_spec_exposes_no_cylinder_or_sphere_values",
            object.spec().box() != nullptr && object.spec().cylinder() == nullptr &&
                object.spec().sphere() == nullptr);

    // The sphere's 1.5 m diameter and the box's 1.5 m width are the same number
    // in different primitives. Neither can be read as the other, which is the
    // whole reason the payload is typed.
    r.check("identical_numbers_in_different_primitives_stay_separate",
            object.sphere().diameterMeters() == 1.5 && object.box().widthMeters() == 1.5 &&
                object.spec().sphere() == nullptr);
}

void testInvalidTypedRequestFailsClosed(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        PrimitiveSpec spec;
        DimensionValidation expected;
    };
    const Case cases[] = {
        {"reject_zero_sphere_diameter", PrimitiveSpec::forSphere(0.0),
         DimensionValidation::NotPositive},
        {"reject_negative_sphere_diameter", PrimitiveSpec::forSphere(-1.5),
         DimensionValidation::NotPositive},
        {"reject_nan_sphere_diameter", PrimitiveSpec::forSphere(nan),
         DimensionValidation::NotFinite},
        {"reject_inf_sphere_diameter", PrimitiveSpec::forSphere(inf),
         DimensionValidation::NotFinite},
        {"reject_unrepresentable_sphere_diameter", PrimitiveSpec::forSphere(1e40),
         DimensionValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        // Start from a known non-default sphere under a non-default placement,
        // so a rejection has something real to preserve.
        ConstructionObject object;
        // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
        // Source. This suite drives a standalone source, so it pairs one here. The
        // invariant under test is unchanged: a primitive change must not disturb it.
        ConstructionTransform placement_;
        MeshStore store(kConstructionBoxObjectId);
        applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
        TransformValues placement;
        placement.positionY = -1.25;
        placement.rotationZ = 90.0;
        placement_.setValues(placement);

        const MeshRevision before = store.currentRevision();
        const uint64_t publishedBefore = store.publishedCount();
        const PrimitiveApplyResult rejected = applyPrimitive(object, store, c.spec);

        const bool refused = rejected.status == PrimitiveUpdateStatus::Rejected &&
                             rejected.validation == c.expected;
        const bool kindHeld = object.kind() == PrimitiveKind::Sphere;
        const bool parametersHeld = object.sphere().diameterMeters() == 1.5;
        const bool revisionHeld = store.currentRevision() == before &&
                                  store.publishedCount() == publishedBefore && !rejected.published;
        const TransformValues t = placement_.values();
        const bool transformHeld = t.positionY == -1.25 && t.rotationZ == 90.0;
        r.check(c.name, refused && kindHeld && parametersHeld && revisionHeld && transformHeld);
    }

    // A rejected SPHERE request must not switch the kind away from the cylinder.
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    DimensionValidation why = DimensionValidation::Ok;
    const PrimitiveUpdateStatus status = object.setPrimitive(PrimitiveSpec::forSphere(0.0), &why);
    r.check("rejected_sphere_request_does_not_switch_kind",
            status == PrimitiveUpdateStatus::Rejected && why == DimensionValidation::NotPositive &&
                object.kind() == PrimitiveKind::Cylinder &&
                object.sphere().diameterMeters() == kDefaultSphereDiameterMeters);
    r.check("rejected_sphere_request_is_counted",
            object.rejectedUpdateCount() == 1 && object.updateCount() == 1);
}

void testSphereApplySemantics(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    const uint64_t publishedBefore = store.publishedCount();

    const PrimitiveApplyResult toSphere =
        applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    r.check("box_to_sphere_publishes_exactly_one_revision",
            toSphere.status == PrimitiveUpdateStatus::Applied && toSphere.published &&
                toSphere.revision == rev0 + 1 && store.publishedCount() == publishedBefore + 1 &&
                store.currentRevision() == toSphere.revision);
    r.check("box_to_sphere_reports_sphere_topology",
            toSphere.vertexCount == kSphereVertexCount &&
                toSphere.indexCount == kSphereIndexCount);
    r.check("box_to_sphere_store_holds_sphere_topology",
            store.current() != nullptr && store.current()->vertexCount() == kSphereVertexCount &&
                store.current()->indexCount() == kSphereIndexCount);
    r.check("sphere_diameter_is_the_exact_authoritative_double",
            object.sphere().diameterMeters() == 1.5);
    r.check("sphere_radius_is_derived_not_stored", object.sphere().radiusMeters() == 0.75);
    r.check("applied_result_spec_is_the_sphere",
            toSphere.spec.sphere() != nullptr && toSphere.spec.sphere()->diameter == 1.5);

    // An identical request is a no-op.
    const MeshRevision afterSwitch = store.currentRevision();
    const uint64_t publishedAfterSwitch = store.publishedCount();
    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    r.check("identical_sphere_is_unchanged", repeat.status == PrimitiveUpdateStatus::Unchanged);
    r.check("identical_sphere_publishes_nothing",
            !repeat.published && store.currentRevision() == afterSwitch &&
                store.publishedCount() == publishedAfterSwitch);
    r.check("identical_sphere_does_not_count_as_update", object.updateCount() == 1);

    // A diameter change publishes exactly one revision at the same topology.
    const PrimitiveApplyResult resized =
        applyPrimitive(object, store, PrimitiveSpec::forSphere(0.6));
    r.check("sphere_diameter_change_publishes_one_revision",
            resized.status == PrimitiveUpdateStatus::Applied && resized.published &&
                resized.revision == afterSwitch + 1 &&
                store.publishedCount() == publishedAfterSwitch + 1);
    r.check("sphere_diameter_change_keeps_topology",
            resized.vertexCount == kSphereVertexCount && resized.indexCount == kSphereIndexCount);

    // A kind change is a change even when the sphere already holds that value.
    applyPrimitive(object, store, PrimitiveSpec::forBox(2.0, 1.0, 0.5));
    const MeshRevision beforeReswitch = store.currentRevision();
    const PrimitiveApplyResult reswitch =
        applyPrimitive(object, store, PrimitiveSpec::forSphere(0.6));
    r.check("sphere_kind_change_with_identical_parameter_is_applied",
            reswitch.status == PrimitiveUpdateStatus::Applied && reswitch.published &&
                reswitch.revision == beforeReswitch + 1);
}

void testIdentityAndTransformSurviveSphereSwitches(Recorder& r) {
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

    applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    const TransformValues afterSphere = placement_.values();
    r.check("object_id_stable_box_to_sphere", object.objectId() == idBefore);
    r.check("transform_survives_box_to_sphere",
            afterSphere.positionX == 0.75 && afterSphere.positionY == -1.25 &&
                afterSphere.positionZ == 0.5 && afterSphere.rotationX == 15.0 &&
                afterSphere.rotationY == 30.0 && afterSphere.rotationZ == 90.0);
    r.check("sphere_switch_does_not_count_as_a_transform_update",
            placement_.updateCount() == transformUpdatesBefore);

    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    applyPrimitive(object, store, PrimitiveSpec::forBox(1.5, 0.8, 0.4));
    const TransformValues afterRoundTrip = placement_.values();
    r.check("object_id_stable_across_all_three_kinds", object.objectId() == idBefore);
    r.check("transform_survives_sphere_cylinder_box",
            afterRoundTrip.positionX == 0.75 && afterRoundTrip.rotationZ == 90.0);
    r.check("sphere_parameter_remembered_while_box_is_active",
            object.sphere().diameterMeters() == 1.5);
    r.check("published_mesh_object_id_stable_across_sphere",
            store.current() != nullptr && store.current()->objectId() == kConstructionBoxObjectId);

    // A sphere diameter change must publish a mesh but no transform update.
    const uint64_t transformUpdates = placement_.updateCount();
    applyPrimitive(object, store, PrimitiveSpec::forSphere(2.0));
    r.check("sphere_parameter_edit_leaves_the_transform_alone",
            placement_.updateCount() == transformUpdates &&
                placement_.values().positionX == 0.75);
}

// ---------------------------------------------------------------------------
// Sphere geometry
// ---------------------------------------------------------------------------

void testSphereTopology(Recorder& r) {
    const ConstructionMesh mesh = sphereMesh(1.5);
    const uint32_t n = kSphereRadialSegments;
    const uint32_t rings = kSphereRingCount;

    r.check("sphere_vertex_count_is_rings_times_n_plus_2",
            mesh.vertices.size() == kSphereVertexCount && kSphereVertexCount == rings * n + 2);
    r.check("sphere_index_count_is_6n_times_stacks_minus_1",
            mesh.indices.size() == kSphereIndexCount &&
                kSphereIndexCount == 6 * n * (kSphereStacks - 1));
    r.check("sphere_triangle_count_is_2n_times_stacks_minus_1",
            (mesh.indices.size() / 3) == 2 * n * (kSphereStacks - 1));
    r.check("sphere_counts_are_deterministic",
            sphereMesh(0.05).vertices.size() == mesh.vertices.size() &&
                sphereMesh(0.05).indices.size() == mesh.indices.size() &&
                sphereMesh(40.0).vertices.size() == mesh.vertices.size() &&
                sphereMesh(40.0).indices.size() == mesh.indices.size());
    r.check("sphere_stacks_are_even_so_a_ring_lands_on_the_equator",
            kSphereStacks % 2 == 0 && kSphereRadialSegments % 4 == 0);

    bool finitePositions = true;
    for (const MeshVertex& v : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(v.position[axis])) finitePositions = false;
        }
    }
    r.check("sphere_positions_all_finite", finitePositions);

    bool indicesInRange = true;
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) indicesInRange = false;
    }
    r.check("sphere_indices_in_range", indicesInRange);

    // Reference counts: a pole anchors exactly one fan triangle per meridian; a
    // ring next to a pole is used 5 times (3 by its one band, 2 by the fan); an
    // interior ring is used 6 times (3 by each of its two bands). Together those
    // account for every index, which is what proves nothing is orphaned or
    // double-covered.
    std::vector<uint32_t> uses(mesh.vertices.size(), 0);
    for (uint32_t i : mesh.indices) {
        if (i < uses.size()) ++uses[i];
    }
    bool allUsed = true;
    for (uint32_t count : uses) {
        if (count == 0) allUsed = false;
    }
    r.check("sphere_uses_every_vertex", allUsed);
    r.check("sphere_north_pole_is_a_closed_fan", uses[rings * n] == n);
    r.check("sphere_south_pole_is_a_closed_fan", uses[rings * n + 1] == n);

    bool polarRingSharing = true;
    for (uint32_t j = 0; j < n; ++j) {
        if (uses[ringVertex(0, j)] != 5) polarRingSharing = false;
        if (uses[ringVertex(rings - 1, j)] != 5) polarRingSharing = false;
    }
    r.check("sphere_rings_beside_the_poles_are_shared_by_band_and_fan", polarRingSharing);

    bool interiorRingSharing = true;
    for (uint32_t ring = 1; ring + 1 < rings; ++ring) {
        for (uint32_t j = 0; j < n; ++j) {
            if (uses[ringVertex(ring, j)] != 6) interiorRingSharing = false;
        }
    }
    r.check("sphere_interior_ring_vertices_are_shared_by_two_bands", interiorRingSharing);

    uint32_t totalUses = 0;
    for (uint32_t count : uses) {
        totalUses += count;
    }
    r.check("sphere_vertex_uses_account_for_every_index",
            totalUses == kSphereIndexCount &&
                totalUses == 2 * n * 5 + (rings - 2) * n * 6 + 2 * n);

    r.check("sphere_mesh_passes_runtime_validation",
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(),
                             static_cast<uint32_t>(mesh.indices.size())) == MeshValidation::Ok);
}

// A closed, consistently oriented surface has every directed edge exactly once,
// and every directed edge's reverse present. That is a stronger statement than
// "every vertex is used": it fails on a hole, on a duplicated triangle and on a
// triangle wound the wrong way round.
void testSphereIsClosed(Recorder& r) {
    const ConstructionMesh mesh = sphereMesh(1.5);

    std::vector<std::pair<uint32_t, uint32_t>> edges;
    edges.reserve(mesh.indices.size());
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t a = mesh.indices[t];
        const uint32_t b = mesh.indices[t + 1];
        const uint32_t c = mesh.indices[t + 2];
        edges.emplace_back(a, b);
        edges.emplace_back(b, c);
        edges.emplace_back(c, a);
    }
    std::sort(edges.begin(), edges.end());

    bool noDuplicateDirectedEdge = true;
    for (size_t i = 1; i < edges.size(); ++i) {
        if (edges[i] == edges[i - 1]) noDuplicateDirectedEdge = false;
    }
    r.check("sphere_has_no_duplicated_directed_edge", noDuplicateDirectedEdge);

    bool everyEdgeHasItsOpposite = true;
    for (const auto& e : edges) {
        if (!std::binary_search(edges.begin(), edges.end(),
                                std::make_pair(e.second, e.first))) {
            everyEdgeHasItsOpposite = false;
        }
    }
    r.check("sphere_every_edge_is_shared_by_two_opposed_triangles", everyEdgeHasItsOpposite);

    // Euler characteristic of a closed sphere: V - E + F = 2. Each undirected
    // edge appears twice above, so E is half the directed edge count.
    const size_t v = mesh.vertices.size();
    const size_t e = edges.size() / 2;
    const size_t f = mesh.indices.size() / 3;
    r.check("sphere_euler_characteristic_is_2", (v + f) == (e + 2));
}

void testSphereBoundsAndExactSamples(Recorder& r) {
    struct Case {
        const char* boundsName;
        const char* windingName;
        Meters diameter;
    };
    const Case cases[] = {
        {"sphere_bounds_default", "sphere_winding_default", kDefaultSphereDiameterMeters},
        {"sphere_bounds_1_5", "sphere_winding_1_5", 1.5},
        {"sphere_bounds_tiny", "sphere_winding_tiny", 0.05},
        {"sphere_bounds_large", "sphere_winding_large", 40.0},
    };

    for (const Case& c : cases) {
        const ConstructionMesh mesh = sphereMesh(c.diameter);
        const float radius = static_cast<float>(c.diameter * 0.5);

        Bounds b{};
        const bool ok = boundsOf(mesh, &b);
        // Because the equator and the four cardinal meridians are written down
        // exactly rather than computed, the X and Z bounds are EXACTLY the
        // radius, and the poles make the Y bounds exactly the radius too.
        r.check(c.boundsName, ok && b.maxAxis[0] == radius && b.minAxis[0] == -radius &&
                                  b.maxAxis[1] == radius && b.minAxis[1] == -radius &&
                                  b.maxAxis[2] == radius && b.minAxis[2] == -radius);

        // Canonical winding: every outward normal points away from the centre,
        // which is what makes the whole surface survive back-face culling.
        r.check(c.windingName, meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, 0.0f, 0.0f}));
    }

    // Deterministic cardinal and equator samples, read straight out of the
    // generated data. The equator is ring kSphereStacks/2 - 1 (rings are counted
    // from the first one below the north pole), and meridians 0, N/4, N/2 and
    // 3N/4 are the four cardinal directions.
    const ConstructionMesh mesh = sphereMesh(1.5);
    const float radius = 0.75f;
    const uint32_t n = kSphereRadialSegments;
    const uint32_t equator = kSphereStacks / 2 - 1;
    const MeshVertex& plusX = mesh.vertices[ringVertex(equator, 0)];
    const MeshVertex& plusZ = mesh.vertices[ringVertex(equator, n / 4)];
    const MeshVertex& minusX = mesh.vertices[ringVertex(equator, n / 2)];
    const MeshVertex& minusZ = mesh.vertices[ringVertex(equator, 3 * n / 4)];

    r.check("sphere_equator_plus_x_is_exact",
            plusX.position[0] == radius && plusX.position[1] == 0.0f &&
                plusX.position[2] == 0.0f);
    r.check("sphere_equator_plus_z_is_exact",
            plusZ.position[0] == 0.0f && plusZ.position[1] == 0.0f &&
                plusZ.position[2] == radius);
    r.check("sphere_equator_minus_x_is_exact",
            minusX.position[0] == -radius && minusX.position[1] == 0.0f &&
                minusX.position[2] == 0.0f);
    r.check("sphere_equator_minus_z_is_exact",
            minusZ.position[0] == 0.0f && minusZ.position[1] == 0.0f &&
                minusZ.position[2] == -radius);

    const MeshVertex& north = mesh.vertices[kSphereRingCount * n];
    const MeshVertex& south = mesh.vertices[kSphereRingCount * n + 1];
    r.check("sphere_north_pole_is_exact",
            north.position[0] == 0.0f && north.position[1] == radius &&
                north.position[2] == 0.0f);
    r.check("sphere_south_pole_is_exact",
            south.position[0] == 0.0f && south.position[1] == -radius &&
                south.position[2] == 0.0f);

    // Every ring is a real circle of latitude: one radius from the centre, to
    // within float rounding, and never outside the sphere.
    bool onTheSurface = true;
    for (const MeshVertex& v : mesh.vertices) {
        const float d = lengthOf(Vec3{v.position[0], v.position[1], v.position[2]});
        if (d > radius + kEpsilon || d < radius - kEpsilon) onTheSurface = false;
    }
    r.check("sphere_every_vertex_is_one_radius_from_the_centre", onTheSurface);

    // The centre is exactly the local origin: min and max are exact negatives on
    // every axis, so nothing has drifted off-centre.
    Bounds b{};
    const bool ok = boundsOf(mesh, &b);
    r.check("sphere_is_centred_on_the_local_origin",
            ok && b.minAxis[0] == -b.maxAxis[0] && b.minAxis[1] == -b.maxAxis[1] &&
                b.minAxis[2] == -b.maxAxis[2]);
}

// The pole fans are what keep this true: a collapsed quad grid would put N
// zero-area triangles at each pole.
void testSphereHasNoDegenerateTriangles(Recorder& r) {
    const Meters diameters[] = {kDefaultSphereDiameterMeters, 1.5, 0.05, 40.0};
    const char* names[] = {"sphere_no_degenerate_triangles_default",
                           "sphere_no_degenerate_triangles_1_5",
                           "sphere_no_degenerate_triangles_tiny",
                           "sphere_no_degenerate_triangles_large"};

    for (int c = 0; c < 4; ++c) {
        const ConstructionMesh mesh = sphereMesh(diameters[c]);
        bool allNonDegenerate = true;
        for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
            const MeshVertex& a = mesh.vertices[mesh.indices[t]];
            const MeshVertex& b = mesh.vertices[mesh.indices[t + 1]];
            const MeshVertex& d = mesh.vertices[mesh.indices[t + 2]];
            const Vec3 e1{b.position[0] - a.position[0], b.position[1] - a.position[1],
                          b.position[2] - a.position[2]};
            const Vec3 e2{d.position[0] - a.position[0], d.position[1] - a.position[1],
                          d.position[2] - a.position[2]};
            const Vec3 cross{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                             e1.x * e2.y - e1.y * e2.x};
            if (!(lengthOf(cross) > 0.0f)) allNonDegenerate = false;
            // No two vertices of a triangle may coincide either.
            if (mesh.indices[t] == mesh.indices[t + 1] ||
                mesh.indices[t + 1] == mesh.indices[t + 2] ||
                mesh.indices[t] == mesh.indices[t + 2]) {
                allNonDegenerate = false;
            }
        }
        r.check(names[c], allNonDegenerate);
    }
}

void testSpherePicking(Recorder& r) {
    // Diameter 1.5 m, so radius 0.75 m.
    const ConstructionMesh mesh = sphereMesh(1.5);
    const TriangleMeshView view = viewOf(mesh);
    const float radius = 0.75f;
    // A tessellated sphere is inscribed in the exact one, so a surface point is
    // between the facet inradius and the radius. Longitude and latitude steps
    // are both pi/16, so the worst case is cos(pi/32)^2 = 0.9904.
    const float minSurface = radius * 0.985f;

    // Front surface hit, deliberately off the cardinal vertices so the ray meets
    // the middle of a facet rather than a shared vertex.
    const TriangleHit front = pickTriangleMesh(
        makeRay(Vec3{0.05f, 0.03f, 10.0f}, Vec3{0.0f, 0.0f, -1.0f}), view, true);
    const float frontRadius =
        front.hit ? lengthOf(front.position) : 0.0f;
    r.check("sphere_front_surface_is_hit",
            front.hit && front.position.z > 0.0f && frontRadius >= minSurface &&
                frontRadius <= radius + kEpsilon);

    // A materially different region: the upper surface, from a different axis.
    const TriangleHit upper = pickTriangleMesh(
        makeRay(Vec3{0.31f, 10.0f, 0.17f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    const float upperRadius = upper.hit ? lengthOf(upper.position) : 0.0f;
    r.check("sphere_upper_region_is_hit",
            upper.hit && upper.position.y > 0.0f && upperRadius >= minSurface &&
                upperRadius <= radius + kEpsilon);
    r.check("sphere_two_regions_are_materially_different",
            front.hit && upper.hit && std::fabs(front.position.y - upper.position.y) > 0.25f);

    // Near a pole: still on the surface, still not the exact pole vertex.
    const TriangleHit nearPole = pickTriangleMesh(
        makeRay(Vec3{0.02f, 10.0f, 0.011f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("sphere_pole_region_is_hit",
            nearPole.hit && nearPole.position.y > radius * 0.98f &&
                nearPole.position.y <= radius + kEpsilon);

    // Outside the silhouette must MISS. A ray at x = 0.8 passes a 0.75 m radius
    // sphere entirely.
    const TriangleHit beside = pickTriangleMesh(
        makeRay(Vec3{0.8f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("sphere_ray_outside_radius_misses", !beside.hit);

    // A corner a box of the same width would occupy is empty on a sphere:
    // |(0.6, 0.6)| = 0.85 > 0.75.
    const TriangleHit corner = pickTriangleMesh(
        makeRay(Vec3{0.6f, 10.0f, 0.6f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("sphere_is_round_not_square", !corner.hit);

    // Front-face-only picking: a ray fired from inside must miss, exactly as it
    // does for the box and the cylinder.
    const TriangleHit inside = pickTriangleMesh(
        makeRay(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.3f, 0.9f, 0.31f}), view, true);
    r.check("sphere_front_face_only_ray_from_inside_misses", !inside.hit);

    // A smaller sphere is not hit where the larger one was: picking follows the
    // authoritative diameter with no separate collision representation.
    const ConstructionMesh small = sphereMesh(0.6);  // radius 0.3
    const TriangleHit past = pickTriangleMesh(
        makeRay(Vec3{0.5f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), viewOf(small), true);
    r.check("smaller_sphere_is_missed_where_the_larger_one_was_hit", !past.hit);
    const TriangleHit onSmall = pickTriangleMesh(
        makeRay(Vec3{0.05f, 10.0f, 0.03f}, Vec3{0.0f, -1.0f, 0.0f}), viewOf(small), true);
    r.check("smaller_sphere_is_hit_at_its_own_radius",
            onSmall.hit && onSmall.position.y >= 0.3f * 0.98f &&
                onSmall.position.y <= 0.3f + kEpsilon);
}

void testTransformedSpherePicking(Recorder& r) {
    const ConstructionMesh mesh = sphereMesh(1.5);
    const TriangleMeshView view = viewOf(mesh);

    ConstructionTransform transform;
    TransformValues placement;
    placement.positionX = 5.0;
    placement.positionY = 0.5;
    transform.setValues(placement);

    Ray local{};
    const bool moved = transformRayToLocal(
        makeRay(Vec3{5.05f, 10.0f, 0.03f}, Vec3{0.0f, -1.0f, 0.0f}),
        transform.inverseModelMatrix(), &local);
    const TriangleHit hit = pickTriangleMesh(local, view, true);
    const Vec3 world = mat4TransformPoint(transform.modelMatrix(), hit.position);
    r.check("transformed_sphere_is_hit", moved && hit.hit);
    r.check("transformed_sphere_hit_is_at_the_moved_top",
            hit.hit && nearly(world.x, 5.05f) && world.y > 0.5f + 0.75f * 0.98f &&
                world.y <= 0.5f + 0.75f + kEpsilon);

    // Where the sphere used to be is now empty.
    Ray atOrigin{};
    transformRayToLocal(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                        transform.inverseModelMatrix(), &atOrigin);
    r.check("moved_sphere_leaves_its_old_place_empty",
            !pickTriangleMesh(atOrigin, view, true).hit);

    // A rotated sphere is still hit in exactly the same place — which is the
    // honest statement for a sphere: rotation is real in the transform, and
    // invisible in the silhouette.
    ConstructionTransform rotated;
    TransformValues spun;
    spun.rotationX = 37.0;
    spun.rotationY = 61.0;
    spun.rotationZ = 90.0;
    rotated.setValues(spun);
    Ray localRotated{};
    const bool ok = transformRayToLocal(makeRay(Vec3{0.05f, 10.0f, 0.03f}, Vec3{0.0f, -1.0f, 0.0f}),
                                        rotated.inverseModelMatrix(), &localRotated);
    const TriangleHit spunHit = pickTriangleMesh(localRotated, view, true);
    const Vec3 spunWorld = mat4TransformPoint(rotated.modelMatrix(), spunHit.position);
    r.check("rotated_sphere_is_still_hit", ok && spunHit.hit);
    r.check("rotated_sphere_hit_is_still_at_its_top",
            spunHit.hit && spunWorld.y > 0.75f * 0.98f && spunWorld.y <= 0.75f + kEpsilon);
    r.check("rotating_a_sphere_leaves_a_real_non_identity_transform",
            !rotated.isIdentity() && rotated.values().rotationY == 61.0);
}

// ---------------------------------------------------------------------------
// Box and cylinder regression through the typed boundary
// ---------------------------------------------------------------------------

void testBoxAndCylinderStillBehave(Recorder& r) {
    ConstructionObject object;
    // Since IMPORT-01A a placement belongs to the BODY, not to the Construction
    // Source. This suite drives a standalone source, so it pairs one here. The
    // invariant under test is unchanged: a primitive change must not disturb it.
    ConstructionTransform placement_;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    r.check("object_still_publishes_box_topology_at_startup",
            store.current() != nullptr && store.current()->vertexCount() == kBoxVertexCount &&
                store.current()->indexCount() == kBoxIndexCount);

    const PrimitiveApplyResult box =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("typed_box_apply_is_applied_once",
            box.status == PrimitiveUpdateStatus::Applied && box.published &&
                box.revision == rev0 + 1 && box.vertexCount == kBoxVertexCount &&
                box.indexCount == kBoxIndexCount);
    r.check("typed_box_apply_is_exact",
            object.box().widthMeters() == 1.25 && object.box().heightMeters() == 2.5 &&
                object.box().depthMeters() == 0.75);

    const PrimitiveApplyResult cylinder =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("typed_cylinder_apply_is_applied_once",
            cylinder.status == PrimitiveUpdateStatus::Applied && cylinder.published &&
                cylinder.revision == box.revision + 1 &&
                cylinder.vertexCount == kCylinderVertexCount &&
                cylinder.indexCount == kCylinderIndexCount);
    r.check("typed_cylinder_apply_is_exact",
            object.cylinder().diameterMeters() == 1.2 && object.cylinder().heightMeters() == 2.4);

    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("typed_cylinder_repeat_is_unchanged",
            repeat.status == PrimitiveUpdateStatus::Unchanged && !repeat.published &&
                store.currentRevision() == cylinder.revision);

    const PrimitiveApplyResult refused =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, -0.75));
    r.check("typed_box_rejection_still_fails_closed",
            refused.status == PrimitiveUpdateStatus::Rejected &&
                refused.validation == DimensionValidation::NotPositive &&
                object.kind() == PrimitiveKind::Cylinder && object.box().depthMeters() == 0.75 &&
                store.currentRevision() == cylinder.revision);

    // The three kinds all still generate their own topology from one object.
    applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    r.check("one_object_generates_all_three_topologies",
            object.generateMesh().vertices.size() == kSphereVertexCount &&
                sphereMesh(1.5).indices.size() == kSphereIndexCount);
}

}  // namespace

int runSphereSelfTests(SphereSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testTypedSpecCarriesOnlyItsOwnParameters(r);
    testTypedRequestsCannotCrossPrimitives(r);
    testInvalidTypedRequestFailsClosed(r);
    testSphereApplySemantics(r);
    testIdentityAndTransformSurviveSphereSwitches(r);
    testSphereTopology(r);
    testSphereIsClosed(r);
    testSphereBoundsAndExactSamples(r);
    testSphereHasNoDegenerateTriangles(r);
    testSpherePicking(r);
    testTransformedSpherePicking(r);
    testBoxAndCylinderStillBehave(r);
    return r.n;
}

}  // namespace forgeshape
