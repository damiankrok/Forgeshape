// The editable scene: an ordered collection of Construction Bodies.
//
// Before Stage 017 the product was one object expressed as three process-global
// singletons -- `constructionObject()`, `meshStore()` and `sculptSession()` --
// and "the object" was simply whichever thing each of them held. That is the
// assumption this file removes. A Construction Body is now a `SceneObject` that
// owns all three, the scene owns an ordered list of them, and the three global
// accessors are redefined (in forgeshape_scene.cpp) as "the ACTIVE body's".
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type.
#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_mesh.h"
#include "forgeshape_object_id.h"
#include "forgeshape_sculpt.h"

namespace forgeshape {

// The first Body keeps the identity the single-object product already used, so
// startup behaviour and every existing ObjectId assertion are unchanged.
constexpr ObjectId kFirstBodyObjectId = kConstructionBoxObjectId;

// ---------------------------------------------------------------------------
// One Construction Body
// ---------------------------------------------------------------------------
//
// Everything that is this body's own truth, and nothing that is the scene's.
// A body is deliberately NOT copyable: it owns a MeshStore (which owns a mutex
// and the published revision chain) and a SculptSession, and duplicating either
// would duplicate identity, which is the one thing an ObjectId exists to
// prevent.
//
// Note there is no separate transform member: `ConstructionObject` has always
// owned its own `ConstructionTransform`, precisely so a primitive change cannot
// lose the placement. That stays true per body.
class SceneObject {
public:
    explicit SceneObject(ObjectId id) : construction_(id), meshStore_(id) {}

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;

    // Stable for the life of the body: unaffected by primitive edits, transform
    // edits, mesh revisions, Freeze/Resume/re-Freeze and GPU reallocation.
    ObjectId objectId() const { return construction_.objectId(); }

    ConstructionObject& construction() { return construction_; }
    const ConstructionObject& construction() const { return construction_; }

    ConstructionTransform& transform() { return construction_.transform(); }
    const ConstructionTransform& transform() const { return construction_.transform(); }

    MeshStore& meshStore() { return meshStore_; }
    const MeshStore& meshStore() const { return meshStore_; }

    // This body's Frozen Sculpt Mesh and its stale flag — the per-body half of
    // sculpting. The mode, the tool, the brush and any stroke in progress are
    // NOT here: they belong to the one editing session, so switching bodies
    // cannot silently change the brush. See FrozenSculpt.
    FrozenSculpt& frozenSculpt() { return frozen_; }
    const FrozenSculpt& frozenSculpt() const { return frozen_; }

private:
    ConstructionObject construction_;
    MeshStore meshStore_;
    FrozenSculpt frozen_;
};

// ---------------------------------------------------------------------------
// An immutable view of the whole renderable scene
// ---------------------------------------------------------------------------
//
// What the renderer and CPU picking consume. Taking one copies a `shared_ptr`
// and two matrices per body and NO geometry, so it is cheap enough to take
// under the state mutex and then use with that mutex released -- which is the
// point: no lock is ever held across normal generation, a GPU upload or a
// triangle scan.
//
// Each item carries its own ObjectId and its own mesh revision, so publishing a
// new revision for one body cannot replace or invalidate another's.
struct SceneDrawItem {
    ObjectId objectId = kNoObject;
    RuntimeMeshPtr mesh;  // immutable; keeps this exact revision alive
    Mat4 model;
    Mat4 inverseModel;
    // How a NORMAL is carried out of object space: R * S^-1, the inverse
    // transpose of the model's upper-left 3x3. It is carried beside the model
    // rather than derived from it because a non-uniform scale makes the two
    // genuinely different matrices, and a renderer that reached for `model`
    // would shade a stretched body wrong in a way nothing else would show.
    // Equal to the model's rotation for every unscaled body.
    Mat4 normalModel;
    bool selected = false;
};

using SceneSnapshot = std::vector<SceneDrawItem>;

// ---------------------------------------------------------------------------
// The scene
// ---------------------------------------------------------------------------
//
// A flat, root-level, insertion-ordered collection. Stage 017 deliberately has
// no parent/child, no groups and no reordering, and carries no speculative
// fields for them.
//
// Not internally synchronised: callers hold the one existing state mutex, the
// same way they already did for the singletons this replaces. That keeps the
// lock order unchanged (state mutex, then MeshStore's own mutex inside
// publish/current) rather than introducing a second scene-level lock.
class ConstructionScene {
public:
    // Creates the first Body, matching the single-object product's startup
    // exactly: one default Box at identity, selected.
    ConstructionScene();

    // Appends a new Body with the same defaults as the startup Body, mints it a
    // fresh ObjectId, and makes it active. Returns it.
    SceneObject& addBody();

    size_t bodyCount() const { return bodies_.size(); }

    // Enumeration is by insertion order and is stable across edits, mode
    // changes and selection changes.
    SceneObject& bodyAt(size_t index) { return *bodies_[index]; }
    const SceneObject& bodyAt(size_t index) const { return *bodies_[index]; }

    // nullptr when no body carries that id. Never a dangling reference: bodies
    // are held by unique_ptr, so their addresses are stable as the list grows.
    SceneObject* findBody(ObjectId id);
    const SceneObject* findBody(ObjectId id) const;

    ObjectId activeBodyId() const { return activeBodyId_; }

    // Selection only. Publishes nothing, mints no revision, and cannot change
    // any body's ObjectId. Returns false (changing nothing) for an unknown id.
    bool setActiveBody(ObjectId id);

    SceneObject& activeBody();
    const SceneObject& activeBody() const;

    // Every body that currently has something published, in scene order.
    SceneSnapshot snapshot() const;

    // -----------------------------------------------------------------------
    // History support: the smallest scene mutations an undo needs
    // -----------------------------------------------------------------------
    //
    // These three exist so that undoing a creation can put a body back with the
    // ObjectId it already had, which `addBody()` — which mints — cannot do.
    // They are deliberately NOT a Delete/Duplicate feature: nothing in the
    // product's UI reaches them, they are driven only by ConstructionHistory,
    // and a detached body is HELD by the history rather than destroyed, so a
    // redo returns the same object with its Frozen Sculpt Mesh intact rather
    // than a fresh one that merely looks the same.

    // Takes a body out of the scene and hands over ownership. Returns null when
    // no body carries that id. If the detached body was active, the selection
    // falls to whichever body is first, so the scene never has an active id
    // pointing at nothing.
    std::unique_ptr<SceneObject> detachBody(ObjectId id);

    // Puts a body back at an exact position in scene order. `index` is clamped
    // to the end. Selection is not changed: the caller decides.
    void insertBody(std::unique_ptr<SceneObject> body, size_t index);

    // Builds a body with an EXPLICIT id, not appended to anything.
    //
    // The id allocator is only ever pushed forward, never rolled back: a redo
    // restores id 5 by name, and a subsequent creation mints 6 rather than
    // reusing 5. Reuse is what would let a stale ObjectId held anywhere — a
    // selection, a render snapshot — silently resolve to a different body.
    std::unique_ptr<SceneObject> makeBody(ObjectId id);

    // Where a body sits in scene order, or bodyCount() when it is not present.
    size_t indexOfBody(ObjectId id) const;

private:
    // unique_ptr rather than by value: SceneObject holds a mutex through
    // MeshStore and must not move when the vector grows.
    std::vector<std::unique_ptr<SceneObject>> bodies_;

    // Monotonic. Never reused, never derived from a collection index, never
    // derived from a revision. Stage 017 has no delete, so there is deliberately
    // no reuse policy to design.
    ObjectId nextObjectId_ = kFirstBodyObjectId;

    ObjectId activeBodyId_ = kNoObject;
};

// The one process-scoped scene. `constructionObject()`, `meshStore()` and
// `sculptSession()` are defined in terms of this scene's ACTIVE body.
ConstructionScene& constructionScene();

}  // namespace forgeshape
