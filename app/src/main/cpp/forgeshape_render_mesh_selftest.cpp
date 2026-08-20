#include "forgeshape_render_mesh_selftest.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_display.h"
#include "forgeshape_matcap.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_render_mesh.h"

namespace forgeshape {
namespace {

struct Recorder {
    RenderMeshSelfTestResult* out;
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

// Positions and normals are float and pass through a cross product, a sum and a
// normalization, so a unit-length check carries a few ulps of accumulated
// rounding. 1e-4 is orders of magnitude above that floor and far tighter than
// any real grouping error, which would put a normal on a different axis
// entirely.
constexpr float kEpsilon = 1e-4f;

bool nearly(float a, float b) { return std::fabs(a - b) <= kEpsilon; }

Vec3 normalOf(const RenderVertex& v) { return Vec3{v.normal[0], v.normal[1], v.normal[2]}; }
Vec3 positionOf(const RenderVertex& v) { return Vec3{v.position[0], v.position[1], v.position[2]}; }

bool isUnit(const Vec3& v) { return nearly(std::sqrt(vec3Dot(v, v)), 1.0f); }

bool build(const ConstructionMesh& source, SurfaceShading shading, RenderMeshData* out) {
    return buildRenderMesh(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                           source.indices.data(), static_cast<uint32_t>(source.indices.size()),
                           shading, out);
}

// Every normal is unit length, finite, and never NaN. Zero normals are allowed
// by the general contract but no closed primitive should produce one, so this
// helper insists on unit — a zero here would mean degenerate triangles.
bool allNormalsUnit(const RenderMeshData& data) {
    for (const RenderVertex& v : data.vertices) {
        if (!isUnit(normalOf(v))) {
            return false;
        }
    }
    return true;
}

// True when the normal points along one coordinate axis and has no measurable
// component on the other two — the signature of a genuinely planar face.
bool isAxisAligned(const Vec3& n) {
    int strong = 0;
    if (!nearly(std::fabs(n.x), 0.0f)) ++strong;
    if (!nearly(std::fabs(n.y), 0.0f)) ++strong;
    if (!nearly(std::fabs(n.z), 0.0f)) ++strong;
    return strong == 1 && isUnit(n);
}

// How radial a normal is: 1 means it points straight out from the local origin,
// which is the exact analytic normal of a sphere centred there.
float radialAgreement(const RenderVertex& v) {
    const Vec3 p = positionOf(v);
    const float lengthSquared = vec3Dot(p, p);
    if (!(lengthSquared > 0.0f)) {
        return 0.0f;
    }
    return vec3Dot(normalOf(v), vec3Normalize(p));
}

// The smallest radial agreement anywhere on the mesh: the honest way to state
// "every vertex on this sphere shades as a sphere", rather than checking one.
float minimumRadialAgreement(const RenderMeshData& data) {
    float worst = 1.0f;
    for (const RenderVertex& v : data.vertices) {
        const float agreement = radialAgreement(v);
        if (agreement < worst) {
            worst = agreement;
        }
    }
    return worst;
}

uint32_t countNormalsWithAxisMagnitude(const RenderMeshData& data, int axis, float minimum) {
    uint32_t count = 0;
    for (const RenderVertex& v : data.vertices) {
        if (std::fabs(v.normal[axis]) >= minimum) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// Primitive fixtures, at their documented defaults
// ---------------------------------------------------------------------------

ConstructionMesh defaultBox() {
    ConstructionBox box;
    box.setDimensionsMeters(kDefaultBoxWidthMeters, kDefaultBoxHeightMeters,
                            kDefaultBoxDepthMeters);
    return box.generateMesh();
}

ConstructionMesh defaultCylinder() {
    ConstructionCylinder cylinder;
    cylinder.setDimensionsMeters(kDefaultCylinderDiameterMeters, kDefaultCylinderHeightMeters);
    return cylinder.generateMesh();
}

ConstructionMesh defaultSphere() {
    ConstructionSphere sphere;
    sphere.setDimensionsMeters(kDefaultSphereDiameterMeters);
    return sphere.generateMesh();
}

ConstructionMesh defaultCone() {
    ConstructionCone cone;
    cone.setDimensionsMeters(kDefaultConeBottomDiameterMeters, kDefaultConeHeightMeters);
    return cone.generateMesh();
}

ConstructionMesh defaultCapsule() {
    ConstructionCapsule capsule;
    capsule.setDimensionsMeters(kDefaultCapsuleDiameterMeters, kDefaultCapsuleTotalHeightMeters);
    return capsule.generateMesh();
}

// The equality case: totalHeight == diameter, which the generator produces as a
// sphere rather than as a capsule with a zero-height middle.
ConstructionMesh sphericalCapsule() {
    ConstructionCapsule capsule;
    capsule.setDimensionsMeters(1.0, 1.0);
    return capsule.generateMesh();
}

// ---------------------------------------------------------------------------
// The checks
// ---------------------------------------------------------------------------

void checkCreasePolicy(Recorder& r) {
    // The threshold has to clear the coarsest curved adjacency and stay well
    // under the sharpest edge, or the primitive contracts below are luck rather
    // than policy. 360/32 = 11.25 degrees is the curved bound; 90 is the sharp
    // one.
    r.check("crease_angle_above_curved_adjacency", kCreaseAngleDegrees > 11.25f * 2.0f);
    r.check("crease_angle_below_right_angle", kCreaseAngleDegrees < 90.0f * 0.75f);

    const float cosine = creaseCosineThreshold();
    r.check("crease_cosine_matches_angle",
            nearly(cosine, std::cos(kCreaseAngleDegrees * 3.14159265358979323846f / 180.0f)));
    // Two coplanar faces must always group; two perpendicular faces must never.
    r.check("crease_groups_coplanar_faces", 1.0f >= cosine);
    r.check("crease_splits_perpendicular_faces", 0.0f < cosine);
}

void checkBoxSmooth(Recorder& r) {
    const ConstructionMesh source = defaultBox();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("box_smooth_builds", built);
    if (!built) return;

    // Every one of the eight corners meets three mutually perpendicular faces,
    // so every corner splits into exactly three render vertices: 8 * 3 = 24,
    // which is also 6 faces * 4 corners.
    r.check("box_source_vertex_count_is_8", data.sourceVertexCount == kBoxVertexCount);
    r.check("box_render_vertex_count_is_24", data.vertexCount() == 24);
    // A corner is remapped, never added: the triangle list is the same list.
    r.check("box_index_count_unchanged", data.indexCount() == kBoxIndexCount);
    r.check("box_source_index_count_recorded", data.sourceIndexCount == kBoxIndexCount);

    r.check("box_normals_unit", allNormalsUnit(data));
    r.check("box_no_nan_or_inf", renderMeshIsFinite(data));

    // THE hard-edge contract: on a box every normal lies exactly on an axis. A
    // normal with two components would be a rounded corner — a corner whose
    // faces had been averaged together — which is precisely what the crease
    // policy exists to prevent.
    bool everyNormalAxisAligned = true;
    for (const RenderVertex& v : data.vertices) {
        if (!isAxisAligned(normalOf(v))) {
            everyNormalAxisAligned = false;
            break;
        }
    }
    r.check("box_every_normal_axis_aligned_no_rounded_corners", everyNormalAxisAligned);

    // Six planar faces, four render vertices each, one per axis direction.
    uint32_t perDirection[6] = {0, 0, 0, 0, 0, 0};
    for (const RenderVertex& v : data.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            if (nearly(v.normal[axis], 1.0f)) ++perDirection[axis * 2 + 0];
            if (nearly(v.normal[axis], -1.0f)) ++perDirection[axis * 2 + 1];
        }
    }
    bool fourPerFace = true;
    for (uint32_t count : perDirection) {
        if (count != 4) fourPerFace = false;
    }
    r.check("box_six_faces_four_vertices_each", fourPerFace);
}

void checkCylinderSmooth(Recorder& r) {
    const ConstructionMesh source = defaultCylinder();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("cylinder_smooth_builds", built);
    if (!built) return;

    r.check("cylinder_source_counts", data.sourceVertexCount == kCylinderVertexCount &&
                                          data.sourceIndexCount == kCylinderIndexCount);
    // Each of the 64 ring vertices splits in two (side group, cap group); the
    // two cap centres do not split. 64 * 2 + 2 = 130.
    r.check("cylinder_render_vertex_count_is_130", data.vertexCount() == 130);
    r.check("cylinder_index_count_unchanged", data.indexCount() == kCylinderIndexCount);
    r.check("cylinder_normals_unit", allNormalsUnit(data));
    r.check("cylinder_no_nan_or_inf", renderMeshIsFinite(data));

    // Two populations and nothing in between: 66 cap normals pointing exactly
    // along the axis (32 top rim + 32 bottom rim + 2 centres) and 64 side
    // normals with no axial component at all. A vertex in between would mean
    // the rim had been smoothed over.
    const uint32_t axial = countNormalsWithAxisMagnitude(data, 1, 0.999f);
    uint32_t radial = 0;
    float worstSideAgreement = 1.0f;
    for (const RenderVertex& v : data.vertices) {
        if (std::fabs(v.normal[1]) < 0.001f) {
            ++radial;
            // A side normal must point straight out from the axis.
            const Vec3 p = positionOf(v);
            const Vec3 axisRelative = vec3Normalize(Vec3{p.x, 0.0f, p.z});
            const float agreement = vec3Dot(normalOf(v), axisRelative);
            if (agreement < worstSideAgreement) worstSideAgreement = agreement;
        }
    }
    r.check("cylinder_66_flat_cap_normals", axial == 66);
    r.check("cylinder_64_smooth_side_normals", radial == 64);
    r.check("cylinder_hard_rim_no_intermediate_normals", axial + radial == data.vertexCount());
    // Not exactly 1.0, and the reason is worth writing down. A cylinder side
    // quad is planar, so both of its triangles share one normal at the quad's
    // mid-azimuth. But the diagonal split gives a ring vertex TWO triangles
    // from one neighbouring quad and ONE from the other, so the area-weighted
    // average lands about a third of the way off the bisector: roughly 1.9
    // degrees, or cos = 0.99946.
    //
    // Every ring vertex is biased the same way, because every quad is split the
    // same way, so this is a uniform ~2 degree rotation of the whole side's
    // lighting rather than any kind of alternating banding. It is invisible,
    // and removing it would mean angle-weighting the accumulation, which would
    // change the sculpt path's normals too for no readability gain.
    r.check("cylinder_side_normals_are_radial", worstSideAgreement > 0.999f);
}

void checkSphereSmooth(Recorder& r) {
    const ConstructionMesh source = defaultSphere();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("sphere_smooth_builds", built);
    if (!built) return;

    r.check("sphere_source_counts", data.sourceVertexCount == kSphereVertexCount &&
                                        data.sourceIndexCount == kSphereIndexCount);
    // A fully smooth closed surface has no creases, so nothing splits and the
    // render mesh is exactly the source topology. That equality is the cheapest
    // possible proof that the grouping did not fragment a smooth surface.
    r.check("sphere_render_vertex_count_equals_source", data.vertexCount() == kSphereVertexCount);
    r.check("sphere_index_count_unchanged", data.indexCount() == kSphereIndexCount);
    r.check("sphere_normals_unit", allNormalsUnit(data));
    r.check("sphere_no_nan_or_inf", renderMeshIsFinite(data));

    // Continuous smooth shading: every normal agrees with the exact analytic
    // sphere normal at that point.
    r.check("sphere_normals_radial_everywhere", minimumRadialAgreement(data) > 0.999f);

    // Stable poles: the pole fan's 32 triangles must average to the exact axis,
    // not to some arbitrary member of the fan.
    bool northPoleExact = false;
    bool southPoleExact = false;
    for (const RenderVertex& v : data.vertices) {
        const Vec3 p = positionOf(v);
        if (nearly(p.x, 0.0f) && nearly(p.z, 0.0f)) {
            if (p.y > 0.0f && nearly(v.normal[1], 1.0f)) northPoleExact = true;
            if (p.y < 0.0f && nearly(v.normal[1], -1.0f)) southPoleExact = true;
        }
    }
    r.check("sphere_north_pole_normal_stable", northPoleExact);
    r.check("sphere_south_pole_normal_stable", southPoleExact);
}

void checkConeSmooth(Recorder& r) {
    const ConstructionMesh source = defaultCone();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("cone_smooth_builds", built);
    if (!built) return;

    r.check("cone_source_counts",
            data.sourceVertexCount == kConeVertexCount && data.sourceIndexCount == kConeIndexCount);
    // 32 rim vertices split in two (lateral, base), plus the base centre and
    // the apex, neither of which splits. 32 * 2 + 1 + 1 = 66.
    r.check("cone_render_vertex_count_is_66", data.vertexCount() == 66);
    r.check("cone_index_count_unchanged", data.indexCount() == kConeIndexCount);
    r.check("cone_normals_unit", allNormalsUnit(data));
    r.check("cone_no_nan_or_inf", renderMeshIsFinite(data));

    // A planar base: 32 rim vertices plus the centre all pointing straight
    // down, and a hard rim, so no normal is partway between base and side.
    uint32_t base = 0;
    uint32_t lateral = 0;
    bool apexStable = false;
    bool apexNonZero = false;
    // For radius 0.5 and height 1 the exact lateral normal has
    // y = R / sqrt(R^2 + H^2) = 0.5 / sqrt(1.25) = 0.4472.
    const float exactLateralY = 0.5f / std::sqrt(1.25f);
    float worstLateralY = 1.0f;
    for (const RenderVertex& v : data.vertices) {
        const Vec3 p = positionOf(v);
        const Vec3 n = normalOf(v);
        if (nearly(n.y, -1.0f)) {
            ++base;
        } else if (n.y > 0.0f) {
            ++lateral;
            if (nearly(p.x, 0.0f) && nearly(p.z, 0.0f) && p.y > 0.0f) {
                // THE apex. A true apex has no single surface normal, so the
                // averaged fan normal must land exactly on the axis — anything
                // else is an arbitrary pick, and a zero would render as a black
                // spike.
                apexStable = nearly(n.y, 1.0f) && nearly(n.x, 0.0f) && nearly(n.z, 0.0f);
                apexNonZero = isUnit(n);
            } else {
                const float delta = std::fabs(n.y - exactLateralY);
                if (1.0f - delta < worstLateralY) worstLateralY = 1.0f - delta;
            }
        }
    }
    r.check("cone_flat_base_33_normals", base == 33);
    r.check("cone_lateral_33_normals", lateral == 33);
    r.check("cone_hard_base_rim_no_intermediate", base + lateral == data.vertexCount());
    r.check("cone_apex_normal_on_axis", apexStable);
    r.check("cone_apex_normal_non_zero_no_black_spike", apexNonZero);
    // The averaged lateral normals sit close to the exact cone normal; the gap
    // is faceting, not error.
    r.check("cone_lateral_normals_match_exact_slope", worstLateralY > 0.99f);
}

void checkCapsuleSmooth(Recorder& r) {
    const ConstructionMesh source = defaultCapsule();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("capsule_smooth_builds", built);
    if (!built) return;

    r.check("capsule_source_counts", data.sourceVertexCount == kCapsuleVertexCount &&
                                         data.sourceIndexCount == kCapsuleIndexCount);
    // Continuous across both hemisphere-to-middle seams, so nothing splits.
    r.check("capsule_render_vertex_count_equals_source",
            data.vertexCount() == kCapsuleVertexCount);
    r.check("capsule_index_count_unchanged", data.indexCount() == kCapsuleIndexCount);
    r.check("capsule_normals_unit", allNormalsUnit(data));
    r.check("capsule_no_nan_or_inf", renderMeshIsFinite(data));

    // The seams are the whole risk: at y = +/-0.5 a hemisphere band meets the
    // cylindrical middle, and if the crease policy split there the capsule
    // would show two hard rings. The true normal at the seam is radial, so a
    // small axial component is the smooth blend and a large one is a split.
    uint32_t seamVertices = 0;
    float worstSeamAxial = 0.0f;
    for (const RenderVertex& v : data.vertices) {
        const Vec3 p = positionOf(v);
        if (nearly(std::fabs(p.y), 0.5f)) {
            ++seamVertices;
            const float axial = std::fabs(v.normal[1]);
            if (axial > worstSeamAxial) worstSeamAxial = axial;
        }
    }
    r.check("capsule_both_seam_rings_present", seamVertices == 2 * kCapsuleRadialSegments);
    r.check("capsule_seams_shade_smoothly", worstSeamAxial < 0.15f);

    // The middle is a cylinder, so its normals carry no axial component at all.
    bool middleIsRadial = true;
    for (const RenderVertex& v : data.vertices) {
        const Vec3 p = positionOf(v);
        if (std::fabs(p.y) < 0.4f && std::fabs(v.normal[1]) > 0.15f) {
            middleIsRadial = false;
        }
    }
    r.check("capsule_middle_normals_radial", middleIsRadial);
}

void checkCapsuleEqualityCase(Recorder& r) {
    const ConstructionMesh source = sphericalCapsule();
    RenderMeshData data;
    const bool built = build(source, SurfaceShading::Smooth, &data);
    r.check("capsule_equality_builds", built);
    if (!built) return;

    // totalHeight == diameter is generated as a sphere, and must therefore
    // SHADE as one: same topology, same radial normals, no seam anywhere.
    r.check("capsule_equality_has_sphere_topology",
            data.sourceVertexCount == kCapsuleSphericalVertexCount &&
                data.sourceIndexCount == kCapsuleSphericalIndexCount);
    r.check("capsule_equality_render_count_equals_source",
            data.vertexCount() == kCapsuleSphericalVertexCount);
    r.check("capsule_equality_normals_unit", allNormalsUnit(data));
    r.check("capsule_equality_no_nan_or_inf", renderMeshIsFinite(data));
    r.check("capsule_equality_shades_as_sphere", minimumRadialAgreement(data) > 0.999f);
}

void checkFaceted(Recorder& r) {
    const ConstructionMesh box = defaultBox();
    RenderMeshData data;
    const bool built = build(box, SurfaceShading::Faceted, &data);
    r.check("faceted_builds", built);
    if (!built) return;

    // Faceted gives every triangle three private vertices, so the render vertex
    // count is exactly the index count and the index buffer is the identity.
    r.check("faceted_vertex_count_is_index_count", data.vertexCount() == kBoxIndexCount);
    r.check("faceted_index_count_unchanged", data.indexCount() == kBoxIndexCount);
    bool identityIndices = true;
    for (uint32_t i = 0; i < data.indexCount(); ++i) {
        if (data.indices[i] != i) identityIndices = false;
    }
    r.check("faceted_indices_are_identity", identityIndices);
    r.check("faceted_normals_unit", allNormalsUnit(data));
    r.check("faceted_no_nan_or_inf", renderMeshIsFinite(data));

    // Every triangle is flat: its three corners share one normal exactly.
    bool flatTriangles = true;
    for (uint32_t t = 0; t * 3 + 2 < data.indexCount(); ++t) {
        const Vec3 a = normalOf(data.vertices[t * 3 + 0]);
        const Vec3 b = normalOf(data.vertices[t * 3 + 1]);
        const Vec3 c = normalOf(data.vertices[t * 3 + 2]);
        if (!nearly(vec3Dot(a, b), 1.0f) || !nearly(vec3Dot(a, c), 1.0f)) {
            flatTriangles = false;
        }
    }
    r.check("faceted_every_triangle_has_one_normal", flatTriangles);

    // On a curved surface, faceted is meant to EXPOSE triangle structure, so it
    // must duplicate where smooth did not.
    const ConstructionMesh sphere = defaultSphere();
    RenderMeshData facetedSphere;
    RenderMeshData smoothSphere;
    const bool bothBuilt = build(sphere, SurfaceShading::Faceted, &facetedSphere) &&
                           build(sphere, SurfaceShading::Smooth, &smoothSphere);
    r.check("faceted_sphere_builds", bothBuilt);
    if (bothBuilt) {
        r.check("faceted_sphere_exposes_triangles",
                facetedSphere.vertexCount() == kSphereIndexCount &&
                    facetedSphere.vertexCount() > smoothSphere.vertexCount());
        // Same triangles either way: presentation changed, geometry did not.
        r.check("faceted_sphere_same_triangle_count",
                facetedSphere.indexCount() == smoothSphere.indexCount());
        r.check("faceted_sphere_normals_not_radial",
                minimumRadialAgreement(facetedSphere) < 0.9999f);
    }
}

void checkPresentationOnly(Recorder& r) {
    // Building render data must not disturb the authoritative mesh in any
    // observable way. This is the SHD-02 / SHD-10 contract, checked on the real
    // RuntimeMesh rather than on a copy.
    const ConstructionMesh source = defaultSphere();
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision revision =
        store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                      source.indices.data(), static_cast<uint32_t>(source.indices.size()));
    r.check("presentation_only_source_published", revision != kNoMeshRevision);

    const RuntimeMeshPtr mesh = store.current();
    r.check("presentation_only_source_available", static_cast<bool>(mesh));
    if (!mesh) return;

    const uint32_t vertexCountBefore = mesh->vertexCount();
    const uint32_t indexCountBefore = mesh->indexCount();
    const MeshRevision revisionBefore = mesh->revision();
    std::vector<MeshVertex> bytesBefore(mesh->vertices(), mesh->vertices() + mesh->vertexCount());
    std::vector<uint32_t> indicesBefore(mesh->indices(), mesh->indices() + mesh->indexCount());

    RenderMeshCache cache;
    bool failed = false;
    const bool rebuilt = cache.refresh(*mesh, SurfaceShading::Smooth, &failed);
    r.check("presentation_only_first_refresh_rebuilds", rebuilt && !failed);

    // Switching Smooth <-> Faceted rebuilds RENDER data and nothing else.
    cache.refresh(*mesh, SurfaceShading::Faceted, &failed);
    cache.refresh(*mesh, SurfaceShading::Smooth, &failed);

    r.check("presentation_only_source_vertex_count_unchanged",
            mesh->vertexCount() == vertexCountBefore);
    r.check("presentation_only_source_index_count_unchanged",
            mesh->indexCount() == indexCountBefore);
    r.check("presentation_only_source_revision_unchanged", mesh->revision() == revisionBefore);
    r.check("presentation_only_store_revision_unchanged", store.currentRevision() == revision);
    r.check("presentation_only_source_vertices_bit_identical",
            std::memcmp(bytesBefore.data(), mesh->vertices(),
                        bytesBefore.size() * sizeof(MeshVertex)) == 0);
    r.check("presentation_only_source_indices_bit_identical",
            std::memcmp(indicesBefore.data(), mesh->indices(),
                        indicesBefore.size() * sizeof(uint32_t)) == 0);
    // Nothing new was published: no revision was minted by any of this.
    r.check("presentation_only_published_count_unchanged", store.publishedCount() == 1);
}

void checkRebuildPolicy(Recorder& r) {
    const ConstructionMesh source = defaultBox();
    MeshStore store(kConstructionBoxObjectId);
    store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                  source.indices.data(), static_cast<uint32_t>(source.indices.size()));
    const RuntimeMeshPtr mesh = store.current();
    if (!mesh) {
        r.check("rebuild_policy_fixture", false);
        return;
    }

    RenderMeshCache cache;
    r.check("rebuild_starts_invalid", !cache.valid() && cache.rebuildCount() == 0);

    r.check("rebuild_first_refresh_builds", cache.refresh(*mesh, SurfaceShading::Smooth));
    r.check("rebuild_count_is_one", cache.rebuildCount() == 1);
    r.check("rebuild_is_valid_after_build", cache.valid());

    // THE per-frame proof: repeated refreshes with nothing changed must not
    // recompute a single normal. 240 stands in for four seconds of frames.
    for (int i = 0; i < 240; ++i) {
        cache.refresh(*mesh, SurfaceShading::Smooth);
    }
    r.check("rebuild_unchanged_geometry_never_rebuilds", cache.rebuildCount() == 1);
    r.check("rebuild_skips_are_counted", cache.skippedRefreshCount() == 240);

    // A surface shading change is the ONE display change that rebuilds.
    r.check("rebuild_on_surface_shading_change", cache.refresh(*mesh, SurfaceShading::Faceted));
    r.check("rebuild_count_is_two", cache.rebuildCount() == 2);
    r.check("rebuild_faceted_is_cached_too", !cache.refresh(*mesh, SurfaceShading::Faceted));
    r.check("rebuild_count_still_two", cache.rebuildCount() == 2);

    // A new source revision rebuilds, because the geometry genuinely changed.
    ConstructionBox bigger;
    bigger.setDimensionsMeters(3.0, 2.0, 1.0);
    const ConstructionMesh changed = bigger.generateMesh();
    store.publish(changed.vertices.data(), static_cast<uint32_t>(changed.vertices.size()),
                  changed.indices.data(), static_cast<uint32_t>(changed.indices.size()));
    const RuntimeMeshPtr newer = store.current();
    r.check("rebuild_on_new_revision", newer && cache.refresh(*newer, SurfaceShading::Faceted));
    r.check("rebuild_count_is_three", cache.rebuildCount() == 3);

    cache.invalidate();
    r.check("rebuild_invalidate_clears", !cache.valid());
    r.check("rebuild_after_invalidate", cache.refresh(*newer, SurfaceShading::Faceted));
    r.check("rebuild_count_is_four", cache.rebuildCount() == 4);
    r.check("rebuild_never_failed", cache.failedRebuildCount() == 0);
}

void checkFailsClosed(Recorder& r) {
    RenderMeshData data;
    const MeshVertex one{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    const uint32_t good[3] = {0, 0, 0};

    r.check("build_refuses_null_output",
            !buildRenderMesh(&one, 1, good, 3, SurfaceShading::Smooth, nullptr));
    r.check("build_refuses_zero_vertices",
            !buildRenderMesh(&one, 0, good, 3, SurfaceShading::Smooth, &data));
    r.check("build_refuses_zero_indices",
            !buildRenderMesh(&one, 1, good, 0, SurfaceShading::Smooth, &data));

    const uint32_t notTriangles[4] = {0, 0, 0, 0};
    r.check("build_refuses_non_triangle_index_count",
            !buildRenderMesh(&one, 1, notTriangles, 4, SurfaceShading::Smooth, &data));

    const uint32_t outOfRange[3] = {0, 1, 2};
    r.check("build_refuses_out_of_range_index",
            !buildRenderMesh(&one, 1, outOfRange, 3, SurfaceShading::Smooth, &data));

    // A non-finite position must be refused by the same gate the authoritative
    // mesh uses, so a NaN can never reach normal generation at all.
    MeshVertex poisoned[3] = {{{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                              {{1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
                              {{0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}};
    poisoned[2].position[0] = std::numeric_limits<float>::quiet_NaN();
    const uint32_t triangle[3] = {0, 1, 2};
    r.check("build_refuses_nan_position",
            !buildRenderMesh(poisoned, 3, triangle, 3, SurfaceShading::Smooth, &data));

    // A degenerate (zero-area) triangle is VALID input — its positions are
    // finite — and must produce the honest zero normal rather than a NaN.
    const MeshVertex degenerate[3] = {{{1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}},
                                      {{1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}},
                                      {{1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}}};
    RenderMeshData degenerateData;
    const bool degenerateBuilt =
        buildRenderMesh(degenerate, 3, triangle, 3, SurfaceShading::Smooth, &degenerateData);
    r.check("build_accepts_degenerate_triangle", degenerateBuilt);
    if (degenerateBuilt) {
        r.check("degenerate_triangle_is_finite", renderMeshIsFinite(degenerateData));
        bool allZero = true;
        for (const RenderVertex& v : degenerateData.vertices) {
            if (!nearly(vec3Dot(normalOf(v), normalOf(v)), 0.0f)) allZero = false;
        }
        r.check("degenerate_triangle_normal_is_zero_not_nan", allZero);
    }

    RenderMeshData facetedDegenerate;
    const bool facetedDegenerateBuilt =
        buildRenderMesh(degenerate, 3, triangle, 3, SurfaceShading::Faceted, &facetedDegenerate);
    r.check("faceted_accepts_degenerate_triangle", facetedDegenerateBuilt);
    if (facetedDegenerateBuilt) {
        r.check("faceted_degenerate_is_finite", renderMeshIsFinite(facetedDegenerate));
    }
}

void checkDeterminism(Recorder& r) {
    const ConstructionMesh source = defaultCapsule();
    RenderMeshData first;
    RenderMeshData second;
    const bool built = build(source, SurfaceShading::Smooth, &first) &&
                       build(source, SurfaceShading::Smooth, &second);
    r.check("determinism_builds", built);
    if (!built) return;

    r.check("determinism_same_counts", first.vertexCount() == second.vertexCount() &&
                                           first.indexCount() == second.indexCount());
    r.check("determinism_bit_identical_vertices",
            std::memcmp(first.vertices.data(), second.vertices.data(),
                        first.vertices.size() * sizeof(RenderVertex)) == 0);
    r.check("determinism_bit_identical_indices",
            std::memcmp(first.indices.data(), second.indices.data(),
                        first.indices.size() * sizeof(uint32_t)) == 0);
}

void checkColourPassthrough(Recorder& r) {
    // The debug source-colour mode reads the render vertex's colour, so the
    // derivation must carry it through unchanged from the source vertex it
    // came from.
    const ConstructionMesh source = defaultBox();
    RenderMeshData data;
    if (!build(source, SurfaceShading::Smooth, &data)) {
        r.check("colour_passthrough_builds", false);
        return;
    }
    r.check("colour_passthrough_builds", true);

    // Every render vertex must carry the colour of a source vertex sitting at
    // the same position.
    bool everyColourMatches = true;
    for (const RenderVertex& rv : data.vertices) {
        bool found = false;
        for (const MeshVertex& sv : source.vertices) {
            if (nearly(rv.position[0], sv.position[0]) && nearly(rv.position[1], sv.position[1]) &&
                nearly(rv.position[2], sv.position[2]) && nearly(rv.color[0], sv.color[0]) &&
                nearly(rv.color[1], sv.color[1]) && nearly(rv.color[2], sv.color[2])) {
                found = true;
                break;
            }
        }
        if (!found) everyColourMatches = false;
    }
    r.check("colour_passthrough_preserved", everyColourMatches);
}

void checkMatCapAsset(Recorder& r) {
    std::vector<uint8_t> texels;
    generateMatCap(&texels);
    r.check("matcap_generates_expected_size", texels.size() == kMatCapByteSize);
    if (texels.size() != kMatCapByteSize) return;

    // Generated, so it must be reproducible: the same build always produces the
    // same asset. That is what makes "ForgeShape-owned, project-generated"
    // verifiable rather than merely asserted.
    std::vector<uint8_t> again;
    generateMatCap(&again);
    r.check("matcap_is_deterministic",
            again.size() == texels.size() &&
                std::memcmp(again.data(), texels.data(), texels.size()) == 0);

    bool opaque = true;
    for (uint32_t i = 3; i < texels.size(); i += kMatCapBytesPerTexel) {
        if (texels[i] != 255) opaque = false;
    }
    r.check("matcap_is_fully_opaque", opaque);

    uint8_t facing[3] = {0, 0, 0};
    uint8_t keySide[3] = {0, 0, 0};
    uint8_t shadowSide[3] = {0, 0, 0};
    uint8_t rim[3] = {0, 0, 0};
    const bool sampled = sampleMatCap(texels, 0.0f, 0.0f, facing) &&
                         sampleMatCap(texels, -0.55f, 0.55f, keySide) &&
                         sampleMatCap(texels, 0.55f, -0.55f, shadowSide) &&
                         sampleMatCap(texels, 0.0f, 0.99f, rim);
    r.check("matcap_samples_inside_disc", sampled);
    if (!sampled) return;

    // A normal outside the unit disc is not a direction any visible surface
    // has, and must be refused rather than clamped silently.
    uint8_t ignored[3] = {0, 0, 0};
    r.check("matcap_refuses_outside_disc", !sampleMatCap(texels, 0.9f, 0.9f, ignored));
    r.check("matcap_refuses_non_finite",
            !sampleMatCap(texels, std::numeric_limits<float>::quiet_NaN(), 0.0f, ignored));

    // Readability, stated as measurements rather than as taste: the key side is
    // brighter than the shadow side, the surface facing the camera is a legible
    // mid-tone, and nothing anywhere is crushed to black.
    const int keyLuma = keySide[0] + keySide[1] + keySide[2];
    const int shadowLuma = shadowSide[0] + shadowSide[1] + shadowSide[2];
    r.check("matcap_key_side_brighter_than_shadow_side", keyLuma > shadowLuma + 60);
    r.check("matcap_facing_is_mid_tone", facing[0] > 90 && facing[0] < 235);
    r.check("matcap_shadow_side_is_not_black", shadowSide[0] > 24);
    r.check("matcap_rim_lifts_silhouette", rim[0] > shadowSide[0]);

    // Neutral clay, not a colour cast: the three channels stay close together.
    const int spread = static_cast<int>(facing[0]) - static_cast<int>(facing[2]);
    r.check("matcap_is_neutral", spread > -40 && spread < 40);

    uint8_t darkest = 255;
    uint8_t brightest = 0;
    for (uint32_t i = 0; i < texels.size(); i += kMatCapBytesPerTexel) {
        if (texels[i] < darkest) darkest = texels[i];
        if (texels[i] > brightest) brightest = texels[i];
    }
    r.check("matcap_has_usable_contrast_range", brightest - darkest > 80);
    r.check("matcap_never_pure_black", darkest > 8);
}

void checkDisplaySettings(Recorder& r) {
    // The product default must be the neutral modelling view, not a diagnostic.
    r.check("display_default_is_studio_solid", kDefaultShadingModel == ShadingModel::StudioSolid);
    r.check("display_default_is_smooth", kDefaultSurfaceShading == SurfaceShading::Smooth);

    // Index mapping is a contract with the Android UI: it sends an int.
    ShadingModel model = ShadingModel::MatCap;
    r.check("display_index_0_is_studio",
            shadingModelFromIndex(0, &model) && model == ShadingModel::StudioSolid);
    r.check("display_index_1_is_matcap",
            shadingModelFromIndex(1, &model) && model == ShadingModel::MatCap);
    r.check("display_index_2_is_debug",
            shadingModelFromIndex(2, &model) && model == ShadingModel::DebugSourceColor);
    r.check("display_refuses_negative_index", !shadingModelFromIndex(-1, &model));
    r.check("display_refuses_out_of_range_index", !shadingModelFromIndex(kShadingModelCount, &model));
    r.check("display_index_round_trips",
            shadingModelIndex(ShadingModel::StudioSolid) == 0 &&
                shadingModelIndex(ShadingModel::MatCap) == 1 &&
                shadingModelIndex(ShadingModel::DebugSourceColor) == 2);

    SurfaceShading shading = SurfaceShading::Faceted;
    r.check("display_surface_index_0_is_smooth",
            surfaceShadingFromIndex(0, &shading) && shading == SurfaceShading::Smooth);
    r.check("display_surface_index_1_is_faceted",
            surfaceShadingFromIndex(1, &shading) && shading == SurfaceShading::Faceted);
    r.check("display_surface_refuses_out_of_range", !surfaceShadingFromIndex(2, &shading));

    // The store reports whether a request was a real transition, so a no-op
    // never looks like a change in a log or an animation.
    DisplaySettingsStore store;
    r.check("display_store_starts_at_default",
            store.shadingModel() == kDefaultShadingModel &&
                store.surfaceShading() == kDefaultSurfaceShading);
    r.check("display_store_no_op_reports_false", !store.setShadingModel(kDefaultShadingModel));
    r.check("display_store_change_count_still_zero", store.changeCount() == 0);
    r.check("display_store_real_change_reports_true", store.setShadingModel(ShadingModel::MatCap));
    r.check("display_store_holds_new_value", store.shadingModel() == ShadingModel::MatCap);
    r.check("display_store_surface_change", store.setSurfaceShading(SurfaceShading::Faceted));
    r.check("display_store_surface_held", store.surfaceShading() == SurfaceShading::Faceted);
    r.check("display_store_counts_two_changes", store.changeCount() == 2);

    // A snapshot must be a coherent pair, which is what a frame is recorded
    // with.
    const ViewportDisplaySettings snapshot = store.snapshot();
    r.check("display_snapshot_matches",
            snapshot.shading == ShadingModel::MatCap && snapshot.surface == SurfaceShading::Faceted);

    // Naming exists for logs and evidence; an unnamed mode is an unreadable log.
    r.check("display_names_present", std::strcmp(shadingModelName(ShadingModel::StudioSolid),
                                                 "StudioSolid") == 0 &&
                                        std::strcmp(shadingModelName(ShadingModel::MatCap),
                                                    "MatCap") == 0 &&
                                        std::strcmp(surfaceShadingName(SurfaceShading::Smooth),
                                                    "Smooth") == 0 &&
                                        std::strcmp(surfaceShadingName(SurfaceShading::Faceted),
                                                    "Faceted") == 0);
}

// A stand-in for a sculpt edit: move some vertices and rebuild. The renderer
// must follow the deformed positions, and must do so WITHOUT reading anything
// from the Sculpt domain — this check deliberately uses only MeshStore.
void checkDeformationFollows(Recorder& r) {
    const ConstructionMesh source = defaultSphere();
    MeshStore store(kConstructionBoxObjectId);
    store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                  source.indices.data(), static_cast<uint32_t>(source.indices.size()));

    RenderMeshCache cache;
    const RuntimeMeshPtr before = store.current();
    if (!before || !cache.refresh(*before, SurfaceShading::Smooth)) {
        r.check("deformation_fixture", false);
        return;
    }
    r.check("deformation_fixture", true);

    std::vector<RenderVertex> normalsBefore = cache.data().vertices;

    // Inflate the upper half, the way a real Inflate stroke would: a SMOOTH
    // radial push with a falloff that reaches zero, with zero slope, at the
    // equator.
    //
    // The falloff matters to what is being measured. An abrupt cut-off (say,
    // scaling only the vertices above some y) creates a genuine cliff, the
    // crease policy correctly splits along it, and the render vertex count
    // changes — which is right, but it would make this check about topology
    // instead of about normals following positions. A smooth deformation keeps
    // the topology fixed, so any normal that moved, moved because the SURFACE
    // moved.
    std::vector<MeshVertex> deformed = source.vertices;
    for (MeshVertex& v : deformed) {
        const float height = v.position[1] / 0.5f;  // -1 at the south pole, +1 at the north
        const float weight = height > 0.0f ? height * height : 0.0f;
        const float scale = 1.0f + 0.4f * weight;
        v.position[0] *= scale;
        v.position[1] *= scale;
        v.position[2] *= scale;
    }
    const MeshRevision deformedRevision =
        store.publish(deformed.data(), static_cast<uint32_t>(deformed.size()),
                      source.indices.data(), static_cast<uint32_t>(source.indices.size()));
    r.check("deformation_published", deformedRevision != kNoMeshRevision);

    const RuntimeMeshPtr after = store.current();
    r.check("deformation_rebuilds_on_new_revision",
            after && cache.refresh(*after, SurfaceShading::Smooth));
    r.check("deformation_rebuild_count_is_two", cache.rebuildCount() == 2);

    // A smooth deformation splits nothing, so the render vertices correspond
    // one for one and can be compared directly. If this ever stops holding, the
    // comparison below would be meaningless, so it is asserted rather than
    // assumed.
    const std::vector<RenderVertex>& normalsAfter = cache.data().vertices;
    r.check("deformation_render_topology_stable", normalsAfter.size() == normalsBefore.size());

    // No stale lighting: normals moved because the surface moved.
    uint32_t movedNormals = 0;
    if (normalsAfter.size() == normalsBefore.size()) {
        for (size_t i = 0; i < normalsAfter.size(); ++i) {
            if (!nearly(vec3Dot(normalOf(normalsAfter[i]), normalOf(normalsBefore[i])), 1.0f)) {
                ++movedNormals;
            }
        }
    }
    r.check("deformation_normals_follow_positions", movedNormals > 0);
    // Not just one stray vertex: an inflated cap relights a large region.
    r.check("deformation_relights_a_region", movedNormals > kSphereRadialSegments);
    r.check("deformation_normals_still_unit", allNormalsUnit(cache.data()));
    r.check("deformation_no_nan_or_inf", renderMeshIsFinite(cache.data()));
    // Topology is untouched by a position-only edit, so nothing split.
    r.check("deformation_topology_unchanged",
            cache.data().indexCount() == kSphereIndexCount &&
                cache.data().sourceVertexCount == kSphereVertexCount);
}

}  // namespace

int runRenderMeshSelfTests(RenderMeshSelfTestResult* out, int max) {
    Recorder r{out, max};

    checkCreasePolicy(r);
    checkBoxSmooth(r);
    checkCylinderSmooth(r);
    checkSphereSmooth(r);
    checkConeSmooth(r);
    checkCapsuleSmooth(r);
    checkCapsuleEqualityCase(r);
    checkFaceted(r);
    checkPresentationOnly(r);
    checkRebuildPolicy(r);
    checkFailsClosed(r);
    checkDeterminism(r);
    checkColourPassthrough(r);
    checkMatCapAsset(r);
    checkDisplaySettings(r);
    checkDeformationFollows(r);

    return r.n;
}

}  // namespace forgeshape
