// Seat D diagnostic 2: does a Freeform edit session (and its selection) survive a
// whole-project replacement through loadProjectDocument when the new project's
// active body has the same ObjectId? Scratch only.
#include <cstdio>
#include "forgeshape_freeform.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
using namespace forgeshape;
int main() {
    // Project P2 (the file to open): a Freeform box whose +face was pushed.
    ConstructionScene other(NoProjectTag{});
    ConstructionHistory otherHistory{other};
    ObjectId otherId = kNoObject;
    createFreeformBody(other, otherHistory, 0, &otherId);
    FreeformCage pushed;
    freeformPushPull(other.findBody(otherId)->freeformOrNull()->cage(), {3}, 0.4, &pushed);
    other.findBody(otherId)->freeformOrNull()->applyCage(std::make_shared<const FreeformCage>(pushed));
    const ProjectDocument doc = captureProjectDocument(other, ProjectKind::Construction);

    // The live project P1: a Freeform box, cage editing open, face 3 selected.
    ConstructionScene scene(NoProjectTag{});
    ConstructionHistory history{scene};
    SculptSession sculpt;
    ObjectId id = kNoObject;
    createFreeformBody(scene, history, 0, &id);
    FreeformEditSession session(scene, history);
    session.begin(id);
    session.setElement(FreeformElement::Face);
    session.select({3});
    std::printf("INFO before load: p1Id=%llu p2Id=%llu active=%d selection=%zu\n",
                (unsigned long long)id, (unsigned long long)otherId, (int)session.active(),
                session.selection().size());
    const ProjectCodecStatus st = loadProjectDocument(doc, scene, sculpt, history, nullptr);
    session.reconcile();
    const bool sameCage = sameFreeformCage(scene.findBody(scene.activeBodyId())->freeformOrNull()->cage(), pushed);
    std::printf("INFO after load: status=%s activeBody=%llu loadedCageIsP2=%d sessionActive=%d selection=%zu firstSel=%u\n",
                projectCodecStatusName(st), (unsigned long long)scene.activeBodyId(), (int)sameCage,
                (int)session.active(), session.selection().size(),
                session.selection().empty() ? 0u : session.selection()[0]);
    // A tool act now operates on P2's cage with P1's selection.
    const FreeformStatus act = session.pushPull(0.1);
    std::printf("INFO pushPull after load -> %s undo=%zu\n", freeformStatusName(act), history.undoDepth());
    return 0;
}
