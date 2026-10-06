// Surface: a body whose truth is an ORDERED list of typed surface features
// over retained sketches, and whose geometry -- open patches, their boundary
// edges, the shells a Stitch joins and the solids a Thicken makes -- is derived
// from that list on every regeneration (`MODELING-FOUNDATIONS-R1` C).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no camera.
//
// What a Surface body is
// ----------------------
// A Surface body may be one open patch, several stitched patches or a closed
// shell, and it is NOT automatically a solid: only a Thicken makes material,
// and only where the offset is mathematically defined (a planar patch; a ruled
// surface over straight lines, a circle or one arc). Every other Thicken is
// refused by name (`ThickenUnsupportedForSurfaceType`) rather than faked by
// pushing triangles along their normals.
//
// Identity
// --------
// A feature's id is durable (`SurfaceFeatureId`, strictly ascending, minted
// from a stored high-water mark, never reused along a forward history branch).
// A patch's id (`SurfacePatchId`: the feature that made it and its ordinal
// there) and a boundary edge's id (`SurfaceEdgeId`: the patch and its ordinal)
// are DERIVED identities a regeneration produces deterministically -- what a
// tap resolves to and a Stitch reports -- and never stored geometry. A render
// triangle index is never an identity.
//
// Regeneration
// ------------
// Features are evaluated IN ORDER, each against the patches the ones before it
// left, and evaluation stops at the FIRST feature that fails: the report names
// it by id and status, the same staged first-failure model the Parametric CAD
// history uses. Nothing is published unless the whole list regenerates.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

// Every refusal is named; a refusal changes nothing. APPENDED only: a code
// crosses JNI.
enum class SurfaceStatus : uint8_t {
    Ok,
    NonFinite,
    OutOfRange,
    TooManySketches,
    TooManyFeatures,
    IdInvalid,
    HighWaterInvalid,
    NoFeatures,
    // The first feature must make a surface (Patch, Extrude, Revolve, Loft).
    FirstFeatureInvalid,
    UnknownSketch,
    SketchInvalid,
    // A feature carries a value its kind does not use, or lacks one it needs.
    PayloadMismatch,
    UnknownFeature,
    // A feature names a feature that is not EARLIER in the list.
    FeatureOrderInvalid,
    // A feature names a feature whose patches a later step already consumed.
    FeatureConsumed,
    // Patch: no region, or a selection the sketch does not derive.
    RegionInvalid,
    // A curve section: an entity id the sketch does not have, a curve that
    // cannot take part, an empty section.
    CurveNotFound,
    CurveSectionEmpty,
    // Three or more curves meet at one point: no single chain.
    CurveChainForked,
    // Extrude: zero, negative, non-finite or out-of-range.
    DistanceInvalid,
    // Revolve
    AxisUnresolved,
    AxisNotStraight,
    AngleInvalid,
    ProfileCrossesAxis,
    // Loft
    LoftSectionCount,      // a section is not exactly ONE chain
    LoftSectionMismatch,   // open to closed
    LoftCorrespondenceInvalid,
    LoftSectionsCoincide,
    // Trim
    TrimUnsupportedTarget,  // not a planar patch: refused, never emulated
    TrimNotCoplanar,
    TrimRegionInvalid,
    TrimRemovesPatch,
    // Stitch
    StitchNoCompatibleEdges,
    StitchGapTooLarge,
    StitchNonManifold,
    StitchIncompatibleBoundary,
    // Thicken
    ThickenUnsupportedForSurfaceType,
    ThickenInvalidThickness,
    ThickenInvalidSolid,
    // The derived geometry is beyond the bounds a phone can carry.
    TooManyPatches,
    TooManyTriangles,
    TessellationFailed,
    NotSurfaceBody,
    EditInProgress,
    // Authoring (forgeshape_surface_authoring.h), appended so every earlier
    // code keeps its number.
    NotSketching,       // a Surface Finish with no Surface sketch open
    SectionMissing,     // a Loft with no pending first section
    TrimTargetMissing,  // a Trim with no live planar patch on the sketch's plane
    NothingToStitch,    // a Stitch over fewer than two live features
};

constexpr int kSurfaceStatusCount = 48;

const char* surfaceStatusName(SurfaceStatus status);
int surfaceStatusCode(SurfaceStatus status);

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------

constexpr uint32_t kMaxSurfaceFeatures = 32;
constexpr uint32_t kMaxSurfaceSketches = 32;
constexpr uint32_t kMaxSurfaceSectionCurves = 64;
constexpr uint32_t kMaxSurfaceStitchFeatures = kMaxSurfaceFeatures;
// Derived: what one regeneration may produce.
constexpr uint32_t kMaxSurfacePatches = 256;
constexpr uint32_t kMaxSurfaceTriangles = 262144;
// A loft samples both sections to one count.
constexpr uint32_t kMaxSurfaceLoftSamples = 256;
// A revolved surface steps at the circle's own density.
constexpr uint32_t kSurfaceRevolveFullTurnSteps = 32;
constexpr double kMinSurfaceDistanceMeters = 1.0e-6;
constexpr double kMaxSurfaceDistanceMeters = 1.0e5;
constexpr double kMinSurfaceAngleDegrees = 0.001;
constexpr double kMaxSurfaceAngleDegrees = 360.0;
// Two boundary edges stitch when they coincide within this, in metres.
constexpr double kSurfaceStitchToleranceMeters = 1.0e-6;
// Within this but beyond the tolerance, a pair is a GAP, refused by name and
// never averaged shut.
constexpr double kSurfaceStitchSearchMeters = 1.0e-3;

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

enum class SurfaceFeatureId : uint32_t {};
constexpr SurfaceFeatureId kNoSurfaceFeature{0};
inline uint32_t idOf(SurfaceFeatureId id) { return static_cast<uint32_t>(id); }

// The feature that made a patch and its ordinal among that feature's patches.
struct SurfacePatchId {
    SurfaceFeatureId feature = kNoSurfaceFeature;
    uint32_t ordinal = 0;
};

// A patch and the ordinal of one of its boundary edges (see `SurfacePatch`).
struct SurfaceEdgeId {
    SurfacePatchId patch;
    uint32_t ordinal = 0;
};

bool sameSurfacePatchId(const SurfacePatchId& a, const SurfacePatchId& b);
bool sameSurfaceEdgeId(const SurfaceEdgeId& a, const SurfaceEdgeId& b);

// ---------------------------------------------------------------------------
// The durable state
// ---------------------------------------------------------------------------

// A retained sketch on a principal workplane, offset along its normal: what a
// Loft's second section and a parallel Trim stand on. Face support is not this
// stage: a Surface sketch never names a TopoRef.
struct SurfaceSketchRecord {
    uint32_t id = 0;
    double offset = 0.0;  // metres along the workplane normal
    CadSketch sketch;
};

enum class SurfaceFeatureKind : uint8_t {
    PlanarPatch = 1,
    ExtrudedSurface = 2,
    RevolvedSurface = 3,
    LoftSurface = 4,
    TrimSurface = 5,
    Stitch = 6,
    Thicken = 7,
};

const char* surfaceFeatureKindName(SurfaceFeatureKind kind);
bool surfaceFeatureKindFromCode(int code, SurfaceFeatureKind* out);

// Curves of one sketch, by entity id (ascending, unique). Joined into chains
// by coincident AUTHORED endpoints, walked from the smallest id.
struct SurfaceSection {
    uint32_t sketchId = 0;
    std::vector<SketchEntityId> curves;
};

// One typed feature. Exactly the fields its kind uses are set; every other one
// keeps its default (a canonical form, so one feature has one encoding).
struct SurfaceFeature {
    SurfaceFeatureId id = kNoSurfaceFeature;
    SurfaceFeatureKind kind = SurfaceFeatureKind::PlanarPatch;
    // Patch, Trim: the sketch; Extrude, Revolve, Loft (section A): the curves.
    SurfaceSection section;
    // Patch: the regions it fills (holes preserved). Trim: the trim region.
    std::vector<ProfileRegionRef> regions;
    // Extrude
    double distance = 0.0;
    ExtrudeDirection direction = ExtrudeDirection::AlongNormal;
    // Revolve: a straight sketch edge of the section's sketch, never a curve
    // of the section.
    CadSketchEdgeRef axis{};
    double angleDegrees = 0.0;
    RevolveDirection revolveDirection = RevolveDirection::Positive;
    // Loft: the second section and how its samples correspond to the first's.
    SurfaceSection sectionB;
    bool reverseB = false;
    uint32_t startOffsetB = 0;
    // Trim: the planar patch feature it clips, and which side stays.
    SurfaceFeatureId target = kNoSurfaceFeature;
    bool keepInside = true;
    // Stitch: the features whose patches join (ascending).
    std::vector<SurfaceFeatureId> stitchFeatures;
    // Thicken: the feature whose patches become a solid; signed, metres,
    // positive along the patch normal.
    SurfaceFeatureId source = kNoSurfaceFeature;
    double thickness = 0.0;
};

struct SurfaceBodyState {
    std::vector<SurfaceSketchRecord> sketches;  // strictly ascending by id
    std::vector<SurfaceFeature> features;       // strictly ascending by id, in order
    uint32_t nextSketchId = 1;
    uint32_t nextFeatureId = 1;
};

bool sameSurfaceSection(const SurfaceSection& a, const SurfaceSection& b);
bool sameSurfaceFeature(const SurfaceFeature& a, const SurfaceFeature& b);
bool sameSurfaceBodyState(const SurfaceBodyState& a, const SurfaceBodyState& b);

const SurfaceSketchRecord* findSurfaceSketch(const SurfaceBodyState& state, uint32_t id);
const SurfaceFeature* findSurfaceFeature(const SurfaceBodyState& state, SurfaceFeatureId id);

// The frame a surface sketch stands on: its workplane, offset along the normal.
CadFrame64 surfaceSketchFrame(const SurfaceSketchRecord& record);

// ---------------------------------------------------------------------------
// The derived geometry
// ---------------------------------------------------------------------------

// One boundary edge of a patch: a polyline in body-local binary64, closed or
// open. Its ordinal is its place in the patch's own canonical list.
struct SurfaceBoundaryEdge {
    std::vector<DVec3> points;
    bool closed = false;
};

enum class SurfacePatchShape : uint8_t {
    Planar,
    Ruled,      // extruded: a chain swept along the sketch normal
    Revolved,
    Lofted,
};

struct SurfacePatch {
    SurfacePatchId id;
    SurfacePatchShape shape = SurfacePatchShape::Planar;
    std::vector<DVec3> positions;
    std::vector<uint32_t> triangles;  // 3 per triangle
    std::vector<SurfaceBoundaryEdge> edges;
    // Planar: the frame and the loops in (u, v), outer first, holes after,
    // each counter-clockwise -- what a Trim clips and a Thicken extrudes.
    CadFrame64 frame;
    std::vector<std::vector<SketchPoint>> loops;
    // Ruled: the chain in (u, v) on `frame`, whether it closes, whether every
    // piece of it is straight, the curve kind it came from, and the signed
    // extent along the frame normal -- what a Thicken offsets.
    std::vector<SketchPoint> chain;
    bool chainClosed = false;
    bool chainStraight = false;
    // The single circle or arc the chain is, when it is exactly one (else 0).
    SketchEntityKind chainCurveKind = SketchEntityKind::Line;
    bool chainSingleCurve = false;
    SketchEntity chainEntity;
    double extentNear = 0.0;
    double extentFar = 0.0;
};

// A pair of boundary edges a Stitch joined.
struct SurfaceStitchPair {
    SurfaceEdgeId a;
    SurfaceEdgeId b;
};

struct SurfaceBodyMesh {
    // The patches still standing after every feature (a Trim replaces its
    // target's, a Thicken consumes its source's), in feature order.
    std::vector<SurfacePatch> patches;
    std::vector<SurfaceStitchPair> stitches;
    // The material Thickens made, each validated by the kernel.
    CadSolid solid;
    // The render mesh: every patch, then the solid. Per triangle, the index
    // into `patches` it came from, or -1 for the solid.
    ConstructionMesh render;
    std::vector<int32_t> trianglePatch;
    // Whether some boundary edge is still open (not stitched): the surface is
    // then drawn from both sides.
    bool open = false;
    uint32_t openEdgeCount = 0;
};

struct SurfaceRegenerationReport {
    SurfaceStatus status = SurfaceStatus::Ok;
    SurfaceFeatureId failedFeature = kNoSurfaceFeature;
};

// Validates and regenerates the whole list. Writes `out` only when every
// feature regenerates; `report` names the first that did not.
SurfaceStatus regenerateSurfaceBody(const SurfaceBodyState& state, SurfaceBodyMesh* out,
                                    SurfaceRegenerationReport* report = nullptr);

// The structural rules alone (ids, marks, sketches, payload canonical form,
// references) -- what the codec holds a file to before regeneration.
SurfaceStatus validateSurfaceBodyState(const SurfaceBodyState& state,
                                       SurfaceFeatureId* outFailed = nullptr);

// FNV-1a 64 over the render mesh's positions and indices: equal for equal
// surfaces, so determinism is a value.
uint64_t surfaceMeshDigest(const SurfaceBodyMesh& mesh);

// The edges of one patch's boundary that are not stitched, as edge ids.
std::vector<SurfaceEdgeId> surfaceOpenEdges(const SurfaceBodyMesh& mesh);

// ---------------------------------------------------------------------------
// Authoring helpers (pure)
// ---------------------------------------------------------------------------

// Appends a sketch, minting its id. Returns the id, or 0 when the table is full
// or the sketch is invalid.
uint32_t appendSurfaceSketch(SurfaceBodyState* state, const CadSketch& sketch, double offset);

// Appends a feature, minting its id (the feature's own id is ignored). Returns
// the id, or kNoSurfaceFeature when the list is full.
SurfaceFeatureId appendSurfaceFeature(SurfaceBodyState* state, SurfaceFeature feature);

// Every region of a sketch that a Patch fills by default: the regions at even
// nesting depth, so a loop inside another is a hole.
std::vector<ProfileRegionRef> surfaceDefaultPatchRegions(const CadSketch& sketch);

// Every regular entity of a sketch except `axis`'s, ascending: what an Extrude
// or a Revolve sweeps by default.
std::vector<SketchEntityId> surfaceDefaultCurves(const CadSketch& sketch,
                                                 SketchEntityId except = kNoSketchEntity);

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

// THE Surface body. Owns its state and a runtime cache of the regeneration.
class SurfaceBody {
public:
    SurfaceBody(ObjectId objectId, SurfaceBodyState state, SurfaceBodyMesh mesh)
        : objectId_(objectId), state_(std::move(state)),
          mesh_(std::make_shared<const SurfaceBodyMesh>(std::move(mesh))) {}

    ObjectId objectId() const { return objectId_; }
    const SurfaceBodyState& state() const { return state_; }
    const SurfaceBodyMesh& mesh() const { return *mesh_; }

    // Replaces the state. FAILS CLOSED: regenerated whole, refused when a mark
    // would go down; an identical state reports Ok with `outChanged` false.
    SurfaceStatus applyState(const SurfaceBodyState& next, bool* outChanged = nullptr,
                             SurfaceRegenerationReport* report = nullptr);

    // History restore: a state that regenerated when it was captured.
    void restoreState(const SurfaceBodyState& state);

private:
    const ObjectId objectId_;
    SurfaceBodyState state_;
    std::shared_ptr<const SurfaceBodyMesh> mesh_;
};

// The neutral colour every Surface vertex carries; presentation only.
constexpr float kSurfaceVertexColor[3] = {0.66f, 0.72f, 0.76f};

}  // namespace forgeshape
