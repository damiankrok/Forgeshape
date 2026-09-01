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
#include <string>
#include <vector>

#include "forgeshape_construction.h"
#include "forgeshape_imported_mesh.h"
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
// The transform is the BODY's, not the Construction Source's. It lived inside
// `ConstructionObject` while every body was a Construction Body, so that a
// primitive change could not lose the placement; `IMPORT-01A` moved it up here
// because a body now has a choice of representation and an Imported Mesh has a
// placement with no primitive to hang it on. Still exactly one per body, and
// still never touched by a primitive change.

// ---------------------------------------------------------------------------
// Which representation a body's geometry comes from
// ---------------------------------------------------------------------------
//
// A body owns exactly ONE of these, for its whole life. There is no conversion
// between them in `IMPORT-01A`: an Imported Mesh never grows a Construction
// Source (nothing could invent the primitive it was never made from), and a
// Construction Body never becomes one.
enum class BodyRepresentation : uint8_t {
    // The exact primitive plus its parameters. Geometry is DERIVED and is
    // regenerated on every load.
    Construction = 1,
    // Polygon geometry read from a file. Geometry IS the truth: no rule could
    // recreate it, so it is serialized.
    Imported = 2,
};

const char* bodyRepresentationName(BodyRepresentation representation);

class SceneObject {
public:
    // A Construction Body: the only kind that existed before `IMPORT-01A`, and
    // still what every creation path in the product makes.
    explicit SceneObject(ObjectId id)
        : objectId_(id),
          representation_(BodyRepresentation::Construction),
          construction_(new ConstructionObject(id)),
          meshStore_(id) {}

    // An Imported Mesh body. Takes the geometry by value because the body OWNS
    // it: there is no source file to go back to and nothing else holds a copy.
    SceneObject(ObjectId id, ImportedMesh mesh, std::string name)
        : objectId_(id),
          representation_(BodyRepresentation::Imported),
          meshStore_(id),
          imported_(std::move(mesh)),
          name_(std::move(name)) {}

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;

    // Stable for the life of the body: unaffected by primitive edits, transform
    // edits, mesh revisions, Freeze/Resume/re-Freeze and GPU reallocation.
    ObjectId objectId() const { return objectId_; }

    BodyRepresentation representation() const { return representation_; }
    bool hasConstructionSource() const {
        return representation_ == BodyRepresentation::Construction;
    }
    bool isImported() const { return representation_ == BodyRepresentation::Imported; }

    // The Construction Source, or nullptr for an Imported Mesh.
    //
    // Deliberately a POINTER rather than a reference: before `IMPORT-01A` every
    // body had one and no caller could be wrong, and the whole point of the
    // change is that a caller now has to say what it does about a body that has
    // none. A nullable return makes the compiler ask that question at every one
    // of the call sites rather than leaving a silent assumption behind.
    ConstructionObject* constructionOrNull() { return construction_.get(); }
    const ConstructionObject* constructionOrNull() const { return construction_.get(); }

    // For the many callers that have already established this is a Construction
    // Body. Undefined for an Imported Mesh, exactly like dereferencing the
    // pointer above would be — this only spells the intent.
    ConstructionObject& construction() { return *construction_; }
    const ConstructionObject& construction() const { return *construction_; }

    // The imported geometry, or nullptr for a Construction Body.
    const ImportedMesh* importedOrNull() const {
        return representation_ == BodyRepresentation::Imported ? &imported_ : nullptr;
    }

    // The body's stored name, empty for a Construction Body.
    //
    // Only an Imported Mesh carries one: it arrives named by the file it came
    // from, and losing that would leave the user with a list of anonymous rows
    // they could not tell apart. A Construction Body is still labelled from its
    // ObjectId by the UI, exactly as before, and this product still has no
    // Rename.
    const std::string& name() const { return name_; }

    // THE body's placement, whichever representation it has.
    //
    // Hoisted here from `ConstructionObject` by `IMPORT-01A`: a body has a
    // placement because it is a body, not because it is a primitive. Still
    // exactly one per body, still the authoritative value the renderer, the
    // picker and the exact-value editors all read.
    ConstructionTransform& transform() { return transform_; }
    const ConstructionTransform& transform() const { return transform_; }

    MeshStore& meshStore() { return meshStore_; }
    const MeshStore& meshStore() const { return meshStore_; }

    // This body's Frozen Sculpt Mesh and its stale flag — the per-body half of
    // sculpting. The mode, the tool, the brush and any stroke in progress are
    // NOT here: they belong to the one editing session, so switching bodies
    // cannot silently change the brush. See FrozenSculpt.
    FrozenSculpt& frozenSculpt() { return frozen_; }
    const FrozenSculpt& frozenSculpt() const { return frozen_; }

private:
    const ObjectId objectId_;
    const BodyRepresentation representation_;
    // Null for an Imported Mesh. Held by pointer rather than by optional so
    // that "this body has no Construction Source" is one null check and not a
    // second kind of emptiness beside the representation enum.
    std::unique_ptr<ConstructionObject> construction_;
    ConstructionTransform transform_;
    MeshStore meshStore_;
    FrozenSculpt frozen_;
    // Empty for a Construction Body. `IMPORT-01A` deliberately does not give an
    // Imported Mesh a Frozen Sculpt Mesh either: Start Sculpting on one is
    // `IMPORT-01B`.
    ImportedMesh imported_;
    std::string name_;
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

    // The id allocator's high-water mark: the id the NEXT mint will hand out.
    // Read by the project codec, which stores it so that a reopened project can
    // never mint an id one of its own loaded bodies is already wearing.
    ObjectId nextObjectId() const { return nextObjectId_; }

    // Pushes the allocator forward so that nothing at or below `highest` can
    // ever be minted again.
    //
    // MONOTONIC by construction: a request that would move it backwards is
    // ignored, because rolling the allocator back is precisely what would let a
    // stale ObjectId held in a selection or a render snapshot resolve to a
    // different body. Publishes nothing, mints nothing and touches no body.
    void reserveObjectIdsThrough(ObjectId highest);

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

    // Appends an already-built Imported Mesh body, minting it a normal
    // ObjectId, and makes it active. Returns it, or nullptr when the geometry
    // is not valid — in which case the scene is untouched.
    //
    // The SAME allocator every Construction Body uses: an imported object's
    // identity is an ordinary ForgeShape identity, not a second kind of key.
    SceneObject* addImportedBody(ImportedMesh mesh, const std::string& name);

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

// Publishes whichever representation this body owns.
//
// ONE dispatch point, so no caller has to ask what a body is before it can put
// its geometry on screen: a Construction Body regenerates from its parameters
// through `publishConstructionObject`, an Imported Mesh republishes the geometry
// it already owns. Neither path reads the other's truth, and neither invents a
// representation the body does not have.
//
// Returns kNoMeshRevision, leaving the store untouched, when the geometry does
// not pass RuntimeMesh validation.
MeshRevision publishSceneObject(SceneObject& body, MeshValidation* outWhy = nullptr);

// The one process-scoped scene. `constructionObject()`, `meshStore()` and
// `sculptSession()` are defined in terms of this scene's ACTIVE body.
ConstructionScene& constructionScene();

}  // namespace forgeshape
