// Debug-only self-tests for the retained sketch table, the selection variant
// and `CADB` v6 (`CAD-V6-S1`).
//
// Model capability and persistence only: nothing here is wired to the session,
// JNI or the UI, so -- like the planar arrangement before it -- it rides inside
// the CAD_FEATURE suite rather than adding a startup token of its own. Each
// case builds the states and documents it needs; nothing reads process state.
#pragma once

#include <string>
#include <vector>

namespace forgeshape {

struct CadV6SelfTestCheck {
    const char* name = nullptr;  // a string literal: the suite keeps the pointer
    bool passed = false;
};

// `digests` receives "name=sha256 ..." for every v6 fixture the production
// encoder reproduces, for the evidence line.
void runCadV6SelfTests(std::vector<CadV6SelfTestCheck>* out, std::string* digests);

}  // namespace forgeshape
