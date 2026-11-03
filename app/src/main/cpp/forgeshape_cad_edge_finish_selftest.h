// Checks of Chamfer and Fillet (`CAD-EDGE-FINISH-R1`): the edges a feature's
// own extrusion exposes and which of them may be finished, the exact removed
// volume of a Chamfer and of a Fillet, a whole face's loop in one finish, a
// boss edge that stops flush on the body it stands on, every refusal by name,
// upstream size edits that carry a finish and structural ones that refuse it,
// and the `CADB` v9 round trip. Run inside the CAD-feature suite, so no new
// startup token exists. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadEdgeFinishSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                               std::string* performance);

}  // namespace forgeshape
