// Checks of retained-sketch management and parametric hardening (`CAD-R2`):
// the sketch inventory, Keep Sketch, the sketch-only edit, reuse of a sketch by
// a later Add or Cut, Delete Sketch, staged feature edits and first-failure
// reporting, planar-face selections of crossing / gridded / point-touching
// cells, face-support lifecycles, and persistence of everything above. Run
// inside the CAD-feature suite, so no new startup token exists. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the `R2_SK_*`, `R2_HIST_*`, `R2_PF_*`, `R2_SUP_*` and `R2_DATA_*`
// checks and writes their bounded timing (`cad_r2_...` tokens) to
// `performance`.
void runCadR2SelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
