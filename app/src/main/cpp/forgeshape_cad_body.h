// The CAD Body: a body whose geometry is a sketch extruded along its
// workplane's normal, and the one regeneration path from that truth to a mesh.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer, no
// camera. `CAD-R0-A1A2`.
//
// A third representation, and why it is one
// ------------------------------------------
// A Construction Body is a primitive plus its parameters; an Imported Mesh IS
// its geometry. A CAD Body is neither: it has no `PrimitiveKind` and no six
// remembered parameter sets, so it cannot be a Construction Source without
// inventing a primitive it was never made from -- the exact rule an Imported
// Mesh already refuses to break -- and its geometry is DERIVED, so storing it
// as an Imported Mesh would throw away the very thing that makes it editable.
// It is its own representation, on the Imported Mesh's terms: exclusive for
// the life of the body, with its own `.forge` section.
//
// What is truth
// -------------
//     CadBodyState
//         CadSketch           the workplane, the entities, the id allocator
//         ExtrudeFeature      which profile, how deep, which way
//
// That is what a history step copies, what the `.forge` `CADB` section
// stores, and what a numeric edit changes. Everything else -- the closed
// profiles, the polygon, the triangles, the extruded mesh, its normals, the
// published revision -- is regenerated from it by `generateCadMesh`, the ONE
// path, every time it is needed. Nothing reads a sketch parameter back out of
// a vertex.
//
// Regeneration is atomic
// ----------------------
// `CadBody::applyState` validates the WHOLE requested state -- every entity,
// the profile the extrusion names, the depth, the mesh it would produce --
// and writes nothing unless all of it passes. An edit that would leave the
// body with no closed profile, or a profile that no longer triangulates, is
// refused by name and the last valid state stands. There is no half-regenerated
// body.
//
// The retained feature chain (`CAD-VERTICAL-SLICE-R1`)
// ----------------------------------------------------
// R0 was one sketch and one linear New-Body extrusion, and that pair is still
// the body's FIRST feature, field for field -- which is what keeps every v1..v4
// `CADB` record meaning exactly what it meant. After it the body may carry a
// bounded, ordered list of LATER features, each a retained sketch on a planar
// face of an EARLIER feature of the same body, extruded, and applied as an Add
// (union) or a Cut (difference) through the boolean kernel
// (forgeshape_cad_kernel.h). A later feature never creates a body: New Body is
// the act that does, and it stays a separate `SceneObject`.
//
//     CadBodyState
//         sketches[]               the retained SKETCH TABLE (`CAD-V6-S1`)
//         baseSketchId + extrude   feature 1: the base New Body extrusion
//         laterFeatures[]          features 2..n: sketchId, extrude, Add|Cut
//
// Sketches are a TABLE, not a field of a feature (`CAD-V6-S1`). Each retained
// sketch has a stable, body-local `CadSketchId` and owns its authored entities
// and its placement; a feature REFERENCES a sketch by id and never carries a
// copy of it, so two features may extrude one sketch and an edit to that sketch
// is what both of them read. A v1..v5 record has one inline sketch per feature
// and is read into the table as exactly that -- one sketch per feature, ids
// 1..n in chain order -- and written back byte-identically while it stays that
// shape.
//
// Regeneration is ORDERED and ATOMIC: the base, then every later feature in
// order, and a mesh is published only when the whole requested chain is valid.
// Editing feature i regenerates i..end; a later feature that becomes impossible
// refuses the edit by name and the previous state stands. Nothing derived --
// no intermediate solid, no kernel output, no face tag -- is ever stored.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_object_id.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_workplane.h"

namespace forgeshape {

// Which SIDE of the sketch plane a One Side extrusion grows on. Exactly
// two-valued, and it is a SIDE and never a sign: the length beside it is always
// positive, in every extent mode.
enum class ExtrudeDirection : uint8_t {
    AlongNormal,
    AgainstNormal,
};

constexpr int kExtrudeDirectionCount = 2;

const char* extrudeDirectionName(ExtrudeDirection direction);
bool extrudeDirectionFromIndex(int index, ExtrudeDirection* out);
int extrudeDirectionIndex(ExtrudeDirection direction);

// How far the solid reaches on each side of the sketch plane (`CAD-EXT-R1`).
//
// The durable truth is TWO non-negative DISTANCES -- one along `+N`, one along
// `-N` -- of which at least one is positive. A mode is not a second model of
// that: it names which combinations the user is authoring and therefore which
// of the two the controls write, and every mode reduces to the same pair
// through `extrudePositiveDistance` / `extrudeNegativeDistance`.
//
// A signed depth is deliberately NOT how a side is stored. A side and a length
// are two different facts, and folding them into one number would make every
// consumer -- the mesh generator, the codec, the face frames, the panel field --
// learn about a sign none of them has ever had to carry.
enum class ExtrudeExtentMode : uint8_t {
    // One distance, on `direction`'s side; the other side is exactly zero.
    // What every extrusion before `CAD-EXT-R1` was, and what a v1/v2/v3
    // `CADB` record still decodes to, value for value.
    OneSide,
    // The same distance on both sides. The stored number is the distance PER
    // SIDE and never a total thickness, so the file does not depend on a
    // presentation preference the UI might later change its mind about.
    Symmetric,
    // Two independent distances: `depth` along `+N` (A) and `secondDistance`
    // along `-N` (B).
    TwoSides,
};

constexpr int kExtrudeExtentModeCount = 3;

const char* extrudeExtentModeName(ExtrudeExtentMode mode);
bool extrudeExtentModeFromIndex(int index, ExtrudeExtentMode* out);
int extrudeExtentModeIndex(ExtrudeExtentMode mode);

// The depth a new extrusion is offered at before the user types one.
constexpr Meters kDefaultExtrudeDepthMeters = 1.0;

// WHICH kind of thing a feature's selection names (`CAD-V6-S1`). An explicit
// discriminator, never inferred from which payload happens to be non-empty.
enum class CadSelectionKind : uint8_t {
    // v1..v5: closed LOOPS and their nesting (`forgeshape_sketch_region.h`),
    // stored as `ProfileRegionRef`s. Every body any earlier version wrote.
    LoopRegions,
    // Atomic faces of the sketch's planar ARRANGEMENT
    // (`forgeshape_sketch_arrangement.h`), stored as canonical `PlanarFaceRef`s.
    // Validated and persisted since `CAD-V6-S1`; not yet regenerated into a
    // solid, and no product path creates one yet.
    PlanarFaces,
};

constexpr int kCadSelectionKindCount = 2;

const char* cadSelectionKindName(CadSelectionKind kind);

// How many planar faces one feature may select, how many holes one face may
// carry, and how many fragments one boundary cycle may have.
//
// The selection bound is the ARRANGEMENT's own face bound
// (`kMaxArrangementFaces`, derived from its source-edge and contact caps in
// forgeshape_sketch_arrangement.h), deliberately NOT the loop-region cap
// (`kMaxProfileRegions`, 16) it was until
// `CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1`: a selection names DISTINCT faces
// of one derived arrangement, so any arrangement that derived at all can have
// every one of its faces chosen and this bound is never the reason a tap is
// refused. It stays a bound because the decoder reads a count before it has an
// arrangement to resolve against. The loop-region cap is untouched and still
// governs `LoopRegions` selections.
//
// The holes cap is the region cap, for the same reason as before; the third
// is the largest polygon a profile may become (`kMaxProfileVertices`): every
// fragment contributes at least one polygon vertex when a face is extruded, so
// a longer cycle could never be regenerated, and bounding it here bounds a
// history step and a file.
constexpr uint32_t kMaxPlanarFaceSelection = kMaxArrangementFaces;
constexpr uint32_t kMaxPlanarFaceHoles = kMaxRegionHoles;
constexpr uint32_t kMaxPlanarFaceCycleFragments = kMaxProfileVertices;

struct ExtrudeFeature {
    // WHAT is extruded: a selection of sketch REGIONS (forgeshape_sketch_region.h),
    // stored by semantic identity and in canonical order. The first region is
    // held in the two fields every earlier version already had -- its outer
    // loop's anchor in `profileEntityId`, its holes in `profileHoleIds` -- and
    // any further region in `additionalRegions`, so a single region without
    // holes IS the R0 profile, byte for byte, and never an index.
    //
    // The anchor of the first selected region's OUTER loop. Canonically the
    // smallest outer anchor of the selection.
    SketchEntityId profileEntityId = kNoSketchEntity;
    // That region's holes, ascending (`CAD-VERTICAL-SLICE-R1`). Empty for a
    // region without holes, which is every region any earlier version stored.
    std::vector<SketchEntityId> profileHoleIds;
    // Further selected regions, ascending by outer anchor, every one above
    // `profileEntityId` (`CAD-VERTICAL-SLICE-R1`).
    std::vector<ProfileRegionRef> additionalRegions;
    // The PRIMARY authored distance, always positive, never signed:
    //   OneSide    the length on `direction`'s side;
    //   Symmetric  the length on EACH side;
    //   TwoSides   the length on the `+N` side (A).
    Meters depth = kDefaultExtrudeDepthMeters;
    // Which side `depth` is on. Meaningful in OneSide ALONE; in every other
    // mode it is canonically `AlongNormal`, because a mode that names both
    // sides has nothing left for a direction to choose and a stored value
    // nothing reads would make two byte-different files of one solid.
    ExtrudeDirection direction = ExtrudeDirection::AlongNormal;
    // `CAD-EXT-R1`. Default OneSide, so every state built before this stage --
    // and every v1/v2/v3 record decoded after it -- is exactly what it was.
    ExtrudeExtentMode extent = ExtrudeExtentMode::OneSide;
    // The `-N` distance (B), in TwoSides ALONE; canonically exactly 0.0 in
    // every other mode, for the reason `direction` is canonical there.
    Meters secondDistance = 0.0;
    // `CAD-V6-S1`. Which payload the selection is. LoopRegions is the three
    // region fields above and `planarFaces` is empty; PlanarFaces is
    // `planarFaces` (canonical: strictly ascending by `comparePlanarFaceRef`)
    // and the region fields are empty. A selection carrying the other kind's
    // payload is refused, never read as either.
    CadSelectionKind selection = CadSelectionKind::LoopRegions;
    std::vector<PlanarFaceRef> planarFaces;
};

bool sameExtrudeFeature(const ExtrudeFeature& a, const ExtrudeFeature& b);

// The selection as ONE canonical list: the first region, then the additional
// ones. Empty when nothing is chosen.
std::vector<ProfileRegionRef> extrudeRegions(const ExtrudeFeature& extrude);

// Writes a whole selection into the three fields, canonically sorted. An empty
// selection clears them.
void setExtrudeRegions(ExtrudeFeature* extrude, std::vector<ProfileRegionRef> regions);

// Whether the selection is exactly one region without holes: the R0 profile,
// which regenerates through the unchanged R0 path.
bool extrudeSelectsSingleSimpleProfile(const ExtrudeFeature& extrude);

// The two durable distances, whatever the mode. THE reduction: every consumer
// that needs to know how far the solid reaches asks these two and never the
// mode, so a fourth mode could not silently mean a fourth geometry rule.
Meters extrudePositiveDistance(const ExtrudeFeature& extrude);
Meters extrudeNegativeDistance(const ExtrudeFeature& extrude);

// Whether the feature is in the ONE canonical form its mode allows: a
// meaningful direction and a zero second distance outside their own modes.
// Validation refuses a non-canonical feature by name rather than repairing it,
// so one solid has exactly one encoding.
bool extrudeFeatureCanonical(const ExtrudeFeature& extrude);

// The feature `to` implies, from the one there is (`CAD-EXT-R1` transitions).
//
// Deterministic and camera-free, so a mode change can never lose a value by
// accident and never averages two the user typed:
//
//   OneSide(d)     -> Symmetric   both sides d
//   OneSide(d)     -> TwoSides    A = B = d
//   Symmetric(d)   -> OneSide     d on `preferredSide`
//   Symmetric(d)   -> TwoSides    A = B = d
//   TwoSides(A, B) -> Symmetric   both sides = the `preferredSide` side's value
//   TwoSides(A, B) -> OneSide     `preferredSide`'s value; the other becomes 0
//
// `preferredSide` is the user's last One Side choice, held by the session as
// volatile intent and never stored: it is what a mode round trip gives back,
// and it is an argument rather than a hidden field precisely so this function
// stays pure. A transition to the mode already held returns the feature
// unchanged. A `preferredSide` whose distance is zero falls back to the other
// side, because a transition may never produce a solid with no extent.
ExtrudeFeature extrudeFeatureWithExtent(const ExtrudeFeature& from, ExtrudeExtentMode to,
                                        ExtrudeDirection preferredSide);

// Writes ONE side's distance, keeping the mode. In Symmetric BOTH sides move,
// because that is what Symmetric means; in OneSide a write to the side the
// solid is not on is refused by returning the feature unchanged.
ExtrudeFeature extrudeFeatureWithSide(const ExtrudeFeature& from, bool positiveSide,
                                      Meters distance);

// Writes the PRIMARY distance and, in OneSide alone, the side it is on. The
// exact-value editor's door: it preserves the mode, so a typed depth over a
// Symmetric body stays symmetric.
ExtrudeFeature extrudeFeatureWithPrimary(const ExtrudeFeature& from, Meters distance,
                                         ExtrudeDirection direction);

// ---------------------------------------------------------------------------
// The feature chain (`CAD-VERTICAL-SLICE-R1`)
// ---------------------------------------------------------------------------

// What an extrusion does to material. The FIRST feature of a body is always
// NewBody -- it is the body -- and every later feature is Add or Cut: a later
// feature never creates a body, and an Add or a Cut that cannot apply is
// refused by name rather than falling back to a new one.
enum class CadFeatureOperation : uint8_t {
    NewBody,
    Add,
    Cut,
};

constexpr int kCadFeatureOperationCount = 3;

const char* cadFeatureOperationName(CadFeatureOperation operation);
bool cadFeatureOperationFromIndex(int index, CadFeatureOperation* out);
int cadFeatureOperationIndex(CadFeatureOperation operation);

// How many features one CAD body may carry, the base included. Bounded so a
// history step, a `.forge` record and a regeneration stay finite; far above
// what a phone session builds on one part.
constexpr uint32_t kMaxCadFeatures = 16;

// Where a LATER feature's sketch stands: a planar face of an EARLIER feature of
// the SAME body, by semantic identity. Deliberately not a `TopoRef`: that names
// another body and derives this body's world placement from it, while this
// names a face in the body's own local space and places only the feature's
// sketch. A body that supported itself through a `TopoRef` would be a cycle.
struct CadFeatureSupport {
    // An earlier feature of this body: the base (kCadFeatureId) or a later one
    // with a smaller id.
    uint32_t featureId = kCadFeatureId;
    CadFaceToken face{};
    // That feature's topology signature when the sketch was placed, on the
    // `TopoRef` rule: a face-structure change fails closed, a size edit keeps
    // it attached.
    uint64_t lineageToken = 0;
};

bool sameCadFeatureSupport(const CadFeatureSupport& a, const CadFeatureSupport& b);

// ---------------------------------------------------------------------------
// The retained sketch table (`CAD-V6-S1`)
// ---------------------------------------------------------------------------

// A retained sketch's identity: body-local, non-zero, minted from the body's
// `nextSketchId`. It is a semantic id: never an index into the table, never
// renderer-derived, never a hash of the sketch's content (two sketches with
// identical entities are two sketches).
//
// How long a CadSketchId and a CadFeatureId live (`CAD-V6-S1-C1`) -- the ONE
// definition; every other comment and document points here.
//
//   * An id is unique along ONE FORWARD HISTORY BRANCH: the body's states from
//     its creation, or from the Open that loaded it, to the current one,
//     through committed edits. A committed edit never LOWERS `nextSketchId` or
//     `nextFeatureId` (`CadBody::applyState` refuses it by name), so an id a
//     committed deletion freed is never minted again on that branch.
//   * Undo restores the whole snapshot, the high-water marks with it, and Redo
//     restores the forward one exactly. So the next edit after an Undo may
//     mint an id that only the undone (redo) step held -- and the commit that
//     mints it is the same `ConstructionHistory::commitEdit` call that clears
//     the redo stack, so the two meanings never both exist in reachable
//     history. Every Undo-reachable state of a body is its creation state or
//     was reached from it through `applyState`, so every id it holds is below
//     the current marks.
//   * A cancelled or refused edit burns nothing: the session mints into its
//     own candidate, and a cancelled Construction edit restores the marks.
//   * The marks are persisted only where a legacy read would not derive them
//     (`CADB` v6), so a reopened project continues from what its file states,
//     and a project Undone back to its saved state is byte- and
//     fingerprint-equal to that save.
//
// Deliberately NOT "for the body's lifetime": that needs an allocator outside
// the snapshot, and then an Undo to a saved project would read unsaved and
// write `CADB` v6 for invisible metadata. What makes the branch rule safe is
// that nothing outside a snapshot holds one of these ids across an Undo: the
// sketch session is exclusive with Undo/Redo, the feature list re-reads on
// every refresh, and nothing above JNI sees a CadSketchId at all.
using CadSketchId = uint32_t;
constexpr CadSketchId kNoCadSketch = 0;

// The id the base sketch of a new body is given, and the id a v1..v5 record's
// base sketch is read as. Later features' sketches read from v5 take 2..n in
// chain order.
constexpr CadSketchId kBaseCadSketchId = 1;

// How many retained sketches one body may carry. The feature cap, because a
// sketch nothing extrudes is legal but a body that is mostly unconsumed
// sketches is not a shape a phone session builds; bounded so a history step
// and a `.forge` record stay finite.
constexpr uint32_t kMaxCadSketches = kMaxCadFeatures;

// One retained sketch: its identity, WHERE it stands, and its authored truth.
//
// Placement belongs to the sketch and not to the features that extrude it,
// because a sketch two features share must stand in one place. It is one of:
//   * the body's ROOT sketch -- `hasFeatureSupport` false -- on its workplane at
//     the body origin, or (through `sketch.faceSupport`, `CAD-A3`) on another
//     body's face. Exactly one root sketch exists, and the base feature
//     extrudes it.
//   * a sketch on a planar face of one of the body's OWN features --
//     `hasFeatureSupport` true, `sketch.plane` canonically XY and no TopoRef.
struct CadSketchRecord {
    CadSketchId sketchId = kNoCadSketch;
    bool hasFeatureSupport = false;
    CadFeatureSupport featureSupport{};
    CadSketch sketch;
};

bool sameCadSketchRecord(const CadSketchRecord& a, const CadSketchRecord& b);

// The empty root sketch a new body starts with.
inline CadSketchRecord emptyRootCadSketchRecord() {
    CadSketchRecord record;
    record.sketchId = kBaseCadSketchId;
    return record;
}

// One later feature: the sketch it extrudes, BY ID, its extrusion and what the
// extrusion does. It holds no sketch and no placement of its own.
struct CadFeature {
    // Stable within the body, strictly ascending along the chain, above the
    // base's kCadFeatureId, minted from `CadBodyState::nextFeatureId`. Never an
    // index. Lifetime: see `CadSketchId`.
    uint32_t featureId = 0;
    CadFeatureOperation operation = CadFeatureOperation::Add;
    CadSketchId sketchId = kNoCadSketch;
    ExtrudeFeature extrude;
};

bool sameCadFeature(const CadFeature& a, const CadFeature& b);

// The whole authored truth of one CAD Body. Plain, copyable, comparable,
// BOUNDED (the sketch caps its entities and every polyline's vertices, the
// table caps its sketches and the chain caps its features), and carrying
// nothing derived.
//
// The base feature is not a `CadFeature` record: its id is always
// kCadFeatureId and its operation always New Body -- the two facts every v1..v5
// record implies -- so a slot for them would be two fields whose only legal
// values are constants. What IS stored for it is what varies: the sketch it
// extrudes (by id, in the one table) and its extrusion.
struct CadBodyState {
    // THE sketch table, strictly ascending by id. The only owner of authored
    // sketch truth in the body. A default state carries one empty root sketch.
    std::vector<CadSketchRecord> sketches{emptyRootCadSketchRecord()};
    // The id the next retained sketch takes: above every id in the table, and
    // never lowered by a committed edit (lifetime: see `CadSketchId`).
    CadSketchId nextSketchId = kBaseCadSketchId + 1u;
    // Feature 1: the base New Body extrusion of the root sketch.
    CadSketchId baseSketchId = kBaseCadSketchId;
    ExtrudeFeature extrude;
    // Features 2..n, in application order. Empty for every body any earlier
    // version created.
    std::vector<CadFeature> laterFeatures;
    // The id the next appended feature takes: above every feature id minted on
    // this forward history branch, so a deleted feature's id is not handed on
    // (the audit's id-reuse finding; lifetime: see `CadSketchId`). Derived as
    // `last + 1` for v1..v5.
    uint32_t nextFeatureId = kCadFeatureId + 1u;
};

// The record with `sketchId`, or null.
const CadSketchRecord* findCadSketchRecord(const CadBodyState& state, CadSketchId sketchId);
CadSketchRecord* findCadSketchRecord(CadBodyState& state, CadSketchId sketchId);

// The base feature's sketch, read through the table. On a state whose base
// sketch does not resolve (never a validated one) the const form answers an
// empty sketch and the mutable form restores the record it names -- the one
// narrow door authoring code that wrote the base sketch keeps, so there is no
// second copy of it anywhere.
const CadSketch& cadBaseSketch(const CadBodyState& state);
CadSketch& cadBaseSketch(CadBodyState& state);

// The sketch record the feature with `featureId` extrudes, or null.
const CadSketchRecord* cadFeatureSketchRecord(const CadBodyState& state, uint32_t featureId);
CadSketchRecord* cadFeatureSketchRecord(CadBodyState& state, uint32_t featureId);

// A one-feature state: `sketch` as the root sketch (id kBaseCadSketchId) and
// `extrude` as the base feature. What every body starts as.
CadBodyState makeCadBodyState(CadSketch sketch, ExtrudeFeature extrude);

// Adds a retained sketch to the table, minting its id from `nextSketchId`.
// `support` null makes it a root sketch; otherwise it stands on that face of
// one of the body's features. Returns kNoCadSketch, adding nothing, when the
// table is full or the high-water mark cannot mint another id. Validates
// nothing else: `validateCadBodyState` is the one judge.
CadSketchId addCadSketchRecord(CadBodyState* state, CadSketch sketch,
                               const CadFeatureSupport* support);

// Appends a later feature extruding the existing sketch `sketchId`, minting its
// id from `nextFeatureId`. Returns 0, adding nothing, when the chain is full.
uint32_t appendCadLaterFeature(CadBodyState* state, CadFeatureOperation operation,
                               CadSketchId sketchId, ExtrudeFeature extrude);

// What an Add or a Cut commit does: a NEW sketch on `support` and a new feature
// extruding it. Returns the feature id, or 0 with nothing added.
uint32_t appendCadLaterFeatureWithSketch(CadBodyState* state, CadFeatureOperation operation,
                                         const CadFeatureSupport& support, CadSketch sketch,
                                         ExtrudeFeature extrude);

// Whether the state says nothing a `CADB` v1..v5 record cannot: one sketch per
// feature with the ids a legacy read synthesizes (base 1, later features 2..n
// in chain order), the root sketch the base's and every later sketch on a
// feature face, the two high-water marks exactly what a legacy read derives,
// and every selection LoopRegions. Exactly the states the codec writes below
// v6, so an unchanged legacy project keeps its bytes.
bool cadBodyStateLegacyRepresentable(const CadBodyState& state);

// Whether any feature selects PlanarFaces. Such a state is valid truth, is
// regenerated like any other since `CAD-V6-S2`, and round-trips only through
// `CADB` v6.
bool cadBodyStateUsesPlanarFaces(const CadBodyState& state);

// The number of features in the chain, the base included.
inline uint32_t cadFeatureCount(const CadBodyState& state) {
    return 1u + static_cast<uint32_t>(state.laterFeatures.size());
}

// A read-only view of one feature, base or later, so a caller can walk the
// chain without asking which kind of slot a feature lives in. `sketch` and
// `support` point INTO the sketch table -- they are the record the feature
// references, never a copy -- and are null when the reference does not
// resolve (never on a validated state).
struct CadFeatureView {
    uint32_t featureId = kCadFeatureId;
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
    CadSketchId sketchId = kNoCadSketch;
    const CadSketch* sketch = nullptr;
    const ExtrudeFeature* extrude = nullptr;
    // The sketch's placement on one of the body's own features. Null for the
    // root sketch, whose support is its own plane or TopoRef -- the base's, and
    // any later feature's that extrudes the base's sketch.
    const CadFeatureSupport* support = nullptr;
};

// The feature at chain position `index` (0 = base). False past the end.
bool cadFeatureAt(const CadBodyState& state, uint32_t index, CadFeatureView* out);
// The feature with `featureId`. False when the chain has none.
bool findCadFeature(const CadBodyState& state, uint32_t featureId, CadFeatureView* out);
// The id the next appended feature takes: the stored high-water mark.
uint32_t nextCadFeatureId(const CadBodyState& state);

// Bit-exact, for the history and the codec.
bool sameCadBodyState(const CadBodyState& a, const CadBodyState& b);

// The whole STRUCTURAL rule for whether a state describes a body this build
// can regenerate: every feature's sketch, depth, direction and extent, a region
// selection its sketch actually derives, the chain's bounds, ids and
// operations, and every later feature's support resolving to an eligible face
// of an earlier feature at the stored lineage. It does NOT run the kernel: an
// Add that would not touch, or a Cut that would remove everything, is only
// known by regenerating (`generateCadMesh`). `outProfiles` receives the BASE
// feature's loop extraction when its sketch is valid.
CadStatus validateCadBodyState(const CadBodyState& state,
                               ProfileExtraction* outProfiles = nullptr);

// One feature's own rule, without the chain: sketch, extent, regions.
// `outRegions` receives the region extraction on success (empty for a
// PlanarFaces selection, which is resolved and merged here instead).
CadStatus validateCadFeatureGeometry(const CadSketch& sketch, const ExtrudeFeature& extrude,
                                     SketchRegionExtraction* outRegions = nullptr);

// The extrusion's own rule -- extent code, canonical form, distances -- which a
// feature satisfies whatever it selects.
CadStatus validateExtrudeExtent(const ExtrudeFeature& extrude);

// The canonical STRUCTURE of one PlanarFaceRef, without any geometry: non-empty
// cycles within the caps, cut kinds in their positions, endpoint cuts carrying
// no partner, every cycle rotated to its unique smallest fragment, holes
// strictly ascending. Orientation is not structural; exact resolution decides it.
CadStatus validatePlanarFaceRefForm(const PlanarFaceRef& ref);

// A PlanarFaces selection over `sketch` (`CAD-V6-S1`): the sketch, the extent,
// no LoopRegions payload beside it, 1..kMaxPlanarFaceSelection faces each in
// canonical form, strictly ascending and distinct, and every one resolving by
// EXACT equality against the sketch's derived arrangement. An arrangement that
// cannot be derived is refused by its own name (a spline, an overlap, a cap).
// No nearest face is ever substituted.
CadStatus validatePlanarFaceSelection(const CadSketch& sketch, const ExtrudeFeature& extrude);

// The same rule, keeping what it derived (`CAD-V6-S2`): the sketch's
// arrangement and, per stored face in stored order, its index in
// `arrangement.faces`. What regeneration and the session read, so a selection
// is resolved by exactly one rule wherever it is used.
CadStatus resolvePlanarFaceSelection(const CadSketch& sketch, const ExtrudeFeature& extrude,
                                     SketchArrangement* outArrangement,
                                     std::vector<size_t>* outFaceIndices);

// The union of a resolved PlanarFaces selection (`mergePlanarFaces`), with the
// arrangement's refusals mapped to their `CadStatus` names: a pinch is
// `PlanarFacesTouchAtPoint` (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`; it was the
// loop model's `OverlappingRegions`, which names something these faces never
// do), a union loop over `kMaxProfileVertices` is
// `TooManyEntities` (the profile cap's existing name), a degenerate one
// `PlanarFaceDegenerate`.
CadStatus mergePlanarFaceSelection(const SketchArrangement& arrangement,
                                   const std::vector<size_t>& faceIndices,
                                   std::vector<PlanarProfileComponent>* out);

// Whether a finished sketch's areas NEED planar faces (`CAD-V6-S2`), given an
// arrangement that derived. True exactly when the arrangement derives (a failed
// one is `decideSketchSelectionMode`'s business), at least one bounded face is bounded by a PROPER fragment (a source
// edge split at a crossing or a T-junction), and the arrangement's face count
// differs from the loop-region count -- i.e. a crossing or a T-junction actually
// CUT an area the region model cannot name. A sketch whose loops only nest (a
// rectangle around two circles), or whose only contact is a dangling line
// touching a rectangle, stays `LoopRegions` and keeps its legacy writer.
bool sketchRequiresPlanarFaces(const SketchArrangement& arrangement,
                               const SketchRegionExtraction& regions);

// Whether the loop model READS a sketch faithfully: no two of its closed loops
// touch or cross, and no chain was refused for crossing itself or forking.
// When that fails, the loops are not the areas the user sees -- a crossing
// split them -- and only the arrangement can name them.
bool sketchLoopsAreExact(const SketchRegionExtraction& regions);

// What Finish makes of a sketch (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`): the one
// decision between the two selection kinds, and the one place a sketch the
// arrangement cannot derive is refused rather than quietly handed to the loop
// model.
//
//   arrangement Ok                -> PlanarFaces when `sketchRequiresPlanarFaces`,
//                                    else LoopRegions (legacy-exact, legacy writer);
//   arrangement failed, loops exact -> LoopRegions: nothing crosses, so the loops
//                                    ARE the areas, exactly as every earlier
//                                    build read them;
//   arrangement failed, loops not exact -> refused with the arrangement's own name
//                                    (`cadStatusForArrangement`): the areas need a
//                                    planar decomposition the sketch cannot have,
//                                    and the loop model's `OverlappingRegions` /
//                                    `OverlappingHoles` would describe a model the
//                                    user is not looking at.
struct SketchSelectionModeDecision {
    CadSelectionKind kind = CadSelectionKind::LoopRegions;
    CadStatus status = CadStatus::Ok;
};
SketchSelectionModeDecision decideSketchSelectionMode(const SketchArrangement& arrangement,
                                                      const SketchRegionExtraction& regions);

// The arrangement status as a `CadStatus` (`UnsupportedCurve` ->
// `PlanarFaceUnsupportedCurve`, ...). Deterministic; `Ok` maps to `Ok`.
CadStatus cadStatusForArrangement(ArrangementStatus status);

// THE regeneration path. Validates, extracts, triangulates, extrudes.
//
// The mesh is closed: a front cap, a back cap and one quad per profile edge,
// sharing the 2n profile vertices, so every edge is on exactly two triangles.
// Winding is the canonical counter-clockwise-from-outside every Construction
// primitive uses, guaranteed by the workplane frame being right-handed and the
// profile being counter-clockwise in (u, v). Hard edges are the render layer's
// business, exactly as they are for a box.
//
// Writes nothing and reports why on any refusal.
//
// Since `CAD-VERTICAL-SLICE-R1` it regenerates the WHOLE chain: a single
// region without holes and no later feature takes exactly the path above (so
// every existing body's mesh is bit-identical); a region with holes or several
// regions build their prisms with inner walls; later features apply through the
// kernel in order.
CadStatus generateCadMesh(const CadBodyState& state, ConstructionMesh* out);

// One semantic face a regenerated mesh carries triangles of.
struct CadMeshFace {
    uint32_t featureId = kCadFeatureId;
    CadFaceToken token{};
    // Whether a sketch may stand on it: a planar face of a New Body or Add
    // feature. A curved side, and every face a Cut leaves behind, is not.
    bool eligible = true;
};

// The full regeneration result: the mesh, and per TRIANGLE the semantic face it
// came from. Derived, transient, never persisted; the face table is what lets a
// tap on a triangle of a boolean result resolve to a stable (feature, face).
struct CadBodyMesh {
    ConstructionMesh mesh;
    std::vector<uint32_t> triangleFace;  // one per triangle, into `faces`
    std::vector<CadMeshFace> faces;
    // Measured on the double-precision solid before it became floats.
    double volume = 0.0;
    uint32_t components = 0;
};

// Why a regeneration stopped, and where.
struct CadRegenerationReport {
    CadStatus status = CadStatus::Ok;
    // The feature that refused, or 0 when the refusal is not a feature's.
    uint32_t failedFeatureId = 0;
    // Microseconds spent in the kernel, for the performance evidence.
    double kernelMicros = 0.0;
};

// THE regeneration, with its derived face table. `generateCadMesh` is this
// without the table.
CadStatus regenerateCadBody(const CadBodyState& state, CadBodyMesh* out,
                            CadRegenerationReport* report = nullptr);

// The neutral colour every CAD vertex carries. Presentation only: it feeds the
// debug-only source-colour shading mode and nothing else.
constexpr float kCadBodyVertexColor[3] = {0.70f, 0.72f, 0.66f};

// How many vertices and indices an extrusion of an n-gon profile produces.
constexpr uint32_t cadExtrusionVertexCount(uint32_t profileVertices) {
    return 2u * profileVertices;
}
constexpr uint32_t cadExtrusionIndexCount(uint32_t profileVertices) {
    // Two caps of (n - 2) triangles, plus 2n side triangles.
    return 3u * (2u * (profileVertices - 2u) + 2u * profileVertices);
}

// THE CAD Body. Owns its state and nothing derived.
class CadBody {
public:
    explicit CadBody(ObjectId objectId) : objectId_(objectId) {}
    CadBody(ObjectId objectId, CadBodyState state)
        : objectId_(objectId), state_(std::move(state)) {}

    ObjectId objectId() const { return objectId_; }
    const CadBodyState& state() const { return state_; }
    // The base feature's sketch (the root of the table).
    const CadSketch& sketch() const { return cadBaseSketch(state_); }
    const ExtrudeFeature& extrude() const { return state_.extrude; }

    // How many times the state actually changed. Diagnostics only.
    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Requests a complete new state.
    //
    // FAILS CLOSED: the whole state is validated through `validateCadBodyState`
    // AND regenerated once, and nothing is written unless both pass. An
    // identical request reports Ok with `outChanged` false and counts nothing.
    // A request that would LOWER either id high-water mark is refused
    // (`HighWaterInvalid`): this is the one door every forward edit takes, and
    // that refusal is what keeps an id unique along the forward branch (see
    // `CadSketchId`). History restores use `restoreState` instead.
    CadStatus applyState(const CadBodyState& requested, bool* outChanged = nullptr);

    // The regenerated mesh for the current state. The current state was
    // validated when it was applied, so this cannot fail for a body that was
    // built through `applyState` or a validated load.
    CadStatus generateMesh(ConstructionMesh* out) const;

    // The full regeneration of the current state, face table included, from
    // the runtime cache when the state has not changed since it was built.
    // The cache is derived, never persisted and never compared.
    CadStatus regenerated(std::shared_ptr<const CadBodyMesh>* out) const;

    // History support, on the same terms as ConstructionObject's pair: a
    // restore writes a state that was authoritative when captured and advances
    // no counter.
    CadBodyState captureState() const { return state_; }
    void restoreState(const CadBodyState& state) {
        state_ = state;
        cache_.reset();
    }

private:
    const ObjectId objectId_;
    CadBodyState state_;
    // Runtime-only: the last regeneration and the state it was built from, so
    // a publish after an apply, and every redraw, does no kernel work twice.
    mutable std::shared_ptr<const CadBodyMesh> cache_;
    mutable CadBodyState cacheState_;
    uint64_t updateCount_ = 0;
    uint64_t rejectedUpdates_ = 0;
};

// The typed edits the exact-value editor makes to an EXISTING CAD Body. Each
// builds a complete candidate state from the current one and applies it
// atomically, so a refused edit leaves the body exactly as it was.
//
// A rectangle or a circle edit addresses the profile the extrusion names --
// it changes the entity that IS that profile, keeping its centre -- because
// that is the one entity the user can see the body was made from.
// Writes the PRIMARY distance and, in OneSide alone, the side. The extent MODE
// is preserved: a typed depth over a Symmetric body edits the per-side distance
// and leaves it symmetric.
CadStatus applyCadExtrude(CadBody& body, Meters depth, ExtrudeDirection direction,
                          bool* outChanged = nullptr);
CadStatus applyCadRectangle(CadBody& body, Meters width, Meters height, bool* outChanged = nullptr);
CadStatus applyCadCircle(CadBody& body, Meters radius, bool* outChanged = nullptr);

// What kind of entity anchors the extruded profile, so the shell can offer
// the right fields: a rectangle, a circle, or a polygon (a polyline or a chain
// of lines) whose vertices are not numerically editable in R0.
enum class CadProfileKind : uint8_t {
    None,
    Rectangle,
    Circle,
    Polygon,
};

CadProfileKind cadProfileKind(const CadBodyState& state);
const char* cadProfileKindName(CadProfileKind kind);

}  // namespace forgeshape
