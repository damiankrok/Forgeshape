// The bounded glTF 2.0 binary reader (`GLB-IMPORT-R0/R1`, `ARCH-OWNER-08/09`).
//
// INDEPENDENT OF THE WRITER. Nothing here calls `forgeshape_gltf_export.*`; it
// shares only `forgeshape_json.*` and `forgeshape_math.h`, and re-derives every
// offset, length, stride and bound from the file rather than from what a writer
// intended. That independence is what lets the roundtrip diagnostic prove the
// exporter against something other than itself.
//
// THE SUPPORTED SUBSET is ordinary STATIC geometry: a node `matrix` or TRS,
// several TRIANGLES primitives per mesh (including ones sharing a POSITION
// accessor), a missing NORMAL (generated), COLOR_*/TEXCOORD_* validated and
// ignored, a material's `doubleSided`, and `extras` ignored. Everything else
// -- a required extension, a sparse accessor, an interleaved view, an external
// buffer, animation, skinning, morph targets, a non-triangle mode, a node with
// children, a node stating both `matrix` and TRS, an unknown attribute --
// FAILS CLOSED with a named `GlbImportStatus`. Silently ignoring any of them
// would put geometry the file does not describe into a project that keeps it.
//
// THIS DECIDES NOTHING ABOUT THE PROJECT. It produces GEOMETRY (`ParsedGlbScene`)
// and never a body, an `ObjectId`, a history step or a `.forge` byte, so one
// parse serves both the durable import (`forgeshape_import_commit.h`) and the
// session-only diagnostic preview (`forgeshape_import_preview.h`).
//
// COORDINATES are not converted: glTF and ForgeShape are both right-handed,
// +Y-up and metric, so an axis swap here would be a defect -- the same fact
// that makes the exporter's lack of a conversion node correct.
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
struct ParsedGlbBatch {
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
struct ParsedGlbMesh {
    // The NODE's `name`, exactly as the file states it and never sanitized
    // here: this reader reports what the file says, and what a ForgeShape
    // object may be called is the importer's rule, not the parser's.
    std::string name;
    // The glTF MESH's own `name`. Kept beside the node's because a durable
    // import prefers the node's and falls back to this one, and neither is
    // guaranteed to be present.
    std::string meshName;
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
    std::vector<ParsedGlbBatch> batches;

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions.size() / 3); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }

    // The world position of one vertex. Already baked, so this is a read.
    Vec3 worldPosition(uint32_t vertex) const;
    // The same vertex in the NODE's local space: the baked world position with
    // the node's translation taken back off.
    //
    // `Model = T · L`, and the bake applied the whole of it, so subtracting the
    // translation leaves exactly `L·p` — the linear part baked in, the
    // placement taken out. That split is what a durable import needs: the
    // rotation, scale and shear of another tool's node have no ForgeShape
    // transform to live in, but its translation is precisely a placement.
    // Nothing is recentred: the origin stays the node's own.
    Vec3 localPosition(uint32_t vertex) const;
    // The world direction of one normal. Already carried through the inverse
    // transpose and normalized, so this is a read.
    Vec3 worldNormal(uint32_t vertex) const;
};

struct ParsedGlbScene {
    std::vector<ParsedGlbMesh> meshes;

    uint32_t totalVertices() const;
    uint32_t totalTriangles() const;
    uint32_t totalBatches() const;
};

// Reads GLB bytes into an ParsedGlbScene, or reports why it would not.
//
// Nothing partial is ever produced: `out` is written only on Ok. Every offset,
// length and index is re-derived and bounds-checked against the bytes actually
// supplied, so a file that lies about its own sizes is refused rather than read
// past.
GlbImportStatus importGlb(const uint8_t* bytes, size_t length, ParsedGlbScene* out);

}  // namespace forgeshape
