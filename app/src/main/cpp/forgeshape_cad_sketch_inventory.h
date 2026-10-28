// The retained sketches of a CAD Body, read as an INVENTORY (`CAD-R2`): who
// consumes each one, where it stands, and which acts the domain permits on it.
//
// Platform-neutral C++17: no JNI, no Android, no renderer, no camera.
//
// Derived, never stored
// ---------------------
// The sketch table (`CadBodyState::sketches`) and the feature chain are the
// truth; a row here is a pure function of a state and the regeneration report
// for exactly that state, rebuilt on every read and written to no `.forge`
// byte, no history step and no fingerprint. The shell offers an act only when
// a row's `acts` says native would accept it, and it holds no copy of the list.
//
// Row order is the timeline's
// ---------------------------
// A sketch is listed in construction order: the first time a feature in chain
// order consumes it, then every sketch no feature consumes, ascending by id --
// exactly the order and the ordinal `buildCadTimeline` gives its sketch rows,
// so "Sketch 2" names the same sketch in the History and in the Sketches list.
//
// Deleting a sketch
// -----------------
// Only a sketch no feature consumes, and never the root (the base's sketch,
// which says where the body is). A refusal is named (`SketchInUse`,
// `SketchIsRoot`) and nothing is cascaded: no feature is removed for the user.
// The high-water mark is never lowered, so the freed id is not minted again on
// this forward branch (see `CadSketchId`).
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Where a retained sketch stands.
enum class CadSketchSupportKind : uint8_t {
    // The root sketch on a principal workplane at the body origin.
    WorldPlane,
    // The root sketch on another body's planar face (`CAD-A3` TopoRef).
    BodyFace,
    // A sketch on a planar face of one of this body's own features.
    FeatureFace,
};

const char* cadSketchSupportKindName(CadSketchSupportKind kind);

// What regeneration says about a sketch.
enum class CadSketchRowState : uint8_t {
    // Consumed, and every consumer regenerated.
    Ok,
    // No feature consumes it, and its placement resolves.
    Unused,
    // One of its consumers is the FIRST feature the chain refused.
    Failed,
    // Every consumer comes after the failing feature: never evaluated.
    NotRegenerated,
    // No feature consumes it and its placement no longer resolves (or the
    // chain stops before its support): it is listed, never silently dropped.
    SupportLost,
};

const char* cadSketchRowStateName(CadSketchRowState state);

// The acts a row may offer, as bits.
constexpr uint32_t kCadSketchActEdit = 1u << 0;
constexpr uint32_t kCadSketchActDelete = 1u << 1;
constexpr uint32_t kCadSketchActReuseAdd = 1u << 2;
constexpr uint32_t kCadSketchActReuseCut = 1u << 3;

struct CadSketchInventoryRow {
    CadSketchId sketchId = kNoCadSketch;
    // 1-based, the timeline's sketch ordinal. Presentation; the id is identity.
    uint32_t ordinal = 0;
    // The base's sketch.
    bool root = false;
    CadSketchSupportKind support = CadSketchSupportKind::WorldPlane;
    // The root sketch's workplane; XY (canonical) for a feature-face sketch.
    Workplane plane = Workplane::XY;
    // A FeatureFace sketch's support: the feature and the face token.
    uint32_t supportFeatureId = 0;
    CadFaceToken supportFace{};
    uint32_t entityCount = 0;
    uint32_t dimensionCount = 0;
    // The features that extrude it, in chain order. Empty for an unused sketch.
    std::vector<uint32_t> consumers;
    CadSketchRowState state = CadSketchRowState::Ok;
    // The failing feature's reason for a Failed row, the placement's reason for
    // a SupportLost row; Ok otherwise.
    CadStatus status = CadStatus::Ok;
    // Which acts native accepts (kCadSketchAct* bits), and for each act it
    // does NOT accept, why -- so the shell can say why a control is absent.
    uint32_t acts = 0;
    CadStatus deleteRefusal = CadStatus::Ok;
    CadStatus reuseRefusal = CadStatus::Ok;
};

struct CadSketchInventory {
    std::vector<CadSketchInventoryRow> rows;
    CadStatus status = CadStatus::Ok;
    uint32_t failedFeatureId = 0;
    // The id the next retained sketch would take (the high-water mark).
    CadSketchId nextSketchId = kNoCadSketch;
};

// THE inventory of `state`, given the regeneration verdict for exactly that
// state. Never fails: a state whose references do not resolve still lists
// every sketch it holds.
CadSketchInventory buildCadSketchInventory(const CadBodyState& state,
                                           const CadRegenerationReport& report);

// The committed body's inventory: regenerates `state` once for the verdict.
CadSketchInventory buildCadSketchInventoryFor(const CadBodyState& state);

// Why a NEW later feature could not extrude `sketchId` (Add or Cut), or Ok:
// the body must be an Extrude body (a revolved body takes no later feature in
// R1), the chain must have room, the sketch must place and close at least one
// area. Whether the Add touches or the Cut removes anything is the
// regeneration's question, answered by the candidate.
CadStatus cadSketchReuseRefusal(const CadBodyState& state, CadSketchId sketchId);

// `state` without the sketch `sketchId`, into `out`. Pure. Refuses the root
// (`SketchIsRoot`), a consumed sketch (`SketchInUse`) and an unknown id
// (`SketchNotFound`), writing nothing. Never lowers `nextSketchId`.
CadStatus removeUnusedCadSketch(const CadBodyState& state, CadSketchId sketchId,
                                CadBodyState* out);

// Delete Sketch on body `bodyId`: ONE `ScopedConstructionEdit` around ONE
// `CadBody::applyState`, so one Delete is one Undo and Undo restores the same
// id. Refused, changing nothing, while a Construction edit is open
// (`RefusedEditInProgress`), for a body that is not a CAD body (`NotCadBody`)
// and by `removeUnusedCadSketch`'s names. The caller refuses while a sketch
// session or Sculpt is open, exactly as it refuses every other body command.
CadStatus deleteCadSketch(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                          CadSketchId sketchId);

}  // namespace forgeshape
