// Reading mesh files of several interchange formats -- GLB, STL, OBJ, PLY and
// 3MF -- into the ONE parsed-scene shape the durable import already commits.
//
// THE SAME DESTINATION. Every reader here produces a `ParsedGlbScene`
// (`forgeshape_gltf_import.h`): world-space positions, generated normals, index
// triangles and one batch per mesh. So `commitImportedGlbScene`, its single
// Undo, the `IMPT` record in `.forge` and the Sculpt seed all work on a body
// that came from an STL exactly as on one that came from a GLB, and none of
// them learns a second input shape. `Glb` is not re-read here: it delegates to
// `importGlb`, the one GLB parser.
//
// WHAT EVERY NON-GLB READER DOES, the same way:
//   * produces GEOMETRY, never a body, an ObjectId, a history step or a byte
//     of `.forge`;
//   * converts the file's unit to metres and converts NO axis (ForgeShape is
//     right-handed and +Y up; a file is taken as it states its coordinates);
//   * leaves the parsed mesh at the identity node transform with a zero
//     translation, so nothing is recentred and the body's origin is the
//     file's origin -- the rule GLB follows for a node at the origin;
//   * drops triangles with no area (two corners on one vertex, or a cross
//     product that is exactly zero), then keeps only the vertices a remaining
//     triangle uses, in first-use order;
//   * GENERATES the normals, area-weighted per vertex from the positions -- the
//     GLB reader's rule for a mesh that states none -- and refuses
//     (`CannotGenerateNormals`) a vertex whose incident faces cancel exactly,
//     rather than inventing a direction for it;
//   * writes `out` only on Ok, and bounds-checks every count, offset and index
//     against the bytes actually given before it is used.
//
// PER FORMAT:
//   * STL (binary and ASCII): MILLIMETRES. Each facet is three loose corners,
//     so corners are WELDED by exact position into one connected mesh -- the
//     only way an STL body can later be sculpted without tearing. The facet
//     normal the file states is not read. An ASCII file may hold several
//     `solid` blocks; each is one mesh named by its `solid` line.
//   * OBJ: metres. `v` (an optional w or vertex colour ignored), `f` with
//     `v`, `v/vt`, `v//vn` and `v/vt/vn` corners, negative (relative) indices,
//     polygons fan-triangulated. Meshes are split by `o`; when a file has NO
//     `o` at all, `g` splits them instead, so a file is cut by one rule and
//     never by both. `vt` and `vn` are ignored (a face's references to them
//     are still range-checked; the normals are regenerated), and so are
//     `usemtl`, `mtllib`, `s`, `l`, `p` and the display attributes. `vp` and
//     every other free-form statement (`cstype`, `curv`, `surf`...) describes
//     geometry this reader does not build and is refused by name
//     (`UnsupportedObjFeature`), as is any statement it does not know. A
//     file's topology is its own: vertices are re-indexed, never welded.
//   * PLY: metres. ascii, binary_little_endian and binary_big_endian 1.0;
//     `vertex` x/y/z of any scalar type; `face` `vertex_indices` or
//     `vertex_index` lists of any integer types, fan-triangulated; every other
//     property and element skipped by its declared type. Never welded.
//   * 3MF: the package's ZIP is read here (STORED and DEFLATE, the inflater is
//     this file's own); the model part is the `_rels/.rels` 3D-model target,
//     else `3D/3dmodel.model`. The `unit` (micron, millimeter -- the default --,
//     centimeter, inch, foot, meter) becomes metres. ONE mesh per build `item`,
//     named by its object's `name`, its item transform baked in (a mirroring
//     one has its winding corrected, as GLB's node bake does). An object built
//     of `<components>` is resolved recursively within the model part (each
//     component transform composed, depth bounded, a cycle refused) and its
//     parts merged into the item's one mesh. An object no build item reaches
//     is not in the build and is not imported; elements of any other
//     vocabulary (materials, slicer metadata) are skipped whole. Refused by
//     name: a REQUIRED extension other than Materials (appearance, which no
//     import keeps) and Production (identity), a component or item in another
//     part (`p:path`), and an unknown unit.
//   * A PLY face list of fewer than three corners is an edge or a point and
//     carries no surface; it is skipped, like OBJ's `l` and `p`. An OBJ `f`
//     IS a polygon by definition, so one with fewer than three corners is
//     `MalformedText`.
//
// Platform-neutral C++17: no Android, no JNI, no filesystem, no `Uri`.
#pragma once

#include <cstddef>
#include <cstdint>

#include "forgeshape_gltf_import.h"

namespace forgeshape {

// The formats a mesh file may be read as. APPEND-ONLY: the ordinal crosses JNI
// and the desktop shell.
enum class MeshImportFormat : uint8_t {
    Unknown,
    Glb,
    Stl,
    Obj,
    Ply,
    ThreeMf,
};

constexpr int kMeshImportFormatCount = 6;

// The stable lower-case token ("unknown", "glb", "stl", "obj", "ply", "3mf").
const char* meshImportFormatName(MeshImportFormat format);

// Which format the bytes are, from the bytes.
//
// Content decides: the GLB magic "glTF"; a ZIP's "PK\3\4" (the only ZIP this
// reads is 3MF); a "ply" first line; the binary-STL length rule
// (84 + 50 x the count at offset 80); an ASCII STL's leading "solid" with a
// "facet" after it; an OBJ by its line grammar. A binary STL whose free-form
// header happens to begin with "solid" is still an STL -- the STL reader then
// tells the two kinds apart. `extensionHint` (lower case with its dot, ".stl";
// may be null or empty) only breaks ties: it picks between two sniffs that both
// matched, and names the format when no sniff matched so that reader can say
// precisely what is wrong. Nothing matched and no usable hint: `Unknown`.
MeshImportFormat detectMeshImportFormat(const uint8_t* bytes, size_t length,
                                        const char* extensionHint);

// Reads `bytes` as `format` into `out`, or reports why it would not.
//
// `Unknown` is refused as `UnknownFormat`; `Glb` is `importGlb`. Nothing
// partial is produced: `out` is written only on Ok.
GlbImportStatus importMeshFile(const uint8_t* bytes, size_t length, MeshImportFormat format,
                               ParsedGlbScene* out);

}  // namespace forgeshape
