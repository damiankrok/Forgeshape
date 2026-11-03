// Checks of the Parametric History (`MODELING-FOUNDATIONS-R1` A): the derived
// timeline over a CAD Body's feature chain, upstream edits staged through the
// real edit session and regenerated in order, the first failing downstream
// feature attributed by id, Cancel and Undo/Redo exact, persistence and the
// legacy `CADB` layouts. PAR-01..15. Run inside the CAD-feature suite beside
// the chain it reads, so no new startup token exists. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the PAR checks and writes the bounded chain timings
// (`parametric_...` tokens) to `performance`.
void runCadTimelineSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
