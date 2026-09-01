#include "forgeshape_scene.h"

#include <algorithm>

namespace forgeshape {

ConstructionScene::ConstructionScene() {
    // The first Body is created eagerly and selected, so the product comes up
    // exactly as the single-object version did: one default Box, at identity,
    // already the edit target. Nothing about startup asks the user to create an
    // object first.
    addBody();
}

SceneObject& ConstructionScene::addBody() {
    const ObjectId id = nextObjectId_++;
    bodies_.push_back(std::make_unique<SceneObject>(id));
    // A new Body is what the user is now working on. Selecting it here rather
    // than at the call site keeps "added" and "active" from ever disagreeing.
    activeBodyId_ = id;
    return *bodies_.back();
}

SceneObject* ConstructionScene::addImportedBody(ImportedMesh mesh, const std::string& name) {
    if (!mesh.valid()) {
        // Refused before an id is minted. A consumed ObjectId for a body that
        // was never added is a gap the allocator can never explain, and the
        // import contract is that a failure costs nothing at all.
        return nullptr;
    }
    const ObjectId id = nextObjectId_++;
    bodies_.push_back(std::unique_ptr<SceneObject>(new SceneObject(id, std::move(mesh), name)));
    activeBodyId_ = id;
    return bodies_.back().get();
}

SceneObject* ConstructionScene::findBody(ObjectId id) {
    for (auto& body : bodies_) {
        if (body->objectId() == id) {
            return body.get();
        }
    }
    return nullptr;
}

const SceneObject* ConstructionScene::findBody(ObjectId id) const {
    for (const auto& body : bodies_) {
        if (body->objectId() == id) {
            return body.get();
        }
    }
    return nullptr;
}

bool ConstructionScene::setActiveBody(ObjectId id) {
    if (findBody(id) == nullptr) {
        return false;  // fails closed: an unknown id leaves the selection alone
    }
    activeBodyId_ = id;
    return true;
}

SceneObject& ConstructionScene::activeBody() {
    SceneObject* body = findBody(activeBodyId_);
    // The constructor creates and selects a Body and nothing can delete one, so
    // an active body always exists. The fallback is a belt-and-braces guard
    // that keeps this reference-returning accessor total rather than UB if that
    // ever stops being true.
    return (body != nullptr) ? *body : *bodies_.front();
}

const SceneObject& ConstructionScene::activeBody() const {
    const SceneObject* body = findBody(activeBodyId_);
    return (body != nullptr) ? *body : *bodies_.front();
}

size_t ConstructionScene::indexOfBody(ObjectId id) const {
    for (size_t i = 0; i < bodies_.size(); ++i) {
        if (bodies_[i]->objectId() == id) {
            return i;
        }
    }
    return bodies_.size();
}

std::unique_ptr<SceneObject> ConstructionScene::detachBody(ObjectId id) {
    const size_t index = indexOfBody(id);
    if (index == bodies_.size()) {
        return nullptr;
    }
    std::unique_ptr<SceneObject> body = std::move(bodies_[index]);
    bodies_.erase(bodies_.begin() + static_cast<std::ptrdiff_t>(index));
    if (activeBodyId_ == id) {
        // Never leave the selection pointing at a body that is gone: every
        // reference-returning accessor here assumes an active body exists.
        activeBodyId_ = bodies_.empty() ? kNoObject : bodies_.front()->objectId();
    }
    return body;
}

void ConstructionScene::insertBody(std::unique_ptr<SceneObject> body, size_t index) {
    if (!body) {
        return;
    }
    const size_t at = std::min(index, bodies_.size());
    bodies_.insert(bodies_.begin() + static_cast<std::ptrdiff_t>(at), std::move(body));
    if (activeBodyId_ == kNoObject) {
        activeBodyId_ = bodies_[at]->objectId();
    }
}

void ConstructionScene::reserveObjectIdsThrough(ObjectId highest) {
    if (highest >= nextObjectId_) {
        nextObjectId_ = highest + 1;
    }
}

std::unique_ptr<SceneObject> ConstructionScene::makeBody(ObjectId id) {
    if (id >= nextObjectId_) {
        nextObjectId_ = id + 1;  // monotonic: an id is never handed out twice
    }
    return std::make_unique<SceneObject>(id);
}

SceneSnapshot ConstructionScene::snapshot() const {
    SceneSnapshot items;
    items.reserve(bodies_.size());
    for (const auto& body : bodies_) {
        RuntimeMeshPtr mesh = body->meshStore().current();
        if (!mesh) {
            continue;  // nothing published for this body yet: not drawable, not pickable
        }
        SceneDrawItem item;
        item.objectId = body->objectId();
        item.mesh = std::move(mesh);
        item.model = body->transform().modelMatrix();
        item.inverseModel = body->transform().inverseModelMatrix();
        item.normalModel = body->transform().normalMatrix();
        // Highlighting is per body. A single global "something is selected"
        // flag would tint every body at once the moment anything was picked.
        item.selected = (body->objectId() == activeBodyId_);
        items.push_back(std::move(item));
    }
    return items;
}

ConstructionScene& constructionScene() {
    static ConstructionScene scene;
    return scene;
}

// ---------------------------------------------------------------------------
// The active-body accessors
// ---------------------------------------------------------------------------
//
// These three are DECLARED by forgeshape_construction.h, forgeshape_mesh.h and
// forgeshape_sculpt.h respectively, and were previously defined next to their
// own types as plain function-local statics. They are defined HERE instead
// because their answer is now a scene question -- "the active body's" -- and
// defining them in their own translation units would make those units depend on
// the scene, which depends on them: a cycle.
//
// Keeping the same three names is what makes this migration small. Every
// existing caller that means "the object the user is editing" keeps working
// unchanged and correctly; only code that means "every body in the scene" (the
// renderer and scene picking) had to change, and that is exactly the code
// Stage 017 rewrote.
const char* bodyRepresentationName(BodyRepresentation representation) {
    switch (representation) {
        case BodyRepresentation::Construction: return "Construction";
        case BodyRepresentation::Imported: return "Imported";
    }
    return "unknown";
}

MeshRevision publishSceneObject(SceneObject& body, MeshValidation* outWhy) {
    if (const ConstructionObject* source = body.constructionOrNull()) {
        return publishConstructionObject(*source, body.meshStore(), outWhy);
    }
    const ImportedMesh* imported = body.importedOrNull();
    if (imported == nullptr) {
        if (outWhy != nullptr) {
            *outWhy = MeshValidation::EmptyVertices;
        }
        return kNoMeshRevision;
    }
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
    if (!imported->buildDrawData(&vertices, &indices)) {
        if (outWhy != nullptr) {
            *outWhy = MeshValidation::EmptyVertices;
        }
        return kNoMeshRevision;
    }
    // `renderBothSides` is deliberately false: an imported object's
    // double-sided submeshes already carry their reversed triangles, emitted
    // per batch by buildDrawData, because that flag is one answer for a whole
    // mesh and an imported object may need a different one per submesh.
    return body.meshStore().publish(vertices.data(), static_cast<uint32_t>(vertices.size()),
                                    indices.data(), static_cast<uint32_t>(indices.size()),
                                    outWhy, /*renderBothSides=*/false);
}

ConstructionObject& constructionObject() { return constructionScene().activeBody().construction(); }

MeshStore& meshStore() { return constructionScene().activeBody().meshStore(); }

// ONE editing session for the whole product, re-pointed at the active body's
// Frozen Sculpt Mesh on every access.
//
// The split is the point. Which mode the product is in, which tool is held, how
// big and how strong the brush is, and any stroke in progress describe the
// EDITING SESSION and must not change when the user switches bodies — "Radius
// and Strength are shared" is a product contract, and a per-body copy would
// break it silently. What IS per body is the Frozen Sculpt Mesh and its
// stale flag, and those the session borrows rather than owns.
//
// Rebinding on every call rather than only when the selection changes is one
// pointer write, and it removes the whole class of bug where the session is
// left pointing at the body the user just navigated away from.
SculptSession& sculptSession() {
    static SculptSession session;
    session.bindTarget(&constructionScene().activeBody().frozenSculpt());
    return session;
}

}  // namespace forgeshape
