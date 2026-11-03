// Checks of Revolve New Body (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`): the durable
// feature kind and its semantic axis ref, the derived solid (full turn and
// partial sweep, touching and crossing the axis, holes, several components),
// the production kernel's validity answer, the session's axis pick, angle,
// Flip and handle drag, commit / Undo / Redo / edit, and `CADB` v7. Run inside
// the CAD-feature suite beside the other CAD model checks, so no new startup
// token exists. Debug-only, like every suite.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

// Appends the REV checks and writes their bounded single-shot timings
// (`revolve_...` tokens) to `performance`.
void runCadRevolveSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance);

// The digests of the four `CADB` v7 fixtures as this build encodes them, for
// the evidence line (`name=sha256 ...`).
std::string cadRevolveFixtureDigests();

}  // namespace forgeshape
