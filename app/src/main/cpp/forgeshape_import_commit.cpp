#include "forgeshape_import_commit.h"

#include <utility>
#include <vector>

#include "forgeshape_mesh.h"
#include "forgeshape_project_document.h"

namespace forgeshape {
namespace {

// One object, fully built and fully proven, waiting for the commit.
struct StagedImportedBody {
    ImportedMesh mesh;
    std::string name;
    TransformValues placement{};
};

// The name this object will carry, by the one stated priority.
//
// The file's own words come first because they are what the person who made
// the model called it; the fallback exists only so that a file naming nothing
// still produces rows a user can tell apart. Sanitizing happens here, once, so
// nothing downstream has to wonder whether a name has been through it.
std::string chooseName(const ParsedGlbMesh& parsed, uint32_t ordinal) {
    std::string chosen = sanitizeImportedMeshName(parsed.name);
    if (chosen.empty()) {
        chosen = sanitizeImportedMeshName(parsed.meshName);
    }
    if (chosen.empty()) {
        chosen = fallbackImportedMeshName(ordinal);
    }
    return chosen;
}

}  // namespace

const char* importCommitStatusName(ImportCommitStatus status) {
    switch (status) {
        case ImportCommitStatus::Ok: return "Ok";
        case ImportCommitStatus::NothingToImport: return "NothingToImport";
        case ImportCommitStatus::TooManyObjects: return "TooManyObjects";
        case ImportCommitStatus::RejectedGeometry: return "RejectedGeometry";
        case ImportCommitStatus::RefusedEditInProgress: return "RefusedEditInProgress";
    }
    return "unknown";
}

ImportCommitStatus commitImportedGlbScene(const ParsedGlbScene& parsed, ConstructionScene& scene,
                                          ConstructionHistory& history,
                                          ImportCommitReport* outReport) {
    ImportCommitReport report;
    const auto refuse = [&](ImportCommitStatus status) {
        if (outReport != nullptr) {
            *outReport = report;
        }
        return status;
    };

    if (history.editInProgress()) {
        return refuse(ImportCommitStatus::RefusedEditInProgress);
    }
    if (parsed.meshes.empty()) {
        return refuse(ImportCommitStatus::NothingToImport);
    }
    // Bounded by what a `.forge` file can carry rather than by a number of this
    // module's own: an import the project could never be SAVED with is refused
    // now, with a reason, instead of at the first checkpoint with none.
    if (scene.bodyCount() + parsed.meshes.size() > kMaxProjectBodies) {
        return refuse(ImportCommitStatus::TooManyObjects);
    }

    // -----------------------------------------------------------------------
    // Stage. Nothing below this comment touches the scene, the history or an
    // ObjectId, so every refusal above and below costs the project nothing.
    // -----------------------------------------------------------------------
    std::vector<StagedImportedBody> staged;
    staged.reserve(parsed.meshes.size());

    for (size_t m = 0; m < parsed.meshes.size(); ++m) {
        const ParsedGlbMesh& source = parsed.meshes[m];

        // The split: the linear part is already baked into the parsed
        // positions, so taking the translation back off leaves local geometry,
        // and the translation itself becomes the placement.
        std::vector<float> positions(source.positions.size());
        for (uint32_t v = 0; v < source.vertexCount(); ++v) {
            const Vec3 local = source.localPosition(v);
            const size_t at = static_cast<size_t>(v) * 3u;
            positions[at] = local.x;
            positions[at + 1] = local.y;
            positions[at + 2] = local.z;
        }

        std::vector<ImportedMeshBatch> batches;
        batches.reserve(source.batches.size());
        for (const ParsedGlbBatch& batch : source.batches) {
            ImportedMeshBatch converted;
            converted.firstIndex = batch.firstIndex;
            converted.indexCount = batch.indexCount;
            // The one material fact this product carries, because it decides
            // which triangles are VISIBLE rather than how they look.
            converted.doubleSided = batch.doubleSided;
            batches.push_back(converted);
        }

        StagedImportedBody body;
        ImportedMeshValidation why = ImportedMeshValidation::Ok;
        body.mesh = ImportedMesh::build(std::move(positions), source.normals, source.indices,
                                        std::move(batches), &why);
        if (why != ImportedMeshValidation::Ok || !body.mesh.valid()) {
            report.geometryWhy = why;
            return refuse(ImportCommitStatus::RejectedGeometry);
        }

        // Proven drawable before anything is committed. `publishSceneObject`
        // below is then a re-run of a pure function over geometry that has not
        // changed, so the commit step cannot fail — which is what makes "all
        // the objects or none of them" true rather than merely intended.
        {
            std::vector<MeshVertex> vertices;
            std::vector<uint32_t> indices;
            if (!body.mesh.buildDrawData(&vertices, &indices)) {
                report.geometryWhy = ImportedMeshValidation::NotDrawable;
                return refuse(ImportCommitStatus::RejectedGeometry);
            }
        }

        body.name = chooseName(source, static_cast<uint32_t>(m + 1));
        // Rotation and scale are left at the identity on purpose: whatever the
        // node's linear part did is already in the geometry above, and an
        // imported body starts at the placement the file gave it and nothing
        // more.
        body.placement.positionX = static_cast<double>(source.translation[0]);
        body.placement.positionY = static_cast<double>(source.translation[1]);
        body.placement.positionZ = static_cast<double>(source.translation[2]);

        report.vertices += body.mesh.vertexCount();
        report.triangles += body.mesh.triangleCount();
        report.batches += body.mesh.batchCount();
        staged.push_back(std::move(body));
    }

    // -----------------------------------------------------------------------
    // Commit. ONE transaction for the whole import, so however many objects a
    // file described, undoing it is one act — the same rule Add Primitive
    // follows for the two mutations it is made of.
    // -----------------------------------------------------------------------
    ScopedConstructionEdit edit(history);
    for (StagedImportedBody& body : staged) {
        SceneObject* created =
                scene.addImportedBody(std::move(body.mesh), body.name);
        if (created == nullptr) {
            // Unreachable: the geometry was validated above and moved in
            // unchanged. Left as a hard stop rather than an assumption,
            // because the alternative is a null dereference in a commit.
            continue;
        }
        created->transform().setValues(body.placement);
        publishSceneObject(*created);
        if (report.firstObjectId == kNoObject) {
            report.firstObjectId = created->objectId();
        }
        report.lastObjectId = created->objectId();
        ++report.objects;
    }
    if (report.objects == 0) {
        return refuse(ImportCommitStatus::NothingToImport);
    }
    // The stated rule: the user lands on the FIRST object the import created.
    // `addImportedBody` selects each body as it appends it, so this is the one
    // place that decides, rather than the loop's last iteration deciding by
    // accident.
    scene.setActiveBody(report.firstObjectId);
    report.activeBodyId = scene.activeBodyId();

    if (outReport != nullptr) {
        *outReport = report;
    }
    return ImportCommitStatus::Ok;
}

}  // namespace forgeshape
