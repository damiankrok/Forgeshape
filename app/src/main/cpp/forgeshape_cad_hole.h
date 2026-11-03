// Holes drilled into a CAD Body's planar faces (`CAD-HOLE-R1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no camera. The
// authored truth is `CadBodyState::holes` (forgeshape_cad_body.h); this module
// derives everything else from it -- where a hole stands, its tool solid, and
// whether a list of holes is valid -- and owns the three user acts.
// Regeneration (`regenerateCadBody`) applies each tool through the one kernel
// seam as a Difference.
#pragma once

#include <cstdint>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_kernel.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// The holes' own rules, without the kernel: ids in the shared id space,
// bounds, each kind's canonical form, and every support resolving to a face a
// sketch may stand on of an earlier feature at the stored lineage.
// `outFailedId` names the hole that failed.
CadStatus validateCadFeatureHoles(const CadBodyState& state, uint32_t* outFailedId = nullptr);

// One hole's own values, without its support: what the editor's fields are
// judged by before anything else is derived.
CadStatus validateCadHoleValues(const CadFeatureHole& hole);

// Where `hole` stands: its support face's material-outward frame, moved to
// the hole's point, so `origin` is the axis point on the face and `-n` is the
// drilling direction. False when the support does not resolve.
bool cadHoleFrame(const std::vector<CadFeatureGeometry>& chain, const CadFeatureHole& hole,
                  CadFrame64* out);
bool cadHoleFrame(const CadBodyState& state, const CadFeatureHole& hole, CadFrame64* out);

// The hole's tool: a closed, outward-wound solid of revolution about the axis
// -- the bore, and the counterbore or the countersink above it -- at the
// circle's density, starting a little ABOVE the face so the kernel sees a clean
// opening, every triangle tagged `tag`. Its depth is the blind depth, or for
// Through All how far `body` reaches below the face plus the same margin.
// `HoleNoEffect` when Through All finds no material below the face.
CadStatus buildCadHoleTool(const CadFeatureHole& hole, const CadFrame64& frame,
                           const CadSolid& body, uint32_t tag, CadSolid* out);

// A readable default diameter for a new hole in `state`: a fifth of the base
// solid's smallest extent, rounded UP to a 1/2/5 x 10^k value. Zero when the
// base does not derive.
double suggestCadHoleDiameter(const CadBodyState& state);

// `value` rounded to the nearest multiple of the grid a new hole of
// `diameter` is placed on: a 1/2/5 x 10^k step at or above a quarter of the
// diameter. A tap lands the hole on a round number the user can read.
double snapCadHolePosition(double value, double diameter);

// --- the three user acts, each ONE `ScopedConstructionEdit` (one Undo) -----
//
// Each judges the whole requested state through `CadBody::applyState`, which
// validates and regenerates it and writes nothing unless all of it passes.

CadStatus addCadFeatureHoleToBody(ConstructionScene& scene, ConstructionHistory& history,
                                  ObjectId bodyId, CadFeatureHole hole,
                                  uint32_t* outHoleId = nullptr);

// Replaces hole `holeId`'s values; its id and its support stay.
CadStatus editCadFeatureHoleOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                   ObjectId bodyId, uint32_t holeId, CadFeatureHole hole);

CadStatus deleteCadFeatureHoleOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                     ObjectId bodyId, uint32_t holeId);

}  // namespace forgeshape
