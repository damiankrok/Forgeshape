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
// R0 is one sketch and one linear New-Body extrusion. The state is a struct
// rather than a list so nothing pretends a feature tree exists; when a second
// feature kind arrives it takes a new `CADB` section version rather than a
// discriminator inside this one.
#pragma once

#include <cstdint>

#include "forgeshape_construction.h"
#include "forgeshape_object_id.h"
#include "forgeshape_sketch.h"
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

struct ExtrudeFeature {
    // The anchor entity of the profile to extrude. Must name one closed
    // profile of the sketch; never an index.
    SketchEntityId profileEntityId = kNoSketchEntity;
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
};

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

// The whole authored truth of one CAD Body. Plain, copyable, comparable,
// BOUNDED (the sketch caps its entities and every polyline's vertices), and
// carrying nothing derived.
struct CadBodyState {
    CadSketch sketch;
    ExtrudeFeature extrude;
};

// Bit-exact, for the history and the codec.
bool sameCadBodyState(const CadBodyState& a, const CadBodyState& b);

// The whole rule for whether a state describes a body this build can
// regenerate: a valid sketch, a valid depth and direction, and a chosen
// profile the sketch actually closes. `outProfiles` receives the extraction
// when the sketch is valid, so a caller that needs the polygon does not run it
// twice.
CadStatus validateCadBodyState(const CadBodyState& state,
                               ProfileExtraction* outProfiles = nullptr);

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
CadStatus generateCadMesh(const CadBodyState& state, ConstructionMesh* out);

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
    const CadSketch& sketch() const { return state_.sketch; }
    const ExtrudeFeature& extrude() const { return state_.extrude; }

    // How many times the state actually changed. Diagnostics only.
    uint64_t updateCount() const { return updateCount_; }
    uint64_t rejectedUpdateCount() const { return rejectedUpdates_; }

    // Requests a complete new state.
    //
    // FAILS CLOSED: the whole state is validated through `validateCadBodyState`
    // AND regenerated once, and nothing is written unless both pass. An
    // identical request reports Ok with `outChanged` false and counts nothing.
    CadStatus applyState(const CadBodyState& requested, bool* outChanged = nullptr);

    // The regenerated mesh for the current state. The current state was
    // validated when it was applied, so this cannot fail for a body that was
    // built through `applyState` or a validated load.
    CadStatus generateMesh(ConstructionMesh* out) const { return generateCadMesh(state_, out); }

    // History support, on the same terms as ConstructionObject's pair: a
    // restore writes a state that was authoritative when captured and advances
    // no counter.
    CadBodyState captureState() const { return state_; }
    void restoreState(const CadBodyState& state) { state_ = state; }

private:
    const ObjectId objectId_;
    CadBodyState state_;
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
