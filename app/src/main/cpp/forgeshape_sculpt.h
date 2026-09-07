// ForgeShape Sculpt domain — the Frozen Sculpt Mesh, the product mode, and the
// one brush kernel that carries Grab, Clay, Smooth, Flatten, Inflate, Crease and
// Mask.
//
// Platform-independent: no JNI, no Android, no Vulkan, no renderer and no UI
// type appears here, and nothing here holds a GPU resource.
//
// Source and sculpt: two truths per body
// --------------------------------------
//     SOURCE representation           Construction Source or Imported Mesh
//         ObjectId                        (a CAD Body has no sculpt yet)
//         -> buildSculptSourceMesh()      LOCAL space, derived, read only
//
//     Frozen Sculpt Mesh              SculptMesh + SculptHistory (`FrozenSculpt`)
//         the SAME ObjectId               a COPY of the local vertices/indices
//         its own SculptRevision          taken at the moment of Freeze
//
// These never write to each other:
//
//   * Freeze COPIES the body's current local source mesh. Nothing is shared, so
//     a sculpt edit cannot reach back into a parameter or an imported vertex.
//   * A sculpt edit NEVER modifies a primitive parameter, a kind, an imported
//     array or the body's transform, and nothing reconstructs any of them from
//     SculptMesh vertices.
//   * The source stays fully available after a Freeze, so switching back shows
//     the original object, unsculpted.
//   * Vulkan buffers remain DERIVED copies: whichever representation is active
//     is published through the MeshStore path, and the renderer owns no truth.
//
// The transform is NOT duplicated here. Both representations are LOCAL geometry
// under the body's one placement, so the object sits in the same place in
// either mode.
//
// Scope: one Frozen Sculpt Mesh PER BODY (owned by `SceneObject`, borrowed by
// the one process-scoped `SculptSession`), SEVEN tools sharing ONE stroke
// kernel, and a bounded per-body stroke Undo/Redo (forgeshape_sculpt_history.h).
// No brush plugin surface or registry, no symmetry, no remesh and no
// topology mutation of any kind. The Sculpt Mask (`SCULPT-FCM-R1`) is a
// runtime-local per-vertex weight that HOLDS the geometry brushes off a vertex;
// it is not project truth and reaches no `.forge` byte.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_picking.h"
#include "forgeshape_sculpt_history.h"

namespace forgeshape {

// Which representation of the one active object the product is currently
// editing. Native code owns this; the Android UI may only REQUEST a change.
enum class ProductMode {
    Construction,  // the exact primitive, its parameters and its placement
    Sculpt,        // the Frozen Sculpt Mesh
};

const char* productModeName(ProductMode mode);

// ---------------------------------------------------------------------------
// The tools
// ---------------------------------------------------------------------------
//
// Seven tools, one kernel. A tool is a DEFORMATION RULE and nothing else: the
// stroke lifecycle, the hit test, the affected set, the falloff weights, the
// radius/strength contract and the publication path are shared and implemented
// exactly once, in SculptStroke.
//
// This is deliberately a closed enum and a switch, not a registry, a base class
// or a plugin surface. Seven tools do not justify a framework, and a framework
// would have to be persisted, versioned and validated like real authored state.
//
// SIX OF THE SEVEN ARE GEOMETRY BRUSHES and one is not: Mask writes a per-vertex
// weight and never a position. That is the only structural division in the set,
// and `sculptToolMovesGeometry` is the one place it is stated.
//
// The three added by `SCULPT-FCM-R1` are APPENDED rather than interleaved into
// the product's reading order (Grab, Clay, Smooth, Flatten, Inflate, Crease,
// Mask), on exactly the terms `SKETCH-UX-R1` appended Arc and Spline: the four
// that were here keep their indices, so nothing that already crossed JNI as an
// index has to be renumbered, and the rail decides the order it presents.
enum class SculptTool {
    Grab,     // drag the surface with the finger, in the camera plane
    Clay,     // deposit material along the normals the surface had at stroke start
    Smooth,   // relax each vertex toward its 1-ring neighbour average
    Inflate,  // expand along the normals the surface has RIGHT NOW
    Flatten,  // draw the surface toward one plane fitted to the brush footprint
    Crease,   // cut a narrow groove: inward along the normal, pinched radially
    Mask,     // paint the weight that holds the other six off a vertex
};

constexpr int kSculptToolCount = 7;

const char* sculptToolName(SculptTool tool);

// Maps the UI's tool index onto the enum. Out-of-range is refused rather than
// clamped: an unknown tool is a caller bug, not a value to repair.
bool sculptToolFromIndex(int index, SculptTool* out);
int sculptToolIndex(SculptTool tool);

// Whether the tool displaces along surface normals. Grab (camera plane), Smooth
// (toward a neighbour average) and Mask (which moves nothing) do not.
bool sculptToolUsesNormals(SculptTool tool);

// Whether the tool writes a vertex POSITION. True for the six geometry brushes
// and false for Mask alone.
//
// The one predicate the mask factor, the revision rule and the history's two
// sides all ask, so "which tool can move a vertex" has a single answer that a
// new tool has to declare rather than inherit by accident.
bool sculptToolMovesGeometry(SculptTool tool);

// The SculptRevision — the Frozen Sculpt Mesh's own revision counter.
//
// It is emphatically NOT a MeshRevision: MeshStore's revisions are minted by the
// publication path and count every published snapshot of whichever
// representation is active, while a SculptRevision counts changes to THIS mesh
// and starts again at 1 on every Freeze. The two cannot be compared and neither
// is derived from the other.
using SculptRevision = uint64_t;

// Reserved: "nothing has been frozen yet".
constexpr SculptRevision kNoSculptRevision = 0;

// The revision every Freeze restarts at. A mesh still sitting at this revision
// is a byte-identical copy of its source: nothing has been sculpted into it yet.
//
// It is NOT the predicate for "this mesh has user edits". It was, until
// `ARCH-OWNER-12`, and Sculpt Undo is what took the two apart: a revision is
// monotonic and must stay so, because the renderer and every derived cache use
// it to notice a change — including the change an Undo makes. Geometry can go
// backwards while the counter only goes forwards, so "has edits" became its own
// stored fact. See SculptMesh::hasEdits().
constexpr SculptRevision kFrozenSculptRevision = 1;

// ---------------------------------------------------------------------------
// Brush parameters, shared by every tool
// ---------------------------------------------------------------------------
//
// The radius is authored in SCREEN PIXELS and resolved to WORLD METERS at the
// depth of the stroke's hit point. That is what makes the brush feel the same
// size regardless of zoom, and it is why the authored value is not a length in
// meters: it is a property of the gesture, not of the object. It is never
// carried into object space — see the brush metric below for why a world radius
// has no single local length once the body carries a non-uniform Scale.

constexpr float kMinBrushRadiusPixels = 24.0f;
constexpr float kMaxBrushRadiusPixels = 600.0f;
constexpr float kDefaultBrushRadiusPixels = 120.0f;

// Strength scales what each tool does. For Grab, 1.0 means the grabbed centre
// follows the finger exactly; for the path-driven tools it scales the amount
// deposited, relaxed, flattened, creased or painted per unit of pointer travel.
// The range is closed and documented, so
// a strength can never be zero (a brush that does nothing) or unbounded (a brush
// that throws vertices to infinity).
constexpr float kMinBrushStrength = 0.05f;
constexpr float kMaxBrushStrength = 1.0f;
constexpr float kDefaultBrushStrength = 0.6f;

constexpr SculptTool kDefaultSculptTool = SculptTool::Grab;

// Both clamp rather than reject: a slider cannot produce a meaningless value,
// and a non-finite one falls back to the default rather than poisoning a stroke.
float clampBrushRadiusPixels(float requested);
float clampBrushStrength(float requested);

// Smooth radial falloff, evaluated once per affected vertex at stroke start and
// shared by all seven tools.
//
//     w(d) = (1 - (d/r)^2)^2
//
// which is 1 at the centre, 0 at and beyond the rim, and has zero derivative at
// both ends — so a stroke leaves no crease at the brush edge. Returns 0 for a
// non-positive or non-finite radius, so a degenerate brush moves nothing.
float sculptFalloff(float distance, float radius);

// ---------------------------------------------------------------------------
// The brush metric — ONE conversion, shared by every brush
// ---------------------------------------------------------------------------
//
// The Frozen Sculpt Mesh is LOCAL geometry; the body carries it into the world
// through Model = T * R * S. The brush radius is authored in SCREEN PIXELS and
// resolved into WORLD meters at the hit depth, so the distance a brush measures
// with has to be the WORLD one. For a local offset d that distance is
//
//     |R * S * d|  =  |S * d|
//
// because a rotation preserves length. Only for S = (1,1,1) does that reduce to
// |d|, which is why measuring in local coordinates turned a round 120 px brush
// into an oval footprint on a body scaled (3,1,1): not a look, the wrong
// metric. One averaged, largest or smallest scale factor cannot repair it
// either — an anisotropic stretch is not a scalar, and collapsing it to one
// would only choose which axis is wrong.
//
// The whole model matrix is taken rather than a scale triple so the helper is
// exact for any placement and has nothing to keep in step with the transform:
// translation drops out with the implicit w = 0, and R contributes no length.
// A non-finite offset propagates to a non-finite distance, which every caller
// already treats as outside the brush.
float brushWorldDistance(const Mat4& model, const Vec3& localDelta);

// The LOCAL step that displaces a vertex by `worldMeters` along the direction a
// LOCAL surface normal actually points on screen.
//
// Clay and Inflate both deposit along a normal, and under a non-uniform scale a
// local normal is neither the displayed direction — that is R * S^-1 * n, the
// inverse transpose, the same matrix the renderer shades with — nor a local
// length equal to a world one. Both conversions live here so the two brushes
// cannot drift apart, and the step is built in WORLD space and carried back
// through the inverse model, which is exactly what Grab already does with its
// camera-plane delta.
//
// The inverse transpose of the model's linear part is the transpose of the
// INVERSE model's linear part, so this needs no third matrix argument and
// cannot fall out of step with the inverse the rest of the stroke uses.
//
// Returns the zero vector — a step that moves nothing — for a degenerate normal
// or a non-finite result, and reduces exactly to `n * worldMeters` for an
// unscaled body with a unit normal, so nothing about an unscaled body changes.
Vec3 brushLocalStepAlongNormal(const Mat4& inverseModel, const Vec3& localNormal,
                               float worldMeters);

// ---------------------------------------------------------------------------
// How much a travel-driven tool does
// ---------------------------------------------------------------------------
//
// Grab is POSITION-driven: the surface follows where the finger IS. The other
// three are PATH-driven: they apply an amount proportional to how far the finger
// has travelled, measured in brush radii, so a slow stroke and a fast stroke
// over the same path do the same thing and the result does not depend on the
// event rate. That is the whole of the accumulation rule; there is no timer, no
// per-event dab and no dependence on how many MotionEvents Android delivered.
//
//     travelFraction = pointer travel this move (pixels) / brush radius (pixels)
//
// Deposition/expansion amount, in WORLD meters — the same metric the affected
// set was selected with, so a stretched body deposits an even slab rather than
// one that is deeper along whichever axis happens to be scaled up:
//
//     amount = strength * worldRadius * kNormalBrushGain * travelFraction
//
// and it is clamped to one brush radius per move, so a single enormous jump
// (a teleporting pointer) can never produce an unbounded displacement.
constexpr float kNormalBrushGain = 0.35f;

// Smoothing interpolates toward the neighbour average by
//
//     lambda = strength * weight * kSmoothGain * travelFraction
//
// clamped to kMaxSmoothLambda so a vertex can never overshoot its own neighbour
// average — which is what keeps repeated smoothing convergent instead of
// oscillating.
constexpr float kSmoothGain = 1.0f;
constexpr float kMaxSmoothLambda = 0.9f;

// ---------------------------------------------------------------------------
// Flatten (`SCULPT-FCM-R1`)
// ---------------------------------------------------------------------------
//
// Flatten is INTERPOLATION toward a plane, on exactly Smooth's terms and for
// exactly Smooth's reason: a vertex may only ever move part of the way toward a
// target it is already near, so repeated passes converge and can never
// overshoot into a ridge on the far side of the plane.
//
//     d      = signed WORLD distance from the vertex to the plane
//     lambda = strength * weight * kFlattenGain * travelFraction, <= kMaxFlattenLambda
//     d'     = d * (1 - lambda)
//
// so |d| shrinks by a factor in [1 - kMaxFlattenLambda, 1) on every pass and
// never changes sign. That is the whole monotonicity claim, and it holds per
// vertex rather than only on average.
constexpr float kFlattenGain = 1.0f;
constexpr float kMaxFlattenLambda = 0.9f;

// ---------------------------------------------------------------------------
// Crease (`SCULPT-FCM-R1`)
// ---------------------------------------------------------------------------
//
// One displacement with TWO components, and the split between them is the whole
// character of the tool. Inward alone digs a round dent; pinch alone gathers the
// surface without deepening it. Together they cut a groove that is narrower than
// the brush that made it.
//
//   inward   along -n, the normal the surface had at stroke start, so a pass
//            deepens the same channel instead of chasing a normal that the
//            previous pass just tilted.
//   pinch    along the TANGENTIAL direction toward the brush centre — the part
//            of (centre - p) perpendicular to that vertex's normal — so the
//            surface is gathered along the groove rather than pushed through it.
//
// Both fractions are of the ONE shared `amount` every path-driven brush
// computes, so Crease answers to Radius, Strength and travel exactly as Clay
// does. They are held here, together, because their RATIO is what makes the
// groove narrow, and two constants in two files would drift.
constexpr float kCreaseInwardFraction = 0.75f;
constexpr float kCreasePinchFraction = 0.45f;

// ---------------------------------------------------------------------------
// Mask (`SCULPT-FCM-R1`)
// ---------------------------------------------------------------------------
//
// Painting is accumulation into [0, 1], by the same travel-driven rule the
// geometry brushes deposit with, so a mask is built up by working over an area
// rather than by one instantaneous toggle:
//
//     delta = strength * weight * kMaskGain * travelFraction
//
// clamped per move to kMaxMaskStep so one enormous pointer jump cannot paint a
// full mask in a single event, and clamped in total to [0, 1] by
// SculptMesh::setMaskWeight.
constexpr float kMaskGain = 1.0f;
constexpr float kMaxMaskStep = 0.5f;

// How a mask weight scales a geometry brush's displacement.
//
//     factor = 1 - w
//
// Linear, with EXACT ends: w = 0 is the full effect and w = 1 is exactly zero,
// which is what makes "a fully masked vertex does not move" an identity rather
// than a tolerance. Returns 1 for a non-finite weight — an unmasked vertex —
// because refusing to sculpt over a value the domain says cannot exist would be
// the tail wagging the dog.
float sculptMaskFactor(float maskWeight);

// ---------------------------------------------------------------------------
// Fixed-topology adjacency
// ---------------------------------------------------------------------------
//
// Sculpt topology never changes: SculptMesh can only move a vertex, so the index
// buffer a Freeze copied is the index buffer for the life of that frozen mesh.
// The adjacency is therefore built ONCE, at Freeze, and reused by every stroke
// and every move. Nothing here is rebuilt per MOVE.
//
// Deliberately NOT a half-edge structure: this stores exactly the two things the
// stage needs — each vertex's 1-ring vertex neighbours and its incident
// triangles — in flat CSR arrays. A half-edge mesh is the right structure for
// changing topology, and nothing here can change topology.
class SculptTopology {
public:
    // Builds from an index buffer. Out-of-range indices and degenerate corners
    // are skipped rather than trusted, so a bad index can never produce an
    // out-of-range neighbour. Neighbour lists are sorted and deduplicated, so a
    // vertex never appears twice in its own 1-ring and never appears in it at
    // all.
    void build(const std::vector<MeshVertex>& vertices, const std::vector<uint32_t>& indices);

    void clear();

    bool built() const { return vertexCount_ > 0; }
    uint32_t vertexCount() const { return vertexCount_; }

    // The 1-ring vertex neighbours of `vertex`, or nullptr when it has none.
    const uint32_t* neighbors(uint32_t vertex, uint32_t* outCount) const;
    uint32_t neighborCount(uint32_t vertex) const;

    // The triangle indices (not index-buffer offsets) touching `vertex`.
    const uint32_t* incidentTriangles(uint32_t vertex, uint32_t* outCount) const;
    uint32_t incidentTriangleCount(uint32_t vertex) const;

    // Introspection for logging and self-tests only.
    uint32_t totalNeighborEntries() const { return static_cast<uint32_t>(neighborList_.size()); }
    uint64_t buildCount() const { return buildCount_; }

private:
    uint32_t vertexCount_ = 0;
    std::vector<uint32_t> neighborStart_;  // vertexCount_ + 1 entries
    std::vector<uint32_t> neighborList_;
    std::vector<uint32_t> triangleStart_;  // vertexCount_ + 1 entries
    std::vector<uint32_t> triangleList_;
    uint64_t buildCount_ = 0;
};

// Area-weighted vertex normals from CURRENT positions.
//
// Each triangle contributes its unnormalized geometric normal
// `(v1 - v0) x (v2 - v0)` to all three of its corners, which weights a large
// triangle more than a sliver without any extra arithmetic, and then each vertex
// normal is normalized.
//
// Safety: a degenerate (zero-area) triangle contributes a zero vector rather
// than a NaN, and a vertex whose accumulated normal has no usable length gets
// the ZERO vector rather than an invented direction. A zero normal is honest —
// that surface has no defined direction — and a normal-based brush simply does
// not move such a vertex. Every value this writes is finite.
void computeVertexNormals(const std::vector<MeshVertex>& vertices,
                          const std::vector<uint32_t>& indices, std::vector<Vec3>* out);

// ---------------------------------------------------------------------------
// The Frozen Sculpt Mesh
// ---------------------------------------------------------------------------
//
// A platform-independent CPU owner of local-space vertex and index data, its
// fixed-topology adjacency, its current-position vertex normals, and its own
// revision. It is created only by freezing a Construction mesh, and the only
// thing that can change afterwards is a vertex POSITION: there is no API here
// that can add, remove or reorder a vertex or touch an index, so topology is
// preserved by construction rather than by discipline.
class SculptMesh {
public:
    // Copies `source` wholesale, builds the adjacency, and starts this mesh at
    // SculptRevision 1.
    //
    // Refuses (leaving any previous state completely untouched) a source that is
    // not a usable triangle mesh, so a SculptMesh that reports frozen() has
    // already been proven publishable.
    bool freezeFrom(const ConstructionMesh& source, ObjectId objectId,
                    MeshValidation* outWhy = nullptr);

    bool frozen() const { return revision_ != kNoSculptRevision; }

    // True when THIS frozen mesh has been sculpted since it was frozen.
    //
    // The predicate the destructive re-Freeze guard asks, and deliberately NOT
    // a session-lifetime stroke count: re-Freeze destroys the edits on the mesh
    // that exists right now, so strokes that landed on some earlier frozen mesh
    // are not something the user can still lose.
    //
    // An explicit flag rather than `revision_ > kFrozenSculptRevision`, which is
    // what it was before Sculpt Undo existed. It is cleared by every freezeFrom
    // and set by advanceRevision(), which runs only when a stroke actually moved
    // a vertex — so a gesture abandoned to navigation, or a stroke that captured
    // no vertex, still correctly reports no edits, exactly as before. What the
    // flag adds is the one case a monotonic counter cannot express: an Undo that
    // returns the mesh to its unedited seed must report NO edits while the
    // revision keeps climbing. See restoreEditedFlag().
    bool hasEdits() const { return frozen() && hasEdits_; }

    // Whether the frozen geometry is a flat, open sheet that is legitimately
    // usable from both sides — carried over from the Construction mesh this was
    // frozen from, NOT re-derived from whatever the Construction Source happens
    // to be now. See ConstructionMesh::renderBothSides and
    // RuntimeMesh::renderBothSides(): sidedness is a fact about a specific
    // published representation, and a frozen sheet stays a sheet even after the
    // Construction Source has been changed to something else entirely.
    bool renderBothSides() const { return renderBothSides_; }

    // The SAME identity the Construction object carries. Freezing does not
    // create a new object; it creates a second representation of this one.
    ObjectId objectId() const { return objectId_; }

    SculptRevision revision() const { return revision_; }

    uint32_t vertexCount() const { return static_cast<uint32_t>(vertices_.size()); }
    uint32_t indexCount() const { return static_cast<uint32_t>(indices_.size()); }

    const std::vector<MeshVertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& indices() const { return indices_; }

    // Built once per Freeze and valid for the life of that frozen mesh, because
    // nothing here can change an index.
    const SculptTopology& topology() const { return topology_; }

    // Vertex normals for the CURRENT positions.
    //
    // Recomputation rule, stated once: the cache is marked dirty by every
    // accepted setVertexPosition and is recomputed on the next read. So normals
    // are recomputed at most ONCE per batch of position writes — in practice
    // once per brush move that changed anything — and never per frame, never per
    // vertex and never when nothing has moved.
    const std::vector<Vec3>& vertexNormals() const;
    Vec3 vertexNormal(uint32_t index) const;

    // Reads one vertex position in LOCAL space.
    Vec3 vertexPosition(uint32_t index) const;

    // Writes one vertex position in LOCAL space. Refuses an out-of-range index
    // and a non-finite position, so no stroke can leave a NaN in the mesh.
    //
    // This is the ONLY mutation this class offers. Indices are not writable at
    // all, and no counts can change.
    bool setVertexPosition(uint32_t index, const Vec3& position);

    // -----------------------------------------------------------------------
    // The Sculpt Mask (`SCULPT-FCM-R1`)
    // -----------------------------------------------------------------------
    //
    // A per-vertex weight in [0, 1] saying how much this vertex is HELD against
    // the six geometry brushes. It lives here, on the mesh, because it is
    // per-vertex data indexed exactly as the positions are: a Freeze sizes it
    // and zeroes it, nothing can resize it afterwards (topology is fixed for the
    // life of a frozen mesh), and one body's mask is structurally incapable of
    // reaching another's because one body's mesh is.
    //
    // IT IS RUNTIME-LOCAL AND IT IS NOT PROJECT TRUTH. No `.forge` byte carries
    // it, no encoder can see it, it moves no project fingerprint, and reopening
    // a project restores the geometry with an empty mask — exactly as it
    // restores the geometry with an empty history. It survives Back to
    // Construction and Resume Sculpt for the same reason the history does: it
    // lives on the body, and leaving Sculpt is navigation.
    //
    // The weights are stored INSIDE the vertex records (MeshVertex::mask)
    // rather than in a parallel array, so the whole publication path from here
    // to the vertex buffer carries the mask with no signature anywhere having
    // to learn about masking. That is a presentation channel exactly as the
    // colour beside it is; see MeshVertex.
    float maskWeight(uint32_t index) const;

    // Writes one vertex's mask weight, CLAMPED into [0, 1]. Refuses an
    // out-of-range index and a non-finite weight; returns true only when the
    // stored value actually changed, so a paint that lands on an already-full
    // mask costs no publication.
    //
    // Clamped rather than refused, unlike a position: a weight is a fraction
    // with two hard ends, and a brush that accumulates past 1.0 is asking for
    // "fully masked", not making an error.
    bool setMaskWeight(uint32_t index, float weight);

    // How many vertices carry a non-zero mask right now.
    //
    // Tracked incrementally rather than scanned, because the chrome asks it on
    // every refresh and the answer decides whether Clear Mask is drawn at all.
    uint32_t maskedVertexCount() const { return maskedCount_; }
    bool hasMask() const { return maskedCount_ > 0; }

    // Mints the next SculptRevision, and marks this mesh edited.
    //
    // Called once after a coherent batch of position writes, so a revision
    // always describes a complete edit.
    //
    // A MASK write deliberately does NOT come through here. The revision and
    // the edited flag are both statements about GEOMETRY — the project
    // fingerprint mixes the revision, and `.forge` stores the flag — so
    // advancing either for a mask would make a runtime annotation dirty the
    // project and earn a recovery checkpoint. What a mask change does need is a
    // re-publication, and a publication mints its own MeshRevision on every
    // call regardless of this counter.
    SculptRevision advanceRevision();

    // Restores the edited flag to a value a Sculpt history entry captured.
    //
    // The ONE caller is SculptSession's undo/redo, and it calls this AFTER
    // advanceRevision(), because an Undo is a change the renderer must see
    // (revision forward) that may nonetheless leave the mesh unedited (flag
    // back). Nothing else may write it: outside history, "edited" is decided by
    // whether a stroke moved a vertex, and a general setter would be a way to
    // lie about that.
    void restoreEditedFlag(bool edited) { hasEdits_ = edited; }

    // Non-owning triangle view, for picking the sculpt geometry directly.
    TriangleMeshView triangleView() const;

    // How many times freezeFrom() has replaced this mesh, and how many times the
    // normal cache has actually been recomputed. Introspection for logging and
    // self-tests only.
    uint64_t freezeCount() const { return freezeCount_; }
    uint64_t normalRecomputeCount() const { return normalRecomputeCount_; }

private:
    ObjectId objectId_ = kNoObject;
    SculptRevision revision_ = kNoSculptRevision;
    // Whether a stroke has moved a vertex of THIS frozen mesh. See hasEdits().
    bool hasEdits_ = false;
    bool renderBothSides_ = false;
    std::vector<MeshVertex> vertices_;
    std::vector<uint32_t> indices_;
    SculptTopology topology_;
    uint64_t freezeCount_ = 0;

    // How many of `vertices_` carry a non-zero mask. Maintained by
    // setMaskWeight and reset by every freezeFrom; see maskedVertexCount().
    uint32_t maskedCount_ = 0;

    // The normal cache is derived data, not truth: it is `mutable` so that
    // reading normals off a const mesh is possible, exactly as reading a
    // position is.
    mutable std::vector<Vec3> normals_;
    mutable bool normalsDirty_ = true;
    mutable uint64_t normalRecomputeCount_ = 0;
};

// Publishes the current sculpt geometry through the existing MeshStore path, so
// the renderer and CPU picking both see it with no second upload mechanism and
// no renderer change.
MeshRevision publishSculptMesh(const SculptMesh& mesh, MeshStore& store,
                               MeshValidation* outWhy = nullptr);

// ---------------------------------------------------------------------------
// The stroke kernel
// ---------------------------------------------------------------------------

// One vertex captured at stroke start: which vertex, how strongly the brush
// holds it, where it was before the stroke began, and which way the surface
// faced there.
//
// Capturing the base position is what makes Grab idempotent in its own
// displacement: every Move recomputes `base + delta * weight`, so the vertex
// tracks the finger instead of accumulating per-move deltas that would drift
// with the event rate. Capturing the base NORMAL is what makes Clay different
// from Inflate — see the tool notes on SculptStroke::update.
struct SculptStrokeVertex {
    uint32_t index = 0;
    float weight = 0.0f;
    Vec3 basePosition{0.0f, 0.0f, 0.0f};
    Vec3 baseNormal{0.0f, 0.0f, 0.0f};

    // The mask as it stood when the finger landed (`SCULPT-FCM-R1`), and the
    // `1 - w` factor derived from it.
    //
    // CAPTURED, like everything else here, because the affected set, the
    // weights, the base positions and the base normals are all fixed for the
    // stroke's life and the mask has to be fixed with them: a brush whose
    // effect changed mid-stroke because something re-read the mask would be a
    // second definition of what one stroke is. Nothing can paint a mask while a
    // geometry stroke is running anyway — one stroke holds one tool.
    float baseMask = 0.0f;
    float maskFactor = 1.0f;

    // What a geometry brush actually multiplies its displacement by. The
    // falloff weight is left untouched beside it so the brush's own footprint
    // stays introspectable independently of what the mask allowed.
    float effectiveWeight() const { return weight * maskFactor; }
};

// A single one-finger stroke, for ANY of the seven tools.
//
// Everything the stroke needs is captured on DOWN and then held fixed: the
// active tool, the affected vertex set, their falloff weights, their starting
// positions and normals, the world-space camera plane, the world-per-pixel scale
// at the hit depth, and the inverse model transform. A stroke therefore behaves
// the same whether the finger moves in one event or fifty, and camera motion
// during a stroke cannot change what it is working on.
class SculptStroke {
public:
    // Starts a stroke at a view-local pixel.
    //
    // Returns false — leaving the stroke inactive and the mesh untouched — when
    // the ray misses the sculpt mesh, when the mesh is not frozen, when the hit
    // lies behind the camera, or when no vertex falls inside the brush. That is
    // the same rule for all seven tools: a miss starts NO stroke.
    bool begin(SculptTool tool, const SculptMesh& mesh, const CameraSnapshot& camera,
               float screenX, float screenY, int viewportWidth, int viewportHeight,
               const Mat4& model, const Mat4& inverseModel, float radiusPixels);

    // Applies this tool's deformation for a pointer that has arrived at
    // (screenX, screenY). Returns true only when at least one position actually
    // changed, so a Move that lands on the same pixel costs no revision and no
    // upload.
    bool update(SculptMesh& mesh, float screenX, float screenY, float strength);

    // Finalizes the stroke. The deformation stays; only the stroke state goes.
    void end();

    // Abandons the stroke. Positions already written stay written — a cancelled
    // stroke is a stroke that stopped, not one that is rolled back.
    //
    // Sculpt Undo does not change that rule, it completes it: because the
    // deformation stands, it must be UNDOABLE, so `SculptSession::cancelStroke`
    // records the same single entry an ordinary end would. A cancel that moved
    // nothing still records nothing. What must never happen — and cannot,
    // because the entry is built in one go from the affected set — is a PARTIAL
    // entry describing half a stroke.
    void cancel();

    // Builds the history entry for this stroke, as the vertices it actually
    // moved and where they were before it started.
    //
    // Returns false, leaving `*out` untouched, when the stroke is inactive or
    // when not one captured vertex ended up anywhere other than where it began
    // — which is the no-op stroke the history must not record.
    //
    // The BEFORE positions cost nothing to keep: `begin` already captured every
    // affected vertex's base position, because the tools need it. The
    // AFTER positions are read from the mesh here, which is why this must run
    // BEFORE end() or cancel() clears the affected set.
    //
    // `beforeHasEdits` comes from what the mesh reported at pointer-down, not
    // from what it reports now — by the time a stroke ends the answer is always
    // "yes", and the whole point of the entry is to restore what it was.
    bool buildDelta(const SculptMesh& mesh, SculptStrokeDelta* out) const;

    bool active() const { return active_; }
    SculptTool tool() const { return tool_; }

    // --- introspection (logging and self-tests only) ---
    int affectedVertexCount() const { return static_cast<int>(affected_.size()); }
    const SculptStrokeVertex& affectedVertex(int i) const { return affected_[i]; }
    Vec3 localCenter() const { return localCenter_; }
    float worldRadius() const { return worldRadius_; }
    float worldPerPixel() const { return worldPerPixel_; }
    float hitDepth() const { return hitDepth_; }
    Vec3 lastLocalDisplacement() const { return lastLocalDisplacement_; }
    float travelPixels() const { return travelPixels_; }
    float lastAmount() const { return lastAmount_; }
    // The weight this stroke captured for a mesh vertex index, or 0 when that
    // vertex is outside the brush.
    float weightOfVertex(uint32_t meshVertexIndex) const;

    // The Flatten plane this stroke fitted at pointer-down, in WORLD space, and
    // whether the fit produced a usable one. Introspection for the self-tests,
    // which assert convergence against the plane the stroke actually used
    // rather than against one they refitted themselves.
    bool hasFlattenPlane() const { return flattenPlaneValid_; }
    Vec3 flattenPlanePoint() const { return flattenPoint_; }
    Vec3 flattenPlaneNormal() const { return flattenNormal_; }

private:
    // The seven deformation rules. Everything above them is shared.
    bool applyGrab(SculptMesh& mesh, float screenX, float screenY, float strength);
    bool applyClay(SculptMesh& mesh, float amount);
    bool applySmooth(SculptMesh& mesh, float strength, float travelFraction);
    bool applyInflate(SculptMesh& mesh, float amount);
    bool applyFlatten(SculptMesh& mesh, float strength, float travelFraction);
    bool applyCrease(SculptMesh& mesh, float amount);
    bool applyMask(SculptMesh& mesh, float strength, float travelFraction);

    // Fits the Flatten plane from the affected set, once, at pointer-down.
    //
    // WORLD space, from the weighted centroid of the captured base positions
    // and the weighted average of their base normals — so the plane is a
    // property of the SURFACE under the brush and of the body's placement, and
    // of nothing else. No camera, no zoom and no viewport enters it, which is
    // what makes a flattened result independent of how the sculpt was looked
    // at. Leaves flattenPlaneValid_ false for a degenerate fit (no usable
    // averaged normal), and Flatten then moves nothing rather than inventing a
    // direction.
    void fitFlattenPlane(const Mat4& model);

    bool active_ = false;
    SculptTool tool_ = kDefaultSculptTool;

    // What SculptMesh::hasEdits() said when this stroke went down. Captured
    // because it is the only moment the answer is still the pre-stroke one; see
    // buildDelta().
    bool beganWithEdits_ = false;

    float anchorX_ = 0.0f;  // view-local pixel where the stroke went down
    float anchorY_ = 0.0f;
    float lastX_ = 0.0f;  // where the pointer was on the previous update
    float lastY_ = 0.0f;
    float travelPixels_ = 0.0f;  // total path length, for logging and tests

    Vec3 localCenter_{0.0f, 0.0f, 0.0f};  // the hit point, in object space
    float worldRadius_ = 0.0f;            // the brush radius, in WORLD meters
    float radiusPixels_ = 0.0f;           // the brush radius as authored
    float worldPerPixel_ = 0.0f;          // world meters per screen pixel at the hit depth
    float hitDepth_ = 0.0f;               // along the camera forward axis

    Vec3 cameraRight_{1.0f, 0.0f, 0.0f};  // world-space camera plane, fixed for the stroke
    Vec3 cameraUp_{0.0f, 1.0f, 0.0f};
    Mat4 inverseModel_ = mat4Identity();  // world displacement -> local displacement
    // The forward transform, captured beside the inverse because Flatten and
    // Crease both have to measure in WORLD space — the metric the affected set
    // was chosen with — and a local position has to be carried out to get
    // there. Grab, Clay, Smooth and Inflate never needed it and still do not.
    Mat4 model_ = mat4Identity();

    // The Flatten plane, WORLD space, fitted once by fitFlattenPlane().
    bool flattenPlaneValid_ = false;
    Vec3 flattenPoint_{0.0f, 0.0f, 0.0f};
    Vec3 flattenNormal_{0.0f, 1.0f, 0.0f};

    Vec3 lastLocalDisplacement_{0.0f, 0.0f, 0.0f};
    float lastAmount_ = 0.0f;

    std::vector<SculptStrokeVertex> affected_;

    // Which slot of `affected_` carries the largest falloff weight — the vertex
    // closest to the centre of the brush. It exists so the logged displacement
    // is always the CENTRE's, deterministically, rather than whichever vertex
    // the loop happened to touch last.
    size_t centreSlot_ = 0;

    // Scratch reused across moves so a stroke does not allocate per event.
    // Smoothing reads a coherent snapshot of neighbour positions and writes
    // afterwards (Jacobi, not Gauss-Seidel), so the result cannot depend on the
    // order the affected set happens to be in.
    std::vector<Vec3> scratchTargets_;
};

// ---------------------------------------------------------------------------
// The sculpt session
// ---------------------------------------------------------------------------
//
// Owns the product mode, the one Frozen Sculpt Mesh, the active tool, the brush
// settings and the live stroke. This is where "which representation is active"
// and "which tool the finger is holding" are decided, and both are native state:
// the Android UI can ask, it cannot hold either.
// ---------------------------------------------------------------------------
// The per-body half of sculpting
// ---------------------------------------------------------------------------
//
// Everything about sculpting that belongs to ONE Construction Body: the Frozen
// Sculpt Mesh it owns, and whether its Construction Source has moved on since
// that mesh was frozen. Each body carries one of these, so two bodies can be
// frozen, sculpted and gone stale entirely independently.
//
// Deliberately NOT here: the product mode, the held tool, the brush radius and
// strength, and any stroke in progress. Those describe the editing session, not
// a body, and duplicating them per body would mean switching bodies silently
// changed the brush — which the product contract forbids.
struct FrozenSculpt {
    SculptMesh mesh;
    bool sourceStale = false;

    // This body's Sculpt Undo/Redo stacks (`ARCH-OWNER-12`).
    //
    // Per body for the same reason the mesh is: the history describes THESE
    // vertices, and a body switch must carry both or neither. Ownership is the
    // whole mechanism — there is no key, no registry and no "current sculpt
    // stack", so body A's Undo is structurally incapable of reaching body B.
    //
    // Runtime-only and volatile. It is not project truth, no encoder can see
    // it, and it dies with the body — which for a deleted body means when the
    // Construction history finally releases the held object, not when the
    // Delete happens.
    SculptHistory history;

    // THE stale-source rule, and the only implementation of it.
    //
    // It lives on the per-body state rather than only on the session because
    // the Construction history restores bodies the session is not currently
    // bound to: an undo that changes body #2's shape while body #1 is active
    // must mark #2 stale, and routing that through the session would mark the
    // wrong body. Nothing here touches a vertex or a revision — going stale is
    // a statement about the SOURCE, never an edit to the mesh.
    void markSourceStale() { sourceStale = mesh.frozen(); }
};

class SculptSession {
public:
    ProductMode mode() const { return mode_; }
    bool inSculptMode() const { return mode_ == ProductMode::Sculpt; }

    const SculptMesh& mesh() const { return target().mesh; }
    SculptMesh& mesh() { return target().mesh; }
    bool hasSculptMesh() const { return target().mesh.frozen(); }

    // Points this session at ONE body's Frozen Sculpt Mesh. Called whenever the
    // active body is resolved, so the session always edits the body the user is
    // on; the mode, the tool, the brush and any stroke in progress are the
    // session's own and are unaffected by rebinding.
    // Rebinding to a DIFFERENT body while a stroke is in flight drops that
    // stroke unrecorded: its affected set and base positions describe the mesh
    // it started on, and recording it against another body's history would let
    // an Undo there write foreign positions. The old target may already be
    // gone, so nothing is read from it. Callers that can close a stroke
    // properly (`loadProjectDocument`) do so before rebinding.
    void bindTarget(FrozenSculpt* target) {
        if (target != target_ && stroke_.active()) {
            stroke_.cancel();
        }
        target_ = target;
    }

    // Takes a coherent snapshot of the supplied Construction local mesh, makes
    // it the Frozen Sculpt Mesh, and enters Sculpt mode.
    //
    // The Construction Source is not read for anything but its generated mesh
    // and its identity, and is not modified in any way.
    bool freezeToSculpt(const ConstructionMesh& source, ObjectId objectId,
                        MeshValidation* outWhy = nullptr);

    // Returns to Construction mode. Any live stroke is dropped; the sculpted
    // vertices are kept, and NOTHING is copied back into Construction.
    void enterConstruction();

    // Returns to Sculpt mode WITHOUT re-freezing, so prior edits come back
    // exactly as they were. Returns false when nothing has ever been frozen —
    // entering Sculpt then requires an explicit Freeze.
    bool enterSculpt();

    // Stale-source policy.
    //
    // When the Construction Source changes while a Frozen Sculpt Mesh exists,
    // the sculpt mesh is NEVER silently replaced or re-derived: it is simply
    // marked as having been frozen from an older source. Adopting the new source
    // is an explicit user act — another Freeze — which is the only thing that
    // clears the flag. There is no automatic sculpt-edit transfer.
    void markSourceStale() { target().markSourceStale(); }
    bool sourceStale() const { return target().sourceStale; }

    SculptStroke& stroke() { return stroke_; }
    const SculptStroke& stroke() const { return stroke_; }

    SculptTool tool() const { return tool_; }
    // Changing the tool never touches geometry and never touches a stroke that
    // is already running: a stroke captured its tool on Down and keeps it, so
    // the tool cannot change under a finger that is already moving.
    void setTool(SculptTool tool) { tool_ = tool; }

    float radiusPixels() const { return radiusPixels_; }
    float strength() const { return strength_; }
    void setRadiusPixels(float requested) { radiusPixels_ = clampBrushRadiusPixels(requested); }
    void setStrength(float requested) { strength_ = clampBrushStrength(requested); }

    // --- introspection (logging and self-tests only) ---
    uint64_t strokeCount() const { return strokeCount_; }
    uint64_t freezeCount() const { return target().mesh.freezeCount(); }

    // Does the ray from this pixel hit the Frozen Sculpt Mesh?
    //
    // This is the arbitration probe: it answers "would a stroke start here?"
    // WITHOUT starting one and without touching a single vertex, which is what
    // lets a one-finger Down be held pending until it is known whether the
    // gesture is a brush stroke or the first half of a two-finger navigation.
    bool hitsSculptMesh(const CameraSnapshot& camera, float screenX, float screenY,
                        int viewportWidth, int viewportHeight, const Mat4& inverseModel) const;

    // Stroke lifecycle, kept here so the stroke counter, the active tool and the
    // mode rule have exactly one implementation.
    bool beginStroke(const CameraSnapshot& camera, float screenX, float screenY,
                     int viewportWidth, int viewportHeight, const Mat4& model,
                     const Mat4& inverseModel);
    bool updateStroke(float screenX, float screenY);
    void endStroke();
    void cancelStroke();

    // -----------------------------------------------------------------------
    // Sculpt Undo / Redo (`ARCH-OWNER-12`)
    // -----------------------------------------------------------------------
    //
    // The active body's OWN history, and the only place a history entry is ever
    // applied to a mesh. `SculptHistory` decides what is retained; this decides
    // what that means for geometry, for the edited flag and for the revision,
    // so there is exactly one implementation of "an Undo is a step backwards in
    // geometry and forwards in revision".
    //
    // Neither touches the Construction Source, the Imported Mesh, the body's
    // placement, the ObjectId allocator or the Construction history. A sculpt
    // step is not a project act and cannot become one.

    // Why an Undo or Redo did not happen. Every one is a refusal that changes
    // nothing: no vertex moves, no revision is minted, and both stacks stand.
    enum class SculptHistoryStatus {
        Ok,
        // Not in Sculpt mode. The two histories are never consulted together,
        // so a Construction-mode request is not silently answered from here.
        NotSculpting,
        // A stroke is in progress. Stepping the history under a finger that is
        // still writing positions would apply an entry the stroke is about to
        // overwrite, so it waits for the stroke to commit or cancel.
        StrokeActive,
        // The active body has no Frozen Sculpt Mesh to step.
        NoSculptMesh,
        // The stack in that direction is empty.
        NothingToDo,
        // A jump named a state that is not on the retained branch
        // (`SCULPT-H1`). Refused by name rather than clamped to the nearest
        // reachable state: an ordinal the caller cannot address is a caller
        // reading a branch that has since moved, and silently landing it
        // somewhere else would take the user to a state they did not tap.
        OutOfRange,
        // Clear Mask on a mask so large that its single history entry would
        // exceed `kMaxSculptHistoryEntryBytes` (`SCULPT-FCM-R1`). REFUSED, and
        // nothing is cleared.
        //
        // This is deliberately NOT the brush's own `NotRetained` policy, and
        // the difference is which act is being asked about. A brush stroke is
        // bounded by the brush and its deformation is what the user is doing;
        // refusing to sculpt because the history is full would be the tail
        // wagging the dog, so an over-large stroke applies and says so. Clear
        // Mask is bounded by the MESH, is a discrete command rather than a
        // gesture, and its whole value is that it can be taken back — so an
        // unretainable one is refused instead of leaving the user with a
        // destroyed mask and no way home.
        EntryTooLarge,
    };

    static const char* sculptHistoryStatusName(SculptHistoryStatus status);

    // Whether a step exists AND could run right now. The enabled state of the
    // two chrome controls, answered natively so no Java-side mirror can
    // disagree with the stacks.
    bool canUndoSculpt() const;
    bool canRedoSculpt() const;

    SculptHistoryStatus undoStroke();
    SculptHistoryStatus redoStroke();

    // -----------------------------------------------------------------------
    // The History navigator's one act (`SCULPT-H1`)
    // -----------------------------------------------------------------------
    //
    // Moves the mesh to the state at `targetCursor` on the retained branch —
    // see `SculptHistoryCursor` for what an ordinal addresses.
    //
    // IT IS REPEATED UNDO AND REPEATED REDO, and it is written that way rather
    // than described that way: the loop below calls `undoStroke()` and
    // `redoStroke()` themselves, so "jumping back three is the same as tapping
    // Undo three times" is a structural fact about this function and not a
    // property two implementations have to keep agreeing on. There is no second
    // delta path, no batched apply and no snapshot restore.
    //
    // It records NOTHING. A jump is navigation over entries that already exist,
    // so no entry is minted, the retained set is unchanged, and the caps are
    // untouched. It is not a Construction step either: the project's own
    // history never hears about it.
    //
    // The ABANDONED FUTURE is not discarded here. Jumping backward leaves the
    // states ahead of the cursor on the redo stack exactly as an Undo does, so
    // the user can walk forward again; they are dropped by the EXISTING rule —
    // `SculptHistory::record` clears the redo stack — when the next stroke
    // makes them describe a future that no longer follows from the present.
    SculptHistoryStatus jumpToHistoryCursor(size_t targetCursor);

    // Whether the navigator has a branch to offer right now. Exactly the
    // conditions a jump checks, asked without performing one, so the control
    // and the act cannot disagree.
    bool canNavigateSculptHistory() const;

    // -----------------------------------------------------------------------
    // Clear Mask (`SCULPT-FCM-R1`)
    // -----------------------------------------------------------------------
    //
    // Sets every masked vertex back to zero, as ONE history entry, so it is
    // taken back by one Undo exactly as a stroke is. It is not a stroke: it
    // mints no SculptRevision, sets no edited flag and moves not one vertex —
    // it is one act on the runtime annotation, recorded in the same history for
    // the same reason the strokes are, because "one act, one Undo" cannot have
    // two answers.
    //
    // Refusals, all of which change nothing: the three a step already asks
    // (`NotSculpting`, `StrokeActive`, `NoSculptMesh`), `NothingToDo` for a
    // mask that is already empty, and `EntryTooLarge` — see the enum.
    SculptHistoryStatus clearMask();

    // Whether Clear Mask has anything to do right now. Exactly the conditions
    // clearMask() checks minus the size test, asked without performing one, so
    // the control and the act cannot disagree. The control is ABSENT when this
    // is false: a control that cannot succeed is not drawn.
    bool canClearMask() const;

    // The active body's history, for depth and byte-budget introspection.
    const SculptHistory& history() const { return target().history; }

private:
    // Records the live stroke, if it moved anything, as ONE entry.
    //
    // The single implementation, called by both endStroke() and cancelStroke()
    // — which is what makes "one completed stroke is one entry" a structural
    // fact rather than a convention two call sites happen to share. It runs
    // BEFORE the stroke clears its affected set, because that set is where the
    // before-positions live.
    void recordActiveStroke();

    // Writes one direction of a history entry into the mesh: the geometry side
    // if the entry has one, the mask side if it has one, or both. Shared by
    // undo and redo, which differ only in which direction of the delta they
    // hand it.
    //
    // The revision and the edited flag move ONLY for an entry that carries
    // geometry. A mask-only entry leaves both exactly where they were, which is
    // what keeps Undo over a mask act out of the project fingerprint — see
    // SculptMesh::advanceRevision.
    void applyHistorySide(const SculptStrokeDelta& entry,
                          const std::vector<Vec3>& positions,
                          const std::vector<float>& mask, bool edited);

    // GLOBAL, because they describe the editing session rather than any one
    // body: which mode the product is in, the stroke in progress, the held
    // tool, and the brush. Radius and Strength being shared is a documented
    // product contract — switching bodies must no more change the brush than
    // switching tools does.
    ProductMode mode_ = ProductMode::Construction;
    SculptStroke stroke_;
    SculptTool tool_ = kDefaultSculptTool;
    float radiusPixels_ = kDefaultBrushRadiusPixels;
    float strength_ = kDefaultBrushStrength;
    uint64_t strokeCount_ = 0;

    // PER BODY, bound to whichever body is active. See bindTarget().
    FrozenSculpt* target_ = nullptr;
    // Used only before anything is bound, so every accessor stays total rather
    // than dereferencing null. A session with no target reports "nothing
    // frozen", which is the truthful answer.
    FrozenSculpt unbound_;

    FrozenSculpt& target() { return (target_ != nullptr) ? *target_ : unbound_; }
    const FrozenSculpt& target() const { return (target_ != nullptr) ? *target_ : unbound_; }
};

// Process-scoped session. Like the camera, the selection, the mesh store and the
// Construction object, it outlives every Surface: the active mode, the active
// tool and every sculpted vertex survive home/resume and swapchain recreation
// for the life of the process.
SculptSession& sculptSession();

}  // namespace forgeshape
