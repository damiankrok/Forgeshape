// The bridge between the live ForgeShape project and the portable `.forge`
// document.
//
// Platform-neutral C++17: it names `ConstructionScene`, `SculptSession` and
// `ConstructionHistory`, all of which are already platform-neutral, and no
// Android, JNI, Vulkan, filesystem or renderer type appears here. Where the
// bytes come from and where they go is the Android adapter's problem; this file
// only turns a running project into a document and a validated document back
// into a running project.
//
// Two directions, and they are deliberately asymmetric
// ----------------------------------------------------
// CAPTURE reads. It publishes nothing, mints no revision, changes no mode and
// cannot fail: everything it needs is already authoritative state.
//
// LOAD is a transaction, and it FAILS CLOSED. It builds a complete replacement
// scene off to the side -- every body constructed, every Construction mesh
// generated and validated, every Frozen Sculpt Mesh rebuilt -- and only then
// replaces the live project in one step. Anything that goes wrong before that
// step leaves the current scene, every Frozen Sculpt Mesh, the active mode, the
// active body and the session history exactly as they were, because none of
// them has been touched yet.
//
// What a successful load does to history is a rule, not an implementation
// detail: the loaded document starts a FRESH session, so the Construction
// Undo/Redo stacks are cleared. Session history is not project truth and is
// never serialized, and a step left over from before the load would describe a
// scene that no longer exists.
#pragma once

#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"

namespace forgeshape {

// The colour a loaded sculpt vertex gets.
//
// `MeshVertex` interleaves a position and a colour, and the colour feeds only
// the debug-only source-colour shading mode -- Studio Solid and MatCap both
// ignore it. It is therefore presentation, not project truth, and storing three
// floats per vertex for a debug view would grow every sculpt file by a third
// for something no user can see. One neutral value keeps the load deterministic
// and says plainly that the colour was never truth.
constexpr float kLoadedSculptVertexColor = 0.5f;

// Whether THIS BUILD can evaluate a decoded document.
//
// A document may legally leave a body with no geometry branch at all — a Sculpt
// project's CONS companion is optional, and a reader that could not understand
// its version is right to skip it after its validated length. That file is not
// at fault, so the CODEC accepts it; this runtime still cannot build a body it
// has no geometry for, and inventing a default Box for one whose real shape the
// file described would be fabricating project data.
//
// Stated ONCE, here, because two callers ask it: `loadProjectDocument` below,
// and the recovery candidate check, which must never offer a candidate this
// build could not load. Restating the rule in either place is how the two
// silently drifted apart when a second representation arrived.
bool runtimeCanEvaluateProject(const ProjectDocument& document);

// Reads the whole running project into a portable document.
//
// `kind` is the mode the file should reopen in, which the caller takes from the
// live session rather than from anything here. A Construction project may still
// carry retained Frozen Sculpt data, and a Sculpt project still carries its
// Construction Source companion: neither branch overwrites the other, and the
// document keeps both whenever both exist.
ProjectDocument captureProjectDocument(const ConstructionScene& scene, ProjectKind kind);

// What a successful load actually rebuilt, so a caller can log it and a test can
// assert against it rather than against a bare boolean.
struct ProjectLoadReport {
    int bodies = 0;
    int sculptMeshes = 0;
    // How many bodies came back as an Imported Mesh rather than a Construction
    // Source. Reported separately because the two are rebuilt by different
    // paths and a mixed project must be able to say it got both.
    int importedBodies = 0;
    ObjectId activeBodyId = kNoObject;
    ProjectKind kind = ProjectKind::Construction;
    // The mesh revision published for the active body's active representation.
    MeshRevision activeRevision = kNoMeshRevision;
};

// Replaces the live project with `document`, atomically or not at all.
//
// The document must already have been decoded; it is validated again here
// regardless, because this function is also the entry point a caller can reach
// with a document it built itself, and the commit step below is unrecoverable.
//
// Order of operations, and every part of it matters:
//   1. validate the document;
//   2. build every replacement body off to the side, including generating and
//      validating each Construction mesh and rebuilding each Frozen Sculpt Mesh
//      from its stored positions and indices;
//   3. only now: swap the scene's contents, push the id allocator forward past
//      the document's high-water mark, select the active body;
//   4. bind the session to the new active body and enter the document's mode;
//   5. publish the active representation;
//   6. clear the Construction history, because the loaded document is a fresh
//      session over a scene the old steps do not describe.
//
// Refused while a Construction edit is open: a half-finished user act has a
// pre-state the history is holding, and replacing the scene under it would
// leave that state describing bodies that no longer exist.
//
// Callers hold the one existing state mutex, exactly as they do for every other
// scene mutation.
ProjectCodecStatus loadProjectDocument(const ProjectDocument& document, ConstructionScene& scene,
                                       SculptSession& session, ConstructionHistory& history,
                                       ProjectLoadReport* outReport = nullptr);

// A cheap fingerprint of everything a `.forge` document would contain.
//
// Autosave needs one question answered often and answered cheaply: "would
// encoding right now produce a different file than the last checkpoint did?"
// Encoding to find out would serialize the whole project on every check, and a
// per-frame or per-stroke write is exactly the storm the checkpoint policy
// forbids. This is the cheap half of that answer.
//
// It hashes the SEMANTIC VALUES rather than the domain's update counters, and
// that distinction is the whole reason it is correct. `restoreState` — the path
// an undo takes — deliberately does not advance `updateCount`, because an undo
// returns the object to a state it has already counted. A counter-based
// fingerprint would therefore call an undone project unchanged and quietly stop
// protecting it. Hashing the values cannot make that mistake: if the document
// would differ, this differs.
//
// The one place it is a proxy rather than the values themselves is the sculpt
// mesh, where hashing every vertex on every check would cost what encoding
// costs. It takes the `SculptRevision` plus the freeze count and the two
// counts, which together move on every stroke and on every re-freeze — the only
// two things that can change a frozen mesh.
//
// It is a change DETECTOR, never an identity: equal fingerprints mean "no
// checkpoint needed", and nothing may treat one as proof that two projects are
// the same file. Reads only; publishes nothing and mutates nothing.
uint64_t projectSemanticFingerprint(const ConstructionScene& scene, ProjectKind kind);

}  // namespace forgeshape
