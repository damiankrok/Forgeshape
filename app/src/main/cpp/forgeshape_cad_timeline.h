// The Parametric History of a CAD Body: its feature chain read as an ordered
// TIMELINE of sketches and features (`MODELING-FOUNDATIONS-R1` A).
//
// Platform-neutral C++17: no JNI, no Android, no renderer, no camera.
//
// Derived, never stored
// ---------------------
// A CAD Body's history already IS its truth: `CadBodyState` holds the retained
// sketch table and the ordered feature chain, and `regenerateCadBody` rebuilds
// the solid in that order and names the first feature that refuses. This file
// adds no second record of that -- a stored timeline would be a second answer to
// "how was this body built" that could disagree with the chain. `buildCadTimeline`
// is a pure function of a state and a regeneration report, recomputed on every
// read, written to no `.forge` byte, no history step and no fingerprint.
//
// Row order is construction order
// -------------------------------
// For each feature in chain order: the SKETCH it consumes (the first time that
// sketch is consumed -- two features extruding one sketch list it once), then the
// FEATURE. A retained sketch no feature consumes comes last, ascending by id.
// A row is identified by a stable semantic id -- a `CadSketchId` or a feature id
// -- and never by its position, which moves when an earlier sketch is shared.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"

namespace forgeshape {

enum class CadTimelineRowKind : uint8_t {
    Sketch,
    Feature,
};

const char* cadTimelineRowKindName(CadTimelineRowKind kind);

// What regeneration says about a row.
enum class CadTimelineRowState : uint8_t {
    // Regenerated (a feature), or consumed by a feature that regenerated (a
    // sketch).
    Ok,
    // The FIRST feature whose regeneration refused; `status` says why. At most
    // one row of a timeline is Failed.
    Failed,
    // A feature after the failing one: regeneration stopped before it, so
    // nothing is known about it. Never reported as Ok.
    NotRegenerated,
    // A staged edit whose candidate has not been evaluated yet (the sketch is
    // still being drawn): the edited feature and everything after it.
    Pending,
    // A retained sketch no feature consumes.
    Unused,
};

const char* cadTimelineRowStateName(CadTimelineRowState state);

// One row. Everything numeric is read straight off the authored state; nothing
// is formatted here except by `cadTimelineRowSummary`, which exists for logs and
// tests -- the shell formats its own text so lengths follow the display unit.
struct CadTimelineRow {
    CadTimelineRowKind kind = CadTimelineRowKind::Sketch;
    // THE identity: the sketch's `CadSketchId` or the feature's id.
    uint32_t id = 0;
    // A feature's sketch; a sketch row's own id.
    CadSketchId sketchId = kNoCadSketch;
    // The feature a sketch row opens for editing: the FIRST feature that
    // consumes it, or 0 for an unused sketch. A feature row's own id.
    uint32_t editFeatureId = 0;
    // 1-based position among rows of the same kind, for a label ("Sketch 2",
    // "Cut 3"). Presentation only; the id is the identity.
    uint32_t ordinal = 0;
    CadTimelineRowState state = CadTimelineRowState::Ok;
    // The refusal, for a Failed row; Ok otherwise.
    CadStatus status = CadStatus::Ok;
    // Whether this row is what a staged edit changes (the edited feature, or
    // the sketch it consumes).
    bool editing = false;

    // --- a sketch row ---
    uint32_t entityCount = 0;
    uint32_t dimensionCount = 0;
    // The root sketch's workplane; meaningless when `onFeatureFace`.
    Workplane plane = Workplane::XY;
    // A root sketch standing on another body's face (`CAD-A3`).
    bool onBodyFace = false;
    // A sketch on a face of one of this body's own earlier features, and which.
    bool onFeatureFace = false;
    uint32_t supportFeatureId = 0;

    // --- a feature row ---
    CadFeatureKind featureKind = CadFeatureKind::Extrude;
    CadFeatureOperation operation = CadFeatureOperation::NewBody;
    CadSelectionKind selection = CadSelectionKind::LoopRegions;
    uint32_t regionCount = 0;
    uint32_t holeCount = 0;
    // Extrude.
    ExtrudeExtentMode extent = ExtrudeExtentMode::OneSide;
    ExtrudeDirection direction = ExtrudeDirection::AlongNormal;
    Meters positiveDistance = 0.0;
    Meters negativeDistance = 0.0;
    // Revolve.
    double angleDegrees = 0.0;
    RevolveDirection revolveDirection = RevolveDirection::Positive;
    CadSketchEdgeRef axis{};
};

struct CadTimeline {
    std::vector<CadTimelineRow> rows;
    // The regeneration verdict the rows were built from.
    CadStatus status = CadStatus::Ok;
    // The first failing feature, or 0.
    uint32_t failedFeatureId = 0;
    // The feature a staged edit changes, or 0 for a committed body.
    uint32_t editingFeatureId = 0;
    // Whether `status` is a real regeneration verdict or the staged candidate
    // has not been evaluated yet (every row from the edited one on is Pending).
    bool evaluated = true;
};

// How many rows a timeline can have: one per feature and one per sketch.
constexpr uint32_t kMaxCadTimelineRows = kMaxCadFeatures + kMaxCadSketches;

// THE timeline of `state`.
//
// `report` is the regeneration verdict for exactly this state (from
// `regenerateCadBody` or a staged candidate's evaluation); `evaluated` false
// means no verdict exists yet. `editingFeatureId` marks the staged feature (0
// for a committed body). Never fails: a state whose references do not resolve
// still lists every feature it holds, so the user can see what is broken.
CadTimeline buildCadTimeline(const CadBodyState& state, const CadRegenerationReport& report,
                             bool evaluated, uint32_t editingFeatureId);

// The committed body's timeline: regenerates `state` once for the verdict.
CadTimeline buildCadTimelineFor(const CadBodyState& state);

// A short, deterministic description of one row ("Extrude New Body · One Side
// +1 m", "Sketch 1 · XY · 4 entities"), lengths in metres. For logs, tests and
// diagnostics; not localized.
std::string cadTimelineRowSummary(const CadTimelineRow& row);

}  // namespace forgeshape
