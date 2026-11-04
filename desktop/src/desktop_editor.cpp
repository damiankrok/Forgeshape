#include "desktop_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

#include "desktop_icons.h"
#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_cad_from_primitive.h"
#include "forgeshape_cad_timeline.h"
#include "forgeshape_cad_from_primitive.h"
#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_display.h"
#include "forgeshape_cad_status_text.h"
#include "forgeshape_editor_core.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_history.h"
#include "forgeshape_log.h"
#include "forgeshape_mass_properties.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_renderer.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_selection.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"
#include "forgeshape_viewport_frame.h"

namespace fsdesktop {

namespace fs = forgeshape;
using fsui::Rect;

namespace {

constexpr const char* kTag = "ForgeShape";

// A READ-ONLY measurement: six significant digits, never the fixed decimals
// that print a 20 mm cube's volume as 0.000 m3, and -0 as 0.
std::string formatMeasure(double v) {
    if (!std::isfinite(v) || std::abs(v) < 1e-15) return "0";
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.6g", v);
    std::string out = buffer;
    if (out.find('e') != std::string::npos) {
        // Plain decimals only: re-express through the field rule.
        out = fsui::Ui::formatFieldNumber(std::stod(out));
    }
    return out;
}

const char* representationIcon(fs::BodyRepresentation r) {
    switch (r) {
        case fs::BodyRepresentation::Cad: return "ic_op_new_body";
        case fs::BodyRepresentation::Freeform: return "ic_freeform";
        case fs::BodyRepresentation::Surface: return "ic_surface";
        case fs::BodyRepresentation::Imported: return "ic_objects";
        default: return "ic_primitive_box";
    }
}

const char* representationLabel(fs::BodyRepresentation r) {
    switch (r) {
        case fs::BodyRepresentation::Cad: return "CAD part";
        case fs::BodyRepresentation::Freeform: return "Freeform";
        case fs::BodyRepresentation::Surface: return "Surface";
        case fs::BodyRepresentation::Imported: return "Imported mesh";
        default: return "Shape";
    }
}

const char* primitiveIcon(fs::PrimitiveKind kind) {
    switch (kind) {
        case fs::PrimitiveKind::Box: return "ic_primitive_box";
        case fs::PrimitiveKind::Cylinder: return "ic_primitive_cylinder";
        case fs::PrimitiveKind::Sphere: return "ic_primitive_sphere";
        case fs::PrimitiveKind::Cone: return "ic_primitive_cone";
        case fs::PrimitiveKind::Capsule: return "ic_primitive_capsule";
        case fs::PrimitiveKind::Plane: return "ic_primitive_plane";
    }
    return "ic_primitive_box";
}

std::string timelineRowLabel(const fs::CadTimelineRow& row) {
    switch (row.kind) {
        case fs::CadTimelineRowKind::Sketch:
            return "Sketch " + std::to_string(row.ordinal);
        case fs::CadTimelineRowKind::Feature: {
            const bool revolve = row.featureKind == fs::CadFeatureKind::Revolve;
            const char* op = "New Body";
            switch (row.operation) {
                case fs::CadFeatureOperation::Add: op = "Add"; break;
                case fs::CadFeatureOperation::Cut: op = "Cut"; break;
                case fs::CadFeatureOperation::Intersect: op = "Intersect"; break;
                default: break;
            }
            return std::string(revolve ? "Revolve " : "Extrude ") + std::to_string(row.ordinal) +
                   " \xC2\xB7 " + op;
        }
        case fs::CadTimelineRowKind::Finish:
            return "Edge finish " + std::to_string(row.ordinal);
        case fs::CadTimelineRowKind::Pattern:
            return "Pattern " + std::to_string(row.ordinal);
        case fs::CadTimelineRowKind::Hole:
            return "Hole " + std::to_string(row.ordinal);
    }
    return "Step";
}

const char* timelineRowIcon(const fs::CadTimelineRow& row) {
    switch (row.kind) {
        case fs::CadTimelineRowKind::Sketch: return "ic_sketch_new";
        case fs::CadTimelineRowKind::Finish: return "ic_edge_fillet";
        case fs::CadTimelineRowKind::Pattern: return "ic_pattern_linear";
        case fs::CadTimelineRowKind::Hole: return "ic_hole";
        case fs::CadTimelineRowKind::Feature:
            switch (row.operation) {
                case fs::CadFeatureOperation::Add: return "ic_op_add";
                case fs::CadFeatureOperation::Cut: return "ic_op_cut";
                case fs::CadFeatureOperation::Intersect: return "ic_op_intersect";
                default: return "ic_op_new_body";
            }
    }
    return "ic_feature_history";
}

// The Tool Rail's eight sketch tools, in its reading order.
struct SketchToolEntry {
    fs::SketchTool tool;
    const char* icon;
    const char* name;
    const char* hint;
    // The single key that holds the tool while a sketch is drawn
    // (`WORKBENCH-UX-R1`), the letters every CAD application uses.
    char key;
};
constexpr SketchToolEntry kSketchTools[] = {
    {fs::SketchTool::Select, "ic_tool_select", "Select", "Click a curve to select it; Delete removes it", 'V'},
    {fs::SketchTool::Line, "ic_tool_line", "Line", "Press, drag and release to draw a line", 'L'},
    {fs::SketchTool::Polyline, "ic_tool_polyline", "Polyline",
     "Click each corner; click the first point to close, Esc to end", 'P'},
    {fs::SketchTool::Rectangle, "ic_tool_rectangle", "Rectangle", "Drag from one corner to the other", 'R'},
    {fs::SketchTool::Circle, "ic_tool_circle", "Circle", "Drag from the centre outwards", 'C'},
    {fs::SketchTool::Arc, "ic_tool_arc", "Arc", "Drag the chord, then click the point the arc passes", 'A'},
    {fs::SketchTool::Spline, "ic_tool_spline", "Spline", "Click the points the curve passes; Esc to end", 'S'},
    {fs::SketchTool::Trim, "ic_tool_trim", "Trim", "Click or swipe across a curve to cut it back", 'T'},
};

// A refusal in the user's words; the code's own name otherwise.
std::string sketchStatusText(fs::CadStatus status) {
    // One sentence per refusal, the core's table (`WORKBENCH-UX-R1`): the
    // desktop never shows an enum's name to a person.
    std::string text = fs::cadStatusUserText(status);
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

const char* extentIcon(fs::ExtrudeExtentMode mode) {
    switch (mode) {
        case fs::ExtrudeExtentMode::Symmetric: return "ic_extent_symmetric";
        case fs::ExtrudeExtentMode::TwoSides: return "ic_extent_two_sides";
        case fs::ExtrudeExtentMode::ThroughAll: return "ic_extent_through_all";
        case fs::ExtrudeExtentMode::ToFace: return "ic_extent_to_face";
        default: return "ic_extent_one_side";
    }
}

const char* extentLabel(fs::ExtrudeExtentMode mode) {
    switch (mode) {
        case fs::ExtrudeExtentMode::Symmetric: return "Symmetric";
        case fs::ExtrudeExtentMode::TwoSides: return "Two Sides";
        case fs::ExtrudeExtentMode::ThroughAll: return "Through All";
        case fs::ExtrudeExtentMode::ToFace: return "To Face";
        default: return "One Side";
    }
}

const char* operationIcon(fs::CadFeatureOperation op) {
    switch (op) {
        case fs::CadFeatureOperation::Add: return "ic_op_add";
        case fs::CadFeatureOperation::Cut: return "ic_op_cut";
        case fs::CadFeatureOperation::Intersect: return "ic_op_intersect";
        default: return "ic_op_new_body";
    }
}

const char* operationLabel(fs::CadFeatureOperation op) {
    switch (op) {
        case fs::CadFeatureOperation::Add: return "Add";
        case fs::CadFeatureOperation::Cut: return "Cut";
        case fs::CadFeatureOperation::Intersect: return "Intersect";
        default: return "New Body";
    }
}

}  // namespace

DesktopEditor::DesktopEditor() = default;

DesktopEditor::~DesktopEditor() { stop(); }

bool DesktopEditor::start(fs::PlatformWindow window, DesktopPlatform* platform,
                          const std::vector<std::string>& fontCandidates,
                          const std::vector<std::string>& boldFontCandidates) {
    platform_ = platform;
    std::shared_ptr<fsui::TrueTypeFont> regular, bold;
    for (const std::string& path : fontCandidates) {
        auto font = std::make_shared<fsui::TrueTypeFont>();
        if (font->loadFile(path)) {
            regular = font;
            break;
        }
    }
    for (const std::string& path : boldFontCandidates) {
        auto font = std::make_shared<fsui::TrueTypeFont>();
        if (font->loadFile(path)) {
            bold = font;
            break;
        }
    }
    FS_LOG_INFO(kTag, "FORGESHAPE_DESKTOP_FONT regular=%d bold=%d", regular ? 1 : 0, bold ? 1 : 0);
    atlas_.setFont(regular, bold);
    for (size_t i = 0; i < kEmbeddedIconCount; ++i) {
        atlas_.addIconSource(kEmbeddedIcons[i].name, kEmbeddedIcons[i].xml);
    }
    reloadTheme();
    ui_.reset(new fsui::Ui(atlas_, theme_));

    renderer_.reset(new fs::Renderer());
    if (!renderer_->createInstance() || !renderer_->attachSurface(window)) {
        FS_LOG_ERROR(kTag, "FORGESHAPE_DESKTOP_RENDERER_FAIL");
        renderer_.reset();
        return false;
    }
    refreshTitle();
    report("Ready");
    return true;
}

void DesktopEditor::stop() {
    if (renderer_) {
        renderer_->detachSurface();
        renderer_->destroyInstance();
        renderer_.reset();
    }
}

void DesktopEditor::resized() {
    if (renderer_) renderer_->requestResize();
}

void DesktopEditor::requestCapture() {
    captureRequested_ = true;
    if (renderer_) renderer_->requestFrameCapture();
}

bool DesktopEditor::captureReady() const { return renderer_ && renderer_->frameCaptureReady(); }

const std::vector<uint8_t>& DesktopEditor::capture(uint32_t* width, uint32_t* height) const {
    return renderer_->capturedFrame(width, height);
}

void DesktopEditor::reloadTheme() {
    theme_ = fsui::Theme();
    if (platform_ != nullptr) {
        const std::string dir = platform_->settingsDirectory();
        if (!dir.empty()) {
            std::string reportText;
            if (theme_.loadOverrides(dir + "/theme.json", &reportText)) {
                FS_LOG_INFO(kTag, "FORGESHAPE_DESKTOP_THEME %s", reportText.c_str());
                status_ = "Theme: " + reportText;
            }
        }
    }
}

void DesktopEditor::report(const std::string& message, bool error) {
    status_ = message;
    statusError_ = error;
    FS_LOG_INFO(kTag, "FORGESHAPE_DESKTOP_STATUS %s%s", error ? "error:" : "", message.c_str());
}

void DesktopEditor::refreshTitle() {
    if (platform_ == nullptr) return;
    platform_->setTitle(projectName_.empty() ? std::string("ForgeShape")
                                             : projectName_ + " \xE2\x80\x94 ForgeShape");
}

std::string DesktopEditor::bodyLabel(uint64_t objectId) const {
    const fs::SceneObject* body = fs::constructionScene().findBody(objectId);
    if (body != nullptr && !body->name().empty()) return body->name();
    return "Body #" + std::to_string(objectId);
}

// ---------------------------------------------------------------------------
// Acts. Each takes the state mutex for exactly the core call it makes.
// ---------------------------------------------------------------------------

namespace {

// The caller holds the state mutex.
uint64_t currentFingerprintLocked() {
    const fs::ConstructionScene& scene = fs::constructionScene();
    if (!scene.hasProject()) return 0;
    return fs::projectSemanticFingerprint(scene, fs::sculptSession().inSculptMode()
                                                     ? fs::ProjectKind::Sculpt
                                                     : fs::ProjectKind::Construction);
}

}  // namespace

void DesktopEditor::noteSaved() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    savedFingerprint_ = currentFingerprintLocked();
}

bool DesktopEditor::hasUnsavedChanges() const {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    if (!fs::constructionScene().hasProject()) return false;
    return currentFingerprintLocked() != savedFingerprint_;
}

bool DesktopEditor::settleUnsavedChanges() {
    if (platform_ == nullptr || !hasUnsavedChanges()) return true;
    const int answer = platform_->askSaveChanges();
    if (answer == 0) return false;
    if (answer == 1) {
        saveProject();
        return !hasUnsavedChanges();
    }
    return true;
}

void DesktopEditor::newCadPart() {
    if (!settleUnsavedChanges()) return;
    std::unique_lock<std::mutex> lock(fs::editorStateMutex());
    fs::ConstructionScene& scene = fs::constructionScene();
    fs::ConstructionHistory& history = fs::constructionHistory();
    fs::constructionHistory().clear();
    scene.closeProject();
    // A new project's first body is seeded inside the session-initialization
    // bracket, so the user's first Undo never rewinds the seed (the Android
    // New Project's rule).
    history.beginSessionInitialization();
    fs::SceneObject& box = scene.addBody();
    fs::publishSceneObject(box);
    fs::ObjectId part = fs::kNoObject;
    const fs::CadStatus status = fs::convertPrimitiveToCadPart(scene, history, box.objectId(), &part);
    history.endSessionInitialization();
    history.clear();
    if (status != fs::CadStatus::Ok) {
        report(std::string("Could not start a CAD part: ") + fs::cadStatusName(status), true);
        return;
    }
    fs::editorSelection().setSelected(part);
    lock.unlock();
    noteSaved();
    projectName_ = "Untitled";
    homeVisible_ = false;
    refreshTitle();
    report("New CAD part");
}

void DesktopEditor::newFreeform() {
    if (!settleUnsavedChanges()) return;
    std::unique_lock<std::mutex> lock(fs::editorStateMutex());
    fs::ConstructionScene& scene = fs::constructionScene();
    fs::ConstructionHistory& history = fs::constructionHistory();
    history.clear();
    scene.closeProject();
    history.beginSessionInitialization();
    fs::ObjectId id = fs::kNoObject;
    const fs::FreeformStatus status = fs::createFreeformBody(scene, history, 0, &id);
    history.endSessionInitialization();
    history.clear();
    if (status != fs::FreeformStatus::Ok) {
        scene.closeProject();
        report("Could not start a Freeform body", true);
        return;
    }
    fs::editorSelection().setSelected(id);
    lock.unlock();
    noteSaved();
    projectName_ = "Untitled";
    homeVisible_ = false;
    refreshTitle();
    report("New Freeform body");
}

void DesktopEditor::openProject() {
    if (platform_ == nullptr || !settleUnsavedChanges()) return;
    std::vector<uint8_t> bytes;
    std::string name;
    if (!platform_->openFile("ForgeShape project", "forge", &bytes, &name)) return;
    openProjectBytes(bytes, name);
}

void DesktopEditor::openProjectBytes(const std::vector<uint8_t>& bytes, const std::string& name) {
    fs::ProjectDocument document;
    fs::ProjectCodecStatus status = fs::decodeProject(bytes.data(), bytes.size(), &document);
    if (status == fs::ProjectCodecStatus::Ok) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::editorSketchCancel();
        fs::supportChooser().cancel();
        fs::ProjectLoadReport loaded;
        status = fs::loadProjectDocument(document, fs::constructionScene(), fs::sculptSession(),
                                         fs::constructionHistory(), &loaded);
    }
    if (status != fs::ProjectCodecStatus::Ok) {
        // All-or-nothing: a refused file changed nothing at all.
        report(std::string("Could not open ") + name + ": " + fs::projectCodecStatusName(status),
               true);
        return;
    }
    noteSaved();
    projectName_ = name;
    homeVisible_ = false;
    refreshTitle();
    report("Opened " + name);
}

void DesktopEditor::saveProject() {
    if (platform_ == nullptr) return;
    std::vector<uint8_t> bytes;
    fs::ProjectCodecStatus why = fs::ProjectCodecStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::constructionScene().hasProject()) return;
        const fs::ProjectKind kind = fs::sculptSession().inSculptMode() ? fs::ProjectKind::Sculpt
                                                                         : fs::ProjectKind::Construction;
        bytes = fs::encodeProjectV1(fs::captureProjectDocument(fs::constructionScene(), kind), &why);
    }
    if (bytes.empty()) {
        report(std::string("Could not save: ") + fs::projectCodecStatusName(why), true);
        return;
    }
    std::string name;
    const std::string suggested = (projectName_.empty() || projectName_ == "Untitled")
                                      ? std::string("Project.forge")
                                      : projectName_;
    if (!platform_->saveFile("ForgeShape project", "forge", suggested, bytes, &name)) return;
    noteSaved();
    projectName_ = name;
    refreshTitle();
    report("Saved " + name);
}

void DesktopEditor::exportGlb() {
    if (platform_ == nullptr) return;
    std::vector<uint8_t> bytes;
    fs::GlbExportStatus why = fs::GlbExportStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::constructionScene().hasProject()) return;
        bytes = fs::exportSceneAsGlb(fs::constructionScene(), fs::ProjectKind::Construction, &why);
    }
    if (bytes.empty()) {
        report(std::string("Could not export: ") + fs::glbExportStatusName(why), true);
        return;
    }
    std::string name;
    if (platform_->saveFile("glTF binary", "glb", "Model.glb", bytes, &name)) {
        report("Exported " + name);
    }
}

void DesktopEditor::undo() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    // While a sketch is open the pair steps its own volatile branch
    // (`SKETCH-KNIFE-UNDO-R1`); Construction Undo stays refused under it.
    if (fs::sketchSession().active()) {
        if (fs::sketchSession().undoSketchEdit()) report("Undo");
        return;
    }
    fs::ConstructionRestoreReport restored;
    if (fs::constructionHistory().undo(&restored)) {
        fs::supportChooser().cancel();
        report("Undo");
    }
}

void DesktopEditor::redo() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    if (fs::sketchSession().active()) {
        if (fs::sketchSession().redoSketchEdit()) report("Redo");
        return;
    }
    fs::ConstructionRestoreReport restored;
    if (fs::constructionHistory().redo(&restored)) {
        fs::supportChooser().cancel();
        report("Redo");
    }
}

void DesktopEditor::addPrimitive(int kind) {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::ConstructionScene& scene = fs::constructionScene();
    if (!scene.hasProject()) return;
    fs::PrimitiveSpec spec = fs::PrimitiveSpec::forBox(1.0, 1.0, 1.0);
    switch (static_cast<fs::PrimitiveKind>(kind)) {
        case fs::PrimitiveKind::Cylinder: spec = fs::PrimitiveSpec::forCylinder(1.0, 1.0); break;
        case fs::PrimitiveKind::Sphere: spec = fs::PrimitiveSpec::forSphere(1.0); break;
        case fs::PrimitiveKind::Cone: spec = fs::PrimitiveSpec::forCone(1.0, 1.0); break;
        case fs::PrimitiveKind::Capsule: spec = fs::PrimitiveSpec::forCapsule(0.6, 1.4); break;
        case fs::PrimitiveKind::Plane: spec = fs::PrimitiveSpec::forPlane(2.0, 2.0); break;
        default: break;
    }
    // One user act, one Undo: the append and the shape inside one edit.
    fs::ScopedConstructionEdit edit(fs::constructionHistory());
    fs::SceneObject& body = scene.addBody();
    fs::publishSceneObject(body);
    fs::applyConstructionPrimitive(spec);
    fs::editorSelection().setSelected(body.objectId());
    report(std::string("Added ") + bodyLabel(body.objectId()));
}

void DesktopEditor::makeCadPart() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::ConstructionScene& scene = fs::constructionScene();
    if (!scene.hasProject()) return;
    fs::ObjectId part = fs::kNoObject;
    const fs::CadStatus status = fs::convertPrimitiveToCadPart(
        scene, fs::constructionHistory(), scene.activeBodyId(), &part);
    if (status != fs::CadStatus::Ok) {
        report(std::string("No CAD part for this body: ") + fs::cadStatusName(status), true);
        return;
    }
    fs::editorSelection().setSelected(part);
    report("Made a CAD part");
}

void DesktopEditor::deleteActive() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    // Inside a sketch, Delete means the selected curve; a body is never
    // deleted from under a sketch.
    if (fs::sketchSession().active()) {
        if (fs::sketchSession().state() == fs::SketchSessionState::Editing &&
            fs::sketchSession().deleteSelected() == fs::CadStatus::Ok) {
            report("Deleted the selected curve");
        }
        return;
    }
    fs::ConstructionScene& scene = fs::constructionScene();
    if (!scene.hasProject()) return;
    const fs::ObjectId id = scene.activeBodyId();
    const std::string label = bodyLabel(id);
    const fs::DeleteBodyStatus status = fs::deleteSceneBody(id, scene, fs::constructionHistory());
    if (status != fs::DeleteBodyStatus::Ok) {
        report(status == fs::DeleteBodyStatus::RefusedLastBody
                   ? std::string("The last body of a project cannot be deleted")
                   : std::string("Could not delete: ") + fs::deleteBodyStatusName(status),
               true);
        return;
    }
    if (scene.hasProject()) fs::editorSelection().setSelected(scene.activeBodyId());
    report("Deleted " + label);
}

void DesktopEditor::closeProject() {
    if (!settleUnsavedChanges()) return;
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorSketchCancel();
    fs::supportChooser().cancel();
    fs::constructionHistory().clear();
    fs::constructionScene().closeProject();
    fs::editorSelection().clearSelection();
    projectName_.clear();
    homeVisible_ = true;
    refreshTitle();
}

// Move, Rotate or Scale (0..2) over the active body, or -1 to put the handles
// away; choosing the mode already up puts them away too. Through the core's
// one activation rule, so a locked or face-placed body refuses by name.
void DesktopEditor::setTransformMode(int mode) {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::GizmoSession& gizmo = fs::gizmoSession();
    if (mode < 0 || (gizmo.active() && static_cast<int>(gizmo.mode()) == mode)) {
        fs::editorSetGizmoActive(false);
        return;
    }
    gizmo.setMode(static_cast<fs::GizmoMode>(mode));
    switch (fs::editorSetGizmoActive(true)) {
        case fs::GizmoActivation::RefusedLocked: report("The body is locked", true); break;
        case fs::GizmoActivation::RefusedDerived:
            report("This part is placed by the face it stands on", true);
            break;
        case fs::GizmoActivation::RefusedInSculpt: report("Not while sculpting", true); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Sketch and Extrude (`DESKTOP-R2`)
// ---------------------------------------------------------------------------

void DesktopEditor::newSketch() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    if (fs::sketchSession().active() || fs::supportChooser().active()) return;
    fs::editorSetGizmoActive(false);
    if (!fs::constructionScene().hasProject()) {
        // The CAD bootstrap (`APP-H1`): ONE volatile sketch over the empty
        // scene, on XY seen along +Z, owning no ObjectId; the first Extrude
        // creates the project, and Cancel before it costs nothing.
        const fs::CadStatus status = fs::editorSketchBegin(fs::Workplane::XY);
        if (status != fs::CadStatus::Ok) {
            report("Could not start a sketch: " + sketchStatusText(status), true);
            return;
        }
        homeVisible_ = false;
        projectName_.clear();
        report("Draw a closed shape on XY, then Finish sketch");
        return;
    }
    if (!fs::editorSupportChooserBegin(true)) {
        report("A sketch cannot start now", true);
        return;
    }
    report("Click a plane or a flat face to sketch on");
}

void DesktopEditor::sketchOnPlane(int plane) {
    fs::Workplane workplane;
    if (!fs::workplaneFromIndex(plane, &workplane)) return;
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorSetGizmoActive(false);
    const fs::CadStatus status = fs::editorSketchBegin(workplane);
    if (status != fs::CadStatus::Ok) {
        report("Could not start a sketch: " + sketchStatusText(status), true);
        return;
    }
    report(std::string("Sketch on ") + fs::workplaneName(workplane));
}

void DesktopEditor::setSketchTool(int tool) {
    fs::SketchTool chosen;
    if (!fs::sketchToolFromIndex(tool, &chosen)) return;
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    if (fs::sketchSession().setTool(chosen)) {
        for (const SketchToolEntry& entry : kSketchTools) {
            if (entry.tool == chosen) report(entry.hint);
        }
    }
}

void DesktopEditor::finishSketch() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::CadFeatureViewSource view = fs::CadFeatureViewSource::Unavailable;
    const fs::CadStatus status = fs::editorSketchFinish(&view);
    if (status != fs::CadStatus::Ok) {
        report(sketchStatusText(status), true);
        return;
    }
    report(fs::sketchSession().selectionChosen()
               ? "Set the distance and Extrude, or drag the arrow"
               : "Click the region to extrude");
}

void DesktopEditor::backToSketch() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorSketchBackToEditing();
    if (fs::sketchSession().state() == fs::SketchSessionState::Ready) {
        report(sketchStatusText(fs::sketchSession().lastStatus()), true);
    }
}

void DesktopEditor::commitSketch() {
    fs::EditorSketchCommit commit;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        // A staged edit of a committed part commits as ONE step on the same
        // body (`WORKBENCH-UX-R1`); a new sketch commits as before.
        if (fs::sketchSession().editingBodyId() != fs::kNoObject) {
            const fs::CadStatus edited = fs::editorSketchCommitEdit();
            if (edited != fs::CadStatus::Ok) {
                report("Not applied: " + sketchStatusText(edited), true);
            } else {
                report("Edit applied \xC2\xB7 one Undo takes it back");
            }
            return;
        }
        commit = fs::editorSketchCommit();
        if (commit.status == fs::CadStatus::Ok && commit.created != fs::kNoObject) {
            fs::editorSelection().setSelected(commit.created);
        }
    }
    if (commit.status != fs::CadStatus::Ok) {
        report("Not extruded: " + sketchStatusText(commit.status), true);
        return;
    }
    if (commit.firstProject) {
        // A new project the user drew is work: it is unsaved until a Save,
        // so closing asks first.
        savedFingerprint_ = 0;
        projectName_ = "Untitled";
        refreshTitle();
        report("Created the part");
        return;
    }
    report(std::string("Extruded \xC2\xB7 ") + operationLabel(commit.operation));
}

bool DesktopEditor::editHistoryRow(const fs::CadTimelineRow& row) {
    fs::CadStatus status = fs::CadStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::constructionScene().hasProject()) return false;
        const fs::ObjectId body = fs::constructionScene().activeBodyId();
        if (row.kind == fs::CadTimelineRowKind::Feature) {
            status = fs::editorSketchBeginEditFeature(body, row.id, /*startReady=*/true);
        } else if (row.kind == fs::CadTimelineRowKind::Sketch) {
            status = row.editFeatureId != 0
                         ? fs::editorSketchBeginEditFeature(body, row.editFeatureId, false)
                         : fs::editorSketchBeginEditSketch(body, row.sketchId);
        } else {
            return false;
        }
    }
    if (status != fs::CadStatus::Ok) {
        report("Cannot edit " + timelineRowLabel(row) + ": " + sketchStatusText(status), true);
        return false;
    }
    sketchPointer_ = false;
    report("Editing " + timelineRowLabel(row) + " \xC2\xB7 Extrude or Finish applies it as one step");
    return true;
}

void DesktopEditor::setStandardView(int view) {
    fs::StandardView chosen;
    if (!fs::standardViewFromIndex(view, &chosen)) return;
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorCamera().setStandardView(chosen);
}

void DesktopEditor::fitAll() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorFitAll();
}

bool DesktopEditor::widgetRect(const std::string& id, fsui::Rect* out) const {
    return ui_ && ui_->widgetRect(id, out);
}

void DesktopEditor::cancelSketch() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    fs::editorSketchCancel();
    fs::supportChooser().cancel();
    sketchPointer_ = false;
    if (!fs::constructionScene().hasProject()) homeVisible_ = true;
    report("Sketch cancelled");
}

bool DesktopEditor::shortcut(char key, bool ctrl, bool shift) {
    if (ui_ && ui_->typing()) return false;
    if (ctrl && key >= '1' && key <= '7') {
        // Ctrl+1..7: Home, Front, Back, Left, Right, Top, Bottom -- refused by
        // the camera inside a sketch's exact view, as the view bar's are.
        setStandardView(key - '1');
        return true;
    }
    if (ctrl) {
        switch (key) {
            case 'Z': shift ? redo() : undo(); return true;
            case 'Y': redo(); return true;
            case 'S': saveProject(); return true;
            case 'O': openProject(); return true;
            case 'E': exportGlb(); return true;
            default: return false;
        }
    }
    if (key == 0x7F) {  // Delete
        deleteActive();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------

bool DesktopEditor::frame(int width, int height, float scale, const fsui::InputState& input) {
    if (!renderer_ || width <= 0 || height <= 0) return true;
    input_ = input;
    scale_ = scale;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::editorCamera().setViewport(width, height);
    }
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (fs::gizmoSession().active() && !gizmoDragging_) {
            fs::editorSetGizmoActive(true);  // re-asks the rule for the body now active
        }
    }
    // Enter and Esc in a sketch, read before the interface so a number field
    // that is still being typed into keeps its own Enter.
    sketchKeys();
    ui_->begin(width, height, scale, input);
    buildInterface(width, height);
    // The viewport gets what no surface or control took this frame.
    routeViewportInput(width, height);
    renderer_->setUiFrame(ui_->end());

    fs::ViewportFrameSources sources;  // no shell overlays on the desktop yet
    fs::composeViewportFrame(*renderer_, sources);
    return renderer_->drawFrame();
}

void DesktopEditor::routeViewportInput(int width, int height) {
    const bool overUi = ui_->pointerCaptured() || homeVisible_;
    if (routeSketchInput(width, height, overUi)) return;
    fs::CameraController& camera = fs::editorCamera();
    // The wheel zooms about the pointer.
    if (!overUi && input_.wheel != 0.0f) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        camera.zoomAtPixel(input_.mouseX, input_.mouseY, input_.wheel);
    }
    // A transform handle under the left button takes the whole drag: the
    // gizmo's own capture, frozen basis and one-transaction commit.
    if (gizmoDragging_) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::GizmoSession& gizmo = fs::gizmoSession();
        const bool cancel = input_.mousePressed[1] ||
                            std::find(input_.keys.begin(), input_.keys.end(), fsui::Key::Escape) !=
                                input_.keys.end();
        if (cancel) {
            gizmo.cancelDrag();
            gizmoDragging_ = false;
        } else if (input_.mouseReleased[0]) {
            gizmo.updateDrag(0, camera.snapshot(), input_.mouseX, input_.mouseY, width, height);
            gizmo.commitDrag();
            gizmoDragging_ = false;
        } else {
            gizmo.updateDrag(0, camera.snapshot(), input_.mouseX, input_.mouseY, width, height);
        }
        return;
    }
    if (input_.mousePressed[0] && !overUi && dragButton_ < 0) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::GizmoSession& gizmo = fs::gizmoSession();
        if (gizmo.active() &&
            gizmo.hitTest(camera.snapshot(), input_.mouseX, input_.mouseY, width, height) !=
                fs::GizmoHandle::None &&
            gizmo.beginDrag(0, camera.snapshot(), input_.mouseX, input_.mouseY, width, height)) {
            gizmoDragging_ = true;
            return;
        }
    }
    for (int b = 0; b < 3; ++b) {
        if (input_.mousePressed[b] && !overUi && dragButton_ < 0) {
            dragButton_ = b;
            dragLastX_ = pressX_ = input_.mouseX;
            dragLastY_ = pressY_ = input_.mouseY;
            dragTravelled_ = false;
        }
    }
    if (dragButton_ >= 0) {
        const float dx = input_.mouseX - dragLastX_;
        const float dy = input_.mouseY - dragLastY_;
        if (std::fabs(input_.mouseX - pressX_) + std::fabs(input_.mouseY - pressY_) > 4.0f * scale_) {
            dragTravelled_ = true;
        }
        if ((dx != 0.0f || dy != 0.0f) && dragTravelled_) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            // Left and right orbit; middle (or Shift with either) pans -- the
            // tablet's mouse grammar, and the left button keeps a finger's
            // meaning: a drag orbits, a click selects.
            if (dragButton_ == 2 || input_.shift) {
                camera.panByPixels(dx, dy);
            } else {
                camera.orbitByPixels(dx, dy);
            }
        }
        dragLastX_ = input_.mouseX;
        dragLastY_ = input_.mouseY;
        if (input_.mouseReleased[dragButton_]) {
            if (dragButton_ == 1 && !dragTravelled_) {
                // A still right click on a body: its context menu, at the
                // pointer. The pick selects it first, as a left click would,
                // so the menu is about the body the user is looking at.
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                const fs::SceneHit hit =
                    fs::pickScene(camera.snapshot(), input_.mouseX, input_.mouseY, width, height);
                contextOpen_ = hit.hit;
                if (hit.hit) {
                    fs::editorSelection().applyPick(hit);
                    fs::constructionScene().setActiveBody(hit.objectId);
                    contextBody_ = hit.objectId;
                    contextX_ = input_.mouseX;
                    contextY_ = input_.mouseY;
                }
            }
            if (dragButton_ == 0 && !dragTravelled_) {
                // A click: the same pick the Android tap makes.
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                const fs::SceneHit hit =
                    fs::pickScene(camera.snapshot(), input_.mouseX, input_.mouseY, width, height);
                fs::editorSelection().applyPick(hit);
                if (hit.hit) {
                    fs::constructionScene().setActiveBody(hit.objectId);
                }
            }
            dragButton_ = -1;
        }
    }
}

void DesktopEditor::buildInterface(int width, int height) {
    bool hasProject = false;
    bool sketching = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        hasProject = fs::constructionScene().hasProject();
        sketching = fs::sketchSession().active();
    }
    // The CAD bootstrap's sketch stands over an EMPTY scene: no project yet,
    // and not Home either.
    if (!hasProject && !sketching) homeVisible_ = true;
    if (sketching) homeVisible_ = false;
    if (homeVisible_) {
        buildHome(width, height);
        return;
    }
    // The SolidCraft / Shapr3D shape (`WORKBENCH-RIBBON-R1`): a title row of
    // file acts, the ribbon of captioned commands under it, the Browser and
    // the Properties panel beside the model, the History strip along the
    // bottom, and the view cube and view bar standing on the model.
    fsui::Ui& ui = *ui_;
    const float w = static_cast<float>(width);
    const Rect title{0, 0, w, ui.px(theme_.toolbarHeight)};
    const Rect ribbon{0, title.bottom(), w, ui.px(theme_.ribbonHeight)};
    const Rect status{0, height - ui.px(theme_.statusHeight), w, ui.px(theme_.statusHeight)};
    // The panels are DOCKED (`WORKBENCH-UX-R1`): flush to the window edges,
    // square, opaque and unshadowed, as SolidCraft's and Fusion's are -- five
    // floating cards with a gutter round each read as card soup, and the
    // model showed through them.
    const Rect history{0, status.y - ui.px(theme_.historyHeight), w, ui.px(theme_.historyHeight)};
    const float top = ribbon.bottom();
    const float bottom = history.y;
    float viewLeft = 0, viewRight = w;
    if (browserOpen_) {
        const Rect browser{0, top, ui.px(theme_.browserWidth), bottom - top};
        buildBrowser(browser);
        viewLeft = browser.right();
    }
    if (propertiesOpen_) {
        const float pw = ui.px(theme_.propertiesWidth);
        const Rect properties{w - pw, top, pw, bottom - top};
        buildProperties(properties);
        viewRight = properties.x;
    }
    viewportRect_ = {viewLeft, top, viewRight - viewLeft, bottom - top};
    buildViewCube(viewportRect_);
    buildViewBar(viewportRect_);
    buildHistoryStrip(history);
    buildRibbon(ribbon);
    buildTitleBar(title);
    buildStatusBar(status);
    buildContextMenu(width, height);
}

void DesktopEditor::buildContextMenu(int width, int height) {
    if (!contextOpen_) return;
    fsui::Ui& ui = *ui_;
    struct Row { const char* id; const char* icon; std::string text; int act; };
    std::vector<Row> rows;
    bool visible = true, locked = false, cad = false, sketching = false;
    size_t bodies = 0;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::ConstructionScene& scene = fs::constructionScene();
        const fs::SceneObject* body = scene.hasProject() ? scene.findBody(contextBody_) : nullptr;
        sketching = fs::sketchSession().active() || fs::supportChooser().active();
        if (body == nullptr || sketching) {
            contextOpen_ = false;
            return;
        }
        visible = body->visible();
        locked = body->locked();
        cad = body->cadOrNull() != nullptr;
        bodies = scene.bodyCount();
    }
    // Only acts that already exist, each drawn only where it can succeed.
    if (cad) rows.push_back({"ctx_edit_sketch", "ic_sketch_new", "Edit sketch", 0});
    rows.push_back({"ctx_fit_body", "ic_view_fit", "Fit to body", 1});
    rows.push_back({"ctx_toggle_visibility", visible ? "ic_object_hidden" : "ic_object_visible",
                    visible ? "Hide" : "Show", 2});
    rows.push_back({"ctx_toggle_lock", locked ? "ic_object_unlocked" : "ic_object_locked",
                    locked ? "Unlock" : "Lock", 3});
    rows.push_back({"ctx_duplicate", "ic_object_duplicate", "Duplicate", 4});
    if (bodies > 1) rows.push_back({"ctx_delete", "ic_delete", "Delete", 5});
    const float rowH = ui.px(32), w = ui.px(200), pad = ui.px(6);
    const float h = rows.size() * rowH + 2 * pad;
    // Flipped to stay on screen near the window's edges.
    float x = contextX_ + ui.px(4), y = contextY_ + ui.px(4);
    if (x + w > width - ui.px(4)) x = contextX_ - w - ui.px(4);
    if (y + h > height - ui.px(4)) y = contextY_ - h - ui.px(4);
    const Rect menu{x, y, w, h};
    ui.panel(menu);
    int chosen = -1;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Rect r{x + pad, y + pad + i * rowH, w - 2 * pad, rowH};
        if (ui.listRow(rows[i].id, r, rows[i].text, rows[i].icon, false)) chosen = rows[i].act;
    }
    const bool escape = std::find(input_.keys.begin(), input_.keys.end(), fsui::Key::Escape) !=
                        input_.keys.end();
    const bool clickedAway = (input_.mousePressed[0] || input_.mousePressed[1] ||
                              input_.mousePressed[2]) &&
                             !menu.contains(input_.mouseX, input_.mouseY);
    if (chosen < 0) {
        if (escape || clickedAway) contextOpen_ = false;
        return;
    }
    contextOpen_ = false;
    const fs::ObjectId id = contextBody_;
    switch (chosen) {
        case 0: {
            fs::CadStatus status;
            {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                status = fs::editorSketchBeginEditFeature(id, 1, false);
            }
            if (status != fs::CadStatus::Ok) report(sketchStatusText(status), true);
            break;
        }
        case 1: {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::editorFitBody(id);
            break;
        }
        case 2: case 3: case 4: {
            fs::BodyCommandStatus status;
            {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                fs::ConstructionScene& scene = fs::constructionScene();
                fs::ConstructionHistory& history = fs::constructionHistory();
                status = chosen == 2   ? fs::setSceneBodyVisible(id, !visible, scene, history)
                         : chosen == 3 ? fs::setSceneBodyLocked(id, !locked, scene, history)
                                       : fs::duplicateSceneBody(id, scene, history);
                if (chosen == 4 && status == fs::BodyCommandStatus::Ok && scene.hasProject()) {
                    fs::editorSelection().setSelected(scene.activeBodyId());
                }
            }
            if (status != fs::BodyCommandStatus::Ok) {
                report(std::string("Refused: ") + fs::bodyCommandStatusName(status), true);
            } else {
                report(chosen == 2 ? (visible ? "Hidden" : "Shown")
                       : chosen == 3 ? (locked ? "Unlocked" : "Locked")
                                     : "Duplicated");
            }
            break;
        }
        case 5:
            deleteActive();
            break;
        default:
            break;
    }
}

void DesktopEditor::buildHome(int width, int height) {
    fsui::Ui& ui = *ui_;
    ui.rect({0, 0, static_cast<float>(width), static_cast<float>(height)},
            theme_.startPageSurface, 0);
    ui.block({0, 0, static_cast<float>(width), static_cast<float>(height)});
    const float w = ui.px(420), h = ui.px(360);
    const Rect card{(width - w) * 0.5f, (height - h) * 0.5f, w, h};
    ui.panel(card);
    const float x = card.x + ui.px(28);
    float y = card.y + ui.px(28);
    ui.icon("ic_forgeshape_mark", {x, y, ui.px(40), ui.px(40)}, theme_.accent);
    ui.label({x + ui.px(52), y, card.w, ui.px(40)}, "ForgeShape", theme_.textPrimary, 22, true);
    y += ui.px(56);
    ui.label({x, y, card.w - ui.px(56), ui.px(20)}, "Start a project", theme_.textSecondary,
             theme_.smallFontSize);
    y += ui.px(28);
    const float bw = card.w - ui.px(56), bh = ui.px(44);
    if (ui.button("home_new_cad", {x, y, bw, bh}, "New CAD part", "ic_sketch_new", true)) newSketch();
    y += bh + ui.px(10);
    if (ui.button("home_new_freeform", {x, y, bw, bh}, "New Freeform body", "ic_freeform")) newFreeform();
    y += bh + ui.px(10);
    if (ui.button("home_open", {x, y, bw, bh}, "Open project\xE2\x80\xA6", "ic_project")) openProject();
    y += bh + ui.px(18);
    ui.label({x, y, bw, ui.px(20)}, status_, statusError_ ? theme_.textError : theme_.textSecondary,
             theme_.smallFontSize);
}

namespace {

// Lays out one ribbon row: commands side by side, each a captioned button,
// in named groups with a hairline between them.
class RibbonRow {
public:
    RibbonRow(fsui::Ui& ui, const fsui::Theme& theme, const Rect& area)
        : ui_(ui), theme_(theme), area_(area), x_(area.x + ui.px(10)), groupStart_(x_) {}

    bool command(const std::string& id, const char* icon, const std::string& caption,
                 bool active = false, bool enabled = true, const std::string& tip = std::string(),
                 bool primary = false) {
        const float w = std::max(ui_.px(60), ui_.textWidth(caption, theme_.smallFontSize) +
                                                 ui_.px(16));
        const bool click = ui_.ribbonButton(id, {x_, area_.y, w, ui_.px(54)}, icon, caption, active,
                                            enabled, tip, primary);
        x_ += w + ui_.px(2);
        return click;
    }
    void endGroup(const std::string& name) {
        ui_.label({groupStart_, area_.y + ui_.px(54), x_ - groupStart_, ui_.px(16)}, name,
                  theme_.textSecondary, theme_.smallFontSize, false, true);
        ui_.rect({x_ + ui_.px(3), area_.y + ui_.px(6), 1.0f, area_.h - ui_.px(12)},
                 theme_.chromeHairline, 0);
        x_ += ui_.px(10);
        groupStart_ = x_;
    }
    void note(const std::string& text) {
        const float w = ui_.textWidth(text, theme_.smallFontSize) + ui_.px(16);
        ui_.label({x_, area_.y, w, ui_.px(52)}, text, theme_.textSecondary, theme_.smallFontSize);
        x_ += w;
    }

private:
    fsui::Ui& ui_;
    const fsui::Theme& theme_;
    Rect area_;
    float x_;
    float groupStart_;
};

}  // namespace

void DesktopEditor::buildTitleBar(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.rect(bar, theme_.chromeSurface, 0);
    ui.block(bar);
    const float s = ui.px(32);
    const float y = bar.y + (bar.h - s) * 0.5f;
    float x = bar.x + ui.px(10);
    ui.icon("ic_forgeshape_mark", {x + ui.px(4), y + ui.px(4), ui.px(24), ui.px(24)}, theme_.accent);
    x += ui.px(36);
    const std::string name = projectName_.empty() ? std::string("ForgeShape") : projectName_;
    const float titleW = std::min(ui.px(200), ui.textWidth(name, theme_.fontSize, true) + ui.px(8));
    ui.label({x, bar.y, titleW, bar.h}, name, theme_.textPrimary, theme_.fontSize, true);
    x += titleW + ui.px(10);
    const auto gap = [&]() {
        ui.rect({x + ui.px(4), y + ui.px(7), 1.0f, s - ui.px(14)}, theme_.chromeHairline, 0);
        x += ui.px(10);
    };
    const auto tool = [&](const char* id, const char* icon, const char* tip, bool active = false,
                          bool enabled = true) {
        const bool click = ui.iconButton(id, {x, y, s, s}, icon, tip, active, enabled);
        x += s + ui.px(2);
        return click;
    };
    bool sketching = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        sketching = fs::sketchSession().active();
    }
    // Undo and Redo have ONE home, the History strip's leading end, as its
    // back and forward arrows (`WORKBENCH-UX-R1`); Ctrl+Z and Ctrl+Y are
    // unchanged.
    if (!sketching) {
        gap();
        if (tool("tb_open", "ic_project", "Open project\xE2\x80\xA6  Ctrl+O")) openProject();
        if (tool("tb_save", "ic_save", "Save project\xE2\x80\xA6  Ctrl+S")) saveProject();
        if (tool("tb_export", "ic_export", "Export GLB\xE2\x80\xA6  Ctrl+E")) exportGlb();
    }

    float rx = bar.right() - ui.px(10) - s;
    const auto right = [&](const char* id, const char* icon, const char* tip, bool active = false) {
        const bool click = ui.iconButton(id, {rx, y, s, s}, icon, tip, active);
        rx -= s + ui.px(2);
        return click;
    };
    if (!sketching && right("tb_close", "ic_sketch_cancel", "Close project")) closeProject();
    if (right("tb_properties", "ic_precision", "Properties", propertiesOpen_))
        propertiesOpen_ = !propertiesOpen_;
    if (right("tb_browser", "ic_panel_browser", "Browser", browserOpen_)) browserOpen_ = !browserOpen_;
    if (right("tb_theme", "ic_settings", "Reload theme (theme.json)")) reloadTheme();
}

void DesktopEditor::buildRibbon(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.rect(bar, theme_.chromeSurface, 0);
    ui.separator(bar.x, bar.y, bar.w);
    ui.separator(bar.x, bar.bottom() - 1, bar.w);
    ui.block(bar);
    bool sketching = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        sketching = fs::sketchSession().active() || fs::supportChooser().active();
    }
    // While a sketch is open the Model tab gives way to the Sketch tab: its
    // acts are refused under a sketch, and a control that cannot succeed is
    // not drawn.
    struct TabEntry { int index; const char* name; };
    static const TabEntry kModelTabs[2] = {{0, "Model"}, {1, "View"}};
    static const TabEntry kSketchTabs[2] = {{2, "Sketch"}, {1, "View"}};
    const TabEntry* tabs = sketching ? kSketchTabs : kModelTabs;
    if (sketching && ribbonTab_ == 0) ribbonTab_ = 2;
    if (!sketching && ribbonTab_ == 2) ribbonTab_ = 0;
    float tx = bar.x + ui.px(10);
    for (int t = 0; t < 2; ++t) {
        const TabEntry& tab = tabs[t];
        const float tw = ui.textWidth(tab.name, theme_.fontSize, true) + ui.px(28);
        const std::string id = std::string("ribbon_tab_") + tab.name;
        if (ui.tab(id, {tx, bar.y + ui.px(4), tw, ui.px(24)}, tab.name, ribbonTab_ == tab.index)) {
            ribbonTab_ = tab.index;
        }
        tx += tw + ui.px(4);
    }
    const Rect area{bar.x, bar.y + ui.px(30), bar.w, bar.h - ui.px(30)};
    switch (ribbonTab_) {
        case 1: buildViewRibbon(area); break;
        case 2: buildSketchRibbon(area); break;
        default: buildModelRibbon(area); break;
    }
}

void DesktopEditor::buildModelRibbon(const Rect& area) {
    RibbonRow row(*ui_, theme_, area);
    int gizmoMode = -1;
    bool localSpace = false, convertible = false, deletable = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        const fs::GizmoSession& gizmo = fs::gizmoSession();
        if (gizmo.active()) gizmoMode = static_cast<int>(gizmo.mode());
        localSpace = gizmo.space() == fs::GizmoSpace::Local;
        const fs::ConstructionScene& scene = fs::constructionScene();
        if (scene.hasProject()) {
            const fs::SceneObject& body = scene.activeBody();
            const fs::ConstructionObject* construction = body.constructionOrNull();
            convertible = construction != nullptr && !body.frozenSculpt().mesh.frozen() &&
                          fs::primitiveHasCadPart(construction->spec());
            deletable = scene.bodyCount() > 1;
        }
    }
    if (row.command("rb_new_sketch", "ic_sketch_new", "New sketch", false, true,
                    "New sketch  Shift+S")) newSketch();
    row.endGroup("Sketch");
    static const fs::PrimitiveKind kinds[] = {fs::PrimitiveKind::Box, fs::PrimitiveKind::Cylinder,
                                              fs::PrimitiveKind::Sphere, fs::PrimitiveKind::Cone,
                                              fs::PrimitiveKind::Capsule, fs::PrimitiveKind::Plane};
    static const char* names[] = {"Box", "Cylinder", "Sphere", "Cone", "Capsule", "Plane"};
    for (int i = 0; i < 6; ++i) {
        if (row.command("rb_add_" + std::to_string(i), primitiveIcon(kinds[i]), names[i])) {
            addPrimitive(static_cast<int>(kinds[i]));
        }
    }
    row.endGroup("Primitives");
    // Drawn only where it can succeed: a primitive body becomes a part.
    if (convertible) {
        if (row.command("rb_cad_part", "ic_op_new_body", "CAD part", false, true,
                        "Make the primitive a CAD part")) {
            makeCadPart();
        }
        row.endGroup("Part");
    }
    if (row.command("rb_move", "ic_gizmo_move", "Move", gizmoMode == 0)) setTransformMode(0);
    if (row.command("rb_rotate", "ic_gizmo_rotate", "Rotate", gizmoMode == 1)) setTransformMode(1);
    if (row.command("rb_scale", "ic_gizmo_scale", "Scale", gizmoMode == 2)) setTransformMode(2);
    // World / Local where it applies: a world-axis scale of a turned body is a
    // shear, so Scale has no space to choose.
    if (gizmoMode == 0 || gizmoMode == 1) {
        if (row.command("rb_space", localSpace ? "ic_space_local" : "ic_space_world",
                        localSpace ? "Local" : "World", false, true,
                        localSpace ? "Local axes (switch to World)" : "World axes (switch to Local)")) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::gizmoSession().setSpace(localSpace ? fs::GizmoSpace::World : fs::GizmoSpace::Local);
        }
    }
    // The last body of a project is never deleted, so Delete is absent then.
    if (deletable && row.command("rb_delete", "ic_delete", "Delete", false, true, "Delete body  Del")) {
        deleteActive();
    }
    row.endGroup("Arrange");
}

void DesktopEditor::buildViewRibbon(const Rect& area) {
    // The standard views, Fit and the projection have ONE home, the view bar
    // standing on the model; the panels' toggles stand at the title row's end.
    RibbonRow row(*ui_, theme_, area);
    fs::DisplaySettingsStore& display = fs::displaySettings();
    const fs::ViewportDisplaySettings settings = display.snapshot();
    if (row.command("rb_grid", "ic_view_grid", "Grid", settings.gridVisible)) {
        display.setGridVisible(!settings.gridVisible);
    }
    if (row.command("rb_outline", "ic_view_outline", "Outline", settings.selectionOutlineVisible, true,
                    "Selection outline")) {
        display.setSelectionOutlineVisible(!settings.selectionOutlineVisible);
    }
    if (row.command("rb_edges", "ic_view_edges", "Edges", settings.featureEdgesVisible, true,
                    "Feature edges")) {
        display.setFeatureEdgesVisible(!settings.featureEdgesVisible);
    }
    row.endGroup("Show");
}

void DesktopEditor::buildHistoryStrip(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.dockedPanel(bar, 3);
    std::vector<fs::CadTimelineRow> rows;
    bool sketching = false, canUndo = false, canRedo = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        const fs::ConstructionScene& scene = fs::constructionScene();
        sketching = fs::sketchSession().active();
        canUndo = sketching ? fs::sketchSession().canUndoSketchEdit()
                            : fs::constructionHistory().canUndo();
        canRedo = sketching ? fs::sketchSession().canRedoSketchEdit()
                            : fs::constructionHistory().canRedo();
        if (scene.hasProject()) {
            if (const fs::CadBody* cad = scene.activeBody().cadOrNull()) {
                rows = fs::buildCadTimelineFor(cad->state()).rows;
            }
        }
    }
    const float s = ui.px(30);
    const float y = bar.y + (bar.h - s) * 0.5f;
    float x = bar.x + ui.px(12);
    const float labelW = ui.textWidth("History", theme_.fontSize, true) + ui.px(12);
    ui.label({x, bar.y, labelW, bar.h}, "History", theme_.textPrimary, theme_.fontSize, true);
    x += labelW;
    // Back and forward are Undo and Redo -- the one history, stepped one act
    // at a time (the sketch's own while a sketch is open); there is no
    // rollback marker, because a rolled-back chain would need durable
    // semantics the project does not have.
    if (ui.iconButton("hist_back", {x, y, s, s}, "ic_undo", "Back (Undo)  Ctrl+Z", false, canUndo)) undo();
    x += s + ui.px(2);
    if (ui.iconButton("hist_forward", {x, y, s, s}, "ic_redo", "Forward (Redo)  Ctrl+Y", false, canRedo)) {
        redo();
    }
    x += s + ui.px(10);
    ui.rect({x, y + ui.px(4), 1.0f, s - ui.px(8)}, theme_.chromeHairline, 0);
    x += ui.px(10);
    if (rows.empty()) {
        ui.label({x, bar.y, bar.right() - x - ui.px(12), bar.h},
                 sketching ? std::string("Sketching \xC2\xB7 Finish to add the step")
                           : std::string("The active body has no feature history"),
                 theme_.textSecondary, theme_.smallFontSize);
        return;
    }
    // The steps in construction order, newest at the right; when they do not
    // fit, the oldest scroll off the leading edge.
    std::vector<float> widths;
    float total = 0;
    for (const fs::CadTimelineRow& row : rows) {
        const float cw = ui.textWidth(timelineRowLabel(row), theme_.smallFontSize) + ui.px(38);
        widths.push_back(cw);
        total += cw + ui.px(6);
    }
    const float room = bar.right() - ui.px(10) - x;
    float cx = total > room ? x + room - total : x;
    ui.pushClip({x, bar.y, room, bar.h});
    for (size_t i = 0; i < rows.size(); ++i) {
        const fs::CadTimelineRow& row = rows[i];
        const Rect chip{cx, y, widths[i], s};
        const bool failed = row.state == fs::CadTimelineRowState::Failed;
        const bool stale = row.state == fs::CadTimelineRowState::NotRegenerated;
        // A step not rebuilt reads in secondary ink, not the disabled grey:
        // it is live content, and the chip's own glyph carries the state.
        const fsui::Color ink = failed ? theme_.textError : stale ? theme_.textSecondary : theme_.textPrimary;
        // A sketch or an extrusion step opens its staged edit (`WORKBENCH-UX-R1`):
        // the History is how a part is changed after the fact, as in every
        // parametric CAD application. A hole, a pattern and a finish have no
        // desktop editor yet, so their steps are drawn and not clickable.
        const bool editable = !sketching && (row.kind == fs::CadTimelineRowKind::Sketch ||
                                             row.kind == fs::CadTimelineRowKind::Feature);
        const std::string id = std::string("hist_") +
                               (row.kind == fs::CadTimelineRowKind::Sketch ? "sketch_" : "feature_") +
                               std::to_string(row.id);
        if (ui.stepChip(id, chip, timelineRowIcon(row), timelineRowLabel(row), row.editing, editable,
                        ink, failed ? theme_.textError : theme_.fieldBorder.alpha(0.6f),
                        editable ? std::string("Edit ") + timelineRowLabel(row) : std::string())) {
            editHistoryRow(row);
        }
        cx += widths[i] + ui.px(6);
    }
    ui.popClip();
}

void DesktopEditor::buildViewCube(const Rect& vp) {
    if (homeVisible_ || vp.w < ui_->px(200) || vp.h < ui_->px(160)) return;
    fsui::Ui& ui = *ui_;
    fs::CameraSnapshot camera;
    bool sketchView = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        camera = fs::editorCamera().snapshot();
        sketchView = fs::editorCamera().sketchViewActive();
    }
    const float radius = ui.px(40);
    const float cx = vp.right() - ui.px(18) - radius;
    const float cy = vp.y + ui.px(18) + radius;
    const Rect box{cx - radius - ui.px(8), cy - radius - ui.px(8), 2 * radius + ui.px(16),
                   2 * radius + ui.px(16)};
    ui.circle(cx, cy, radius + ui.px(8), theme_.surfaceFloating.alpha(0.55f));
    ui.block(box);
    // The view matrix's rows are screen right, screen up and toward the eye.
    const float right[3] = {camera.view.m[0], camera.view.m[4], camera.view.m[8]};
    const float up[3] = {camera.view.m[1], camera.view.m[5], camera.view.m[9]};
    const float toward[3] = {camera.view.m[2], camera.view.m[6], camera.view.m[10]};
    struct End { int axis; bool positive; float depth; float sx, sy; };
    std::vector<End> ends;
    for (int a = 0; a < 3; ++a) {
        for (int sign = 0; sign < 2; ++sign) {
            const float k = sign == 0 ? 1.0f : -1.0f;
            ends.push_back({a, sign == 0, k * toward[a], cx + k * right[a] * radius * 0.78f,
                            cy - k * up[a] * radius * 0.78f});
        }
    }
    std::sort(ends.begin(), ends.end(), [](const End& l, const End& r) { return l.depth < r.depth; });
    static const fsui::Color colours[3] = {fsui::Color::hex(0xFFE5534B), fsui::Color::hex(0xFF57AB5A),
                                           fsui::Color::hex(0xFF539BF5)};
    static const char* letters[3] = {"X", "Y", "Z"};
    // Each end is a view: +X Right, +Y Top, +Z Front, and their opposites.
    static const fs::StandardView views[3][2] = {
        {fs::StandardView::Right, fs::StandardView::Left},
        {fs::StandardView::Top, fs::StandardView::Bottom},
        {fs::StandardView::Front, fs::StandardView::Back}};
    for (const End& end : ends) {
        const fsui::Color colour = colours[end.axis];
        const float r = end.positive ? ui.px(10) : ui.px(7);
        if (end.positive) ui.line(cx, cy, end.sx, end.sy, ui.px(2.5f), colour);
        const std::string id = std::string("cube_") + (end.positive ? "+" : "-") +
                               static_cast<char>('x' + end.axis);
        // In a sketch view the cube only reports orientation: the sketch owns
        // the camera, so its ends are not controls there.
        bool hot = false;
        if (!sketchView) {
            const Rect target{end.sx - ui.px(12), end.sy - ui.px(12), ui.px(24), ui.px(24)};
            if (ui.iconButton(id, target, "", std::string(end.positive ? "" : "-") + letters[end.axis] +
                                                  " \xC2\xB7 " + fs::standardViewName(views[end.axis][end.positive ? 0 : 1]) + " view")) {
                setStandardView(static_cast<int>(views[end.axis][end.positive ? 0 : 1]));
            }
            hot = target.contains(input_.mouseX, input_.mouseY);
        }
        if (end.positive) {
            ui.circle(end.sx, end.sy, r + (hot ? ui.px(2) : 0.0f), colour);
            ui.label({end.sx - r, end.sy - r, 2 * r, 2 * r}, letters[end.axis],
                     fsui::Color::hex(0xFF12110F), theme_.smallFontSize - 1.0f, true, true);
        } else {
            ui.circle(end.sx, end.sy, r + (hot ? ui.px(2) : 0.0f), colour.alpha(0.45f));
        }
    }
}

void DesktopEditor::buildViewBar(const Rect& vp) {
    if (homeVisible_) return;
    bool sketchView = false, ortho = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        sketchView = fs::editorCamera().sketchViewActive();
        ortho = fs::editorCamera().projectionMode() == fs::ProjectionMode::Orthographic;
    }
    if (sketchView) return;  // the sketch's exact view owns the camera
    fsui::Ui& ui = *ui_;
    const float s = ui.px(32);
    const int count = 6;
    const float w = count * (s + ui.px(2)) + ui.px(10) + ui.px(10);
    const Rect bar{vp.x + (vp.w - w) * 0.5f, vp.bottom() - s - ui.px(18), w, s + ui.px(8)};
    if (bar.x < vp.x) return;
    ui.shadow(bar, theme_.shadow, ui.px(theme_.panelRadius), ui.px(10));
    ui.rect(bar, theme_.surfaceFloating, ui.px(theme_.panelRadius));
    ui.block(bar);
    float x = bar.x + ui.px(6);
    const float y = bar.y + ui.px(4);
    const auto tool = [&](const char* id, const char* icon, const char* tip, bool active = false) {
        const bool click = ui.iconButton(id, {x, y, s, s}, icon, tip, active);
        x += s + ui.px(2);
        return click;
    };
    if (tool("vb_fit", "ic_view_fit", "Fit everything  F")) fitAll();
    if (tool("vb_home", "ic_view_home", "Home view  Ctrl+1")) setStandardView(0);
    if (tool("vb_front", "ic_view_front", "Front view  Ctrl+2")) setStandardView(static_cast<int>(fs::StandardView::Front));
    if (tool("vb_top", "ic_view_top", "Top view  Ctrl+6")) setStandardView(static_cast<int>(fs::StandardView::Top));
    if (tool("vb_right", "ic_view_right", "Right view  Ctrl+5")) setStandardView(static_cast<int>(fs::StandardView::Right));
    ui.rect({x + ui.px(3), y + ui.px(6), 1.0f, s - ui.px(12)}, theme_.chromeHairline, 0);
    x += ui.px(8);
    if (tool("vb_projection", ortho ? "ic_projection_orthographic" : "ic_projection_perspective",
             ortho ? "Orthographic (switch to Perspective)" : "Perspective (switch to Orthographic)")) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::editorCamera().setProjectionMode(ortho ? fs::ProjectionMode::Perspective
                                                   : fs::ProjectionMode::Orthographic);
    }
}

void DesktopEditor::buildBrowser(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.dockedPanel(panel, 1);
    const float inner = ui.px(theme_.panelInset);
    float y = panel.y + inner;
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    ui.label({x, y, w, ui.px(22)}, "Objects", theme_.textPrimary, theme_.titleFontSize, true);
    y += ui.px(28);
    const float rowH = ui.px(theme_.rowHeight);
    std::vector<std::pair<fs::ObjectId, fs::BodyRepresentation>> bodies;
    fs::ObjectId active = fs::kNoObject;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        const fs::ConstructionScene& scene = fs::constructionScene();
        for (size_t i = 0; i < scene.bodyCount(); ++i) {
            bodies.push_back({scene.bodyAt(i).objectId(), scene.bodyAt(i).representation()});
        }
        if (scene.hasProject()) active = scene.activeBodyId();
    }
    ui.pushClip({panel.x, y, panel.w, panel.bottom() - inner - y});
    for (const auto& body : bodies) {
        const std::string id = "obj_" + std::to_string(body.first);
        if (ui.listRow(id, {x, y, w, rowH}, bodyLabel(body.first), representationIcon(body.second),
                       body.first == active, representationLabel(body.second))) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            // Body switching is refused while a sketch is open, as on Android.
            if (!fs::sketchSession().active()) {
                fs::constructionScene().setActiveBody(body.first);
                fs::editorSelection().setSelected(body.first);
            }
        }
        y += rowH + ui.px(2);
    }
    ui.popClip();
}

void DesktopEditor::buildProperties(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.dockedPanel(panel, 2);
    const float inner = ui.px(theme_.panelInset);
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    float y = panel.y + inner;

    {
        bool sketching = false, choosing = false;
        {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            sketching = fs::sketchSession().active();
            choosing = fs::supportChooser().active();
        }
        if (sketching) {
            buildSketchProperties(panel);
            return;
        }
        if (choosing) {
            buildChooserProperties(panel);
            return;
        }
    }
    fs::ObjectId active = fs::kNoObject;
    fs::BodyRepresentation representation = fs::BodyRepresentation::Construction;
    fs::TransformValues t;
    bool derived = false;
    bool hasPrimitive = false;
    fs::PrimitiveSpec spec = fs::PrimitiveSpec::forBox(1, 1, 1);
    fs::MassProperties mass;
    bool measured = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::ConstructionScene& scene = fs::constructionScene();
        if (!scene.hasProject()) return;
        const fs::SceneObject& body = scene.activeBody();
        active = body.objectId();
        representation = body.representation();
        t = body.transform().values();
        derived = body.isFaceSupportedCad();
        if (const fs::ConstructionObject* source = body.constructionOrNull()) {
            hasPrimitive = true;
            spec = source->spec();
        }
        for (const fs::SceneDrawItem& item : fs::viewSceneSnapshot()) {
            if (item.objectId == active && item.mesh) {
                measured = fs::computeMassProperties(item.mesh->vertices(), item.mesh->vertexCount(),
                                                     item.mesh->indices(), item.mesh->indexCount(),
                                                     item.model, &mass);
                break;
            }
        }
    }

    ui.label({x, y, w, ui.px(24)}, bodyLabel(active), theme_.textPrimary, theme_.panelTitleFontSize, true);
    y += ui.px(24);
    ui.label({x, y, w, ui.px(18)}, representationLabel(representation), theme_.textSecondary,
             theme_.smallFontSize);
    y += ui.px(28);

    const float labelW = ui.px(70);
    const float fieldH = ui.px(theme_.controlHeight);
    // Three fields and a narrow unit column, so the third field is as wide as
    // the first two.
    const float unitCol = ui.px(16);
    const float fieldW = (w - labelW - unitCol) / 3.0f - ui.px(4);
    const auto section = [&](const char* title) {
        ui.separator(x, y, w);
        y += ui.px(10);
        // A heading in sentence case, primary ink: it names the section, and
        // an uppercase grey caption read as one more piece of chrome.
        ui.label({x, y, w, ui.px(20)}, title, theme_.textPrimary, theme_.titleFontSize, true);
        y += ui.px(26);
    };
    // Placement: the authored transform, typed. A face-supported part stands
    // where its producer's face puts it, so it has no placement of its own.
    section("Placement");
    if (derived) {
        ui.label({x, y, w, ui.px(20)}, "Placed by the face it stands on", theme_.textSecondary,
                 theme_.smallFontSize);
        y += ui.px(28);
    } else {
        double* rows[3][3] = {{&t.positionX, &t.positionY, &t.positionZ},
                              {&t.rotationX, &t.rotationY, &t.rotationZ},
                              {&t.scaleX, &t.scaleY, &t.scaleZ}};
        const char* rowNames[3] = {"Position", "Rotation", "Scale"};
        const char* units[3] = {"m", "\xC2\xB0", ""};
        bool changed = false;
        for (int r = 0; r < 3; ++r) {
            ui.label({x, y, labelW, fieldH}, rowNames[r], theme_.textPrimary, theme_.smallFontSize);
            for (int c = 0; c < 3; ++c) {
                const std::string id = "tf_" + std::to_string(active) + "_" + std::to_string(r * 3 + c);
                const Rect field{x + labelW + c * (fieldW + ui.px(4)), y, fieldW, fieldH};
                if (ui.numberField(id, field, rows[r][c], 3, "")) {
                    changed = true;
                }
            }
            ui.label({x + w - unitCol + ui.px(2), y, unitCol, fieldH}, units[r], theme_.textSecondary,
                     theme_.smallFontSize);
            y += fieldH + ui.px(6);
        }
        if (changed) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::BodyPlacementRefusal refusal = fs::BodyPlacementRefusal::None;
            const fs::TransformApplyResult result = fs::applySceneBodyTransform(
                active, t, fs::constructionScene(), fs::constructionHistory(), &refusal);
            if (refusal == fs::BodyPlacementRefusal::Locked) {
                report("The body is locked", true);
            } else if (result.status == fs::TransformUpdateStatus::Rejected) {
                report("That placement was refused", true);
            }
        }
    }
    // Shape: a Construction primitive's own parameters.
    if (hasPrimitive) {
        section("Shape");
        const auto field = [&](const char* name, double* value, const char* id) {
            ui.label({x, y, labelW, fieldH}, name, theme_.textPrimary, theme_.smallFontSize);
            const bool committed = ui.numberField(std::string(id) + std::to_string(active),
                                                  {x + labelW, y, w - labelW, fieldH}, value, 3, "m");
            y += fieldH + ui.px(6);
            return committed;
        };
        fs::PrimitiveSpec next = spec;
        bool changed = false;
        if (const fs::BoxDimensionsMeters* b = spec.box()) {
            double a = b->width, h = b->height, d = b->depth;
            changed |= field("Width", &a, "pw");
            changed |= field("Height", &h, "ph");
            changed |= field("Depth", &d, "pd");
            next = fs::PrimitiveSpec::forBox(a, h, d);
        } else if (const fs::CylinderDimensionsMeters* c = spec.cylinder()) {
            double d = c->diameter, h = c->height;
            changed |= field("Diameter", &d, "pcd");
            changed |= field("Height", &h, "pch");
            next = fs::PrimitiveSpec::forCylinder(d, h);
        } else if (const fs::SphereDimensionsMeters* s = spec.sphere()) {
            double d = s->diameter;
            changed |= field("Diameter", &d, "psd");
            next = fs::PrimitiveSpec::forSphere(d);
        } else if (const fs::ConeDimensionsMeters* c = spec.cone()) {
            double d = c->bottomDiameter, h = c->height;
            changed |= field("Base dia.", &d, "pkd");
            changed |= field("Height", &h, "pkh");
            next = fs::PrimitiveSpec::forCone(d, h);
        } else if (const fs::CapsuleDimensionsMeters* c = spec.capsule()) {
            double d = c->diameter, h = c->totalHeight;
            changed |= field("Diameter", &d, "pad");
            changed |= field("Height", &h, "pah");
            next = fs::PrimitiveSpec::forCapsule(d, h);
        } else if (const fs::PlaneDimensionsMeters* p = spec.plane()) {
            double a = p->width, d = p->depth;
            changed |= field("Width", &a, "plw");
            changed |= field("Depth", &d, "pld");
            next = fs::PrimitiveSpec::forPlane(a, d);
        }
        if (changed) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::ScopedConstructionEdit edit(fs::constructionHistory());
            const fs::PrimitiveApplyResult result = fs::applyConstructionPrimitive(next);
            if (result.status == fs::PrimitiveUpdateStatus::Rejected) {
                report("Those dimensions were refused", true);
            }
        }
        if (ui.button("make_cad_part", {x, y, w, fieldH}, "Make CAD part", "ic_op_new_body")) {
            makeCadPart();
        }
        y += fieldH + ui.px(10);
    }
    // Features (`WORKBENCH-UX-R1`): a CAD part's chain, each step the door to
    // its staged edit -- the same rows and acts the History strip carries.
    if (representation == fs::BodyRepresentation::Cad) {
        std::vector<fs::CadTimelineRow> rows;
        {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            const fs::SceneObject* object = fs::constructionScene().findBody(active);
            if (const fs::CadBody* cad = object != nullptr ? object->cadOrNull() : nullptr) {
                rows = fs::buildCadTimelineFor(cad->state()).rows;
            }
        }
        if (!rows.empty()) {
            section("Features");
            const float rowH = ui.px(theme_.rowHeight);
            for (const fs::CadTimelineRow& row : rows) {
                const bool editable = row.kind == fs::CadTimelineRowKind::Sketch ||
                                      row.kind == fs::CadTimelineRowKind::Feature;
                const bool failed = row.state == fs::CadTimelineRowState::Failed;
                const std::string id = std::string("feat_") +
                                       (row.kind == fs::CadTimelineRowKind::Sketch ? "sketch_" : "feature_") +
                                       std::to_string(row.id);
                if (ui.stepChip(id, {x, y, w, rowH}, timelineRowIcon(row), timelineRowLabel(row),
                                false, editable, failed ? theme_.textError : theme_.textPrimary,
                                failed ? theme_.textError : theme_.chromeHairline,
                                editable ? std::string("Edit ") + timelineRowLabel(row)
                                         : std::string())) {
                    editHistoryRow(row);
                }
                y += rowH + ui.px(4);
            }
            y += ui.px(6);
        }
    }
    // Measure: read off the mesh as drawn, never stored.
    section("Measure");
    if (measured) {
        const auto line = [&](const char* name, const std::string& value) {
            ui.label({x, y, labelW + ui.px(20), ui.px(22)}, name, theme_.textSecondary, theme_.smallFontSize);
            ui.label({x + labelW + ui.px(20), y, w - labelW - ui.px(20), ui.px(22)}, value,
                     theme_.textPrimary, theme_.fontSize);
            y += ui.px(22);
        };
        const fs::DVec3 size{mass.boundsMax.x - mass.boundsMin.x, mass.boundsMax.y - mass.boundsMin.y,
                             mass.boundsMax.z - mass.boundsMin.z};
        line("Size", formatMeasure(size.x) + " \xC3\x97 " + formatMeasure(size.y) + " \xC3\x97 " +
                         formatMeasure(size.z) + " m");
        line("Area", formatMeasure(mass.area) + " m\xC2\xB2");
        line("Volume", mass.closed ? formatMeasure(mass.volume) + " m\xC2\xB3" : std::string("open surface"));
        if (mass.closed) {
            line("Centre", formatMeasure(mass.centroid.x) + ", " + formatMeasure(mass.centroid.y) +
                               ", " + formatMeasure(mass.centroid.z) + " m");
        }
    }
}

void DesktopEditor::buildStatusBar(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.rect(bar, theme_.chromeSurface, 0);
    ui.separator(bar.x, bar.y, bar.w);
    ui.block(bar);
    ui.label({bar.x + ui.px(12), bar.y, bar.w * 0.6f, bar.h}, status_,
             statusError_ ? theme_.textError : theme_.textSecondary, theme_.smallFontSize);
    fs::SketchSessionState sketchState = fs::SketchSessionState::Inactive;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        sketchState = fs::sketchSession().state();
    }
    const std::string hint =
        sketchState == fs::SketchSessionState::Editing
            ? "Left: draw \xC2\xB7 Middle drag: pan \xC2\xB7 Wheel: zoom \xC2\xB7 Enter: Finish sketch \xC2\xB7 Esc: end a run"
        : sketchState == fs::SketchSessionState::Ready
            ? "Click: choose a region \xC2\xB7 Drag the arrow: distance \xC2\xB7 Drag: orbit \xC2\xB7 Enter: Extrude"
            : "Left drag: orbit \xC2\xB7 Middle or Shift+drag: pan \xC2\xB7 Wheel: zoom \xC2\xB7 Click: select";
    const float hw = ui.textWidth(hint, theme_.smallFontSize);
    ui.label({bar.right() - hw - ui.px(12), bar.y, hw, bar.h}, hint, theme_.textSecondary,
             theme_.smallFontSize);
}

// ---------------------------------------------------------------------------
// The sketch's chrome (`DESKTOP-R2`)
// ---------------------------------------------------------------------------

namespace {

// Splits a hint into lines that fit `width` at the small size.
std::vector<std::string> wrapText(fsui::Ui& ui, const std::string& text, float width, float size) {
    std::vector<std::string> lines;
    std::string line, word;
    const auto flushWord = [&]() {
        if (word.empty()) return;
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && ui.textWidth(candidate, size) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    };
    for (char c : text) {
        if (c == ' ') {
            flushWord();
        } else {
            word.push_back(c);
        }
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}  // namespace

bool DesktopEditor::routeSketchInput(int width, int height, bool overUi) {
    bool sketching = false, choosing = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        sketching = fs::sketchSession().active();
        choosing = fs::supportChooser().active();
    }
    if (!sketching && !choosing) {
        sketchPointer_ = false;
        return false;
    }
    fs::CameraController& camera = fs::editorCamera();
    if (!overUi && input_.wheel != 0.0f) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        camera.zoomAtPixel(input_.mouseX, input_.mouseY, input_.wheel);
    }
    // Navigation: the right button orbits and the middle (or Shift) pans, as
    // everywhere; the camera itself refuses an orbit while the exact sketch
    // view stands. In the chooser the left button navigates too, and a click
    // that did not travel chooses the target under it.
    const int firstNavButton = choosing ? 0 : 1;
    for (int b = firstNavButton; b < 3; ++b) {
        if (input_.mousePressed[b] && !overUi && dragButton_ < 0) {
            dragButton_ = b;
            dragLastX_ = pressX_ = input_.mouseX;
            dragLastY_ = pressY_ = input_.mouseY;
            dragTravelled_ = false;
        }
    }
    if (dragButton_ >= 0 && dragButton_ < firstNavButton) dragButton_ = -1;
    if (dragButton_ >= 0) {
        const float dx = input_.mouseX - dragLastX_;
        const float dy = input_.mouseY - dragLastY_;
        if (std::fabs(input_.mouseX - pressX_) + std::fabs(input_.mouseY - pressY_) > 4.0f * scale_) {
            dragTravelled_ = true;
        }
        if ((dx != 0.0f || dy != 0.0f) && dragTravelled_) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            if (dragButton_ == 2 || input_.shift) {
                camera.panByPixels(dx, dy);
            } else {
                camera.orbitByPixels(dx, dy);
            }
        }
        dragLastX_ = input_.mouseX;
        dragLastY_ = input_.mouseY;
        if (input_.mouseReleased[dragButton_]) {
            if (choosing && dragButton_ == 0 && !dragTravelled_) {
                std::unique_lock<std::mutex> lock(fs::editorStateMutex());
                const fs::ChosenSupport chosen = fs::supportChooser().select(
                    camera.snapshot(), input_.mouseX, input_.mouseY, width, height,
                    fs::constructionScene());
                if (chosen.kind != fs::ChosenSupport::Kind::None) {
                    // A pointer hovers before it clicks, so one click is the
                    // whole choice: the chooser's own confirm.
                    const fs::CadStatus status = fs::editorConfirmChosenSupport();
                    lock.unlock();
                    if (status == fs::CadStatus::Ok) {
                        report(chosen.kind == fs::ChosenSupport::Kind::Face ? "Sketch on the face"
                                                                            : "Sketch on the plane");
                    } else {
                        report("Could not sketch there: " + sketchStatusText(status), true);
                    }
                }
            }
            dragButton_ = -1;
        }
    }
    if (choosing && !overUi && dragButton_ < 0) {
        // A mouse hovers: the target under it is highlighted, never chosen.
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (fs::supportChooser().active()) {
            fs::supportChooser().hover(camera.snapshot(), input_.mouseX, input_.mouseY, width,
                                       height, fs::constructionScene());
        }
    }
    if (!sketching) return true;
    // The left button IS one pointer: Down, Move, Up, through the core's one
    // sketch touch path -- the drawing, the arrow drag and a region tap alike.
    const auto send = [&](fs::TouchAction action) {
        fs::TouchPointer pointer{0, input_.mouseX, input_.mouseY, fs::PointerToolType::Mouse};
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::sketchSession().active()) return;
        fs::editorSketchTouch(action, 0, &pointer, 1, width, height);
    };
    if (input_.mousePressed[0] && !overUi && !sketchPointer_) {
        sketchPointer_ = true;
        sketchLastX_ = input_.mouseX;
        sketchLastY_ = input_.mouseY;
        send(fs::TouchAction::Down);
    }
    if (sketchPointer_) {
        if (input_.mouseX != sketchLastX_ || input_.mouseY != sketchLastY_) {
            send(fs::TouchAction::Move);
            sketchLastX_ = input_.mouseX;
            sketchLastY_ = input_.mouseY;
        }
        if (input_.mouseReleased[0] || !input_.mouseDown[0]) {
            send(fs::TouchAction::Up);
            sketchPointer_ = false;
        }
    }
    return true;
}

void DesktopEditor::sketchKeys() {
    if (ui_ && ui_->typing()) return;
    // F frames everything, as in most CAD applications; while a sketch is
    // drawn its tools answer to their letters, and Shift+S begins a sketch.
    fs::SketchSessionState letterState = fs::SketchSessionState::Inactive;
    bool letterChoosing = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        letterState = fs::sketchSession().state();
        letterChoosing = fs::supportChooser().active();
    }
    if (!input_.ctrl) {
        for (char32_t c : input_.text) {
            if (c == U'f' || c == U'F') {
                fitAll();
                continue;
            }
            if (c == U'S' && input_.shift && letterState == fs::SketchSessionState::Inactive &&
                !letterChoosing && !homeVisible_) {
                newSketch();
                continue;
            }
            if (letterState != fs::SketchSessionState::Editing || c > 0x7F) continue;
            const char upper = static_cast<char>(c >= U'a' && c <= U'z' ? c - 32 : c);
            for (const SketchToolEntry& entry : kSketchTools) {
                if (entry.key == upper) setSketchTool(fs::sketchToolIndex(entry.tool));
            }
        }
    }
    for (fsui::Key key : input_.keys) {
        fs::SketchSessionState state = fs::SketchSessionState::Inactive;
        bool choosing = false;
        {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            state = fs::sketchSession().state();
            choosing = fs::supportChooser().active();
        }
        if (key == fsui::Key::Escape) {
            if (choosing) {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                fs::supportChooser().cancel();
            } else if (state == fs::SketchSessionState::Editing) {
                // One step, as Back is: a run being placed ends first, then
                // the tool is put down.
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                fs::SketchSession& sketch = fs::sketchSession();
                if (sketch.polylineInProgress()) {
                    sketch.endPolylineInProgress();
                } else if (sketch.tool() != fs::SketchTool::Select) {
                    sketch.setTool(fs::SketchTool::Select);
                }
            } else if (state == fs::SketchSessionState::Ready) {
                backToSketch();
            }
        } else if (key == fsui::Key::Enter) {
            if (state == fs::SketchSessionState::Editing) {
                finishSketch();
            } else if (state == fs::SketchSessionState::Ready) {
                commitSketch();
            }
        }
    }
}

void DesktopEditor::buildSketchRibbon(const Rect& area) {
    RibbonRow row(*ui_, theme_, area);
    fs::SketchSessionState state = fs::SketchSessionState::Inactive;
    fs::SketchTool current = fs::SketchTool::Select;
    bool canExtrude = false, oneSide = false, selected = false;
    bool choosing = false;
    size_t entities = 0;
    fs::CadFeatureOperation operation = fs::CadFeatureOperation::NewBody;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::SketchSession& sketch = fs::sketchSession();
        choosing = fs::supportChooser().active();
        state = sketch.state();
        current = sketch.tool();
        entities = sketch.sketch().entities.size();
        selected = !sketch.selectedEntityIds().empty();
        oneSide = sketch.extrude().extent == fs::ExtrudeExtentMode::OneSide;
        operation = sketch.operation();
        if (state == fs::SketchSessionState::Ready && sketch.selectionChosen()) {
            canExtrude = sketch.evaluateCandidate().valid;
        }
    }
    if (choosing && state == fs::SketchSessionState::Inactive) {
        row.note("Click a plane or a flat face of a part");
        if (row.command("rb_chooser_cancel", "ic_sketch_cancel", "Cancel")) cancelSketch();
        row.endGroup("New sketch");
        return;
    }
    if (state == fs::SketchSessionState::Editing) {
        const auto toolCommand = [&](size_t index, const char* group) {
            (void)group;
            const SketchToolEntry& entry = kSketchTools[index];
            if (row.command("sk_tool_" + std::to_string(index), entry.icon, entry.name,
                            entry.tool == current, true,
                            std::string(entry.name) + "  " + entry.key + " \xC2\xB7 " + entry.hint)) {
                setSketchTool(fs::sketchToolIndex(entry.tool));
            }
        };
        for (size_t i = 1; i <= 6; ++i) toolCommand(i, "Draw");  // Line .. Spline
        row.endGroup("Draw");
        toolCommand(0, "Modify");  // Select
        toolCommand(7, "Modify");  // Trim
        if (selected && row.command("sk_delete", "ic_delete", "Delete", false, true,
                                    "Delete the selected curve  Del")) {
            deleteActive();
        }
        row.endGroup("Modify");
        // The sketch's Undo / Redo are the History strip's arrows. Finish is
        // the screen's ONE primary, drawn where native would accept it: a
        // sketch with nothing in it has nothing to finish.
        if (entities > 0 && row.command("sk_finish", "ic_sketch_keep", "Finish", false, true,
                                        "Finish sketch  Enter", true)) {
            finishSketch();
        }
        if (row.command("sk_cancel", "ic_sketch_cancel", "Cancel", false, true, "Cancel sketch")) {
            cancelSketch();
        }
        row.endGroup("Sketch");
    } else if (state == fs::SketchSessionState::Ready) {
        // The operation's own glyph: what this Extrude will do to the part.
        // Withdrawn while the candidate would be refused: Properties names why.
        if (canExtrude && row.command("sk_extrude", operationIcon(operation), "Extrude", false, true,
                                      std::string("Extrude \xC2\xB7 ") + operationLabel(operation) +
                                          "  Enter",
                                      true)) {
            commitSketch();
        }
        if (oneSide && row.command("sk_flip", "ic_extrude_flip", "Flip", false, true,
                                   "Flip the direction")) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::sketchSession().flipExtrudeDirection();
        }
        row.endGroup("Extrude");
        if (row.command("sk_back", "ic_sketch_back", "Back", false, true, "Back to sketch  Esc")) {
            backToSketch();
        }
        if (row.command("sk_cancel", "ic_sketch_cancel", "Cancel", false, true, "Cancel sketch")) {
            cancelSketch();
        }
        row.endGroup("Sketch");
    }
}

void DesktopEditor::buildChooserProperties(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.dockedPanel(panel, 2);
    const float inner = ui.px(theme_.panelInset);
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    float y = panel.y + inner;
    ui.label({x, y, w, ui.px(24)}, "New sketch", theme_.textPrimary, theme_.panelTitleFontSize, true);
    y += ui.px(30);
    for (const std::string& line :
         wrapText(ui, "Click a plane or a flat face of a part in the view. Esc cancels.", w,
                  theme_.smallFontSize)) {
        ui.label({x, y, w, ui.px(18)}, line, theme_.textSecondary, theme_.smallFontSize);
        y += ui.px(18);
    }
    y += ui.px(12);
    ui.separator(x, y, w);
    y += ui.px(10);
    ui.label({x, y, w, ui.px(20)}, "Planes", theme_.textPrimary, theme_.titleFontSize, true);
    y += ui.px(24);
    const float h = ui.px(theme_.controlHeight);
    static const char* names[3] = {"XY plane", "XZ plane", "YZ plane"};
    for (int i = 0; i < 3; ++i) {
        const std::string id = std::string("chooser_plane_") + std::to_string(i);
        if (ui.button(id, {x, y, w, h}, names[i], "ic_sketch_new")) sketchOnPlane(i);
        y += h + ui.px(6);
    }
    y += ui.px(6);
    if (ui.button("chooser_cancel", {x, y, w, h}, "Cancel", "ic_sketch_cancel")) cancelSketch();
}

void DesktopEditor::buildSketchProperties(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.dockedPanel(panel, 2);
    const float inner = ui.px(theme_.panelInset);
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    float y = panel.y + inner;
    const float h = ui.px(theme_.controlHeight);
    const float labelW = ui.px(84);

    // One consistent read of the session.
    fs::SketchSessionState state;
    fs::SketchTool current;
    fs::Workplane plane;
    bool onFace = false, inRun = false, chosen = false;
    size_t entities = 0, areas = 0;
    fs::ExtrudeFeature extrude, shown;
    fs::CadFeatureOperation operation;
    bool operations[4] = {false, false, false, false};
    bool throughAll = false, toFace = false;
    bool candidateValid = false;
    fs::CadStatus candidateStatus = fs::CadStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::SketchSession& sketch = fs::sketchSession();
        state = sketch.state();
        current = sketch.tool();
        plane = sketch.plane();
        onFace = sketch.operationAvailable(fs::CadFeatureOperation::Cut);
        inRun = sketch.polylineInProgress();
        entities = sketch.sketch().entities.size();
        chosen = sketch.selectionChosen();
        areas = sketch.selectedAreaCount();
        extrude = sketch.extrude();
        shown = sketch.displayExtrude();
        operation = sketch.operation();
        static const fs::CadFeatureOperation all[4] = {
            fs::CadFeatureOperation::NewBody, fs::CadFeatureOperation::Add,
            fs::CadFeatureOperation::Cut, fs::CadFeatureOperation::Intersect};
        for (int i = 0; i < 4; ++i) operations[i] = sketch.operationAvailable(all[i]);
        throughAll = sketch.throughAllAvailable();
        toFace = sketch.toFaceAvailable();
        if (state == fs::SketchSessionState::Ready && chosen) {
            const fs::CadCandidateEvaluation& evaluation = sketch.evaluateCandidate();
            candidateValid = evaluation.valid;
            candidateStatus = evaluation.status;
        }
    }
    const auto section = [&](const char* title) {
        ui.separator(x, y, w);
        y += ui.px(10);
        // A heading in sentence case, primary ink: it names the section, and
        // an uppercase grey caption read as one more piece of chrome.
        ui.label({x, y, w, ui.px(20)}, title, theme_.textPrimary, theme_.titleFontSize, true);
        y += ui.px(26);
    };
    const auto text = [&](const std::string& value, fsui::Color colour) {
        for (const std::string& line : wrapText(ui, value, w, theme_.smallFontSize)) {
            ui.label({x, y, w, ui.px(18)}, line, colour, theme_.smallFontSize);
            y += ui.px(18);
        }
    };

    if (state == fs::SketchSessionState::Editing) {
        ui.label({x, y, w, ui.px(24)}, "Sketch", theme_.textPrimary, theme_.panelTitleFontSize, true);
        y += ui.px(24);
        ui.label({x, y, w, ui.px(18)},
                 onFace ? std::string("On a face of the part")
                        : std::string("On the ") + fs::workplaneName(plane) + " plane",
                 theme_.textSecondary, theme_.smallFontSize);
        y += ui.px(28);
        section("Tool");
        for (const SketchToolEntry& entry : kSketchTools) {
            if (entry.tool != current) continue;
            ui.label({x, y, w, ui.px(20)}, entry.name, theme_.textPrimary, theme_.fontSize, true);
            y += ui.px(22);
            text(entry.hint, theme_.textSecondary);
        }
        if (inRun) text("Placing a run \xC2\xB7 Esc ends it", theme_.textSecondary);
        y += ui.px(8);
        ui.label({x, y, w, ui.px(18)},
                 std::to_string(entities) + (entities == 1 ? " curve" : " curves"),
                 theme_.textSecondary, theme_.smallFontSize);
        // Finish and Cancel have one home, the ribbon's Sketch group.
        return;
    }

    // Ready: the extrusion, its extent, its distances and its operation.
    ui.label({x, y, w, ui.px(24)}, "Extrude", theme_.textPrimary, theme_.panelTitleFontSize, true);
    y += ui.px(26);
    text(chosen ? std::to_string(areas) + (areas == 1 ? " region chosen" : " regions chosen") +
                      " \xC2\xB7 click to add or remove"
                : std::string("Click a region in the view to choose it"),
         theme_.textSecondary);
    y += ui.px(8);

    section("Extent");
    {
        std::vector<fs::ExtrudeExtentMode> modes = {fs::ExtrudeExtentMode::OneSide,
                                                    fs::ExtrudeExtentMode::Symmetric,
                                                    fs::ExtrudeExtentMode::TwoSides};
        if (throughAll) modes.push_back(fs::ExtrudeExtentMode::ThroughAll);
        if (toFace) modes.push_back(fs::ExtrudeExtentMode::ToFace);
        const float s = ui.px(36);
        float bx = x;
        for (fs::ExtrudeExtentMode mode : modes) {
            const std::string id = "ext_mode_" + std::to_string(static_cast<int>(mode));
            if (ui.iconButton(id, {bx, y, s, s}, extentIcon(mode), extentLabel(mode),
                              extrude.extent == mode)) {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                const fs::CadStatus status = fs::sketchSession().setExtrudeExtent(mode);
                if (status != fs::CadStatus::Ok) report(sketchStatusText(status), true);
            }
            bx += s + ui.px(4);
        }
        y += s + ui.px(4);
        ui.label({x, y, w, ui.px(18)}, extentLabel(extrude.extent), theme_.textPrimary,
                 theme_.smallFontSize);
        y += ui.px(26);
    }
    const auto distance = [&](const char* name, const char* id, double value, bool positiveSide) {
        ui.label({x, y, labelW, h}, name, theme_.textPrimary, theme_.smallFontSize);
        double typed = value;
        if (ui.numberField(id, {x + labelW, y, w - labelW, h}, &typed, 3, "m")) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::SketchSession& sketch = fs::sketchSession();
            // A typed value is a statement the user made: refused, never
            // clamped, through the one door a drag also lands through.
            const fs::CadStatus status =
                extrude.extent == fs::ExtrudeExtentMode::OneSide
                    ? sketch.setExtrude(typed, sketch.extrude().direction)
                    : sketch.setExtrudeSide(positiveSide, typed);
            if (status != fs::CadStatus::Ok) report(sketchStatusText(status), true);
        }
        y += h + ui.px(6);
    };
    switch (extrude.extent) {
        case fs::ExtrudeExtentMode::OneSide:
            distance("Distance", "ext_distance", fs::extrudePositiveDistance(shown) +
                                                   fs::extrudeNegativeDistance(shown), true);
            if (ui.button("ext_flip", {x, y, w, h}, "Flip direction", "ic_extrude_flip")) {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                fs::sketchSession().flipExtrudeDirection();
            }
            y += h + ui.px(6);
            break;
        case fs::ExtrudeExtentMode::Symmetric:
            distance("Each side", "ext_each", fs::extrudePositiveDistance(shown), true);
            break;
        case fs::ExtrudeExtentMode::TwoSides:
            distance("Side A", "ext_side_a", fs::extrudePositiveDistance(shown), true);
            distance("Side B", "ext_side_b", fs::extrudeNegativeDistance(shown), false);
            break;
        default:
            // Through All and To Face derive their reach from the part.
            ui.label({x, y, w, ui.px(20)},
                     "Reaches " + formatMeasure(shown.depth) + " m", theme_.textSecondary,
                     theme_.smallFontSize);
            y += ui.px(26);
            break;
    }

    section("Operation");
    {
        static const fs::CadFeatureOperation all[4] = {
            fs::CadFeatureOperation::NewBody, fs::CadFeatureOperation::Add,
            fs::CadFeatureOperation::Cut, fs::CadFeatureOperation::Intersect};
        const float s = ui.px(36);
        float bx = x;
        for (int i = 0; i < 4; ++i) {
            if (!operations[i]) continue;
            const std::string id = "ext_op_" + std::to_string(i);
            if (ui.iconButton(id, {bx, y, s, s}, operationIcon(all[i]), operationLabel(all[i]),
                              operation == all[i])) {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                const fs::CadStatus status = fs::sketchSession().setOperation(all[i]);
                if (status != fs::CadStatus::Ok) report(sketchStatusText(status), true);
            }
            bx += s + ui.px(4);
        }
        y += s + ui.px(4);
        ui.label({x, y, w, ui.px(18)}, operationLabel(operation), theme_.textPrimary,
                 theme_.smallFontSize);
        y += ui.px(26);
    }
    // The preview IS the candidate: a refusal is named before the commit.
    if (chosen && !candidateValid) {
        text("Cannot extrude: " + sketchStatusText(candidateStatus), theme_.textError);
        y += ui.px(6);
    }
    // A SECONDARY Extrude beside the typed values (the ribbon's is the one
    // primary); Back and Cancel have one home, the ribbon's Sketch group.
    if (chosen && candidateValid &&
        ui.button("ext_commit", {x, y, w, h}, "Extrude", operationIcon(operation), false, true)) {
        commitSketch();
    }
}

}  // namespace fsdesktop
