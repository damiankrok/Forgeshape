// Checks of a later Revolve Add or Cut (`CAD-REVOLVE-FEATURE-R1`): a ring
// revolved on a block's top about a Construction axis lying in the face adds
// (or removes) exactly its half above (or below) the face; one payload per
// feature; a pattern of a Revolve refused; the `CADB` v15 round trip, its
// version rule and the fingerprint; and the session offering Add and Cut for
// a revolve on a part's face and committing one. Run inside the CAD-feature
// suite. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadRevolveFeatureSelfTests(std::vector<ArrangementSelfTestCheck>* out,
                                   std::string* performance);

}  // namespace forgeshape
