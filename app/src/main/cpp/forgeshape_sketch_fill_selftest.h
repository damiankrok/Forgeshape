// Checks of the fill-bucket correction (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`):
// every bounded cell a sketch's curves make -- splines included -- is one
// planar face, and Finish never hands a sketch whose loops cross to the loop
// model. Run inside the CAD-feature suite, beside the arrangement's own checks,
// so no new startup token exists. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends FILL-01..13 and the mode-decision checks, and writes the bounded
// spline-arrangement timing (`fill_spline_us=median/max over N`).
void runSketchFillSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
