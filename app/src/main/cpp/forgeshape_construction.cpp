#include "forgeshape_construction.h"

#include <cmath>

namespace forgeshape {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Corner colours. This is DERIVED PRESENTATION data, not Construction truth: it
// exists only because RuntimeMesh carries a colour channel and a per-corner
// gradient is what makes the box's orientation readable by eye. No dimension is
// ever inferred from it.
const float kCornerColors[kBoxVertexCount][3] = {
    {0.10f, 0.15f, 0.85f},  // -x -y -z
    {0.95f, 0.25f, 0.15f},  // +x -y -z
    {0.98f, 0.85f, 0.15f},  // +x +y -z
    {0.15f, 0.85f, 0.35f},  // -x +y -z
    {0.10f, 0.75f, 0.90f},  // -x -y +z
    {0.90f, 0.35f, 0.75f},  // +x -y +z
    {0.98f, 0.98f, 0.98f},  // +x +y +z
    {0.45f, 0.30f, 0.85f},  // -x +y +z
};

// Canonical box topology: every triangle is counter-clockwise seen from OUTSIDE
// the solid, which is the convention documented in forgeshape_picking.h and
// enforced by the pipeline's VK_CULL_MODE_BACK_BIT /
// VK_FRONT_FACE_COUNTER_CLOCKWISE.
// The corner ordering above is what makes this table correct, so the two must
// be read together.
const uint32_t kBoxIndices[kBoxIndexCount] = {
    4, 5, 6, 6, 7, 4,  // +Z
    1, 0, 3, 3, 2, 1,  // -Z
    0, 4, 7, 7, 3, 0,  // -X
    5, 1, 2, 2, 6, 5,  // +X
    3, 7, 6, 6, 2, 3,  // +Y
    0, 1, 5, 5, 4, 0,  // -Y
};

// Sign of each corner along x, y, z, in the order the colour table assumes.
const float kCornerSigns[kBoxVertexCount][3] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
};

// Plane corner colours. DERIVED PRESENTATION data, exactly like the box's: a
// per-corner gradient makes orientation and the -X/+Z corners readable by eye.
// No dimension is ever inferred from it.
const float kPlaneCornerColors[kPlaneVertexCount][3] = {
    {0.20f, 0.30f, 0.85f},  // -x -z
    {0.90f, 0.35f, 0.20f},  // +x -z
    {0.95f, 0.90f, 0.25f},  // +x +z
    {0.25f, 0.85f, 0.45f},  // -x +z
};

// Sign of each corner along x, z, in the order the colour table above assumes.
const float kPlaneCornerSigns[kPlaneVertexCount][2] = {
    {-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f},
};

// Canonical plane topology: one rectangle split on the (-X-Z, +X+Z) diagonal,
// both triangles counter-clockwise seen from the canonical front (+Y) — the
// same winding rule the box's +Y face uses, and for the same reason: the
// geometric normal (v1-v0) x (v2-v0) must point away from the surface's front.
const uint32_t kPlaneIndices[kPlaneIndexCount] = {
    0, 3, 2,
    2, 1, 0,
};

// Exact unit direction for ring vertex `segment` of a ring divided into
// `segmentCount` equal parts, where segmentCount is divisible by four.
//
// The four cardinal directions are written down rather than computed: cos(pi/2)
// in floating point is about 6.1e-17, not 0, and that tiny error would make the
// generated bounds not quite the radius. Because the segment count is divisible
// by four, those four directions always land on real ring vertices, so the
// bounds come out exactly right.
void exactRingDirection(uint32_t segment, uint32_t segmentCount, double* outCos, double* outSin) {
    const uint32_t quarter = segmentCount / 4;
    if (segment % quarter == 0) {
        switch (segment / quarter) {
            case 0: *outCos = 1.0;  *outSin = 0.0;  return;   //   0 degrees
            case 1: *outCos = 0.0;  *outSin = 1.0;  return;   //  90 degrees
            case 2: *outCos = -1.0; *outSin = 0.0;  return;   // 180 degrees
            default: *outCos = 0.0; *outSin = -1.0; return;   // 270 degrees
        }
    }
    const double angle =
        (2.0 * kPi * static_cast<double>(segment)) / static_cast<double>(segmentCount);
    *outCos = std::cos(angle);
    *outSin = std::sin(angle);
}

void ringDirection(uint32_t segment, double* outCos, double* outSin) {
    exactRingDirection(segment, kCylinderRadialSegments, outCos, outSin);
}

// Exact polar angle for a capsule hemisphere ring, counted from that
// hemisphere's pole: theta = (pi/2) * band / kCapsuleHemisphereBands, for band
// in [1, kCapsuleHemisphereBands].
//
// The seam band (band == kCapsuleHemisphereBands) is theta = pi/2 and is written
// down for the same reason the sphere's equator is: cos(pi/2) computed in
// floating point is about 6.1e-17 rather than 0, which would leave the seam ring
// a hair off the end of the cylindrical middle and its radius a hair under the
// capsule's, so the X and Z bounds would not be exactly +/- diameter/2.
void exactHemisphereLatitude(uint32_t band, double* outCosTheta, double* outSinTheta) {
    if (band == kCapsuleHemisphereBands) {
        *outCosTheta = 0.0;  // the seam plane
        *outSinTheta = 1.0;  // at the full radius
        return;
    }
    const double theta = (kPi * static_cast<double>(band)) /
                         (2.0 * static_cast<double>(kCapsuleHemisphereBands));
    *outCosTheta = std::cos(theta);
    *outSinTheta = std::sin(theta);
}

// Exact polar angle for sphere ring `ring`, counted from the north pole, where
// theta = pi * ring / kSphereStacks and `ring` is strictly between 0 and
// kSphereStacks.
//
// The equator is written down for the same reason the cardinal directions are:
// kSphereStacks is even, so exactly one ring lands on it, and cos(pi/2) computed
// in floating point would leave that ring a hair off y = 0 and its radius a hair
// under the sphere's. Writing it down is what makes the X and Z bounds exactly
// +/- the radius.
void exactLatitude(uint32_t ring, double* outCosTheta, double* outSinTheta) {
    if (ring == kSphereStacks / 2) {
        *outCosTheta = 0.0;  // the equator plane, y = 0
        *outSinTheta = 1.0;  // at the full radius
        return;
    }
    const double theta = (kPi * static_cast<double>(ring)) / static_cast<double>(kSphereStacks);
    *outCosTheta = std::cos(theta);
    *outSinTheta = std::sin(theta);
}

}  // namespace

const char* primitiveKindName(PrimitiveKind kind) {
    switch (kind) {
        case PrimitiveKind::Box: return "box";
        case PrimitiveKind::Cylinder: return "cylinder";
        case PrimitiveKind::Sphere: return "sphere";
        case PrimitiveKind::Cone: return "cone";
        case PrimitiveKind::Capsule: return "capsule";
        case PrimitiveKind::Plane: return "plane";
    }
    return "unknown";
}

const char* dimensionValidationName(DimensionValidation why) {
    switch (why) {
        case DimensionValidation::Ok: return "ok";
        case DimensionValidation::NotFinite: return "not_finite";
        case DimensionValidation::NotPositive: return "not_positive";
        case DimensionValidation::NotRepresentable: return "not_representable";
        case DimensionValidation::RelationInvalid: return "relation_invalid";
    }
    return "unknown";
}

const char* primitiveUpdateStatusName(PrimitiveUpdateStatus status) {
    switch (status) {
        case PrimitiveUpdateStatus::Applied: return "applied";
        case PrimitiveUpdateStatus::Unchanged: return "unchanged";
        case PrimitiveUpdateStatus::Rejected: return "rejected";
    }
    return "unknown";
}

DimensionValidation validateDimensionMeters(Meters value) {
    if (!std::isfinite(value)) {
        return DimensionValidation::NotFinite;
    }
    if (value <= 0.0) {
        return DimensionValidation::NotPositive;
    }
    // The mesh is float. A dimension that survives as a double but collapses to
    // zero or blows up to infinity once halved into a float half-extent would
    // produce a degenerate or invalid mesh, so it is refused here rather than
    // downstream. This is a representability limit, not a product size policy.
    const float halfExtent = static_cast<float>(value * 0.5);
    if (!std::isfinite(halfExtent) || halfExtent <= 0.0f) {
        return DimensionValidation::NotRepresentable;
    }
    if (!std::isfinite(halfExtent * 2.0f)) {
        return DimensionValidation::NotRepresentable;
    }
    return DimensionValidation::Ok;
}

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------

PrimitiveUpdateStatus ConstructionBox::setDimensionsMeters(Meters width, Meters height,
                                                           Meters depth,
                                                           DimensionValidation* outWhy) {
    // Validate all three BEFORE touching anything, so a bad depth cannot leave a
    // half-applied width behind.
    DimensionValidation why = validateDimensionMeters(width);
    if (why == DimensionValidation::Ok) {
        why = validateDimensionMeters(height);
    }
    if (why == DimensionValidation::Ok) {
        why = validateDimensionMeters(depth);
    }
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;  // previous valid parameters stand
    }

    if (width == dimensions_.width && height == dimensions_.height &&
        depth == dimensions_.depth) {
        return PrimitiveUpdateStatus::Unchanged;  // no revision is worth publishing
    }

    dimensions_.width = width;
    dimensions_.height = height;
    dimensions_.depth = depth;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

ConstructionMesh ConstructionBox::generateMesh() const {
    // Centred on the local origin: the extents are exactly +/- half of each
    // authoritative dimension, with no offset term anywhere.
    const float halfX = static_cast<float>(dimensions_.width * 0.5);
    const float halfY = static_cast<float>(dimensions_.height * 0.5);
    const float halfZ = static_cast<float>(dimensions_.depth * 0.5);

    ConstructionMesh mesh;
    mesh.vertices.resize(kBoxVertexCount);
    for (uint32_t i = 0; i < kBoxVertexCount; ++i) {
        MeshVertex& v = mesh.vertices[i];
        v.position[0] = kCornerSigns[i][0] * halfX;
        v.position[1] = kCornerSigns[i][1] * halfY;
        v.position[2] = kCornerSigns[i][2] * halfZ;
        v.color[0] = kCornerColors[i][0];
        v.color[1] = kCornerColors[i][1];
        v.color[2] = kCornerColors[i][2];
    }
    mesh.indices.assign(kBoxIndices, kBoxIndices + kBoxIndexCount);
    return mesh;
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------

PrimitiveUpdateStatus ConstructionCylinder::setDimensionsMeters(Meters diameter, Meters height,
                                                                DimensionValidation* outWhy) {
    DimensionValidation why = validateDimensionMeters(diameter);
    if (why == DimensionValidation::Ok) {
        why = validateDimensionMeters(height);
    }
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;
    }

    if (diameter == dimensions_.diameter && height == dimensions_.height) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    dimensions_.diameter = diameter;
    dimensions_.height = height;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

// Vertex layout, fixed and shared with the index table below:
//
//   [0, N)          bottom ring, y = -height/2
//   [N, 2N)         top ring,    y = +height/2
//   2N              bottom cap centre
//   2N + 1          top cap centre
//
// The two rings are SHARED between the side wall and the caps. That is what
// makes the count 2N + 2 rather than 4N + 2; it costs a slightly softer cap edge
// under smooth shading, which this renderer does not do — it has no normals at
// all, only per-vertex colour.
ConstructionMesh ConstructionCylinder::generateMesh() const {
    const uint32_t n = kCylinderRadialSegments;
    const float radius = static_cast<float>(dimensions_.diameter * 0.5);
    const float halfY = static_cast<float>(dimensions_.height * 0.5);

    ConstructionMesh mesh;
    mesh.vertices.resize(kCylinderVertexCount);

    for (uint32_t i = 0; i < n; ++i) {
        double dirCos = 0.0;
        double dirSin = 0.0;
        ringDirection(i, &dirCos, &dirSin);
        const float x = static_cast<float>(dirCos) * radius;
        const float z = static_cast<float>(dirSin) * radius;

        // Colour is derived PRESENTATION data: it sweeps with the angle and
        // brightens toward the top so the cylinder's orientation and its seam
        // are readable by eye. No dimension is ever inferred from it.
        const float warm = static_cast<float>(0.5 + 0.5 * dirCos);
        const float cool = static_cast<float>(0.5 + 0.5 * dirSin);

        MeshVertex& bottom = mesh.vertices[i];
        bottom.position[0] = x;
        bottom.position[1] = -halfY;
        bottom.position[2] = z;
        bottom.color[0] = 0.20f + 0.60f * warm;
        bottom.color[1] = 0.15f + 0.35f * cool;
        bottom.color[2] = 0.35f;

        MeshVertex& top = mesh.vertices[n + i];
        top.position[0] = x;
        top.position[1] = halfY;
        top.position[2] = z;
        top.color[0] = 0.35f + 0.60f * warm;
        top.color[1] = 0.45f + 0.50f * cool;
        top.color[2] = 0.92f;
    }

    const uint32_t bottomCentre = 2 * n;
    const uint32_t topCentre = 2 * n + 1;

    MeshVertex& bc = mesh.vertices[bottomCentre];
    bc.position[0] = 0.0f;
    bc.position[1] = -halfY;
    bc.position[2] = 0.0f;
    bc.color[0] = 0.30f; bc.color[1] = 0.22f; bc.color[2] = 0.40f;

    MeshVertex& tc = mesh.vertices[topCentre];
    tc.position[0] = 0.0f;
    tc.position[1] = halfY;
    tc.position[2] = 0.0f;
    tc.color[0] = 0.95f; tc.color[1] = 0.95f; tc.color[2] = 0.98f;

    // Canonical winding: counter-clockwise seen from OUTSIDE, so the geometric
    // normal (v1-v0) x (v2-v0) points away from the solid. Side normals point
    // radially outward, the top cap's point at +Y and the bottom cap's at -Y.
    mesh.indices.clear();
    mesh.indices.reserve(kCylinderIndexCount);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;
        const uint32_t b0 = i;
        const uint32_t b1 = j;
        const uint32_t t0 = n + i;
        const uint32_t t1 = n + j;

        // Side wall: two triangles per segment.
        mesh.indices.push_back(b0);
        mesh.indices.push_back(t0);
        mesh.indices.push_back(t1);

        mesh.indices.push_back(b0);
        mesh.indices.push_back(t1);
        mesh.indices.push_back(b1);

        // Top cap fan: reversed relative to the ring order, because the outward
        // normal there is +Y.
        mesh.indices.push_back(topCentre);
        mesh.indices.push_back(t1);
        mesh.indices.push_back(t0);

        // Bottom cap fan: ring order, because the outward normal there is -Y.
        mesh.indices.push_back(bottomCentre);
        mesh.indices.push_back(b0);
        mesh.indices.push_back(b1);
    }
    return mesh;
}

// ---------------------------------------------------------------------------
// Sphere
// ---------------------------------------------------------------------------

PrimitiveUpdateStatus ConstructionSphere::setDimensionsMeters(Meters diameter,
                                                              DimensionValidation* outWhy) {
    const DimensionValidation why = validateDimensionMeters(diameter);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;
    }

    if (diameter == dimensions_.diameter) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    dimensions_.diameter = diameter;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

// Vertex layout, fixed and shared with the index table below:
//
//   ring r in [0, kSphereRingCount), meridian j in [0, N):
//       index r * N + j          latitude theta = pi * (r + 1) / kSphereStacks
//   kSphereRingCount * N         north pole, y = +radius
//   kSphereRingCount * N + 1     south pole, y = -radius
//
// Ring 0 is the ring nearest the north pole and ring kSphereRingCount - 1 the
// one nearest the south pole, so ring index increases as y decreases — the same
// direction the polar angle runs.
//
// Each pole is ONE vertex fanned to its adjacent ring rather than a collapsed
// row of a quad grid. That is deliberate: the collapsed-quad form produces N
// zero-area triangles at each pole, which are invisible but real, and would show
// up in every winding, area and validation check as noise.
ConstructionMesh ConstructionSphere::generateMesh() const {
    const uint32_t n = kSphereRadialSegments;
    const uint32_t rings = kSphereRingCount;
    const float radius = static_cast<float>(dimensions_.diameter * 0.5);

    ConstructionMesh mesh;
    mesh.vertices.resize(kSphereVertexCount);

    for (uint32_t r = 0; r < rings; ++r) {
        double cosTheta = 0.0;
        double sinTheta = 0.0;
        exactLatitude(r + 1, &cosTheta, &sinTheta);
        const float y = static_cast<float>(cosTheta) * radius;

        for (uint32_t j = 0; j < n; ++j) {
            double dirCos = 0.0;
            double dirSin = 0.0;
            exactRingDirection(j, n, &dirCos, &dirSin);

            // The two exact factors are multiplied in double and converted once,
            // so an equator vertex on a cardinal meridian lands on exactly
            // +/- radius instead of one rounding short of it.
            const float x = static_cast<float>(dirCos * sinTheta) * radius;
            const float z = static_cast<float>(dirSin * sinTheta) * radius;

            // Colour is derived PRESENTATION data: it sweeps with longitude and
            // brightens toward the north pole, so the sphere's orientation and
            // its seam are readable by eye on a renderer that has no normals. No
            // dimension is ever inferred from it.
            const float warm = static_cast<float>(0.5 + 0.5 * dirCos);
            const float cool = static_cast<float>(0.5 + 0.5 * dirSin);
            const float up = static_cast<float>(0.5 + 0.5 * cosTheta);

            MeshVertex& v = mesh.vertices[r * n + j];
            v.position[0] = x;
            v.position[1] = y;
            v.position[2] = z;
            v.color[0] = 0.20f + 0.70f * warm * up;
            v.color[1] = 0.18f + 0.55f * cool;
            v.color[2] = 0.35f + 0.60f * up;
        }
    }

    const uint32_t northPole = rings * n;
    const uint32_t southPole = rings * n + 1;

    MeshVertex& north = mesh.vertices[northPole];
    north.position[0] = 0.0f;
    north.position[1] = radius;
    north.position[2] = 0.0f;
    north.color[0] = 0.96f; north.color[1] = 0.96f; north.color[2] = 0.99f;

    MeshVertex& south = mesh.vertices[southPole];
    south.position[0] = 0.0f;
    south.position[1] = -radius;
    south.position[2] = 0.0f;
    south.color[0] = 0.22f; south.color[1] = 0.16f; south.color[2] = 0.34f;

    // Canonical winding: counter-clockwise seen from OUTSIDE, so the geometric
    // normal (v1-v0) x (v2-v0) points away from the centre everywhere. The band
    // ordering is the cylinder's side-wall ordering with "lower ring" and "upper
    // ring" in the same roles, and the two pole fans are its two cap fans.
    mesh.indices.clear();
    mesh.indices.reserve(kSphereIndexCount);

    // North pole fan: the pole and the ring just below it.
    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t j1 = (j + 1) % n;
        mesh.indices.push_back(northPole);
        mesh.indices.push_back(j1);
        mesh.indices.push_back(j);
    }

    // Full bands between consecutive rings. `upper` is nearer the north pole.
    for (uint32_t r = 0; r + 1 < rings; ++r) {
        const uint32_t upper = r * n;
        const uint32_t lower = (r + 1) * n;
        for (uint32_t j = 0; j < n; ++j) {
            const uint32_t j1 = (j + 1) % n;
            const uint32_t u0 = upper + j;
            const uint32_t u1 = upper + j1;
            const uint32_t l0 = lower + j;
            const uint32_t l1 = lower + j1;

            mesh.indices.push_back(l0);
            mesh.indices.push_back(u0);
            mesh.indices.push_back(u1);

            mesh.indices.push_back(l0);
            mesh.indices.push_back(u1);
            mesh.indices.push_back(l1);
        }
    }

    // South pole fan: the pole and the last ring.
    const uint32_t lastRing = (rings - 1) * n;
    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t j1 = (j + 1) % n;
        mesh.indices.push_back(southPole);
        mesh.indices.push_back(lastRing + j);
        mesh.indices.push_back(lastRing + j1);
    }
    return mesh;
}

// ---------------------------------------------------------------------------
// Cone
// ---------------------------------------------------------------------------

PrimitiveUpdateStatus ConstructionCone::setDimensionsMeters(Meters bottomDiameter, Meters height,
                                                            DimensionValidation* outWhy) {
    DimensionValidation why = validateDimensionMeters(bottomDiameter);
    if (why == DimensionValidation::Ok) {
        why = validateDimensionMeters(height);
    }
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;
    }

    if (bottomDiameter == dimensions_.bottomDiameter && height == dimensions_.height) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    dimensions_.bottomDiameter = bottomDiameter;
    dimensions_.height = height;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

// Vertex layout, fixed and shared with the index table below:
//
//   [0, N)          base ring, y = -height/2
//   N               base centre
//   N + 1           apex,      y = +height/2
//
// The apex is ONE vertex fanned to the base ring, exactly as a sphere's pole is,
// and for the same reason: degenerating the cylinder's top ring to a point would
// give N zero-area triangles at the tip — invisible, but real, and noise in every
// winding, area and validation check. There is no top ring and no top cap,
// because a cone has neither: the apex radius is zero by definition, not by
// having been set to zero.
ConstructionMesh ConstructionCone::generateMesh() const {
    const uint32_t n = kConeRadialSegments;
    const float radius = static_cast<float>(dimensions_.bottomDiameter * 0.5);
    const float halfY = static_cast<float>(dimensions_.height * 0.5);

    ConstructionMesh mesh;
    mesh.vertices.resize(kConeVertexCount);

    for (uint32_t i = 0; i < n; ++i) {
        double dirCos = 0.0;
        double dirSin = 0.0;
        exactRingDirection(i, n, &dirCos, &dirSin);

        // Colour is derived PRESENTATION data: it sweeps with the angle so the
        // cone's seam and its rotation are readable by eye on a renderer that
        // has no normals. No dimension is ever inferred from it.
        const float warm = static_cast<float>(0.5 + 0.5 * dirCos);
        const float cool = static_cast<float>(0.5 + 0.5 * dirSin);

        MeshVertex& v = mesh.vertices[i];
        v.position[0] = static_cast<float>(dirCos) * radius;
        v.position[1] = -halfY;
        v.position[2] = static_cast<float>(dirSin) * radius;
        v.color[0] = 0.24f + 0.62f * warm;
        v.color[1] = 0.16f + 0.34f * cool;
        v.color[2] = 0.38f;
    }

    const uint32_t baseCentre = n;
    const uint32_t apex = n + 1;

    MeshVertex& bc = mesh.vertices[baseCentre];
    bc.position[0] = 0.0f;
    bc.position[1] = -halfY;
    bc.position[2] = 0.0f;
    bc.color[0] = 0.28f; bc.color[1] = 0.20f; bc.color[2] = 0.42f;

    MeshVertex& tip = mesh.vertices[apex];
    tip.position[0] = 0.0f;
    tip.position[1] = halfY;
    tip.position[2] = 0.0f;
    tip.color[0] = 0.97f; tip.color[1] = 0.96f; tip.color[2] = 0.99f;

    // Canonical winding: counter-clockwise seen from OUTSIDE, so the geometric
    // normal (v1-v0) x (v2-v0) points away from the solid. Side normals tilt
    // outward and upward; the base's points at -Y.
    mesh.indices.clear();
    mesh.indices.reserve(kConeIndexCount);
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;

        // Side: the cylinder's (b0, t1, b1) triangle with the top ring collapsed
        // to the apex. Its (b0, t0, t1) partner is the one that would have been
        // degenerate, so it simply does not exist here.
        mesh.indices.push_back(i);
        mesh.indices.push_back(apex);
        mesh.indices.push_back(j);

        // Base fan: ring order, because the outward normal there is -Y. This is
        // the cylinder's bottom cap, unchanged.
        mesh.indices.push_back(baseCentre);
        mesh.indices.push_back(i);
        mesh.indices.push_back(j);
    }
    return mesh;
}

// ---------------------------------------------------------------------------
// Capsule
// ---------------------------------------------------------------------------

namespace {

// True when the capsule has a cylindrical middle at all, given the ALREADY
// VALIDATED parameters. The decision is made on the authoritative doubles, and
// validation has already established that a positive middle survives into float
// as a real separation — so a mesh with a middle band never has a zero-height
// one.
bool capsuleHasMiddle(const CapsuleDimensionsMeters& capsule) {
    return capsule.totalHeight > capsule.diameter;
}

uint32_t capsuleRingCount(const CapsuleDimensionsMeters& capsule) {
    // With a middle: kCapsuleHemisphereBands rings per hemisphere, the last of
    // each being that hemisphere's seam ring, so 2H rings. Without one, the two
    // seam rings are the SAME ring, so there are 2H - 1 — exactly a sphere's.
    return capsuleHasMiddle(capsule) ? kCapsuleRingCount : kCapsuleRingCount - 1;
}

// The smallest y-step anywhere on a hemisphere, as a fraction of the radius:
// the gap between the pole and the first ring below it,
// 1 - cos(pi / (2 * kCapsuleHemisphereBands)). Every other latitude step is
// larger, so a capsule whose float coordinates can resolve THIS step can resolve
// all of them.
double capsuleSmallestLatitudeStepFraction() {
    return 1.0 - std::cos(kPi / (2.0 * static_cast<double>(kCapsuleHemisphereBands)));
}

}  // namespace

// The capsule's own validation, because a capsule is the first primitive whose
// two parameters are related rather than independent.
//
// Beyond the ordinary per-length checks it enforces two things:
//
//   * the RELATION: totalHeight >= diameter, because the two hemispherical ends
//     alone are already `diameter` tall. Equality is valid and means no middle.
//   * float RESOLVABILITY: the generated positions must not merely be finite,
//     they must be far enough apart to describe the shape. A 1e30 m capsule
//     1e-6 m across is finite in every coordinate and yet its whole hemisphere
//     rounds to one float, which would be a mesh of zero-area triangles. That is
//     refused here rather than produced and then discovered downstream.
DimensionValidation validateCapsuleMeters(Meters diameter, Meters totalHeight) {
    DimensionValidation why = validateDimensionMeters(diameter);
    if (why != DimensionValidation::Ok) {
        return why;
    }
    why = validateDimensionMeters(totalHeight);
    if (why != DimensionValidation::Ok) {
        return why;
    }
    if (totalHeight < diameter) {
        return DimensionValidation::RelationInvalid;
    }

    const double radius = diameter * 0.5;
    const double halfMiddle = (totalHeight - diameter) * 0.5;

    // A middle that exists in double but vanishes in float would put the two
    // seam rings on top of each other and make the middle band zero-area.
    if (halfMiddle > 0.0 && !(static_cast<float>(halfMiddle) > 0.0f)) {
        return DimensionValidation::NotRepresentable;
    }

    // The pole is the furthest point from the origin, so it is where float
    // resolution is worst; the smallest step is the pole-to-first-ring gap.
    const float pole = static_cast<float>(totalHeight * 0.5);
    const float firstRing =
        static_cast<float>(halfMiddle + radius * (1.0 - capsuleSmallestLatitudeStepFraction()));
    if (!std::isfinite(pole) || !std::isfinite(firstRing) || !(firstRing < pole)) {
        return DimensionValidation::NotRepresentable;
    }
    return DimensionValidation::Ok;
}

PrimitiveUpdateStatus ConstructionCapsule::setDimensionsMeters(Meters diameter, Meters totalHeight,
                                                               DimensionValidation* outWhy) {
    const DimensionValidation why = validateCapsuleMeters(diameter, totalHeight);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;
    }

    if (diameter == dimensions_.diameter && totalHeight == dimensions_.totalHeight) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    dimensions_.diameter = diameter;
    dimensions_.totalHeight = totalHeight;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

// Vertex layout, fixed and shared with the index table below:
//
//   ring r in [0, R), meridian j in [0, N):
//       index r * N + j
//   R * N          north pole, y = +totalHeight/2
//   R * N + 1      south pole, y = -totalHeight/2
//
// Rings run from the north pole downward. The first kCapsuleHemisphereBands of
// them are the top hemisphere, the last of those being the TOP SEAM ring at
// y = +middle/2; the rest are the bottom hemisphere, the first of those being
// the BOTTOM SEAM ring at y = -middle/2.
//
// The cylindrical middle is therefore not a special case in the index loop at
// all: it is simply the band between ring H-1 and ring H, generated by the same
// code as every other band. That is what makes the degenerate capsule work — when
// there is no middle, those two seam rings are ONE ring, R is one smaller, that
// band does not exist, and the result is exactly the sphere's 482:2880 topology
// with no duplicated ring and no zero-area triangle anywhere in it.
ConstructionMesh ConstructionCapsule::generateMesh() const {
    const uint32_t n = kCapsuleRadialSegments;
    const uint32_t h = kCapsuleHemisphereBands;
    const bool hasMiddle = capsuleHasMiddle(dimensions_);
    const uint32_t rings = capsuleRingCount(dimensions_);

    const double radius = dimensions_.diameter * 0.5;
    // Zero when there is no middle, so the two hemispheres meet on the origin
    // plane and the shape is a sphere.
    const double halfMiddle = hasMiddle ? (dimensions_.totalHeight - dimensions_.diameter) * 0.5
                                        : 0.0;

    ConstructionMesh mesh;
    mesh.vertices.resize(rings * n + 2);

    for (uint32_t r = 0; r < rings; ++r) {
        // Which hemisphere this ring belongs to, and which band of it. With a
        // middle, ring h is the bottom seam; without one, ring h-1 is the shared
        // seam and the bottom hemisphere resumes at ring h with band h-1.
        const bool northern = r < h;
        const uint32_t band = northern ? (r + 1)
                                       : (hasMiddle ? (2 * h - r) : (2 * h - 1 - r));
        const double sign = northern ? 1.0 : -1.0;

        double cosTheta = 0.0;
        double sinTheta = 0.0;
        exactHemisphereLatitude(band, &cosTheta, &sinTheta);

        // The seam bands have cosTheta exactly 0, so a seam ring lands exactly
        // on +/- middle/2, and sinTheta exactly 1, so its radius is exactly the
        // capsule's.
        const float y = static_cast<float>(sign * (halfMiddle + radius * cosTheta));

        for (uint32_t j = 0; j < n; ++j) {
            double dirCos = 0.0;
            double dirSin = 0.0;
            exactRingDirection(j, n, &dirCos, &dirSin);

            // The two exact factors are multiplied in double and converted once,
            // so a seam vertex on a cardinal meridian lands on exactly
            // +/- radius instead of one rounding short of it.
            const float x = static_cast<float>(dirCos * sinTheta * radius);
            const float z = static_cast<float>(dirSin * sinTheta * radius);

            // Colour is derived PRESENTATION data: it sweeps with longitude and
            // brightens toward the top, so the capsule's orientation, its seam
            // and the boundary between its middle and its ends are readable by
            // eye. No dimension is ever inferred from it.
            const float warm = static_cast<float>(0.5 + 0.5 * dirCos);
            const float cool = static_cast<float>(0.5 + 0.5 * dirSin);
            const float up = static_cast<float>(northern ? 0.5 + 0.5 * cosTheta
                                                         : 0.5 - 0.5 * cosTheta);

            MeshVertex& v = mesh.vertices[r * n + j];
            v.position[0] = x;
            v.position[1] = y;
            v.position[2] = z;
            v.color[0] = 0.18f + 0.68f * warm * up;
            v.color[1] = 0.22f + 0.52f * cool;
            v.color[2] = 0.38f + 0.58f * up;
        }
    }

    const uint32_t northPole = rings * n;
    const uint32_t southPole = rings * n + 1;

    // The poles are written down from the authoritative TOTAL height rather than
    // accumulated as halfMiddle + radius, so the Y bounds are exactly
    // +/- totalHeight/2 rather than one double rounding away from it.
    const float halfTotal = static_cast<float>(dimensions_.totalHeight * 0.5);
    MeshVertex& north = mesh.vertices[northPole];
    north.position[0] = 0.0f;
    north.position[1] = halfTotal;
    north.position[2] = 0.0f;
    north.color[0] = 0.95f; north.color[1] = 0.96f; north.color[2] = 0.99f;

    MeshVertex& south = mesh.vertices[southPole];
    south.position[0] = 0.0f;
    south.position[1] = -halfTotal;
    south.position[2] = 0.0f;
    south.color[0] = 0.20f; south.color[1] = 0.15f; south.color[2] = 0.33f;

    // Canonical winding, identical to the sphere's: the band ordering is the
    // cylinder's side-wall ordering with "lower ring" and "upper ring" in the
    // same roles, and the two pole fans are its two cap fans.
    mesh.indices.clear();
    mesh.indices.reserve(6 * n * rings);

    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t j1 = (j + 1) % n;
        mesh.indices.push_back(northPole);
        mesh.indices.push_back(j1);
        mesh.indices.push_back(j);
    }

    for (uint32_t r = 0; r + 1 < rings; ++r) {
        const uint32_t upper = r * n;
        const uint32_t lower = (r + 1) * n;
        for (uint32_t j = 0; j < n; ++j) {
            const uint32_t j1 = (j + 1) % n;
            const uint32_t u0 = upper + j;
            const uint32_t u1 = upper + j1;
            const uint32_t l0 = lower + j;
            const uint32_t l1 = lower + j1;

            mesh.indices.push_back(l0);
            mesh.indices.push_back(u0);
            mesh.indices.push_back(u1);

            mesh.indices.push_back(l0);
            mesh.indices.push_back(u1);
            mesh.indices.push_back(l1);
        }
    }

    const uint32_t lastRing = (rings - 1) * n;
    for (uint32_t j = 0; j < n; ++j) {
        const uint32_t j1 = (j + 1) % n;
        mesh.indices.push_back(southPole);
        mesh.indices.push_back(lastRing + j);
        mesh.indices.push_back(lastRing + j1);
    }
    return mesh;
}

// ---------------------------------------------------------------------------
// Plane
// ---------------------------------------------------------------------------

PrimitiveUpdateStatus ConstructionPlane::setDimensionsMeters(Meters width, Meters depth,
                                                              DimensionValidation* outWhy) {
    DimensionValidation why = validateDimensionMeters(width);
    if (why == DimensionValidation::Ok) {
        why = validateDimensionMeters(depth);
    }
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;
    }

    if (width == dimensions_.width && depth == dimensions_.depth) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    dimensions_.width = width;
    dimensions_.depth = depth;
    ++updateCount_;
    return PrimitiveUpdateStatus::Applied;
}

// Vertex layout, fixed and shared with the index table above:
//
//   0   -x -z          1   +x -z
//   3   -x +z          2   +x +z
//
// Exactly 4 vertices and 2 triangles, always — a plane has no tessellation
// parameter because there is nothing to divide. Y is always exactly 0: this is
// a zero-thickness sheet, not a thin box.
ConstructionMesh ConstructionPlane::generateMesh() const {
    const float halfX = static_cast<float>(dimensions_.width * 0.5);
    const float halfZ = static_cast<float>(dimensions_.depth * 0.5);

    ConstructionMesh mesh;
    mesh.vertices.resize(kPlaneVertexCount);
    for (uint32_t i = 0; i < kPlaneVertexCount; ++i) {
        MeshVertex& v = mesh.vertices[i];
        v.position[0] = kPlaneCornerSigns[i][0] * halfX;
        v.position[1] = 0.0f;
        v.position[2] = kPlaneCornerSigns[i][1] * halfZ;
        v.color[0] = kPlaneCornerColors[i][0];
        v.color[1] = kPlaneCornerColors[i][1];
        v.color[2] = kPlaneCornerColors[i][2];
    }
    mesh.indices.assign(kPlaneIndices, kPlaneIndices + kPlaneIndexCount);
    // A Plane has no interior: it is the one primitive whose render and pick
    // layers are each authorized to treat both sides as legitimate surface.
    mesh.renderBothSides = true;
    return mesh;
}

// ---------------------------------------------------------------------------
// The one active object
// ---------------------------------------------------------------------------

PrimitiveSpec PrimitiveSpec::of(const BoxDimensionsMeters& box) {
    PrimitiveSpec spec;
    spec.payload_ = box;
    return spec;
}

PrimitiveSpec PrimitiveSpec::of(const CylinderDimensionsMeters& cylinder) {
    PrimitiveSpec spec;
    spec.payload_ = cylinder;
    return spec;
}

PrimitiveSpec PrimitiveSpec::of(const SphereDimensionsMeters& sphere) {
    PrimitiveSpec spec;
    spec.payload_ = sphere;
    return spec;
}

PrimitiveSpec PrimitiveSpec::of(const ConeDimensionsMeters& cone) {
    PrimitiveSpec spec;
    spec.payload_ = cone;
    return spec;
}

PrimitiveSpec PrimitiveSpec::of(const CapsuleDimensionsMeters& capsule) {
    PrimitiveSpec spec;
    spec.payload_ = capsule;
    return spec;
}

PrimitiveSpec PrimitiveSpec::of(const PlaneDimensionsMeters& plane) {
    PrimitiveSpec spec;
    spec.payload_ = plane;
    return spec;
}

PrimitiveSpec PrimitiveSpec::forBox(Meters width, Meters height, Meters depth) {
    BoxDimensionsMeters box;
    box.width = width;
    box.height = height;
    box.depth = depth;
    return of(box);
}

PrimitiveSpec PrimitiveSpec::forCylinder(Meters diameter, Meters height) {
    CylinderDimensionsMeters cylinder;
    cylinder.diameter = diameter;
    cylinder.height = height;
    return of(cylinder);
}

PrimitiveSpec PrimitiveSpec::forSphere(Meters diameter) {
    SphereDimensionsMeters sphere;
    sphere.diameter = diameter;
    return of(sphere);
}

PrimitiveSpec PrimitiveSpec::forCone(Meters bottomDiameter, Meters height) {
    ConeDimensionsMeters cone;
    cone.bottomDiameter = bottomDiameter;
    cone.height = height;
    return of(cone);
}

PrimitiveSpec PrimitiveSpec::forCapsule(Meters diameter, Meters totalHeight) {
    CapsuleDimensionsMeters capsule;
    capsule.diameter = diameter;
    capsule.totalHeight = totalHeight;
    return of(capsule);
}

PrimitiveSpec PrimitiveSpec::forPlane(Meters width, Meters depth) {
    PlaneDimensionsMeters plane;
    plane.width = width;
    plane.depth = depth;
    return of(plane);
}

// The payload's alternative order IS the kind, so a mismatch here would make
// every kind() answer silently wrong. Caught at compile time instead.
static_assert(PrimitiveSpec::Payload{BoxDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Box),
              "PrimitiveSpec payload order must match PrimitiveKind");
static_assert(PrimitiveSpec::Payload{CylinderDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Cylinder),
              "PrimitiveSpec payload order must match PrimitiveKind");
static_assert(PrimitiveSpec::Payload{SphereDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Sphere),
              "PrimitiveSpec payload order must match PrimitiveKind");
static_assert(PrimitiveSpec::Payload{ConeDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Cone),
              "PrimitiveSpec payload order must match PrimitiveKind");
static_assert(PrimitiveSpec::Payload{CapsuleDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Capsule),
              "PrimitiveSpec payload order must match PrimitiveKind");
static_assert(PrimitiveSpec::Payload{PlaneDimensionsMeters{}}.index() ==
                  static_cast<size_t>(PrimitiveKind::Plane),
              "PrimitiveSpec payload order must match PrimitiveKind");

PrimitiveSpec ConstructionObject::spec() const {
    switch (kind_) {
        case PrimitiveKind::Box: return PrimitiveSpec::of(box_.dimensionsMeters());
        case PrimitiveKind::Cylinder: return PrimitiveSpec::of(cylinder_.dimensionsMeters());
        case PrimitiveKind::Sphere: return PrimitiveSpec::of(sphere_.dimensionsMeters());
        case PrimitiveKind::Cone: return PrimitiveSpec::of(cone_.dimensionsMeters());
        case PrimitiveKind::Capsule: return PrimitiveSpec::of(capsule_.dimensionsMeters());
        case PrimitiveKind::Plane: return PrimitiveSpec::of(plane_.dimensionsMeters());
    }
    return PrimitiveSpec::of(box_.dimensionsMeters());
}

namespace {

// Validates whichever parameters the request actually carries. There is no
// branch on a separate kind field, because there is no separate kind field: the
// payload decides which overload runs.
DimensionValidation validateParameters(const BoxDimensionsMeters& box) {
    DimensionValidation why = validateDimensionMeters(box.width);
    if (why == DimensionValidation::Ok) why = validateDimensionMeters(box.height);
    if (why == DimensionValidation::Ok) why = validateDimensionMeters(box.depth);
    return why;
}

DimensionValidation validateParameters(const CylinderDimensionsMeters& cylinder) {
    DimensionValidation why = validateDimensionMeters(cylinder.diameter);
    if (why == DimensionValidation::Ok) why = validateDimensionMeters(cylinder.height);
    return why;
}

DimensionValidation validateParameters(const SphereDimensionsMeters& sphere) {
    return validateDimensionMeters(sphere.diameter);
}

DimensionValidation validateParameters(const ConeDimensionsMeters& cone) {
    DimensionValidation why = validateDimensionMeters(cone.bottomDiameter);
    if (why == DimensionValidation::Ok) why = validateDimensionMeters(cone.height);
    return why;
}

// The capsule is the one primitive whose parameters are related, so it is the
// one that has validation of its own beyond the per-length rule.
DimensionValidation validateParameters(const CapsuleDimensionsMeters& capsule) {
    return validateCapsuleMeters(capsule.diameter, capsule.totalHeight);
}

// A plane's two extents are independent, exactly like a box's three: there is
// no relation to enforce beyond the ordinary per-length rule.
DimensionValidation validateParameters(const PlaneDimensionsMeters& plane) {
    DimensionValidation why = validateDimensionMeters(plane.width);
    if (why == DimensionValidation::Ok) why = validateDimensionMeters(plane.depth);
    return why;
}

bool sameParameters(const BoxDimensionsMeters& a, const BoxDimensionsMeters& b) {
    return a.width == b.width && a.height == b.height && a.depth == b.depth;
}

bool sameParameters(const CylinderDimensionsMeters& a, const CylinderDimensionsMeters& b) {
    return a.diameter == b.diameter && a.height == b.height;
}

bool sameParameters(const SphereDimensionsMeters& a, const SphereDimensionsMeters& b) {
    return a.diameter == b.diameter;
}

bool sameParameters(const ConeDimensionsMeters& a, const ConeDimensionsMeters& b) {
    return a.bottomDiameter == b.bottomDiameter && a.height == b.height;
}

bool sameParameters(const CapsuleDimensionsMeters& a, const CapsuleDimensionsMeters& b) {
    return a.diameter == b.diameter && a.totalHeight == b.totalHeight;
}

bool sameParameters(const PlaneDimensionsMeters& a, const PlaneDimensionsMeters& b) {
    return a.width == b.width && a.depth == b.depth;
}

}  // namespace

// One overload per primitive kind, each comparing the requested payload
// against the matching member's current parameters. Replaces an if-else chain
// over typed accessors: std::visit in setPrimitive below requires a matching
// overload for every PrimitiveSpec payload alternative, so a kind added to the
// variant without a matching overload here is a COMPILE error instead of a
// silently-skipped branch.
bool ConstructionObject::parametersDiffer(const BoxDimensionsMeters& box) const {
    return !sameParameters(box, box_.dimensionsMeters());
}
bool ConstructionObject::parametersDiffer(const CylinderDimensionsMeters& cylinder) const {
    return !sameParameters(cylinder, cylinder_.dimensionsMeters());
}
bool ConstructionObject::parametersDiffer(const SphereDimensionsMeters& sphere) const {
    return !sameParameters(sphere, sphere_.dimensionsMeters());
}
bool ConstructionObject::parametersDiffer(const ConeDimensionsMeters& cone) const {
    return !sameParameters(cone, cone_.dimensionsMeters());
}
bool ConstructionObject::parametersDiffer(const CapsuleDimensionsMeters& capsule) const {
    return !sameParameters(capsule, capsule_.dimensionsMeters());
}
bool ConstructionObject::parametersDiffer(const PlaneDimensionsMeters& plane) const {
    return !sameParameters(plane, plane_.dimensionsMeters());
}

void ConstructionObject::writeParameters(const BoxDimensionsMeters& box) {
    box_.setDimensionsMeters(box.width, box.height, box.depth);
}
void ConstructionObject::writeParameters(const CylinderDimensionsMeters& cylinder) {
    cylinder_.setDimensionsMeters(cylinder.diameter, cylinder.height);
}
void ConstructionObject::writeParameters(const SphereDimensionsMeters& sphere) {
    sphere_.setDimensionsMeters(sphere.diameter);
}
void ConstructionObject::writeParameters(const ConeDimensionsMeters& cone) {
    cone_.setDimensionsMeters(cone.bottomDiameter, cone.height);
}
void ConstructionObject::writeParameters(const CapsuleDimensionsMeters& capsule) {
    capsule_.setDimensionsMeters(capsule.diameter, capsule.totalHeight);
}
void ConstructionObject::writeParameters(const PlaneDimensionsMeters& plane) {
    plane_.setDimensionsMeters(plane.width, plane.depth);
}

PrimitiveUpdateStatus ConstructionObject::setPrimitive(const PrimitiveSpec& requested,
                                                       DimensionValidation* outWhy) {
    // Validate FIRST, so that a refused request cannot leave the kind switched
    // or a parameter half written. Nothing below this point can fail.
    const DimensionValidation why = std::visit(
        [](const auto& parameters) { return validateParameters(parameters); }, requested.payload());
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != DimensionValidation::Ok) {
        ++rejectedUpdates_;
        return PrimitiveUpdateStatus::Rejected;  // kind, parameters and transform all stand
    }

    // A kind change is always a change, even when the target primitive already
    // holds exactly these parameters: the object is a different shape after it.
    const bool kindChanged = requested.kind() != kind_;
    const bool parametersChanged = std::visit(
        [this](const auto& parameters) { return parametersDiffer(parameters); },
        requested.payload());

    if (!kindChanged && !parametersChanged) {
        return PrimitiveUpdateStatus::Unchanged;
    }

    std::visit([this](const auto& parameters) { writeParameters(parameters); },
              requested.payload());
    kind_ = requested.kind();
    ++updateCount_;
    // transform_ is deliberately untouched: where the object sits is not part of
    // what it is.
    return PrimitiveUpdateStatus::Applied;
}

ConstructionMesh ConstructionObject::generateMesh() const {
    switch (kind_) {
        case PrimitiveKind::Box: return box_.generateMesh();
        case PrimitiveKind::Cylinder: return cylinder_.generateMesh();
        case PrimitiveKind::Sphere: return sphere_.generateMesh();
        case PrimitiveKind::Cone: return cone_.generateMesh();
        case PrimitiveKind::Capsule: return capsule_.generateMesh();
        case PrimitiveKind::Plane: return plane_.generateMesh();
    }
    return box_.generateMesh();
}

// ---------------------------------------------------------------------------
// Publication and the one apply entry point
// ---------------------------------------------------------------------------

namespace {

MeshRevision publishMesh(const ConstructionMesh& mesh, MeshStore& store, MeshValidation* outWhy) {
    return store.publish(mesh.vertices.data(), static_cast<uint32_t>(mesh.vertices.size()),
                         mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size()), outWhy,
                         mesh.renderBothSides);
}

}  // namespace

MeshRevision publishConstructionObject(const ConstructionObject& object, MeshStore& store,
                                       MeshValidation* outWhy) {
    return publishMesh(object.generateMesh(), store, outWhy);
}

MeshRevision publishConstructionBox(const ConstructionBox& box, MeshStore& store,
                                    MeshValidation* outWhy) {
    return publishMesh(box.generateMesh(), store, outWhy);
}

PrimitiveApplyResult applyPrimitive(ConstructionObject& object, MeshStore& store,
                                    const PrimitiveSpec& requested) {
    PrimitiveApplyResult result;
    result.status = object.setPrimitive(requested, &result.validation);
    result.spec = object.spec();

    if (result.status != PrimitiveUpdateStatus::Applied) {
        // Unchanged and Rejected both publish nothing, so an edit that changes
        // no shape can never cost a mesh revision or a GPU upload, and a refused
        // edit can never leave the store holding geometry that no parameter set
        // produced.
        result.revision = store.currentRevision();
        const RuntimeMeshPtr current = store.current();
        if (current) {
            result.vertexCount = current->vertexCount();
            result.indexCount = current->indexCount();
        }
        return result;
    }

    const ConstructionMesh mesh = object.generateMesh();
    result.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    result.indexCount = static_cast<uint32_t>(mesh.indices.size());
    result.revision = publishMesh(mesh, store, &result.meshValidation);
    result.published = result.revision != kNoMeshRevision;
    if (!result.published) {
        result.revision = store.currentRevision();
    }
    return result;
}

ConstructionObject& constructionObject() {
    static ConstructionObject object(kConstructionBoxObjectId);
    return object;
}

PrimitiveApplyResult applyConstructionPrimitive(const PrimitiveSpec& requested) {
    return applyPrimitive(constructionObject(), meshStore(), requested);
}

PrimitiveApplyResult applyConstructionBox(Meters width, Meters height, Meters depth) {
    return applyConstructionPrimitive(PrimitiveSpec::forBox(width, height, depth));
}

PrimitiveApplyResult applyConstructionCylinder(Meters diameter, Meters height) {
    return applyConstructionPrimitive(PrimitiveSpec::forCylinder(diameter, height));
}

PrimitiveApplyResult applyConstructionSphere(Meters diameter) {
    return applyConstructionPrimitive(PrimitiveSpec::forSphere(diameter));
}

PrimitiveApplyResult applyConstructionCone(Meters bottomDiameter, Meters height) {
    return applyConstructionPrimitive(PrimitiveSpec::forCone(bottomDiameter, height));
}

PrimitiveApplyResult applyConstructionCapsule(Meters diameter, Meters totalHeight) {
    return applyConstructionPrimitive(PrimitiveSpec::forCapsule(diameter, totalHeight));
}

PrimitiveApplyResult applyConstructionPlane(Meters width, Meters depth) {
    return applyConstructionPrimitive(PrimitiveSpec::forPlane(width, depth));
}

}  // namespace forgeshape
