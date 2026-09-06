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

const ProjectConstructionBody* findConstructionBody(const ProjectDocument& document, ObjectId id) {
    if (!document.hasConstruction) {
        return nullptr;
    }
    for (const ProjectConstructionBody& body : document.construction.bodies) {
        if (body.objectId == id) {
            return &body;
        }
    }
    return nullptr;
}

const ProjectImportedBody* findImportedBody(const ProjectDocument& document, ObjectId id) {
    if (!document.hasImported) {
        return nullptr;
    }
    for (const ProjectImportedBody& body : document.imported.bodies) {
        if (body.objectId == id) {
            return &body;
        }
    }
    return nullptr;
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

const ProjectCadBody* findCadBody(const ProjectDocument& document, ObjectId id) {
    if (!document.hasCad) {
        return nullptr;
    }
    for (const ProjectCadBody& body : document.cad.bodies) {
        if (body.objectId == id) {
            return &body;
        }
    }
    return nullptr;
}

}  // namespace

bool runtimeCanEvaluateProject(const ProjectDocument& document) {
    for (const ProjectBodyPlacement& placement : document.scene.bodies) {
        if (findConstructionBody(document, placement.objectId) == nullptr
            && findImportedBody(document, placement.objectId) == nullptr
            && findCadBody(document, placement.objectId) == nullptr) {
            return false;
        }
    }
    return true;
}

ProjectDocument captureProjectDocument(const ConstructionScene& scene, ProjectKind kind) {
    ProjectDocument document;
    document.kind = kind;
    document.scene.nextObjectId = scene.nextObjectId();
    document.scene.activeObjectId = scene.activeBodyId();

    const size_t bodyCount = scene.bodyCount();
    document.scene.bodies.reserve(bodyCount);
    document.construction.bodies.reserve(bodyCount);

    for (size_t i = 0; i < bodyCount; ++i) {
        const SceneObject& body = scene.bodyAt(i);

        ProjectBodyPlacement placement;
        placement.objectId = body.objectId();
        placement.transform = body.transform().values();
        // Stage 018A. Visibility and lock are project truth for every body, so
        // they are written for every body; the NAME is written here only for a
        // body whose name SCNE owns. An Imported Mesh's name is `IMPT`'s and is
        // written there, and duplicating it would be two answers to what one
        // body is called -- `validateProjectDocument` refuses a file that does.
        placement.visible = body.visible();
        placement.locked = body.locked();
        if (!body.isImported()) {
            placement.name = body.name();
        }
        document.scene.bodies.push_back(placement);

        // WHICH branch a body is written to is its representation, and each
        // body goes to exactly one. CONS is the REQUIRED section of a
        // Construction project and the retained companion of a Sculpt one;
        // which of those it is comes from the header's ProjectKind, not from
        // whether the data exists.
        if (const ConstructionObject* source = body.constructionOrNull()) {
            ProjectConstructionBody construction;
            construction.objectId = body.objectId();
            // Placement is SCNE's and is not here: since IMPORT-01A the shape
            // state does not carry one at all, so the document has exactly one
            // answer to where a body sits by construction rather than by
            // clearing a field.
            construction.shape = source->captureState();
            construction.features.push_back(ProjectFeatureRecord{});
            document.construction.bodies.push_back(std::move(construction));
            document.hasConstruction = true;
        } else if (const ImportedMesh* imported = body.importedOrNull()) {
            // The geometry itself, because nothing could recreate it. This is
            // the one representation whose vertices ARE project truth.
            ProjectImportedBody record;
            record.objectId = body.objectId();
            record.name = body.name();
            record.positions = imported->positions();
            record.normals = imported->normals();
            record.indices = imported->indices();
            record.batches = imported->batches();
            document.imported.bodies.push_back(std::move(record));
            document.hasImported = true;
        } else if (const CadBody* cad = body.cadOrNull()) {
            // The authored state and nothing derived: the mesh is regenerated
            // from exactly this on load.
            ProjectCadBody record;
            record.objectId = body.objectId();
            record.state = cad->captureState();
            document.cad.bodies.push_back(std::move(record));
            document.hasCad = true;
        }

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
    // A DOCUMENT may legally leave a body with no geometry branch at all, and
    // this build cannot evaluate one that does. Asked before anything is
    // staged, through the same predicate the recovery-candidate check uses.
    if (!runtimeCanEvaluateProject(document)) {
        return ProjectCodecStatus::MissingRequiredSection;
    }

    // -----------------------------------------------------------------------
    // Stage. Nothing below this comment touches the live project.
    // -----------------------------------------------------------------------
    const size_t bodyCount = document.scene.bodies.size();
    std::vector<std::unique_ptr<SceneObject>> staged;
    staged.reserve(bodyCount);
    int sculptMeshes = 0;
    int importedBodies = 0;

    for (size_t i = 0; i < bodyCount; ++i) {
        const ProjectBodyPlacement& placement = document.scene.bodies[i];
        const ProjectConstructionBody* shape =
                findConstructionBody(document, placement.objectId);
        const ProjectImportedBody* imported = findImportedBody(document, placement.objectId);
        const ProjectCadBody* cad = findCadBody(document, placement.objectId);

        // Proven above by runtimeCanEvaluateProject; re-checked here because
        // the pointers are about to be dereferenced.
        if (shape == nullptr && imported == nullptr && cad == nullptr) {
            return ProjectCodecStatus::MissingRequiredSection;
        }

        std::unique_ptr<SceneObject> body;
        MeshValidation meshWhy = MeshValidation::Ok;
        if (cad != nullptr) {
            // validateProjectDocument has already run the whole CAD rule over
            // this state, so the body is built with it directly; the publish
            // below regenerates the mesh through the one CAD path and is the
            // second proof.
            body.reset(new SceneObject(placement.objectId, cad->state));
        } else if (imported != nullptr) {
            // Rebuilt through the same `ImportedMesh::build` an import goes
            // through, so a loaded object is exactly as validated as a freshly
            // imported one and no second construction path exists.
            ImportedMeshValidation importedWhy = ImportedMeshValidation::Ok;
            ImportedMesh mesh =
                    ImportedMesh::build(imported->positions, imported->normals,
                                        imported->indices, imported->batches, &importedWhy);
            if (importedWhy != ImportedMeshValidation::Ok || !mesh.valid()) {
                return ProjectCodecStatus::InvalidSemanticValue;
            }
            // Built directly rather than through the scene, precisely because
            // the scene would push the LIVE allocator forward -- a mutation of
            // the project we may still be about to refuse.
            body.reset(new SceneObject(placement.objectId, std::move(mesh), imported->name));
            ++importedBodies;
        } else {
            body = std::make_unique<SceneObject>(placement.objectId);
            // restoreState rather than setPrimitive/applyTransformValues: these
            // values were authoritative, and therefore already validated, when
            // they were captured, and validateProjectDocument has just
            // re-checked them against the same domain contracts. Going through
            // the edit entry points would count them as user updates and would
            // rebuild the six remembered parameter sets one primitive at a time.
            body->construction().restoreState(shape->shape);
        }
        // The placement is the BODY's, whichever representation it has, and
        // SCNE is where it came from.
        body->transform().setValues(placement.transform);
        // Stage 018A, on the same terms. A v1 file states neither flag and its
        // decoded record carries the defaults, so this restores "visible and
        // unlocked" for an older project without a migration branch. The name
        // is taken from SCNE only where SCNE owns it -- an Imported Mesh was
        // already constructed with `imported->name` above, and overwriting it
        // with SCNE's (validated) empty string would erase it.
        body->setVisible(placement.visible);
        body->setLocked(placement.locked);
        if (imported == nullptr) {
            body->setName(placement.name);
        }

        // ONE dispatch point: a Construction Body regenerates from its
        // parameters, an Imported Mesh republishes the geometry it owns.
        if (publishSceneObject(*body, &meshWhy) == kNoMeshRevision) {
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
    // A stroke still in flight belongs to the body it started on, which is
    // about to be destroyed. It is closed HERE, while the session is still
    // bound to that body, so its entry lands on the old body's history and
    // never on a loaded body's (DEEP-AUDIT-R1 F-02): recorded after the rebind
    // below it would carry the old mesh's positions into the new body's Undo.
    session.cancelStroke();
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
        outReport->importedBodies = importedBodies;
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

void mixPoint(uint64_t& hash, const SketchPoint& p) {
    mixDouble(hash, p.u);
    mixDouble(hash, p.v);
}

// Every authored CAD value, because every one is in the file. Small by
// construction -- the sketch is bounded -- so this is the values themselves
// and not a proxy.
void mixCad(uint64_t& hash, const CadBodyState& state) {
    mixU64(hash, static_cast<uint64_t>(workplaneIndex(state.sketch.plane)));
    // CAD-A3: the face support is project truth, so it is part of the semantic
    // fingerprint -- a body re-supported on a different face is a different
    // project and must trigger a checkpoint.
    mixU64(hash, state.sketch.hasFaceSupport ? 1u : 0u);
    if (state.sketch.hasFaceSupport) {
        const TopoRef& ref = state.sketch.faceSupport;
        mixU64(hash, static_cast<uint64_t>(ref.producerObjectId));
        mixU64(hash, ref.producerLocalFeatureId);
        mixU64(hash, cadFaceTokenCode(ref.face));
        mixU64(hash, ref.lineageToken);
    }
    mixU64(hash, state.sketch.nextEntityId);
    mixU64(hash, state.extrude.profileEntityId);
    mixDouble(hash, state.extrude.depth);
    mixU64(hash, static_cast<uint64_t>(extrudeDirectionIndex(state.extrude.direction)));
    mixU64(hash, state.sketch.entities.size());
    for (const SketchEntity& entity : state.sketch.entities) {
        mixU64(hash, entity.id());
        mixU64(hash, static_cast<uint64_t>(entity.kind()));
        if (const SketchLine* line = entity.line()) {
            mixPoint(hash, line->start);
            mixPoint(hash, line->end);
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            mixU64(hash, polyline->closed ? 1u : 0u);
            mixU64(hash, polyline->vertices.size());
            for (const SketchPoint& p : polyline->vertices) {
                mixPoint(hash, p);
            }
        } else if (const SketchRectangle* rectangle = entity.rectangle()) {
            mixPoint(hash, rectangle->center);
            mixDouble(hash, rectangle->width);
            mixDouble(hash, rectangle->height);
        } else if (const SketchCircle* circle = entity.circle()) {
            mixPoint(hash, circle->center);
            mixDouble(hash, circle->radius);
        } else if (const SketchArc* arc = entity.arc()) {
            // Every authored point, or an in-place curve edit that keeps the
            // entity id would not move the fingerprint (DEEP-AUDIT-R1 F-01).
            mixPoint(hash, arc->start);
            mixPoint(hash, arc->mid);
            mixPoint(hash, arc->end);
        } else if (const SketchSpline* spline = entity.spline()) {
            mixU64(hash, spline->points.size());
            for (const SketchPoint& p : spline->points) {
                mixPoint(hash, p);
            }
        }
    }
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
        // The representation itself, so the two branches below can never
        // collide by producing the same bytes for different kinds of object.
        mixU64(hash, static_cast<uint64_t>(body.representation()));
        if (const ConstructionObject* source = body.constructionOrNull()) {
            mixShape(hash, source->captureState());
        } else if (const ImportedMesh* imported = body.importedOrNull()) {
            // Identity, name and topology counts — and that is COMPLETE, not a
            // proxy like the sculpt mesh's below. An ImportedMesh is immutable
            // for the life of its body: there is no edit path that can change a
            // vertex of one, so nothing the file would store can differ while
            // these agree. Hashing four million positions on every autosave
            // check to learn that would be the storm the checkpoint policy
            // exists to prevent.
            mixBytes(hash, body.name().data(), body.name().size());
            mixU64(hash, imported->vertexCount());
            mixU64(hash, imported->triangleCount());
            for (const ImportedMeshBatch& batch : imported->batches()) {
                mixU64(hash, batch.firstIndex);
                mixU64(hash, batch.indexCount);
                mixU64(hash, batch.doubleSided ? 1u : 0u);
            }
        } else if (const CadBody* cad = body.cadOrNull()) {
            mixCad(hash, cad->state());
        }
        mixTransform(hash, body.transform().values());
        // Stage 018A. All three are project truth -- they reach `.forge` bytes
        // -- so a Rename, a Show/Hide and a Lock/Unlock must each move the
        // fingerprint and earn a checkpoint, exactly as a placement edit does.
        // The name is mixed for EVERY representation here, not only for the
        // imported branch above: the fingerprint hashes what the document
        // carries, and which SECTION carries it is the codec's business.
        // Hashing it twice for an imported body is harmless and keeps this one
        // statement rather than two conditional ones.
        mixBytes(hash, body.name().data(), body.name().size());
        mixU64(hash, body.visible() ? 1u : 0u);
        mixU64(hash, body.locked() ? 1u : 0u);

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
