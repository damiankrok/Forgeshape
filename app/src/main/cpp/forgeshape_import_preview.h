// The Imported Mesh Preview — session-only diagnostic renderer state.
//
// WHAT IT IS NOT, FIRST
// ---------------------
// A preview is **not a Construction Body**, and nothing here may make it look
// like one. `ARCH-OWNER-08` names the boundary and this file is where it is
// enforced. `ARCH-OWNER-09` widened what the PARSER will read — external static
// files, not only ForgeShape's own — and widened nothing below; every line of
// this list is unchanged by it:
//
//   * no `ObjectId` from the scene's allocator, and no entry in
//     `ConstructionScene` — the preview is not in the body list, is not
//     selectable, is not the active body and cannot become one;
//   * no Construction Source, so no primitive, no dimension, no parameter;
//   * no Frozen Sculpt Mesh, no `SculptRevision`, no stroke;
//   * no `MeshStore`, so no published `MeshRevision` anything else observes;
//   * never a `ConstructionHistory` step, in either direction;
//   * never encoded into `.forge` — not the manual slot, not the recovery
//     checkpoint, and not `projectSemanticFingerprint`;
//   * never re-exported as project content;
//   * gone on Clear, and gone on process restart because it is only ever here.
//
// It exists so the owner can SEE what an independent reader made of a file, in
// the same viewport, beside the thing it was made from. That is a diagnostic,
// and diagnostics that quietly become features are how a session-only preview
// turns into an undocumented import path.
//
// THE RENDERER KEY IS NOT AN IDENTITY
// -----------------------------------
// The renderer caches GPU buffers per `SceneDrawItem::objectId`, so a preview
// mesh needs a distinct key or two previews would share one body's buffers.
// The keys come from `kFirstPreviewRenderKey`, a range far above anything the
// scene's allocator can reach, and they are RENDERER RESOURCE KEYS: they are
// minted here, never by the scene, never persisted, never shown, never picked
// against, and they restart from the same base every session. Nothing may treat
// one as an identity. `previewRenderKeyIsReserved` exists so a test can assert
// the two ranges cannot meet.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "forgeshape_gltf_import.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Where preview renderer keys begin. 2^60 is unreachable by the body
// allocator, which starts at 1 and increments once per created body: reaching
// it would take a billion bodies a second for thirty million years.
constexpr ObjectId kFirstPreviewRenderKey = 1ull << 60;

// Whether a value is in the reserved preview range rather than the body range.
inline bool previewRenderKeyIsReserved(ObjectId key) { return key >= kFirstPreviewRenderKey; }

// The colour every preview mesh is drawn in.
//
// One flat neutral, deliberately different from the Construction palette so a
// preview is never mistaken for the model at a glance, and deliberately NOT a
// property anything can set: the preview carries no material and this is not
// the beginning of one.
constexpr float kPreviewColor[3] = {0.62f, 0.66f, 0.72f};

// One imported file, held for the session.
//
// Holds its own CPU arrays and its own immutable draw meshes. It borrows
// nothing from the project and the project borrows nothing from it, so clearing
// it can never leave the project holding a dangling anything.
class ImportedMeshPreview {
public:
    // Replaces whatever was loaded. Returns false and changes NOTHING when the
    // parsed scene cannot be turned into drawable meshes, so a refused import
    // leaves an existing preview exactly as it was.
    bool load(const ImportedScene& scene);

    // Forgets everything. The draw meshes are released here; the renderer drops
    // its GPU copies when it next sees a scene that does not name them.
    void clear();

    bool loaded() const { return !items_.empty(); }

    // Whether the viewport should draw the preview INSTEAD of the project.
    // Meaningless without a loaded preview, and forced back to false by clear().
    bool visible() const { return visible_ && loaded(); }
    void setVisible(bool visible) { visible_ = visible && loaded(); }

    // The FILE's counts, not the renderer's. One mesh may become several draw
    // items — one per primitive, so per-primitive `doubleSided` survives — and
    // several primitives may share one POSITION accessor, so neither the draw
    // item count nor a sum over draw items would be what the file describes.
    uint32_t meshCount() const { return meshCount_; }
    uint32_t vertexCount() const { return vertexCount_; }
    uint32_t triangleCount() const { return triangleCount_; }
    // How many draw batches the meshes became. A renderer fact, reported so a
    // diagnostic can say a multi-primitive file did not lose a primitive.
    uint32_t batchCount() const { return static_cast<uint32_t>(items_.size()); }
    const std::string& sourceSummary() const { return summary_; }

    // The world axis-aligned bounds of everything loaded, as min xyz then max
    // xyz. False with nothing loaded, and `out` is then untouched.
    //
    // World, not local, because the node transform is already baked into the
    // vertices — which is exactly what makes this the number a test can use to
    // say a node matrix was applied, and applied the right way round.
    bool worldBounds(float* out) const;

    // What the renderer draws. Every item carries a reserved renderer key, its
    // own immutable mesh and an identity model matrix, because the importer
    // has already baked the node transform into the positions.
    const SceneSnapshot& snapshot() const { return items_; }

private:
    SceneSnapshot items_;
    uint32_t meshCount_ = 0;
    uint32_t vertexCount_ = 0;
    uint32_t triangleCount_ = 0;
    bool visible_ = false;
    std::string summary_;
};

// The one process-scoped preview. Session state, exactly like the camera:
// created empty, never persisted, and gone when the process is.
ImportedMeshPreview& importedMeshPreview();

}  // namespace forgeshape
