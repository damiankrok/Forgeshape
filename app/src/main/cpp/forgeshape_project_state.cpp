#include "forgeshape_project_state.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace forgeshape {
namespace {

// Rebuilds the ConstructionMesh a Frozen Sculpt Mesh is frozen FROM, out of the
// stored positions and indices.
//
// This is the whole of the sculpt restore, and it deliberately goes through the
// ordinary `SculptMesh::freezeFrom` rather than writing the mesh's members: that
// is the one path that validates the data as a usable triangle mesh, builds the
// adjacency and starts the revision, so a loaded sculpt mesh is exactly as
// proven as a freshly frozen one. Positions come back bit for bit; normals,
// adjacency and the revision are rebuilt, which is what makes them derived.
ConstructionMesh sculptSourceFrom(const ProjectSculptBody& stored) {
    ConstructionMesh source;
    const uint32_t vertexCount = stored.vertexCount();
    source.vertices.resize(vertexCount);
    for (uint32_t v = 0; v < vertexCount; ++v) {
        MeshVertex& vertex = source.vertices[v];
        vertex.position[0] = stored.positions[static_cast<size_t>(v) * 3u + 0u];
        vertex.position[1] = stored.positions[static_cast<size_t>(v) * 3u + 1u];
        vertex.position[2] = stored.positions[static_cast<size_t>(v) * 3u + 2u];
        vertex.color[0] = kLoadedSculptVertexColor;
        vertex.color[1] = kLoadedSculptVertexColor;
        vertex.color[2] = kLoadedSculptVertexColor;
    }
    source.indices = stored.indices;
    // A geometric fact about this mesh, carried across the file rather than
    // re-derived from whatever the Construction Source happens to be now: a
    // frozen sheet stays a sheet even after its source has become something
    // else entirely.
    source.renderBothSides = stored.renderBothSides;
    return source;
}

const ProjectSculptBody* findSculptBody(const ProjectDocument& document, ObjectId id) {
    if (!document.hasSculpt) {
        return nullptr;
    }
    for (const ProjectSculptBody& body : document.sculpt.bodies) {
        if (body.objectId == id) {
            return &body;
        }
    }
    return nullptr;
}

}  // namespace

ProjectDocument captureProjectDocument(const ConstructionScene& scene, ProjectKind kind) {
    ProjectDocument document;
    document.kind = kind;
    document.scene.nextObjectId = scene.nextObjectId();
    document.scene.activeObjectId = scene.activeBodyId();

    const size_t bodyCount = scene.bodyCount();
    document.scene.bodies.reserve(bodyCount);
    document.construction.bodies.reserve(bodyCount);
    // Every current body has a Construction Source, so CONS is always written.
    // It is the REQUIRED section of a Construction project and the retained
    // companion of a Sculpt one; which of those it is comes from the header's
    // ProjectKind, not from whether the data exists.
    document.hasConstruction = bodyCount > 0;

    for (size_t i = 0; i < bodyCount; ++i) {
        const SceneObject& body = scene.bodyAt(i);

        ProjectBodyPlacement placement;
        placement.objectId = body.objectId();
        placement.transform = body.transform().values();
        document.scene.bodies.push_back(placement);

        ProjectConstructionBody construction;
        construction.objectId = body.objectId();
        // Placement is SCNE's and is not here: since IMPORT-01A the shape state
        // does not carry one at all, so the document has exactly one answer to
        // where a body sits by construction rather than by clearing a field.
        construction.shape = body.construction().captureState();
        construction.features.push_back(ProjectFeatureRecord{});
        document.construction.bodies.push_back(std::move(construction));

        const FrozenSculpt& frozen = body.frozenSculpt();
        if (!frozen.mesh.frozen()) {
            continue;
        }
        ProjectSculptBody sculpt;
        sculpt.objectId = body.objectId();
        sculpt.renderBothSides = frozen.mesh.renderBothSides();
        sculpt.sourceStale = frozen.sourceStale;
        // The FACT of having been sculpted, not the revision it is derived from.
        // See ProjectSculptBody::hasEdits: this is what the destructive
        // Reset-Sculpt-from-Shape guard asks, and a reopened project that
        // reported an unedited mesh would let that reset discard the whole file
        // without a word.
        sculpt.hasEdits = frozen.mesh.hasEdits();
        const std::vector<MeshVertex>& vertices = frozen.mesh.vertices();
        sculpt.positions.reserve(vertices.size() * 3u);
        for (const MeshVertex& vertex : vertices) {
            // Positions only. Normals are not stored at all, and the colour is
            // debug-only presentation -- see kLoadedSculptVertexColor.
            sculpt.positions.push_back(vertex.position[0]);
            sculpt.positions.push_back(vertex.position[1]);
            sculpt.positions.push_back(vertex.position[2]);
        }
        sculpt.indices = frozen.mesh.indices();
        document.sculpt.bodies.push_back(std::move(sculpt));
        document.hasSculpt = true;
    }
    return document;
}

ProjectCodecStatus loadProjectDocument(const ProjectDocument& document, ConstructionScene& scene,
                                       SculptSession& session, ConstructionHistory& history,
                                       ProjectLoadReport* outReport) {
    if (history.editInProgress()) {
        return ProjectCodecStatus::RefusedEditInProgress;
    }
    const ProjectCodecStatus why = validateProjectDocument(document);
    if (why != ProjectCodecStatus::Ok) {
        return why;
    }
    // A DOCUMENT may legally have no Construction branch — a Sculpt project's
    // CONS companion is optional, and a reader that could not understand its
    // version is right to skip it. This RUNTIME cannot build a body without a
    // Construction Source, though: every `SceneObject` has one, and inventing a
    // default Box for a body whose real shape the file described would be
    // fabricating project data. So it is refused here, where the reason is
    // "this build cannot evaluate that project", rather than in the codec,
    // where the file itself is not at fault.
    if (!document.hasConstruction
        || document.construction.bodies.size() != document.scene.bodies.size()) {
        return ProjectCodecStatus::MissingRequiredSection;
    }

    // -----------------------------------------------------------------------
    // Stage. Nothing below this comment touches the live project.
    // -----------------------------------------------------------------------
    const size_t bodyCount = document.scene.bodies.size();
    std::vector<std::unique_ptr<SceneObject>> staged;
    staged.reserve(bodyCount);
    int sculptMeshes = 0;

    for (size_t i = 0; i < bodyCount; ++i) {
        const ProjectBodyPlacement& placement = document.scene.bodies[i];
        // Built directly rather than through ConstructionScene::makeBody,
        // precisely because makeBody would push the LIVE allocator forward --
        // a mutation of the project we may still be about to refuse.
        std::unique_ptr<SceneObject> body = std::make_unique<SceneObject>(placement.objectId);

        const ConstructionObjectState state = document.construction.bodies[i].shape;
        // restoreState rather than setPrimitive/applyTransformValues: these
        // values were authoritative, and therefore already validated, when they
        // were captured, and validateProjectDocument has just re-checked them
        // against the same domain contracts. Going through the edit entry points
        // would count them as user updates and would rebuild the six remembered
        // parameter sets one primitive at a time.
        body->construction().restoreState(state);
        // The placement is the BODY's, and SCNE is where it came from.
        body->transform().setValues(placement.transform);

        MeshValidation meshWhy = MeshValidation::Ok;
        if (publishConstructionObject(body->construction(), body->meshStore(), &meshWhy)
            == kNoMeshRevision) {
            return ProjectCodecStatus::InvalidSemanticValue;
        }

        const ProjectSculptBody* stored = findSculptBody(document, placement.objectId);
        if (stored != nullptr) {
            const ConstructionMesh source = sculptSourceFrom(*stored);
            if (!body->frozenSculpt().mesh.freezeFrom(source, placement.objectId, &meshWhy)) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            // Set directly rather than through markSourceStale(): the flag is
            // being RESTORED, not decided. markSourceStale answers "has the
            // source moved on since the freeze", and the answer is the one the
            // file carries.
            body->frozenSculpt().sourceStale = stored->sourceStale;
            if (stored->hasEdits) {
                // One advance is enough: hasEdits() asks whether the revision
                // has moved past the one a freeze starts at, and the NUMBER is
                // not file truth. Restoring the fact costs one increment;
                // restoring the number would make a derived counter into
                // something a file could lie about.
                body->frozenSculpt().mesh.advanceRevision();
            }
            ++sculptMeshes;

            // In a Sculpt project the sculpt mesh is the ACTIVE representation
            // of the active body, so it is what that body's store must hold.
            // Published here, while the body is still off to the side, so that
            // nothing can fail after the commit step below.
            if (document.kind == ProjectKind::Sculpt
                && placement.objectId == document.scene.activeObjectId) {
                if (publishSculptMesh(body->frozenSculpt().mesh, body->meshStore(), &meshWhy)
                    == kNoMeshRevision) {
                    return ProjectCodecStatus::InvalidSemanticValue;
                }
            }
        }
        staged.push_back(std::move(body));
    }

    // -----------------------------------------------------------------------
    // Commit. Everything from here on is arithmetic on already-built objects
    // and cannot fail.
    // -----------------------------------------------------------------------
    while (scene.bodyCount() > 0) {
        scene.detachBody(scene.bodyAt(0).objectId());
    }
    for (size_t i = 0; i < staged.size(); ++i) {
        scene.insertBody(std::move(staged[i]), i);
    }
    // The allocator is only ever pushed FORWARD. The document's high-water mark
    // is above every id it carries (validateProjectDocument proves it), and this
    // process may already have minted further, so the survivor is the larger --
    // which is what makes a post-load creation unable to collide with a loaded
    // body or with anything this process handed out earlier.
    scene.reserveObjectIdsThrough(document.scene.nextObjectId - 1);
    scene.setActiveBody(document.scene.activeObjectId);

    // The session is re-pointed at the new active body before the mode changes,
    // so entering Sculpt asks the right body whether anything is frozen.
    session.bindTarget(&scene.activeBody().frozenSculpt());
    if (document.kind == ProjectKind::Sculpt) {
        session.enterSculpt();
    } else {
        session.enterConstruction();
    }

    // The loaded document starts a fresh session. A step recorded before the
    // load describes a scene that no longer exists, and the redo stack would be
    // holding detached bodies from it.
    history.clear();

    if (outReport != nullptr) {
        outReport->bodies = static_cast<int>(bodyCount);
        outReport->sculptMeshes = sculptMeshes;
        outReport->activeBodyId = scene.activeBodyId();
        outReport->kind = document.kind;
        const RuntimeMeshPtr active = scene.activeBody().meshStore().current();
        outReport->activeRevision = active ? active->revision() : kNoMeshRevision;
    }
    return ProjectCodecStatus::Ok;
}

namespace {

// FNV-1a over 64 bits. Chosen because it is four lines, has no table, and is
// completely specified by two constants — the fingerprint is an internal change
// detector, never a file field, so nothing outside this process has to
// reproduce it.
constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void mixBytes(uint64_t& hash, const void* data, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
}

void mixU64(uint64_t& hash, uint64_t value) { mixBytes(hash, &value, sizeof(value)); }

// The BIT PATTERN, for the same reason the codec writes bit patterns: two
// values that differ only in the sign of a zero, or one of which is a NaN, are
// different documents and must produce different fingerprints.
void mixDouble(uint64_t& hash, double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    mixU64(hash, bits);
}

void mixShape(uint64_t& hash, const ConstructionObjectState& shape) {
    mixU64(hash, primitiveFileCode(shape.kind));
    // All six remembered sets, because all six are in the file: a Box -> Sphere
    // -> Box round trip that came back to a different box must be checkpointed.
    mixDouble(hash, shape.box.width);
    mixDouble(hash, shape.box.height);
    mixDouble(hash, shape.box.depth);
    mixDouble(hash, shape.cylinder.diameter);
    mixDouble(hash, shape.cylinder.height);
    mixDouble(hash, shape.sphere.diameter);
    mixDouble(hash, shape.cone.bottomDiameter);
    mixDouble(hash, shape.cone.height);
    mixDouble(hash, shape.capsule.diameter);
    mixDouble(hash, shape.capsule.totalHeight);
    mixDouble(hash, shape.plane.width);
    mixDouble(hash, shape.plane.depth);
}

void mixTransform(uint64_t& hash, const TransformValues& values) {
    mixDouble(hash, values.positionX);
    mixDouble(hash, values.positionY);
    mixDouble(hash, values.positionZ);
    mixDouble(hash, values.rotationX);
    mixDouble(hash, values.rotationY);
    mixDouble(hash, values.rotationZ);
    mixDouble(hash, values.scaleX);
    mixDouble(hash, values.scaleY);
    mixDouble(hash, values.scaleZ);
}

}  // namespace

uint64_t projectSemanticFingerprint(const ConstructionScene& scene, ProjectKind kind) {
    uint64_t hash = kFnvOffsetBasis;
    // The header's own fields first: the reopen mode is part of the document,
    // so leaving Construction for Sculpt is a change worth checkpointing even
    // when not one number moved.
    mixU64(hash, static_cast<uint64_t>(kind));
    mixU64(hash, scene.bodyCount());
    mixU64(hash, scene.activeBodyId());
    mixU64(hash, scene.nextObjectId());

    for (size_t i = 0; i < scene.bodyCount(); ++i) {
        const SceneObject& body = scene.bodyAt(i);
        // The index as well as the id, so reordering — which the scene cannot
        // do today — could never be silently invisible to a later stage.
        mixU64(hash, i);
        mixU64(hash, body.objectId());
        mixShape(hash, body.construction().captureState());
        mixTransform(hash, body.transform().values());

        const FrozenSculpt& frozen = body.frozenSculpt();
        mixU64(hash, frozen.mesh.frozen() ? 1u : 0u);
        mixU64(hash, frozen.sourceStale ? 1u : 0u);
        if (!frozen.mesh.frozen()) {
            continue;
        }
        // The proxy, and the whole of it. A stroke advances the revision; a
        // re-freeze restarts the revision but advances the freeze count, so the
        // pair cannot repeat across a re-freeze the way the revision alone
        // could. The counts catch a freeze from a different-sized source.
        mixU64(hash, frozen.mesh.revision());
        mixU64(hash, frozen.mesh.freezeCount());
        mixU64(hash, frozen.mesh.vertexCount());
        mixU64(hash, frozen.mesh.indexCount());
        mixU64(hash, frozen.mesh.renderBothSides() ? 1u : 0u);
        mixU64(hash, frozen.mesh.hasEdits() ? 1u : 0u);
    }
    return hash;
}

}  // namespace forgeshape
