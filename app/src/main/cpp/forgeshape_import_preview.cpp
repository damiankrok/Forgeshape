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
    items.reserve(scene.meshes.size());
    uint32_t vertices = 0;
    uint32_t triangles = 0;

    for (size_t i = 0; i < scene.meshes.size(); ++i) {
        const ImportedMesh& mesh = scene.meshes[i];
        if (mesh.positions.empty() || mesh.indices.empty()) {
            return false;
        }

        std::vector<MeshVertex> drawVertices(mesh.vertexCount());
        for (uint32_t v = 0; v < mesh.vertexCount(); ++v) {
            const size_t at = static_cast<size_t>(v) * 3u;
            drawVertices[v].position[0] = mesh.positions[at];
            drawVertices[v].position[1] = mesh.positions[at + 1];
            drawVertices[v].position[2] = mesh.positions[at + 2];
            // A flat neutral. The preview carries no material and this is not
            // the start of one; the parsed NORMALS are kept in the importer's
            // own structures for the diagnostic and are not shading input here.
            drawVertices[v].color[0] = kPreviewColor[0];
            drawVertices[v].color[1] = kPreviewColor[1];
            drawVertices[v].color[2] = kPreviewColor[2];
        }

        // Revision 1 for every preview mesh: a preview has no revision history
        // and never publishes, and `createRuntimeMesh` reserves 0 for "nothing
        // published". The distinct RENDERER KEY, not the revision, is what
        // keeps two previews' buffers apart.
        MeshValidation why = MeshValidation::Ok;
        const RuntimeMeshPtr runtime = createRuntimeMesh(
                kFirstPreviewRenderKey + static_cast<ObjectId>(i), /*revision=*/1,
                drawVertices.data(), static_cast<uint32_t>(drawVertices.size()),
                mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size()), &why,
                /*renderBothSides=*/false);
        if (runtime == nullptr) {
            return false;
        }

        SceneDrawItem item;
        item.objectId = kFirstPreviewRenderKey + static_cast<ObjectId>(i);
        item.mesh = runtime;
        // The node transform, per glTF semantics, and nothing else. The
        // importer already refused any node whose rotation or scale would make
        // this wrong, so the inverse is the negated translation exactly and the
        // normal matrix is the identity — a translation turns no normal.
        item.model = mesh.nodeTransform;
        item.inverseModel = mat4Translation(
                Vec3{-mesh.translation[0], -mesh.translation[1], -mesh.translation[2]});
        item.normalModel = mat4Identity();
        // Never selected. Selection is an identity the preview does not have,
        // and the picker is not given this snapshot in the first place.
        item.selected = false;
        items.push_back(item);

        vertices += mesh.vertexCount();
        triangles += mesh.triangleCount();
    }

    items_ = std::move(items);
    vertexCount_ = vertices;
    triangleCount_ = triangles;
    // Loading does not show. The caller decides, so an import that succeeds
    // while the user is looking at the model does not yank the viewport.
    visible_ = false;

    char text[128];
    std::snprintf(text, sizeof(text), "%u mesh%s, %u vertices, %u triangles", meshCount(),
                  meshCount() == 1 ? "" : "es", vertexCount_, triangleCount_);
    summary_ = text;
    return true;
}

void ImportedMeshPreview::clear() {
    // Releasing the shared_ptrs here is what frees the CPU meshes. The GPU
    // copies go when the renderer next sees a scene that does not name these
    // keys, which is the same path a deleted body would take if the product had
    // one - no special teardown, and none that could run on the wrong thread.
    items_.clear();
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
