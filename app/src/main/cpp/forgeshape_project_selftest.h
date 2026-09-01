// Debug-only self-tests for the portable `.forge` project format (FSR1A-01..15).
//
// The codec is platform-neutral, so every case here is too: nothing in the
// implementation file names an Android, JNI, Vulkan or renderer type, and no
// case reads process-scoped state. Each builds the scene, session and history
// it needs, exactly as the scene, history and gizmo suites do, so a result never
// depends on what a live session or an earlier suite left behind.
#pragma once

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

}  // namespace forgeshape
