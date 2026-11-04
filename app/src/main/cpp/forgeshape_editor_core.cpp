#include "forgeshape_editor_core.h"

#include <algorithm>
#include <utility>

#include "forgeshape_body_commands.h"
#include "forgeshape_cad_extrude_tool.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

CameraController& editorCamera() {
    static CameraController camera;
    return camera;
}

SelectionController& editorSelection() {
    static SelectionController selection;
    return selection;
}

std::mutex& editorStateMutex() {
    static std::mutex mutex;
    return mutex;
}

bool editorActiveBodyDimensions(LocalBounds* outBounds, BodyDimensions* outDimensions) {
    const ConstructionScene& scene = constructionScene();
    if (!scene.hasProject()) {
        return false;
    }
    return sceneBodyDimensions(scene.activeBodyId(), scene, outBounds, outDimensions);
}

bool editorActiveBodyDimensionsEditable(LocalBounds* outBounds, BodyDimensions* outDimensions) {
    const ConstructionScene& scene = constructionScene();
    return editorActiveBodyDimensions(outBounds, outDimensions) && scene.hasProject()
        && bodySizeEditable(scene.activeBody()) && !sculptSession().inSculptMode();
}

namespace {

// ---------------------------------------------------------------------------
// The CAD operation preview (`CAD-VERTICAL-SLICE-R1`)
// ---------------------------------------------------------------------------
//
// What an Add, a Cut or a New Body WILL make, drawn before it is committed. It
// is the sketch session's own candidate evaluation -- the same regeneration
// the commit applies -- handed to the RENDERER'S copy of the scene list only:
// `viewSceneSnapshot()` stays exactly what picking, saving and export read, so
// a preview can never be picked, saved, checkpointed or exported.
//
// Latest-only by construction: the evaluation is keyed by the candidate
// revision and this runs once per frame, so a drag that moves forty times
// between two frames costs at most one regeneration per frame and never
// queues one per MotionEvent. The RuntimeMesh is rebuilt only when the
// evaluation's revision changes, under a revision space of its own (high bit)
// that no body's store can reach, so the renderer's revision-only upload gate
// re-uploads on every change -- and again when the real mesh returns.
constexpr ObjectId kCadNewBodyPreviewRenderKey = 1ull << 61;
constexpr MeshRevision kCadPreviewRevisionBase = 1ull << 62;

struct CadPreviewCache {
    uint64_t evaluationRevision = 0;
    ObjectId key = kNoObject;
    RuntimeMeshPtr mesh;
    MeshRevision nextRevision = kCadPreviewRevisionBase;
};

CadPreviewCache& cadPreviewCache() {
    static CadPreviewCache cache;
    return cache;
}

}  // namespace

void applyCadOperationPreview(SceneSnapshot* list) {
    SketchSession& session = sketchSession();
    if (list == nullptr || session.state() != SketchSessionState::Ready) {
        return;
    }
    const CadCandidateEvaluation& evaluation = session.evaluateCandidate();
    if (evaluation.status != CadStatus::Ok || evaluation.mesh == nullptr) {
        // An invalid candidate shows NO result -- never the last valid one --
        // and the HUD names why.
        return;
    }
    const ObjectId key = evaluation.targetBodyId != kNoObject
                                             ? evaluation.targetBodyId
                                             : kCadNewBodyPreviewRenderKey;
    CadPreviewCache& cache = cadPreviewCache();
    if (cache.mesh == nullptr || cache.evaluationRevision != evaluation.revision
        || cache.key != key) {
        const ConstructionMesh& m = evaluation.mesh->mesh;
        RuntimeMeshPtr runtime = createRuntimeMesh(
                key, cache.nextRevision++, m.vertices.data(),
                static_cast<uint32_t>(m.vertices.size()), m.indices.data(),
                static_cast<uint32_t>(m.indices.size()), nullptr, m.renderBothSides);
        if (runtime == nullptr) {
            return;
        }
        cache.mesh = std::move(runtime);
        cache.evaluationRevision = evaluation.revision;
        cache.key = key;
    }
    float tint[4];
    cadOperationPreviewTint(evaluation.operation, tint);
    if (evaluation.targetBodyId != kNoObject) {
        // The target body is drawn AS its candidate: same key, same model.
        for (SceneDrawItem& item : *list) {
            if (item.objectId == evaluation.targetBodyId) {
                item.mesh = cache.mesh;
                std::copy(tint, tint + 4, item.previewTint);
            }
        }
        return;
    }
    Mat4 model;
    if (!session.candidateWorldModel(constructionScene(), &model)) {
        return;
    }
    SceneDrawItem item;
    item.objectId = key;
    item.mesh = cache.mesh;
    item.model = model;
    if (!mat4AffineInverse(model, &item.inverseModel)) {
        return;
    }
    item.normalModel = mat4NormalMatrix(model);
    item.selected = false;
    std::copy(tint, tint + 4, item.previewTint);
    list->push_back(std::move(item));
}


}  // namespace forgeshape
