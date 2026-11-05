#include "desktop_editor.h"

#include <algorithm>
#include <cctype>
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
#include "forgeshape_import_commit.h"
#include "forgeshape_mesh_import.h"
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
// A label READ at the geometry: the display precision, three decimals in
// metres with trailing zeros stripped (the annotation rule both shells use).
std::string formatLabel(double v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.3f", v);
    std::string out = buffer;
    while (!out.empty() && out.back() == '0') out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    if (out == "-0") out = "0";
    return out + " m";
}

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
        case fs::BodyRepresentation::Cad: return "CAD Body";
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
    // The core's one rule for both shells (`WORKBENCH-RIBBON-R2`).
    if (body != nullptr) return fs::bodyDisplayLabel(*body);
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

void DesktopEditor::exportAs(fs::MeshExportFormat format) {
    if (platform_ == nullptr) return;
    std::vector<uint8_t> bytes;
    fs::GlbExportStatus why = fs::GlbExportStatus::Ok;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::constructionScene().hasProject()) return;
        bytes = fs::exportSceneAs(format, fs::constructionScene(), fs::ProjectKind::Construction, &why);
    }
    if (bytes.empty()) {
        report(std::string("Could not export: ") + fs::glbExportStatusName(why), true);
        return;
    }
    // The extension without its dot names both the filter and the file.
    const std::string extension = std::string(fs::meshExportFileExtension(format)).substr(1);
    std::string name;
    if (platform_->saveFile(fs::meshExportFormatName(format), extension.c_str(),
                            "Model." + extension, bytes, &name)) {
        report("Exported " + name);
    }
}

bool DesktopEditor::importMeshBytes(const std::vector<uint8_t>& bytes, const std::string& name) {
    std::string extension;
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        extension = name.substr(dot);
        for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const fs::MeshImportFormat format =
        fs::detectMeshImportFormat(bytes.data(), bytes.size(), extension.c_str());
    fs::ParsedGlbScene parsed;
    const fs::GlbImportStatus parsedWhy = fs::importMeshFile(bytes.data(), bytes.size(), format, &parsed);
    if (parsedWhy != fs::GlbImportStatus::Ok) {
        report(std::string("Could not import ") + name + ": " + fs::glbImportStatusName(parsedWhy), true);
        return false;
    }
    fs::ImportCommitReport committed;
    fs::ImportCommitStatus status;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::ImportModeFacts facts;
        facts.sculpting = fs::sculptSession().inSculptMode();
        facts.sketchActive = fs::sketchSession().active();
        status = fs::commitImportedGlbSceneInMode(parsed, fs::constructionScene(),
                                                  fs::constructionHistory(), facts, &committed);
    }
    if (status != fs::ImportCommitStatus::Ok) {
        report(std::string("Could not import ") + name + ": " + fs::importCommitStatusName(status), true);
        return false;
    }
    report("Imported " + std::to_string(committed.objects) + (committed.objects == 1 ? " object" : " objects") +
           " from " + name);
    return true;
}

void DesktopEditor::importMesh() {
    if (platform_ == nullptr) return;
    std::vector<uint8_t> bytes;
    std::string name;
    if (!platform_->openFile("Meshes", "glb;*.stl;*.obj;*.ply;*.3mf", &bytes, &name)) return;
    importMeshBytes(bytes, name);
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
    if (ctrl && key >= '1' && key <= '8') {
        // Ctrl+1..8: Home, Front, Back, Left, Right, Top, Bottom, Isometric -- refused by
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
            case 'I': importMesh(); return true;
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
    buildLivePlacementLabel(width, height);
    buildHistoryStrip(history);
    buildRibbon(ribbon);
    buildTitleBar(title);
    buildStatusBar(status);
    buildContextMenu(width, height);
    buildProjectMenu(width, height);
    buildShortcutsSheet(width, height);
}

void DesktopEditor::buildLivePlacementLabel(int width, int height) {
    fs::EditorLivePlacement live;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        if (!fs::editorSketchLivePlacement(width, height, &live)) return;
    }
    // The size of what the drag is placing, beside it (`WORKBENCH-UX-R1`), on
    // the same opaque plate as every value read at the geometry.
    std::string text;
    switch (live.tool) {
        case fs::SketchTool::Line: text = "L " + formatLabel(live.first); break;
        case fs::SketchTool::Rectangle:
            text = formatLabel(live.first) + " \xC3\x97 " + formatLabel(live.second);
            break;
        case fs::SketchTool::Circle: text = "\xC3\x98 " + formatLabel(live.first); break;
        default: return;
    }
    fsui::Ui& ui = *ui_;
    const float w = ui.textWidth(text, theme_.fontSize) + ui.px(16), h = ui.px(26);
    float x = live.screenX + ui.px(14), y = live.screenY + ui.px(14);
    if (x + w > width - ui.px(4)) x = live.screenX - w - ui.px(14);
    if (y + h > height - ui.px(4)) y = live.screenY - h - ui.px(14);
    const Rect plate{x, y, w, h};
    ui.rect(plate, theme_.surfaceFloating, ui.px(6));
    ui.border(plate, theme_.chromeHairline, ui.px(6), 1.0f);
    ui.label({x + ui.px(8), y, w - ui.px(16), h}, text, theme_.textPrimary, theme_.fontSize);
    liveLabel_ = text;
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
    // A full-window start PAGE, not a card on a scrim (`SKETCH-UX-R1`, the
    // Android start page's shape): one centred column at most 720 px wide,
    // the mark and the wordmark, and the starts as tiles side by side. Only
    // what the desktop editor can open today is offered.
    const float columnW = std::min(ui.px(720), width - ui.px(64));
    const float x = (width - columnW) * 0.5f;
    const float tileH = ui.px(128);
    const float blockH = ui.px(64) + ui.px(40) + tileH + ui.px(48);
    float y = std::max(ui.px(32), (height - blockH) * 0.5f);
    ui.icon("ic_forgeshape_mark", {x, y, ui.px(64), ui.px(64)}, theme_.accent);
    ui.label({x + ui.px(80), y + ui.px(4), columnW - ui.px(80), ui.px(36)}, "ForgeShape",
             theme_.textPrimary, 28, true);
    ui.label({x + ui.px(80), y + ui.px(40), columnW - ui.px(80), ui.px(20)},
             "Parametric parts and freeform bodies", theme_.textSecondary, theme_.fontSize);
    y += ui.px(64) + ui.px(40);
    const float gap = ui.px(16);
    const float tileW = (columnW - 2 * gap) / 3.0f;
    if (ui.tile("home_new_cad", {x, y, tileW, tileH}, "ic_sketch_new", "New CAD part",
                "Sketch, then extrude", true)) {
        newSketch();
    }
    if (ui.tile("home_new_freeform", {x + tileW + gap, y, tileW, tileH}, "ic_freeform",
                "New Freeform body", "A smooth body from a cage")) {
        newFreeform();
    }
    if (ui.tile("home_open", {x + 2 * (tileW + gap), y, tileW, tileH}, "ic_project",
                "Open project\xE2\x80\xA6", "A ForgeShape .forge file")) {
        openProject();
    }
    y += tileH + ui.px(20);
    // A verdict only: a refused open says why; nothing stands here at rest.
    if (statusError_) {
        ui.label({x, y, columnW, ui.px(20)}, status_, theme_.textError, theme_.fontSize);
    }
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
    // The ForgeShape mark is the ONE door to the project (as on Android): it
    // opens the project menu that holds every file act, so the title row
    // carries no second copy of them and no ambiguous trailing cross.
    if (ui.iconButton("tb_project", {x, y, s, s}, "", "Project  (open, save, export, close)",
                      projectMenuOpen_)) {
        projectMenuOpen_ = !projectMenuOpen_;
    }
    ui.icon("ic_forgeshape_mark", {x + ui.px(4), y + ui.px(4), ui.px(24), ui.px(24)}, theme_.accent);
    x += ui.px(36);
    const std::string name = projectName_.empty() ? std::string("ForgeShape") : projectName_;
    const float titleW = std::min(ui.px(200), ui.textWidth(name, theme_.fontSize, true) + ui.px(8));
    ui.label({x, bar.y, titleW, bar.h}, name, theme_.textPrimary, theme_.fontSize, true);
    x += titleW + ui.px(10);
    projectMenuX_ = bar.x + ui.px(10);
    projectMenuY_ = bar.bottom();
    // Undo and Redo have ONE home, the History strip's leading end, as its
    // back and forward arrows (`WORKBENCH-UX-R1`); Ctrl+Z and Ctrl+Y are
    // unchanged. Open, Save, Export and Close live in the project menu.

    float rx = bar.right() - ui.px(10) - s;
    const auto right = [&](const char* id, const char* icon, const char* tip, bool active = false) {
        const bool click = ui.iconButton(id, {rx, y, s, s}, icon, tip, active);
        rx -= s + ui.px(2);
        return click;
    };
    if (right("tb_properties", "ic_precision", "Properties", propertiesOpen_))
        propertiesOpen_ = !propertiesOpen_;
    if (right("tb_browser", "ic_panel_browser", "Browser", browserOpen_)) browserOpen_ = !browserOpen_;
}

void DesktopEditor::buildProjectMenu(int width, int height) {
    if (!projectMenuOpen_) return;
    fsui::Ui& ui = *ui_;
    bool project = false, sketching = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        project = fs::constructionScene().hasProject();
        sketching = fs::sketchSession().active() || fs::supportChooser().active();
    }
    struct Row { const char* id; const char* icon; const char* text; const char* key; int act; };
    std::vector<Row> rows;
    // Each act drawn only where it can succeed: a half-drawn sketch is not
    // project truth, so the file acts wait for it to finish or cancel.
    if (!sketching) {
        if (project) rows.push_back({"pm_new_sketch", "ic_sketch_new", "New sketch", "Shift+S", 0});
        rows.push_back({"pm_open", "ic_project", "Open project\xE2\x80\xA6", "Ctrl+O", 1});
        if (project) {
            rows.push_back({"pm_save", "ic_save", "Save project\xE2\x80\xA6", "Ctrl+S", 2});
            rows.push_back({"pm_import", "ic_import", "Import mesh\xE2\x80\xA6", "Ctrl+I", 6});
            rows.push_back({"pm_export", "ic_export", "Export GLB\xE2\x80\xA6", "Ctrl+E", 3});
            rows.push_back({"pm_export_stl", "ic_export", "Export STL (mm)\xE2\x80\xA6", "", 7});
            rows.push_back({"pm_export_3mf", "ic_export", "Export 3MF (mm)\xE2\x80\xA6", "", 8});
            rows.push_back({"pm_export_obj", "ic_export", "Export OBJ\xE2\x80\xA6", "", 9});
            rows.push_back({"pm_export_ply", "ic_export", "Export PLY\xE2\x80\xA6", "", 10});
            rows.push_back({"pm_close", "ic_sketch_cancel", "Close project", "", 4});
        }
    }
    rows.push_back({"pm_reload_theme", "ic_settings", "Reload theme", "", 5});
    const float rowH = ui.px(32), w = ui.px(260), pad = ui.px(6);
    const float h = rows.size() * rowH + 2 * pad;
    const Rect menu{projectMenuX_, projectMenuY_ + ui.px(2), w, h};
    ui.panel(menu);
    int chosen = -1;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Rect r{menu.x + pad, menu.y + pad + i * rowH, w - 2 * pad, rowH};
        if (ui.listRow(rows[i].id, r, rows[i].text, rows[i].icon, false, rows[i].key)) {
            chosen = rows[i].act;
        }
    }
    const bool escape = std::find(input_.keys.begin(), input_.keys.end(), fsui::Key::Escape) !=
                        input_.keys.end();
    Rect mark;
    const bool onMark = ui.widgetRect("tb_project", &mark) && mark.contains(input_.mouseX, input_.mouseY);
    const bool clickedAway = (input_.mousePressed[0] || input_.mousePressed[1] ||
                              input_.mousePressed[2]) &&
                             !menu.contains(input_.mouseX, input_.mouseY) && !onMark;
    (void)width;
    (void)height;
    if (chosen < 0) {
        if (escape || clickedAway) projectMenuOpen_ = false;
        return;
    }
    projectMenuOpen_ = false;
    switch (chosen) {
        case 0: newSketch(); break;
        case 1: openProject(); break;
        case 2: saveProject(); break;
        case 3: exportGlb(); break;
        case 4: closeProject(); break;
        case 5: reloadTheme(); break;
        case 6: importMesh(); break;
        case 7: exportAs(fs::MeshExportFormat::Stl); break;
        case 8: exportAs(fs::MeshExportFormat::ThreeMf); break;
        case 9: exportAs(fs::MeshExportFormat::Obj); break;
        case 10: exportAs(fs::MeshExportFormat::Ply); break;
        default: break;
    }
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
        if (row.command("rb_cad_part", "ic_cad_part", "CAD part", false, true,
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
        // The empty state follows the stage the user is at: what the next
        // act adds, never a standing instruction.
        fs::SketchSessionState state = fs::SketchSessionState::Inactive;
        {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            state = fs::sketchSession().state();
        }
        const std::string empty =
            state == fs::SketchSessionState::Editing ? "Finish the sketch to add it"
            : state == fs::SketchSessionState::Ready ? "Choose a region, then Extrude"
                                                     : "No steps: this body has no feature history";
        ui.label({x, bar.y, bar.right() - x - ui.px(12), bar.h}, empty, theme_.textSecondary,
                 theme_.smallFontSize);
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
    // The cube is a handle on the view: a press that travels past 4 px turns
    // the camera as a drag on the model does, and chooses no view.
    if (!sketchView) {
        const float mx = input_.mouseX, my = input_.mouseY;
        if (input_.mousePressed[0] && box.contains(mx, my)) {
            cubePress_ = true;
            cubeDragging_ = false;
            cubeDownX_ = cubeLastX_ = mx;
            cubeDownY_ = cubeLastY_ = my;
        }
        if (cubePress_ && input_.mouseDown[0]) {
            if (!cubeDragging_ && std::hypot(mx - cubeDownX_, my - cubeDownY_) > ui.px(4)) {
                cubeDragging_ = true;
            }
            if (cubeDragging_) {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                fs::editorCamera().orbitByPixels(mx - cubeLastX_, my - cubeLastY_);
                camera = fs::editorCamera().snapshot();
                cubeLastX_ = mx;
                cubeLastY_ = my;
            }
        }
    }
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
        const float r = ui.px(10);
        if (end.positive) ui.line(cx, cy, end.sx, end.sy, ui.px(2.5f), colour);
        const std::string id = std::string("cube_") + (end.positive ? "+" : "-") +
                               static_cast<char>('x' + end.axis);
        // In a sketch view the cube only reports orientation: the sketch owns
        // the camera, so its ends are not controls there.
        bool hot = false;
        if (!sketchView) {
            // The end already facing the eye (within 10 degrees) names the
            // OPPOSITE view, so Bottom, Back and Left are one click away.
            const bool facing = end.depth > 0.985f;
            const fs::StandardView named = views[end.axis][(end.positive != facing) ? 0 : 1];
            const Rect target{end.sx - ui.px(12), end.sy - ui.px(12), ui.px(24), ui.px(24)};
            if (ui.iconButton(id, target, "", std::string(end.positive ? "" : "-") + letters[end.axis] +
                                                  " \xC2\xB7 " + fs::standardViewName(named) + " view") &&
                !cubeDragging_) {
                setStandardView(static_cast<int>(named));
            }
            hot = target.contains(input_.mouseX, input_.mouseY);
        }
        const float grow = hot ? ui.px(2) : 0.0f;
        if (end.positive) {
            ui.circle(end.sx, end.sy, r + grow, colour);
            ui.label({end.sx - r, end.sy - r, 2 * r, 2 * r}, letters[end.axis],
                     fsui::Color::hex(0xFF12110F), theme_.smallFontSize - 1.0f, true, true);
        } else {
            // A labelled RING at the axis colour: the opposite end is a view
            // too, and reads as one at rest.
            ui.circle(end.sx, end.sy, r + grow, colour);
            ui.circle(end.sx, end.sy, r + grow - ui.px(1.5f), theme_.surfaceFloating);
            ui.label({end.sx - r, end.sy - r, 2 * r, 2 * r}, std::string("\xE2\x88\x92") + letters[end.axis],
                     colour, theme_.smallFontSize - 3.0f, true, true);
        }
    }
    if (input_.mouseReleased[0]) {
        cubePress_ = false;
        cubeDragging_ = false;
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
    const int count = 7;
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
    if (tool("vb_iso", "ic_view_iso", "Isometric view  Ctrl+8")) setStandardView(static_cast<int>(fs::StandardView::Isometric));
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
        // Arrange (`PLACEMENT-PRECISION-R1`): one act each, one Undo each.
        const float half = (w - ui.px(6)) * 0.5f;
        const auto arrange = [&](fs::EditorArrangeAct act, const char* done) {
            fs::EditorArrangeStatus status;
            {
                std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                status = fs::editorArrangeActiveBody(act);
            }
            if (status == fs::EditorArrangeStatus::Ok) report(done);
            else if (status == fs::EditorArrangeStatus::NothingToDo) report("The body is already there");
            else report("This body cannot be moved", true);
        };
        if (ui.button("arrange_drop", {x, y, half, fieldH}, "Drop to ground", "")) {
            arrange(fs::EditorArrangeAct::DropToGround, "Standing on the ground");
        }
        if (ui.button("arrange_center", {x + half + ui.px(6), y, half, fieldH}, "Centre on origin", "")) {
            arrange(fs::EditorArrangeAct::CenterOnOrigin, "Centred on the origin");
        }
        y += fieldH + ui.px(8);
        // Snap while dragging: session presentation read at each drag's start.
        fs::GizmoSnap snap;
        {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            snap = fs::gizmoSnapSettings();
        }
        const auto snapRow = [&](const char* name, const char* prefix, const double* steps,
                                 const char* const* labels, int count, bool move) {
            // A narrower name column than the fields' and centred steps with
            // no row padding: five padded rows in the field column would not
            // fit "10 cm" and would be drawn as an ellipsis.
            const float nameW = ui.px(50);
            ui.label({x, y, nameW, fieldH}, name, theme_.textPrimary, theme_.smallFontSize);
            const float gap = ui.px(2);
            const float cellW =
                (w - nameW - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
            for (int i = 0; i < count; ++i) {
                const double current = move ? snap.translationMeters : snap.rotationDegrees;
                const Rect cell{x + nameW + i * (cellW + gap), y, cellW, fieldH};
                if (ui.tab(std::string(prefix) + std::to_string(i), cell, labels[i],
                           current == steps[i])) {
                    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
                    (move ? fs::gizmoSnapSettings().translationMeters
                          : fs::gizmoSnapSettings().rotationDegrees) = steps[i];
                }
            }
            y += fieldH + ui.px(6);
        };
        static const double moveSteps[5] = {0.0, 0.001, 0.01, 0.1, 1.0};
        static const char* moveLabels[5] = {"Off", "1 mm", "1 cm", "10 cm", "1 m"};
        static const double turnSteps[4] = {0.0, 5.0, 15.0, 45.0};
        static const char* turnLabels[4] = {"Off", "5\xC2\xB0", "15\xC2\xB0", "45\xC2\xB0"};
        ui.label({x, y, w, ui.px(18)}, "Snap while dragging", theme_.textSecondary, theme_.smallFontSize);
        y += ui.px(20);
        snapRow("Move", "snap_move_", moveSteps, moveLabels, 5, true);
        snapRow("Rotate", "snap_turn_", turnSteps, turnLabels, 4, false);
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
        if (ui.button("make_cad_part", {x, y, w, fieldH}, "Make CAD part", "ic_cad_part")) {
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
    (void)sketchState;
    // The status bar carries verdicts; how the mouse and the keys work is one
    // click (or ?) away on the sheet, not a permanent line of instructions
    // (`WORKBENCH-UX-R1`, "chrome reports, it does not instruct").
    const std::string open = "Keyboard & mouse  ?";
    const float hw = ui.textWidth(open, theme_.fontSize) + ui.px(24);
    if (ui.button("sb_shortcuts", {bar.right() - hw - ui.px(6), bar.y + ui.px(2), hw, bar.h - ui.px(4)},
                  open)) {
        shortcutsOpen_ = !shortcutsOpen_;
    }
}

void DesktopEditor::buildShortcutsSheet(int width, int height) {
    if (!shortcutsOpen_) return;
    fsui::Ui& ui = *ui_;
    struct Line { const char* keys; const char* act; };
    struct Group { const char* title; std::vector<Line> lines; };
    const std::vector<Group> groups = {
        {"Mouse", {{"Left drag", "Orbit (on a body handle: move it)"},
                   {"Middle drag, Shift+drag", "Pan"},
                   {"Wheel", "Zoom about the pointer"},
                   {"Click", "Select a body"},
                   {"Right click", "The body's menu"},
                   {"Drag the view cube", "Turn the view"}}},
        {"Views", {{"Ctrl+1 \xE2\x80\xA6 Ctrl+8", "Home, Front, Back, Left, Right, Top, Bottom, Isometric"},
                   {"F", "Fit everything"},
                   {"A view cube end facing you", "The opposite view"}}},
        {"Sketch", {{"Shift+S", "New sketch"},
                    {"V L P R C A S T", "Select, Line, Polyline, Rectangle, Circle, Arc, Spline, Trim"},
                    {"Enter", "Finish sketch, then Extrude"},
                    {"Esc", "End a run, then cancel"}}},
        {"Project", {{"Ctrl+Z, Ctrl+Y", "Undo, Redo"},
                     {"Ctrl+S, Ctrl+O, Ctrl+E", "Save, Open, Export GLB"},
                     {"Delete", "Delete the selected body"}}},
    };
    const float rowH = ui.px(24), titleH = ui.px(30), pad = ui.px(20);
    float contentH = 0;
    for (const Group& g : groups) contentH += titleH + g.lines.size() * rowH + ui.px(8);
    const float w = std::min(ui.px(760), width - ui.px(40));
    const float h = std::min(contentH + 2 * pad + ui.px(30), height - ui.px(40));
    const Rect sheet{(width - w) * 0.5f, (height - h) * 0.5f, w, h};
    ui.panel(sheet);
    ui.label({sheet.x + pad, sheet.y + pad - ui.px(4), w - 2 * pad, ui.px(26)}, "Keyboard and mouse",
             theme_.textPrimary, theme_.panelTitleFontSize, true);
    float y = sheet.y + pad + ui.px(30);
    const float keyW = ui.px(250);
    for (const Group& g : groups) {
        ui.label({sheet.x + pad, y, w - 2 * pad, titleH}, g.title, theme_.textSecondary,
                 theme_.smallFontSize, true);
        y += titleH;
        for (const Line& line : g.lines) {
            ui.label({sheet.x + pad, y, keyW, rowH}, line.keys, theme_.textPrimary, theme_.fontSize, true);
            ui.label({sheet.x + pad + keyW, y, w - 2 * pad - keyW, rowH}, line.act, theme_.textSecondary,
                     theme_.fontSize);
            y += rowH;
        }
        y += ui.px(8);
    }
    Rect toggle;
    const bool onToggle = ui.widgetRect("sb_shortcuts", &toggle) &&
                          toggle.contains(input_.mouseX, input_.mouseY);
    const bool escape = std::find(input_.keys.begin(), input_.keys.end(), fsui::Key::Escape) !=
                        input_.keys.end();
    const bool clickedAway = (input_.mousePressed[0] || input_.mousePressed[1] ||
                              input_.mousePressed[2]) &&
                             !sheet.contains(input_.mouseX, input_.mouseY) && !onToggle;
    if (escape || clickedAway) shortcutsOpen_ = false;
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
            if (c == U'?') {  // the keyboard and mouse sheet
                shortcutsOpen_ = !shortcutsOpen_;
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
