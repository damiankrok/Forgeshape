// Checks of the three sketch-ergonomics changes of `SKETCH-KNIFE-UNDO-R1`:
// pushing a face sketch's extrude arrow THROUGH its plane switches it to a Cut
// (and back), the staged sketch's own Undo / Redo while it is being drawn, and
// the knife -- the Trim tool on the rail, whose swipe removes every interval it
// crosses. Driven through real Down / Move / Up samples where a finger is the
// subject. Run inside the CAD-feature suite, so no new startup token exists.
// Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runSketchKnifeUndoSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                 std::string* performance);

}  // namespace forgeshape
