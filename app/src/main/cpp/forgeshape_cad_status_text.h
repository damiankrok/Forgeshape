// What a person reads about a CAD refusal (`WORKBENCH-UX-R1`): one sentence
// per CadStatus, the same words on every shell. The Android shell keeps its
// own string resources (localisable); this table is the core's, for the
// desktop and for any surface that has no resource system, and the two are
// kept in step by a host check that walks every code. The exact reason is
// still in the log by name (cadStatusName); this is never parsed.
#pragma once

#include "forgeshape_sketch.h"

namespace forgeshape {

// A short sentence for `status`, ending in a full stop; never null, never the
// enum's name.
const char* cadStatusUserText(CadStatus status);

}  // namespace forgeshape
