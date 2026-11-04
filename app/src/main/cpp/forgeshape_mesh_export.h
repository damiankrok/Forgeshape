// Exporting the current project as a mesh file in one of several interchange
// formats: GLB, STL, OBJ, PLY and 3MF.
//
// ONE SNAPSHOT, FIVE ENCODINGS. Every format is written from the snapshot the
// GLB exporter already takes (`captureGlbExportScene`): the current effective
// geometry of EVERY body, its linear placement `L` baked in and its world
// translation beside it. So a body exports the same shape, at the same place,
// whichever format the user picks -- a second capture path would be a second
// answer to what the project looks like. `Glb` is not re-implemented here at
// all: it delegates to `exportSceneAsGlb`, byte for byte.
//
// ONE FILE. Each format is a single self-contained file: OBJ writes no `.mtl`,
// 3MF carries its parts inside its own ZIP, and nothing writes a sidecar. Every
// export is a READ of the project: no revision, no history step, no ObjectId.
//
// UNITS AND AXES. ForgeShape is metric, right-handed and +Y up, and NO axis is
// converted for any format. The unit is stated per format, because the formats
// disagree about whether they have one:
//
//   * GLB  -- metres (glTF's unit).
//   * STL  -- MILLIMETRES. STL has no unit field; every slicer reads it as mm,
//             so 1 m is written as 1000.
//   * OBJ  -- metres. OBJ has no unit field either, and the DCC tools that read
//             it (Blender among them) default to metres.
//   * PLY  -- metres, for the same reason.
//   * 3MF  -- MILLIMETRES, declared in the model's `unit` attribute, which is
//             the 3MF default and what every slicer expects.
//
// A slicer treats +Z as up, so a ForgeShape part arrives lying on its back;
// rotating it on the build plate is the slicer's ordinary job, and rotating it
// here would be an axis conversion nothing else in the product performs.
//
// THE PRINTABLE TRIANGLE SET. The four non-GLB encoders write exactly the same
// triangles: per body, the FIRST triangle of the snapshot on each distinct set
// of three DIFFERENT positions (exact values), in its own order and winding. A
// later triangle on the same three positions is the same facet restated or its
// reversed twin, and is not written. The twin rule matters because an Imported
// Mesh's double-sided submesh reaches the snapshot as its triangles plus their
// reversed twins (`ImportedMesh::buildDrawData`); none of these formats can
// say "double-sided", and two coincident opposite facets are a non-manifold
// defect every slicer reports. GLB keeps the snapshot untouched.
//
// WELDING. STL has no shared vertices to weld. OBJ and 3MF write the body's
// distinct positions once (OBJ keeps the render mesh's per-corner normals as
// `vn`), so a closed body arrives in a slicer watertight; PLY carries one
// normal per vertex and so writes the render mesh's own split vertices.
//
// DETERMINISTIC. The same project always produces byte-identical bytes: fixed
// key and element order, one fixed number format (the shortest text that
// round-trips a float32, written without consulting the C locale, so a decimal
// comma can never reach a file), fixed ZIP timestamps, no clock, no host name.
//
// Platform-neutral C++17: no Android, no JNI, no filesystem, no `Uri`.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_gltf_export.h"
#include "forgeshape_object_id.h"
#include "forgeshape_project_document.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// The formats a project can be exported as. APPEND-ONLY: the ordinal crosses
// JNI and the desktop shell, so a new format takes the next number and no
// existing one ever moves.
enum class MeshExportFormat : uint8_t {
    Glb,
    Stl,
    Obj,
    Ply,
    ThreeMf,
};

// How many formats exist, for a boundary that receives an ordinal and has to
// range-check it before casting.
constexpr int kMeshExportFormatCount = 5;

// The stable lower-case token ("glb", "stl", "obj", "ply", "3mf").
const char* meshExportFormatName(MeshExportFormat format);
// The file extension, with its dot (".glb", ".stl", ".obj", ".ply", ".3mf").
const char* meshExportFileExtension(MeshExportFormat format);
// The media type a document picker is handed for the new file.
const char* meshExportMimeType(MeshExportFormat format);

// One body of a non-GLB export, in WORLD space: the snapshot's baked positions
// with the body's translation added, so every format writes the place the
// viewport draws. Metres; an encoder that writes millimetres scales on the way
// out.
struct MeshExportObject {
    ObjectId objectId = kNoObject;
    // The body's own name as the domain stored it (already sanitized UTF-8),
    // or "Body_<id>" -- the GLB exporter's technical name -- when it has none.
    // Each encoder narrows it further to what its format can carry.
    std::string name;
    std::vector<float> positions;  // xyz triples, world metres
    std::vector<float> normals;    // xyz triples, world, unit (or zero)
    std::vector<uint32_t> indices; // triangles
};

struct MeshExportSnapshot {
    std::vector<MeshExportObject> objects;
};

// Reads the project into a world-space snapshot through `captureGlbExportScene`,
// so it refuses exactly what the GLB export refuses and for the same reasons.
GlbExportStatus captureMeshExportSnapshot(const ConstructionScene& scene, ProjectKind kind,
                                          MeshExportSnapshot* out);

// The encoders. Each returns the whole file, or empty with the reason in
// `outWhy`; a refusal never produces a partial file. Each re-validates its
// input (finite values, indices in range, whole triangles) because a snapshot
// may be built by hand, and refuses with `InvalidMesh` when a body has no
// triangle left in the printable set, `TooLarge` past `kMaxGlbBytes`.
std::vector<uint8_t> encodeMeshExportStl(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy = nullptr);
std::vector<uint8_t> encodeMeshExportObj(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy = nullptr);
std::vector<uint8_t> encodeMeshExportPly(const MeshExportSnapshot& snapshot,
                                         GlbExportStatus* outWhy = nullptr);
std::vector<uint8_t> encodeMeshExportThreeMf(const MeshExportSnapshot& snapshot,
                                             GlbExportStatus* outWhy = nullptr);

// The whole export in one call. `Glb` is `exportSceneAsGlb`, unchanged.
std::vector<uint8_t> exportSceneAs(MeshExportFormat format, const ConstructionScene& scene,
                                   ProjectKind kind, GlbExportStatus* outWhy = nullptr);

}  // namespace forgeshape
