// Checks of the large planar-face selection (`CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1`):
// a fill-bucket selection holds as many atomic faces as the derived arrangement
// can have, extrudes and persists at that size, and a non-empty selection is
// never reported as "choose one". Run inside the CAD-feature suite, beside the
// arrangement's own checks, so no new startup token exists. Debug-only, like
// every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the MULTIFACE checks and writes their bounded single-shot timings
// (`multiface_...` tokens) to `performance`.
void runCadMultiFaceSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
