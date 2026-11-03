// Checks of the Through All extent (`CAD-EXTENT-R1`): the derived distance on
// a block, after an upstream size edit and from a boss; every refusal by name;
// the pure transitions; the `CADB` v12 round trip and its version rule; and the
// session -- offered only for a Cut, the arrow reading the derived reach, a
// drag setting a depth that a second finger takes back, and a commit that goes
// through. Run inside the CAD-feature suite. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadThroughAllSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
