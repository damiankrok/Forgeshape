// Turning a parsed GLB file into durable project objects.
//
// This is the whole of `IMPORT-01A`'s write direction, and it is deliberately
// one function. Everything either side of it already exists: the R1 parser
// (`forgeshape_gltf_import.h`) reads the bytes and decides nothing about the
// project, and `ImportedMesh` (`forgeshape_imported_mesh.h`) is what a body
// owns once the decision is made. What lives here is the decision — how many
// objects a file becomes, what they are called, where they are placed, and the
// rule that none of it happens by halves.
//
// ATOMIC, AND WHY
// ---------------
// An import can produce several objects, and a file that describes four good
// meshes and one broken one is not four fifths of a project. Every object is
// built and validated OFF the scene first; only when all of them exist does
// anything reach `ConstructionScene`. So a refusal costs nothing at all: no
// `ObjectId` is minted, no body is appended, no history step is recorded, no
// fingerprint moves, and the user's project is byte-for-byte what it was.
//
// The whole commit runs inside ONE `ScopedConstructionEdit`, so an import of
// forty objects is exactly one Undo — the same rule Add Primitive follows for
// its two mutations.
//
// THE TRANSFORM SPLIT
// -------------------
// `Model = T · L`. glTF states a node transform this product cannot store: its
// linear part may carry rotation, non-uniform scale and shear, and ForgeShape's
// placement is nine authored values with a strictly positive diagonal scale.
// Rather than decompose it — which for a sheared node has no correct answer —
// the linear part is BAKED into the object's local geometry and the translation
// becomes the body's placement. An imported body therefore arrives at
// `rotation = 0,0,0` and `scale = 1,1,1`, with the geometry already carrying
// whatever the file's node did to it, and every later Move/Rotate/Scale is an
// ordinary ForgeShape transform over that. Nothing is recentred: the object's
// origin is the node's own origin, which is the pivot every downstream tool
// inherits.
//
// WHAT THIS IS NOT
// ----------------
// Not the diagnostic preview. `ImportedMeshPreview` still exists, still shares
// nothing with this, and is still session-only; this path creates real bodies
// with real identities that are saved, undone and reopened.
//
// Not sculpt. `IMPORT-01A` gives an imported body no Frozen Sculpt Mesh and no
// Start Sculpting — that is `IMPORT-01B`.
//
// Not appearance. Materials, textures, colours and UVs were validated and then
// ignored by the parser, so there is nothing here to carry them.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem, no
// `Uri`. Where the bytes came from is not project truth and does not appear.
#pragma once

#include <cstdint>

#include "forgeshape_gltf_import.h"
#include "forgeshape_history.h"
#include "forgeshape_imported_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Why an import produced no objects. Every one is a refusal, and a refusal
// leaves the project exactly as it was.
enum class ImportCommitStatus {
    Ok,
    // The parsed scene describes no meshes at all. The parser refuses this
    // already; it is restated here because this function is a public entry
    // point a caller can reach with a scene it built itself.
    NothingToImport,
    // The project cannot hold this many bodies. Bounded by what a `.forge`
    // file can carry, deliberately: an import that could not be SAVED is not
    // an import, and finding that out at the first checkpoint would be worse
    // than finding it out now.
    TooManyObjects,
    // One object's geometry did not pass `ImportedMesh` validation. `outReport`
    // carries which rule refused it.
    RejectedGeometry,
    // A Construction edit is open. Creating bodies underneath a half-finished
    // user act would leave that act's captured pre-state describing a scene it
    // never saw.
    RefusedEditInProgress,
};

const char* importCommitStatusName(ImportCommitStatus status);

// What an import actually did, so a caller can log it and a test can assert
// against it rather than against a bare boolean.
struct ImportCommitReport {
    int objects = 0;
    uint32_t vertices = 0;
    uint32_t triangles = 0;
    uint32_t batches = 0;
    // The object left selected, and the ids of the range this import created.
    // The scene's allocator is monotonic, so the created ids are contiguous
    // from `firstObjectId` — but a caller that needs one asks for it rather
    // than assuming arithmetic on an identity.
    ObjectId activeBodyId = kNoObject;
    ObjectId firstObjectId = kNoObject;
    ObjectId lastObjectId = kNoObject;
    // Meaningful only on RejectedGeometry.
    ImportedMeshValidation geometryWhy = ImportedMeshValidation::Ok;
};

// Creates one durable Imported Mesh body per supported top-level mesh node.
//
// A glTF mesh's several TRIANGLES primitives stay INSIDE one object as submesh
// batches — they are parts of a thing, not things — so the Objects list gains
// one row per node and never one per primitive.
//
// Naming, in order: the node's `name`, then the mesh's `name`, then the
// deterministic `Imported <n>` fallback, where `n` is 1-based within this
// import. A name from another tool is sanitized by the domain's own rule
// (`sanitizeImportedMeshName`) before it can become project truth.
//
// The object left ACTIVE is the FIRST one this import created. One rule, stated
// here: a multi-object import puts the user at the start of what just arrived,
// whatever order the rest came in and however many there were.
//
// Callers hold the one existing state mutex, exactly as for every other scene
// mutation.
ImportCommitStatus commitImportedGlbScene(const ParsedGlbScene& parsed, ConstructionScene& scene,
                                          ConstructionHistory& history,
                                          ImportCommitReport* outReport = nullptr);

}  // namespace forgeshape
