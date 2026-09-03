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

// Which way along the workplane normal the solid grows. Cheap, clean and
// exactly two-valued; a symmetric or two-sided extrusion is not R0.
enum class ExtrudeDirection : uint8_t {
    AlongNormal,
    AgainstNormal,
};

constexpr int kExtrudeDirectionCount = 2;

const char* extrudeDirectionName(ExtrudeDirection direction);
bool extrudeDirectionFromIndex(int index, ExtrudeDirection* out);
int extrudeDirectionIndex(ExtrudeDirection direction);

// The depth a new extrusion is offered at before the user types one.
constexpr Meters kDefaultExtrudeDepthMeters = 1.0;

struct ExtrudeFeature {
    // The anchor entity of the profile to extrude. Must name one closed
    // profile of the sketch; never an index.
    SketchEntityId profileEntityId = kNoSketchEntity;
    Meters depth = kDefaultExtrudeDepthMeters;
    ExtrudeDirection direction = ExtrudeDirection::AlongNormal;
};

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
