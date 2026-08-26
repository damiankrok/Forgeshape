// ForgeShape Sculpt domain — the Frozen Sculpt Mesh, the product mode, and the
// one brush kernel that carries Grab, Clay, Smooth and Inflate.
//
// Platform-independent: no JNI, no Android, no Vulkan, no renderer and no UI
// type appears here, and nothing here holds a GPU resource.
//
// The two representations
// -----------------------
//     Construction Source           ConstructionObject
//         ObjectId                      kind + that kind's exact parameters
//         PrimitiveKind                 + ConstructionTransform
//         exact parameters
//         ConstructionTransform     -> generateMesh()  (LOCAL space, derived)
//
//     Frozen Sculpt Mesh            SculptMesh
//         the SAME ObjectId             a COPY of the local vertices/indices
//         its own SculptRevision        taken at the moment of Freeze
//
// These are separate truths and they never write to each other:
//
//   * Freeze COPIES the Construction object's current local mesh. Nothing is
//     shared, so a sculpt edit cannot reach back into Construction data.
//   * A sculpt edit NEVER modifies a primitive parameter, a kind or a transform.
//   * Nothing ever reconstructs a Construction parameter from SculptMesh
//     vertices. That direction does not exist, exactly as it does not exist from
//     the Construction mesh.
//   * The Construction Source stays fully available after a Freeze, so switching
//     back shows the original object, unsculpted.
//   * Vulkan buffers remain DERIVED copies: whichever representation is active
//     is published through the existing MeshStore path, and the renderer still
//     owns no geometry truth.
//
// The transform is deliberately NOT duplicated here. Both representations are
// LOCAL geometry under the one ConstructionTransform, so the object sits in
// exactly the same place in either mode.
//
// Scope: exactly ONE Frozen Sculpt Mesh for the one active object, and exactly
// FOUR tools sharing ONE stroke kernel. There is no multi-object sculpt storage,
// no brush plugin surface or registry, no undo, no symmetry, no mask, no remesh
// and no topology mutation of any kind.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_picking.h"

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
// Four tools, one kernel. A tool is a DEFORMATION RULE and nothing else: the
// stroke lifecycle, the hit test, the affected set, the falloff weights, the
// radius/strength contract and the publication path are shared and implemented
// exactly once, in SculptStroke.
//
// This is deliberately a closed enum and a switch, not a registry, a base class
// or a plugin surface. Four tools do not justify a framework, and a framework
// would have to be persisted, versioned and validated like real authored state.
enum class SculptTool {
    Grab,     // drag the surface with the finger, in the camera plane
    Clay,     // deposit material along the normals the surface had at stroke start
    Smooth,   // relax each vertex toward its 1-ring neighbour average
    Inflate,  // expand along the normals the surface has RIGHT NOW
};

constexpr int kSculptToolCount = 4;

const char* sculptToolName(SculptTool tool);

// Maps the UI's tool index onto the enum. Out-of-range is refused rather than
// clamped: an unknown tool is a caller bug, not a value to repair.
bool sculptToolFromIndex(int index, SculptTool* out);
int sculptToolIndex(SculptTool tool);

// Whether the tool displaces along surface normals. Grab (camera plane) and
// Smooth (toward a neighbour average) do not.
bool sculptToolUsesNormals(SculptTool tool);

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
// is a byte-identical copy of its Construction source: nothing has been
// sculpted into it yet. That makes `revision > kFrozenSculptRevision` the exact
// predicate for "THIS frozen mesh has user edits" — see SculptMesh::hasEdits().
constexpr SculptRevision kFrozenSculptRevision = 1;

// ---------------------------------------------------------------------------
// Brush parameters, shared by every tool
// ---------------------------------------------------------------------------
//
// The radius is authored in SCREEN PIXELS and converted to object space at the
// depth of the stroke's hit point. That is what makes the brush feel the same
// size regardless of zoom, and it is why the radius is not a length in meters:
// it is a property of the gesture, not of the object.

constexpr float kMinBrushRadiusPixels = 24.0f;
constexpr float kMaxBrushRadiusPixels = 600.0f;
constexpr float kDefaultBrushRadiusPixels = 120.0f;

// Strength scales what each tool does. For Grab, 1.0 means the grabbed centre
// follows the finger exactly; for the other three it scales the amount deposited
// or relaxed per unit of pointer travel. The range is closed and documented, so
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
// shared by all four tools.
//
//     w(d) = (1 - (d/r)^2)^2
//
// which is 1 at the centre, 0 at and beyond the rim, and has zero derivative at
// both ends — so a stroke leaves no crease at the brush edge. Returns 0 for a
// non-positive or non-finite radius, so a degenerate brush moves nothing.
float sculptFalloff(float distance, float radius);

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
// Deposition/expansion amount, in object space:
//
//     amount = strength * localRadius * kNormalBrushGain * travelFraction
//
// and it is clamped to one local radius per move, so a single enormous jump
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
    // are not something the user can still lose. `revision_` is restarted at
    // kFrozenSculptRevision by every freezeFrom and advanced only by
    // advanceRevision(), which runs only when a stroke actually moved a vertex —
    // so a gesture that began and was abandoned to navigation, or a stroke that
    // captured no vertex, correctly reports no edits.
    bool hasEdits() const { return frozen() && revision_ > kFrozenSculptRevision; }

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

    // Mints the next SculptRevision. Called once after a coherent batch of
    // position writes, so a revision always describes a complete edit.
    SculptRevision advanceRevision();

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
    bool renderBothSides_ = false;
    std::vector<MeshVertex> vertices_;
    std::vector<uint32_t> indices_;
    SculptTopology topology_;
    uint64_t freezeCount_ = 0;

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
};

// A single one-finger stroke, for ANY of the four tools.
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
    // the same rule for all four tools: a miss starts NO stroke.
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
    // stroke is a stroke that stopped, not one that is undone; there is no undo
    // in this stage and inventing a partial one here would be worse.
    void cancel();

    bool active() const { return active_; }
    SculptTool tool() const { return tool_; }

    // --- introspection (logging and self-tests only) ---
    int affectedVertexCount() const { return static_cast<int>(affected_.size()); }
    const SculptStrokeVertex& affectedVertex(int i) const { return affected_[i]; }
    Vec3 localCenter() const { return localCenter_; }
    float localRadius() const { return localRadius_; }
    float worldPerPixel() const { return worldPerPixel_; }
    float hitDepth() const { return hitDepth_; }
    Vec3 lastLocalDisplacement() const { return lastLocalDisplacement_; }
    float travelPixels() const { return travelPixels_; }
    float lastAmount() const { return lastAmount_; }
    // The weight this stroke captured for a mesh vertex index, or 0 when that
    // vertex is outside the brush.
    float weightOfVertex(uint32_t meshVertexIndex) const;

private:
    // The four deformation rules. Everything above them is shared.
    bool applyGrab(SculptMesh& mesh, float screenX, float screenY, float strength);
    bool applyClay(SculptMesh& mesh, float amount);
    bool applySmooth(SculptMesh& mesh, float strength, float travelFraction);
    bool applyInflate(SculptMesh& mesh, float amount);

    bool active_ = false;
    SculptTool tool_ = kDefaultSculptTool;

    float anchorX_ = 0.0f;  // view-local pixel where the stroke went down
    float anchorY_ = 0.0f;
    float lastX_ = 0.0f;  // where the pointer was on the previous update
    float lastY_ = 0.0f;
    float travelPixels_ = 0.0f;  // total path length, for logging and tests

    Vec3 localCenter_{0.0f, 0.0f, 0.0f};  // the hit point, in object space
    float localRadius_ = 0.0f;            // the brush radius, in object space
    float radiusPixels_ = 0.0f;           // the brush radius as authored
    float worldPerPixel_ = 0.0f;          // world meters per screen pixel at the hit depth
    float hitDepth_ = 0.0f;               // along the camera forward axis

    Vec3 cameraRight_{1.0f, 0.0f, 0.0f};  // world-space camera plane, fixed for the stroke
    Vec3 cameraUp_{0.0f, 1.0f, 0.0f};
    Mat4 inverseModel_ = mat4Identity();  // world displacement -> local displacement

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
    void bindTarget(FrozenSculpt* target) { target_ = target; }

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

private:
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
