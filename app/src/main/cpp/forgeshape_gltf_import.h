// GLB-IMPORT — a bounded, diagnostic glTF 2.0 binary reader.
//
// WHAT THIS IS FOR
// ----------------
// R0 (`ARCH-OWNER-08`) asked one question: *does the `.glb` ForgeShape wrote,
// decoded according to glTF semantics by something that is not the writer,
// describe the same world-space geometry as the ForgeShape scene it came
// from?* Answering that needs a reader with no shared assumptions with the
// writer, so nothing here calls `forgeshape_gltf_export.*`. It shares
// `forgeshape_json.*` (which knows only what JSON is) and `forgeshape_math.h`
// (vectors and matrices, no glTF in either), and it re-derives every offset,
// length, stride and bound from the file rather than from what a writer
// intended.
//
// R1 (`ARCH-OWNER-09`) widens the subset to ordinary STATIC low-poly GLB files
// written by other tools — the class of file a sculpting app exports — so the
// owner can look at one in the ForgeShape viewport. What widened, exactly:
//
//   * a node `matrix`, or ordinary TRS, instead of a bare translation;
//   * several TRIANGLES primitives per mesh, including ones that share a
//     POSITION accessor;
//   * a missing NORMAL, which is generated;
//   * COLOR_0/COLOR_1/TEXCOORD_0/TEXCOORD_1, validated and then ignored;
//   * a material's `doubleSided`, which reaches preview culling and nothing
//     else;
//   * `extras`, which is ignored and never becomes project data.
//
// WHAT THIS IS NOT
// ----------------
// **This is not production import.** It is a diagnostic. What it builds is a
// session-only preview, never a Construction Body, never `.forge` content and
// never anything the user can edit. Durable import — materials, hierarchy, the
// whole question of what an imported object even IS in a Construction/Sculpt
// product — is `IMPORT-01` and stays post-MVP. OBJ and FBX are absent in both
// directions.
//
// Everything outside the supported subset FAILS CLOSED with a named reason. A
// diagnostic that silently ignored a transform, a sparse accessor or a
// compression extension would answer the owner's question wrongly, which is
// worse than refusing to answer it.
//
// COORDINATES
// -----------
// None are converted. glTF is right-handed, +Y-up and metric, and so is
// ForgeShape, so an axis swap here would be a defect — the same fact that
// makes the exporter's lack of a conversion node correct. In particular there
// is no Blender-style Y/Z fix.
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
// How many TRIANGLES primitives one mesh may carry. Well past the seven a
// low-poly character with per-region materials produces, and far short of a
// count that could make the batch list itself an allocation attack.
constexpr uint32_t kMaxImportedPrimitivesPerMesh = 4096;

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
    MissingBinChunk,        // the buffer must be embedded
    UnknownChunk,
    MalformedJson,

    // --- document ----------------------------------------------------------
    UnsupportedAssetVersion,
    NoScene,
    NoMeshes,
    NothingToImport,        // parses, but describes no geometry

    // --- the supported-subset boundary -------------------------------------
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
    NodeHierarchy,          // a node with children; the reader reads a flat scene
    NodeTransformConflict,  // a node stating BOTH a `matrix` and a TRS member
    SingularNodeTransform,  // a node transform this bake cannot invert: no
                            // volume, or a non-affine bottom row
    MissingAttribute,       // POSITION absent
    NonIndexedPrimitive,    // a primitive with no `indices`
    UnknownAttribute,       // an attribute outside the known ignore list
    UnsupportedComponentType,
    UnsupportedAccessorType,
    InterleavedAccessor,    // a bufferView byteStride this reader does not read

    // --- data --------------------------------------------------------------
    AccessorOutOfRange,     // an accessor reads past its bufferView or buffer
    IndexOutOfRange,        // an index names a vertex that does not exist
    IndexCountNotTriangles,
    CountMismatch,          // an attribute disagrees with POSITION on count
    CannotGenerateNormals,  // a normal, supplied or generated, is not a direction
    NonFiniteValue,
    TooLarge,
};

const char* glbImportStatusName(GlbImportStatus status);

// The bounded kind of refusal, for the one sentence a user is shown.
//
// The status above is the STABLE TOKEN: it names precisely what stopped the
// reader and it belongs in the log, where a person diagnosing a file can act
// on it. A user looking at a viewport can act on three things and no more —
// the file is not a GLB this reader can open, the file uses features this
// preview does not read, or the file's own geometry does not add up — so the
// product says one of those and puts the rest in the log.
enum class GlbImportCategory {
    Unreadable,     // not a GLB, truncated, malformed, or nothing to draw
    Unsupported,    // a valid glTF feature outside the supported subset
    Inconsistent,   // the file's own accessors, indices or normals disagree
};

GlbImportCategory glbImportStatusCategory(GlbImportStatus status);

// One TRIANGLES primitive, as a contiguous range of its mesh's index array.
//
// Primitives are kept apart rather than merged because `doubleSided` is a
// PER-PRIMITIVE fact and the preview has to honour it per primitive: a
// character whose eyes are open sheets and whose body is a closed solid is one
// mesh with two culling answers. The ranges are in file order and together
// cover the mesh's indices exactly, so a consumer that ignores batching still
// draws every triangle exactly once.
struct ImportedPrimitiveBatch {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    // From the primitive's material. It reaches preview culling and nothing
    // else: this reader implements no material pipeline, and base colour,
    // roughness, metallic and textures are all deliberately unread.
    bool doubleSided = false;
};

// One node's mesh, in WORLD space.
//
// The node transform is BAKED here rather than carried to the renderer, which
// is what lets the preview draw with an identity model matrix and what makes
// generated normals well defined — they are accumulated from the baked
// positions, so a non-uniform node scale is already in them. `nodeTransform`
// and `normalTransform` are kept so a diagnostic can say what the FILE stated,
// not only what the geometry became.
struct ImportedMesh {
    std::string name;
    // The node's complete transform, as glTF semantics give it: the `matrix`
    // when it states one, otherwise T * R * S.
    Mat4 nodeTransform{};
    // How a direction leaves that transform: the inverse transpose of its
    // upper-left 3x3. Not the transform itself, which is only the same answer
    // while the scale is uniform.
    Mat4 normalTransform{};
    // The translation component, for a reader that wants the node's placement
    // without decomposing the matrix.
    float translation[3] = {0.0f, 0.0f, 0.0f};

    // True when the file carried no NORMAL and these were generated from the
    // baked positions. A diagnostic must be able to say which it is looking at.
    bool normalsGenerated = false;
    // True when the node transform's determinant was negative and triangle
    // winding was corrected during the bake, so front faces stay front faces.
    bool windingCorrected = false;

    std::vector<float> positions;   // xyz triples, WORLD space (baked)
    std::vector<float> normals;     // xyz triples, WORLD space, unit length
    std::vector<uint32_t> indices;  // every batch, concatenated in file order
    std::vector<ImportedPrimitiveBatch> batches;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }

    // The world position of one vertex. Already baked, so this is a read.
    Vec3 worldPosition(uint32_t vertex) const;
    // The world direction of one normal. Already carried through the inverse
    // transpose and normalized, so this is a read.
    Vec3 worldNormal(uint32_t vertex) const;
};

struct ImportedScene {
    std::vector<ImportedMesh> meshes;

    uint32_t totalVertices() const;
    uint32_t totalTriangles() const;
    uint32_t totalBatches() const;
};

// Reads GLB bytes into an ImportedScene, or reports why it would not.
//
// Nothing partial is ever produced: `out` is written only on Ok. Every offset,
// length and index is re-derived and bounds-checked against the bytes actually
// supplied, so a file that lies about its own sizes is refused rather than read
// past.
GlbImportStatus importGlb(const uint8_t* bytes, size_t length, ImportedScene* out);

}  // namespace forgeshape
