// The `SURF` section of a `.forge` project: one record per Surface body,
// carrying its retained sketches and its ordered feature list and nothing
// derived (`MODELING-FOUNDATIONS-R1` C; DATA_PACKAGE_SPEC.md §7k).
//
// Patches, boundary edges, stitches and thickened solids are regenerated on
// load by `regenerateSurfaceBody`, exactly as a CAD body's mesh is; no
// triangle, vertex or normal is ever written. A sketch is written in the CADB
// v8 entity and dimension grammar, so one sketch has one encoding whichever
// section retains it.
#pragma once

#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"

namespace forgeshape {

// Writes the whole `SURF` payload, bodies in scene order.
void writeSurfacePayload(ByteWriter& out, const ProjectSurfaceRecord& record);

// Reads a `SURF` payload, every count bounded before anything is allocated.
// Structure only: `validateProjectDocument` holds each body to the domain's
// own rules and regenerates it.
ProjectCodecStatus decodeSurfacePayload(ByteReader& in, ProjectSurfaceRecord* record);

bool sameProjectSurfaceRecord(const ProjectSurfaceRecord& a, const ProjectSurfaceRecord& b);

}  // namespace forgeshape
