// Checks of the sketch drafting toolkit (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`):
// the Construction role, retained sketch dimensions, the snap priority and
// inference guides, Trim / Extend / Offset / Mirror, the session's modify
// modes and multi-selection, and `CADB` v8. Run inside the CAD-feature suite
// beside the other CAD model checks, so no new startup token exists.
// Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the DR checks and writes their bounded single-shot timings
// (`drafting_...` tokens) to `performance`.
void runSketchDraftingSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                std::string* performance);

// The digests of the five `CADB` v8 fixtures as this build encodes them, for
// the evidence line (`name=sha256 ...`).
std::string sketchDraftingFixtureDigests();

}  // namespace forgeshape
