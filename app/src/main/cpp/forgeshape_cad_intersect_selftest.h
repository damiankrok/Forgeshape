// Checks of the Intersect operation (`CAD-INTERSECT-R1`): a later feature that
// keeps only the material its tool shares with the body; the two refusals by
// name; a pattern of one refused; the `CADB` v16 round trip, its version rule
// and the fingerprint; and the session offering it on a face sketch, growing
// it into the body and committing it as one step. Run inside the CAD-feature
// suite. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadIntersectSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
