#include "forgeshape_import_preview.h"

#include <cstdio>

namespace forgeshape {

bool ImportedMeshPreview::load(const ImportedScene& scene) {
    if (scene.meshes.empty()) {
        return false;
    }

    // Built into LOCAL storage first. Nothing touches the live preview until
    // every mesh has been validated, so a refusal halfway through leaves the
    // previous preview whole rather than half-replaced.
    SceneSnapshot items;
    items.reserve(scene.totalBatches());
    uint32_t vertices = 0;
    uint32_t triangles = 0;
    ObjectId nextKey = kFirstPreviewRenderKey;

    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        const ImportedMesh& mesh = scene.meshes[i];
        if (mesh.positions.empty() || mesh.indices.empty() || mesh.batches.empty()) {
            return false;
        }

        std::vector<MeshVertex> drawVertices(mesh.vertexCount());
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            const size_t at = static_cast<size_t>(v) * 3u;
            // Already in world space: the importer bakes the node transform
            // into the positions, which is why every draw item below carries
            // an identity model matrix.
            drawVertices[v].position[0] = mesh.positions[at];
            drawVertices[v].position[1] = mesh.positions[at + 1];
            drawVertices[v].position[2] = mesh.positions[at + 2];
            // A flat neutral. The preview carries no material and this is not
            // the start of one; the file's COLOR_0/COLOR_1 are validated by the
            // parser and deliberately never decoded, so nothing here could
            // claim to show them.
            drawVertices[v].color[0] = kPreviewColor[0];
            drawVertices[v].color[1] = kPreviewColor[1];
            drawVertices[v].color[2] = kPreviewColor[2];
        }

        // ONE DRAW ITEM PER PRIMITIVE, because `doubleSided` is a per-primitive
        // fact and the whole mesh has to be able to hold both answers at once —
        // a character whose eyes are open sheets and whose body is a closed
        // solid is exactly that file. Every batch shares this mesh's vertex
        // array, so a seven-primitive character over one POSITION accessor
        // stays the vertex count the file states.
        for (size_t b = 0; b < mesh.batches.size(); ++b) {
            const ImportedPrimitiveBatch& batch = mesh.batches[b];
            if (batch.indexCount == 0
                || static_cast<uint64_t>(batch.firstIndex) + batch.indexCount
                        > mesh.indices.size()) {
                return false;
            }

            // Revision 1 for every preview mesh: a preview has no revision
            // history and never publishes, and `createRuntimeMesh` reserves 0
            // for "nothing published". The distinct RENDERER KEY, not the
            // revision, is what keeps two previews' buffers apart.
            MeshValidation why = MeshValidation::Ok;
            const ObjectId key = nextKey++;
            const RuntimeMeshPtr runtime = createRuntimeMesh(
                    key, /*revision=*/1, drawVertices.data(),
                    static_cast<uint32_t>(drawVertices.size()),
                    mesh.indices.data() + batch.firstIndex, batch.indexCount, &why,
                    // The file's material said this primitive has no inside to
                    // hide, so the preview must not cull its back faces. It
                    // reaches culling and nothing else: no colour, no
                    // roughness, no texture is read anywhere.
                    /*renderBothSides=*/batch.doubleSided);
            if (runtime == nullptr) {
                return false;
            }

            SceneDrawItem item;
            item.objectId = key;
            item.mesh = runtime;
            // Identity, because the node transform is already in the vertices.
            // Nothing downstream needs a second copy of a placement the
            // geometry already carries, and a preview has no transform anyone
            // can edit.
            item.model = mat4Identity();
            item.inverseModel = mat4Identity();
            item.normalModel = mat4Identity();
            // Never selected. Selection is an identity the preview does not
            // have, and the picker is not given this snapshot in the first
            // place.
            item.selected = false;
            items.push_back(item);

            triangles += batch.indexCount / 3u;
        }

        vertices += mesh.vertexCount();
    }

    items_ = std::move(items);
    meshCount_ = static_cast<uint32_t>(scene.meshes.size());
    vertexCount_ = vertices;
    triangleCount_ = triangles;
    // Loading does not show. The caller decides, so an import that succeeds
    // while the user is looking at the model does not yank the viewport.
    visible_ = false;

    char text[128];
    std::snprintf(text, sizeof(text), "%u mesh%s, %u vertices, %u triangles", meshCount_,
                  meshCount_ == 1 ? "" : "es", vertexCount_, triangleCount_);
    summary_ = text;
    return true;
}

bool ImportedMeshPreview::worldBounds(float* out) const {
    if (out == nullptr || items_.empty()) {
        return false;
    }
    bool first = true;
    for (const SceneDrawItem& item : items_) {
        if (item.mesh == nullptr) {
            continue;
        }
        for (uint32_t v = 0; v < item.mesh->vertexCount(); ++v) {
            const float* position = item.mesh->vertices()[v].position;
            for (int c = 0; c < 3; ++c) {
                if (first) {
                    out[c] = position[c];
                    out[3 + c] = position[c];
                } else {
                    if (position[c] < out[c]) out[c] = position[c];
                    if (position[c] > out[3 + c]) out[3 + c] = position[c];
                }
            }
            first = false;
        }
    }
    return !first;
}

void ImportedMeshPreview::clear() {
    // Releasing the shared_ptrs here is what frees the CPU meshes. The GPU
    // copies go when the renderer next sees a scene that does not name these
    // keys, which is the same path a deleted body would take if the product had
    // one - no special teardown, and none that could run on the wrong thread.
    items_.clear();
    meshCount_ = 0;
    vertexCount_ = 0;
    triangleCount_ = 0;
    visible_ = false;
    summary_.clear();
}

ImportedMeshPreview& importedMeshPreview() {
    static ImportedMeshPreview preview;
    return preview;
}

}  // namespace forgeshape
