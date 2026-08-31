// Early GLB 2.0 export — the current project as one binary glTF file.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no renderer, no
// filesystem and no `Uri`. It consumes a read-only snapshot of domain truth and
// produces bytes. Where those bytes go is the Android adapter's problem, exactly
// as it is for `.forge`.
//
// WHY NO AXIS CONVERSION
// ----------------------
// ForgeShape and glTF 2.0 already agree on every convention this file depends
// on, so the export applies no root transform, no axis swap and no scale
// factor. Each fact below is repository truth, cited where it is defined:
//
//   * LENGTH — metres. `forgeshape_transform.h` names position `Meters` and the
//     Construction domain authors dimensions in metres. glTF's unit is the
//     metre. So 1.0 exports as 1.0; there is deliberately no x1000 anywhere.
//   * UP and HANDEDNESS — right-handed, +Y up. Stated as THE definition in
//     `forgeshape_transform.h` ("Right-handed world space with +Y up,
//     column-vector math"), repeated in `forgeshape_math.h`, and relied on by
//     `forgeshape_grid.h` ("the world XZ plane at y = 0, in a +Y-up
//     right-handed world"). glTF 2.0 is right-handed, +Y up.
//   * MATRIX LAYOUT — `Mat4` is column-major, `m[column * 4 + row]`, "laid out
//     exactly as GLSL expects it". glTF requires node matrices in exactly that
//     column-major order, so the sixteen floats are copied straight out.
//   * WINDING — counter-clockwise viewed from outside the surface, with the
//     geometric normal `(v1 - v0) x (v2 - v0)` pointing away from the solid
//     (ARCHITECTURE.md, "Canonical winding and culling"; the pipeline uses
//     `VK_FRONT_FACE_COUNTER_CLOCKWISE`). glTF's front face is counter-clockwise
//     too, so the index buffer is copied unreversed.
//
// A conversion node would therefore be a bug, not a safety net: it would rotate
// or mirror geometry that already lines up.
//
// WHAT IS EXPORTED
// ----------------
// The CURRENT representation of each body, evaluated now — never a decoded
// `.forge` file and never a GPU buffer. Construction bodies are re-evaluated
// through `ConstructionObject::generateMesh()`, the same generator the product
// publishes from. A Sculpt project exports each sculpted body's Frozen Sculpt
// Mesh, because that is what the user is working on; falling back to the
// Construction shape there would silently export something they did not make.
//
// Vertices stay in BODY-LOCAL space and the placement travels as the node's
// matrix. Nothing is recentred, nothing is baked across bodies, and the pivot a
// body is drawn and rotated about is the pivot it exports with.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_project_document.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// The `generator` string written into the asset block.
//
// Names the product and nothing else. No device model, no user, no path, no
// build host: an exported file is something a user may hand to somebody, and a
// generator string is a place identifying data leaks into one by accident.
constexpr const char* kGlbGenerator = "ForgeShape";

// Why an export produced no file. Every one is a refusal, and a refusal writes
// nothing at all rather than a partial or corrupt `.glb`.
enum class GlbExportStatus {
    Ok,
    // The scene has no body that can be exported.
    NothingToExport,
    // A body's current representation is not a usable triangle mesh. Fails the
    // same `validateMeshData` contract `RuntimeMesh` uses.
    InvalidMesh,
    // A position, a normal or a placement value is not finite. Never written:
    // a NaN in an exported file is a corrupt file that looks valid.
    NonFiniteValue,
    // The export would exceed the byte ceiling below.
    TooLarge,
};

const char* glbExportStatusName(GlbExportStatus status);

// A hard ceiling on the file this module will build, so every size computation
// is provably free of overflow. Far above anything the editor can produce; its
// job is to refuse an impossible request before allocating for it.
constexpr uint64_t kMaxGlbBytes = 512ull * 1024ull * 1024ull;

// One body, ready to write.
//
// `render` holds POSITIONS and NORMALS in body-local space. It is built through
// the product's own `buildRenderMesh`, so the crease policy that gives a box
// hard 90-degree edges and a sphere continuous shading is the SAME policy the
// viewport draws with — a second normal derivation here would be a second
// answer to what the surface looks like.
struct GlbExportBody {
    ObjectId objectId = kNoObject;
    // T * Rz * Ry * Rx * S, column-major, straight from ConstructionTransform.
    Mat4 model{};
    RenderMeshData render;
    // True when the source is this body's Frozen Sculpt Mesh rather than its
    // re-evaluated Construction geometry. Reported so a test can prove a Sculpt
    // project did not silently fall back.
    bool fromSculpt = false;
    // Carried from the source representation, and expressed in the exported
    // MATERIAL rather than by duplicating geometry. `buildRenderMesh`'s
    // two-sided mode duplicates every vertex with a negated normal and every
    // triangle reversed, which is right for a Vulkan pipeline with one global
    // cull mode and wrong for a file: an importer would see twice the triangles
    // a user drew. glTF says this with `material.doubleSided`.
    bool doubleSided = false;
};

struct GlbExportScene {
    std::vector<GlbExportBody> bodies;
};

// Reads the current project into an export snapshot.
//
// Publishes nothing, mints no revision, changes no mode and mutates nothing:
// the Construction meshes are generated into local storage and the sculpt
// meshes are copied. `kind` decides which representation a sculpted body
// exports; the caller takes it from the live session, exactly as a `.forge`
// save does.
//
// A body whose current representation is not a usable triangle mesh is refused
// rather than skipped — a file quietly missing a body is worse than no file.
GlbExportStatus captureGlbExportScene(const ConstructionScene& scene, ProjectKind kind,
                                      GlbExportScene* out);

// Encodes a snapshot as GLB 2.0 bytes, or returns empty and reports why.
//
// Deterministic: the same snapshot always produces byte-identical output. The
// JSON is written by hand in one fixed key order with one fixed number format,
// because a serializer that reordered keys or shortened a float differently
// would make two exports of the same project differ for no reason a user could
// see.
std::vector<uint8_t> encodeGlb(const GlbExportScene& scene, GlbExportStatus* outWhy = nullptr);

// The whole export in one call, against the process-scoped scene's contents.
// Convenience for JNI; owns no policy of its own.
std::vector<uint8_t> exportSceneAsGlb(const ConstructionScene& scene, ProjectKind kind,
                                      GlbExportStatus* outWhy = nullptr);

}  // namespace forgeshape
