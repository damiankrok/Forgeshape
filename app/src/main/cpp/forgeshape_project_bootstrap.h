// The start of a project (`APP-H1`): Home, and the transient CAD bootstrap.
//
// Platform-neutral C++17: no JNI, no Android, no Vulkan, no renderer type.
//
// Home is not a project
// ---------------------
// While no project is open the process scene is EMPTY (`ConstructionScene::
// hasProject()` is false). Nothing is drawn, nothing is picked, no history
// exists, `captureProjectDocument` has nothing to write and the codec would
// refuse an empty SCNE anyway, so no `.forge` byte, no checkpoint and no
// fingerprint can describe Home. Nothing fabricates a default primitive or an
// invisible placeholder body to stand in for the project the user has not
// started yet.
//
// The CAD bootstrap
// -----------------
// New Project -> CAD enters the spatial world-plane chooser and then the one
// volatile `SketchSession`, exactly as New Sketch does inside an open project.
// The difference is where the commit lands. Inside a project a commit is one
// `addCadBody` inside one history step; with NO project open there is nothing
// to add the body TO, and `commitFirstCadProject` below instead builds a
// complete one-body `ProjectDocument` from the session's candidate state and
// replaces the (empty) scene through `loadProjectDocument` -- the same
// validated, fail-closed, all-or-nothing path Open and Recover take. So:
//
//   * the sketch session owns no ObjectId and no SceneObject until that moment;
//   * a refused profile, depth or regeneration creates no project at all and
//     leaves the sketch in Ready with its reason named;
//   * the new project starts with an EMPTY history, as every loaded document
//     does -- undoing the creation of the only body would give an empty project,
//     which this product does not have;
//   * Back or Cancel before that moment costs nothing: the scene was empty and
//     stays empty.
//
// The Sculpt bootstrap needs nothing here: it is the existing seeded path
// (add a body, shape it into a sphere, freeze) inside the session-initialization
// bracket, run by the shell through the ordinary entry points.
#pragma once

#include "forgeshape_history.h"
#include "forgeshape_object_id.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

// What the first commit made.
struct FirstProjectReport {
    ObjectId bodyId = kNoObject;
    MeshRevision revision = kNoMeshRevision;
};

// THE first commit: the volatile sketch becomes the first durable CAD project.
//
// Preconditions, each refused by name: no project may be open (`NotSketching`
// -- inside a project the ordinary `SketchSession::commit` is the path), the
// session must be in Ready with a profile chosen (`NotSketching`,
// `AmbiguousProfile`, `ProfileNotFound`), no Construction edit may be open
// (`RefusedEditInProgress`), and the candidate state must validate and
// regenerate (the state's own `CadStatus`, or `RegenerationFailed`).
//
// On success the scene holds exactly one body -- a world-plane CAD body at the
// identity placement, wearing the id the process allocator hands out next --
// which is active and published; the session is Construction; the history is
// empty both ways; and the sketch session is Inactive. On any refusal NOTHING
// changes: the scene stays empty and the sketch stays in Ready.
CadStatus commitFirstCadProject(SketchSession& sketch, ConstructionScene& scene,
                                SculptSession& session, ConstructionHistory& history,
                                FirstProjectReport* outReport = nullptr);

}  // namespace forgeshape
