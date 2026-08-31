// GLB-IMPORT-R0 — a bounded, diagnostic glTF 2.0 binary reader.
//
// WHAT THIS IS FOR
// ----------------
// One question, asked by the owner (`ARCH-OWNER-08`): *does the `.glb`
// ForgeShape wrote, decoded according to glTF semantics by something that is
// not the writer, describe the same world-space geometry as the ForgeShape
// scene it came from?*
//
// Answering that needs a reader with no shared assumptions with the writer.
// Reading a file back through the code that wrote it proves the two halves
// agree with each other and nothing about whether either agrees with glTF, and
// a discrepancy the owner can see in Blender is exactly the case where "the two
// halves agree" is worthless.
//
// So nothing here calls `forgeshape_gltf_export.*`. It shares
// `forgeshape_json.*` (which knows only what JSON is) and `forgeshape_math.h`
// (vectors and matrices, no glTF in either), and it re-derives every offset,
// length, stride and bound from the file rather than from what a writer
// intended.
//
// WHAT THIS IS NOT
// ----------------
// **This is not production import.** It is a diagnostic that reads a file
// ForgeShape itself produced. What it builds is a session-only preview, never
// a Construction Body, never `.forge` content and never anything the user can
// edit. Durable import — arbitrary files, materials, hierarchy, the whole
// question of what an imported object even IS in a Construction/Sculpt product
// — is `IMPORT-01` and stays post-MVP.
//
// The supported subset is therefore deliberately exactly what the current
// exporter emits, and everything else FAILS CLOSED with a named reason. A
// diagnostic that silently ignored a transform, a sparse accessor or a
// compression extension would answer the owner's question wrongly, which is
// worse than refusing to answer it.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem, no
// `Uri`. It takes bytes and produces geometry.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_math.h"

namespace forgeshape {

// A ceiling on the file this module will read, matching the exporter's own
// write ceiling so a file ForgeShape can write is a file it can read back.
constexpr uint64_t kMaxImportedGlbBytes = 512ull * 1024ull * 1024ull;

// Bounds on what one file may describe, so a malformed count cannot make the
// reader allocate before it has read anything.
constexpr uint32_t kMaxImportedMeshes = 4096;
constexpr uint32_t kMaxImportedVerticesPerMesh = 8u * 1000u * 1000u;

// Why an import produced nothing. Every one is a refusal, and a refusal builds
// no partial preview: the caller's existing state is untouched.
enum class GlbImportStatus {
    Ok,

    // --- container ---------------------------------------------------------
    NoData,
    NotGlb,                 // the 12-byte header's magic is not "glTF"
    UnsupportedVersion,     // container version is not 2
    TruncatedFile,          // a declared length runs past the bytes given
    ChunkMisaligned,        // a chunk length is not a multiple of 4
    MissingJsonChunk,
    MissingBinChunk,        // R0 requires the buffer to be embedded
    UnknownChunk,
    MalformedJson,

    // --- document ----------------------------------------------------------
    UnsupportedAssetVersion,
    NoScene,
    NoMeshes,
    NothingToImport,        // parses, but describes no geometry

    // --- the R0 subset boundary --------------------------------------------
    // Each of these is a valid glTF feature this reader does not implement.
    // They are separate values rather than one "unsupported" because the whole
    // point of the diagnostic is to say precisely what stopped it.
    ExternalBuffer,         // a buffer or image with a `uri`
    UnsupportedExtension,   // an extensionsRequired this reader does not know
    HasAnimation,
    HasSkin,
    SparseAccessor,
    MorphTargets,
    NonTriangleMode,
    NodeHierarchy,          // a node with children; R0 reads a flat scene
    NodeMatrix,             // a node stating a `matrix`
    NodeRotation,           // a node stating a non-identity rotation
    NodeScale,              // a node stating a non-identity scale
    MissingAttribute,       // POSITION or NORMAL absent
    UnsupportedComponentType,
    UnsupportedAccessorType,
    InterleavedAccessor,    // a bufferView byteStride this reader does not read

    // --- data --------------------------------------------------------------
    AccessorOutOfRange,     // an accessor reads past its bufferView or buffer
    IndexOutOfRange,        // an index names a vertex that does not exist
    IndexCountNotTriangles,
    CountMismatch,          // POSITION and NORMAL disagree on vertex count
    NonFiniteValue,
    TooLarge,
};

const char* glbImportStatusName(GlbImportStatus status);

// One mesh as the FILE describes it, before any ForgeShape meaning is put on it.
//
// `positions` and `normals` are what the accessors hold — for a ForgeShape
// export that means rotation and scale are already baked in, because that is
// what ARCH-OWNER-07 makes the file say. `translation` is what the node states.
// This struct does not know that; it reports what it read.
struct ImportedMesh {
    std::string name;
    // The node's transform, as glTF semantics give it. R0 accepts a
    // translation and refuses a rotation, a scale or a matrix, so this is
    // always a pure translation — kept as a full matrix anyway so the world
    // transform below is one multiplication rather than a special case.
    Mat4 nodeTransform{};
    float translation[3] = {0.0f, 0.0f, 0.0f};
    std::vector<float> positions;  // xyz triples, mesh-local
    std::vector<float> normals;    // xyz triples, mesh-local
    std::vector<uint32_t> indices;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }

    // The world position of one vertex: nodeTransform * localPosition.
    Vec3 worldPosition(uint32_t vertex) const;
    // The world direction of one normal. A pure translation does not change a
    // direction, which is why this is not the inverse transpose: R0 refuses
    // every node transform for which the two would differ.
    Vec3 worldNormal(uint32_t vertex) const;
};

struct ImportedScene {
    std::vector<ImportedMesh> meshes;

    uint32_t totalVertices() const;
    uint32_t totalTriangles() const;
};

// Reads GLB bytes into an ImportedScene, or reports why it would not.
//
// Nothing partial is ever produced: `out` is written only on Ok. Every offset,
// length and index is re-derived and bounds-checked against the bytes actually
// supplied, so a file that lies about its own sizes is refused rather than read
// past.
GlbImportStatus importGlb(const uint8_t* bytes, size_t length, ImportedScene* out);

}  // namespace forgeshape
