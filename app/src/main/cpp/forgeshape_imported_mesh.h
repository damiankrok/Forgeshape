// The Imported Mesh — a durable, non-parametric project representation.
//
// WHAT IT IS
// ----------
// `IMPORT-01A` (`ARCH-OWNER-10`) promotes the GLB-IMPORT-R1 ingestion path from
// a session-only preview into real project objects. An Imported Mesh is one of
// the two representations a `SceneObject` can own, and it is the first one that
// is **not** derived from parameters:
//
//   * a Construction Source is a primitive plus its dimensions, and its
//     geometry is REGENERATED on every load by `generateMesh()`;
//   * an Imported Mesh IS the geometry. There is no rule that could recreate
//     it, so it is project truth and it is serialized.
//
// That difference is the whole reason this type exists rather than an imported
// file being flattened into a Box nobody could edit back.
//
// WHAT IT IS NOT
// --------------
// **Not a Construction Source.** It has no `PrimitiveKind`, no dimensions and
// no remembered parameter sets, and nothing may reconstruct one from it. A body
// owns one representation or the other and never both.
//
// **Not a Frozen Sculpt Mesh.** `IMPORT-01A` deliberately has no Start
// Sculpting on an imported body — that is `IMPORT-01B` — and this type carries
// no `SculptRevision`, no adjacency and no stroke state.
//
// **Not appearance.** The R1 parser validates COLOR_0/COLOR_1/TEXCOORD and the
// material's colour, roughness, metallic and textures, and decodes none of
// them. They were never read, so they are not preserved here and no document
// may claim they are. `doubleSided` is the single exception, and it is carried
// because it changes which triangles are VISIBLE, not how they look.
//
// SUBMESH BATCHES
// ---------------
// One glTF mesh's several TRIANGLES primitives stay INSIDE one object — they
// are submeshes, not objects, and they must never appear as separate rows in
// the Objects list. They survive as index ranges over one vertex array, each
// carrying its own `doubleSided`, because a character whose eyes are open
// sheets and whose body is a closed solid is one mesh with two culling answers.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem, no
// `Uri`. Where the bytes came from is not project truth and is not here.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"

namespace forgeshape {

// Bounds on one imported object. Far above the low-poly class `IMPORT-01A`
// targets, and low enough that every size computation below is provably free of
// overflow. A count past one of these is refused before a byte is allocated.
constexpr uint32_t kMaxImportedMeshVertices = 4u * 1000u * 1000u;
constexpr uint32_t kMaxImportedMeshIndices = 24u * 1000u * 1000u;
constexpr uint32_t kMaxImportedMeshBatches = 4096;

// How long an imported object's name may be, in bytes of UTF-8.
//
// A glTF `name` is arbitrary text from another tool, so it is bounded here
// rather than trusted. Truncation is on a UTF-8 boundary so a stored name is
// always well-formed text.
constexpr size_t kMaxImportedMeshNameBytes = 96;

// Why an Imported Mesh would not be built. Every one is a refusal, and a
// refusal produces nothing: a half-built imported object is exactly what must
// never reach the scene.
enum class ImportedMeshValidation {
    Ok,
    EmptyVertices,
    EmptyIndices,
    TooLarge,
    IndexCountNotTriangles,
    IndexOutOfRange,
    NonFinitePosition,
    NonFiniteNormal,      // a normal that is not a finite unit direction
    CountMismatch,        // one normal per position, or none at all
    NoBatches,
    BatchesDoNotTile,     // the ranges must cover the index array exactly, in order
    // The geometry is a valid Imported Mesh, but the DRAW data it produces is
    // not a valid `RuntimeMesh` — a two-sided submesh's reversed copy pushing
    // the index count past the ceiling, or a vertex count above what a
    // published mesh may hold. A separate name because it is a separate
    // question: `validateImportedMeshData` never reports it, and only a caller
    // that has asked for the draw data can.
    NotDrawable,
};

const char* importedMeshValidationName(ImportedMeshValidation why);

// One submesh: a contiguous range of the object's index array.
//
// Ranges are in file order and together tile the index array exactly, so a
// consumer that ignores batching still draws every triangle exactly once.
struct ImportedMeshBatch {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    // From the source file's material. It decides whether this submesh's back
    // faces are drawn, and it reaches nothing else: this representation carries
    // no material and is not the beginning of one.
    bool doubleSided = false;
};

// One imported object's geometry, in the object's LOCAL space.
//
// Local, not world: the node transform's linear part is baked into these
// positions at import time and its translation becomes the object's authored
// placement, so a gizmo moves the object exactly as it moves a Construction
// Body. See `IMPORT-01A`'s bake rule.
class ImportedMesh {
public:
    ImportedMesh() = default;

    // Builds one, or refuses. FAILS CLOSED: on anything but Ok the returned
    // object is empty and `outWhy` says which rule stopped it.
    //
    // `normals` must hold exactly one finite unit direction per position. The
    // importer already guarantees that — it generates them when the file states
    // none — and it is re-checked here because this is also the entry point a
    // decoded `.forge` document arrives through, and a file is not the
    // importer.
    static ImportedMesh build(std::vector<float> positions, std::vector<float> normals,
                              std::vector<uint32_t> indices,
                              std::vector<ImportedMeshBatch> batches,
                              ImportedMeshValidation* outWhy = nullptr);

    bool valid() const { return !positions_.empty() && !indices_.empty(); }

    uint32_t vertexCount() const { return static_cast<uint32_t>(positions_.size() / 3u); }
    uint32_t triangleCount() const { return static_cast<uint32_t>(indices_.size() / 3u); }
    uint32_t batchCount() const { return static_cast<uint32_t>(batches_.size()); }

    const std::vector<float>& positions() const { return positions_; }
    const std::vector<float>& normals() const { return normals_; }
    const std::vector<uint32_t>& indices() const { return indices_; }
    const std::vector<ImportedMeshBatch>& batches() const { return batches_; }

    // The object's local axis-aligned bounds, min xyz then max xyz. False when
    // nothing is loaded, leaving `out` untouched.
    bool localBounds(float* out) const;

    // Builds the drawable geometry for this representation.
    //
    // A double-sided submesh has its triangles emitted a SECOND time with
    // reversed winding, here, rather than by asking the published mesh for
    // `renderBothSides` — because that flag is one answer for a whole mesh and
    // an imported object legitimately has one answer per submesh. Doing it at
    // build time keeps per-submesh fidelity exact with no renderer change and
    // no second draw path.
    //
    // Returns false and writes nothing when the geometry does not pass
    // `RuntimeMesh` validation.
    bool buildDrawData(std::vector<MeshVertex>* outVertices,
                       std::vector<uint32_t>* outIndices) const;

private:
    std::vector<float> positions_;
    std::vector<float> normals_;
    std::vector<uint32_t> indices_;
    std::vector<ImportedMeshBatch> batches_;
};

// THE rule for whether these four arrays describe an imported object.
//
// Separate from `build` so the `.forge` codec can ask the question without
// copying a multi-megabyte mesh into a temporary object just to be told the
// answer. One implementation, two callers: the importer builds through `build`,
// which calls this, and `validateProjectDocument` calls this directly on the
// decoded arrays. Neither restates a rule the other applies.
ImportedMeshValidation validateImportedMeshData(const std::vector<float>& positions,
                                                const std::vector<float>& normals,
                                                const std::vector<uint32_t>& indices,
                                                const std::vector<ImportedMeshBatch>& batches);

// The colour every imported vertex is given.
//
// One flat neutral, exactly as the R1 preview used. Vertex colour feeds only
// the debug-only source-colour shading mode, so it is presentation and never
// truth — and an imported file's own COLOR_0 was never decoded, so inventing a
// colour from it here would be claiming an appearance the product did not read.
constexpr float kImportedMeshVertexColor[3] = {0.62f, 0.66f, 0.72f};

// Truncates a name from another tool to something this product can store.
//
// Trims surrounding whitespace, replaces control characters, DROPS bytes that
// are not part of a well-formed UTF-8 sequence, cuts on a UTF-8 boundary at
// `kMaxImportedMeshNameBytes`, and returns empty when nothing usable survives —
// which is the caller's signal to fall back to the deterministic `Imported <n>`
// form.
//
// Malformed UTF-8 is dropped rather than tolerated because this string is
// stored in a `.forge` file, handed across JNI to a Java `String` and drawn in
// a row: a half-encoded character from another tool's exporter would be a
// defect in all three places. The function is IDEMPOTENT — sanitizing an
// already-sanitized name returns it unchanged — which is what lets the codec
// state its rule as "the stored name is what this would produce".
std::string sanitizeImportedMeshName(const std::string& raw);

// Whether a name is one this product would have stored: non-empty, and exactly
// what `sanitizeImportedMeshName` produces for itself. This is the check the
// `.forge` decoder applies, so a file cannot carry a name the importer could
// not have made.
bool importedMeshNameIsStorable(const std::string& name);

// The deterministic fallback for an object whose file named it nothing usable.
//
// `ordinal` is 1-based and is the object's position among the objects THIS
// import created, never an ObjectId: an id is minted and would make the same
// file produce different names in different sessions.
std::string fallbackImportedMeshName(uint32_t ordinal);

}  // namespace forgeshape
