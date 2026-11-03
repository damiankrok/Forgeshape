// Checks of Holes (`CAD-HOLE-R1`): the exact removed volume of a Simple, a
// Counterbore and a Countersink hole, blind and Through All, on a cap, a side,
// a boss and a pocket floor; Through All following an upstream size edit;
// every refusal by name; the chain order by id; hole faces never becoming
// supports; the `CADB` v11 round trip and its refusals; the three acts as one
// Undo each; the History row; and the editor's seeds. Run inside the
// CAD-feature suite, so no new startup token exists. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadHoleSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
