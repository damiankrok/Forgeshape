// Turning a parsed GLB file into durable project objects (`IMPORT-01A`).
//
// The parser (`forgeshape_gltf_import.h`) reads bytes and decides nothing about
// the project; `ImportedMesh` is what a body owns afterwards. This one function
// is the decision between them: how many objects a file becomes, what they are
// called, where they are placed, and that none of it happens by halves.
//
// ATOMIC. Every object is built and validated OFF the scene first; only when
// all of them exist does anything reach `ConstructionScene`. A refusal mints no
// `ObjectId`, appends no body, records no step and moves no fingerprint. The
// whole commit is ONE `ScopedConstructionEdit`, so forty objects are one Undo.
//
// THE TRANSFORM SPLIT. `Model = T · L`. A glTF node's linear part may carry
// rotation, non-uniform scale and shear, which the nine-value placement (with
// its strictly positive diagonal scale) cannot store; decomposing a sheared
// node has no correct answer. So `L` is BAKED into the local geometry and `T`
// becomes the body's placement: an imported body arrives at rotation 0,0,0 and
// scale 1,1,1, with its origin the node's own origin -- the pivot every
// downstream tool inherits. Nothing is recentred.
//
// NOT the diagnostic preview (`ImportedMeshPreview` is session-only and shares
// nothing with this). NOT appearance: materials, textures, colours and UVs were
// validated and ignored by the parser. Sculpting an imported body is
// `IMPORT-01B`'s and happens later, through `buildSculptSourceMesh`.
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
