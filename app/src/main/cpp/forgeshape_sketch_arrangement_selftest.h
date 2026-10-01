// Checks of the planar arrangement (`CAD-PLANAR-FACE-PF-S1`), run inside the
// CAD-feature suite so no new startup token or JNI runner exists for an
// engine nothing in the product consumes yet. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

namespace forgeshape {

struct ArrangementSelfTestCheck {
    const char* name = nullptr;  // a string literal: the suite keeps the pointer
    bool passed = false;
};

// Appends every PF-S1 check and writes the bounded cap-sketch timing
// (`arrangement_cap_us=median/max over N`).
void runSketchArrangementSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                   std::string* performance);

}  // namespace forgeshape
