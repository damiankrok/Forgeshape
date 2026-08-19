#include "forgeshape_mesh_selftest.h"

#include <cmath>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_demo_mesh.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_mesh_fixtures.h"
#include "forgeshape_picking.h"
#include "forgeshape_selection.h"

namespace forgeshape {
namespace {

constexpr int kTestViewportWidth = 1080;
constexpr int kTestViewportHeight = 2400;
constexpr float kCenterX = kTestViewportWidth * 0.5f;
constexpr float kCenterY = kTestViewportHeight * 0.5f;

struct Recorder {
    MeshSelfTestResult* out;
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

CameraController makeController() {
    CameraController c;
    c.setViewport(kTestViewportWidth, kTestViewportHeight);
    return c;
}

bool allPositionsFinite(const FixtureMesh& mesh) {
    for (const MeshVertex& v : mesh.vertices) {
        if (!std::isfinite(v.position[0]) || !std::isfinite(v.position[1]) ||
            !std::isfinite(v.position[2])) {
            return false;
        }
    }
    return true;
}

bool allIndicesInRange(const FixtureMesh& mesh) {
    for (uint32_t i : mesh.indices) {
        if (i >= mesh.vertices.size()) {
            return false;
        }
    }
    return true;
}

TriangleMeshView viewOf(const FixtureMesh& mesh) {
    TriangleMeshView view{};
    view.positions = mesh.vertices.empty() ? nullptr : mesh.vertices[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    view.indices = mesh.indices.empty() ? nullptr : mesh.indices.data();
    view.indexCount = static_cast<uint32_t>(mesh.indices.size());
    return view;
}

// A ray through the middle of the viewport with the default camera pose. Every
// fixture straddles the world origin, so all of them are hit by it.
bool centerRay(Ray* out) {
    CameraController c = makeController();
    return buildPickRay(c.snapshot(), kCenterX, kCenterY, kTestViewportWidth,
                        kTestViewportHeight, out);
}

// ---------------------------------------------------------------------------
// CPU mesh validation
// ---------------------------------------------------------------------------
void testValidation(Recorder& r) {
    const FixtureMesh baseline = buildFixtureBaseline();
    const MeshVertex* v = baseline.vertices.data();
    const uint32_t vc = static_cast<uint32_t>(baseline.vertices.size());
    const uint32_t* idx = baseline.indices.data();
    const uint32_t ic = static_cast<uint32_t>(baseline.indices.size());

    r.check("validate_accepts_valid_triangle_mesh",
            validateMeshData(v, vc, idx, ic) == MeshValidation::Ok);

    std::vector<MeshVertex> nonFinite(baseline.vertices);
    nonFinite[3].position[1] = NAN;
    r.check("validate_rejects_non_finite_vertex",
            validateMeshData(nonFinite.data(), vc, idx, ic) == MeshValidation::NonFinitePosition);

    std::vector<MeshVertex> infinite(baseline.vertices);
    infinite[5].position[2] = INFINITY;
    r.check("validate_rejects_infinite_vertex",
            validateMeshData(infinite.data(), vc, idx, ic) == MeshValidation::NonFinitePosition);

    std::vector<uint32_t> outOfRange(baseline.indices);
    outOfRange[7] = vc;  // one past the last valid vertex
    r.check("validate_rejects_out_of_range_index",
            validateMeshData(v, vc, outOfRange.data(), ic) == MeshValidation::IndexOutOfRange);

    r.check("validate_rejects_ragged_index_count",
            validateMeshData(v, vc, idx, ic - 1) == MeshValidation::IndexCountNotTriangles);

    r.check("validate_rejects_null_vertices",
            validateMeshData(nullptr, vc, idx, ic) == MeshValidation::NullData);
    r.check("validate_rejects_null_indices",
            validateMeshData(v, vc, nullptr, ic) == MeshValidation::NullData);
    r.check("validate_rejects_zero_vertices",
            validateMeshData(v, 0, idx, ic) == MeshValidation::EmptyVertices);
    r.check("validate_rejects_zero_indices",
            validateMeshData(v, vc, idx, 0) == MeshValidation::EmptyIndices);
    r.check("validate_rejects_too_many_vertices",
            validateMeshData(v, kMaxMeshVertices + 1, idx, ic) == MeshValidation::TooLarge);

    // The factory is the only way to make a mesh, and it fails closed.
    MeshValidation why = MeshValidation::Ok;
    r.check("create_rejects_invalid_mesh",
            createRuntimeMesh(kDemoCubeObjectId, 1, v, vc, outOfRange.data(), ic, &why) == nullptr &&
                why == MeshValidation::IndexOutOfRange);
    r.check("create_rejects_reserved_revision_zero",
            createRuntimeMesh(kDemoCubeObjectId, kNoMeshRevision, v, vc, idx, ic) == nullptr);

    const RuntimeMeshPtr good = createRuntimeMesh(kDemoCubeObjectId, 7, v, vc, idx, ic, &why);
    r.check("create_accepts_valid_mesh", good != nullptr && why == MeshValidation::Ok);
    r.check("created_mesh_reports_counts",
            good && good->vertexCount() == vc && good->indexCount() == ic &&
                good->triangleCount() == ic / 3);
    r.check("created_mesh_reports_byte_sizes",
            good && good->vertexBytes() == vc * sizeof(MeshVertex) &&
                good->indexBytes() == ic * sizeof(uint32_t));
    r.check("created_mesh_keeps_object_id", good && good->objectId() == kDemoCubeObjectId);
    r.check("created_mesh_keeps_revision", good && good->revision() == 7);
    // Immutability: the mesh copied the data, it did not alias the caller's.
    r.check("created_mesh_copies_data", good && good->vertices() != v && good->indices() != idx);
}

// ---------------------------------------------------------------------------
// Revision rules
// ---------------------------------------------------------------------------
void testRevisions(Recorder& r) {
    MeshStore store(kDemoCubeObjectId);
    const FixtureMesh baseline = buildFixtureBaseline();
    const FixtureMesh deformed = buildFixtureDeformed();

    r.check("store_starts_empty",
            store.currentRevision() == kNoMeshRevision && store.current() == nullptr);

    const MeshRevision r1 = store.publish(baseline.vertices.data(),
                                          static_cast<uint32_t>(baseline.vertices.size()),
                                          baseline.indices.data(),
                                          static_cast<uint32_t>(baseline.indices.size()));
    const MeshRevision r2 = store.publish(deformed.vertices.data(),
                                          static_cast<uint32_t>(deformed.vertices.size()),
                                          deformed.indices.data(),
                                          static_cast<uint32_t>(deformed.indices.size()));
    r.check("store_first_revision_is_not_zero", r1 != kNoMeshRevision);
    r.check("store_revisions_are_monotonic", r2 > r1);
    r.check("store_current_is_latest", store.currentRevision() == r2);
    r.check("store_published_count", store.publishedCount() == 2);

    // Invalid input fails closed: the current revision must not move.
    std::vector<uint32_t> bad(baseline.indices);
    bad[0] = 9999;
    MeshValidation why = MeshValidation::Ok;
    const MeshRevision rejected =
        store.publish(baseline.vertices.data(), static_cast<uint32_t>(baseline.vertices.size()),
                      bad.data(), static_cast<uint32_t>(bad.size()), &why);
    r.check("store_rejects_invalid_publish", rejected == kNoMeshRevision);
    r.check("store_reports_rejection_reason", why == MeshValidation::IndexOutOfRange);
    r.check("store_keeps_previous_revision_after_rejection", store.currentRevision() == r2);
    r.check("store_rejected_count", store.rejectedCount() == 1);

    // A stale snapshot can never replace a newer one.
    const RuntimeMeshPtr stale =
        createRuntimeMesh(kDemoCubeObjectId, r1, baseline.vertices.data(),
                          static_cast<uint32_t>(baseline.vertices.size()), baseline.indices.data(),
                          static_cast<uint32_t>(baseline.indices.size()));
    r.check("store_rejects_stale_snapshot", stale && !store.publishSnapshot(stale));
    r.check("store_revision_unchanged_after_stale", store.currentRevision() == r2);

    const RuntimeMeshPtr same =
        createRuntimeMesh(kDemoCubeObjectId, r2, baseline.vertices.data(),
                          static_cast<uint32_t>(baseline.vertices.size()), baseline.indices.data(),
                          static_cast<uint32_t>(baseline.indices.size()));
    r.check("store_rejects_equal_revision_snapshot", same && !store.publishSnapshot(same));

    const RuntimeMeshPtr newer =
        createRuntimeMesh(kDemoCubeObjectId, r2 + 5, baseline.vertices.data(),
                          static_cast<uint32_t>(baseline.vertices.size()), baseline.indices.data(),
                          static_cast<uint32_t>(baseline.indices.size()));
    r.check("store_accepts_newer_snapshot", newer && store.publishSnapshot(newer));
    r.check("store_current_follows_newer_snapshot", store.currentRevision() == r2 + 5);

    // A later publish must still mint a revision above the adopted one.
    const MeshRevision r3 = store.publish(baseline.vertices.data(),
                                          static_cast<uint32_t>(baseline.vertices.size()),
                                          baseline.indices.data(),
                                          static_cast<uint32_t>(baseline.indices.size()));
    r.check("store_next_revision_exceeds_adopted", r3 > r2 + 5);

    r.check("store_rejects_null_snapshot", !store.publishSnapshot(nullptr));
    const RuntimeMeshPtr foreign =
        createRuntimeMesh(kDemoCubeObjectId + 99, r3 + 1, baseline.vertices.data(),
                          static_cast<uint32_t>(baseline.vertices.size()), baseline.indices.data(),
                          static_cast<uint32_t>(baseline.indices.size()));
    r.check("store_rejects_foreign_object_id", foreign && !store.publishSnapshot(foreign));

    // Stable identity does not move when the geometry does.
    r.check("object_id_survives_revision_changes",
            store.objectId() == kDemoCubeObjectId && store.current() &&
                store.current()->objectId() == kDemoCubeObjectId);

    // Latest-wins coalescing: after N publishes only the newest is observable.
    MeshRevision last = kNoMeshRevision;
    for (uint32_t i = 0; i < 10; ++i) {
        const FixtureMesh step = buildStressStep(i);
        last = store.publish(step.vertices.data(), static_cast<uint32_t>(step.vertices.size()),
                             step.indices.data(), static_cast<uint32_t>(step.indices.size()));
    }
    r.check("coalescing_observes_only_newest",
            last != kNoMeshRevision && store.currentRevision() == last);
    r.check("coalescing_keeps_object_id", store.current()->objectId() == kDemoCubeObjectId);
}

// ---------------------------------------------------------------------------
// GPU capacity policy (pure arithmetic; no Vulkan needed to prove it)
// ---------------------------------------------------------------------------
void testCapacityPolicy(Recorder& r) {
    uint64_t out = 0;

    r.check("capacity_grows_from_zero", growCapacityBytes(0, 1024, &out) && out >= 1024);

    r.check("capacity_reuses_on_equal_need", growCapacityBytes(4096, 4096, &out) && out == 4096);
    r.check("capacity_reuses_on_smaller_vertex_need",
            growCapacityBytes(4096, 192, &out) && out == 4096);
    r.check("capacity_reuses_on_smaller_index_need",
            growCapacityBytes(8192, 144, &out) && out == 8192);
    r.check("capacity_never_shrinks", growCapacityBytes(1 << 20, 16, &out) && out == (1 << 20));

    r.check("capacity_grows_on_larger_vertex_need",
            growCapacityBytes(4096, 9000, &out) && out >= 9000);
    r.check("capacity_grows_on_larger_index_need",
            growCapacityBytes(1024, 5000, &out) && out >= 5000);
    r.check("capacity_growth_is_at_least_one_and_a_half",
            growCapacityBytes(4096, 4097, &out) && out == 6144);

    r.check("capacity_sufficient_predicate_equal", capacityIsSufficient(4096, 4096));
    r.check("capacity_sufficient_predicate_smaller", capacityIsSufficient(4096, 4095));
    r.check("capacity_sufficient_predicate_larger", !capacityIsSufficient(4096, 4097));

    // Arithmetic can never overflow: anything past the hard cap is refused, and
    // the cap itself is still satisfiable.
    r.check("capacity_refuses_beyond_cap", !growCapacityBytes(0, kMaxMeshBytes + 1, &out));
    r.check("capacity_refuses_huge_need", !growCapacityBytes(0, UINT64_MAX, &out));
    r.check("capacity_refuses_corrupt_current", !growCapacityBytes(UINT64_MAX, 16, &out));
    r.check("capacity_allows_exact_cap",
            growCapacityBytes(0, kMaxMeshBytes, &out) && out == kMaxMeshBytes);
    r.check("capacity_clamps_growth_to_cap",
            growCapacityBytes(kMaxMeshBytes, kMaxMeshBytes, &out) && out == kMaxMeshBytes);
    r.check("capacity_rejects_null_out", !growCapacityBytes(0, 16, nullptr));
}

// ---------------------------------------------------------------------------
// Fixture correctness
// ---------------------------------------------------------------------------
void testFixtures(Recorder& r) {
    const FixtureMesh a = buildFixtureBaseline();
    const FixtureMesh b = buildFixtureDeformed();
    const FixtureMesh c = buildFixtureLarge();

    r.check("fixture_a_matches_bootstrap_cube",
            a.vertices.size() == demoCubeVertexCount() && a.indices.size() == demoCubeIndexCount());
    r.check("fixture_a_winding_is_canonical",
            meshObeysCanonicalWinding(viewOf(a), Vec3{0.0f, 0.0f, 0.0f}));

    r.check("fixture_b_same_vertex_count", b.vertices.size() == a.vertices.size());
    r.check("fixture_b_same_index_count", b.indices.size() == a.indices.size());
    r.check("fixture_b_winding_is_canonical",
            meshObeysCanonicalWinding(viewOf(b), Vec3{0.0f, 0.0f, 0.0f}));
    bool geometryChanged = false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].position[0] != b.vertices[i].position[0]) {
            geometryChanged = true;
        }
    }
    r.check("fixture_b_geometry_actually_changed", geometryChanged);

    r.check("fixture_c_has_more_vertices", c.vertices.size() > a.vertices.size() * 8);
    r.check("fixture_c_has_more_indices", c.indices.size() > a.indices.size() * 8);
    r.check("fixture_c_expected_counts",
            c.vertices.size() == 6u * (kFixtureLargeSubdivisions + 1) *
                                     (kFixtureLargeSubdivisions + 1) &&
                c.indices.size() ==
                    6u * kFixtureLargeSubdivisions * kFixtureLargeSubdivisions * 6u);
    r.check("fixture_c_winding_is_canonical",
            meshObeysCanonicalWinding(viewOf(c), Vec3{0.0f, 0.0f, 0.0f}));

    r.check("fixture_indices_all_in_range",
            allIndicesInRange(a) && allIndicesInRange(b) && allIndicesInRange(c));
    r.check("fixture_positions_all_finite",
            allPositionsFinite(a) && allPositionsFinite(b) && allPositionsFinite(c));
    r.check("fixture_index_counts_are_triangles",
            (a.indices.size() % 3) == 0 && (b.indices.size() % 3) == 0 &&
                (c.indices.size() % 3) == 0);
    r.check("fixture_all_validate",
            validateMeshData(a.vertices.data(), static_cast<uint32_t>(a.vertices.size()),
                             a.indices.data(), static_cast<uint32_t>(a.indices.size())) ==
                    MeshValidation::Ok &&
                validateMeshData(b.vertices.data(), static_cast<uint32_t>(b.vertices.size()),
                                 b.indices.data(), static_cast<uint32_t>(b.indices.size())) ==
                    MeshValidation::Ok &&
                validateMeshData(c.vertices.data(), static_cast<uint32_t>(c.vertices.size()),
                                 c.indices.data(), static_cast<uint32_t>(c.indices.size())) ==
                    MeshValidation::Ok);

    // Stress steps keep the baseline topology, which is what makes the repeated
    // update path exercise capacity REUSE rather than reallocation.
    bool stressStable = true;
    bool stressMoves = false;
    for (uint32_t step = 0; step < 60; ++step) {
        const FixtureMesh s = buildStressStep(step);
        if (s.vertices.size() != a.vertices.size() || s.indices.size() != a.indices.size() ||
            !allPositionsFinite(s) || !allIndicesInRange(s) ||
            !meshObeysCanonicalWinding(viewOf(s), Vec3{0.0f, 0.0f, 0.0f})) {
            stressStable = false;
        }
        if (s.vertices[6].position[0] != a.vertices[6].position[0]) {
            stressMoves = true;
        }
    }
    r.check("stress_steps_keep_topology_and_winding", stressStable);
    r.check("stress_steps_actually_move_vertices", stressMoves);

    // Deterministic camera: every fixture is hit through the middle of the view.
    Ray ray{};
    const bool rayOk = centerRay(&ray);
    r.check("center_ray_builds", rayOk);
    r.check("fixture_a_hit_from_center", rayOk && pickTriangleMesh(ray, viewOf(a), true).hit);
    r.check("fixture_b_hit_from_center", rayOk && pickTriangleMesh(ray, viewOf(b), true).hit);
    r.check("fixture_c_hit_from_center", rayOk && pickTriangleMesh(ray, viewOf(c), true).hit);
    // The three fixtures are genuinely different surfaces along the same ray,
    // and the large fixture's hit lies on its own spherified radius rather than
    // on the baseline cube.
    const TriangleHit hitA = rayOk ? pickTriangleMesh(ray, viewOf(a), true) : TriangleHit{};
    const TriangleHit hitB = rayOk ? pickTriangleMesh(ray, viewOf(b), true) : TriangleHit{};
    const TriangleHit hitC = rayOk ? pickTriangleMesh(ray, viewOf(c), true) : TriangleHit{};
    r.check("fixtures_differ_along_the_same_ray",
            hitA.hit && hitB.hit && hitC.hit && hitA.t != hitB.t && hitA.t != hitC.t);
    const float radiusC =
        hitC.hit ? std::sqrt(vec3Dot(hitC.position, hitC.position)) : 0.0f;
    r.check("fixture_c_hit_lies_on_its_own_surface",
            hitC.hit && radiusC > 1.20f && radiusC < 1.30f);
}

// ---------------------------------------------------------------------------
// Snapshot / picking coherence through the real pickScene path
// ---------------------------------------------------------------------------
void testCoherence(Recorder& r) {
    CameraController camera = makeController();
    const CameraSnapshot snapshot = camera.snapshot();
    MeshStore& store = meshStore();

    const FixtureMesh a = buildFixtureBaseline();
    const FixtureMesh b = buildFixtureDeformed();
    const FixtureMesh c = buildFixtureLarge();

    const MeshRevision ra =
        store.publish(a.vertices.data(), static_cast<uint32_t>(a.vertices.size()),
                      a.indices.data(), static_cast<uint32_t>(a.indices.size()));
    SceneHit hitA = pickScene(snapshot, kCenterX, kCenterY, kTestViewportWidth, kTestViewportHeight);
    r.check("pick_reads_baseline_revision", ra != kNoMeshRevision && hitA.hit);
    r.check("pick_baseline_object_id_is_stable", hitA.objectId == kDemoCubeObjectId);

    const MeshRevision rb =
        store.publish(b.vertices.data(), static_cast<uint32_t>(b.vertices.size()),
                      b.indices.data(), static_cast<uint32_t>(b.indices.size()));
    SceneHit hitB = pickScene(snapshot, kCenterX, kCenterY, kTestViewportWidth, kTestViewportHeight);
    r.check("pick_reads_same_topology_revision", rb > ra && hitB.hit);
    r.check("pick_same_topology_object_id_is_stable", hitB.objectId == kDemoCubeObjectId);
    r.check("pick_sees_changed_geometry", hitB.hit && hitA.hit && hitB.distance != hitA.distance);

    const MeshRevision rc =
        store.publish(c.vertices.data(), static_cast<uint32_t>(c.vertices.size()),
                      c.indices.data(), static_cast<uint32_t>(c.indices.size()));
    SceneHit hitC = pickScene(snapshot, kCenterX, kCenterY, kTestViewportWidth, kTestViewportHeight);
    r.check("pick_reads_larger_replacement_revision", rc > rb && hitC.hit);
    r.check("pick_larger_object_id_is_stable", hitC.objectId == kDemoCubeObjectId);
    r.check("pick_larger_uses_updated_triangles",
            hitC.hit && hitC.triangleIndex >= 0 &&
                static_cast<size_t>(hitC.triangleIndex) < c.indices.size() / 3);

    // A snapshot held across a newer publish keeps reading its own revision:
    // that is what makes an in-progress pick safe.
    const RuntimeMeshPtr held = store.current();
    const uint32_t heldCount = held ? held->vertexCount() : 0;
    store.publish(a.vertices.data(), static_cast<uint32_t>(a.vertices.size()), a.indices.data(),
                  static_cast<uint32_t>(a.indices.size()));
    r.check("held_snapshot_is_immutable",
            held && held->vertexCount() == heldCount && held->revision() == rc);
    r.check("store_moved_on_after_held_snapshot", store.currentRevision() > rc);

    // Invalid publication fails closed and leaves picking working.
    std::vector<uint32_t> bad(a.indices);
    bad[2] = 12345;
    const MeshRevision rejected =
        store.publish(a.vertices.data(), static_cast<uint32_t>(a.vertices.size()), bad.data(),
                      static_cast<uint32_t>(bad.size()));
    const MeshRevision afterReject = store.currentRevision();
    SceneHit hitAfter =
        pickScene(snapshot, kCenterX, kCenterY, kTestViewportWidth, kTestViewportHeight);
    r.check("invalid_revision_fails_closed", rejected == kNoMeshRevision);
    r.check("pick_still_works_after_rejected_revision",
            hitAfter.hit && hitAfter.objectId == kDemoCubeObjectId && afterReject != kNoMeshRevision);

    // Selection identity is unaffected by any of it.
    SelectionController selection;
    selection.applyPick(hitA);
    const ObjectId afterBaseline = selection.selected();
    selection.applyPick(hitC);
    r.check("selection_id_unchanged_across_mesh_revisions",
            afterBaseline == kDemoCubeObjectId && selection.selected() == kDemoCubeObjectId);
    r.check("selection_change_count_stays_one",
            selection.changeCount() == 1);  // hitC did not change the identity
}

}  // namespace

int runMeshSelfTests(MeshSelfTestResult* out, int max) {
    if (out == nullptr || max <= 0) {
        return 0;
    }
    Recorder r{out, max};
    testValidation(r);
    testRevisions(r);
    testCapacityPolicy(r);
    testFixtures(r);
    testCoherence(r);
    return r.n;
}

}  // namespace forgeshape
