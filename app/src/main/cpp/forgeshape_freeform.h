// Freeform/SubD: a body whose truth is a quad CONTROL CAGE, and whose smooth
// surface is derived from it by Catmull-Clark subdivision
// (`MODELING-FOUNDATIONS-R1` B).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no camera.
//
// What this is, and what it is not
// --------------------------------
// The cage is a manifold, orientable, all-quad polygon mesh with stable ids for
// every vertex, edge and face. Every edit -- a move, a push, an extrusion, an
// inserted loop, a crease weight, a deleted face -- changes THE CAGE, and the
// smooth surface is regenerated from it (forgeshape_freeform_subdivision.h).
// The derived mesh is never stored, compared, serialized or read back.
//
// It is NOT a T-Spline: there are no T-junctions and no local refinement. An
// inserted loop always runs the whole quad ring, and the surface is plain
// Catmull-Clark. The product calls it Freeform.
//
// It is not Sculpt either: a Frozen Sculpt Mesh is a fixed triangle mesh whose
// vertex positions a brush displaces; nothing here is a sculpt vertex and
// `SculptHistory` never sees a cage edit. A cage edit is one Construction
// history step.
//
// Identity
// --------
// Ids are strong types (`enum class` over `uint32_t`, so an edge id cannot be
// passed where a vertex id is expected), non-zero, strictly ascending within
// their table, minted from stored high-water marks and never reused along a
// forward history branch -- the rule `CadSketchId` states, for the same reason.
// No vector index is ever an identity: a face names its four vertices by id,
// and the edge between two loop neighbours is found by its record.
//
// Symmetry
// --------
// Up to three planes through the body origin (x = 0, y = 0, z = 0). With
// symmetry on, the cage is EXACTLY symmetric: every vertex's reflection is a
// vertex of the cage bit for bit, a vertex on a plane has that coordinate
// exactly 0, and the reflected face set is the face set. Every tool re-derives
// that after it runs (`freeformSnapSymmetry`), so no tolerance drift
// accumulates and no duplicate centre vertex is ever created.
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_object_id.h"

namespace forgeshape {

// ---------------------------------------------------------------------------
// The status vocabulary
// ---------------------------------------------------------------------------

// Every refusal is named; a refusal changes nothing. APPENDED only: a code
// crosses JNI.
enum class FreeformStatus : uint8_t {
    Ok,
    NonFinite,
    OutOfRange,
    TooManyVertices,
    TooManyEdges,
    TooManyFaces,
    // An id that is zero, not strictly ascending, or not below its mark.
    IdInvalid,
    // A high-water mark that is not above every id, or an edit lowering one.
    HighWaterInvalid,
    // A face or edge naming a vertex the cage does not carry.
    UnknownVertex,
    // An edge or face id the cage does not carry (an edit's target).
    UnknownEdge,
    UnknownFace,
    // A face that is not four distinct vertices.
    FaceNotQuad,
    // A face whose loop is not rotated to its smallest vertex id.
    FaceNotCanonical,
    // Two corners of a face at the same position.
    FaceDegenerate,
    // An edge record whose v0 is not below its v1.
    EdgeNotCanonical,
    // Two edge records over one vertex pair, or two faces over one vertex set.
    DuplicateEdge,
    DuplicateFace,
    // A face side with no edge record.
    MissingEdge,
    // An edge record no face uses.
    EdgeUnused,
    // An edge used by more than two faces.
    NonManifoldEdge,
    // Two faces traversing a shared edge in the same direction.
    InconsistentWinding,
    // A vertex no face uses.
    IsolatedVertex,
    // A vertex whose faces form more than one fan (a bow tie).
    BowTieVertex,
    // A crease weight outside [0, 1].
    InvalidCrease,
    // A subdivision level outside 0..4.
    InvalidSubdivisionLevel,
    // The derived surface at the stored level would exceed its quad budget.
    SubdivisionBudgetExceeded,
    // A symmetry flag outside X | Y | Z.
    InvalidSymmetry,
    // Symmetry requested over a cage that is not symmetric about that plane.
    CageNotSymmetric,
    // A stored or produced cage that breaks the symmetry it declares.
    SymmetryBroken,
    // A loop on a ring its own mirror image crosses, at a ratio other than 0.5.
    SymmetryRequiresMidpoint,
    // A tool with nothing selected.
    EmptySelection,
    // An edge ring that re-enters a face it already crossed.
    EdgeLoopSelfCrossing,
    // A split ratio outside (0, 1).
    InvalidRatio,
    // An extrusion region whose boundary touches itself at a vertex.
    ExtrudeRegionPinched,
    // A distance that is zero, non-finite or out of range.
    InvalidDistance,
    // Deleting every face.
    DeleteWouldEmpty,
    // Deleting faces that would leave a non-manifold or bow-tie cage.
    DeleteWouldBreakManifold,
    // A cage with no faces.
    EmptyCage,
    // A transform that collapses the selection (a zero or negative scale).
    TransformDegenerate,
    // A body that is not a Freeform body.
    NotFreeformBody,
    // The act is refused while no Freeform edit is open, or in a state that
    // cannot take it.
    NotEditing,
    // A Construction edit is already open.
    EditInProgress,
    // The body is locked (placement and form are both held).
    BodyLocked,
    // The body is hidden: what is not drawn cannot be edited by touch.
    BodyHidden,
};

constexpr int kFreeformStatusCount = 44;

const char* freeformStatusName(FreeformStatus status);
int freeformStatusCode(FreeformStatus status);

// ---------------------------------------------------------------------------
// Ids
// ---------------------------------------------------------------------------

enum class FreeformVertexId : uint32_t {};
enum class FreeformEdgeId : uint32_t {};
enum class FreeformFaceId : uint32_t {};

constexpr FreeformVertexId kNoFreeformVertex{0};
constexpr FreeformEdgeId kNoFreeformEdge{0};
constexpr FreeformFaceId kNoFreeformFace{0};

inline uint32_t idOf(FreeformVertexId id) { return static_cast<uint32_t>(id); }
inline uint32_t idOf(FreeformEdgeId id) { return static_cast<uint32_t>(id); }
inline uint32_t idOf(FreeformFaceId id) { return static_cast<uint32_t>(id); }

// ---------------------------------------------------------------------------
// Bounds
// ---------------------------------------------------------------------------

constexpr uint32_t kMaxFreeformVertices = 4096;
constexpr uint32_t kMaxFreeformEdges = 8192;
constexpr uint32_t kMaxFreeformFaces = 4096;
constexpr uint8_t kMaxFreeformSubdivisionLevel = 4;
// The derived quads at the stored level: faces * 4^level. A 512-face cage at
// level 4. Bounded so a regeneration, an upload and a pick stay interactive on
// a phone; a cage over it at its level is refused by name, never silently
// shown at a lower level.
constexpr uint32_t kMaxFreeformDerivedQuads = 131072;
constexpr double kMaxFreeformCoordinateMeters = 1.0e5;
// Two corners closer than this are one point.
constexpr double kFreeformCoincidenceMeters = 1.0e-9;
// An edit distance (push, extrude) at or below this is no edit.
constexpr double kMinFreeformDistanceMeters = 1.0e-6;

// Symmetry planes, as bits.
constexpr uint8_t kFreeformSymmetryX = 0x01u;  // the plane x = 0
constexpr uint8_t kFreeformSymmetryY = 0x02u;  // the plane y = 0
constexpr uint8_t kFreeformSymmetryZ = 0x04u;  // the plane z = 0
constexpr uint8_t kFreeformSymmetryMask = 0x07u;

// ---------------------------------------------------------------------------
// The durable cage
// ---------------------------------------------------------------------------

struct FreeformVertex {
    FreeformVertexId id = kNoFreeformVertex;
    DVec3 position{0.0, 0.0, 0.0};
};

struct FreeformEdge {
    FreeformEdgeId id = kNoFreeformEdge;
    // Canonical: v0 < v1 by id. The direction is not an orientation; a face
    // traverses an edge one way or the other.
    FreeformVertexId v0 = kNoFreeformVertex;
    FreeformVertexId v1 = kNoFreeformVertex;
    // 0 smooth .. 1 sharp, continuous (see forgeshape_freeform_subdivision.h).
    double crease = 0.0;
};

struct FreeformFace {
    FreeformFaceId id = kNoFreeformFace;
    // Counter-clockwise seen from OUTSIDE, rotated so loop[0] is the smallest
    // vertex id: one face, one encoding.
    std::array<FreeformVertexId, 4> loop{};
};

// The whole authored truth of one Freeform body. Plain, copyable, comparable,
// bounded, carrying nothing derived.
struct FreeformCage {
    std::vector<FreeformVertex> vertices;  // strictly ascending by id
    std::vector<FreeformEdge> edges;       // strictly ascending by id
    std::vector<FreeformFace> faces;       // strictly ascending by id
    uint32_t nextVertexId = 1;
    uint32_t nextEdgeId = 1;
    uint32_t nextFaceId = 1;
    uint8_t subdivisionLevel = 2;
    uint8_t symmetry = 0;
};

bool sameFreeformCage(const FreeformCage& a, const FreeformCage& b);

const FreeformVertex* findFreeformVertex(const FreeformCage& cage, FreeformVertexId id);
const FreeformEdge* findFreeformEdge(const FreeformCage& cage, FreeformEdgeId id);
const FreeformFace* findFreeformFace(const FreeformCage& cage, FreeformFaceId id);

// The edge record over a vertex pair (either order), or null.
const FreeformEdge* findFreeformEdgeBetween(const FreeformCage& cage, FreeformVertexId a,
                                            FreeformVertexId b);

// Whether any edge bounds exactly one face: an OPEN cage (a plane, a cage with
// a deleted face). Such a surface is drawn from both sides.
bool freeformCageHasBoundary(const FreeformCage& cage);

// ---------------------------------------------------------------------------
// Derived topology (dense indices into the cage's own tables)
// ---------------------------------------------------------------------------

struct FreeformTopology {
    // Per face: its four vertex indices, and the edge index of side k
    // (loop[k] -> loop[k + 1]).
    std::vector<uint32_t> faceVertices;
    std::vector<uint32_t> faceEdges;
    // Per edge: up to two face indices (-1 when absent) and how many.
    std::vector<int32_t> edgeFaces;
    std::vector<uint8_t> edgeFaceCount;
    // Per vertex: incident edge indices and face indices, ascending.
    std::vector<std::vector<uint32_t>> vertexEdges;
    std::vector<std::vector<uint32_t>> vertexFaces;
};

// Builds the topology of a STRUCTURALLY valid cage (ids, quads, edge records),
// refusing by name what makes one impossible. `validateFreeformCage` is the
// whole rule; this is its structural half.
FreeformStatus buildFreeformTopology(const FreeformCage& cage, FreeformTopology* out);

// THE rule for a cage this build can carry: bounds, ids and marks, finite
// in-range positions, canonical quads, one edge record per face side, a
// manifold orientable surface with boundary allowed, no isolated or bow-tie
// vertex, non-degenerate faces, level and quad budget, and exact symmetry when
// declared.
FreeformStatus validateFreeformCage(const FreeformCage& cage);

// ---------------------------------------------------------------------------
// Creation (deterministic, exactly symmetric about all three planes)
// ---------------------------------------------------------------------------

// A 1 m cube: 8 vertices, 12 edges, 6 faces.
FreeformCage makeFreeformBox();
// A 2 m square in XZ facing +Y, 4 x 4 quads: 25 vertices, 40 edges, 16 faces.
FreeformCage makeFreeformPlane();
// Radius 0.5 m, height 1 m about Y: 8 quads around, each end closed by four
// quads meeting at a centre vertex. 18 vertices, 32 edges, 16 faces.
FreeformCage makeFreeformCylinder();

// Mints a cage from positions and outward quads (indices into `positions`):
// vertices 1..n in order, edges in first-use order over the faces, faces in
// order, each loop rotated canonical. The one assembler every creation form
// uses, so ids are a pure function of the authored shape.
FreeformCage assembleFreeformCage(const std::vector<DVec3>& positions,
                                  const std::vector<std::array<uint32_t, 4>>& quads,
                                  uint8_t subdivisionLevel);

// ---------------------------------------------------------------------------
// Selections
// ---------------------------------------------------------------------------

enum class FreeformElement : uint8_t {
    Vertex,
    Edge,
    Face,
};

const char* freeformElementName(FreeformElement element);

// The vertices a selection moves: the vertices themselves, an edge's two, a
// face's four. Ascending, unique; unknown ids are refused by name.
FreeformStatus freeformSelectionVertices(const FreeformCage& cage, FreeformElement element,
                                         const std::vector<uint32_t>& ids,
                                         std::vector<FreeformVertexId>* out);

// The selection's centroid in body-local space: the mean of its vertices.
bool freeformSelectionCentroid(const FreeformCage& cage, FreeformElement element,
                               const std::vector<uint32_t>& ids, DVec3* out);

// A face's outward normal (Newell), unit; false when degenerate.
bool freeformFaceNormal(const FreeformCage& cage, const FreeformFace& face, DVec3* out);

// ---------------------------------------------------------------------------
// Tools: each a pure function from one cage to another, or a named refusal
// ---------------------------------------------------------------------------

// An affine edit of the selected vertices about `pivot`: p' = pivot + L (p -
// pivot) + translation, with L a 3x3 in row-major order. With symmetry on, the
// moved vertices are the authority and their mirror partners follow exactly;
// a vertex on a symmetry plane stays on it.
struct FreeformAffine {
    double linear[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    DVec3 translation{0.0, 0.0, 0.0};
    DVec3 pivot{0.0, 0.0, 0.0};
};

FreeformStatus freeformTransform(const FreeformCage& cage, FreeformElement element,
                                 const std::vector<uint32_t>& ids, const FreeformAffine& affine,
                                 FreeformCage* out);

// Moves the selected faces' vertices along their (averaged) outward normals by
// `distance` metres. A single planar quad moves exactly `distance` along its
// normal.
FreeformStatus freeformPushPull(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                double distance, FreeformCage* out);

// Extrudes the selected face region by `distance` along its averaged normals:
// the region's faces keep their ids and stand on new vertices; one new quad per
// region boundary edge joins the old boundary to the new.
FreeformStatus freeformExtrudeFaces(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                    double distance, FreeformCage* out);

// Inserts the edge loop across the quad ring through `edgeId`, splitting every
// ring edge at `ratio` (0..1, exclusive) measured from the edge's own v0 and
// carried consistently around the ring.
FreeformStatus freeformInsertEdgeLoop(const FreeformCage& cage, uint32_t edgeId, double ratio,
                                      FreeformCage* out);

// The edges the ring through `edgeId` crosses, in walk order. For verification.
FreeformStatus freeformEdgeRing(const FreeformCage& cage, uint32_t edgeId,
                                std::vector<uint32_t>* outEdges, bool* outClosed);

// Sets the crease weight of the selected edges.
FreeformStatus freeformSetCrease(const FreeformCage& cage, const std::vector<uint32_t>& edgeIds,
                                 double crease, FreeformCage* out);

// Deletes the selected faces, and the edges and vertices left unused.
FreeformStatus freeformDeleteFaces(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                   FreeformCage* out);

// Sets the subdivision level (0..4), within the quad budget.
FreeformStatus freeformSetSubdivisionLevel(const FreeformCage& cage, int level, FreeformCage* out);

// Sets the symmetry planes. Turning a plane ON requires the cage to be
// symmetric about it already (within 1e-9 of its size), and then makes it so
// exactly; turning one off changes no position.
FreeformStatus freeformSetSymmetry(const FreeformCage& cage, uint8_t symmetry, FreeformCage* out);

// The mirror images of a selection under the cage's symmetry, as ids of the
// same element kind, including the selection itself. Ascending, unique.
std::vector<uint32_t> freeformSymmetricSelection(const FreeformCage& cage, FreeformElement element,
                                                 const std::vector<uint32_t>& ids);

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

struct FreeformMesh;  // forgeshape_freeform_subdivision.h

// THE Freeform body. Owns its cage -- shared, immutable once published, and
// REPLACED on every edit, so a history step holds the very cage it captured and
// two steps that did not change it share one -- and nothing derived but a
// runtime cache.
class FreeformBody {
public:
    FreeformBody(ObjectId objectId, std::shared_ptr<const FreeformCage> cage)
        : objectId_(objectId), cage_(std::move(cage)) {}

    ObjectId objectId() const { return objectId_; }
    const FreeformCage& cage() const { return *cage_; }
    const std::shared_ptr<const FreeformCage>& cagePointer() const { return cage_; }

    // Replaces the cage. FAILS CLOSED: validated whole, refused when a mark
    // would go down; an identical cage reports Ok with `outChanged` false.
    FreeformStatus applyCage(std::shared_ptr<const FreeformCage> next, bool* outChanged = nullptr);

    // History restore: a cage that was authoritative when captured.
    void restoreCage(std::shared_ptr<const FreeformCage> cage);

    // The derived smooth surface at the stored level, from a runtime cache.
    FreeformStatus derived(std::shared_ptr<const FreeformMesh>* out) const;

    uint64_t updateCount() const { return updateCount_; }

private:
    const ObjectId objectId_;
    std::shared_ptr<const FreeformCage> cage_;
    mutable std::shared_ptr<const FreeformMesh> cache_;
    mutable std::shared_ptr<const FreeformCage> cacheCage_;
    uint64_t updateCount_ = 0;
};

// The neutral colour every Freeform vertex carries; presentation only.
constexpr float kFreeformVertexColor[3] = {0.72f, 0.70f, 0.74f};

}  // namespace forgeshape
