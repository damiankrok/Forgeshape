// Checks of Linear, Circular and Mirror feature patterns (`CAD-PATTERN-R1`):
// the instance transforms, every refusal by name, the exact removed and added
// volumes, instance faces never becoming supports, the chain order by id, the
// `CADB` v10 round trip and its refusals, the fingerprint, the three acts as
// one Undo each, and the History rows. Run inside the CAD-feature suite, so no
// new startup token exists. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadPatternSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
