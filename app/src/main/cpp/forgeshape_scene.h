// The editable scene: an ordered collection of bodies, each a Construction
// Body, an Imported Mesh or a CAD Body.
//
// A body is a `SceneObject` that owns its source representation, its MeshStore,
// its Frozen Sculpt Mesh and its placement; the scene owns an ordered list of
// them, the monotonic ObjectId allocator and which body is active. The
// process-scoped accessors in forgeshape_scene.cpp (`meshStore()`,
// `sculptSession()`, `activeConstructionOrNull()`, `constructionTransform()`)
// mean "the ACTIVE body's", and each answers for "no project" without reading
// `activeBody()` (`APP-H1`).
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
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
// One body
// ---------------------------------------------------------------------------
//
// Everything that is this body's own truth, and nothing that is the scene's.
// A body is deliberately NOT copyable: it owns a MeshStore (which owns a mutex
// and the published revision chain) and a FrozenSculpt, and duplicating either
// would duplicate identity, which is the one thing an ObjectId exists to
// prevent.
//
// The transform is the BODY's, not the representation's: a body has a placement
// because it is a body, whatever generates its geometry, and no primitive or
// sketch change ever touches it.

// ---------------------------------------------------------------------------
// Which representation a body's geometry comes from
// ---------------------------------------------------------------------------
//
// A body owns exactly ONE of these, for its whole life. There is no conversion
// between them: an Imported Mesh never grows a Construction Source (nothing
// could invent the primitive it was never made from), and a Construction Body
// never becomes one. `IMPORT-01B` does not weaken that -- it lets EITHER of
// them be sculpted, and a Frozen Sculpt Mesh is a body's second
// representation rather than a change of its first.
enum class BodyRepresentation : uint8_t {
    // The exact primitive plus its parameters. Geometry is DERIVED and is
    // regenerated on every load.
    Construction = 1,
    // Polygon geometry read from a file. Geometry IS the truth: no rule could
    // recreate it, so it is serialized.
    Imported = 2,
    // A sketch on a workplane, extruded (`CAD-R0-A1A2`). The sketch and the
    // extrusion are the truth; the mesh is DERIVED and regenerated, exactly as
    // a primitive's is. Its own representation because it has no primitive to
    // be a Construction Source with, and no fixed geometry to be an Imported
    // Mesh with.
    Cad = 3,
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

    // A CAD Body. Takes the authored state by value because the body OWNS
    // it; the caller has already validated that it regenerates (see
    // ConstructionScene::addCadBody), so this cannot fail.
    SceneObject(ObjectId id, CadBodyState cad)
        : objectId_(id),
          representation_(BodyRepresentation::Cad),
          cad_(new CadBody(id, std::move(cad))),
          meshStore_(id) {}

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
    bool isCad() const { return representation_ == BodyRepresentation::Cad; }

    // The CAD Body, or nullptr for any other representation. A pointer for the
    // same reason `constructionOrNull()` is one: every call site has to say
    // what it does about a body that has none.
    CadBody* cadOrNull() { return cad_.get(); }
    const CadBody* cadOrNull() const { return cad_.get(); }

    // The face support of a FACE-SUPPORTED CAD body (`CAD-A3`), or nullptr for
    // every other body -- including a world-plane CAD body, whose placement is
    // its own. A face-supported body's world placement is DERIVED from the
    // producer this names, so `transform()` is not its placement and the gizmo
    // and the exact-value editors refuse it.
    const TopoRef* cadFaceSupportOrNull() const {
        return (cad_ && cad_->sketch().hasFaceSupport) ? &cad_->sketch().faceSupport : nullptr;
    }
    bool isFaceSupportedCad() const { return cadFaceSupportOrNull() != nullptr; }

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

    // The body's stored name, empty when it has none.
    //
    // An Imported Mesh arrives with one, named by the file it came from. Since
    // Stage 018A EVERY representation may carry one, because Rename is
    // representation-neutral: a body has a name because it is a body, exactly
    // as it has a placement because it is a body. Empty is still the SIGNAL for
    // the UI's ObjectId-derived fallback label and is never itself a name.
    const std::string& name() const { return name_; }

    // Renames the body. The caller has already sanitized and accepted the
    // string (see `renameSceneBody`, the one entry point) -- this only stores
    // it. Publishes nothing, mints no MeshRevision and moves no vertex: a name
    // is project truth about identity, not about geometry.
    void setName(std::string name) { name_ = std::move(name); }

    // -----------------------------------------------------------------------
    // Visibility and lock (Stage 018A)
    // -----------------------------------------------------------------------
    //
    // Both are DURABLE project truth and both are representation-neutral: a
    // body is hidden or locked because the user said so about that body, and no
    // rule here asks what generates its geometry. Both are carried by a
    // Construction history step and both reach the `.forge` file, which is the
    // difference between them and every presentation flag in the product.
    //
    // Neither is geometry. Toggling one publishes nothing, mints no
    // MeshRevision, rebuilds no CAD mesh, uploads nothing and moves no sculpt
    // vertex; what changes is whether `ConstructionScene::snapshot` offers the
    // body at all, and whether the transform entry points accept a write.

    // Hidden is NOT DRAWN and NOT PICKED, and it is one fact rather than two:
    // `snapshot()` is the single list the renderer and CPU picking both
    // consume, so leaving a hidden body out of it makes both true at once with
    // no renderer branch and no second predicate that could drift. A hidden
    // body is still in the scene, still in the Objects list, still selectable
    // from its row, still saved and still exported -- hiding is not deleting.
    bool visible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }

    // Locked stays VISIBLE and stays PICKABLE, deliberately.
    //
    // What lock removes is the ability to MOVE the body: the gizmo is refused
    // over one and every transform write is refused by name. It is not removed
    // from the viewport, because a body you can see and select but not move is
    // exactly what a lock means to a user, and a body that silently stopped
    // responding to taps would read as a rendering fault rather than as a
    // state. Reaching Unlock therefore needs no special path -- the ordinary
    // row is still there and so is the ordinary tap.
    //
    // This is not permission or security: it is one boolean the user sets and
    // clears, and every guard it drives is local to this process.
    bool locked() const { return locked_; }
    void setLocked(bool locked) { locked_ = locked; }

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
    // Null for every representation but Cad, on the same terms.
    std::unique_ptr<CadBody> cad_;
    ConstructionTransform transform_;
    MeshStore meshStore_;
    FrozenSculpt frozen_;
    // Empty for a Construction Body. Since `IMPORT-01B` an Imported Mesh may
    // also own a Frozen Sculpt Mesh above -- the two live side by side, and
    // this one stays immutable source truth whatever is sculpted from it.
    ImportedMesh imported_;
    std::string name_;
    // Both default to the state a body has always been in, which is what makes
    // a `.forge` file written before Stage 018A load correctly: no flags in the
    // file means visible and unlocked, and that is these two initializers
    // rather than a migration.
    bool visible_ = true;
    bool locked_ = false;
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

// A restriction the CALLER hands `ConstructionScene::snapshot` (Stage027,
// Sculpt Isolate). The scene never reaches out for one: it is a value it is
// GIVEN, so the scene learns no presentation concept and reads no session,
// while the one list the renderer draws and CPU picking casts against is
// still decided in exactly one loop. It composes with durable visibility --
// it can only remove bodies from the list, never put a hidden one back.
struct SceneViewRestriction {
    // The one body the list is restricted to; `kNoObject` is no restriction.
    // An id no body carries yields an EMPTY list rather than a guessed
    // substitute: nothing is drawn in place of a body that is gone.
    ObjectId isolateTo = kNoObject;
};

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
// The one way to build a scene that holds NO project. See ConstructionScene.
struct NoProjectTag {};

class ConstructionScene {
public:
    // Creates the first Body, matching the single-object product's startup
    // exactly: one default Box at identity, selected. What every self-test and
    // every loaded project starts from.
    ConstructionScene();

    // Creates a scene with NO body: the "no project is open" state (`APP-H1`).
    //
    // The process scene starts this way. Home is not a project, and a project
    // is never empty -- Delete still refuses the last body (`RefusedLastBody`)
    // -- so the two facts are one fact: a project is open exactly when the
    // scene holds at least one body. Nothing fabricates a default primitive or
    // a placeholder body behind Home; the first body of a new project arrives
    // through the CAD bootstrap's first commit, the Sculpt bootstrap's seed or
    // a load, and never before the user asked for one.
    explicit ConstructionScene(NoProjectTag) {}

    // Whether a project is open: at least one body. The ONE answer the Android
    // shell, the codec and the autosave read; no second flag exists.
    bool hasProject() const { return !bodies_.empty(); }

    // Closes the project: every body is destroyed and nothing is selected. The
    // scene is then the no-project scene the process started with. The id
    // allocator is NOT rolled back -- ids stay unique for the life of the
    // process, so a renderer resource keyed by an old project's ObjectId can
    // never be mistaken for a new project's body. Callers drop the history,
    // the sketch and the sculpt mode beside it; this touches only the bodies.
    void closeProject();

    // How many times `activeBody()` was asked for a body while NO project was
    // open. Zero in every product flow; a non-zero count is a shell that read
    // the active body across Home or the CAD bootstrap and is a defect the
    // device suite asserts against. Process-wide, never reset.
    static uint64_t activeBodyMisuseCount();

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

    // The active body. TOTAL while a project is open: the constructor of a
    // project scene creates and selects a Body, Delete refuses the last one and
    // a load selects the document's active body, so an open project always has
    // one. While NO project is open there is no body to return, and the answer
    // is a process-static null object that is in no scene, is never rendered,
    // picked, saved or edited, and wears no allocated id -- returned rather
    // than dereferencing an empty list, and COUNTED (activeBodyMisuseCount) so
    // the product can prove it never reads a body across Home. The four global
    // accessors (`activeConstructionOrNull`, `meshStore`, `sculptSession`,
    // `constructionTransform`) ask `hasProject()` first and never reach this.
    SceneObject& activeBody();
    const SceneObject& activeBody() const;

    // Every body that currently has something published, in scene order. Each
    // item's model is the body's RESOLVED world model: its own placement for an
    // independent body, and the producer's world model composed with the
    // resolved face frame for a face-supported CAD body (`CAD-A3`). A
    // face-supported body whose support cannot resolve is left out.
    //
    // `restriction` is a value the caller hands in (see SceneViewRestriction);
    // the default is no restriction, which is every caller that is not the
    // viewport's own list.
    SceneSnapshot snapshot(const SceneViewRestriction& restriction = {}) const;

    // The world model matrix a body draws and picks at, resolving a face
    // support against its producer chain. Returns false -- writing nothing --
    // for an unknown body, a broken or stale dependency, or a cycle. Bounded by
    // the body count, so a corrupt graph fails closed rather than recursing.
    bool resolveWorldModel(ObjectId id, Mat4* outModel) const;

    // The CAD bodies whose face support names `id` as their producer, and
    // whether there are any. Used by Delete (a producer with dependents is
    // refused) and by the dependency graph.
    std::vector<ObjectId> cadDependentsOf(ObjectId id) const;
    bool hasCadDependents(ObjectId id) const;

    // Whether a face support resolves against the scene right now: the producer
    // exists and is a CAD body, its topology signature still matches the
    // reference's lineage token, and the named face resolves and is eligible.
    // `Ok`, or the reason it does not.
    CadStatus validateCadFaceSupport(const TopoRef& support) const;

    // -----------------------------------------------------------------------
    // History support: the smallest scene mutations an undo needs
    // -----------------------------------------------------------------------
    //
    // These exist so that undoing a creation can put a body back with the
    // ObjectId it already had, which `addBody()` -- which mints -- cannot do.
    // They are still not a feature on their own: they are driven by
    // ConstructionHistory and by `deleteSceneBody`, both of which HOLD a
    // detached body rather than destroying it, so an undo or a redo returns the
    // same object with its Frozen Sculpt Mesh intact rather than a fresh one
    // that merely looks the same. Stage 018A's Duplicate is built on the same
    // two calls: it creates through the ordinary `add*Body` entry points rather
    // than here, because a copy is minted a fresh id like any other creation.

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

    // Appends an already-validated CAD Body, minting it a normal ObjectId, and
    // makes it active. Returns it, or nullptr -- writing why to `outWhy` and
    // leaving the scene untouched -- when the state does not regenerate.
    //
    // Validated by regenerating ONCE before the id is minted, so a refusal
    // costs no id: the same rule `addImportedBody` follows.
    SceneObject* addCadBody(CadBodyState state, CadStatus* outWhy = nullptr);

    // Builds a body with an EXPLICIT id, not appended to anything.
    //
    // The id allocator is only ever pushed forward, never rolled back: a redo
    // restores id 5 by name, and a subsequent creation mints 6 rather than
    // reusing 5. Reuse is what would let a stale ObjectId held anywhere — a
    // selection, a render snapshot — silently resolve to a different body.
    std::unique_ptr<SceneObject> makeBody(ObjectId id);

    // The same, for a CAD Body whose state a history step holds. A CAD Body IS
    // derivable from its state -- unlike an Imported Mesh -- so a step can
    // rebuild one that was never held. The state is NOT re-validated here: it
    // was authoritative when captured.
    std::unique_ptr<SceneObject> makeCadBody(ObjectId id, const CadBodyState& state);

    // Where a body sits in scene order, or bodyCount() when it is not present.
    size_t indexOfBody(ObjectId id) const;

private:
    // The recursive worker behind resolveWorldModel, carrying the recursion
    // depth so a dependency cycle is bounded rather than a stack overflow.
    bool resolveWorldModelDepth(ObjectId id, Mat4* outModel, int depth) const;

    // unique_ptr rather than by value: SceneObject holds a mutex through
    // MeshStore and must not move when the vector grows.
    std::vector<std::unique_ptr<SceneObject>> bodies_;

    // Monotonic. Never reused, never derived from a collection index, never
    // derived from a revision -- and `IMPORT-01B`'s Delete does NOT roll it
    // back. A deleted body's id is restored by name when its Undo puts the same
    // object back, and the next creation mints a fresh one; reuse is what would
    // let a stale ObjectId held in a selection or a render snapshot silently
    // resolve to a different body.
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

// Builds the LOCAL triangle mesh a Frozen Sculpt Mesh would be frozen FROM.
//
// The second ONE dispatch point, beside `publishSceneObject`, and it exists for
// the same reason: `IMPORT-01B` lets either representation be sculpted, so no
// caller has to ask what a body is before it can ask for something to sculpt. A
// Construction Body regenerates from its parameters through the generator the
// product already publishes from; an Imported Mesh hands over the arrays it
// already owns. Neither reads the other's truth, and the source is only READ --
// this cannot change a primitive parameter, a placement or one imported vertex.
//
// `ConstructionMesh` is the type only because it is already this codebase's
// plain carrier for "vertices, indices and a sidedness answer": the `.forge`
// sculpt restore has built one out of stored bytes since E2E-R1A. It carries no
// Construction meaning here, and nothing about the returned mesh claims the body
// has a Construction Source.
//
// For an Imported Mesh two things are deliberately NOT done:
//
//   * the RAW index array is copied, never `buildDrawData`'s. That one emits a
//     double-sided submesh's triangles a SECOND time with reversed winding,
//     which is right for drawing and ruinous for sculpting: the reversed copy
//     contributes the exact negation of its twin to every area-weighted vertex
//     normal, so a two-sided submesh would freeze with zero normals and no
//     normal-based brush could move it at all;
//   * so per-submesh `doubleSided` collapses into the mesh's ONE
//     `renderBothSides`, true when ANY submesh is two-sided. A frozen mesh has a
//     single sidedness answer by construction, and the safe collapse is the
//     permissive one: it keeps an imported sheet both visible and reachable by a
//     brush from behind, where the strict one would leave the user unable to
//     touch half of what they imported.
//
// Returns false, writing nothing, for a body with no geometry to freeze -- and
// for a CAD Body, DELIBERATELY: CAD -> Sculpt is not implemented (the wording
// of the way back out of Sculpt, the stale-source rule over a CAD edit and the
// `CADB`+`SCUL` file combination each need an owner decision). The control is
// absent for a CAD Body and the freeze refuses by name.
bool buildSculptSourceMesh(const SceneObject& body, ConstructionMesh* out);

// The one process-scoped scene. `activeConstructionOrNull()`, `meshStore()`,
// `sculptSession()` and `constructionTransform()` are all defined in terms of
// this scene's ACTIVE body.
ConstructionScene& constructionScene();

// THE list the viewport draws and taps pick against: the process scene's
// snapshot under the restriction the Sculpt session currently holds (Sculpt
// Isolate, Stage027). The renderer and `pickScene` both call this and nothing
// else, so "not drawn" and "not picked" stay one fact under an isolate exactly
// as they are under Hide -- neither consumer carries a predicate of its own.
// Caller holds the state mutex, as for every other read of the scene.
SceneSnapshot viewSceneSnapshot();

}  // namespace forgeshape
