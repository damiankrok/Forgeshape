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
#include "forgeshape_construction.h"
#include "forgeshape_display.h"
#include "forgeshape_editor_core.h"
#include "forgeshape_freeform_session.h"
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

std::string formatNumber(double v, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, v);
    return buffer;
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

void DesktopEditor::newCadPart() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
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
    projectName_ = "Untitled";
    homeVisible_ = false;
    refreshTitle();
    report("New CAD part");
}

void DesktopEditor::newFreeform() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
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
    projectName_ = "Untitled";
    homeVisible_ = false;
    refreshTitle();
    report("New Freeform body");
}

void DesktopEditor::openProject() {
    if (platform_ == nullptr) return;
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
        if (fs::sketchSession().active()) fs::sketchSession().cancel();
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
    fs::ConstructionRestoreReport restored;
    if (fs::constructionHistory().undo(&restored)) {
        fs::supportChooser().cancel();
        report("Undo");
    }
}

void DesktopEditor::redo() {
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
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
    std::lock_guard<std::mutex> lock(fs::editorStateMutex());
    if (fs::sketchSession().active()) fs::sketchSession().cancel();
    fs::constructionHistory().clear();
    fs::constructionScene().closeProject();
    fs::editorSelection().clearSelection();
    projectName_.clear();
    homeVisible_ = true;
    refreshTitle();
}

bool DesktopEditor::shortcut(char key, bool ctrl, bool shift) {
    if (ui_ && ui_->typing()) return false;
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
    fs::CameraController& camera = fs::editorCamera();
    // The wheel zooms about the pointer.
    if (!overUi && input_.wheel != 0.0f) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        camera.zoomAtPixel(input_.mouseX, input_.mouseY, input_.wheel);
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
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        hasProject = fs::constructionScene().hasProject();
    }
    if (!hasProject) homeVisible_ = true;
    if (homeVisible_) {
        buildHome(width, height);
        return;
    }
    fsui::Ui& ui = *ui_;
    const float pad = ui.px(theme_.panelInset);
    const Rect bar{0, 0, static_cast<float>(width), ui.px(theme_.toolbarHeight)};
    const Rect status{0, height - ui.px(theme_.statusHeight), static_cast<float>(width),
                      ui.px(theme_.statusHeight)};
    const float top = bar.bottom() + pad;
    const float bottom = status.y - pad;
    if (browserOpen_) {
        buildBrowser({pad, top, ui.px(theme_.browserWidth), bottom - top});
    }
    if (propertiesOpen_) {
        const float w = ui.px(theme_.propertiesWidth);
        buildProperties({width - w - pad, top, w, bottom - top});
    }
    buildToolbar(bar);
    buildStatusBar(status);
}

void DesktopEditor::buildHome(int width, int height) {
    fsui::Ui& ui = *ui_;
    ui.rect({0, 0, static_cast<float>(width), static_cast<float>(height)},
            fsui::Color::hex(0xFF262421), 0);
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
    if (ui.button("home_new_cad", {x, y, bw, bh}, "New CAD part", "ic_op_new_body", true)) newCadPart();
    y += bh + ui.px(10);
    if (ui.button("home_new_freeform", {x, y, bw, bh}, "New Freeform body", "ic_freeform")) newFreeform();
    y += bh + ui.px(10);
    if (ui.button("home_open", {x, y, bw, bh}, "Open project\xE2\x80\xA6", "ic_project")) openProject();
    y += bh + ui.px(18);
    ui.label({x, y, bw, ui.px(20)}, status_, statusError_ ? theme_.textError : theme_.textSecondary,
             theme_.smallFontSize);
}

void DesktopEditor::buildToolbar(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.rect(bar, theme_.chromeSurface, 0);
    ui.separator(bar.x, bar.bottom() - 1, bar.w);
    ui.block(bar);
    const float s = ui.px(36);
    const float y = bar.y + (bar.h - s) * 0.5f;
    float x = bar.x + ui.px(10);
    ui.icon("ic_forgeshape_mark", {x + ui.px(4), y + ui.px(6), ui.px(24), ui.px(24)}, theme_.accent);
    x += ui.px(36);
    const float titleW = std::min(ui.px(180), ui.textWidth(projectName_.empty() ? "ForgeShape" : projectName_, theme_.fontSize, true) + ui.px(8));
    ui.label({x, bar.y, titleW, bar.h}, projectName_.empty() ? "ForgeShape" : projectName_,
             theme_.textPrimary, theme_.fontSize, true);
    x += titleW + ui.px(10);
    const auto gap = [&]() {
        ui.rect({x + ui.px(4), y + ui.px(8), 1.0f, s - ui.px(16)}, theme_.chromeHairline, 0);
        x += ui.px(10);
    };
    const auto tool = [&](const char* id, const char* icon, const char* tip, bool active = false) {
        const bool click = ui.iconButton(id, {x, y, s, s}, icon, tip, active);
        x += s + ui.px(2);
        return click;
    };
    if (tool("tb_browser", "ic_panel_browser", "Browser (objects and history)", browserOpen_))
        browserOpen_ = !browserOpen_;
    gap();
    if (tool("tb_open", "ic_project", "Open project\xE2\x80\xA6  Ctrl+O")) openProject();
    if (tool("tb_save", "ic_sketch_keep", "Save project\xE2\x80\xA6  Ctrl+S")) saveProject();
    if (tool("tb_export", "ic_object_duplicate", "Export GLB\xE2\x80\xA6  Ctrl+E")) exportGlb();
    gap();
    static const fs::PrimitiveKind kinds[] = {fs::PrimitiveKind::Box, fs::PrimitiveKind::Cylinder,
                                              fs::PrimitiveKind::Sphere, fs::PrimitiveKind::Cone,
                                              fs::PrimitiveKind::Capsule, fs::PrimitiveKind::Plane};
    static const char* names[] = {"Add Box", "Add Cylinder", "Add Sphere", "Add Cone",
                                  "Add Capsule", "Add Plane"};
    for (int i = 0; i < 6; ++i) {
        const std::string id = std::string("tb_add_") + std::to_string(i);
        if (tool(id.c_str(), primitiveIcon(kinds[i]), names[i])) addPrimitive(static_cast<int>(kinds[i]));
    }
    gap();
    if (tool("tb_cad_part", "ic_op_new_body", "Make CAD part")) makeCadPart();
    if (tool("tb_delete", "ic_delete", "Delete body  Del")) deleteActive();
    gap();
    if (tool("tb_undo", "ic_undo", "Undo  Ctrl+Z")) undo();
    if (tool("tb_redo", "ic_redo", "Redo  Ctrl+Y")) redo();

    // The trailing group, right aligned.
    float rx = bar.right() - ui.px(10) - s;
    const auto right = [&](const char* id, const char* icon, const char* tip, bool active = false) {
        const bool click = ui.iconButton(id, {rx, y, s, s}, icon, tip, active);
        rx -= s + ui.px(2);
        return click;
    };
    if (right("tb_close", "ic_sketch_cancel", "Close project")) closeProject();
    if (right("tb_properties", "ic_precision", "Properties", propertiesOpen_))
        propertiesOpen_ = !propertiesOpen_;
    if (right("tb_theme", "ic_settings", "Reload theme (theme.json)")) reloadTheme();
}

void DesktopEditor::buildBrowser(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.panel(panel);
    const float inner = ui.px(10);
    float y = panel.y + inner;
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    ui.label({x + ui.px(4), y, w, ui.px(22)}, "OBJECTS", theme_.textSecondary, theme_.titleFontSize, true);
    y += ui.px(26);
    const float rowH = ui.px(theme_.rowHeight);
    std::vector<std::pair<fs::ObjectId, fs::BodyRepresentation>> bodies;
    fs::ObjectId active = fs::kNoObject;
    std::vector<fs::CadTimelineRow> timeline;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        const fs::ConstructionScene& scene = fs::constructionScene();
        for (size_t i = 0; i < scene.bodyCount(); ++i) {
            bodies.push_back({scene.bodyAt(i).objectId(), scene.bodyAt(i).representation()});
        }
        if (scene.hasProject()) {
            active = scene.activeBodyId();
            if (const fs::CadBody* cad = scene.activeBody().cadOrNull()) {
                timeline = fs::buildCadTimelineFor(cad->state()).rows;
            }
        }
    }
    const float listBottom = timeline.empty() ? panel.bottom() - inner : panel.y + panel.h * 0.5f;
    ui.pushClip({panel.x, y, panel.w, listBottom - y});
    for (const auto& body : bodies) {
        const std::string id = "obj_" + std::to_string(body.first);
        if (ui.listRow(id, {x, y, w, rowH}, bodyLabel(body.first), representationIcon(body.second),
                       body.first == active, representationLabel(body.second))) {
            std::lock_guard<std::mutex> lock(fs::editorStateMutex());
            fs::constructionScene().setActiveBody(body.first);
            fs::editorSelection().setSelected(body.first);
        }
        y += rowH + ui.px(2);
    }
    ui.popClip();
    if (timeline.empty()) return;
    y = listBottom + ui.px(6);
    ui.separator(x, y, w);
    y += ui.px(8);
    ui.label({x + ui.px(4), y, w, ui.px(22)}, "HISTORY", theme_.textSecondary, theme_.titleFontSize, true);
    y += ui.px(26);
    ui.pushClip({panel.x, y, panel.w, panel.bottom() - inner - y});
    for (const fs::CadTimelineRow& row : timeline) {
        const std::string id = "hist_" + std::to_string(static_cast<int>(row.kind)) + "_" +
                               std::to_string(row.id);
        const char* state = row.state == fs::CadTimelineRowState::Failed ? "failed"
                            : row.state == fs::CadTimelineRowState::NotRegenerated ? "not rebuilt"
                                                                                    : "";
        ui.listRow(id, {x, y, w, rowH}, timelineRowLabel(row), timelineRowIcon(row), false, state);
        y += rowH + ui.px(2);
    }
    ui.popClip();
}

void DesktopEditor::buildProperties(const Rect& panel) {
    fsui::Ui& ui = *ui_;
    ui.panel(panel);
    const float inner = ui.px(12);
    const float x = panel.x + inner;
    const float w = panel.w - 2 * inner;
    float y = panel.y + inner;

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

    ui.label({x, y, w, ui.px(24)}, bodyLabel(active), theme_.textPrimary, 15, true);
    y += ui.px(24);
    ui.label({x, y, w, ui.px(18)}, representationLabel(representation), theme_.textSecondary,
             theme_.smallFontSize);
    y += ui.px(28);

    const float labelW = ui.px(70);
    const float fieldH = ui.px(theme_.controlHeight);
    const float fieldW = (w - labelW - ui.px(8)) / 3.0f - ui.px(4);
    const auto section = [&](const char* title) {
        ui.separator(x, y, w);
        y += ui.px(10);
        ui.label({x, y, w, ui.px(20)}, title, theme_.textSecondary, theme_.titleFontSize, true);
        y += ui.px(24);
    };
    // Placement: the authored transform, typed. A face-supported part stands
    // where its producer's face puts it, so it has no placement of its own.
    section("PLACEMENT");
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
                if (ui.numberField(id, field, rows[r][c], r == 2 ? 3 : 3, c == 2 ? units[r] : "")) {
                    changed = true;
                }
            }
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
        section("SHAPE");
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
    // Measure: read off the mesh as drawn, never stored.
    section("MEASURE");
    if (measured) {
        const auto line = [&](const char* name, const std::string& value) {
            ui.label({x, y, labelW + ui.px(20), ui.px(22)}, name, theme_.textSecondary, theme_.smallFontSize);
            ui.label({x + labelW + ui.px(20), y, w - labelW - ui.px(20), ui.px(22)}, value,
                     theme_.textMeasure, theme_.smallFontSize);
            y += ui.px(22);
        };
        const fs::DVec3 size{mass.boundsMax.x - mass.boundsMin.x, mass.boundsMax.y - mass.boundsMin.y,
                             mass.boundsMax.z - mass.boundsMin.z};
        line("Size", formatNumber(size.x, 3) + " \xC3\x97 " + formatNumber(size.y, 3) + " \xC3\x97 " +
                         formatNumber(size.z, 3) + " m");
        line("Area", formatNumber(mass.area, 4) + " m\xC2\xB2");
        line("Volume", mass.closed ? formatNumber(mass.volume, 5) + " m\xC2\xB3" : std::string("open surface"));
        if (mass.closed) {
            line("Centre", formatNumber(mass.centroid.x, 3) + ", " + formatNumber(mass.centroid.y, 3) +
                               ", " + formatNumber(mass.centroid.z, 3));
        }
    }
    y += ui.px(8);
    // Display: presentation the core already owns.
    section("DISPLAY");
    fs::DisplaySettingsStore& display = fs::displaySettings();
    const fs::ViewportDisplaySettings settings = display.snapshot();
    bool grid = settings.gridVisible;
    bool outline = settings.selectionOutlineVisible;
    bool ortho = false;
    {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        ortho = fs::editorCamera().projectionMode() == fs::ProjectionMode::Orthographic;
    }
    if (ui.toggle("disp_grid", {x, y, w, fieldH}, "Grid", &grid)) display.setGridVisible(grid);
    y += fieldH + ui.px(4);
    if (ui.toggle("disp_outline", {x, y, w, fieldH}, "Selection outline", &outline))
        display.setSelectionOutlineVisible(outline);
    y += fieldH + ui.px(4);
    if (ui.toggle("disp_ortho", {x, y, w, fieldH}, "Orthographic", &ortho)) {
        std::lock_guard<std::mutex> lock(fs::editorStateMutex());
        fs::editorCamera().setProjectionMode(ortho ? fs::ProjectionMode::Orthographic
                                                   : fs::ProjectionMode::Perspective);
    }
}

void DesktopEditor::buildStatusBar(const Rect& bar) {
    fsui::Ui& ui = *ui_;
    ui.rect(bar, theme_.chromeSurface, 0);
    ui.separator(bar.x, bar.y, bar.w);
    ui.block(bar);
    ui.label({bar.x + ui.px(12), bar.y, bar.w * 0.6f, bar.h}, status_,
             statusError_ ? theme_.textError : theme_.textSecondary, theme_.smallFontSize);
    const std::string hint =
        "Left drag: orbit \xC2\xB7 Middle or Shift+drag: pan \xC2\xB7 Wheel: zoom \xC2\xB7 Click: select";
    const float hw = ui.textWidth(hint, theme_.smallFontSize);
    ui.label({bar.right() - hw - ui.px(12), bar.y, hw, bar.h}, hint, theme_.textDisabled,
             theme_.smallFontSize);
}

}  // namespace fsdesktop
