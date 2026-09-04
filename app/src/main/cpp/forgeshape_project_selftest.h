// Debug-only self-tests for the portable `.forge` project format (FSR1A-01..15).
//
// The codec is platform-neutral, so every case here is too: nothing in the
// implementation file names an Android, JNI, Vulkan or renderer type, and no
// case reads process-scoped state. Each builds the scene, session and history
// it needs, exactly as the scene, history and gizmo suites do, so a result never
// depends on what a live session or an earlier suite left behind.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_construction.h"

namespace forgeshape {

struct ProjectSelfTestResult {
    const char* name = nullptr;
    bool passed = false;
};

int runProjectSelfTests(ProjectSelfTestResult* out, int maxOut);

// The SHA-256 of the two canonical golden fixtures, as this build's encoder
// produces them, in lowercase hex.
//
// Reported alongside the suite so that a drift between the committed corpus and
// the encoder is a value a human can read out of logcat and compare against
// DATA_PACKAGE_SPEC.md, rather than only a failed assertion. Returns a pointer
// to storage owned by the implementation; valid for the life of the process.
const char* canonicalConstructionFixtureSha256();
const char* canonicalSculptFixtureSha256();

// The same, for the three `IMPORT-01A` fixtures: an imported-only project, a
// Construction body beside an imported one, and all three branches at once.
const char* canonicalImportedOnlyFixtureSha256();
const char* canonicalConstructionImportedFixtureSha256();
const char* canonicalMixedImportedFixtureSha256();

// And for the two `IMPORT-01B` fixtures: an imported body carrying a Frozen
// Sculpt Mesh with no Construction branch at all, and one document holding all
// four valid source/sculpt combinations at once.
const char* canonicalImportedSculptFixtureSha256();
const char* canonicalMixedImportedSculptFixtureSha256();

// And for the four `CAD-R0-A1A2` fixtures: a rectangle extrusion with no
// Construction branch, a circle extrusion, a Construction body beside two CAD
// bodies, and the rectangle fixture with a corrupt workplane code that only
// the semantic check can refuse.
const char* canonicalCadRectangleFixtureSha256();
const char* canonicalCadCircleFixtureSha256();
const char* canonicalMixedCadFixtureSha256();
const char* canonicalCadBadPlaneFixtureSha256();

// And for the six `CAD-A3` CADB v2 fixtures: a dependent on a producer's far
// cap, one on a side face, a three-body chain, every representation beside a
// face-supported body, and two corrupt files -- a face reference that names
// no face, and a dependency cycle -- that only the semantic validation can
// refuse.
const char* canonicalCadFaceSketchCapFixtureSha256();
const char* canonicalCadFaceSketchSideFixtureSha256();
const char* canonicalCadFaceChainFixtureSha256();
const char* canonicalMixedCadFaceFixtureSha256();
const char* canonicalCadBadFaceRefFixtureSha256();
const char* canonicalCadDependencyCycleFixtureSha256();

// The SHA-256 of an arbitrary byte sequence, lowercase hex.
//
// Debug-only and shared: every suite that pins a corpus fixture needs this one
// function, and a second copy of a hash implementation is a second thing that
// could be subtly wrong while agreeing with itself. `SKETCH-UX-R1`'s suite
// pins the six `CADB` v3 fixtures through it.
std::string projectFixtureSha256Hex(const std::vector<uint8_t>& bytes);

// The canonical parameter set every corpus fixture's Construction Body carries:
// all six remembered dimension sets at the same values, with `kind` selecting
// which is active. Shared for the same reason the hash is -- a second copy is a
// second thing that could drift while agreeing with itself.
ConstructionObjectState canonicalCorpusShape(PrimitiveKind kind);

}  // namespace forgeshape
