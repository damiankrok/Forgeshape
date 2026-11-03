// Checks of the To Face extent and of a hole placed from two edges
// (`CAD-TO-FACE-R1`): the derived distance of an Add and a Cut, after an
// upstream size edit and down to the body's own bottom; every refusal by name;
// the candidates; the `CADB` v13 round trip, its version rule and the
// fingerprint; the edge-placed hole's seed, drill, follow and refusals; and the
// session -- offered for an Add or a Cut, the nearest face chosen, a tapped
// face retargeting, a drag that a second finger takes back, and a commit.
// Run inside the CAD-feature suite. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadToFaceSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
