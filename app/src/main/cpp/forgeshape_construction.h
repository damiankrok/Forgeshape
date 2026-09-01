// ForgeShape Construction domain — the exact primitives and the one active object.
//
// Platform-independent: no JNI, no Android, no Vulkan, no renderer and no UI
// type appears here, and nothing here holds a GPU resource.
//
// Source-of-truth hierarchy
// -------------------------
//     ConstructionObject
//         |  ObjectId              <-- stable identity, independent of everything
//         |  PrimitiveKind         <-- which primitive is ACTIVE
//         |  ConstructionBox       <-- exact W/H/D parameters
//         |  ConstructionCylinder  <-- exact diameter/height parameters
//         |  ConstructionSphere    <-- exact diameter parameter
//         |  ConstructionCone      <-- exact bottom-diameter/height parameters
//         |  ConstructionCapsule   <-- exact diameter/total-height parameters
//         |  ConstructionPlane     <-- exact width/depth parameters
//         |  ConstructionTransform <-- exact placement (see forgeshape_transform.h)
//         |
//         -> generateMesh()   (LOCAL-space, derived float RuntimeMesh data)
//             -> MeshStore
//                 -> CPU picking + renderer
//                     -> Vulkan buffers
//
// Nothing ever reads a parameter back out of mesh vertices or GPU data. The mesh
// is a derived artefact; the parameters are the truth.
//
// Unit contract
// -------------
// The internal Construction length unit is the METER, carried as `double` and
// named accordingly at every boundary. RuntimeMesh positions are DERIVED `float`
// data. There is no mm/cm/m presentation or input conversion in this module.
//
// Scope: exactly ONE active object which is a box, a cylinder, a sphere, a cone,
// a capsule or a plane. There is no registry, no scene graph, no hierarchy, no
// second object and no create or delete operation.
#pragma once

#include <cstdint>
#include <variant>
#include <vector>

#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_transform.h"

namespace forgeshape {

// The internal Construction length unit. Always meters, always double.
using Meters = double;

// The default box: deliberately non-cubic, so a wrong axis is visible instantly
// both on screen and in a log line.
constexpr Meters kDefaultBoxWidthMeters = 2.0;
constexpr Meters kDefaultBoxHeightMeters = 1.0;
constexpr Meters kDefaultBoxDepthMeters = 0.5;

// The default cylinder: taller than it is wide, for the same reason.
constexpr Meters kDefaultCylinderDiameterMeters = 1.0;
constexpr Meters kDefaultCylinderHeightMeters = 2.0;

// The default sphere. A sphere has nothing to be non-cubic about: one diameter
// is the whole of it.
constexpr Meters kDefaultSphereDiameterMeters = 1.0;

// The default cone: as tall as it is wide, so neither dimension can be mistaken
// for the other by eye.
constexpr Meters kDefaultConeBottomDiameterMeters = 1.0;
constexpr Meters kDefaultConeHeightMeters = 1.0;

// The default capsule: clearly taller than it is wide, so the cylindrical middle
// and both hemispherical ends are all visible at once. The invariant
// totalHeight >= diameter holds.
constexpr Meters kDefaultCapsuleDiameterMeters = 1.0;
constexpr Meters kDefaultCapsuleTotalHeightMeters = 2.0;

// The default plane: square, so neither of its two extents can be mistaken for
// the other by eye.
constexpr Meters kDefaultPlaneWidthMeters = 1.0;
constexpr Meters kDefaultPlaneDepthMeters = 1.0;

// A box is 8 logical corners and 12 triangles, always.
constexpr uint32_t kBoxVertexCount = 8;
constexpr uint32_t kBoxIndexCount = 36;

// A plane is 4 logical corners and 2 triangles, always — one rectangle, no
// tessellation, because there is nothing to divide.
constexpr uint32_t kPlaneVertexCount = 4;
constexpr uint32_t kPlaneIndexCount = 6;

// ---------------------------------------------------------------------------
// Shared primitive tessellation
// ---------------------------------------------------------------------------
//
// ONE owner for the two numbers every round primitive is drawn with. Each
// generator names these rather than writing down a segment count of its own, so
// the cylinder, the sphere, the cone and the capsule cannot drift apart and a
// capsule's hemispheres are provably the same tessellation as a sphere's.
//
// Both are FIXED and deliberately not user-editable: they are a rendering detail
// of an exact shape, not a property of it. A user who could set them would be
// authoring the approximation rather than the shape, and the approximation would
// then have to be persisted, versioned and validated like a real parameter.
//
// 32 radial segments, divisible by FOUR, so the four cardinal directions
// (0/90/180/270 degrees) land exactly on ring vertices and a generated X/Z bound
// is exactly the radius rather than the radius times cos(pi/32).
constexpr uint32_t kPrimitiveRadialSegments = 32;
// 16 latitude bands from pole to pole. EVEN, so one ring falls exactly on the
// equator (ring radius = the radius) rather than straddling it, and so a
// hemisphere is exactly half of them.
constexpr uint32_t kPrimitiveLatitudeStacks = 16;

// Cylinder tessellation.
constexpr uint32_t kCylinderRadialSegments = kPrimitiveRadialSegments;

// Two shared rings plus one centre per cap; sides + both caps.
constexpr uint32_t kCylinderVertexCount = 2 * kCylinderRadialSegments + 2;  // 66
constexpr uint32_t kCylinderIndexCount = 12 * kCylinderRadialSegments;      // 384

// Sphere tessellation.
constexpr uint32_t kSphereRadialSegments = kPrimitiveRadialSegments;
constexpr uint32_t kSphereStacks = kPrimitiveLatitudeStacks;
// The rings strictly between the two poles.
constexpr uint32_t kSphereRingCount = kSphereStacks - 1;  // 15

// Ring vertices plus one vertex per pole. The poles are single vertices fanned
// to the adjacent ring, which is what keeps every triangle non-degenerate.
constexpr uint32_t kSphereVertexCount =
    kSphereRingCount * kSphereRadialSegments + 2;  // 482
// Two triangles per quad in each of the (kSphereStacks - 2) full bands, plus one
// fan triangle per meridian at each pole: 2 * N * (S - 1) triangles.
constexpr uint32_t kSphereIndexCount =
    6 * kSphereRadialSegments * (kSphereStacks - 1);  // 2880

// Cone tessellation: one base ring, one base centre, one apex.
constexpr uint32_t kConeRadialSegments = kPrimitiveRadialSegments;
// The apex is ONE vertex fanned to the base ring, not a collapsed second ring:
// a collapsed ring would put kConeRadialSegments zero-area triangles at the tip.
constexpr uint32_t kConeVertexCount = kConeRadialSegments + 2;  // 34
// One side triangle plus one base-fan triangle per segment.
constexpr uint32_t kConeIndexCount = 6 * kConeRadialSegments;  // 192

// Capsule tessellation. Each hemispherical end is exactly half of the sphere's
// latitude bands, so a capsule end IS a sphere end, at the same fidelity.
constexpr uint32_t kCapsuleRadialSegments = kPrimitiveRadialSegments;
constexpr uint32_t kCapsuleHemisphereBands = kPrimitiveLatitudeStacks / 2;  // 8

// A capsule with a cylindrical middle carries TWO seam rings, one at each end of
// that middle: kCapsuleHemisphereBands rings per hemisphere, the last of which
// is that hemisphere's seam ring.
constexpr uint32_t kCapsuleRingCount = 2 * kCapsuleHemisphereBands;  // 16
constexpr uint32_t kCapsuleVertexCount =
    kCapsuleRingCount * kCapsuleRadialSegments + 2;  // 514
// R rings plus two poles is (R - 1) quad bands plus two pole fans: 2*N*R
// triangles.
constexpr uint32_t kCapsuleIndexCount =
    6 * kCapsuleRadialSegments * kCapsuleRingCount;  // 3072

// The degenerate capsule — totalHeight exactly equal to diameter — has NO
// cylindrical middle, so its two seam rings are one ring and it is exactly the
// sphere's topology rather than the capsule's with a zero-height band in it.
// This is stated as an assertion, not a coincidence: it is what makes the
// equality case free of duplicate rings and zero-area triangles.
constexpr uint32_t kCapsuleSphericalVertexCount = kSphereVertexCount;  // 482
constexpr uint32_t kCapsuleSphericalIndexCount = kSphereIndexCount;    // 2880
static_assert(kCapsuleSphericalVertexCount ==
                  (kCapsuleRingCount - 1) * kCapsuleRadialSegments + 2,
              "a capsule with no middle must be exactly one ring shorter");
static_assert(kCapsuleSphericalIndexCount ==
                  6 * kCapsuleRadialSegments * (kCapsuleRingCount - 1),
              "a capsule with no middle must be exactly one band shorter");

// Which primitive the one active object currently is.
//
// The numeric order matters: it is the alternative order of PrimitiveSpec's
// payload variant, and the two are asserted to agree.
enum class PrimitiveKind {
    Box,
    Cylinder,
    Sphere,
    Cone,
    Capsule,
    Plane,
};

const char* primitiveKindName(PrimitiveKind kind);

// Why a requested dimension was refused. There is no arbitrary user-facing size
// limit here: the only rejections are values that are not a physical length, or
// that cannot survive the conversion into the derived float mesh.
enum class DimensionValidation {
    Ok,
    NotFinite,        // NaN or infinity
    NotPositive,      // zero or negative
    NotRepresentable, // the derived float half-extent would be 0 or non-finite
    // Every value is a usable length on its own, but the primitive's own rule
    // relating two of them is broken. Today there is exactly one such rule: a
    // capsule's total height cannot be less than its diameter, because the two
    // hemispherical ends alone are already that tall.
    //
    // This is a distinct reason on purpose: "0.5 m is not a length" and "0.5 m
    // is too short to be this capsule's total height" are different problems and
    // need different messages.
    RelationInvalid,
};

const char* dimensionValidationName(DimensionValidation why);

// Outcome of a primitive update request.
enum class PrimitiveUpdateStatus {
    Applied,    // parameters or kind changed; the caller should publish a new mesh
    Unchanged,  // the request was valid but identical; nothing was published
    Rejected,   // invalid; previous valid state is fully preserved
};

const char* primitiveUpdateStatusName(PrimitiveUpdateStatus status);

// True when this single dimension is a usable Construction length.
DimensionValidation validateDimensionMeters(Meters value);

// The capsule's complete rule, in one place: both lengths usable on their own,
// totalHeight >= diameter, and a derived float geometry that can actually
// resolve the shape rather than merely being finite.
//
// It exists as a named function because the capsule is the first primitive whose
// two parameters are related, and that relation belongs to the domain rather
// than to a UI check or a generator comment.
DimensionValidation validateCapsuleMeters(Meters diameter, Meters totalHeight);

// The three authoritative box parameters, in meters.
struct BoxDimensionsMeters {
    Meters width = kDefaultBoxWidthMeters;
    Meters height = kDefaultBoxHeightMeters;
    Meters depth = kDefaultBoxDepthMeters;
};

// The two authoritative cylinder parameters, in meters. Diameter, not radius:
// diameter is what a drawing and a caliper give you.
struct CylinderDimensionsMeters {
    Meters diameter = kDefaultCylinderDiameterMeters;
    Meters height = kDefaultCylinderHeightMeters;
};

// The one authoritative sphere parameter, in meters. Diameter for the same
// reason as the cylinder's; the radius is derived and never stored.
//
// This is a distinct TYPE rather than a bare double precisely so that a sphere's
// diameter cannot be handed to a cylinder, or a cylinder's height to a sphere,
// by a caller that got its argument order wrong.
struct SphereDimensionsMeters {
    Meters diameter = kDefaultSphereDiameterMeters;
};

// The two authoritative cone parameters, in meters.
//
// `bottomDiameter` is named for the end it belongs to because a cone has two
// ends and only one of them has a diameter at all: the apex radius is ZERO, by
// definition, and is not a parameter. There is deliberately no top diameter and
// no frustum: a truncated cone is a different shape, not a cone with an extra
// number.
struct ConeDimensionsMeters {
    Meters bottomDiameter = kDefaultConeBottomDiameterMeters;
    Meters height = kDefaultConeHeightMeters;
};

// The two authoritative capsule parameters, in meters.
//
// `totalHeight` is the WHOLE height, hemispherical ends included — the number a
// caliper gives you — not the length of the cylindrical middle. The middle is
// derived (`totalHeight - diameter`) and never stored, for the same reason a
// radius is never stored: it is not what the user measured.
//
// The two are related, and the relation is part of the contract:
// `totalHeight >= diameter`, because the two hemispheres alone are already
// `diameter` tall. Equality is valid and means a capsule with no middle at all.
struct CapsuleDimensionsMeters {
    Meters diameter = kDefaultCapsuleDiameterMeters;
    Meters totalHeight = kDefaultCapsuleTotalHeightMeters;
};

// The two authoritative plane parameters, in meters: local X extent and local Z
// extent of a flat, finite, zero-thickness rectangular sheet. Independent of
// each other — a plane has no relation to validate beyond the ordinary
// per-length rule every primitive has.
struct PlaneDimensionsMeters {
    Meters width = kDefaultPlaneWidthMeters;
    Meters depth = kDefaultPlaneDepthMeters;
};

// Generated geometry, ready to hand to MeshStore. Derived data, not truth.
struct ConstructionMesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    // True only for a flat, open, single-sided sheet with no "inside" — today,
    // exactly the Construction Plane. See createRuntimeMesh's doc comment in
    // forgeshape_mesh.h for what this authorizes downstream and why it is a
    // geometric fact about the mesh rather than a display setting.
    bool renderBothSides = false;
};

// The exact-dimension Construction box.
//
// Its parameter state IS the geometry. Mesh generation is a pure deterministic
// function of that state, so the same parameters always produce byte-identical
// vertices, and the object's identity is independent of both.
class ConstructionBox {
public:
    explicit ConstructionBox(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    // Stable for the life of the object: it does NOT change when dimensions
    // change, when a new mesh revision is published, or when GPU buffers are
    // reallocated. That is what keeps selection coherent across an edit.
    ObjectId objectId() const { return objectId_; }

    Meters widthMeters() const { return dimensions_.width; }
    Meters heightMeters() const { return dimensions_.height; }
    Meters depthMeters() const { return dimensions_.depth; }
    BoxDimensionsMeters dimensionsMeters() const { return dimensions_; }

    // How many times the parameters actually changed. Introspection for logging
    // and self-tests only; it is not a mesh revision.
    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Requests new authoritative dimensions.
    //
    // FAILS CLOSED: if any of the three is invalid, NOTHING is applied — not
    // even the valid ones — and the previous parameters remain exactly as they
    // were. A request identical to the current state is a no-op and reports
    // Unchanged, so it cannot cause a pointless mesh revision.
    //
    // `outWhy` receives the reason for the FIRST invalid dimension found, in
    // width, height, depth order, and Ok otherwise.
    PrimitiveUpdateStatus setDimensionsMeters(Meters width, Meters height, Meters depth,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation from the current parameters: 8 corners at
    // +/- width/2, +/- height/2, +/- depth/2 around the LOCAL origin, and 36
    // indices in the canonical ForgeShape winding (counter-clockwise seen from
    // outside). Colour is derived PRESENTATION data, not Construction truth.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    BoxDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The exact-dimension Construction cylinder.
//
// Contract: centred on the LOCAL origin, axis along local +Y, so its extents are
// exactly +/- height/2 in Y and +/- diameter/2 in X and Z. Radial tessellation
// is fixed (see kCylinderRadialSegments) and is not a parameter.
class ConstructionCylinder {
public:
    explicit ConstructionCylinder(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    ObjectId objectId() const { return objectId_; }

    Meters diameterMeters() const { return dimensions_.diameter; }
    Meters heightMeters() const { return dimensions_.height; }
    // Derived, never stored: the authoritative parameter is the diameter.
    Meters radiusMeters() const { return dimensions_.diameter * 0.5; }
    CylinderDimensionsMeters dimensionsMeters() const { return dimensions_; }

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Same fail-closed contract as the box: both values are validated before
    // either is written.
    //
    // `outWhy` receives the reason for the FIRST invalid value found, in
    // diameter then height order, and Ok otherwise.
    PrimitiveUpdateStatus setDimensionsMeters(Meters diameter, Meters height,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation: two shared rings of kCylinderRadialSegments
    // vertices plus one centre vertex per cap, closed sides and closed top and
    // bottom, all in the canonical ForgeShape winding. The four cardinal
    // directions use exact 0 / +/-1 sines and cosines rather than trigonometry,
    // so the X and Z bounds are exactly the radius.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    CylinderDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The exact-diameter Construction sphere.
//
// Contract: centred on the LOCAL origin, so its extents are exactly
// +/- diameter/2 on all three axes. The authoritative parameter is the DIAMETER
// as double meters; the radius is derived on demand and never stored. Radial and
// latitudinal tessellation are fixed (see kSphereRadialSegments /
// kSphereStacks) and are not parameters.
class ConstructionSphere {
public:
    explicit ConstructionSphere(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    ObjectId objectId() const { return objectId_; }

    Meters diameterMeters() const { return dimensions_.diameter; }
    // Derived, never stored: the authoritative parameter is the diameter.
    Meters radiusMeters() const { return dimensions_.diameter * 0.5; }
    SphereDimensionsMeters dimensionsMeters() const { return dimensions_; }

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Same fail-closed contract as the box and the cylinder. With a single
    // parameter there is nothing to leave half written, but the rule is stated
    // the same way so the three primitives cannot drift apart.
    PrimitiveUpdateStatus setDimensionsMeters(Meters diameter,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation: kSphereRingCount rings of kSphereRadialSegments
    // vertices, plus one vertex at each pole fanned to the adjacent ring, all in
    // the canonical ForgeShape winding. The equator ring and the four cardinal
    // meridians use exact 0 / +/-1 sines and cosines rather than trigonometry,
    // so the X, Y and Z bounds are exactly the radius.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    SphereDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The exact-dimension Construction cone.
//
// Contract: centred on the LOCAL origin, axis along local +Y, base at
// y = -height/2 and apex at y = +height/2, so its extents are exactly
// +/- height/2 in Y and +/- bottomDiameter/2 in X and Z. The base is CLOSED. The
// apex radius is zero by definition and is not a parameter. Radial tessellation
// is shared (see kConeRadialSegments) and is not a parameter.
class ConstructionCone {
public:
    explicit ConstructionCone(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    ObjectId objectId() const { return objectId_; }

    Meters bottomDiameterMeters() const { return dimensions_.bottomDiameter; }
    Meters heightMeters() const { return dimensions_.height; }
    // Derived, never stored: the authoritative parameter is the diameter.
    Meters bottomRadiusMeters() const { return dimensions_.bottomDiameter * 0.5; }
    ConeDimensionsMeters dimensionsMeters() const { return dimensions_; }

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Same fail-closed contract as every other primitive: both values are
    // validated before either is written.
    //
    // `outWhy` receives the reason for the FIRST invalid value found, in
    // bottom-diameter then height order, and Ok otherwise.
    PrimitiveUpdateStatus setDimensionsMeters(Meters bottomDiameter, Meters height,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation: one base ring of kConeRadialSegments vertices,
    // one base centre and ONE apex vertex fanned to the ring, all in the
    // canonical ForgeShape winding. The four cardinal directions use exact
    // 0 / +/-1 sines and cosines rather than trigonometry, so the X and Z bounds
    // are exactly the bottom radius.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    ConeDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The exact-dimension Construction capsule.
//
// Contract: centred on the LOCAL origin, axis along local +Y. Radius is
// diameter/2; the cylindrical middle is totalHeight - diameter long and is
// centred on the origin too, so its ends are at y = +/- (totalHeight -
// diameter)/2; each hemispherical end has that same radius and caps that end.
// The extents are therefore exactly +/- diameter/2 in X and Z and
// +/- totalHeight/2 in Y. Tessellation is shared (see kCapsuleRadialSegments,
// kCapsuleHemisphereBands) and is not a parameter.
//
// totalHeight == diameter is VALID and describes a capsule with no middle at
// all, which is a sphere. It is generated as one, with the two seam rings
// collapsed into a single shared ring, so the equality case has no duplicated
// zero-length ring and no zero-area triangle in it.
class ConstructionCapsule {
public:
    explicit ConstructionCapsule(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    ObjectId objectId() const { return objectId_; }

    Meters diameterMeters() const { return dimensions_.diameter; }
    Meters totalHeightMeters() const { return dimensions_.totalHeight; }
    // Both derived, never stored: the authoritative parameters are the diameter
    // and the TOTAL height, which is what a caliper measures.
    Meters radiusMeters() const { return dimensions_.diameter * 0.5; }
    Meters middleHeightMeters() const {
        return dimensions_.totalHeight - dimensions_.diameter;
    }
    CapsuleDimensionsMeters dimensionsMeters() const { return dimensions_; }

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Same fail-closed contract as every other primitive, plus the capsule's own
    // relation: totalHeight may not be less than diameter.
    //
    // `outWhy` receives the reason for the FIRST problem found, in diameter,
    // total-height, then relation order, and Ok otherwise.
    PrimitiveUpdateStatus setDimensionsMeters(Meters diameter, Meters totalHeight,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation: kCapsuleHemisphereBands latitude bands per
    // hemisphere at the shared radial resolution, one pole vertex per end fanned
    // to its adjacent ring, and the cylindrical middle as the single band
    // between the two seam rings. When there is no middle the two seam rings ARE
    // one ring and that band does not exist.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    CapsuleDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The exact-dimension Construction plane: a flat, finite, ZERO-THICKNESS
// rectangular sheet — not a solid, not a sketch plane and not an infinite grid.
//
// Contract: centred on the LOCAL origin, lying in the local XZ plane at
// y = 0, with its canonical FRONT along local +Y. Extents are exactly
// +/- width/2 in X and +/- depth/2 in Z; Y is always exactly 0. There is no
// tessellation parameter because there is nothing to divide: the source
// topology is always exactly 4 vertices and 2 triangles.
class ConstructionPlane {
public:
    explicit ConstructionPlane(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId) {}

    ObjectId objectId() const { return objectId_; }

    Meters widthMeters() const { return dimensions_.width; }
    Meters depthMeters() const { return dimensions_.depth; }
    PlaneDimensionsMeters dimensionsMeters() const { return dimensions_; }

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Same fail-closed contract as every other primitive: both values are
    // validated before either is written.
    //
    // `outWhy` receives the reason for the FIRST invalid dimension found, in
    // width then depth order, and Ok otherwise.
    PrimitiveUpdateStatus setDimensionsMeters(Meters width, Meters depth,
                                              DimensionValidation* outWhy = nullptr);

    // Deterministic generation: exactly 4 corners at +/- width/2, +/- depth/2
    // around the LOCAL origin at y = 0, and 6 indices (2 triangles) in the
    // canonical ForgeShape winding, counter-clockwise seen from the canonical
    // front (+Y). Colour is derived PRESENTATION data, not Construction truth.
    ConstructionMesh generateMesh() const;

private:
    const ObjectId objectId_;
    PlaneDimensionsMeters dimensions_{};
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// A complete, self-describing primitive request: a payload that IS one
// primitive's parameters, and nothing else.
//
// This is a real tagged union (std::variant), not a struct carrying every
// primitive's parameters at once. That is the point: a spec built for a cylinder
// physically does not contain box or sphere values, so no caller can read one
// primitive's numbers as another's, and there is no such thing as an "inactive
// parameter group" travelling alongside the active one. `kind()` is derived from
// the payload rather than stored beside it, so the two cannot disagree.
//
// Construction is through the named factories only, so a request always says
// which primitive it is by construction.
class PrimitiveSpec {
public:
    // Alternative order MUST match PrimitiveKind's enumerator order.
    using Payload = std::variant<BoxDimensionsMeters, CylinderDimensionsMeters,
                                 SphereDimensionsMeters, ConeDimensionsMeters,
                                 CapsuleDimensionsMeters, PlaneDimensionsMeters>;

    // Defaults to the default box, which is what the object starts as.
    PrimitiveSpec() = default;

    static PrimitiveSpec forBox(Meters width, Meters height, Meters depth);
    static PrimitiveSpec forCylinder(Meters diameter, Meters height);
    static PrimitiveSpec forSphere(Meters diameter);
    static PrimitiveSpec forCone(Meters bottomDiameter, Meters height);
    static PrimitiveSpec forCapsule(Meters diameter, Meters totalHeight);
    static PrimitiveSpec forPlane(Meters width, Meters depth);

    static PrimitiveSpec of(const BoxDimensionsMeters& box);
    static PrimitiveSpec of(const CylinderDimensionsMeters& cylinder);
    static PrimitiveSpec of(const SphereDimensionsMeters& sphere);
    static PrimitiveSpec of(const ConeDimensionsMeters& cone);
    static PrimitiveSpec of(const CapsuleDimensionsMeters& capsule);
    static PrimitiveSpec of(const PlaneDimensionsMeters& plane);

    // Derived from the payload, never stored separately.
    PrimitiveKind kind() const { return static_cast<PrimitiveKind>(payload_.index()); }

    // Typed access. Each returns null unless the spec IS that primitive, so
    // reading the wrong one is a null check away rather than silent nonsense.
    const BoxDimensionsMeters* box() const { return std::get_if<BoxDimensionsMeters>(&payload_); }
    const CylinderDimensionsMeters* cylinder() const {
        return std::get_if<CylinderDimensionsMeters>(&payload_);
    }
    const SphereDimensionsMeters* sphere() const {
        return std::get_if<SphereDimensionsMeters>(&payload_);
    }
    const ConeDimensionsMeters* cone() const { return std::get_if<ConeDimensionsMeters>(&payload_); }
    const CapsuleDimensionsMeters* capsule() const {
        return std::get_if<CapsuleDimensionsMeters>(&payload_);
    }
    const PlaneDimensionsMeters* plane() const {
        return std::get_if<PlaneDimensionsMeters>(&payload_);
    }

    // For std::visit, so a caller can handle all three exhaustively.
    const Payload& payload() const { return payload_; }

private:
    Payload payload_{BoxDimensionsMeters{}};
};

// A complete, order-free copy of everything a Construction Body's Construction
// Source owns except its identity: which primitive is active, EVERY primitive's
// remembered parameters, and the placement.
//
// It exists for exactly one caller — the Construction history — and it is
// deliberately a plain value: no ObjectId, no mesh, no revision, no sculpt
// vertex. That is what makes an undo step a bounded Construction-domain fact
// rather than a copy of derived geometry, and it is why restoring one can never
// migrate identity between bodies.
//
// The inactive primitives' remembered parameters are part of it because they
// are part of what the user sees: a Box -> Sphere -> Box round trip must come
// back to the box the user typed, and a restore that wrote only the active
// primitive would silently forget the other five.
// The SHAPE half of a body's Construction state, and only that half.
//
// Placement is deliberately NOT here. It was, while every body was a
// Construction Body and the two could not exist apart; `IMPORT-01A` makes a
// body's representation a choice, and an Imported Mesh has a placement with no
// Construction Source to hang it on. So the transform moved up to
// `SceneObject`, which every body has, and this struct describes the primitive
// and nothing else — the same split the `.forge` document already made, where
// `SCNE` carries placement for both project kinds and `CONS` carries shape.
struct ConstructionObjectState {
    PrimitiveKind kind = PrimitiveKind::Box;
    BoxDimensionsMeters box{};
    CylinderDimensionsMeters cylinder{};
    SphereDimensionsMeters sphere{};
    ConeDimensionsMeters cone{};
    CapsuleDimensionsMeters capsule{};
    PlaneDimensionsMeters plane{};
};

// True when the two states describe the same SHAPE — the active kind and every
// remembered parameter — regardless of where the object sits.
//
// Kept separate from whole-state equality because the difference decides
// whether a restore has to publish a mesh revision at all: a placement change
// costs a derived matrix and nothing else, and republishing for one would be
// exactly the duplicate geometry work a transaction boundary exists to remove.
bool sameConstructionShape(const ConstructionObjectState& a, const ConstructionObjectState& b);

// True when the two placements are identical, value for value.
//
// Takes the placement itself rather than a Construction state, because a body's
// placement is no longer part of one: an Imported Mesh has a placement and no
// Construction Source.
bool sameConstructionPlacement(const TransformValues& a, const TransformValues& b);

// THE one active Construction object.
//
// It owns identity, which primitive is active, both primitives' parameters, and
// the placement. This is deliberately ONE object, not a registry: there is no
// container, no list, no parent, no create and no delete.
//
// Every primitive's parameters are retained across a kind change, so switching
// Box -> Cylinder -> Box does not silently forget the box's dimensions. Only
// `kind` decides which set is authoritative for the geometry right now.
class ConstructionObject {
public:
    explicit ConstructionObject(ObjectId objectId = kConstructionBoxObjectId)
        : objectId_(objectId),
          box_(objectId),
          cylinder_(objectId),
          sphere_(objectId),
          cone_(objectId),
          capsule_(objectId),
          plane_(objectId) {}

    // Stable across dimension edits, transform edits, mesh revisions, GPU
    // reallocation AND primitive changes. Changing a box into a cylinder does
    // not create a new object; it changes what this object is.
    ObjectId objectId() const { return objectId_; }

    PrimitiveKind kind() const { return kind_; }

    const ConstructionBox& box() const { return box_; }
    const ConstructionCylinder& cylinder() const { return cylinder_; }
    const ConstructionSphere& sphere() const { return sphere_; }
    const ConstructionCone& cone() const { return cone_; }
    const ConstructionCapsule& capsule() const { return capsule_; }
    const ConstructionPlane& plane() const { return plane_; }

    // Placement is NOT here. A body's `ConstructionTransform` lives on
    // `SceneObject`, because `IMPORT-01A` gave a body a choice of
    // representation and an Imported Mesh has a placement with no Construction
    // Source to hang it on. There is still exactly ONE transform per body and
    // it is still never touched by a primitive change; it simply belongs to the
    // body rather than to one of the things a body can be.

    // The ACTIVE primitive's parameters, as a complete typed spec. The inactive
    // primitives' remembered parameters are deliberately NOT part of it: they
    // are reachable through box()/cylinder()/sphere() when something genuinely
    // wants a remembered value, and cannot be mistaken for the active ones.
    PrimitiveSpec spec() const;

    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Requests a complete new primitive state.
    //
    // Only the parameters carried by `requested` are validated and written; the
    // inactive primitives keep whatever they had.
    //
    // FAILS CLOSED: if any relevant value is invalid, nothing at all is written
    // — not the kind, not the parameters — and the previous state stands.
    //
    // Unchanged means the kind matches AND every relevant parameter matches. A
    // kind change is always a change, even when the target primitive already
    // held exactly those parameters.
    //
    // The transform is never read, written or reset by this call.
    PrimitiveUpdateStatus setPrimitive(const PrimitiveSpec& requested,
                                       DimensionValidation* outWhy = nullptr);

    // Generates the ACTIVE primitive's local-space mesh.
    ConstructionMesh generateMesh() const;

    // ---------------------------------------------------------------------
    // History support
    // ---------------------------------------------------------------------
    //
    // The pair the Construction history uses to take a bounded snapshot of this
    // object and to put it back. Not a product edit path: no UI reaches either,
    // and `restoreState` deliberately does not go through `setPrimitive`,
    // because it is not requesting a change — it is returning the object to a
    // state that was authoritative, and therefore already validated, when it
    // was captured.
    ConstructionObjectState captureState() const;

    // Writes a previously captured state back verbatim.
    //
    // `updateCount()` is NOT advanced: an undo returns the object to a state it
    // has already counted, and counting it again would make the diagnostic say
    // the user made an edit they did not. The transform is restored alongside
    // the shape, which is the one place in this class where the two move
    // together — because a history step is a moment in time, not an edit.
    void restoreState(const ConstructionObjectState& state);

private:
    // Typed dispatch for setPrimitive's update half, one overload per
    // primitive kind, resolved by std::visit over the requested payload rather
    // than an if-else chain over typed accessors. A new PrimitiveSpec
    // alternative with no matching overload here fails to COMPILE — the
    // property an if-else chain cannot offer, and the reason this pair of
    // overload sets is the whole of the per-kind dispatch this class does.
    bool parametersDiffer(const BoxDimensionsMeters& box) const;
    bool parametersDiffer(const CylinderDimensionsMeters& cylinder) const;
    bool parametersDiffer(const SphereDimensionsMeters& sphere) const;
    bool parametersDiffer(const ConeDimensionsMeters& cone) const;
    bool parametersDiffer(const CapsuleDimensionsMeters& capsule) const;
    bool parametersDiffer(const PlaneDimensionsMeters& plane) const;

    void writeParameters(const BoxDimensionsMeters& box);
    void writeParameters(const CylinderDimensionsMeters& cylinder);
    void writeParameters(const SphereDimensionsMeters& sphere);
    void writeParameters(const ConeDimensionsMeters& cone);
    void writeParameters(const CapsuleDimensionsMeters& capsule);
    void writeParameters(const PlaneDimensionsMeters& plane);

    const ObjectId objectId_;
    PrimitiveKind kind_ = PrimitiveKind::Box;
    ConstructionBox box_;
    ConstructionCylinder cylinder_;
    ConstructionSphere sphere_;
    ConstructionCone cone_;
    ConstructionCapsule capsule_;
    ConstructionPlane plane_;

    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// Generates a mesh and publishes it as a new MeshStore revision.
//
// Returns kNoMeshRevision (leaving the store's current revision untouched) if
// the generated data does not pass RuntimeMesh validation. Contains no Vulkan
// work: publication is a CPU act, and the render thread does the upload.
MeshRevision publishConstructionObject(const ConstructionObject& object, MeshStore& store,
                                       MeshValidation* outWhy = nullptr);

// Box-only convenience, retained because the self-tests build standalone boxes.
MeshRevision publishConstructionBox(const ConstructionBox& box, MeshStore& store,
                                    MeshValidation* outWhy = nullptr);

// Everything a caller needs to know about one primitive apply, so that no caller
// has to re-derive it — or re-implement the "publish only when it changed" rule.
struct PrimitiveApplyResult {
    PrimitiveUpdateStatus status = PrimitiveUpdateStatus::Rejected;
    // Why the request was refused. Meaningful only when status == Rejected.
    DimensionValidation validation = DimensionValidation::Ok;
    // Why publication failed, in the should-not-happen case where validated
    // parameters still produced mesh data the store refused.
    MeshValidation meshValidation = MeshValidation::Ok;
    // The store's revision after the call: the NEW one when a revision was
    // published, and the untouched current one otherwise.
    MeshRevision revision = kNoMeshRevision;
    // The authoritative primitive state after the call. On Rejected this is the
    // previous valid state, unchanged.
    PrimitiveSpec spec{};
    // Published vertex/index counts, so a caller can log the topology without
    // reaching into the store.
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    // True only when a new mesh revision was actually published by this call.
    bool published = false;
};

// THE Construction shape entry point.
//
// One call performs the whole product act: validate the requested primitive's
// parameters, update kind and parameters atomically, and publish exactly one new
// mesh revision if — and only if — something changed. Callers (JNI, the debug
// driver) decide nothing and duplicate no rule.
//
//   Applied   -> kind and/or parameters changed, exactly one new revision
//   Unchanged -> valid but identical; nothing written, nothing published
//   Rejected  -> previous kind, parameters, transform and revision all untouched
//
// The transform is never modified by this call, in any outcome.
PrimitiveApplyResult applyPrimitive(ConstructionObject& object, MeshStore& store,
                                    const PrimitiveSpec& requested);

// The ACTIVE body's Construction Source, or nullptr when the active body is an
// Imported Mesh.
//
// Process-scoped, like the camera and the mesh store: it outlives every Surface,
// so the active body's primitive and parameters survive home/resume and
// swapchain recreation.
//
// Deliberately a POINTER, and deliberately renamed by `IMPORT-01A` from
// `constructionObject()`. Until then every body had a Construction Source and no
// caller could be wrong; now one representation has none, and a nullable return
// with a name that says so makes the compiler ask every call site what it does
// about that instead of leaving a silent assumption behind. Placement is NOT
// here — see `constructionTransform()`, which is the body's and is total.
ConstructionObject* activeConstructionOrNull();

// The same entry point against the process-scoped object and mesh store.
PrimitiveApplyResult applyConstructionPrimitive(const PrimitiveSpec& requested);

// The per-kind product entry points the Android UI drives through JNI, one per
// primitive.
//
// They exist so that no caller above this line ever assembles a primitive out of
// positional numbers whose meaning depends on a separate kind argument: asking
// for a sphere means calling the function that takes a sphere's one parameter,
// and a capsule's TOTAL height physically cannot arrive in the slot a cone's
// height belongs in. All are thin — each builds the matching typed spec and
// forwards to
// applyConstructionPrimitive, so the update-then-publish rule still has exactly
// one implementation.
PrimitiveApplyResult applyConstructionBox(Meters width, Meters height, Meters depth);
PrimitiveApplyResult applyConstructionCylinder(Meters diameter, Meters height);
PrimitiveApplyResult applyConstructionSphere(Meters diameter);
PrimitiveApplyResult applyConstructionCone(Meters bottomDiameter, Meters height);
PrimitiveApplyResult applyConstructionCapsule(Meters diameter, Meters totalHeight);
PrimitiveApplyResult applyConstructionPlane(Meters width, Meters depth);

}  // namespace forgeshape
