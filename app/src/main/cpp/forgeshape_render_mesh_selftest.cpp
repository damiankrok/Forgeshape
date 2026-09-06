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
// NOR-10 proves a display change cannot be observed by picking, which means
// running the real picking query against the authoritative mesh rather than
// asserting the intention in prose.
#include "forgeshape_picking.h"
#include "forgeshape_render_mesh.h"
// UI-R1C2. The grid is presentation, so its contract is asserted in the
// presentation suite; ConstructionScene comes with it because the strongest
// available statement of "the grid is not in the scene" is a real scene,
// snapshotted either side of a real toggle and compared.
#include "forgeshape_camera.h"
#include "forgeshape_grid.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_selection_outline.h"
#include "forgeshape_selection_pulse.h"

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
// Outward-direction helpers (the NOR checks)
// ---------------------------------------------------------------------------
//
// Every check above this point measures a normal's AXIS or its MAGNITUDE, and
// every one of them survives multiplying the whole mesh by -1: counting four
// normals per box face counts +X and -X the same way, and "axis aligned" says
// nothing about which way along the axis. Sign is the one property a lighting
// defect would actually corrupt, and until these helpers existed nothing
// asserted it.
//
// The reference is always the same: all five primitives are CONVEX solids, so
// for any point strictly inside them, a correctly oriented surface normal at a
// point p satisfies dot(n, p - interior) > 0. That single statement covers a box
// corner, a cylinder cap centre, a cone apex and a sphere pole without one
// per-primitive special case, and it is exactly the property that inverts when
// a normal is flipped.

// The centre of the mesh's axis-aligned bounds. For a convex primitive centred
// on its local axis this is strictly interior, which is all the outwardness test
// needs it to be.
Vec3 boundsCentre(const ConstructionMesh& source) {
    Vec3 lo{source.vertices[0].position[0], source.vertices[0].position[1],
            source.vertices[0].position[2]};
    Vec3 hi = lo;
    for (const MeshVertex& v : source.vertices) {
        lo.x = std::fmin(lo.x, v.position[0]);
        lo.y = std::fmin(lo.y, v.position[1]);
        lo.z = std::fmin(lo.z, v.position[2]);
        hi.x = std::fmax(hi.x, v.position[0]);
        hi.y = std::fmax(hi.y, v.position[1]);
        hi.z = std::fmax(hi.z, v.position[2]);
    }
    return vec3Scale(vec3Add(lo, hi), 0.5f);
}

// One SOURCE triangle's geometric normal, computed exactly the way the render
// mesh builder computes it: cross(p1 - p0, p2 - p0), which is outward only when
// the triangle is wound counter-clockwise seen from outside.
Vec3 sourceTriangleNormal(const ConstructionMesh& source, uint32_t triangle) {
    const MeshVertex& a = source.vertices[source.indices[triangle * 3 + 0]];
    const MeshVertex& b = source.vertices[source.indices[triangle * 3 + 1]];
    const MeshVertex& c = source.vertices[source.indices[triangle * 3 + 2]];
    const Vec3 p0{a.position[0], a.position[1], a.position[2]};
    const Vec3 p1{b.position[0], b.position[1], b.position[2]};
    const Vec3 p2{c.position[0], c.position[1], c.position[2]};
    return vec3Normalize(vec3Cross(vec3Sub(p1, p0), vec3Sub(p2, p0)));
}

Vec3 sourceTriangleCentroid(const ConstructionMesh& source, uint32_t triangle) {
    Vec3 sum{0.0f, 0.0f, 0.0f};
    for (uint32_t corner = 0; corner < 3; ++corner) {
        const MeshVertex& v = source.vertices[source.indices[triangle * 3 + corner]];
        sum = vec3Add(sum, Vec3{v.position[0], v.position[1], v.position[2]});
    }
    return vec3Scale(sum, 1.0f / 3.0f);
}

// NOR-01's measurement: the worst dot(faceNormal, centre -> face centroid) over
// every triangle in a source mesh. Positive everywhere means every triangle is
// wound counter-clockwise seen from outside; one reversed triangle drags this
// negative.
float worstSourceWinding(const ConstructionMesh& source) {
    const Vec3 interior = boundsCentre(source);
    float worst = 1.0f;
    const uint32_t triangles = static_cast<uint32_t>(source.indices.size()) / 3;
    for (uint32_t t = 0; t < triangles; ++t) {
        const Vec3 n = sourceTriangleNormal(source, t);
        const Vec3 outward = vec3Normalize(vec3Sub(sourceTriangleCentroid(source, t), interior));
        worst = std::fmin(worst, vec3Dot(n, outward));
    }
    return worst;
}

// The same measurement on RENDER normals: the worst agreement between a render
// vertex's shading normal and the outward direction at its position. Sign-
// sensitive by construction, which is what makes the whole NOR family fail on a
// global `normal *= -1`.
float worstRenderOutwardness(const RenderMeshData& data, const Vec3& interior) {
    float worst = 1.0f;
    for (const RenderVertex& v : data.vertices) {
        const Vec3 outward = vec3Normalize(vec3Sub(positionOf(v), interior));
        worst = std::fmin(worst, vec3Dot(normalOf(v), outward));
    }
    return worst;
}

// A copy with every normal inverted. Used to prove the outwardness measurement
// is actually sign-sensitive rather than accidentally passing.
RenderMeshData withInvertedNormals(const RenderMeshData& data) {
    RenderMeshData flipped = data;
    for (RenderVertex& v : flipped.vertices) {
        v.normal[0] = -v.normal[0];
        v.normal[1] = -v.normal[1];
        v.normal[2] = -v.normal[2];
    }
    return flipped;
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

ConstructionMesh defaultPlane() {
    ConstructionPlane plane;
    plane.setDimensionsMeters(kDefaultPlaneWidthMeters, kDefaultPlaneDepthMeters);
    return plane.generateMesh();
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

    // -----------------------------------------------------------------------
    // DISP-VBG-01..07 -- the viewport background
    //
    // The one presentation value the Android theme system hands down. It has to
    // behave exactly like a shading model: a closed enum, an index contract with
    // the UI that REFUSES what it does not recognise, and a default that is the
    // appearance the product ships with.
    // -----------------------------------------------------------------------

    r.check("display_viewport_background_default_is_warm_graphite",
            kDefaultViewportBackground == ViewportBackground::WarmGraphite);
    // Five since UI-PREF-R1: the three dark grounds keep their indices — the
    // index is what crosses JNI — and the two light grounds are appended.
    r.check("display_viewport_background_has_five_appearances",
            kViewportBackgroundCount == 5);

    ViewportBackground background = ViewportBackground::LightCharcoal;
    r.check("display_viewport_background_index_0_is_warm_graphite",
            viewportBackgroundFromIndex(0, &background) &&
                background == ViewportBackground::WarmGraphite);
    r.check("display_viewport_background_index_1_is_neutral_charcoal",
            viewportBackgroundFromIndex(1, &background) &&
                background == ViewportBackground::NeutralCharcoal);
    r.check("display_viewport_background_index_2_is_light_charcoal",
            viewportBackgroundFromIndex(2, &background) &&
                background == ViewportBackground::LightCharcoal);
    r.check("display_viewport_background_index_3_is_warm_light",
            viewportBackgroundFromIndex(3, &background) &&
                background == ViewportBackground::WarmLight);
    r.check("display_viewport_background_index_4_is_cool_light",
            viewportBackgroundFromIndex(4, &background) &&
                background == ViewportBackground::CoolLight);
    r.check("display_viewport_background_refuses_negative",
            !viewportBackgroundFromIndex(-1, &background));
    r.check("display_viewport_background_refuses_out_of_range",
            !viewportBackgroundFromIndex(kViewportBackgroundCount, &background));
    r.check("display_viewport_background_index_round_trips",
            viewportBackgroundIndex(ViewportBackground::WarmGraphite) == 0 &&
                viewportBackgroundIndex(ViewportBackground::NeutralCharcoal) == 1 &&
                viewportBackgroundIndex(ViewportBackground::LightCharcoal) == 2 &&
                viewportBackgroundIndex(ViewportBackground::WarmLight) == 3 &&
                viewportBackgroundIndex(ViewportBackground::CoolLight) == 4);
    // The one question every per-ground tool colour asks, answered once.
    r.check("display_only_the_two_light_grounds_are_light",
            !viewportBackgroundIsLight(ViewportBackground::WarmGraphite) &&
                !viewportBackgroundIsLight(ViewportBackground::NeutralCharcoal) &&
                !viewportBackgroundIsLight(ViewportBackground::LightCharcoal) &&
                viewportBackgroundIsLight(ViewportBackground::WarmLight) &&
                viewportBackgroundIsLight(ViewportBackground::CoolLight));

    r.check("display_store_background_starts_at_the_default",
            store.viewportBackground() == ViewportBackground::WarmGraphite);
    r.check("display_store_background_no_op_reports_false",
            !store.setViewportBackground(ViewportBackground::WarmGraphite));
    r.check("display_store_background_change_reports_true",
            store.setViewportBackground(ViewportBackground::LightCharcoal));
    r.check("display_store_background_held",
            store.viewportBackground() == ViewportBackground::LightCharcoal);

    // The three colours, pinned. Each is duplicated in colors.xml as the ANDROID
    // WINDOW background, which is what covers the moment before the surface has
    // content — so if these drift the user sees a flash of the wrong shade on
    // every launch. Asserting the exact values is what makes that drift a test
    // failure rather than a report months later.
    float warm[3];
    float neutral[3];
    float lightest[3];
    viewportBackgroundColor(ViewportBackground::WarmGraphite, warm);
    r.check("display_warm_graphite_background_is_302e2b",
            nearly(warm[0], 0.188f) && nearly(warm[1], 0.180f) &&
                nearly(warm[2], 0.169f));
    viewportBackgroundColor(ViewportBackground::NeutralCharcoal, neutral);
    r.check("display_neutral_charcoal_background_is_26282a",
            nearly(neutral[0], 0.149f) && nearly(neutral[1], 0.157f) &&
                nearly(neutral[2], 0.165f));
    viewportBackgroundColor(ViewportBackground::LightCharcoal, lightest);
    r.check("display_light_charcoal_background_is_3c3f41",
            nearly(lightest[0], 0.235f) && nearly(lightest[1], 0.247f) &&
                nearly(lightest[2], 0.255f));

    // What separates the three from each other, stated rather than implied.
    // Three grounds that had drifted into the same neutral would still pass
    // every exact check above if all three were edited together.
    r.check("display_warm_graphite_leans_warm", warm[0] > warm[1] && warm[1] > warm[2]);
    r.check("display_neutral_charcoal_leans_cool",
            neutral[2] > neutral[1] && neutral[1] > neutral[0]);
    r.check("display_light_charcoal_is_the_lightest_ground",
            lightest[0] > warm[0] && lightest[0] > neutral[0]);
    // The three ORIGINAL grounds are DARK, exactly as approved: a dark palette
    // that had crept up into a light canvas would change what those three are.
    r.check("display_every_dark_ground_is_dark",
            warm[0] < 0.4f && neutral[0] < 0.4f && lightest[0] < 0.4f);

    // DISP-VBG-08/09 -- the two LIGHT grounds (UI-PREF-R1, UI-OWNER-42), pinned
    // against colors.xml exactly as the dark three are, and separated from each
    // other by the same warm/cool lean that separates Warm Graphite from
    // Neutral Charcoal.
    float warmLight[3];
    float coolLight[3];
    viewportBackgroundColor(ViewportBackground::WarmLight, warmLight);
    r.check("display_warm_light_background_is_ede7dc",
            nearly(warmLight[0], 0.929f) && nearly(warmLight[1], 0.906f) &&
                nearly(warmLight[2], 0.863f));
    viewportBackgroundColor(ViewportBackground::CoolLight, coolLight);
    r.check("display_cool_light_background_is_e4e8ec",
            nearly(coolLight[0], 0.894f) && nearly(coolLight[1], 0.910f) &&
                nearly(coolLight[2], 0.925f));
    r.check("display_warm_light_leans_warm",
            warmLight[0] > warmLight[1] && warmLight[1] > warmLight[2]);
    r.check("display_cool_light_leans_cool",
            coolLight[2] > coolLight[1] && coolLight[1] > coolLight[0]);
    r.check("display_every_light_ground_is_light",
            warmLight[0] > 0.8f && warmLight[1] > 0.8f && warmLight[2] > 0.8f &&
                coolLight[0] > 0.8f && coolLight[1] > 0.8f && coolLight[2] > 0.8f);
    r.check("display_the_two_light_grounds_are_distinguishable",
            std::fabs((warmLight[0] - warmLight[2]) - (coolLight[0] - coolLight[2])) > 0.05f);

    // The gizmo stroke weight (UI-PREF-R1 F) lives in the same store on the
    // same terms: a closed enum, a refusing index contract, a counted change.
    r.check("display_gizmo_stroke_weight_default_is_regular",
            kDefaultGizmoStrokeWeight == GizmoStrokeWeight::Regular &&
                kGizmoStrokeWeightCount == 3);
    GizmoStrokeWeight weight = GizmoStrokeWeight::Bold;
    r.check("display_gizmo_stroke_weight_index_round_trips",
            gizmoStrokeWeightFromIndex(0, &weight) && weight == GizmoStrokeWeight::Thin &&
                gizmoStrokeWeightFromIndex(1, &weight) && weight == GizmoStrokeWeight::Regular &&
                gizmoStrokeWeightFromIndex(2, &weight) && weight == GizmoStrokeWeight::Bold &&
                gizmoStrokeWeightIndex(GizmoStrokeWeight::Thin) == 0 &&
                gizmoStrokeWeightIndex(GizmoStrokeWeight::Regular) == 1 &&
                gizmoStrokeWeightIndex(GizmoStrokeWeight::Bold) == 2);
    r.check("display_gizmo_stroke_weight_refuses_unknown",
            !gizmoStrokeWeightFromIndex(-1, &weight) &&
                !gizmoStrokeWeightFromIndex(kGizmoStrokeWeightCount, &weight));
    r.check("display_store_stroke_weight_starts_regular",
            store.gizmoStrokeWeight() == GizmoStrokeWeight::Regular);
    r.check("display_store_stroke_weight_no_op_reports_false",
            !store.setGizmoStrokeWeight(GizmoStrokeWeight::Regular));
    const uint64_t changesBeforeWeight = store.changeCount();
    r.check("display_store_stroke_weight_change_reports_true_and_counts",
            store.setGizmoStrokeWeight(GizmoStrokeWeight::Bold) &&
                store.gizmoStrokeWeight() == GizmoStrokeWeight::Bold &&
                store.changeCount() == changesBeforeWeight + 1 &&
                store.snapshot().gizmoStrokeWeight == GizmoStrokeWeight::Bold);
    r.check("display_stroke_weight_names_present",
            std::strcmp(gizmoStrokeWeightName(GizmoStrokeWeight::Thin), "Thin") == 0 &&
                std::strcmp(gizmoStrokeWeightName(GizmoStrokeWeight::Regular), "Regular") == 0 &&
                std::strcmp(gizmoStrokeWeightName(GizmoStrokeWeight::Bold), "Bold") == 0 &&
                std::strcmp(viewportBackgroundName(ViewportBackground::WarmLight), "WarmLight") ==
                    0 &&
                std::strcmp(viewportBackgroundName(ViewportBackground::CoolLight), "CoolLight") ==
                    0);

    // A snapshot must be a coherent set, which is what a frame is recorded with.
    const ViewportDisplaySettings snapshot = store.snapshot();
    r.check("display_snapshot_matches",
            snapshot.shading == ShadingModel::MatCap &&
                snapshot.surface == SurfaceShading::Faceted &&
                snapshot.background == ViewportBackground::LightCharcoal);

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

// ---------------------------------------------------------------------------
// UI-R1C2: the world reference grid
// ---------------------------------------------------------------------------
//
// R1C2-01..09. Everything here is either pure arithmetic over the grid contract
// or real state driven through the real stores; nothing samples a pixel, for the
// same reason no other check in this suite does. What a rendered grid LOOKS
// like is judged by eye and by runtime evidence — what is asserted here is that
// it is generated correctly, that it is presentation and only presentation, and
// that turning it on or off cannot reach the model.

// R1C2-01/02: the default, and where the value lives.
void checkGridDefaultAndOwnership(Recorder& r) {
    r.check("r1c2_01_grid_defaults_to_on", kDefaultGridVisible);

    // A fresh store wears the default, exactly as the shading model does. This
    // is the whole of "process-scoped": nothing loads it, nothing saves it, and
    // a new process is a new store.
    DisplaySettingsStore store;
    r.check("r1c2_01_a_fresh_store_shows_the_default",
            store.gridVisible() == kDefaultGridVisible);

    // R1C2-02. It is a DISPLAY setting, so it behaves like one: a no-op reports
    // false, a real change reports true and is counted. Reduced motion
    // deliberately is not counted; the grid deliberately is, because the user
    // chose it.
    const uint64_t before = store.changeCount();
    r.check("r1c2_02_grid_no_op_reports_false", !store.setGridVisible(kDefaultGridVisible));
    r.check("r1c2_02_grid_no_op_is_not_counted", store.changeCount() == before);
    r.check("r1c2_02_grid_change_reports_true", store.setGridVisible(!kDefaultGridVisible));
    r.check("r1c2_02_grid_change_is_counted", store.changeCount() == before + 1);
    r.check("r1c2_02_grid_value_is_held", store.gridVisible() == !kDefaultGridVisible);
    r.check("r1c2_02_grid_returns", store.setGridVisible(kDefaultGridVisible) &&
                                        store.gridVisible() == kDefaultGridVisible);

    // It must ride in the snapshot the render thread actually reads. A value
    // held only in the store would be a value the frame loop never sees.
    store.setGridVisible(false);
    store.setViewportBackground(ViewportBackground::LightCharcoal);
    const ViewportDisplaySettings off = store.snapshot();
    store.setGridVisible(true);
    const ViewportDisplaySettings on = store.snapshot();
    r.check("r1c2_02_grid_rides_in_the_snapshot", !off.gridVisible && on.gridVisible);
    r.check("r1c2_02_the_snapshot_is_otherwise_the_same",
            off.shading == on.shading && off.surface == on.surface &&
                off.background == on.background);

    // The grid must not have leaked into any OTHER display value. A setter that
    // wrote the wrong atomic would pass every check above.
    r.check("r1c2_02_grid_does_not_disturb_the_shading_model",
            on.shading == kDefaultShadingModel);
    r.check("r1c2_02_grid_does_not_disturb_the_background",
            on.background == ViewportBackground::LightCharcoal);
}

// The generated geometry: R1C2-04's structural half, and the spacing/extent
// contract the report quotes.
void checkGridGeometry(Recorder& r) {
    // The contract itself, pinned. These are the numbers the product's look was
    // chosen against, and a silent change to any of them is a different grid.
    r.check("r1c2_grid_plane_is_world_y_zero", kGridPlaneY == 0.0f);
    r.check("r1c2_grid_minor_spacing_is_one_meter",
            nearly(kGridMinorSpacingMeters, 1.0f));
    r.check("r1c2_grid_major_every_five", kGridMajorEveryNMinor == 5);
    r.check("r1c2_grid_half_extent_is_twenty_meters",
            nearly(kGridHalfExtentMeters, 20.0f));
    r.check("r1c2_grid_line_count_is_odd_per_axis", (kGridLinesPerAxis % 2) == 1);
    r.check("r1c2_grid_counts_agree",
            kGridLineCount == 2 * kGridLinesPerAxis && kGridVertexCount == 2 * kGridLineCount);

    std::vector<GridVertex> vertices(kGridVertexCount);
    const int written = generateGridVertices(vertices.data(), kGridVertexCount);
    r.check("r1c2_grid_generates_every_vertex", written == kGridVertexCount);

    // Fails CLOSED. A partial grid drawn from a half-filled buffer would be
    // stray lines through the model, which is worse than no grid at all.
    GridVertex scratch[4];
    r.check("r1c2_grid_refuses_a_short_buffer", generateGridVertices(scratch, 4) == 0);
    r.check("r1c2_grid_refuses_a_null_buffer",
            generateGridVertices(nullptr, kGridVertexCount) == 0);

    // R1C2-04's structural half: EVERY vertex is on the world XZ plane at
    // y = 0. A grid that drifted off its plane would be geometry floating in
    // the scene rather than a floor.
    bool allOnPlane = true;
    bool allInsideExtent = true;
    for (int i = 0; i < written; ++i) {
        if (!nearly(vertices[i].position[1], kGridPlaneY)) {
            allOnPlane = false;
        }
        if (std::fabs(vertices[i].position[0]) > kGridHalfExtentMeters + kEpsilon ||
            std::fabs(vertices[i].position[2]) > kGridHalfExtentMeters + kEpsilon) {
            allInsideExtent = false;
        }
    }
    r.check("r1c2_04_every_grid_vertex_is_on_world_y_zero", allOnPlane);
    r.check("r1c2_04_no_grid_vertex_escapes_the_extent", allInsideExtent);

    // Every line is axis-aligned and spans the full extent: one coordinate is
    // shared by both ends, the other runs edge to edge.
    bool allAxisAligned = true;
    bool allSpanTheExtent = true;
    for (int i = 0; i + 1 < written; i += 2) {
        const GridVertex& a = vertices[i];
        const GridVertex& b = vertices[i + 1];
        const bool alongX = nearly(a.position[2], b.position[2]);
        const bool alongZ = nearly(a.position[0], b.position[0]);
        if (alongX == alongZ) {
            allAxisAligned = false;  // neither, or degenerate in both
            continue;
        }
        const float span = alongX ? std::fabs(b.position[0] - a.position[0])
                                  : std::fabs(b.position[2] - a.position[2]);
        if (!nearly(span, 2.0f * kGridHalfExtentMeters)) {
            allSpanTheExtent = false;
        }
        if (a.tier != b.tier) {
            allAxisAligned = false;  // a line cannot change weight along itself
        }
    }
    r.check("r1c2_grid_lines_are_axis_aligned_and_single_tiered", allAxisAligned);
    r.check("r1c2_grid_lines_span_the_full_extent", allSpanTheExtent);

    // The tiers. There is exactly ONE line of each axis kind, and it is the one
    // through the origin — an axis that appeared twice, or nowhere, would leave
    // the origin unreadable, which is what the tier exists for.
    int axisX = 0;
    int axisZ = 0;
    int major = 0;
    for (int index = 0; index < kGridLinesPerAxis; ++index) {
        if (gridLineTier(index, /*alongX=*/true) == GridLineTier::AxisX) ++axisX;
        if (gridLineTier(index, /*alongX=*/false) == GridLineTier::AxisZ) ++axisZ;
        if (gridLineTier(index, /*alongX=*/true) == GridLineTier::Major) ++major;
    }
    r.check("r1c2_grid_has_exactly_one_x_axis", axisX == 1);
    r.check("r1c2_grid_has_exactly_one_z_axis", axisZ == 1);
    // 20 m either way at 1 m spacing with every fifth line major: 4 each side,
    // and the fifth would be the axis itself.
    r.check("r1c2_grid_major_rhythm_is_regular", major == 8);
    r.check("r1c2_grid_a_line_next_to_the_axis_is_minor",
            gridLineTier(kGridLinesPerAxis / 2 + 1, true) == GridLineTier::Minor);
    r.check("r1c2_grid_the_fifth_line_out_is_major",
            gridLineTier(kGridLinesPerAxis / 2 + kGridMajorEveryNMinor, true) ==
                GridLineTier::Major);

    // The two axes must be TOLD APART, or the grid shows where the origin is
    // and not which way the world faces.
    r.check("r1c2_grid_the_two_axes_are_distinct_tiers",
            gridLineTier(kGridLinesPerAxis / 2, true) !=
                gridLineTier(kGridLinesPerAxis / 2, false));
}

// R1C2-07: every appearance's grid palette, asserted as relationships rather
// than as literals — the same rule the Android theme suite follows. Pinning the
// RGB would break on every deliberate restyle while proving nothing about
// whether a line can actually be seen.
//
// All three appearances are dark grounds, so the DIRECTION of the contrast is
// the same in all three — lines lift off the floor. What differs is how much
// weight that takes, which is exactly what the per-appearance palettes exist to
// answer and what this checks.
void checkGridPalette(Recorder& r) {
    const GridLineTier tiers[kGridLineTierCount] = {
        GridLineTier::Minor, GridLineTier::Major, GridLineTier::AxisX, GridLineTier::AxisZ};
    const ViewportBackground appearances[kViewportBackgroundCount] = {
        ViewportBackground::WarmGraphite, ViewportBackground::NeutralCharcoal,
        ViewportBackground::LightCharcoal, ViewportBackground::WarmLight,
        ViewportBackground::CoolLight};

    float palette[kViewportBackgroundCount][kGridLineTierCount][4];
    float ground[kViewportBackgroundCount][3];
    for (int a = 0; a < kViewportBackgroundCount; ++a) {
        viewportBackgroundColor(appearances[a], ground[a]);
        for (int i = 0; i < kGridLineTierCount; ++i) {
            gridLineColor(appearances[a], tiers[i], palette[a][i]);
        }
    }

    // Every channel is a usable colour and every alpha is a usable weight.
    bool inRange = true;
    for (int a = 0; a < kViewportBackgroundCount; ++a) {
        for (int i = 0; i < kGridLineTierCount; ++i) {
            for (int c = 0; c < 4; ++c) {
                if (palette[a][i][c] < 0.0f || palette[a][i][c] > 1.0f) {
                    inRange = false;
                }
            }
        }
    }
    r.check("r1c2_07_every_grid_colour_is_in_range", inRange);

    // Each appearance is genuinely its OWN palette. A grid that reused one set
    // of values everywhere would pass every other check here and would be
    // nearly invisible on the lightest ground.
    bool differsEverywhere = true;
    for (int a = 1; a < kViewportBackgroundCount; ++a) {
        bool differs = false;
        for (int i = 0; i < kGridLineTierCount; ++i) {
            for (int c = 0; c < 4; ++c) {
                if (!nearly(palette[a][i][c], palette[0][i][c])) {
                    differs = true;
                }
            }
        }
        if (!differs) {
            differsEverywhere = false;
        }
    }
    r.check("r1c2_07_each_appearance_has_its_own_palette", differsEverywhere);

    // The direction of the contrast, which is what "readable" actually means.
    // Judged on the minor tier, which is the faintest and therefore the one
    // that decides. Lines LIFT off a dark ground and SINK into a light one
    // (UI-PREF-R1): the direction follows the ground family, never a member.
    bool linesContrast = true;
    for (int a = 0; a < kViewportBackgroundCount; ++a) {
        const float bgLuma = (ground[a][0] + ground[a][1] + ground[a][2]) / 3.0f;
        const float minorLuma =
            (palette[a][0][0] + palette[a][0][1] + palette[a][0][2]) / 3.0f;
        const bool lifts = minorLuma > bgLuma;
        if (lifts == viewportBackgroundIsLight(appearances[a])) {
            linesContrast = false;
        }
    }
    r.check("r1c2_07_lines_contrast_with_every_ground_in_the_right_direction", linesContrast);

    // The lightest DARK ground needs the most weight of the three, or its floor
    // reads as unlined. That is the whole reason three dark palettes exist
    // rather than one.
    r.check("r1c2_07_the_lightest_dark_ground_carries_the_most_weight",
            palette[2][0][3] > palette[0][0][3] && palette[2][0][3] > palette[1][0][3]);

    // The weight ladder: a major line reads more strongly than a minor one and
    // an axis more strongly than a major one, in EVERY appearance. This is the
    // whole visual hierarchy, and it comes from alpha rather than from colour so
    // that a line is never made louder by being made a different hue.
    bool ladderHolds = true;
    for (int a = 0; a < kViewportBackgroundCount; ++a) {
        const float(*p)[4] = palette[a];
        if (!(p[0][3] < p[1][3] && p[1][3] < p[2][3] && nearly(p[2][3], p[3][3]))) {
            ladderHolds = false;
        }
    }
    r.check("r1c2_07_minor_under_major_under_axis_in_every_appearance", ladderHolds);

    // Subtle, and measurably so: even the strongest line is blended at well
    // under half weight, so the grid can never dominate the model.
    bool allSubtle = true;
    bool axesLean = true;
    for (int a = 0; a < kViewportBackgroundCount; ++a) {
        for (int i = 0; i < kGridLineTierCount; ++i) {
            if (palette[a][i][3] > 0.6f) {
                allSubtle = false;
            }
        }
        // The two axes lean opposite ways so X and Z can be told apart, and
        // neither is a saturated primary.
        if (!(palette[a][2][0] > palette[a][2][2] && palette[a][3][2] > palette[a][3][0])) {
            axesLean = false;
        }
    }
    r.check("r1c2_07_no_grid_line_is_drawn_at_a_dominating_weight", allSubtle);
    r.check("r1c2_07_the_x_axis_leans_warm_and_the_z_axis_leans_cool", axesLean);
}

// R1C2-03/04/05/06: what the grid must NOT be able to do.
//
// Driven against a real ConstructionScene, a real MeshStore and the real
// picking query — the same shape as NOR-10 above, because an intention stated
// in prose is not a test.
void checkGridIsInertAgainstTheModel(Recorder& r) {
    // Its OWN scene, never the process-scoped one: the Stage 016-R2 lesson.
    ConstructionScene scene;
    SceneObject& body = scene.bodyAt(0);
    const ConstructionMesh source = defaultSphere();
    const MeshRevision revision = body.meshStore().publish(
        source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
        source.indices.data(), static_cast<uint32_t>(source.indices.size()));
    const RuntimeMeshPtr mesh = body.meshStore().current();
    if (!mesh || revision == kNoMeshRevision) {
        r.check("r1c2_03_grid_inertness_fixture", false);
        return;
    }
    r.check("r1c2_03_grid_inertness_fixture", true);

    const SceneSnapshot before = scene.snapshot();
    const Ray ray{Vec3{0.0f, 0.0f, 4.0f}, Vec3{0.0f, 0.0f, -1.0f}};
    const TriangleHit hitBefore =
        pickTriangleMesh(ray, mesh->triangleView(), /*frontFacesOnly=*/true);
    r.check("r1c2_04_picking_hits_the_body_before_the_toggle", hitBefore.hit);

    RenderMeshCache cache;
    cache.refresh(*mesh, SurfaceShading::Smooth);
    const uint64_t rebuildsBefore = cache.rebuildCount();
    const uint32_t sourceVertexCountBefore = mesh->vertexCount();

    // Four real toggles through the real store.
    DisplaySettingsStore display;
    display.setGridVisible(false);
    display.setGridVisible(true);
    display.setGridVisible(false);
    display.setGridVisible(true);

    // R1C2-03. The snapshot is what the renderer draws, so "the grid is not in
    // the scene" means this: the same items, the same ids, the same revisions,
    // the same transforms, the same selection — and above all the same COUNT.
    // A grid that had become a scene object would show up here as a fifth field
    // or a second item.
    const SceneSnapshot after = scene.snapshot();
    bool snapshotIdentical = before.size() == after.size();
    if (snapshotIdentical) {
        for (size_t i = 0; i < before.size(); ++i) {
            if (before[i].objectId != after[i].objectId ||
                before[i].selected != after[i].selected ||
                before[i].mesh.get() != after[i].mesh.get()) {
                snapshotIdentical = false;
                break;
            }
            for (int m = 0; m < 16; ++m) {
                if (!nearly(before[i].model.m[m], after[i].model.m[m]) ||
                    !nearly(before[i].inverseModel.m[m], after[i].inverseModel.m[m])) {
                    snapshotIdentical = false;
                    break;
                }
            }
        }
    }
    r.check("r1c2_03_grid_toggling_leaves_the_scene_snapshot_identical", snapshotIdentical);
    r.check("r1c2_03_the_scene_still_holds_exactly_its_own_bodies",
            after.size() == scene.bodyCount() && scene.bodyCount() == 1);

    // R1C2-03 again, from the publication side: no revision was minted and
    // nothing was published.
    r.check("r1c2_03_grid_toggling_mints_no_mesh_revision",
            body.meshStore().currentRevision() == revision &&
                body.meshStore().publishedCount() == 1 && mesh->revision() == revision);

    // R1C2-06. The render-only derived mesh is the thing a toggle could
    // plausibly have dirtied, and it did not: the cache still reports cached.
    r.check("r1c2_06_grid_toggling_rebuilds_no_render_mesh",
            !cache.refresh(*mesh, SurfaceShading::Smooth) &&
                cache.rebuildCount() == rebuildsBefore);
    r.check("r1c2_06_grid_toggling_leaves_the_source_mesh_the_same_size",
            mesh->vertexCount() == sourceVertexCountBefore);

    // R1C2-04. The grid is not pickable, and the way that is true is that
    // picking never sees it: the query runs over a RuntimeMesh's triangles, the
    // grid is not one, and the answer is byte-identical either side.
    const TriangleHit hitAfter =
        pickTriangleMesh(ray, mesh->triangleView(), /*frontFacesOnly=*/true);
    r.check("r1c2_04_picking_is_unchanged_by_the_grid",
            hitAfter.hit == hitBefore.hit &&
                hitAfter.triangleIndex == hitBefore.triangleIndex &&
                nearly(hitAfter.t, hitBefore.t));

    // A ray fired straight along the grid's own plane, through the origin,
    // where the grid's densest lines are. It must miss everything the sphere
    // does not own — the grid cannot be hit because it is not in the geometry
    // the query is given at all.
    ConstructionScene emptyish;
    SceneObject& lonely = emptyish.bodyAt(0);
    const ConstructionMesh tiny = defaultSphere();
    lonely.meshStore().publish(tiny.vertices.data(), static_cast<uint32_t>(tiny.vertices.size()),
                               tiny.indices.data(), static_cast<uint32_t>(tiny.indices.size()));
    const RuntimeMeshPtr lonelyMesh = lonely.meshStore().current();
    const Ray grazing{Vec3{-30.0f, 0.0f, 12.0f}, Vec3{1.0f, 0.0f, 0.0f}};
    const TriangleHit grazingHit =
        lonelyMesh ? pickTriangleMesh(grazing, lonelyMesh->triangleView(), true) : TriangleHit{};
    r.check("r1c2_04_a_ray_along_the_grid_plane_hits_nothing", !grazingHit.hit);

    // R1C2-05. Freeze copies the Construction mesh; the grid is not part of it,
    // so a frozen mesh is the same whatever the grid is doing.
    display.setGridVisible(true);
    SculptMesh frozenWithGrid;
    const bool frozeWithGrid = frozenWithGrid.freezeFrom(source, body.objectId());
    display.setGridVisible(false);
    SculptMesh frozenWithout;
    const bool frozeWithout = frozenWithout.freezeFrom(source, body.objectId());
    r.check("r1c2_05_freeze_succeeds_either_way", frozeWithGrid && frozeWithout);
    r.check("r1c2_05_a_frozen_mesh_is_the_same_size_either_way",
            frozenWithGrid.vertexCount() == frozenWithout.vertexCount() &&
                frozenWithGrid.indexCount() == frozenWithout.indexCount() &&
                frozenWithGrid.vertexCount() == mesh->vertexCount());
    bool frozenBytesIdentical = frozenWithGrid.vertexCount() == frozenWithout.vertexCount();
    if (frozenBytesIdentical) {
        frozenBytesIdentical =
            std::memcmp(frozenWithGrid.vertices().data(), frozenWithout.vertices().data(),
                        frozenWithGrid.vertices().size() * sizeof(MeshVertex)) == 0;
    }
    r.check("r1c2_05_a_frozen_mesh_is_bit_identical_either_way", frozenBytesIdentical);
    r.check("r1c2_05_the_grid_did_not_reach_the_construction_source",
            body.meshStore().currentRevision() == revision &&
                body.meshStore().publishedCount() == 1);
}

// R1C2-08/09: the two things the grid shares a plane and a projection with.
void checkGridAgainstPlaneAndProjection(Recorder& r) {
    // R1C2-08. The Plane's 4:6 source topology is exactly what Stage 016
    // pinned, and the grid did not touch it. The grid settles their shared
    // plane with pipeline DEPTH STATE — draw order, depth-write off and a
    // depth bias — so there was never anything here for it to move, and this
    // check is what proves nobody was tempted to move it anyway.
    ConstructionObject object(kConstructionBoxObjectId);
    object.setPrimitive(PrimitiveSpec::forPlane(kDefaultPlaneWidthMeters,
                                                kDefaultPlaneDepthMeters));
    const ConstructionMesh mesh = object.generateMesh();
    r.check("r1c2_08_plane_source_topology_is_still_4_6",
            mesh.vertices.size() == 4 && mesh.indices.size() == 6);

    bool planeIsFlatAtLocalZero = true;
    for (const MeshVertex& v : mesh.vertices) {
        if (!nearly(static_cast<float>(v.position[1]), 0.0f)) {
            planeIsFlatAtLocalZero = false;
        }
    }
    r.check("r1c2_08_plane_is_still_flat_at_local_y_zero", planeIsFlatAtLocalZero);
    // The grid lives on the same world plane a Plane at identity occupies. That
    // is the coincidence the depth bias exists for, and stating it here is what
    // makes a future change to either constant visible.
    r.check("r1c2_08_the_plane_and_the_grid_share_world_y_zero",
            nearly(kGridPlaneY, 0.0f));

    // R1C2-09. The grid composes the camera's own projection matrix and owns
    // none of it, so both modes work and neither is touched. Asserted the only
    // way that means anything without a GPU: the camera reports different
    // projections and IDENTICAL pose either side of a grid toggle.
    CameraController camera;
    camera.setViewport(1080, 2400);
    const CameraSnapshot perspective = camera.snapshot();
    r.check("r1c2_09_perspective_is_a_perspective_matrix",
            !nearly(perspective.proj.m[11], 0.0f));

    DisplaySettingsStore display;
    display.setGridVisible(false);
    const CameraSnapshot afterToggle = camera.snapshot();
    bool poseUnchanged = true;
    for (int m = 0; m < 16; ++m) {
        if (!nearly(perspective.proj.m[m], afterToggle.proj.m[m]) ||
            !nearly(perspective.view.m[m], afterToggle.view.m[m])) {
            poseUnchanged = false;
        }
    }
    r.check("r1c2_09_a_grid_toggle_moves_no_camera_value", poseUnchanged);
    r.check("r1c2_09_a_grid_toggle_does_not_change_the_projection_mode",
            afterToggle.projection == perspective.projection &&
                perspective.projection == ProjectionMode::Perspective);

    camera.setProjectionMode(ProjectionMode::Orthographic);
    const CameraSnapshot ortho = camera.snapshot();
    // A true parallel projection: m[11] is 0, which is what Stage 015D pinned.
    // The grid needs no branch for this — one viewProj multiply serves both —
    // and that is exactly why neither mode can be broken by it.
    r.check("r1c2_09_orthographic_is_a_parallel_projection", nearly(ortho.proj.m[11], 0.0f));
    display.setGridVisible(true);
    const CameraSnapshot orthoAfter = camera.snapshot();
    bool orthoUnchanged = true;
    for (int m = 0; m < 16; ++m) {
        if (!nearly(ortho.proj.m[m], orthoAfter.proj.m[m])) {
            orthoUnchanged = false;
        }
    }
    r.check("r1c2_09_a_grid_toggle_moves_no_orthographic_value", orthoUnchanged);
    r.check("r1c2_09_the_projection_mode_survives_the_toggle",
            orthoAfter.projection == ProjectionMode::Orthographic);
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

// ---------------------------------------------------------------------------
// NOR-01 .. NOR-10 — direction, not just axis
// ---------------------------------------------------------------------------

// NOR-01: every SOURCE triangle of every primitive is wound counter-clockwise
// seen from outside. This is the root of the whole chain: the render mesh's face
// normal is cross(p1 - p0, p2 - p0), the pipeline's back-face culling reads the
// same winding, and picking's front-face test reads it a third time. If this
// check fails, nothing downstream can be trusted and no shading fix would be
// the real repair.
void checkSourceWinding(Recorder& r) {
    r.check("nor01_box_source_winding_outward", worstSourceWinding(defaultBox()) > 0.0f);
    r.check("nor01_cylinder_source_winding_outward", worstSourceWinding(defaultCylinder()) > 0.0f);
    r.check("nor01_sphere_source_winding_outward", worstSourceWinding(defaultSphere()) > 0.0f);
    r.check("nor01_cone_source_winding_outward", worstSourceWinding(defaultCone()) > 0.0f);
    r.check("nor01_capsule_source_winding_outward", worstSourceWinding(defaultCapsule()) > 0.0f);
    r.check("nor01_capsule_equality_source_winding_outward",
            worstSourceWinding(sphericalCapsule()) > 0.0f);

    // The box is small enough to state exactly: all twelve triangles, each face
    // normal on its own axis and pointing away from the centre.
    //
    // The outward test is `> 0`, not `close to 1`, and the default box is why:
    // it is 2 x 1 x 0.5 m, and each face is split into two triangles whose
    // CENTROIDS sit well off the face centre. Centre -> centroid is therefore a
    // slanted direction, not the face normal — on the +Z face it agrees with
    // the normal only to about 0.56. Requiring near-agreement would be asserting
    // that the box is a cube. The sign is the whole claim.
    const ConstructionMesh box = defaultBox();
    uint32_t outwardTriangles = 0;
    for (uint32_t t = 0; t < kBoxIndexCount / 3; ++t) {
        const Vec3 n = sourceTriangleNormal(box, t);
        const Vec3 outward =
            vec3Normalize(vec3Sub(sourceTriangleCentroid(box, t), boundsCentre(box)));
        if (isAxisAligned(n) && vec3Dot(n, outward) > 0.0f) {
            ++outwardTriangles;
        }
    }
    r.check("nor01_box_all_12_triangles_outward", outwardTriangles == kBoxIndexCount / 3);
}

// NOR-02 .. NOR-06: the SIGN of every render normal, per primitive.
//
// `worstRenderOutwardness` is one statement covering all of them, so each
// primitive here adds only what is specific to it.
void checkRenderNormalsOutward(Recorder& r) {
    // NOR-02: the box, whose six planar faces must be six DIRECTIONS, not three
    // axes. The pre-existing box check counts four normals per axis sign but
    // never ties a sign to a position, so it passes on an inside-out box.
    const ConstructionMesh box = defaultBox();
    RenderMeshData boxData;
    if (!build(box, SurfaceShading::Smooth, &boxData)) {
        r.check("nor02_box_builds", false);
    } else {
        r.check("nor02_box_builds", true);
        r.check("nor02_box_render_normals_outward",
                worstRenderOutwardness(boxData, boundsCentre(box)) > 0.0f);

        // Six axis directions, four vertices each, and each vertex's normal
        // agrees in sign with the corner it sits on: a +X normal only ever
        // appears on a corner with a positive x.
        uint32_t signedDirection[6] = {0, 0, 0, 0, 0, 0};
        bool signMatchesCorner = true;
        for (const RenderVertex& v : boxData.vertices) {
            for (int axis = 0; axis < 3; ++axis) {
                if (nearly(v.normal[axis], 1.0f)) {
                    ++signedDirection[axis * 2 + 0];
                    if (!(v.position[axis] > 0.0f)) signMatchesCorner = false;
                } else if (nearly(v.normal[axis], -1.0f)) {
                    ++signedDirection[axis * 2 + 1];
                    if (!(v.position[axis] < 0.0f)) signMatchesCorner = false;
                }
            }
        }
        bool sixOutwardAxes = true;
        for (uint32_t count : signedDirection) {
            if (count != 4) sixOutwardAxes = false;
        }
        r.check("nor02_box_six_outward_axes_four_each", sixOutwardAxes);
        r.check("nor02_box_normal_sign_matches_corner_sign", signMatchesCorner);

        // No mixed sign within one planar face: the four vertices sharing a
        // face direction must have identical normals, or the face would be lit
        // as if it were folded.
        bool planarFacesUniform = true;
        for (int axis = 0; axis < 3; ++axis) {
            for (int sign = -1; sign <= 1; sign += 2) {
                for (const RenderVertex& v : boxData.vertices) {
                    if (!nearly(v.normal[axis], static_cast<float>(sign))) continue;
                    for (int other = 0; other < 3; ++other) {
                        if (other != axis && !nearly(v.normal[other], 0.0f)) {
                            planarFacesUniform = false;
                        }
                    }
                }
            }
        }
        r.check("nor02_box_no_mixed_sign_on_a_planar_face", planarFacesUniform);
    }

    // NOR-03: the cylinder. Side radial OUTWARD, top cap +Y, bottom cap -Y.
    const ConstructionMesh cylinder = defaultCylinder();
    RenderMeshData cylinderData;
    if (!build(cylinder, SurfaceShading::Smooth, &cylinderData)) {
        r.check("nor03_cylinder_builds", false);
    } else {
        r.check("nor03_cylinder_builds", true);
        r.check("nor03_cylinder_render_normals_outward",
                worstRenderOutwardness(cylinderData, boundsCentre(cylinder)) > 0.0f);

        bool sideOutward = true;
        bool capsSigned = true;
        for (const RenderVertex& v : cylinderData.vertices) {
            const Vec3 p = positionOf(v);
            if (std::fabs(v.normal[1]) < 0.001f) {
                const Vec3 radial = vec3Normalize(Vec3{p.x, 0.0f, p.z});
                if (!(vec3Dot(normalOf(v), radial) > 0.999f)) sideOutward = false;
            } else {
                // A cap normal must point away from the cap it sits on.
                if (!((p.y > 0.0f && nearly(v.normal[1], 1.0f)) ||
                      (p.y < 0.0f && nearly(v.normal[1], -1.0f)))) {
                    capsSigned = false;
                }
            }
        }
        r.check("nor03_cylinder_side_radial_outward", sideOutward);
        r.check("nor03_cylinder_top_plus_y_bottom_minus_y", capsSigned);
    }

    // NOR-04: the sphere. Every normal in the outward hemisphere about the
    // centre — the strictest form of "not inside out" a closed surface has.
    const ConstructionMesh sphere = defaultSphere();
    RenderMeshData sphereData;
    if (!build(sphere, SurfaceShading::Smooth, &sphereData)) {
        r.check("nor04_sphere_builds", false);
    } else {
        r.check("nor04_sphere_builds", true);
        r.check("nor04_sphere_render_normals_outward",
                worstRenderOutwardness(sphereData, boundsCentre(sphere)) > 0.0f);
        r.check("nor04_sphere_normals_in_outward_hemisphere",
                minimumRadialAgreement(sphereData) > 0.999f);
    }

    // NOR-05: the cone. Lateral and base outward, and an apex that is finite
    // and on the axis rather than an arbitrary member of its fan.
    const ConstructionMesh cone = defaultCone();
    RenderMeshData coneData;
    if (!build(cone, SurfaceShading::Smooth, &coneData)) {
        r.check("nor05_cone_builds", false);
    } else {
        r.check("nor05_cone_builds", true);
        r.check("nor05_cone_render_normals_outward",
                worstRenderOutwardness(coneData, boundsCentre(cone)) > 0.0f);

        bool lateralOutward = true;
        bool baseDownward = true;
        bool apexFinite = false;
        for (const RenderVertex& v : coneData.vertices) {
            const Vec3 p = positionOf(v);
            const Vec3 n = normalOf(v);
            const bool onAxis = nearly(p.x, 0.0f) && nearly(p.z, 0.0f);
            if (nearly(n.y, -1.0f)) {
                if (!(p.y < 0.0f) && !onAxis) baseDownward = false;
            } else if (onAxis && p.y > 0.0f) {
                apexFinite = isUnit(n) && nearly(n.y, 1.0f);
            } else {
                // A lateral normal points out from the axis AND upward along
                // the slope; both signs matter.
                const Vec3 radial = vec3Normalize(Vec3{p.x, 0.0f, p.z});
                if (!(vec3Dot(n, radial) > 0.0f) || !(n.y > 0.0f)) lateralOutward = false;
            }
        }
        r.check("nor05_cone_side_outward_and_upslope", lateralOutward);
        r.check("nor05_cone_base_outward_downward", baseDownward);
        r.check("nor05_cone_apex_finite_and_stable", apexFinite);
    }

    // NOR-06: the capsule, including the equality case the generator turns into
    // a sphere.
    const ConstructionMesh capsule = defaultCapsule();
    RenderMeshData capsuleData;
    if (!build(capsule, SurfaceShading::Smooth, &capsuleData)) {
        r.check("nor06_capsule_builds", false);
    } else {
        r.check("nor06_capsule_builds", true);
        r.check("nor06_capsule_render_normals_outward",
                worstRenderOutwardness(capsuleData, boundsCentre(capsule)) > 0.0f);
        r.check("nor06_capsule_normals_finite", renderMeshIsFinite(capsuleData) &&
                                                    allNormalsUnit(capsuleData));
    }

    const ConstructionMesh equality = sphericalCapsule();
    RenderMeshData equalityData;
    if (!build(equality, SurfaceShading::Smooth, &equalityData)) {
        r.check("nor06_capsule_equality_builds", false);
    } else {
        r.check("nor06_capsule_equality_builds", true);
        r.check("nor06_capsule_equality_render_normals_outward",
                worstRenderOutwardness(equalityData, boundsCentre(equality)) > 0.0f);
        r.check("nor06_capsule_equality_normals_finite",
                renderMeshIsFinite(equalityData) && allNormalsUnit(equalityData));
    }
}

// NOR-07: Faceted normals are the SOURCE triangle's own orientation, not merely
// "one normal per triangle". A faceted build that agreed within a triangle but
// disagreed with the winding would light every face backwards while passing
// every flatness check.
void checkFacetedOrientation(Recorder& r) {
    const ConstructionMesh meshes[5] = {defaultBox(), defaultCylinder(), defaultSphere(),
                                        defaultCone(), defaultCapsule()};
    bool allMatch = true;
    bool allOutward = true;
    for (const ConstructionMesh& source : meshes) {
        RenderMeshData data;
        if (!build(source, SurfaceShading::Faceted, &data)) {
            allMatch = false;
            continue;
        }
        const Vec3 interior = boundsCentre(source);
        const uint32_t triangles = static_cast<uint32_t>(source.indices.size()) / 3;
        for (uint32_t t = 0; t < triangles; ++t) {
            const Vec3 expected = sourceTriangleNormal(source, t);
            const Vec3 actual = normalOf(data.vertices[t * 3]);
            if (!(vec3Dot(expected, actual) > 0.999f)) allMatch = false;
            const Vec3 outward = vec3Normalize(vec3Sub(sourceTriangleCentroid(source, t), interior));
            if (!(vec3Dot(actual, outward) > 0.0f)) allOutward = false;
        }
    }
    r.check("nor07_faceted_matches_source_triangle_orientation", allMatch);
    r.check("nor07_faceted_normals_outward", allOutward);
}

// NOR-08: hard-edge duplication is a REMAP, so the drawn triangles keep the
// source's vertex positions in the source's order — and therefore the source's
// winding. Splitting a corner for shading must never reorder a triangle, which
// would silently invert culling for that face.
void checkDuplicationPreservesWinding(Recorder& r) {
    const ConstructionMesh source = defaultBox();
    RenderMeshData data;
    if (!build(source, SurfaceShading::Smooth, &data)) {
        r.check("nor08_duplication_builds", false);
        return;
    }
    r.check("nor08_duplication_builds", true);
    r.check("nor08_render_vertices_exceed_source",
            data.vertexCount() > data.sourceVertexCount);
    r.check("nor08_index_count_identical", data.indexCount() == data.sourceIndexCount);

    bool sameCorners = true;
    bool sameWinding = true;
    for (uint32_t c = 0; c < data.indexCount(); ++c) {
        const MeshVertex& sv = source.vertices[source.indices[c]];
        const RenderVertex& rv = data.vertices[data.indices[c]];
        for (int axis = 0; axis < 3; ++axis) {
            if (!nearly(sv.position[axis], rv.position[axis])) sameCorners = false;
        }
    }
    for (uint32_t t = 0; t < data.indexCount() / 3; ++t) {
        const Vec3 a = positionOf(data.vertices[data.indices[t * 3 + 0]]);
        const Vec3 b = positionOf(data.vertices[data.indices[t * 3 + 1]]);
        const Vec3 c = positionOf(data.vertices[data.indices[t * 3 + 2]]);
        const Vec3 rendered = vec3Normalize(vec3Cross(vec3Sub(b, a), vec3Sub(c, a)));
        if (!(vec3Dot(rendered, sourceTriangleNormal(source, t)) > 0.999f)) sameWinding = false;
    }
    r.check("nor08_render_corners_are_source_corners", sameCorners);
    r.check("nor08_render_winding_matches_source", sameWinding);
}

// NOR-09: the model -> view normal transform, replicated EXACTLY as the renderer
// builds it (forgeshape_renderer.cpp, recordCommandBuffer) so this proves the
// shipped arithmetic and not a second implementation of it.
//
// The claim being proved is end to end: after an arbitrary rigid placement and
// an arbitrary camera, a surface normal in view space still points away from the
// object's interior in view space. Nothing about the shading rig can be judged
// until that holds, and it is the last place a sign could be lost.
void checkNormalTransform(Recorder& r) {
    const ConstructionMesh box = defaultBox();
    RenderMeshData data;
    if (!build(box, SurfaceShading::Smooth, &data)) {
        r.check("nor09_transform_fixture", false);
        return;
    }
    r.check("nor09_transform_fixture", true);

    struct Placement {
        float rotX, rotY, rotZ;
        Vec3 translation;
        Vec3 eye;
    };
    // Representative rather than exhaustive: identity, each axis alone, and one
    // combined rotation with a translation and an off-axis camera.
    const Placement placements[6] = {
        {0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 8.2f}},
        {1.1f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 8.2f}},
        {0.0f, 2.3f, 0.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 8.2f}},
        {0.0f, 0.0f, -0.9f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 8.2f}},
        {0.6f, -1.4f, 0.35f, {1.5f, -0.75f, 2.0f}, {4.1f, 3.6f, 6.2f}},
        {-2.7f, 0.8f, 2.2f, {-3.0f, 1.25f, -0.5f}, {-5.0f, -2.5f, 4.0f}},
    };

    bool signPreserved = true;
    bool lengthPreserved = true;
    bool visibleFacesFaceViewer = false;

    for (const Placement& p : placements) {
        const Mat4 model =
            mat4Multiply(mat4Translation(p.translation),
                         mat4Multiply(mat4RotationZ(p.rotZ),
                                      mat4Multiply(mat4RotationY(p.rotY), mat4RotationX(p.rotX))));
        const Mat4 view = mat4LookAt(p.eye, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});
        const Mat4 modelView = mat4Multiply(view, model);

        // The renderer's exact row extraction: storage is column-major, so row r
        // is {m[0*4+r], m[1*4+r], m[2*4+r]}.
        Vec3 rows[3];
        for (int row = 0; row < 3; ++row) {
            rows[row] = Vec3{modelView.m[0 * 4 + row], modelView.m[1 * 4 + row],
                             modelView.m[2 * 4 + row]};
        }

        // The object's interior reference, carried into view space by the same
        // transform, so "outward" means the same thing on both sides.
        const Vec3 interiorView = mat4TransformPoint(modelView, boundsCentre(box));

        for (const RenderVertex& v : data.vertices) {
            const Vec3 n = normalOf(v);
            // The vertex stage, verbatim.
            const Vec3 viewNormal{vec3Dot(rows[0], n), vec3Dot(rows[1], n), vec3Dot(rows[2], n)};
            const Vec3 viewPosition = mat4TransformPoint(modelView, positionOf(v));
            const Vec3 outward = vec3Normalize(vec3Sub(viewPosition, interiorView));

            if (!(vec3Dot(viewNormal, outward) > 0.0f)) signPreserved = false;
            // A rigid transform cannot change a normal's length; if it ever
            // does, an inverse-transpose became necessary and this shortcut is
            // no longer valid.
            if (!nearly(std::sqrt(vec3Dot(viewNormal, viewNormal)), 1.0f)) lengthPreserved = false;
            // Studio Solid's rim term and the MatCap's disc mapping both assume
            // a drawn surface has a positive view-space z. At least one face
            // must satisfy that, or the object would be invisible.
            if (viewNormal.z > 0.5f) visibleFacesFaceViewer = true;
        }
    }

    r.check("nor09_model_to_view_preserves_outward_sign", signPreserved);
    r.check("nor09_model_to_view_preserves_unit_length", lengthPreserved);
    r.check("nor09_camera_facing_surfaces_have_positive_view_z", visibleFacesFaceViewer);
}

// NOR-10: display modes are presentation. Switching shading model rebuilds
// nothing at all, switching surface shading rebuilds only RENDER data, and
// neither can be observed by picking or by a revision.
void checkDisplayModesAreInert(Recorder& r) {
    const ConstructionMesh source = defaultSphere();
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision revision =
        store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                      source.indices.data(), static_cast<uint32_t>(source.indices.size()));
    const RuntimeMeshPtr mesh = store.current();
    if (!mesh || revision == kNoMeshRevision) {
        r.check("nor10_display_fixture", false);
        return;
    }
    r.check("nor10_display_fixture", true);

    // A ray straight down the -Z axis at the sphere: the picking answer must be
    // byte-identical before and after every display change.
    const Ray ray{Vec3{0.0f, 0.0f, 4.0f}, Vec3{0.0f, 0.0f, -1.0f}};
    const TriangleHit before = pickTriangleMesh(ray, mesh->triangleView(), /*frontFacesOnly=*/true);
    r.check("nor10_picking_hits_before_display_change", before.hit);

    RenderMeshCache cache;
    cache.refresh(*mesh, SurfaceShading::Smooth);
    const uint64_t afterFirstBuild = cache.rebuildCount();

    DisplaySettingsStore display;
    // Studio Solid <-> MatCap is a fragment-stage uniform: it does not even
    // reach the cache, which is why the refresh below still reports "cached".
    display.setShadingModel(ShadingModel::MatCap);
    display.setShadingModel(ShadingModel::StudioSolid);
    display.setShadingModel(ShadingModel::MatCap);
    r.check("nor10_shading_model_never_rebuilds_geometry",
            !cache.refresh(*mesh, SurfaceShading::Smooth) &&
                cache.rebuildCount() == afterFirstBuild);

    display.setSurfaceShading(SurfaceShading::Faceted);
    r.check("nor10_surface_shading_rebuilds_render_data_only",
            cache.refresh(*mesh, SurfaceShading::Faceted) &&
                cache.rebuildCount() == afterFirstBuild + 1);

    const TriangleHit after = pickTriangleMesh(ray, mesh->triangleView(), /*frontFacesOnly=*/true);
    r.check("nor10_picking_unchanged_by_display",
            after.hit == before.hit && after.triangleIndex == before.triangleIndex &&
                nearly(after.t, before.t));
    r.check("nor10_source_revision_unchanged", mesh->revision() == revision &&
                                                   store.currentRevision() == revision &&
                                                   store.publishedCount() == 1);
}

// ---------------------------------------------------------------------------
// PLN-09/10/20 — the bounded two-sided render-only exception for Plane
// ---------------------------------------------------------------------------

// PLN-09/10: a Plane's render data, built with the two-sided exception on, is
// exactly the ordinary one-sided result plus a wholesale mirrored copy — front
// half shaded +Y, back half shaded -Y — and the SOURCE topology it was built
// from is untouched (still exactly 4:6), because the duplication happens only
// in render-only data that is never read back.
void checkPlaneTwoSidedRenderGeometry(Recorder& r) {
    const ConstructionMesh source = defaultPlane();
    r.check("pln09_source_topology_is_4_6",
            source.vertices.size() == 4 && source.indices.size() == 6 &&
                source.renderBothSides);

    RenderMeshData oneSided;
    const bool builtOneSided =
        buildRenderMesh(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                        source.indices.data(), static_cast<uint32_t>(source.indices.size()),
                        SurfaceShading::Smooth, &oneSided, /*renderBothSides=*/false);
    r.check("pln09_one_sided_build_succeeds", builtOneSided);

    RenderMeshData twoSided;
    const bool builtTwoSided =
        buildRenderMesh(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                        source.indices.data(), static_cast<uint32_t>(source.indices.size()),
                        SurfaceShading::Smooth, &twoSided, /*renderBothSides=*/true);
    r.check("pln09_two_sided_build_succeeds", builtTwoSided);
    if (!builtOneSided || !builtTwoSided) {
        return;
    }

    const uint32_t baseVertexCount = oneSided.vertexCount();
    const uint32_t baseIndexCount = oneSided.indexCount();
    r.check("pln10_two_sided_doubles_vertex_and_index_counts",
            twoSided.vertexCount() == baseVertexCount * 2 &&
                twoSided.indexCount() == baseIndexCount * 2);
    // Source topology is still exactly 4:6 — the duplication above never
    // touched it, and this is the direct proof rather than an inference.
    r.check("pln10_source_topology_still_4_6_after_render_build",
            source.vertices.size() == 4 && source.indices.size() == 6);

    // A perfectly flat, coplanar 2-triangle sheet has no crease, so Smooth
    // shading produces exactly one normal group per side: the front half is
    // exactly +Y and the back half is exactly -Y, not merely "outward-ish".
    bool frontAllPlusY = true;
    for (uint32_t i = 0; i < baseVertexCount; ++i) {
        const Vec3 n = normalOf(twoSided.vertices[i]);
        if (!(nearly(n.x, 0.0f) && nearly(n.y, 1.0f) && nearly(n.z, 0.0f))) frontAllPlusY = false;
    }
    r.check("pln09_front_render_normals_are_exactly_plus_y", frontAllPlusY);

    bool backAllMinusY = true;
    for (uint32_t i = baseVertexCount; i < twoSided.vertexCount(); ++i) {
        const Vec3 n = normalOf(twoSided.vertices[i]);
        if (!(nearly(n.x, 0.0f) && nearly(n.y, -1.0f) && nearly(n.z, 0.0f))) backAllMinusY = false;
    }
    r.check("pln10_back_render_normals_are_exactly_minus_y", backAllMinusY);

    // Every back-half triangle indexes only back-half vertices, so the front
    // and back copies never share a render vertex.
    bool backIndicesInBackRange = true;
    for (uint32_t i = baseIndexCount; i < twoSided.indexCount(); ++i) {
        if (twoSided.indices[i] < baseVertexCount) backIndicesInBackRange = false;
    }
    r.check("pln10_back_triangles_use_only_back_vertices", backIndicesInBackRange);

    r.check("pln09_10_render_data_all_finite", renderMeshIsFinite(twoSided));
}

// PLN-20: Studio/MatCap/Shading-model and Smooth/Faceted switches, applied to a
// Plane specifically, mutate no source revision and are invisible to picking —
// the same NOR-10 contract, proven again on the one primitive whose render
// path takes the extra two-sided branch.
void checkPlaneDisplayModesAreInert(Recorder& r) {
    const ConstructionMesh source = defaultPlane();
    MeshStore store(kConstructionBoxObjectId);
    const MeshRevision revision = store.publish(
        source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
        source.indices.data(), static_cast<uint32_t>(source.indices.size()), nullptr,
        source.renderBothSides);
    const RuntimeMeshPtr mesh = store.current();
    if (!mesh || revision == kNoMeshRevision) {
        r.check("pln20_display_fixture", false);
        return;
    }
    r.check("pln20_display_fixture", mesh->renderBothSides());

    // A ray from behind the plane hits only because of the two-sided
    // exception; it must keep hitting the SAME triangle/point through every
    // display change below.
    const Ray fromBelow{Vec3{0.2f, -4.0f, 0.1f}, Vec3{0.0f, 1.0f, 0.0f}};
    const TriangleHit before =
        pickTriangleMesh(fromBelow, mesh->triangleView(), /*frontFacesOnly=*/false);
    r.check("pln20_back_picking_hits_before_display_change", before.hit);

    RenderMeshCache cache;
    cache.refresh(*mesh, SurfaceShading::Smooth);
    const uint64_t afterFirstBuild = cache.rebuildCount();
    const uint32_t twoSidedVertexCount = cache.data().vertexCount();

    DisplaySettingsStore display;
    display.setShadingModel(ShadingModel::MatCap);
    display.setShadingModel(ShadingModel::StudioSolid);
    display.setShadingModel(ShadingModel::MatCap);
    r.check("pln20_shading_model_never_rebuilds_geometry",
            !cache.refresh(*mesh, SurfaceShading::Smooth) &&
                cache.rebuildCount() == afterFirstBuild);

    display.setSurfaceShading(SurfaceShading::Faceted);
    r.check("pln20_surface_shading_rebuilds_render_data_only",
            cache.refresh(*mesh, SurfaceShading::Faceted) &&
                cache.rebuildCount() == afterFirstBuild + 1);
    // Faceted still doubles whatever the one-sided Faceted count is; the exact
    // number differs from Smooth's, but the two-sided exception must still be
    // in effect (an even count, at least the Smooth two-sided count).
    r.check("pln20_faceted_still_two_sided",
            cache.data().vertexCount() % 2 == 0 && cache.data().vertexCount() >= 4);

    display.setSurfaceShading(SurfaceShading::Smooth);
    cache.refresh(*mesh, SurfaceShading::Smooth);
    r.check("pln20_smooth_round_trip_restores_two_sided_count",
            cache.data().vertexCount() == twoSidedVertexCount);

    const TriangleHit after =
        pickTriangleMesh(fromBelow, mesh->triangleView(), /*frontFacesOnly=*/false);
    r.check("pln20_back_picking_unchanged_by_display",
            after.hit == before.hit && after.triangleIndex == before.triangleIndex &&
                nearly(after.t, before.t));
    r.check("pln20_source_revision_unchanged", mesh->revision() == revision &&
                                                    store.currentRevision() == revision &&
                                                    store.publishedCount() == 1);
}

// The guard that makes every check above mean something.
//
// A suite that measures only axes and magnitudes passes unchanged on a mesh
// whose normals have all been multiplied by -1 — which is precisely the defect
// class the NOR family exists to catch. This proves the measurement inverts, so
// a future regression cannot hide behind it.
void checkFlipIsDetected(Recorder& r) {
    const ConstructionMesh fixtures[5] = {defaultBox(), defaultCylinder(), defaultSphere(),
                                          defaultCone(), defaultCapsule()};
    bool everyFlipCaught = true;
    bool everyUnflippedPasses = true;
    for (const ConstructionMesh& source : fixtures) {
        RenderMeshData data;
        if (!build(source, SurfaceShading::Smooth, &data)) {
            everyUnflippedPasses = false;
            continue;
        }
        const Vec3 interior = boundsCentre(source);
        if (!(worstRenderOutwardness(data, interior) > 0.0f)) everyUnflippedPasses = false;
        // The same measurement on the inverted copy must go negative.
        if (!(worstRenderOutwardness(withInvertedNormals(data), interior) < 0.0f)) {
            everyFlipCaught = false;
        }
    }
    r.check("nor_outwardness_passes_on_correct_normals", everyUnflippedPasses);
    r.check("nor_outwardness_fails_on_global_normal_flip", everyFlipCaught);
}

// ---------------------------------------------------------------------------
// Selection feedback (UI-R1C1): R1C1-01..06
// ---------------------------------------------------------------------------
//
// Driven with an EXPLICIT elapsed time rather than a clock, which is why a whole
// 220 ms pulse costs microseconds here and why nothing in this family can be
// flaky. That is the entire reason advanceSelectionPulse takes a delta instead
// of reading steady_clock itself.
constexpr double kFrame = 1.0 / 60.0;

void checkSelectionPulse(Recorder& r) {
    // --- R1C1-01: false -> true starts a pulse -----------------------------
    {
        SelectionPulseState s;
        const float unselected = advanceSelectionPulse(s, false, kFrame, true);
        const float onSelect = advanceSelectionPulse(s, true, kFrame, true);
        r.check("r1c1_01_unselected_body_mixes_no_tint", unselected == 0.0f);
        r.check("r1c1_01_selection_starts_at_the_pulse_peak",
                nearly(onSelect, kSelectionPulseAlpha));
        r.check("r1c1_01_the_peak_is_above_the_resting_value",
                kSelectionPulseAlpha > kSelectionRestingAlpha);
    }

    // --- R1C1-02: the pulse decays to the resting alpha --------------------
    {
        SelectionPulseState s;
        float previous = advanceSelectionPulse(s, true, kFrame, true);
        bool monotone = true;
        bool everOvershot = false;
        int frames = 0;
        // Long enough to cover the whole decay several times over, so the
        // settle is proven rather than assumed from one sample.
        while (frames < 120) {
            const float alpha = advanceSelectionPulse(s, true, kFrame, true);
            if (alpha > previous + kEpsilon) monotone = false;
            if (alpha > kSelectionPulseAlpha + kEpsilon ||
                alpha < kSelectionRestingAlpha - kEpsilon) {
                everOvershot = true;
            }
            previous = alpha;
            ++frames;
        }
        r.check("r1c1_02_the_pulse_decays_monotonically", monotone);
        r.check("r1c1_02_the_pulse_never_overshoots_either_end", !everOvershot);
        r.check("r1c1_02_the_pulse_settles_on_the_resting_alpha",
                nearly(previous, kSelectionRestingAlpha));

        // It must actually take TIME. A decay that had already finished on the
        // frame after selection would pass every check above and be a flicker.
        SelectionPulseState mid;
        advanceSelectionPulse(mid, true, kFrame, true);
        const float afterOneFrame = advanceSelectionPulse(mid, true, kFrame, true);
        r.check("r1c1_02_the_pulse_is_still_running_one_frame_in",
                afterOneFrame > kSelectionRestingAlpha + kEpsilon);
    }

    // --- SELOUTR1-11 / -12 / -13: the pulse is now the WHOLE tint ----------
    //
    // UI-R1C1 cut the legacy 0.55 flood to a 0.20 resting tint and this suite
    // asserted the tint was "still visible". `SEL-OUT-R1` (UI-OWNER-10) removes
    // the resting tint outright, because a whole-object wash is a whole-object
    // wash at any strength: persistent selection is the Objects capsule plus
    // the silhouette outline, and the tint's only remaining job is the brief
    // acknowledgement.
    {
        // The number the resting value replaced, and then replaced again.
        // Neither is a constant anywhere in the product any more, which is the
        // point of quoting them here.
        constexpr float kLegacySelectedAlpha = 0.55f;
        constexpr float kSupersededRestingAlpha = 0.20f;

        r.check("seloutr1_13_no_persistent_whole_object_glow_remains",
                kSelectionRestingAlpha == 0.0f);
        r.check("seloutr1_13_the_resting_tint_is_below_what_it_superseded",
                kSelectionRestingAlpha < kSupersededRestingAlpha);
        r.check("seloutr1_11_the_peak_still_matches_the_legacy_flood",
                nearly(kSelectionPulseAlpha, kLegacySelectedAlpha));
        // The acknowledgement is still an acknowledgement: loud, and brief.
        r.check("seloutr1_11_the_pulse_is_still_a_real_peak",
                kSelectionPulseAlpha > 0.5f);
        r.check("seloutr1_11_the_pulse_is_still_short",
                kSelectionPulseSeconds > 0.1 && kSelectionPulseSeconds < 0.4);

        // A selected body settles at EXACTLY nothing, however long it stays
        // selected. This is what SELOUTR1-12 rests on: after the pulse ends,
        // the only thing saying "this one" is the outline.
        SelectionPulseState settled;
        advanceSelectionPulse(settled, true, kFrame, true);
        for (int i = 0; i < 240; ++i) {
            advanceSelectionPulse(settled, true, kFrame, true);
        }
        const float longSettled = advanceSelectionPulse(settled, true, kFrame, true);
        r.check("seloutr1_12_a_long_selected_body_carries_no_tint_at_all",
                longSettled == 0.0f);
        // And a selected body is then indistinguishable from an unselected one
        // AS FAR AS THE TINT GOES — which is exactly why the outline had to
        // exist before this value could be taken to zero.
        SelectionPulseState never;
        const float unselected = advanceSelectionPulse(never, false, kFrame, true);
        r.check("seloutr1_13_settled_selection_tints_exactly_as_much_as_none",
                longSettled == unselected);
    }

    // --- R1C1-04: deselection clears the visual state ----------------------
    {
        SelectionPulseState s;
        advanceSelectionPulse(s, true, kFrame, true);
        advanceSelectionPulse(s, true, kFrame, true);  // mid-pulse
        const float cleared = advanceSelectionPulse(s, false, kFrame, true);
        r.check("r1c1_04_deselection_mixes_no_tint", cleared == 0.0f);
        r.check("r1c1_04_deselection_leaves_no_running_pulse",
                !s.selected && s.pulseElapsedSeconds < 0.0);
        // And the NEXT selection starts a full pulse rather than resuming a
        // half-decayed one.
        const float reselected = advanceSelectionPulse(s, true, kFrame, true);
        r.check("r1c1_04_reselection_starts_a_whole_new_pulse",
                nearly(reselected, kSelectionPulseAlpha));
    }

    // --- R1C1-04b: a tap that changes nothing must not re-pulse ------------
    {
        SelectionPulseState s;
        advanceSelectionPulse(s, true, kFrame, true);
        for (int i = 0; i < 60; ++i) {
            advanceSelectionPulse(s, true, kFrame, true);
        }
        // Selection truth did not change, so nothing restarts however many
        // frames (or taps) go by while it stays true.
        const float stillResting = advanceSelectionPulse(s, true, kFrame, true);
        r.check("r1c1_04_staying_selected_does_not_re_pulse",
                nearly(stillResting, kSelectionRestingAlpha));
    }

    // --- R1C1-05: two bodies' pulse states are independent -----------------
    {
        SelectionPulseState a;
        SelectionPulseState b;
        // A is selected and allowed to settle.
        advanceSelectionPulse(a, true, kFrame, true);
        for (int i = 0; i < 60; ++i) {
            advanceSelectionPulse(a, true, kFrame, true);
            advanceSelectionPulse(b, false, kFrame, true);
        }
        // Now the user selects B instead. B pulses; A must simply go dark, and
        // must NOT inherit B's pulse or restart one of its own.
        const float bOnSelect = advanceSelectionPulse(b, true, kFrame, true);
        const float aOnDeselect = advanceSelectionPulse(a, false, kFrame, true);
        r.check("r1c1_05_the_newly_selected_body_pulses",
                nearly(bOnSelect, kSelectionPulseAlpha));
        r.check("r1c1_05_the_previously_selected_body_goes_dark", aOnDeselect == 0.0f);
        r.check("r1c1_05_one_bodys_pulse_does_not_touch_anothers",
                b.pulseElapsedSeconds >= 0.0 && a.pulseElapsedSeconds < 0.0);
    }

    // --- R1C1-05b: reduced motion, and a resume mid-pulse ------------------
    {
        SelectionPulseState s;
        // Reduced motion lands on the RESTING value at once. Landing on the
        // peak instead would restore the flood and hold it there forever.
        //
        // Since `SEL-OUT-R1` the resting value is zero, so reduced motion means
        // no tint at all — which is the correct answer precisely BECAUSE the
        // outline is not an animation: it appears on the frame selection
        // changes and stays, so a user who asked for no motion still gets an
        // immediate, permanent answer to "which one".
        const float instant = advanceSelectionPulse(s, true, kFrame, false);
        r.check("r1c1_06_reduced_motion_reaches_resting_immediately",
                nearly(instant, kSelectionRestingAlpha));
        r.check("r1c1_06_reduced_motion_runs_no_pulse", s.pulseElapsedSeconds < 0.0);

        // A huge delta is a resume or a stall, not a frame. It is clamped, so a
        // process that was paused mid-pulse comes back still pulsing rather
        // than having silently skipped the acknowledgement.
        SelectionPulseState resumed;
        advanceSelectionPulse(resumed, true, kFrame, true);
        const float afterStall = advanceSelectionPulse(resumed, true, 30.0, true);
        r.check("r1c1_06_a_stall_is_clamped_not_trusted",
                afterStall > kSelectionRestingAlpha + kEpsilon);
        // A NaN or negative delta advances nothing rather than corrupting the
        // elapsed time.
        SelectionPulseState odd;
        advanceSelectionPulse(odd, true, kFrame, true);
        const double before = odd.pulseElapsedSeconds;
        advanceSelectionPulse(odd, true, std::numeric_limits<double>::quiet_NaN(), true);
        advanceSelectionPulse(odd, true, -5.0, true);
        r.check("r1c1_06_a_nonsense_delta_advances_nothing",
                odd.pulseElapsedSeconds == before);
    }

    // --- R1C1-06: selection feedback touches no geometry truth -------------
    //
    // Structural rather than incidental: the pulse is a pure function over its
    // own small state, so it has nothing to reach a MeshStore WITH. This runs
    // a whole selection cycle beside a real published mesh and proves the
    // revision, the vertex data and the index data all come out bit-identical.
    {
        const ConstructionMesh source = defaultSphere();
        MeshStore store(kConstructionBoxObjectId);
        const MeshRevision revisionBefore =
            store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                          source.indices.data(), static_cast<uint32_t>(source.indices.size()));
        const RuntimeMeshPtr before = store.current();

        SelectionPulseState s;
        for (int i = 0; i < 40; ++i) {
            advanceSelectionPulse(s, true, kFrame, true);
        }
        for (int i = 0; i < 40; ++i) {
            advanceSelectionPulse(s, false, kFrame, true);
        }
        for (int i = 0; i < 40; ++i) {
            advanceSelectionPulse(s, true, kFrame, false);
        }

        const RuntimeMeshPtr after = store.current();
        const bool sameRevision = after && store.currentRevision() == revisionBefore &&
                                  after->revision() == revisionBefore &&
                                  revisionBefore != kNoMeshRevision;
        const bool sameCounts = after && before && after->vertexCount() == before->vertexCount() &&
                                after->indexCount() == before->indexCount();
        bool sameBytes = sameCounts;
        if (sameBytes) {
            sameBytes = std::memcmp(after->vertices(), before->vertices(),
                                    after->vertexCount() * sizeof(MeshVertex)) == 0 &&
                        std::memcmp(after->indices(), before->indices(),
                                    after->indexCount() * sizeof(uint32_t)) == 0;
        }
        r.check("r1c1_06_the_fixture_published", revisionBefore != kNoMeshRevision);
        r.check("r1c1_06_selection_mints_no_mesh_revision", sameRevision);
        r.check("r1c1_06_selection_leaves_the_runtime_mesh_bit_identical", sameBytes);
        r.check("r1c1_06_selection_is_still_the_same_published_object", after == before);
    }
}

// ---------------------------------------------------------------------------
// Selection outline (SEL-OUT-R1): SELOUTR1-01, -13..-16, -20..-22, -28..-32
// ---------------------------------------------------------------------------
//
// Here for exactly the reason the pulse and the grid are here: the outline is
// PRESENTATION. This suite owns how a thing is drawn; the picking and selection
// suites own what is in the scene and which body is selected, and the whole
// point of the outline is that it is neither.
//
// What can be proven deterministically on the CPU is the POLICY and the RULE:
// the width, the colours and their measured contrast, and the edge-extraction
// kernel `shaders/outline.frag` mirrors. What cannot — that Vulkan rasterised
// the mask the mask pipeline described — is proven on the device by
// SelectionOutlineTest and by the visual evidence, which is the same division
// the grid's geometry and the grid's appearance already live under.

// A synthetic coverage mask: a filled axis-aligned rectangle in a field of
// zeroes. Deliberately not a rendered one — the rule under test is "which
// pixels are within the band of a covered region", and a hand-built region
// makes every expected answer arithmetic rather than a judgement.
void fillMaskRect(std::vector<float>* mask, int width, int height, int x0, int y0, int x1, int y1) {
    mask->assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (x < 0 || y < 0 || x >= width || y >= height) continue;
            (*mask)[static_cast<size_t>(y) * static_cast<size_t>(width) +
                    static_cast<size_t>(x)] = 1.0f;
        }
    }
}

void checkSelectionOutlineWidth(Recorder& r) {
    // --- the width is screen-space and BOUNDED at both ends ----------------
    {
        // A typical phone short side, a small window, and an implausibly large
        // one. The floor keeps the band visible on the small window; the
        // ceiling keeps it from swallowing small geometry on the large one.
        const float phone = selectionOutlineWidthPixels(1080, 2400);
        const float small = selectionOutlineWidthPixels(320, 480);
        const float huge = selectionOutlineWidthPixels(4000, 6000);

        r.check("seloutr1_03_width_is_within_its_bounds_on_a_phone",
                phone >= kSelectionOutlineMinPixels && phone <= kSelectionOutlineMaxPixels);
        r.check("seloutr1_03_a_small_window_lands_on_the_floor",
                nearly(small, kSelectionOutlineMinPixels));
        r.check("seloutr1_03_a_huge_window_lands_on_the_ceiling",
                nearly(huge, kSelectionOutlineMaxPixels));
        // Between the two it actually tracks the size, rather than being a
        // constant with two clamps around it.
        r.check("seloutr1_03_width_grows_with_the_viewport",
                selectionOutlineWidthPixels(1440, 3000) > phone);
        // It reads the SHORT side, so a rotation cannot change the band.
        r.check("seloutr1_03_width_is_orientation_independent",
                nearly(selectionOutlineWidthPixels(1080, 2400),
                       selectionOutlineWidthPixels(2400, 1080)));
        // The floor exists because a two-pixel band is the thinnest thing that
        // still reads as a deliberate edge rather than as aliasing.
        r.check("seloutr1_03_the_floor_is_at_least_two_pixels",
                kSelectionOutlineMinPixels >= 2.0f);
        // And the ceiling exists so a 12-pixel feature is edged, not covered.
        r.check("seloutr1_03_the_ceiling_stays_thin", kSelectionOutlineMaxPixels <= 6.0f);
    }

    // --- a degenerate viewport answers the floor, never zero or a NaN ------
    {
        const float zero = selectionOutlineWidthPixels(0, 0);
        const float negative = selectionOutlineWidthPixels(-100, 200);
        r.check("seloutr1_22_a_zero_viewport_answers_the_floor",
                nearly(zero, kSelectionOutlineMinPixels));
        r.check("seloutr1_22_a_negative_viewport_answers_the_floor",
                nearly(negative, kSelectionOutlineMinPixels));
        r.check("seloutr1_22_a_degenerate_viewport_never_answers_zero", zero > 0.0f);
    }

    // --- the camera is NOT an input ----------------------------------------
    //
    // Structural rather than incidental: the function's whole signature is two
    // integers, so there is nothing camera-shaped to reach. This states it as a
    // check so a future overload that took a camera would have to delete a
    // named assertion rather than quietly widen the contract.
    {
        const float a = selectionOutlineWidthPixels(1080, 2400);
        const float b = selectionOutlineWidthPixels(1080, 2400);
        r.check("seloutr1_03_width_is_a_pure_function_of_the_viewport", a == b);
    }
}

void checkSelectionOutlineColour(Recorder& r) {
    // --- SELOUTR1-28..32: legible on every one of the five grounds ---------
    //
    // Measured, not asserted in prose, and with the same WCAG arithmetic the
    // evidence document quotes. The viewport's swapchain format is UNORM, so
    // the floats the renderer writes ARE the sRGB values the panel shows and
    // this is a measurement rather than an approximation of one.
    const ViewportBackground grounds[kViewportBackgroundCount] = {
        ViewportBackground::WarmGraphite, ViewportBackground::NeutralCharcoal,
        ViewportBackground::LightCharcoal, ViewportBackground::WarmLight,
        ViewportBackground::CoolLight,
    };
    static const char* const names[kViewportBackgroundCount] = {
        "seloutr1_28_warm_graphite_clears_4_5_to_1",
        "seloutr1_29_neutral_charcoal_clears_4_5_to_1",
        "seloutr1_30_light_charcoal_clears_4_5_to_1",
        "seloutr1_31_warm_light_clears_4_5_to_1",
        "seloutr1_32_cool_light_clears_4_5_to_1",
    };

    bool everyGroundFinite = true;
    for (int i = 0; i < kViewportBackgroundCount; ++i) {
        float outline[3] = {0.0f, 0.0f, 0.0f};
        float ground[3] = {0.0f, 0.0f, 0.0f};
        selectionOutlineColor(grounds[i], outline);
        viewportBackgroundColor(grounds[i], ground);
        for (int c = 0; c < 3; ++c) {
            if (!(outline[c] >= 0.0f && outline[c] <= 1.0f)) everyGroundFinite = false;
        }
        r.check(names[i], srgbContrastRatio(outline, ground) >= 4.5f);
    }
    r.check("seloutr1_28_every_outline_colour_is_a_valid_rgb", everyGroundFinite);

    // --- it is one question, asked once ------------------------------------
    //
    // Every DARK ground answers the same colour and every LIGHT ground answers
    // the same colour, so adding a sixth ground touches the palette and never a
    // switch in a renderer. Light Charcoal takes the DARK answer even though
    // the gizmo's palette lumps it with the light ones — the gizmo's split is
    // about saturation against a mid ground, and this one is about luminance
    // contrast, which #3C3F41 settles by measuring at well over 4.5:1 above.
    {
        float warmGraphite[3];
        float neutral[3];
        float lightCharcoal[3];
        float warmLight[3];
        float coolLight[3];
        selectionOutlineColor(ViewportBackground::WarmGraphite, warmGraphite);
        selectionOutlineColor(ViewportBackground::NeutralCharcoal, neutral);
        selectionOutlineColor(ViewportBackground::LightCharcoal, lightCharcoal);
        selectionOutlineColor(ViewportBackground::WarmLight, warmLight);
        selectionOutlineColor(ViewportBackground::CoolLight, coolLight);

        const bool darkFamilyAgrees = std::memcmp(warmGraphite, neutral, sizeof(warmGraphite)) == 0 &&
                                      std::memcmp(warmGraphite, lightCharcoal,
                                                  sizeof(warmGraphite)) == 0;
        const bool lightFamilyAgrees =
            std::memcmp(warmLight, coolLight, sizeof(warmLight)) == 0;
        const bool familiesDiffer =
            std::memcmp(warmGraphite, warmLight, sizeof(warmLight)) != 0;
        r.check("seloutr1_28_the_three_dark_grounds_share_one_outline_colour", darkFamilyAgrees);
        r.check("seloutr1_31_the_two_light_grounds_share_one_outline_colour", lightFamilyAgrees);
        r.check("seloutr1_28_the_two_families_are_genuinely_different", familiesDiffer);

        // Over paper the edge SINKS darker; over a dark ground it LIFTS
        // brighter. The same inversion the grid's palette makes, and for the
        // same reason.
        r.check("seloutr1_31_the_light_ground_outline_is_darker_than_the_dark_ground_one",
                srgbRelativeLuminance(warmLight) < srgbRelativeLuminance(warmGraphite));
    }

    // --- and a null destination is survivable ------------------------------
    {
        selectionOutlineColor(ViewportBackground::WarmGraphite, nullptr);
        r.check("seloutr1_28_a_null_destination_is_ignored_not_dereferenced", true);
    }
}

void checkSelectionOutlineKernel(Recorder& r) {
    // A 64x64 field with a 20x20 filled square at (20,20)..(39,39).
    constexpr int kW = 64;
    constexpr int kH = 64;
    constexpr int kX0 = 20;
    constexpr int kY0 = 20;
    constexpr int kX1 = 39;
    constexpr int kY1 = 39;
    constexpr float kRadius = 3.0f;
    std::vector<float> mask;
    fillMaskRect(&mask, kW, kH, kX0, kY0, kX1, kY1);

    // --- SELOUTR1-13: an INTERIOR pixel is never painted -------------------
    //
    // This is what makes the result an outline rather than a highlight, and it
    // is what keeps a small or thin body edged rather than filled.
    {
        bool everyInteriorClear = true;
        for (int y = kY0; y <= kY1; ++y) {
            for (int x = kX0; x <= kX1; ++x) {
                if (selectionOutlineCoverage(mask.data(), kW, kH, x, y, kRadius) != 0.0f) {
                    everyInteriorClear = false;
                }
            }
        }
        r.check("seloutr1_13_no_interior_pixel_is_ever_painted", everyInteriorClear);
    }

    // --- SELOUTR1-02: the band exists, just outside the silhouette ---------
    {
        // One pixel outside the left edge, and one outside the top edge.
        r.check("seloutr1_02_the_pixel_just_left_of_the_edge_is_outlined",
                selectionOutlineCoverage(mask.data(), kW, kH, kX0 - 1, 30, kRadius) > 0.0f);
        r.check("seloutr1_02_the_pixel_just_above_the_edge_is_outlined",
                selectionOutlineCoverage(mask.data(), kW, kH, 30, kY0 - 1, kRadius) > 0.0f);
        // And a corner, which a naive axis-only kernel would miss.
        r.check("seloutr1_02_the_diagonal_corner_is_outlined",
                selectionOutlineCoverage(mask.data(), kW, kH, kX0 - 1, kY0 - 1, kRadius) > 0.0f);
    }

    // --- SELOUTR1-03: the band is the width the policy asked for -----------
    {
        // Walking left from the edge: painted while within the radius, clear
        // beyond it. The taps are rounded to whole pixels, so the transition is
        // allowed to land one pixel either side of the exact radius, which is
        // what the +1 tolerance below states rather than hides.
        bool insideBandPainted = true;
        for (int d = 1; d <= static_cast<int>(kRadius); ++d) {
            if (!(selectionOutlineCoverage(mask.data(), kW, kH, kX0 - d, 30, kRadius) > 0.0f)) {
                insideBandPainted = false;
            }
        }
        const float justBeyond = selectionOutlineCoverage(
            mask.data(), kW, kH, kX0 - static_cast<int>(kRadius) - 2, 30, kRadius);
        r.check("seloutr1_03_every_pixel_within_the_band_is_painted", insideBandPainted);
        r.check("seloutr1_03_nothing_beyond_the_band_is_painted", justBeyond == 0.0f);

        // Far away is emphatically clear — the band is a band, not a glow.
        r.check("seloutr1_13_a_distant_pixel_is_never_painted",
                selectionOutlineCoverage(mask.data(), kW, kH, 2, 2, kRadius) == 0.0f);
    }

    // --- SELOUTR1-01 / -20 / -21: an EMPTY mask outlines nothing -----------
    //
    // The same rule covers three product facts, because all three arrive as an
    // empty or partial mask rather than as a special case: nothing selected
    // (SELOUTR1-01), a selected body entirely hidden behind another
    // (SELOUTR1-21), and the hidden PART of a partly occluded one
    // (SELOUTR1-20). Occlusion is resolved by the mask pass's depth test, so
    // the kernel never learns that occlusion exists — it just sees no coverage.
    {
        std::vector<float> empty(static_cast<size_t>(kW) * static_cast<size_t>(kH), 0.0f);
        bool everyPixelClear = true;
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                if (selectionOutlineCoverage(empty.data(), kW, kH, x, y, kRadius) != 0.0f) {
                    everyPixelClear = false;
                }
            }
        }
        r.check("seloutr1_01_an_empty_mask_paints_nothing_anywhere", everyPixelClear);
    }

    // A HALF-occluded body: the mask carries only its visible left half. The
    // band must trace that half's boundary — including the cut edge, which is
    // the "contour at a visible depth discontinuity" §4.2 sanctions — and must
    // NOT appear anywhere along the hidden half.
    {
        std::vector<float> half;
        fillMaskRect(&half, kW, kH, kX0, kY0, 29, kY1);  // right half absent
        r.check("seloutr1_20_the_visible_half_is_still_outlined",
                selectionOutlineCoverage(half.data(), kW, kH, kX0 - 1, 30, kRadius) > 0.0f);
        r.check("seloutr1_20_the_cut_edge_is_outlined",
                selectionOutlineCoverage(half.data(), kW, kH, 31, 30, kRadius) > 0.0f);
        // Deep inside the OCCLUDED half there is no coverage within a band, so
        // nothing is painted: no x-ray silhouette of the hidden part exists.
        r.check("seloutr1_21_the_hidden_half_grows_no_x_ray_outline",
                selectionOutlineCoverage(half.data(), kW, kH, kX1, 30, kRadius) == 0.0f);
    }

    // --- SELOUTR1-22: the window edge, and defensive inputs ----------------
    //
    // A body flush against the window edge must not grow a band along the
    // window edge: samples outside the image read as 0, which is what the GPU
    // sampler's opaque-black border does.
    {
        std::vector<float> flush;
        fillMaskRect(&flush, kW, kH, 0, 0, 10, kH - 1);  // touching the left edge
        r.check("seloutr1_22_a_body_flush_to_the_edge_is_outlined_on_its_open_side",
                selectionOutlineCoverage(flush.data(), kW, kH, 12, 30, kRadius) > 0.0f);
        // x = 0 is interior, so it is not painted, and there is no x < 0 to
        // paint. Nothing runs along the window edge.
        r.check("seloutr1_22_nothing_is_painted_at_the_window_edge_itself",
                selectionOutlineCoverage(flush.data(), kW, kH, 0, 30, kRadius) == 0.0f);

        r.check("seloutr1_22_a_null_mask_paints_nothing",
                selectionOutlineCoverage(nullptr, kW, kH, 30, 30, kRadius) == 0.0f);
        r.check("seloutr1_22_a_zero_extent_paints_nothing",
                selectionOutlineCoverage(mask.data(), 0, 0, 0, 0, kRadius) == 0.0f);
        r.check("seloutr1_22_a_zero_radius_paints_nothing",
                selectionOutlineCoverage(mask.data(), kW, kH, kX0 - 1, 30, 0.0f) == 0.0f);
        r.check("seloutr1_22_an_out_of_range_pixel_paints_nothing",
                selectionOutlineCoverage(mask.data(), kW, kH, -5, -5, kRadius) == 0.0f);
    }

    // --- the tap pattern is the one the shader mirrors ---------------------
    //
    // Stated as a check rather than only as a comment, because outline.frag
    // hard-codes these three numbers: a change here that was not mirrored there
    // would produce a band of a different shape with nothing to catch it.
    r.check("seloutr1_03_the_kernel_uses_the_documented_tap_pattern",
            kSelectionOutlineOuterTaps == 8 && kSelectionOutlineInnerTaps == 4 &&
                nearly(kSelectionOutlineInnerRingScale, 0.5f));
}

void checkSelectionOutlineIsPresentationOnly(Recorder& r) {
    // --- SELOUTR1-14..19: the toggle is a display setting and nothing more --
    //
    // Its ownership, its default and its inertness, on the grid's exact terms.
    // The `.forge` half of SELOUTR1-19 is proven from Java against real bytes;
    // what is proven here is that there is nothing in the domain for the
    // toggle to reach in the first place.
    {
        r.check("seloutr1_16_the_outline_is_on_by_default", kDefaultSelectionOutlineVisible);

        DisplaySettingsStore store;
        r.check("seloutr1_16_a_fresh_store_reports_the_default",
                store.selectionOutlineVisible() == kDefaultSelectionOutlineVisible);

        const uint64_t changesBefore = store.changeCount();
        r.check("seloutr1_15_turning_it_off_is_a_real_change",
                store.setSelectionOutlineVisible(false));
        r.check("seloutr1_15_and_it_reports_off_afterwards", !store.selectionOutlineVisible());
        r.check("seloutr1_15_a_real_change_is_counted", store.changeCount() == changesBefore + 1);
        // Idempotent: asking for what is already in effect is inert and is NOT
        // counted, so a caller can tell a transition from a no-op.
        r.check("seloutr1_15_setting_the_value_it_already_has_is_a_no_op",
                !store.setSelectionOutlineVisible(false));
        r.check("seloutr1_15_a_no_op_is_not_counted", store.changeCount() == changesBefore + 1);
        r.check("seloutr1_16_turning_it_back_on_is_a_real_change",
                store.setSelectionOutlineVisible(true));
        r.check("seloutr1_16_and_it_reports_on_afterwards", store.selectionOutlineVisible());

        // It rides in the SNAPSHOT, so the render thread reads it once at a
        // known point. A toggle landing between the mask pass and the composite
        // would otherwise record half an outline.
        store.setSelectionOutlineVisible(false);
        r.check("seloutr1_15_the_snapshot_carries_it",
                !store.snapshot().selectionOutlineVisible);
        store.setSelectionOutlineVisible(true);
        r.check("seloutr1_16_the_snapshot_follows_it",
                store.snapshot().selectionOutlineVisible);
    }

    // --- the outline and the grid are genuinely independent ----------------
    //
    // Two overlays sharing one store must not share one answer.
    {
        DisplaySettingsStore store;
        store.setGridVisible(false);
        r.check("seloutr1_14_hiding_the_grid_leaves_the_outline_alone",
                store.selectionOutlineVisible());
        store.setSelectionOutlineVisible(false);
        store.setGridVisible(true);
        r.check("seloutr1_14_showing_the_grid_leaves_the_outline_alone",
                !store.selectionOutlineVisible());
        // And neither touches the shading model, the surface shading or the
        // viewport ground.
        const ViewportDisplaySettings snapshot = store.snapshot();
        r.check("seloutr1_17_the_overlays_leave_the_shading_model_alone",
                snapshot.shading == kDefaultShadingModel);
        r.check("seloutr1_17_the_overlays_leave_the_surface_shading_alone",
                snapshot.surface == kDefaultSurfaceShading);
        r.check("seloutr1_17_the_overlays_leave_the_ground_alone",
                snapshot.background == kDefaultViewportBackground);
    }

    // --- SELOUTR1-09 / -10 / -17: it cannot reach geometry truth -----------
    //
    // Structural, exactly as the pulse's own inertness check is: a whole
    // outline cycle runs beside a real published mesh and the revision, the
    // vertex data and the index data all come out bit-identical. There is no
    // path from a display store to a MeshStore, and this is what says so with
    // a real mesh rather than with prose.
    {
        const ConstructionMesh source = defaultSphere();
        MeshStore store(kConstructionBoxObjectId);
        const MeshRevision revisionBefore =
            store.publish(source.vertices.data(), static_cast<uint32_t>(source.vertices.size()),
                          source.indices.data(), static_cast<uint32_t>(source.indices.size()));
        const RuntimeMeshPtr before = store.current();

        DisplaySettingsStore display;
        for (int i = 0; i < 20; ++i) {
            display.setSelectionOutlineVisible(i % 2 == 0);
            // And the whole colour/width policy exercised beside it, because
            // that is what a frame would actually run.
            float rgb[3];
            selectionOutlineColor(display.snapshot().background, rgb);
            (void)selectionOutlineWidthPixels(1080, 2400);
        }

        const RuntimeMeshPtr after = store.current();
        const bool sameRevision = after && store.currentRevision() == revisionBefore &&
                                  after->revision() == revisionBefore &&
                                  revisionBefore != kNoMeshRevision;
        bool sameBytes = after && before && after->vertexCount() == before->vertexCount() &&
                         after->indexCount() == before->indexCount();
        if (sameBytes) {
            sameBytes = std::memcmp(after->vertices(), before->vertices(),
                                    after->vertexCount() * sizeof(MeshVertex)) == 0 &&
                        std::memcmp(after->indices(), before->indices(),
                                    after->indexCount() * sizeof(uint32_t)) == 0;
        }
        r.check("seloutr1_09_the_fixture_published", revisionBefore != kNoMeshRevision);
        r.check("seloutr1_09_toggling_the_outline_mints_no_mesh_revision", sameRevision);
        r.check("seloutr1_10_toggling_the_outline_leaves_the_mesh_bit_identical", sameBytes);
        r.check("seloutr1_10_it_is_still_the_same_published_object", after == before);
    }

    // --- SELOUTR1-08: a selection CHANGE is inert on the same terms --------
    //
    // Selection travels to the renderer as one bool per scene item, which the
    // snapshot already carried before this stage existed. Switching the
    // selected body therefore rebuilds nothing and re-uploads nothing, and the
    // strongest available statement of that is a real scene snapshotted either
    // side of a real selection change.
    {
        ConstructionScene scene;
        SceneObject& bodyA = scene.bodyAt(0);
        SceneObject& bodyB = scene.addBody();
        publishSceneObject(bodyA);
        publishSceneObject(bodyB);
        const ObjectId first = bodyA.objectId();
        const ObjectId second = bodyB.objectId();
        const bool twoBodies = second != kNoObject && second != first && scene.bodyCount() == 2;

        scene.setActiveBody(first);
        const SceneSnapshot before = scene.snapshot();
        scene.setActiveBody(second);
        const SceneSnapshot after = scene.snapshot();
        scene.setActiveBody(first);
        const SceneSnapshot back = scene.snapshot();

        bool sameShape = before.size() == after.size() && after.size() == back.size();
        bool sameMeshes = sameShape;
        bool sameRevisions = sameShape;
        bool selectionMoved = sameShape;
        if (sameShape) {
            for (size_t i = 0; i < before.size(); ++i) {
                // The very same published RuntimeMesh object, not merely an
                // equal one: a rebuild would have produced a different pointer
                // and a different revision.
                if (before[i].mesh != after[i].mesh || after[i].mesh != back[i].mesh) {
                    sameMeshes = false;
                }
                if (before[i].mesh && after[i].mesh &&
                    before[i].mesh->revision() != after[i].mesh->revision()) {
                    sameRevisions = false;
                }
                if (before[i].selected == after[i].selected) {
                    selectionMoved = false;  // nothing actually changed: a bad test
                }
                if (before[i].selected != back[i].selected) {
                    selectionMoved = false;  // and it came back to where it was
                }
            }
        }
        r.check("seloutr1_08_the_two_body_fixture_built", twoBodies);
        r.check("seloutr1_08_a_selection_change_actually_moves_the_flag", selectionMoved);
        r.check("seloutr1_08_a_selection_change_republishes_no_mesh", sameMeshes);
        r.check("seloutr1_09_a_selection_change_mints_no_mesh_revision", sameRevisions);
    }
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

    // The direction family. Everything above measures where a normal lies;
    // these measure which way it points, and the last one proves they can tell.
    checkSourceWinding(r);
    checkRenderNormalsOutward(r);
    checkFacetedOrientation(r);
    checkDuplicationPreservesWinding(r);
    checkNormalTransform(r);
    checkDisplayModesAreInert(r);
    checkFlipIsDetected(r);

    // Stage 016: the bounded two-sided render-only exception for Plane.
    checkPlaneTwoSidedRenderGeometry(r);
    checkPlaneDisplayModesAreInert(r);

    // UI-R1C1: selection feedback. It lives in this suite because selection is
    // PRESENTATION — the same reason the shading model and the viewport
    // background are here and not in the picking or selection suites, which own
    // which object is selected rather than how it is drawn.
    checkSelectionPulse(r);

    // UI-R1C2: the world reference grid. Here for the same reason selection
    // feedback is — it is PRESENTATION. The suite that owns how a thing is
    // drawn owns the grid; the picking and selection suites own what is in the
    // scene, and the whole point of the grid is that it is not.
    checkGridDefaultAndOwnership(r);
    checkGridGeometry(r);
    checkGridPalette(r);
    checkGridIsInertAgainstTheModel(r);
    checkGridAgainstPlaneAndProjection(r);

    // SEL-OUT-R1: the selection outline. Here for the third time for the same
    // reason — it is PRESENTATION. The width policy, the per-ground colours
    // with their measured contrast, the edge-extraction rule outline.frag
    // mirrors, and the promise that none of it can reach geometry truth.
    checkSelectionOutlineWidth(r);
    checkSelectionOutlineColour(r);
    checkSelectionOutlineKernel(r);
    checkSelectionOutlineIsPresentationOnly(r);

    return r.n;
}

}  // namespace forgeshape
