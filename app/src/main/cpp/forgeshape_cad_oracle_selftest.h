// Independent geometry oracles for CAD regeneration (`CAD-R2` P3).
//
// Every expected value here is an ANALYTIC formula written from the shape's
// definition (box volumes and areas, Pappus, polygon areas), and every measured
// value is computed from the published float render mesh by functions written
// in this file -- never the model's own `CadBodyMesh::volume`,
// `cadSolidVolume`, the arrangement's face areas or any regeneration helper.
// Two tolerance contracts are kept apart:
//   * EXACT polygonal solids (every vertex on the authored geometry): bounded by
//     float32 rounding of the published positions -- relative 1e-5;
//   * CHORDAL solids (a circle or a revolve tessellated into N chords): the
//     analytic pi-based value bounds the mesh from above, and the gap may not
//     exceed the inscribed-polygon deficit 1 - sin(2*pi/N)/(2*pi/N) derived
//     from the product's own segment count.
// Run inside the CAD-feature suite (no new startup token). Each check is named
// `R2_OR_*`; a failure prints one `CADR2_ORACLE_FAIL` line with the expected
// and measured values, and the suite's performance line carries
// `CAD_R2_ORACLE_PASS=<N>`.
#pragma once

#include <string>
#include <vector>

#include "forgeshape_sketch_arrangement_selftest.h"

namespace forgeshape {

void runCadOracleSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* report);

}  // namespace forgeshape
