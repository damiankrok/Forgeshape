// How a Surface body is authored (`MODELING-FOUNDATIONS-R1` C): a sketch drawn
// in the ONE sketch session becomes a Surface feature, a body-level act adds a
// Stitch or a Thicken, and a typed value edits one feature or one sketch's
// offset -- each ONE Construction transaction, each held to the whole-chain
// regeneration before anything is written.
//
// Platform-neutral C++17: no JNI, no Android, no renderer. The JNI layer only
// forwards a request and reads back what changed.
//
// Nothing here is a second model of a Surface body. The body's truth is its
// `SurfaceBodyState`; every function builds a CANDIDATE state, regenerates it
// through `regenerateSurfaceBody` (the one path), and writes it through
// `SurfaceBody::applyState` inside one `ScopedConstructionEdit` only when the
// whole chain passes. A refusal names the first failing feature and changes
// nothing.
//
// A sketch drawn for a Surface feature is the ordinary sketch session's
// sketch: the same tools, snapping, selection, Construction role and
// dimensions. What differs is what Finish means. `SurfaceSketchPurpose` holds
// that -- which body the commit lands on, or none for a new one -- and it is
// SESSION state: never serialized, never in a history step, cleared with the
// sketch. While it is set the CAD commit refuses, so a Surface sketch can never
// silently become a CAD body.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_history.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_surface.h"

namespace forgeshape {

// What Finish turns a Surface sketch into.
enum class SurfaceCreateKind : uint8_t {
    Patch = 1,    // the sketch's closed regions (holes kept) as planar patches
    Extrude = 2,  // its curves -- open chains valid -- swept along the normal
    Revolve = 3,  // its curves swept about its ONE Construction straight edge
    Loft = 4,     // the body's pending section to this sketch's curves
    Trim = 5,     // the latest coplanar planar patch clipped by its regions
    Section = 6,  // retained only: the first section of a later Loft
};

constexpr int kSurfaceCreateKindCount = 6;
const char* surfaceCreateKindName(SurfaceCreateKind kind);
bool surfaceCreateKindFromCode(int code, SurfaceCreateKind* out);

// The values a Finish carries. Each kind reads its own.
struct SurfaceCreateRequest {
    SurfaceCreateKind kind = SurfaceCreateKind::Patch;
    double distance = 0.5;        // Extrude, metres along the sketch normal
    double angleDegrees = 360.0;  // Revolve
    bool keepInside = false;      // Trim: keep the region's inside, or cut it out
};

// The body a Surface sketch commits to; `body == kNoObject` makes a new body
// (or, with no project open, the first project).
struct SurfaceSketchPurpose {
    bool active = false;
    ObjectId body = kNoObject;
};

SurfaceSketchPurpose& surfaceSketchPurpose();

// The selection that counts as a choice of curves: what the user selected
// with the Select tool, never the entity a drawing tool just placed.
std::vector<SketchEntityId> surfaceChosenCurves(const SketchSession& sketch);

// The curves a sweep uses: the chosen selection when there is one, else
// every Regular entity -- never a Construction one, which is reference
// geometry (a Revolve's axis among it).
std::vector<SketchEntityId> surfaceSweepCurves(const CadSketch& sketch,
                                               const std::vector<SketchEntityId>& selected);

// The Revolve axis: the sketch's ONE Construction straight edge (a Line, or a
// single-segment open Polyline). None, or more than one, is
// `AxisUnresolved` -- nothing guesses which line the user meant.
SurfaceStatus surfaceRevolveAxis(const CadSketch& sketch, CadSketchEdgeRef* out);

// The latest retained sketch no feature consumes: the first section a Loft
// starts from. 0 when there is none.
uint32_t surfacePendingSection(const SurfaceBodyState& state);

// The candidate state a Finish would commit: `base` (null for a new body) plus
// the sketch record and the feature. Regenerated whole; `report` names the
// first failing feature. Writes `out` on Ok only.
SurfaceStatus surfaceCandidateFromSketch(const SurfaceBodyState* base, const CadSketch& sketch, double offset,
                                         const std::vector<SketchEntityId>& selected,
                                         const SurfaceCreateRequest& request, SurfaceBodyState* out,
                                         SurfaceRegenerationReport* report = nullptr);

// THE Finish of a Surface sketch. With no project open it CREATES the first
// project through `loadProjectDocument` (the same all-or-nothing path the CAD
// bootstrap takes, so the project starts with an empty history); otherwise it
// is ONE `ScopedConstructionEdit` around one `addSurfaceBody` or one
// `applyState`. On Ok the sketch session is Inactive and the purpose cleared;
// on any refusal nothing changes and the sketch stands.
SurfaceStatus surfaceCommitSketch(SketchSession& sketch, ConstructionScene& scene, SculptSession& sculpt,
                                  ConstructionHistory& history, const SurfaceCreateRequest& request,
                                  ObjectId* outBody = nullptr, SurfaceRegenerationReport* report = nullptr);

// The features whose patches are live (drawn, not yet consumed), ascending.
std::vector<SurfaceFeatureId> surfaceLiveFeatures(const SurfaceBodyMesh& mesh);

// Stitch every live feature (at least two). The candidate, then the commit.
SurfaceStatus surfaceStitchCandidate(const SurfaceBody& body, SurfaceBodyState* out,
                                     SurfaceRegenerationReport* report = nullptr);
SurfaceStatus surfaceStitch(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                            SurfaceRegenerationReport* report = nullptr);

// Thicken one live feature's patches by a signed thickness.
SurfaceStatus surfaceThickenCandidate(const SurfaceBody& body, SurfaceFeatureId source, double thickness,
                                      SurfaceBodyState* out, SurfaceRegenerationReport* report = nullptr);
SurfaceStatus surfaceThicken(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                             SurfaceFeatureId source, double thickness,
                             SurfaceRegenerationReport* report = nullptr);

// The ONE typed value an edit changes. A feature: an Extrude's distance, a
// Revolve's angle, a Thicken's thickness. A sketch: its offset along its
// plane's normal. Anything else has no value (`PayloadMismatch`).
enum class SurfaceValueTarget : uint8_t { Feature = 0, Sketch = 1 };

bool surfaceFeatureHasValue(SurfaceFeatureKind kind);
bool surfaceValueOf(const SurfaceBodyState& state, SurfaceValueTarget target, uint32_t id, double* out);
SurfaceStatus surfaceStateWithValue(const SurfaceBodyState& state, SurfaceValueTarget target, uint32_t id,
                                    double value, SurfaceBodyState* out);
SurfaceStatus surfaceApplyValue(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                                SurfaceValueTarget target, uint32_t id, double value,
                                SurfaceRegenerationReport* report = nullptr);

// The History surface's rows for a Surface body: every retained sketch and
// every feature, interleaved in construction order (a sketch just before the
// first feature that reads it; a section no feature reads yet after the
// last), derived on every read and stored nowhere.
enum class SurfaceTimelineRowState : uint8_t { Ok = 0, Failed = 1, NotRegenerated = 2, Unused = 4 };

struct SurfaceTimelineRow {
    bool feature = false;
    uint32_t id = 0;            // sketch id or feature id: THE identity
    uint32_t sketchId = 0;      // a feature's (first) sketch; a sketch's own id
    uint32_t ordinal = 0;       // 1-based among rows of its kind
    SurfaceTimelineRowState state = SurfaceTimelineRowState::Ok;
    SurfaceStatus status = SurfaceStatus::Ok;
    bool editing = false;
    bool hasValue = false;      // whether a tap edits a value
    double value = 0.0;         // distance, angle, thickness or offset
    SurfaceFeatureKind kind = SurfaceFeatureKind::PlanarPatch;
    uint32_t entityCount = 0;   // a sketch row
    Workplane plane = Workplane::XY;
};

struct SurfaceTimeline {
    SurfaceStatus status = SurfaceStatus::Ok;
    SurfaceFeatureId failedFeature = kNoSurfaceFeature;
    std::vector<SurfaceTimelineRow> rows;
};

SurfaceTimeline buildSurfaceTimeline(const SurfaceBodyState& state, const SurfaceRegenerationReport& report,
                                     bool editingSet = false, SurfaceValueTarget editTarget = SurfaceValueTarget::Feature,
                                     uint32_t editId = 0);

}  // namespace forgeshape
