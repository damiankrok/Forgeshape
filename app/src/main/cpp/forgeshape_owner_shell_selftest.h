// Checks of the OWNER shell/sketch correction (`MODELING-R1-OWNER-CORRECTION`):
// New Sketch on the planar faces of a committed CAD body -- a base cap and
// side, an Add's cap, a Cut's floor and straight wall -- through the real
// support chooser and sketch session, and the OWNER-like fill selection whose
// union touches itself at a node, now split there rather than refused. Run
// inside the CAD-feature suite beside MULTIFACE, so no new startup token
// exists. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the `OSS_...` checks and writes their bounded single-shot timing
// (`oss_...` tokens) to `performance`.
void runOwnerShellSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
