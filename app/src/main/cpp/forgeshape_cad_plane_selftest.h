// Checks of a construction plane off a face (`CAD-PLANE-R1`): a Cut drawn on
// a plane offset above the face and carried by an upstream size edit; a plane
// tilted off the face and its frame; every refusal by name; the `CADB` v14
// round trip, its version rule and the fingerprint; and the session -- the
// plane offered on a face, the authoring frame moving with it, out-of-range
// values refused, and the commit writing the plane. Run inside the
// CAD-feature suite. Debug-only.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadPlaneSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

}  // namespace forgeshape
