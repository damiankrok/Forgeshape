// The `FRFM` section of a `.forge` project: one record per Freeform body,
// carrying its control cage and nothing derived (`MODELING-FOUNDATIONS-R1` B;
// DATA_PACKAGE_SPEC.md §7j).
//
// The cage IS the truth -- ids, binary64 positions, edge creases, face loops,
// the three high-water marks, the subdivision level and the symmetry planes --
// and the smooth surface is regenerated on load, exactly as a primitive's mesh
// is. No derived vertex, no triangle and no normal is ever written.
#pragma once

#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"

namespace forgeshape {

// Writes the whole `FRFM` payload, bodies in scene order.
void writeFreeformPayload(ByteWriter& out, const ProjectFreeformRecord& record);

// Reads a `FRFM` payload, every count bounded before anything is allocated.
// Structure only: `validateProjectDocument` holds each cage to the domain's own
// `validateFreeformCage`.
ProjectCodecStatus decodeFreeformPayload(ByteReader& in, ProjectFreeformRecord* record);

bool sameProjectFreeformRecord(const ProjectFreeformRecord& a, const ProjectFreeformRecord& b);

}  // namespace forgeshape
