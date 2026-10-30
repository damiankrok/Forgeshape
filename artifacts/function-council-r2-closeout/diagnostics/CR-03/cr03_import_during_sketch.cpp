// CR-03 (FUNCTION-COUNCIL-R2-CLOSEOUT-R1): durable GLB import while a sketch
// session (or the support chooser) is open. Audit diagnostic only; never part
// of the product build. Uses the PROCESS-SCOPED singletons the JNI uses
// (constructionScene(), constructionHistory(), sculptSession(),
// sketchSession(), supportChooser()) and replicates, call for call, the
// bodies of three JNI entry points at baseline 70a6c14:
//   importGlbDurable  forgeshape_jni.cpp:8560-8609
//   runHistoryStep    forgeshape_jni.cpp:7995-8020 (guards + undo)
//   sketchCommit      forgeshape_jni.cpp:4487-4530 (branches)
// Logging, the mutex and the camera (endSketchView) are omitted: none of them
// changes domain state. One scenario per process (argv[1]) so the singletons
// start clean.
#include <cstdio>
#include <cstring>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_gltf_import.h"
#include "forgeshape_import_commit.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_surface_authoring.h"

using namespace forgeshape;

static constexpr int kImportCommitStatusBase = 1000;  // forgeshape_jni.cpp kImportCommitStatusBase

// --- replica of Java_..._importGlbDurable after the byte copy --------------
static int jniImportGlbDurable(const std::vector<uint8_t>& bytes, ImportCommitReport* outReport) {
    ParsedGlbScene parsed;
    const GlbImportStatus why = importGlb(bytes.data(), bytes.size(), &parsed);
    if (why != GlbImportStatus::Ok) return static_cast<int>(why);
    ImportCommitReport report;
    ImportCommitStatus committed = ImportCommitStatus::Ok;
    if (sculptSession().inSculptMode()) {
        committed = ImportCommitStatus::RefusedInSculpt;
    } else {
        committed = commitImportedGlbScene(parsed, constructionScene(), constructionHistory(), &report);
    }
    if (outReport) *outReport = report;
    std::printf("  import: parse=Ok commit=%s\n", importCommitStatusName(committed));
    if (committed != ImportCommitStatus::Ok) return kImportCommitStatusBase + static_cast<int>(committed);
    return 0;
}

// --- replica of runHistoryStep's guards + step -----------------------------
static const char* jniUndo() {
    if (sculptSession().inSculptMode()) return "REFUSED:in_sculpt_mode";
    if (sketchSession().active()) return "REFUSED:in_sketch";
    return constructionHistory().undo() ? "MOVED" : "NOTHING";
}

// --- replica of Java_..._sketchCommit's branches ---------------------------
static const char* jniSketchCommit(ObjectId* created, bool* firstProject) {
    *created = kNoObject;
    *firstProject = false;
    ConstructionScene& scene = constructionScene();
    CadStatus status;
    if (surfaceSketchPurpose().active) {
        status = CadStatus::NotSketching;
    } else if (!scene.hasProject()) {
        *firstProject = true;
        FirstProjectReport report;
        status = commitFirstCadProject(sketchSession(), scene, sculptSession(), constructionHistory(), &report);
        *created = report.bodyId;
        if (status == CadStatus::Ok) supportChooser().cancel();
    } else {
        status = sketchSession().commit(scene, constructionHistory(), created);
    }
    return cadStatusName(status);
}

static std::vector<uint8_t> makeGlb() {
    // One default Box exported by the product's own writer, from a PRIVATE
    // scene so the process scene is untouched.
    ConstructionScene source;  // project constructor: one default Box
    publishSceneObject(source.activeBody());
    GlbExportStatus ew = GlbExportStatus::Ok;
    std::vector<uint8_t> glb = exportSceneAsGlb(source, ProjectKind::Construction, &ew);
    std::printf("glb: bytes=%zu export_ok=%d\n", glb.size(), ew == GlbExportStatus::Ok ? 1 : 0);
    return glb;
}

static void seedConstructionProject() {
    // What the product's session-initialization bracket does for a new
    // project: a body, published, recording nothing.
    constructionHistory().beginSessionInitialization();
    SceneObject& body = constructionScene().addBody();
    publishSceneObject(body);
    constructionHistory().endSessionInitialization();
}

static void drawRectangleUnfinished() {
    // The selftests' own seam (forgeshape_sketch_ux_selftest.cpp:392) for an
    // entity a Rectangle gesture would have added: state stays Editing.
    SketchRectangle rect;
    rect.center = SketchPoint{0.0, 0.0};
    rect.width = 0.4;
    rect.height = 0.2;
    addSketchEntity(const_cast<CadSketch*>(&sketchSession().sketch()), rect);
}

static void dump(const char* label) {
    ConstructionScene& scene = constructionScene();
    std::printf("[%s] hasProject=%d bodies=%zu active=%llu undo=%zu redo=%zu editInProgress=%d "
                "sketch_active=%d sketch_state=%s sketch_entities=%zu sketch_plane=%d chooser_active=%d "
                "sculpt=%d fingerprint=%016llx\n",
                label, scene.hasProject() ? 1 : 0, scene.bodyCount(),
                (unsigned long long)(scene.hasProject() ? scene.activeBodyId() : 0),
                constructionHistory().undoDepth(), constructionHistory().redoDepth(),
                constructionHistory().editInProgress() ? 1 : 0,
                sketchSession().active() ? 1 : 0, sketchSessionStateName(sketchSession().state()),
                sketchSession().sketch().entities.size(), (int)sketchSession().plane(),
                supportChooser().active() ? 1 : 0, sculptSession().inSculptMode() ? 1 : 0,
                scene.hasProject() ? (unsigned long long)projectSemanticFingerprint(scene, ProjectKind::Construction) : 0ull);
    if (scene.hasProject()) {
        for (size_t i = 0; i < scene.bodyCount(); ++i) {
            const SceneObject& b = scene.bodyAt(i);
            std::printf("    body[%zu] id=%llu rep=%s\n", i, (unsigned long long)b.objectId(),
                        b.cadOrNull() ? "CAD" : b.importedOrNull() ? "Imported" :
                        b.constructionOrNull() ? "Construction" : "other");
        }
    }
}

static void roundTrip(const char* label) {
    if (!constructionScene().hasProject()) { std::printf("  [%s] encode: no project\n", label); return; }
    ProjectCodecStatus ew = ProjectCodecStatus::Ok;
    const ProjectDocument doc = captureProjectDocument(constructionScene(), ProjectKind::Construction);
    const std::vector<uint8_t> bytes = encodeProjectV1(doc, &ew);
    ProjectDocument back;
    const ProjectCodecStatus dw = bytes.empty() ? ew : decodeProject(bytes.data(), bytes.size(), &back);
    std::printf("  [%s] encode=%s bytes=%zu decode=%s decoded_bodies=%zu\n", label,
                projectCodecStatusName(ew), bytes.size(), projectCodecStatusName(dw),
                dw == ProjectCodecStatus::Ok ? back.scene.bodies.size() : (size_t)0);
}

int main(int argc, char** argv) {
    const char* s = argc > 1 ? argv[1] : "A";
    std::printf("=== CR-03 scenario %s ===\n", s);
    const std::vector<uint8_t> glb = makeGlb();
    ImportCommitReport report;

    if (!std::strcmp(s, "A") || !std::strcmp(s, "B")) {
        seedConstructionProject();
        const CadStatus begun = sketchSession().begin(Workplane::XY);
        drawRectangleUnfinished();
        std::printf("sketch_begin=%s\n", cadStatusName(begun));
        dump("before_import");
        const uint64_t fpBefore = projectSemanticFingerprint(constructionScene(), ProjectKind::Construction);
        const int st = jniImportGlbDurable(glb, &report);
        std::printf("importGlbDurable_return=%d objects=%d firstObjectId=%llu activeBodyId=%llu\n", st,
                    report.objects, (unsigned long long)report.firstObjectId,
                    (unsigned long long)report.activeBodyId);
        dump("after_import");
        std::printf("fingerprint_changed=%d\n",
                    fpBefore != projectSemanticFingerprint(constructionScene(), ProjectKind::Construction));
        roundTrip("after_import");
        std::printf("jni_undo_while_sketch=%s\n", jniUndo());
        dump("after_refused_undo");
        if (!std::strcmp(s, "A")) {
            const CadStatus fin = sketchSession().finish();
            const CadStatus ext = sketchSession().setExtrude(0.1, ExtrudeDirection::AlongNormal);
            std::printf("finish=%s setExtrude=%s selectionChosen=%d\n", cadStatusName(fin),
                        cadStatusName(ext), sketchSession().selectionChosen() ? 1 : 0);
            ObjectId created = kNoObject; bool first = false;
            const char* cs = jniSketchCommit(&created, &first);
            std::printf("sketchCommit=%s created=%llu firstProjectPath=%d\n", cs,
                        (unsigned long long)created, first ? 1 : 0);
            dump("after_commit");
            roundTrip("after_commit");
            std::printf("jni_undo_1=%s\n", jniUndo());
            dump("after_undo_1");
            std::printf("jni_undo_2=%s\n", jniUndo());
            dump("after_undo_2");
            std::printf("jni_undo_3=%s\n", jniUndo());
            dump("after_undo_3");
        } else {
            sketchSession().cancel();
            dump("after_sketch_cancel");
            std::printf("jni_undo_1=%s\n", jniUndo());
            dump("after_undo_1");
        }
    } else if (!std::strcmp(s, "C")) {
        seedConstructionProject();
        supportChooser().begin(true);
        dump("before_import");
        const int st = jniImportGlbDurable(glb, &report);
        std::printf("importGlbDurable_return=%d\n", st);
        dump("after_import");
        std::printf("jni_undo_while_chooser=%s\n", jniUndo());
        dump("after_undo");
    } else if (!std::strcmp(s, "D")) {
        // CONTROL: Sculpt mode, same import path.
        seedConstructionProject();
        ConstructionMesh source;
        const bool built = buildSculptSourceMesh(constructionScene().activeBody(), &source);
        MeshValidation why = MeshValidation::Ok;
        const bool froze = sculptSession().freezeToSculpt(source, constructionScene().activeBodyId(), &why);
        std::printf("sculpt_source=%d freeze=%d\n", built ? 1 : 0, froze ? 1 : 0);
        dump("before_import");
        const uint64_t fpBefore = projectSemanticFingerprint(constructionScene(), ProjectKind::Construction);
        const int st = jniImportGlbDurable(glb, &report);
        std::printf("importGlbDurable_return=%d (expected %d = base + RefusedInSculpt)\n", st,
                    kImportCommitStatusBase + (int)ImportCommitStatus::RefusedInSculpt);
        dump("after_import");
        std::printf("fingerprint_changed=%d\n",
                    fpBefore != projectSemanticFingerprint(constructionScene(), ProjectKind::Construction));
    } else if (!std::strcmp(s, "E")) {
        // CAD bootstrap: no project, a sketch open (UI: mark withdrawn).
        const CadStatus begun = sketchSession().begin(Workplane::XY);
        drawRectangleUnfinished();
        std::printf("sketch_begin=%s\n", cadStatusName(begun));
        dump("before_import");
        const int st = jniImportGlbDurable(glb, &report);
        std::printf("importGlbDurable_return=%d\n", st);
        dump("after_import");
        const CadStatus fin = sketchSession().finish();
        std::printf("finish=%s\n", cadStatusName(fin));
        ObjectId created = kNoObject; bool first = false;
        const char* cs = jniSketchCommit(&created, &first);
        std::printf("sketchCommit=%s created=%llu firstProjectPath=%d\n", cs,
                    (unsigned long long)created, first ? 1 : 0);
        dump("after_commit");
        std::printf("jni_undo_1=%s\n", jniUndo());
        dump("after_undo_1");
        std::printf("jni_undo_2=%s\n", jniUndo());
        dump("after_undo_2");
    }
    return 0;
}
