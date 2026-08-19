#include "forgeshape_cone_capsule_selftest.h"

#include <algorithm>
#include <cmath>
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
    ConeCapsuleSelfTestResult* out;
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
// rounding plus the ray arithmetic. 1e-5 absolute is two orders of magnitude
// above that floor at these sizes and far tighter than any real topology or
// winding error, which would be off by a whole radius.
constexpr float kEpsilon = 1e-5f;

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

ConstructionMesh coneMesh(Meters bottomDiameter, Meters height) {
    ConstructionCone cone;
    cone.setDimensionsMeters(bottomDiameter, height);
    return cone.generateMesh();
}

ConstructionMesh capsuleMesh(Meters diameter, Meters totalHeight) {
    ConstructionCapsule capsule;
    capsule.setDimensionsMeters(diameter, totalHeight);
    return capsule.generateMesh();
}

bool allPositionsFinite(const ConstructionMesh& mesh) {
    for (const MeshVertex& v : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(v.position[axis])) return false;
        }
    }
    return true;
}

bool allIndicesInRange(const ConstructionMesh& mesh) {
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) return false;
    }
    return mesh.indices.size() % 3 == 0;
}

// No triangle may have zero area, and no triangle may repeat a vertex. The first
// is the geometric statement and the second the topological one; a collapsed
// apex ring or a duplicated seam ring fails both.
bool noDegenerateTriangles(const ConstructionMesh& mesh) {
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const uint32_t ia = mesh.indices[t];
        const uint32_t ib = mesh.indices[t + 1];
        const uint32_t ic = mesh.indices[t + 2];
        if (ia == ib || ib == ic || ia == ic) return false;
        const MeshVertex& a = mesh.vertices[ia];
        const MeshVertex& b = mesh.vertices[ib];
        const MeshVertex& c = mesh.vertices[ic];
        const Vec3 e1{b.position[0] - a.position[0], b.position[1] - a.position[1],
                      b.position[2] - a.position[2]};
        const Vec3 e2{c.position[0] - a.position[0], c.position[1] - a.position[1],
                      c.position[2] - a.position[2]};
        const Vec3 cross{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z,
                         e1.x * e2.y - e1.y * e2.x};
        if (!(lengthOf(cross) > 0.0f)) return false;
    }
    return true;
}

// A closed, consistently oriented surface has every directed edge exactly once
// with its reverse present, and satisfies V - E + F = 2. That is stronger than
// "every vertex is used": it fails on a hole (an open base), on a duplicated
// triangle and on a triangle wound the wrong way round.
bool isClosedAndOriented(const ConstructionMesh& mesh, size_t* outEdges) {
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
    for (size_t i = 1; i < edges.size(); ++i) {
        if (edges[i] == edges[i - 1]) return false;
    }
    for (const auto& e : edges) {
        if (!std::binary_search(edges.begin(), edges.end(), std::make_pair(e.second, e.first))) {
            return false;
        }
    }
    if (outEdges != nullptr) {
        *outEdges = edges.size() / 2;
    }
    return true;
}

// ---------------------------------------------------------------------------
// CC-01  Typed Cone/Capsule payloads, and no generic-parameter regression
// ---------------------------------------------------------------------------

void testTypedConeAndCapsulePayloads(Recorder& r) {
    const PrimitiveSpec cone = PrimitiveSpec::forCone(2.0, 3.0);
    r.check("cone_spec_reports_cone_kind", cone.kind() == PrimitiveKind::Cone);
    r.check("cone_spec_carries_only_cone_parameters",
            cone.cone() != nullptr && cone.box() == nullptr && cone.cylinder() == nullptr &&
                cone.sphere() == nullptr && cone.capsule() == nullptr);
    r.check("cone_spec_values_are_exact",
            cone.cone()->bottomDiameter == 2.0 && cone.cone()->height == 3.0);

    const PrimitiveSpec capsule = PrimitiveSpec::forCapsule(1.0, 3.0);
    r.check("capsule_spec_reports_capsule_kind", capsule.kind() == PrimitiveKind::Capsule);
    r.check("capsule_spec_carries_only_capsule_parameters",
            capsule.capsule() != nullptr && capsule.box() == nullptr &&
                capsule.cylinder() == nullptr && capsule.sphere() == nullptr &&
                capsule.cone() == nullptr);
    r.check("capsule_spec_values_are_exact",
            capsule.capsule()->diameter == 1.0 && capsule.capsule()->totalHeight == 3.0);

    // kind() is derived from the payload's alternative index, so the tag and the
    // data cannot disagree even now that there are five of them.
    r.check("cone_and_capsule_kinds_match_payload_index",
            static_cast<size_t>(cone.kind()) == cone.payload().index() &&
                static_cast<size_t>(capsule.kind()) == capsule.payload().index());

    // A cone's (diameter, height) and a capsule's (diameter, total height) are
    // the same two numbers in the same order and mean different things. Neither
    // can be read as the other, which is the whole reason the payload is typed.
    const PrimitiveSpec sameNumbers = PrimitiveSpec::forCapsule(2.0, 3.0);
    r.check("cone_and_capsule_with_identical_numbers_stay_separate",
            sameNumbers.cone() == nullptr && sameNumbers.capsule() != nullptr &&
                cone.capsule() == nullptr && cone.cone() != nullptr);
}

void testTypedRequestsStillCannotCrossPrimitives(Recorder& r) {
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    publishConstructionObject(object, store);

    applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
    r.check("cone_request_applies_only_to_the_cone",
            object.kind() == PrimitiveKind::Cone && object.cone().bottomDiameterMeters() == 2.0 &&
                object.cone().heightMeters() == 3.0 &&
                object.box().widthMeters() == kDefaultBoxWidthMeters &&
                object.cylinder().diameterMeters() == kDefaultCylinderDiameterMeters &&
                object.sphere().diameterMeters() == kDefaultSphereDiameterMeters &&
                object.capsule().diameterMeters() == kDefaultCapsuleDiameterMeters);
    r.check("cone_active_spec_exposes_no_other_primitive_values",
            object.spec().cone() != nullptr && object.spec().box() == nullptr &&
                object.spec().cylinder() == nullptr && object.spec().sphere() == nullptr &&
                object.spec().capsule() == nullptr);

    applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
    r.check("capsule_request_applies_only_to_the_capsule",
            object.kind() == PrimitiveKind::Capsule && object.capsule().diameterMeters() == 1.0 &&
                object.capsule().totalHeightMeters() == 3.0 &&
                object.cone().bottomDiameterMeters() == 2.0 && object.cone().heightMeters() == 3.0);
    r.check("capsule_active_spec_exposes_no_other_primitive_values",
            object.spec().capsule() != nullptr && object.spec().cone() == nullptr &&
                object.spec().box() == nullptr && object.spec().cylinder() == nullptr &&
                object.spec().sphere() == nullptr);

    // The derived values are derived, never stored beside the authoritative
    // ones, so they cannot drift out of step with them.
    r.check("capsule_radius_and_middle_are_derived",
            object.capsule().radiusMeters() == 0.5 && object.capsule().middleHeightMeters() == 2.0);
    r.check("cone_bottom_radius_is_derived", object.cone().bottomRadiusMeters() == 1.0);
}

// ---------------------------------------------------------------------------
// CC-02  Applied / Unchanged / Rejected, and the strong failure guarantee
// ---------------------------------------------------------------------------

void testConeApplySemantics(Recorder& r) {
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);
    const uint64_t publishedBefore = store.publishedCount();

    const PrimitiveApplyResult toCone =
        applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
    r.check("box_to_cone_publishes_exactly_one_revision",
            toCone.status == PrimitiveUpdateStatus::Applied && toCone.published &&
                toCone.revision == rev0 + 1 && store.publishedCount() == publishedBefore + 1 &&
                store.currentRevision() == toCone.revision);
    r.check("box_to_cone_reports_cone_topology",
            toCone.vertexCount == kConeVertexCount && toCone.indexCount == kConeIndexCount);
    r.check("box_to_cone_store_holds_cone_topology",
            store.current() != nullptr && store.current()->vertexCount() == kConeVertexCount &&
                store.current()->indexCount() == kConeIndexCount);

    const MeshRevision afterSwitch = store.currentRevision();
    const uint64_t publishedAfterSwitch = store.publishedCount();
    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
    r.check("identical_cone_is_unchanged_and_publishes_nothing",
            repeat.status == PrimitiveUpdateStatus::Unchanged && !repeat.published &&
                store.currentRevision() == afterSwitch &&
                store.publishedCount() == publishedAfterSwitch);

    const PrimitiveApplyResult resized =
        applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 5.0));
    r.check("cone_height_change_publishes_one_revision_at_the_same_topology",
            resized.status == PrimitiveUpdateStatus::Applied && resized.published &&
                resized.revision == afterSwitch + 1 &&
                store.publishedCount() == publishedAfterSwitch + 1 &&
                resized.vertexCount == kConeVertexCount && resized.indexCount == kConeIndexCount);

    // A kind change is a change even when the cone already holds those values.
    applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    const MeshRevision beforeReswitch = store.currentRevision();
    const PrimitiveApplyResult reswitch =
        applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 5.0));
    r.check("cone_kind_change_with_identical_parameters_is_applied",
            reswitch.status == PrimitiveUpdateStatus::Applied && reswitch.published &&
                reswitch.revision == beforeReswitch + 1);
}

void testCapsuleApplySemantics(Recorder& r) {
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision rev0 = publishConstructionObject(object, store);

    const PrimitiveApplyResult toCapsule =
        applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
    r.check("box_to_capsule_publishes_exactly_one_revision",
            toCapsule.status == PrimitiveUpdateStatus::Applied && toCapsule.published &&
                toCapsule.revision == rev0 + 1 && store.currentRevision() == toCapsule.revision);
    r.check("box_to_capsule_reports_capsule_topology",
            toCapsule.vertexCount == kCapsuleVertexCount &&
                toCapsule.indexCount == kCapsuleIndexCount);

    const MeshRevision afterSwitch = store.currentRevision();
    const PrimitiveApplyResult repeat =
        applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
    r.check("identical_capsule_is_unchanged_and_publishes_nothing",
            repeat.status == PrimitiveUpdateStatus::Unchanged && !repeat.published &&
                store.currentRevision() == afterSwitch);

    // The degenerate capsule is a legal shape with a DIFFERENT topology, so it
    // is an ordinary Applied that happens to publish a smaller mesh.
    const PrimitiveApplyResult degenerate =
        applyPrimitive(object, store, PrimitiveSpec::forCapsule(2.0, 2.0));
    r.check("capsule_equal_to_its_diameter_is_applied",
            degenerate.status == PrimitiveUpdateStatus::Applied && degenerate.published &&
                degenerate.revision == afterSwitch + 1);
    r.check("capsule_with_no_middle_publishes_the_spherical_topology",
            degenerate.vertexCount == kCapsuleSphericalVertexCount &&
                degenerate.indexCount == kCapsuleSphericalIndexCount);
    r.check("capsule_with_no_middle_keeps_its_authoritative_parameters",
            object.capsule().diameterMeters() == 2.0 &&
                object.capsule().totalHeightMeters() == 2.0 &&
                object.capsule().middleHeightMeters() == 0.0);
}

void testInvalidConeAndCapsuleFailClosed(Recorder& r) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    struct Case {
        const char* name;
        PrimitiveSpec spec;
        DimensionValidation expected;
    };
    const Case cases[] = {
        {"reject_zero_cone_diameter", PrimitiveSpec::forCone(0.0, 3.0),
         DimensionValidation::NotPositive},
        {"reject_negative_cone_height", PrimitiveSpec::forCone(2.0, -3.0),
         DimensionValidation::NotPositive},
        {"reject_nan_cone_diameter", PrimitiveSpec::forCone(nan, 3.0),
         DimensionValidation::NotFinite},
        {"reject_inf_cone_height", PrimitiveSpec::forCone(2.0, inf),
         DimensionValidation::NotFinite},
        {"reject_unrepresentable_cone_diameter", PrimitiveSpec::forCone(1e40, 3.0),
         DimensionValidation::NotRepresentable},
        {"reject_zero_capsule_diameter", PrimitiveSpec::forCapsule(0.0, 3.0),
         DimensionValidation::NotPositive},
        {"reject_negative_capsule_total_height", PrimitiveSpec::forCapsule(1.0, -3.0),
         DimensionValidation::NotPositive},
        {"reject_nan_capsule_total_height", PrimitiveSpec::forCapsule(1.0, nan),
         DimensionValidation::NotFinite},
        {"reject_inf_capsule_diameter", PrimitiveSpec::forCapsule(inf, 3.0),
         DimensionValidation::NotFinite},
        {"reject_unrepresentable_capsule_diameter", PrimitiveSpec::forCapsule(1e40, 1e41),
         DimensionValidation::NotRepresentable},
        // The capsule's own relation. Both values are perfectly good lengths;
        // what is wrong is the pair.
        {"reject_capsule_shorter_than_its_diameter", PrimitiveSpec::forCapsule(2.0, 1.0),
         DimensionValidation::RelationInvalid},
        {"reject_capsule_barely_shorter_than_its_diameter",
         PrimitiveSpec::forCapsule(2.0, 1.9999999), DimensionValidation::RelationInvalid},
        // Finite in every coordinate, and yet a whole hemisphere would round to
        // one float: refused as unbuildable rather than published as a mesh of
        // zero-area triangles.
        {"reject_capsule_whose_ends_cannot_be_resolved_in_float",
         PrimitiveSpec::forCapsule(1e-6, 1e30), DimensionValidation::NotRepresentable},
    };

    for (const Case& c : cases) {
        // Start from a known non-default CONE under a non-default placement, so
        // a rejection has an active kind, remembered payloads, an ObjectId, a
        // transform and a mesh revision to preserve.
        ConstructionObject object;
        MeshStore store(kConstructionBoxObjectId);
        applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
        applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
        TransformValues placement;
        placement.positionY = -1.25;
        placement.rotationZ = 90.0;
        object.transform().setValues(placement);

        const ObjectId idBefore = object.objectId();
        const MeshRevision before = store.currentRevision();
        const uint64_t publishedBefore = store.publishedCount();
        const PrimitiveApplyResult rejected = applyPrimitive(object, store, c.spec);

        const bool refused = rejected.status == PrimitiveUpdateStatus::Rejected &&
                             rejected.validation == c.expected;
        const bool kindHeld = object.kind() == PrimitiveKind::Cone;
        const bool activeHeld = object.cone().bottomDiameterMeters() == 2.0 &&
                                object.cone().heightMeters() == 3.0;
        // Every remembered payload, not only the active one.
        const bool rememberedHeld = object.capsule().diameterMeters() == 1.0 &&
                                    object.capsule().totalHeightMeters() == 3.0 &&
                                    object.box().widthMeters() == kDefaultBoxWidthMeters &&
                                    object.sphere().diameterMeters() ==
                                        kDefaultSphereDiameterMeters;
        const bool revisionHeld = store.currentRevision() == before &&
                                  store.publishedCount() == publishedBefore && !rejected.published;
        const TransformValues t = object.transform().values();
        const bool transformHeld = t.positionY == -1.25 && t.rotationZ == 90.0;
        const bool identityHeld = object.objectId() == idBefore;
        r.check(c.name, refused && kindHeld && activeHeld && rememberedHeld && revisionHeld &&
                            transformHeld && identityHeld);
    }

    // A refused request must not switch the kind, which is what "fails closed"
    // means for a primitive rather than for a single dimension.
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    DimensionValidation why = DimensionValidation::Ok;
    const PrimitiveUpdateStatus coneStatus =
        object.setPrimitive(PrimitiveSpec::forCone(0.0, 3.0), &why);
    r.check("rejected_cone_request_does_not_switch_kind",
            coneStatus == PrimitiveUpdateStatus::Rejected &&
                why == DimensionValidation::NotPositive &&
                object.kind() == PrimitiveKind::Cylinder &&
                object.cone().bottomDiameterMeters() == kDefaultConeBottomDiameterMeters);
    const PrimitiveUpdateStatus capsuleStatus =
        object.setPrimitive(PrimitiveSpec::forCapsule(2.0, 1.0), &why);
    r.check("rejected_capsule_relation_does_not_switch_kind",
            capsuleStatus == PrimitiveUpdateStatus::Rejected &&
                why == DimensionValidation::RelationInvalid &&
                object.kind() == PrimitiveKind::Cylinder &&
                object.capsule().totalHeightMeters() == kDefaultCapsuleTotalHeightMeters);
    r.check("rejected_cone_and_capsule_requests_are_counted",
            object.rejectedUpdateCount() == 2 && object.updateCount() == 1);

    // The relation is a domain rule with a named owner, so it can be asserted
    // directly rather than only through an apply.
    r.check("capsule_relation_owner_accepts_equality",
            validateCapsuleMeters(2.0, 2.0) == DimensionValidation::Ok);
    r.check("capsule_relation_owner_refuses_shortfall",
            validateCapsuleMeters(2.0, 1.999) == DimensionValidation::RelationInvalid);
    r.check("capsule_relation_owner_reports_length_problems_first",
            validateCapsuleMeters(-1.0, 0.5) == DimensionValidation::NotPositive &&
                validateCapsuleMeters(nan, 3.0) == DimensionValidation::NotFinite);
}

// ---------------------------------------------------------------------------
// CC-03  Cone geometry
// ---------------------------------------------------------------------------

void testConeTopology(Recorder& r) {
    const ConstructionMesh mesh = coneMesh(2.0, 2.0);
    const uint32_t n = kConeRadialSegments;

    r.check("cone_vertex_count_is_n_plus_2",
            mesh.vertices.size() == kConeVertexCount && kConeVertexCount == n + 2);
    r.check("cone_index_count_is_6n",
            mesh.indices.size() == kConeIndexCount && kConeIndexCount == 6 * n);
    r.check("cone_triangle_count_is_2n", (mesh.indices.size() / 3) == 2 * n);
    r.check("cone_counts_are_deterministic",
            coneMesh(0.05, 0.05).vertices.size() == mesh.vertices.size() &&
                coneMesh(0.05, 0.05).indices.size() == mesh.indices.size() &&
                coneMesh(40.0, 0.01).vertices.size() == mesh.vertices.size() &&
                coneMesh(40.0, 0.01).indices.size() == mesh.indices.size());
    r.check("cone_positions_all_finite", allPositionsFinite(mesh));
    r.check("cone_indices_in_range", allIndicesInRange(mesh));
    r.check("cone_shares_the_one_radial_tessellation_owner",
            kConeRadialSegments == kPrimitiveRadialSegments && kConeRadialSegments % 4 == 0);

    // Reference counts: the apex anchors one side triangle per segment, the base
    // centre one fan triangle per segment, and each ring vertex is used four
    // times — twice by the two side triangles it borders and twice by the two
    // base-fan triangles. Together those account for the whole index buffer.
    std::vector<uint32_t> uses(mesh.vertices.size(), 0);
    for (uint32_t i : mesh.indices) {
        if (i < uses.size()) ++uses[i];
    }
    bool ringSharing = true;
    for (uint32_t j = 0; j < n; ++j) {
        if (uses[j] != 4) ringSharing = false;
    }
    r.check("cone_ring_vertices_are_shared_by_side_and_base", ringSharing);
    r.check("cone_apex_is_a_closed_fan", uses[n + 1] == n);
    r.check("cone_base_centre_is_a_closed_fan", uses[n] == n);
    uint32_t totalUses = 0;
    for (uint32_t count : uses) {
        totalUses += count;
    }
    r.check("cone_vertex_uses_account_for_every_index",
            totalUses == kConeIndexCount && totalUses == 4 * n + 2 * n);

    r.check("cone_mesh_passes_runtime_validation",
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(),
                             static_cast<uint32_t>(mesh.indices.size())) == MeshValidation::Ok);

    // A CLOSED base is the whole point of this check: an open one would leave
    // the base ring's outer edges unmatched.
    size_t edges = 0;
    const bool closed = isClosedAndOriented(mesh, &edges);
    r.check("cone_is_closed_and_consistently_oriented", closed);
    r.check("cone_euler_characteristic_is_2",
            closed && (mesh.vertices.size() + mesh.indices.size() / 3) == (edges + 2));
}

void testConeBoundsWindingAndDegeneracy(Recorder& r) {
    struct Case {
        const char* boundsName;
        const char* windingName;
        const char* degenerateName;
        Meters diameter;
        Meters height;
    };
    const Case cases[] = {
        {"cone_bounds_default", "cone_winding_default", "cone_no_degenerate_default",
         kDefaultConeBottomDiameterMeters, kDefaultConeHeightMeters},
        {"cone_bounds_2x2", "cone_winding_2x2", "cone_no_degenerate_2x2", 2.0, 2.0},
        {"cone_bounds_tiny", "cone_winding_tiny", "cone_no_degenerate_tiny", 0.05, 0.02},
        {"cone_bounds_wide_and_flat", "cone_winding_wide_and_flat",
         "cone_no_degenerate_wide_and_flat", 40.0, 0.01},
        {"cone_bounds_tall_and_thin", "cone_winding_tall_and_thin",
         "cone_no_degenerate_tall_and_thin", 0.01, 40.0},
    };

    for (const Case& c : cases) {
        const ConstructionMesh mesh = coneMesh(c.diameter, c.height);
        const float radius = static_cast<float>(c.diameter * 0.5);
        const float halfY = static_cast<float>(c.height * 0.5);

        Bounds b{};
        const bool ok = boundsOf(mesh, &b);
        // The four cardinal directions are written down exactly rather than
        // computed, so the X and Z bounds are EXACTLY the bottom radius; the
        // base and the apex make the Y bounds exactly +/- height/2.
        r.check(c.boundsName, ok && b.maxAxis[0] == radius && b.minAxis[0] == -radius &&
                                  b.maxAxis[2] == radius && b.minAxis[2] == -radius &&
                                  b.maxAxis[1] == halfY && b.minAxis[1] == -halfY);

        // Canonical winding: every outward normal points away from the solid,
        // which is what makes the sides AND the base survive back-face culling.
        // The reference point is the cone's centroid, which is inside it.
        r.check(c.windingName,
                meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, -halfY * 0.5f, 0.0f}));

        // The apex is one vertex fanned to the base ring, so nothing here is
        // zero-area — a collapsed top ring would put N of them at the tip.
        r.check(c.degenerateName, noDegenerateTriangles(mesh));
    }

    // Deterministic cardinal samples on the base ring, read straight out of the
    // generated data, plus the apex and the base centre.
    const ConstructionMesh mesh = coneMesh(2.0, 2.0);
    const uint32_t n = kConeRadialSegments;
    r.check("cone_base_plus_x_is_exact",
            mesh.vertices[0].position[0] == 1.0f && mesh.vertices[0].position[1] == -1.0f &&
                mesh.vertices[0].position[2] == 0.0f);
    r.check("cone_base_plus_z_is_exact",
            mesh.vertices[n / 4].position[0] == 0.0f &&
                mesh.vertices[n / 4].position[2] == 1.0f);
    r.check("cone_base_minus_x_is_exact",
            mesh.vertices[n / 2].position[0] == -1.0f &&
                mesh.vertices[n / 2].position[2] == 0.0f);
    r.check("cone_base_minus_z_is_exact",
            mesh.vertices[3 * n / 4].position[0] == 0.0f &&
                mesh.vertices[3 * n / 4].position[2] == -1.0f);
    r.check("cone_apex_is_a_point_on_the_axis",
            mesh.vertices[n + 1].position[0] == 0.0f && mesh.vertices[n + 1].position[1] == 1.0f &&
                mesh.vertices[n + 1].position[2] == 0.0f);
    r.check("cone_base_centre_is_on_the_axis_at_the_base",
            mesh.vertices[n].position[0] == 0.0f && mesh.vertices[n].position[1] == -1.0f &&
                mesh.vertices[n].position[2] == 0.0f);

    // The whole base ring lies in one plane and one circle: the base is flat and
    // round, not a cone that has drifted.
    bool baseIsFlatAndRound = true;
    for (uint32_t j = 0; j < n; ++j) {
        const MeshVertex& v = mesh.vertices[j];
        if (v.position[1] != -1.0f) baseIsFlatAndRound = false;
        const float d = std::sqrt(v.position[0] * v.position[0] + v.position[2] * v.position[2]);
        if (std::fabs(d - 1.0f) > kEpsilon) baseIsFlatAndRound = false;
    }
    r.check("cone_base_ring_is_flat_and_circular", baseIsFlatAndRound);
}

void testConePicking(Recorder& r) {
    // Bottom diameter 2.0 m, height 2.0 m: base radius 1.0 at y = -1, apex at
    // y = +1, so the exact radius at height y is (1 - y)/2.
    const ConstructionMesh mesh = coneMesh(2.0, 2.0);
    const TriangleMeshView view = viewOf(mesh);
    // A tessellated cone is inscribed in the exact one, so a side hit lies
    // between radius * cos(pi/32) = 0.99518 and radius.
    const float inradius = 0.995f;

    // The side, at mid-height, where the exact radius is 0.5.
    const TriangleHit side =
        pickTriangleMesh(makeRay(Vec3{10.0f, 0.0f, 0.03f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("cone_side_is_hit",
            side.hit && side.position.x <= 0.5f + kEpsilon &&
                side.position.x >= 0.5f * inradius - 0.05f);

    // The base, from below. This is what proves the base is closed rather than
    // merely wound: an open cone would let the ray through.
    const TriangleHit base =
        pickTriangleMesh(makeRay(Vec3{0.5f, -10.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}), view, true);
    r.check("cone_base_is_hit_from_below",
            base.hit && std::fabs(base.position.y + 1.0f) <= kEpsilon);
    const TriangleHit baseCentre =
        pickTriangleMesh(makeRay(Vec3{0.02f, -10.0f, 0.01f}, Vec3{0.0f, 1.0f, 0.0f}), view, true);
    r.check("cone_base_is_closed_at_its_centre",
            baseCentre.hit && std::fabs(baseCentre.position.y + 1.0f) <= kEpsilon);

    // Outside the flare must MISS: a ray straight down at 1.5 m from the axis
    // passes a cone whose widest radius is 1.0.
    const TriangleHit beside =
        pickTriangleMesh(makeRay(Vec3{1.5f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("cone_ray_outside_the_base_radius_misses", !beside.hit);

    // A cone is not a cylinder: at mid-height it is only 0.5 m in radius, so a
    // ray that would hit a 1.0 m cylinder there misses this.
    const TriangleHit taper =
        pickTriangleMesh(makeRay(Vec3{10.0f, 0.0f, 0.8f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("cone_tapers_so_a_cylinder_ray_misses", !taper.hit);

    // Front-face-only picking: a ray fired from inside must miss.
    const TriangleHit inside =
        pickTriangleMesh(makeRay(Vec3{0.0f, -0.5f, 0.0f}, Vec3{0.3f, 0.2f, 0.31f}), view, true);
    r.check("cone_front_face_only_ray_from_inside_misses", !inside.hit);

    // Picking follows the authoritative parameters with no separate collision
    // representation: a narrower cone is missed where the wider one was hit.
    const ConstructionMesh narrow = coneMesh(0.6, 2.0);
    const TriangleHit past = pickTriangleMesh(
        makeRay(Vec3{0.8f, -10.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}), viewOf(narrow), true);
    r.check("narrower_cone_is_missed_where_the_wider_one_was_hit", !past.hit);
}

void testTransformedConePicking(Recorder& r) {
    const ConstructionMesh mesh = coneMesh(2.0, 2.0);
    const TriangleMeshView view = viewOf(mesh);

    ConstructionTransform transform;
    TransformValues placement;
    placement.positionX = 5.0;
    placement.positionY = 0.5;
    transform.setValues(placement);

    // The ray moves into local space; the mesh never does.
    Ray local{};
    const bool moved = transformRayToLocal(
        makeRay(Vec3{5.02f, 10.0f, 0.01f}, Vec3{0.0f, -1.0f, 0.0f}), transform.inverseModelMatrix(),
        &local);
    const TriangleHit hit = pickTriangleMesh(local, view, true);
    const Vec3 world = mat4TransformPoint(transform.modelMatrix(), hit.position);
    r.check("transformed_cone_is_hit_near_its_moved_apex",
            moved && hit.hit && world.y > 0.5f + 0.9f && world.y <= 0.5f + 1.0f + kEpsilon);

    Ray atOrigin{};
    transformRayToLocal(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                        transform.inverseModelMatrix(), &atOrigin);
    r.check("moved_cone_leaves_its_old_place_empty",
            !pickTriangleMesh(atOrigin, view, true).hit);

    // Rotated 180 degrees about Z the cone points DOWN, so the apex is at the
    // bottom and the wide base is at the top — a rotation a sphere could not
    // show and a cone can.
    ConstructionTransform flipped;
    TransformValues spun;
    spun.rotationZ = 180.0;
    flipped.setValues(spun);
    Ray localFlipped{};
    const bool ok = transformRayToLocal(makeRay(Vec3{0.8f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                                        flipped.inverseModelMatrix(), &localFlipped);
    const TriangleHit flippedHit = pickTriangleMesh(localFlipped, view, true);
    const Vec3 flippedWorld = mat4TransformPoint(flipped.modelMatrix(), flippedHit.position);
    r.check("flipped_cone_is_hit_on_its_now_upward_base",
            ok && flippedHit.hit && std::fabs(flippedWorld.y - 1.0f) <= kEpsilon);
    // The same ray meets the UNROTATED cone somewhere else entirely: 0.8 m from
    // the axis it strikes the sloped flank, where the exact radius (1 - y)/2 is
    // 0.8, so y = -0.6 — not the flat surface at +1.0 the flipped one presents.
    // That difference is the rotation showing in the geometry rather than in the
    // matrix.
    const TriangleHit upright =
        pickTriangleMesh(makeRay(Vec3{0.8f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("unflipped_cone_is_hit_far_lower_on_its_sloped_flank",
            upright.hit && upright.position.y < -0.55f && upright.position.y > -0.65f &&
                flippedHit.hit && (flippedWorld.y - upright.position.y) > 1.5f);
}

// ---------------------------------------------------------------------------
// CC-04  Capsule geometry
// ---------------------------------------------------------------------------

void testCapsuleTopology(Recorder& r) {
    const ConstructionMesh mesh = capsuleMesh(1.0, 3.0);
    const uint32_t n = kCapsuleRadialSegments;

    r.check("capsule_vertex_count_is_rings_times_n_plus_2",
            mesh.vertices.size() == kCapsuleVertexCount &&
                kCapsuleVertexCount == kCapsuleRingCount * n + 2);
    r.check("capsule_index_count_is_6nr",
            mesh.indices.size() == kCapsuleIndexCount &&
                kCapsuleIndexCount == 6 * n * kCapsuleRingCount);
    r.check("capsule_counts_are_deterministic_for_a_given_shape",
            capsuleMesh(0.05, 0.2).vertices.size() == mesh.vertices.size() &&
                capsuleMesh(0.05, 0.2).indices.size() == mesh.indices.size() &&
                capsuleMesh(40.0, 100.0).vertices.size() == mesh.vertices.size() &&
                capsuleMesh(40.0, 100.0).indices.size() == mesh.indices.size());
    r.check("capsule_positions_all_finite", allPositionsFinite(mesh));
    r.check("capsule_indices_in_range", allIndicesInRange(mesh));
    r.check("capsule_shares_the_one_tessellation_owner",
            kCapsuleRadialSegments == kPrimitiveRadialSegments &&
                kCapsuleHemisphereBands == kPrimitiveLatitudeStacks / 2 &&
                kCapsuleRadialSegments % 4 == 0);

    // Reference counts, exactly as for the sphere: a pole anchors one fan
    // triangle per meridian, a ring beside a pole is used 5 times (3 by its one
    // band, 2 by the fan) and an interior ring 6 times (3 by each of two bands).
    std::vector<uint32_t> uses(mesh.vertices.size(), 0);
    for (uint32_t i : mesh.indices) {
        if (i < uses.size()) ++uses[i];
    }
    const uint32_t rings = kCapsuleRingCount;
    r.check("capsule_north_pole_is_a_closed_fan", uses[rings * n] == n);
    r.check("capsule_south_pole_is_a_closed_fan", uses[rings * n + 1] == n);
    bool polarRingSharing = true;
    for (uint32_t j = 0; j < n; ++j) {
        if (uses[j] != 5) polarRingSharing = false;
        if (uses[(rings - 1) * n + j] != 5) polarRingSharing = false;
    }
    r.check("capsule_rings_beside_the_poles_are_shared_by_band_and_fan", polarRingSharing);
    bool interiorRingSharing = true;
    for (uint32_t ring = 1; ring + 1 < rings; ++ring) {
        for (uint32_t j = 0; j < n; ++j) {
            if (uses[ring * n + j] != 6) interiorRingSharing = false;
        }
    }
    r.check("capsule_interior_ring_vertices_are_shared_by_two_bands", interiorRingSharing);
    uint32_t totalUses = 0;
    for (uint32_t count : uses) {
        totalUses += count;
    }
    r.check("capsule_vertex_uses_account_for_every_index", totalUses == kCapsuleIndexCount);

    r.check("capsule_mesh_passes_runtime_validation",
            validateMeshData(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                             mesh.indices.data(),
                             static_cast<uint32_t>(mesh.indices.size())) == MeshValidation::Ok);

    size_t edges = 0;
    const bool closed = isClosedAndOriented(mesh, &edges);
    r.check("capsule_is_closed_and_consistently_oriented", closed);
    r.check("capsule_euler_characteristic_is_2",
            closed && (mesh.vertices.size() + mesh.indices.size() / 3) == (edges + 2));
}

// The equality case: a capsule as tall as it is wide is a sphere, and it is
// generated as one rather than as a capsule with a zero-height band in it.
void testCapsuleEqualityCase(Recorder& r) {
    const ConstructionMesh mesh = capsuleMesh(2.0, 2.0);

    r.check("capsule_with_no_middle_has_the_spherical_counts",
            mesh.vertices.size() == kCapsuleSphericalVertexCount &&
                mesh.indices.size() == kCapsuleSphericalIndexCount);
    r.check("capsule_with_no_middle_matches_the_sphere_topology_exactly",
            kCapsuleSphericalVertexCount == kSphereVertexCount &&
                kCapsuleSphericalIndexCount == kSphereIndexCount);
    r.check("capsule_with_no_middle_has_no_degenerate_triangle", noDegenerateTriangles(mesh));
    r.check("capsule_with_no_middle_has_finite_positions_and_valid_indices",
            allPositionsFinite(mesh) && allIndicesInRange(mesh));

    // No duplicated ring: every vertex position is distinct from every other's.
    // A duplicated seam ring would show up here as N coincident pairs.
    bool allDistinct = true;
    for (size_t i = 0; i < mesh.vertices.size() && allDistinct; ++i) {
        for (size_t j = i + 1; j < mesh.vertices.size(); ++j) {
            const MeshVertex& a = mesh.vertices[i];
            const MeshVertex& b = mesh.vertices[j];
            if (a.position[0] == b.position[0] && a.position[1] == b.position[1] &&
                a.position[2] == b.position[2]) {
                allDistinct = false;
                break;
            }
        }
    }
    r.check("capsule_with_no_middle_has_no_duplicated_zero_length_ring", allDistinct);

    size_t edges = 0;
    const bool closed = isClosedAndOriented(mesh, &edges);
    r.check("capsule_with_no_middle_is_closed_and_oriented", closed);
    r.check("capsule_with_no_middle_is_spherical",
            meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, 0.0f, 0.0f}));

    // And it really is a sphere: every vertex is one radius from the centre.
    bool onTheSurface = true;
    for (const MeshVertex& v : mesh.vertices) {
        const float d = lengthOf(Vec3{v.position[0], v.position[1], v.position[2]});
        if (std::fabs(d - 1.0f) > kEpsilon) onTheSurface = false;
    }
    r.check("capsule_with_no_middle_is_one_radius_everywhere", onTheSurface);

    // A capsule barely taller than it is wide DOES have a middle, so the two
    // topologies are the two sides of one exact comparison and nothing in
    // between.
    const ConstructionMesh barely = capsuleMesh(2.0, 2.0000001);
    r.check("capsule_barely_taller_than_wide_has_a_middle",
            barely.vertices.size() == kCapsuleVertexCount &&
                barely.indices.size() == kCapsuleIndexCount &&
                noDegenerateTriangles(barely));
}

void testCapsuleBoundsAndWinding(Recorder& r) {
    struct Case {
        const char* boundsName;
        const char* windingName;
        const char* degenerateName;
        Meters diameter;
        Meters totalHeight;
    };
    const Case cases[] = {
        {"capsule_bounds_default", "capsule_winding_default", "capsule_no_degenerate_default",
         kDefaultCapsuleDiameterMeters, kDefaultCapsuleTotalHeightMeters},
        {"capsule_bounds_1x3", "capsule_winding_1x3", "capsule_no_degenerate_1x3", 1.0, 3.0},
        {"capsule_bounds_equal", "capsule_winding_equal", "capsule_no_degenerate_equal", 2.0, 2.0},
        {"capsule_bounds_tiny", "capsule_winding_tiny", "capsule_no_degenerate_tiny", 0.05, 0.2},
        {"capsule_bounds_long", "capsule_winding_long", "capsule_no_degenerate_long", 0.5, 40.0},
        {"capsule_bounds_large", "capsule_winding_large", "capsule_no_degenerate_large", 40.0,
         100.0},
    };

    for (const Case& c : cases) {
        const ConstructionMesh mesh = capsuleMesh(c.diameter, c.totalHeight);
        const float radius = static_cast<float>(c.diameter * 0.5);
        const float halfTotal = static_cast<float>(c.totalHeight * 0.5);

        Bounds b{};
        const bool ok = boundsOf(mesh, &b);
        // The seam rings are written down at exactly sinTheta = 1, and the poles
        // straight from the authoritative TOTAL height, so all six bounds are
        // exact rather than a rounding away.
        r.check(c.boundsName, ok && b.maxAxis[0] == radius && b.minAxis[0] == -radius &&
                                  b.maxAxis[2] == radius && b.minAxis[2] == -radius &&
                                  b.maxAxis[1] == halfTotal && b.minAxis[1] == -halfTotal);
        r.check(c.windingName, meshObeysCanonicalWinding(viewOf(mesh), Vec3{0.0f, 0.0f, 0.0f}));
        r.check(c.degenerateName, noDegenerateTriangles(mesh));
    }

    // Deterministic samples on the two seam rings of a 1.0 x 3.0 capsule: the
    // middle is 2.0 long, so the seams are at y = +/- 1.0 and both are at the
    // full 0.5 radius on the cardinal meridians.
    const ConstructionMesh mesh = capsuleMesh(1.0, 3.0);
    const uint32_t n = kCapsuleRadialSegments;
    const uint32_t topSeam = (kCapsuleHemisphereBands - 1) * n;
    const uint32_t bottomSeam = kCapsuleHemisphereBands * n;
    r.check("capsule_top_seam_plus_x_is_exact",
            mesh.vertices[topSeam].position[0] == 0.5f &&
                mesh.vertices[topSeam].position[1] == 1.0f &&
                mesh.vertices[topSeam].position[2] == 0.0f);
    r.check("capsule_bottom_seam_plus_x_is_exact",
            mesh.vertices[bottomSeam].position[0] == 0.5f &&
                mesh.vertices[bottomSeam].position[1] == -1.0f &&
                mesh.vertices[bottomSeam].position[2] == 0.0f);
    r.check("capsule_seam_plus_z_is_exact",
            mesh.vertices[topSeam + n / 4].position[0] == 0.0f &&
                mesh.vertices[topSeam + n / 4].position[2] == 0.5f);
    r.check("capsule_poles_are_at_the_total_height",
            mesh.vertices[kCapsuleRingCount * n].position[1] == 1.5f &&
                mesh.vertices[kCapsuleRingCount * n + 1].position[1] == -1.5f);

    // Every vertex is exactly one radius from its own hemisphere's centre, or on
    // the middle's surface: that is what makes the ends hemispheres rather than
    // a squashed sphere stretched along Y.
    bool endsAreHemispheres = true;
    for (uint32_t ring = 0; ring < kCapsuleRingCount; ++ring) {
        const float centreY = ring < kCapsuleHemisphereBands ? 1.0f : -1.0f;
        for (uint32_t j = 0; j < n; ++j) {
            const MeshVertex& v = mesh.vertices[ring * n + j];
            const float d = lengthOf(
                Vec3{v.position[0], v.position[1] - centreY, v.position[2]});
            if (std::fabs(d - 0.5f) > kEpsilon) endsAreHemispheres = false;
        }
    }
    r.check("capsule_ends_are_true_hemispheres_about_the_seam_centres", endsAreHemispheres);
}

void testCapsulePicking(Recorder& r) {
    // Diameter 1.0 m, total height 3.0 m: radius 0.5, middle 2.0 long, seams at
    // y = +/- 1.0, poles at y = +/- 1.5.
    const ConstructionMesh mesh = capsuleMesh(1.0, 3.0);
    const TriangleMeshView view = viewOf(mesh);
    const float radius = 0.5f;
    const float inradius = radius * 0.99f;

    // The cylindrical middle, at the waist.
    const TriangleHit middle =
        pickTriangleMesh(makeRay(Vec3{10.0f, 0.0f, 0.02f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("capsule_middle_is_hit",
            middle.hit && middle.position.x <= radius + kEpsilon &&
                middle.position.x >= inradius - kEpsilon);

    // The middle really is cylindrical: the same ray at three different heights
    // inside it hits at the same distance from the axis.
    const TriangleHit low =
        pickTriangleMesh(makeRay(Vec3{10.0f, -0.9f, 0.02f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    const TriangleHit high =
        pickTriangleMesh(makeRay(Vec3{10.0f, 0.9f, 0.02f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("capsule_middle_is_a_true_cylinder",
            low.hit && high.hit && std::fabs(low.position.x - high.position.x) <= kEpsilon &&
                std::fabs(low.position.x - middle.position.x) <= kEpsilon);

    // A hemispherical end, from above.
    const TriangleHit pole =
        pickTriangleMesh(makeRay(Vec3{0.02f, 10.0f, 0.01f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("capsule_hemisphere_is_hit_at_the_total_height",
            pole.hit && pole.position.y > 1.5f * 0.99f && pole.position.y <= 1.5f + kEpsilon);

    // The end is ROUND, not a flat cap: half way up the top hemisphere
    // (y = 1.25) the exact radius is sqrt(0.5^2 - 0.25^2) = 0.433, materially
    // less than the middle's 0.5.
    const TriangleHit shoulder =
        pickTriangleMesh(makeRay(Vec3{10.0f, 1.25f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("capsule_end_is_round_not_flat",
            shoulder.hit && shoulder.position.x < radius - 0.03f && shoulder.position.x > 0.40f);

    // Misses: beside the silhouette, and past the total height.
    const TriangleHit beside =
        pickTriangleMesh(makeRay(Vec3{0.6f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true);
    r.check("capsule_ray_outside_the_radius_misses", !beside.hit);
    const TriangleHit above =
        pickTriangleMesh(makeRay(Vec3{10.0f, 1.6f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("capsule_ray_above_the_total_height_misses", !above.hit);

    // A capsule is not a cylinder: at y = 1.4, well inside a 3.0 m cylinder's
    // side, the capsule has tapered to a radius of 0.28.
    const TriangleHit taper =
        pickTriangleMesh(makeRay(Vec3{10.0f, 1.4f, 0.45f}, Vec3{-1.0f, 0.0f, 0.0f}), view, true);
    r.check("capsule_tapers_so_a_cylinder_ray_misses", !taper.hit);

    // Front-face-only picking: a ray fired from inside must miss.
    const TriangleHit inside =
        pickTriangleMesh(makeRay(Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.3f, 0.9f, 0.31f}), view, true);
    r.check("capsule_front_face_only_ray_from_inside_misses", !inside.hit);
}

void testTransformedCapsulePicking(Recorder& r) {
    const ConstructionMesh mesh = capsuleMesh(1.0, 3.0);
    const TriangleMeshView view = viewOf(mesh);

    ConstructionTransform transform;
    TransformValues placement;
    placement.positionX = 5.0;
    placement.positionY = 0.5;
    transform.setValues(placement);

    Ray local{};
    const bool moved = transformRayToLocal(
        makeRay(Vec3{5.02f, 10.0f, 0.01f}, Vec3{0.0f, -1.0f, 0.0f}), transform.inverseModelMatrix(),
        &local);
    const TriangleHit hit = pickTriangleMesh(local, view, true);
    const Vec3 world = mat4TransformPoint(transform.modelMatrix(), hit.position);
    r.check("transformed_capsule_is_hit_at_its_moved_top",
            moved && hit.hit && world.y > 0.5f + 1.5f * 0.99f &&
                world.y <= 0.5f + 1.5f + kEpsilon);

    Ray atOrigin{};
    transformRayToLocal(makeRay(Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                        transform.inverseModelMatrix(), &atOrigin);
    r.check("moved_capsule_leaves_its_old_place_empty",
            !pickTriangleMesh(atOrigin, view, true).hit);

    // Laid on its side by Rz(90), the capsule is 3.0 m along world X and 1.0 m
    // along world Y — the transform is real and the picker follows it.
    ConstructionTransform lying;
    TransformValues spun;
    spun.rotationZ = 90.0;
    lying.setValues(spun);
    Ray localLying{};
    const bool ok = transformRayToLocal(makeRay(Vec3{1.4f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}),
                                        lying.inverseModelMatrix(), &localLying);
    const TriangleHit lyingHit = pickTriangleMesh(localLying, view, true);
    r.check("capsule_laid_on_its_side_is_hit_1_4_m_along_x", ok && lyingHit.hit);
    // The same ray misses the upright capsule, which is only 0.5 m wide.
    r.check("upright_capsule_is_missed_by_the_same_ray",
            !pickTriangleMesh(makeRay(Vec3{1.4f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}), view, true)
                 .hit);
}

// ---------------------------------------------------------------------------
// CC-09 / CC-10  Transform independence and the cross-kind round trip
// ---------------------------------------------------------------------------

void testIdentityTransformAndRememberedValuesSurvive(Recorder& r) {
    ConstructionObject object;
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
    object.transform().setValues(placement);
    const uint64_t transformUpdatesBefore = object.transform().updateCount();

    // Give every primitive a non-default remembered value, then walk the whole
    // cycle back to the start.
    applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
    applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
    applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));

    r.check("object_id_is_stable_across_all_five_kinds", object.objectId() == idBefore);
    const TransformValues after = object.transform().values();
    r.check("transform_survives_the_whole_five_kind_round_trip",
            after.positionX == 0.75 && after.positionY == -1.25 && after.positionZ == 0.5 &&
                after.rotationX == 15.0 && after.rotationY == 30.0 && after.rotationZ == 90.0);
    r.check("no_shape_change_counted_as_a_transform_update",
            object.transform().updateCount() == transformUpdatesBefore);
    r.check("every_primitives_typed_values_are_remembered",
            object.box().widthMeters() == 1.25 && object.box().heightMeters() == 2.5 &&
                object.box().depthMeters() == 0.75 && object.cylinder().diameterMeters() == 1.2 &&
                object.cylinder().heightMeters() == 2.4 && object.sphere().diameterMeters() == 1.5 &&
                object.cone().bottomDiameterMeters() == 2.0 && object.cone().heightMeters() == 3.0 &&
                object.capsule().diameterMeters() == 1.0 &&
                object.capsule().totalHeightMeters() == 3.0);
    r.check("round_trip_back_to_the_box_is_unchanged_the_second_time",
            applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75)).status ==
                PrimitiveUpdateStatus::Unchanged);

    // A transform-only edit publishes NOTHING, for a cone and a capsule exactly
    // as for every other primitive.
    for (int i = 0; i < 2; ++i) {
        applyPrimitive(object, store,
                       i == 0 ? PrimitiveSpec::forCone(2.0, 3.0)
                              : PrimitiveSpec::forCapsule(1.0, 3.0));
        const MeshRevision before = store.currentRevision();
        const uint64_t publishedBefore = store.publishedCount();
        TransformValues moved = object.transform().values();
        moved.positionX += 1.0;
        moved.rotationY += 10.0;
        object.transform().setValues(moved);
        r.check(i == 0 ? "cone_transform_edit_publishes_no_revision"
                       : "capsule_transform_edit_publishes_no_revision",
                store.currentRevision() == before && store.publishedCount() == publishedBefore);
    }
}

// ---------------------------------------------------------------------------
// REG-01  The three older primitives, through the now five-way boundary
// ---------------------------------------------------------------------------

void testOlderPrimitivesStillBehave(Recorder& r) {
    ConstructionObject object;
    MeshStore store(kConstructionBoxObjectId);
    publishConstructionObject(object, store);
    r.check("object_still_publishes_box_topology_at_startup",
            store.current() != nullptr && store.current()->vertexCount() == kBoxVertexCount &&
                store.current()->indexCount() == kBoxIndexCount);

    const PrimitiveApplyResult cylinder =
        applyPrimitive(object, store, PrimitiveSpec::forCylinder(1.2, 2.4));
    r.check("cylinder_topology_is_unchanged_by_this_stage",
            cylinder.vertexCount == kCylinderVertexCount &&
                cylinder.indexCount == kCylinderIndexCount &&
                kCylinderVertexCount == 66 && kCylinderIndexCount == 384);

    const PrimitiveApplyResult sphere =
        applyPrimitive(object, store, PrimitiveSpec::forSphere(1.5));
    r.check("sphere_topology_is_unchanged_by_this_stage",
            sphere.vertexCount == kSphereVertexCount && sphere.indexCount == kSphereIndexCount &&
                kSphereVertexCount == 482 && kSphereIndexCount == 2880);

    const PrimitiveApplyResult box =
        applyPrimitive(object, store, PrimitiveSpec::forBox(1.25, 2.5, 0.75));
    r.check("box_topology_is_unchanged_by_this_stage",
            box.vertexCount == kBoxVertexCount && box.indexCount == kBoxIndexCount);

    // Sharing one tessellation owner must not have moved any of them.
    r.check("shared_tessellation_owner_did_not_move_the_old_primitives",
            kCylinderRadialSegments == 32 && kSphereRadialSegments == 32 && kSphereStacks == 16 &&
                kSphereRingCount == 15);

    // One object still generates every topology from its own parameters.
    applyPrimitive(object, store, PrimitiveSpec::forCone(2.0, 3.0));
    const size_t coneVertices = object.generateMesh().vertices.size();
    applyPrimitive(object, store, PrimitiveSpec::forCapsule(1.0, 3.0));
    const size_t capsuleVertices = object.generateMesh().vertices.size();
    r.check("one_object_generates_all_five_topologies",
            coneVertices == kConeVertexCount && capsuleVertices == kCapsuleVertexCount);
}

}  // namespace

int runConeCapsuleSelfTests(ConeCapsuleSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testTypedConeAndCapsulePayloads(r);
    testTypedRequestsStillCannotCrossPrimitives(r);
    testConeApplySemantics(r);
    testCapsuleApplySemantics(r);
    testInvalidConeAndCapsuleFailClosed(r);
    testConeTopology(r);
    testConeBoundsWindingAndDegeneracy(r);
    testConePicking(r);
    testTransformedConePicking(r);
    testCapsuleTopology(r);
    testCapsuleEqualityCase(r);
    testCapsuleBoundsAndWinding(r);
    testCapsulePicking(r);
    testTransformedCapsulePicking(r);
    testIdentityTransformAndRememberedValuesSurvive(r);
    testOlderPrimitivesStillBehave(r);
    return r.n;
}

}  // namespace forgeshape
