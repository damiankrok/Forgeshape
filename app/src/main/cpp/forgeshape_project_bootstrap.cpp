#include "forgeshape_project_bootstrap.h"

#include "forgeshape_cad_body.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"

namespace forgeshape {

namespace {

// Every refusal and the one success leave the reason where `lastStatus` reads it.
CadStatus finish(SketchSession& sketch, CadStatus why) {
    sketch.recordLastStatus(why);
    return why;
}

}  // namespace

CadStatus commitFirstCadProject(SketchSession& sketch, ConstructionScene& scene,
                                SculptSession& session, ConstructionHistory& history,
                                FirstProjectReport* outReport) {
    if (outReport != nullptr) {
        *outReport = FirstProjectReport{};
    }
    if (scene.hasProject()) {
        // A project is open: the ordinary commit adds a body to it. Reaching
        // this with one open is a dispatch error, refused rather than answered
        // by replacing the user's project.
        return finish(sketch, CadStatus::NotSketching);
    }
    if (sketch.state() != SketchSessionState::Ready) {
        return finish(sketch, CadStatus::NotSketching);
    }
    if (history.editInProgress()) {
        return finish(sketch, CadStatus::RefusedEditInProgress);
    }
    if (sketch.selectedProfileId() == kNoSketchEntity) {
        return finish(sketch, sketch.profiles().profiles.size() > 1
                                  ? CadStatus::AmbiguousProfile
                                  : CadStatus::ProfileNotFound);
    }

    // The candidate is held to the domain's own rule BEFORE a document is
    // built, so a refusal is named in the sketch's vocabulary (the depth, the
    // profile) rather than as a codec status about a file nobody wrote.
    const CadBodyState state = sketch.candidateState();
    ProfileExtraction extraction;
    const CadStatus valid = validateCadBodyState(state, &extraction);
    if (valid != CadStatus::Ok) {
        return finish(sketch, valid);
    }

    // One world-plane CAD body at the identity, active, wearing the id the
    // allocator hands out next. The document is the SAME canonical shape a
    // `.forge` file carries, minus the bytes: it exists only to travel through
    // the one validated replace-the-scene path, and it is never encoded.
    const ObjectId bodyId = scene.nextObjectId();
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = bodyId + 1;
    document.scene.activeObjectId = bodyId;
    ProjectBodyPlacement placement;
    placement.objectId = bodyId;
    document.scene.bodies.push_back(placement);
    document.hasCad = true;
    ProjectCadBody cad;
    cad.objectId = bodyId;
    cad.state = state;
    document.cad.bodies.push_back(cad);

    ProjectLoadReport report;
    const ProjectCodecStatus loaded =
        loadProjectDocument(document, scene, session, history, &report);
    if (loaded != ProjectCodecStatus::Ok) {
        // The load is all-or-nothing: the scene is still empty and the sketch
        // still stands. The only way a validated state fails here is the
        // regeneration the load performs.
        return finish(sketch, CadStatus::RegenerationFailed);
    }
    // The session is over; the truth is now the project's. Nothing before this
    // line touched the scene, and nothing after it touches the sketch again.
    sketch.cancel();
    if (outReport != nullptr) {
        outReport->bodyId = report.activeBodyId;
        outReport->revision = report.activeRevision;
    }
    return finish(sketch, CadStatus::Ok);
}

}  // namespace forgeshape
