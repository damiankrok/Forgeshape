#include "forgeshape_body_commands.h"

#include <memory>
#include <string>
#include <utility>

#include "forgeshape_imported_mesh.h"
#include "forgeshape_mesh.h"

namespace forgeshape {
namespace {

// The one shape every command's preamble has: refuse an open edit, then resolve
// the body. Both refusals are asked BEFORE any edit is opened, so a refused
// command does not even leave an empty transaction behind -- the rule
// `deleteSceneBody` already follows.
BodyCommandStatus resolveTarget(ObjectId id, ConstructionScene& scene,
                                const ConstructionHistory& history, SceneObject** out) {
    if (history.editInProgress()) {
        return BodyCommandStatus::RefusedEditInProgress;
    }
    SceneObject* body = scene.findBody(id);
    if (body == nullptr) {
        return BodyCommandStatus::UnknownBody;
    }
    *out = body;
    return BodyCommandStatus::Ok;
}

// Whether any body in the scene already stores this exact name.
//
// Names are NOT unique in this product and nothing enforces that they are: two
// bodies the user renamed identically are their business. This is asked only by
// `duplicateBodyName`, so that a copy does not land wearing a name that is
// already on screen -- a convenience about the suffix, never a constraint on
// Rename.
bool nameIsTaken(const ConstructionScene& scene, const std::string& name) {
    for (size_t i = 0; i < scene.bodyCount(); ++i) {
        if (scene.bodyAt(i).name() == name) {
            return true;
        }
    }
    return false;
}

// Clones the source's CURRENT frozen sculpt geometry onto `target`.
//
// It goes through `freezeFrom`, the same entry point a Freeze and a `.forge`
// load both use, so the copy's adjacency, normals and revision are built by the
// one path rather than by a second copier written here. That is also what makes
// the copy's SculptHistory empty by construction: `freezeFrom` starts a mesh
// at SculptRevision 1 with no entries, and nothing here adds any.
//
// The stale-source flag and the edited FACT are carried across, because both
// are statements about the geometry that came over. The history STACK is not,
// and cannot be: it lives beside the mesh in `FrozenSculpt` and this only ever
// touches the mesh.
bool cloneFrozenSculpt(const SceneObject& source, SceneObject& target) {
    const SculptMesh& from = source.frozenSculpt().mesh;
    if (!from.frozen()) {
        return false;
    }
    ConstructionMesh seed;
    seed.vertices = from.vertices();
    seed.indices = from.indices();
    seed.renderBothSides = from.renderBothSides();
    MeshValidation why = MeshValidation::Ok;
    if (!target.frozenSculpt().mesh.freezeFrom(seed, target.objectId(), &why)) {
        return false;
    }
    target.frozenSculpt().sourceStale = source.frozenSculpt().sourceStale;
    if (from.hasEdits()) {
        // The FACT, restored the way a load restores it: one advance, never the
        // source's SculptRevision. The number is derived state that the
        // renderer, the picker and the fingerprint read, and the copy is
        // entitled to its own.
        target.frozenSculpt().mesh.advanceRevision();
    }
    return true;
}

}  // namespace

const char* bodyCommandStatusName(BodyCommandStatus status) {
    switch (status) {
        case BodyCommandStatus::Ok: return "Ok";
        case BodyCommandStatus::UnknownBody: return "UnknownBody";
        case BodyCommandStatus::RefusedEditInProgress: return "RefusedEditInProgress";
        case BodyCommandStatus::RefusedInvalidName: return "RefusedInvalidName";
        case BodyCommandStatus::RefusedFaceSupportedCad: return "RefusedFaceSupportedCad";
        case BodyCommandStatus::RefusedNotDuplicable: return "RefusedNotDuplicable";
        case BodyCommandStatus::RefusedNotMirrorable: return "RefusedNotMirrorable";
        case BodyCommandStatus::RefusedNotRepresentable: return "RefusedNotRepresentable";
    }
    return "unknown";
}

BodyCommandStatus renameSceneBody(ObjectId id, const std::string& requestedName,
                                  ConstructionScene& scene, ConstructionHistory& history) {
    SceneObject* body = nullptr;
    const BodyCommandStatus resolved = resolveTarget(id, scene, history, &body);
    if (resolved != BodyCommandStatus::Ok) {
        return resolved;
    }
    // The DOMAIN's rule, not a second one. Idempotent, so a name that came back
    // out of a `.forge` file and straight into this is unchanged by it.
    const std::string sanitized = sanitizeImportedMeshName(requestedName);
    if (!importedMeshNameIsStorable(sanitized)) {
        // Empty, or empty once the malformed bytes were dropped. Refused rather
        // than replaced: the fallback label is what a body with NO name gets,
        // and a user must not be able to reach it by typing.
        return BodyCommandStatus::RefusedInvalidName;
    }
    {
        // One Rename is one step. Renaming to the name it already has is a
        // no-op by `commitEdit`'s existing comparison and records nothing.
        ScopedConstructionEdit edit(history);
        body->setName(sanitized);
    }
    return BodyCommandStatus::Ok;
}

BodyCommandStatus setSceneBodyVisible(ObjectId id, bool visible, ConstructionScene& scene,
                                      ConstructionHistory& history) {
    SceneObject* body = nullptr;
    const BodyCommandStatus resolved = resolveTarget(id, scene, history, &body);
    if (resolved != BodyCommandStatus::Ok) {
        return resolved;
    }
    {
        ScopedConstructionEdit edit(history);
        body->setVisible(visible);
        // The selection is deliberately NOT moved when the active body is
        // hidden. See the header: the row stays selected so Show is one tap
        // away, and nothing downstream needs an active body to be drawable.
    }
    return BodyCommandStatus::Ok;
}

BodyCommandStatus setSceneBodyLocked(ObjectId id, bool locked, ConstructionScene& scene,
                                     ConstructionHistory& history) {
    SceneObject* body = nullptr;
    const BodyCommandStatus resolved = resolveTarget(id, scene, history, &body);
    if (resolved != BodyCommandStatus::Ok) {
        return resolved;
    }
    {
        ScopedConstructionEdit edit(history);
        body->setLocked(locked);
    }
    return BodyCommandStatus::Ok;
}

std::string derivedBodyName(const std::string& sourceName, const std::string& suffix,
                            const ConstructionScene& scene) {
    if (sourceName.empty()) {
        // A body with no stored name has a DERIVED label, and deriving a stored
        // name from a derived label would turn presentation into project truth.
        // The new body gets no name and falls back to its own id, exactly as
        // the source does.
        return std::string();
    }
    const std::string base = sourceName + " " + suffix;
    const std::string first = sanitizeImportedMeshName(base);
    if (importedMeshNameIsStorable(first) && !nameIsTaken(scene, first)) {
        return first;
    }
    // Bounded by the scene's own body cap: at most one name per body can be
    // taken, so an ordinal one past the body count is always free.
    for (size_t ordinal = 2; ordinal <= scene.bodyCount() + 2; ++ordinal) {
        const std::string candidate =
            sanitizeImportedMeshName(base + " " + std::to_string(ordinal));
        if (importedMeshNameIsStorable(candidate) && !nameIsTaken(scene, candidate)) {
            return candidate;
        }
    }
    // Unreachable for any scene this product can build. Falling back to no name
    // keeps the new body legal (it wears its id) rather than failing the whole
    // command over a label.
    return std::string();
}

std::string duplicateBodyName(const std::string& sourceName, const ConstructionScene& scene) {
    return derivedBodyName(sourceName, "copy", scene);
}

BodyCommandStatus duplicateSceneBody(ObjectId id, ConstructionScene& scene,
                                     ConstructionHistory& history,
                                     DuplicateBodyReport* outReport) {
    DuplicateBodyReport report;
    report.sourceBodyId = id;
    report.bodyCount = scene.bodyCount();
    const auto finish = [&](BodyCommandStatus status) {
        if (outReport != nullptr) {
            *outReport = report;
        }
        return status;
    };

    SceneObject* source = nullptr;
    const BodyCommandStatus resolved = resolveTarget(id, scene, history, &source);
    if (resolved != BodyCommandStatus::Ok) {
        return finish(resolved);
    }
    if (source->isFaceSupportedCad()) {
        // The one shape this stage refuses, and it is refused BEFORE anything
        // is minted or opened. See the header: the copy's placement would be
        // derived from the same producer face as the original's, so the two
        // would be permanently coincident and the copy would be untransformable
        // by `SceneObject::isFaceSupportedCad`'s own rule. Nothing is
        // retargeted and no dependency is rewritten.
        return finish(BodyCommandStatus::RefusedFaceSupportedCad);
    }

    // Everything the copy needs is read from the source BEFORE the scene is
    // mutated, because appending a body can reallocate the body list and the
    // source pointer with it.
    const BodyRepresentation representation = source->representation();
    const TransformValues placement = source->transform().values();
    const std::string copyName = duplicateBodyName(source->name(), scene);
    const bool visible = source->visible();
    const bool locked = source->locked();
    const bool hasSculpt = source->frozenSculpt().mesh.frozen();
    ConstructionObjectState constructionState{};
    if (const ConstructionObject* construction = source->constructionOrNull()) {
        constructionState = construction->captureState();
    }
    CadBodyState cadState{};
    if (const CadBody* cad = source->cadOrNull()) {
        cadState = cad->captureState();
    }
    ImportedMesh importedCopy;
    if (const ImportedMesh* imported = source->importedOrNull()) {
        // The arrays are COPIED, never shared: an Imported Mesh IS its
        // geometry, and two bodies pointing at one buffer would be one object
        // wearing two ids.
        importedCopy = *imported;
    }

    // ONE transaction, and always this scope's own: an open edit was refused
    // above. One Duplicate is therefore exactly one step -- and one Undo
    // removes exactly the copy, because the step's BEFORE state is the scene
    // without it.
    ObjectId newId = kNoObject;
    {
        ScopedConstructionEdit edit(history);
        SceneObject* copy = nullptr;
        switch (representation) {
            case BodyRepresentation::Construction: {
                copy = &scene.addBody();
                // restoreState rather than the edit entry points: these values
                // were authoritative, and therefore already validated, on the
                // source. Going through an Apply would count them as user
                // updates and rebuild the six remembered sets one at a time.
                copy->construction().restoreState(constructionState);
                break;
            }
            case BodyRepresentation::Imported: {
                copy = scene.addImportedBody(std::move(importedCopy), copyName);
                break;
            }
            case BodyRepresentation::Cad: {
                // Re-validated by the scene, which regenerates ONCE before it
                // mints -- so a state that somehow no longer closes a profile
                // costs no ObjectId, exactly as a fresh Extrude would not.
                copy = scene.addCadBody(cadState);
                break;
            }
        }
        if (copy == nullptr) {
            // The scene is untouched by a refused add, and this scope closes
            // with nothing different, so `commitEdit` records nothing.
            return finish(BodyCommandStatus::RefusedNotDuplicable);
        }
        newId = copy->objectId();
        copy->transform().setValues(placement);
        // `addImportedBody` already took the name; setting it again is
        // harmless and keeps ONE statement of the rule for all three branches.
        copy->setName(copyName);
        copy->setVisible(visible);
        copy->setLocked(locked);
        // Published once, through the ONE dispatch point, so no branch here has
        // to know how its representation makes geometry.
        publishSceneObject(*copy);
        if (hasSculpt) {
            report.clonedSculptMesh = cloneFrozenSculpt(*scene.findBody(id), *copy);
        }
        // The copy is what the user is about to work on. `addBody`,
        // `addImportedBody` and `addCadBody` each already made it active; this
        // states it once so the rule does not depend on three of them agreeing.
        scene.setActiveBody(newId);
    }

    report.newBodyId = newId;
    report.newIndex = scene.indexOfBody(newId);
    report.bodyCount = scene.bodyCount();
    return finish(BodyCommandStatus::Ok);
}

BodyCommandStatus mirrorSceneBody(ObjectId id, MirrorPlane plane, ConstructionScene& scene,
                                  ConstructionHistory& history, MirrorBodyReport* outReport) {
    MirrorBodyReport report;
    report.sourceBodyId = id;
    report.bodyCount = scene.bodyCount();
    const auto finish = [&](BodyCommandStatus status) {
        if (outReport != nullptr) {
            *outReport = report;
        }
        return status;
    };

    SceneObject* source = nullptr;
    const BodyCommandStatus resolved = resolveTarget(id, scene, history, &source);
    if (resolved != BodyCommandStatus::Ok) {
        return finish(resolved);
    }
    // Eligibility is asked BEFORE anything is minted or opened, so an ineligible
    // body costs no ObjectId and leaves no empty transaction behind.
    report.eligibility = mirrorEligibilityOf(*source);
    if (report.eligibility != MirrorEligibility::Eligible) {
        return finish(BodyCommandStatus::RefusedNotMirrorable);
    }

    // The reflection is solved from VALUES before the scene is touched. A
    // refusal here therefore changes nothing at all, exactly as an ineligible
    // body does -- and the source's own transform is read, never written.
    TransformValues mirrored{};
    if (mirrorPlacement(source->transform().values(), plane, &mirrored) != MirrorStatus::Ok) {
        return finish(BodyCommandStatus::RefusedNotRepresentable);
    }
    report.placement = mirrored;

    // Everything the reflection needs is read from the source BEFORE the scene
    // is mutated, because appending a body can reallocate the body list and the
    // source pointer with it.
    const ConstructionObjectState constructionState = source->construction().captureState();
    const std::string mirrorName = derivedBodyName(source->name(), "Mirror", scene);
    // Stage 018A's Duplicate policy, unchanged and deliberately not re-decided
    // here: a hidden source produces a hidden reflection and a locked source a
    // locked one, and the SOURCE keeps both, because Mirror creates a body and
    // mutates none.
    const bool visible = source->visible();
    const bool locked = source->locked();

    // ONE transaction, and always this scope's own: an open edit was refused
    // above. One Mirror is therefore exactly one step -- and one Undo removes
    // exactly the reflection, because the step's BEFORE state is the scene
    // without it.
    ObjectId newId = kNoObject;
    {
        ScopedConstructionEdit edit(history);
        SceneObject& reflection = scene.addBody();
        newId = reflection.objectId();
        // restoreState rather than the edit entry points, exactly as Duplicate
        // does: these parameters were authoritative, and therefore already
        // validated, on the source.
        reflection.construction().restoreState(constructionState);
        reflection.transform().setValues(mirrored);
        reflection.setName(mirrorName);
        reflection.setVisible(visible);
        reflection.setLocked(locked);
        // Published once, for the new body only. The SOURCE is not republished
        // and not re-tessellated: its mesh is untouched by a reflection that
        // read nothing but nine numbers off its transform.
        publishSceneObject(reflection);
        // The reflection is what the user is about to work on. `addBody` has
        // already made it active; this states it once so the rule does not
        // depend on that.
        scene.setActiveBody(newId);
    }

    report.newBodyId = newId;
    report.newIndex = scene.indexOfBody(newId);
    report.bodyCount = scene.bodyCount();
    return finish(BodyCommandStatus::Ok);
}

}  // namespace forgeshape
