// The desktop shell's automated check (`DESKTOP-R1`): the whole desktop editor
// -- the shared core, the shared renderer and ForgeShape's own interface --
// driven headless on a Linux runner through Mesa's software Vulkan driver, a
// frame read back and checked, and written as evidence.
//
// It proves the renderer's desktop path and the interface draw; it does not
// prove a Windows GPU, a real window or the owner's PC, which stay UNVERIFIED
// until the owner runs the installer.
#include <algorithm>
#include <cstdio>
#include <mutex>
#include <cstdlib>
#include <string>
#include <vector>

#include "desktop_editor.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_editor_core.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_mass_properties.h"
#include "forgeshape_scene.h"
#include "forgeshape_render_surface.h"
#include "forgeshape_selection.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_support_chooser.h"
#include "png_writer.h"

namespace {

class HeadlessPlatform : public fsdesktop::DesktopPlatform {
public:
    bool openFile(const char*, const char*, std::vector<uint8_t>*, std::string*) override {
        return false;
    }
    bool saveFile(const char*, const char*, const std::string&, const std::vector<uint8_t>& bytes,
                  std::string* name) override {
        lastSaved = bytes;
        *name = "headless.forge";
        return true;
    }
    void setTitle(const std::string&) override {}
    void requestQuit() override {}
    std::string settingsDirectory() override { return std::string(); }
    std::vector<uint8_t> lastSaved;
};

struct Pixel {
    int r, g, b;
};

Pixel at(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t x, uint32_t y) {
    const size_t i = (static_cast<size_t>(y) * w + x) * 4;
    return {rgba[i], rgba[i + 1], rgba[i + 2]};
}

bool near(Pixel p, int r, int g, int b, int tol) {
    return std::abs(p.r - r) <= tol && std::abs(p.g - g) <= tol && std::abs(p.b - b) <= tol;
}

// The active body's measured volume and world bounds, off the mesh it draws.
bool measureActive(forgeshape::MassProperties* out) {
    const forgeshape::ConstructionScene& scene = forgeshape::constructionScene();
    if (!scene.hasProject()) return false;
    for (const forgeshape::SceneDrawItem& item : forgeshape::viewSceneSnapshot()) {
        if (item.objectId == scene.activeBodyId() && item.mesh) {
            return forgeshape::computeMassProperties(item.mesh->vertices(), item.mesh->vertexCount(),
                                                     item.mesh->indices(), item.mesh->indexCount(),
                                                     item.model, out);
        }
    }
    return false;
}

bool screenOf(const forgeshape::Vec3& world, uint32_t w, uint32_t h, float* x, float* y) {
    return forgeshape::projectWorldToScreen(forgeshape::editorCamera().snapshot(), world,
                                            static_cast<int>(w), static_cast<int>(h), x, y);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "desktop-headless.png";
    const uint32_t width = 1600, height = 900;
    forgeshape::DesktopWindow window;
    window.width = width;
    window.height = height;
    HeadlessPlatform platform;
    fsdesktop::DesktopEditor editor;
    const std::vector<std::string> fonts = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
    const std::vector<std::string> bold = {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"};
    if (!editor.start(&window, &platform, fonts, bold)) {
        std::printf("FORGESHAPE_DESKTOP_HEADLESS_FAIL:start\n");
        return 1;
    }
    int failures = 0;
    const auto check = [&failures](const char* name, bool ok) {
        std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", name);
        if (!ok) ++failures;
    };
    fsui::InputState idle;
    // The gallery (`WORKBENCH-UX-R1`): named captures of the states the look
    // is iterated against, written beside the evidence image.
    const std::string gallery = out.substr(0, out.size() - 4) + "-";
    const auto snap = [&](const char* name) {
        editor.requestCapture();
        for (int i = 0; i < 6 && !editor.captureReady(); ++i) editor.frame(width, height, 1.0f, idle);
        if (!editor.captureReady()) return;
        uint32_t cw = 0, ch = 0;
        const std::vector<uint8_t>& shot = editor.capture(&cw, &ch);
        fsdesktop::writePng(gallery + name + ".png", shot.data(), cw, ch, 4);
    };
    // Home first: one frame, then a CAD part, a cylinder beside it, a frame.
    for (int i = 0; i < 2; ++i) editor.frame(width, height, 1.0f, idle);
    snap("01-home");
    editor.newCadPart();
    editor.addPrimitive(1);  // a cylinder
    for (int i = 0; i < 3; ++i) editor.frame(width, height, 1.0f, idle);
    snap("02-two-bodies");
    // Save and re-open through the core's own codec: the same bytes come back.
    editor.saveProject();
    check("save_wrote_a_forge_document", platform.lastSaved.size() > 16
                                             && platform.lastSaved[0] == 'F');
    const std::vector<uint8_t> saved = platform.lastSaved;
    editor.openProjectBytes(saved, "headless.forge");
    editor.saveProject();
    check("reopen_encodes_identically", platform.lastSaved == saved);

    // The mouse, through the very path a Windows mouse takes. A left drag on
    // empty viewport orbits; the wheel zooms in about the pointer.
    const auto frameWith = [&](const fsui::InputState& in) { editor.frame(width, height, 1.0f, in); };
    const auto mouse = [](float x, float y) {
        fsui::InputState in;
        in.mouseX = x;
        in.mouseY = y;
        return in;
    };
    {
        const float yaw = forgeshape::editorCamera().yaw();
        fsui::InputState in = mouse(800, 700);
        in.mousePressed[0] = in.mouseDown[0] = true;
        frameWith(in);
        for (int i = 1; i <= 6; ++i) {
            in = mouse(800 + 20.0f * i, 700);
            in.mouseDown[0] = true;
            frameWith(in);
        }
        in = mouse(920, 700);
        in.mouseReleased[0] = true;
        frameWith(in);
        check("a_left_drag_orbits", forgeshape::editorCamera().yaw() != yaw);
        const float distance = forgeshape::editorCamera().distance();
        in = mouse(800, 450);
        in.wheel = 1.0f;
        frameWith(in);
        check("the_wheel_zooms_in", forgeshape::editorCamera().distance() < distance);
    }
    // The Move handles: find the X axis under the cursor, drag it, and the
    // body moves along X as ONE Undo step.
    {
        editor.setTransformMode(0);
        frameWith(fsui::InputState());
        const forgeshape::CameraSnapshot camera = forgeshape::editorCamera().snapshot();
        float hx = -1, hy = -1;
        for (int y = 150; y < 750 && hx < 0; y += 3) {
            for (int x = 320; x < 1280 && hx < 0; x += 3) {
                if (forgeshape::gizmoSession().hitTest(camera, static_cast<float>(x), static_cast<float>(y),
                                                        width, height) == forgeshape::GizmoHandle::AxisX) {
                    hx = static_cast<float>(x);
                    hy = static_cast<float>(y);
                }
            }
        }
        check("the_move_handles_stand_over_the_body", hx >= 0);
        const double before = forgeshape::constructionScene().activeBody().transform().values().positionX;
        const size_t undo = forgeshape::constructionHistory().undoDepth();
        fsui::InputState in = mouse(hx, hy);
        in.mousePressed[0] = in.mouseDown[0] = true;
        frameWith(in);
        for (int i = 1; i <= 5; ++i) {
            in = mouse(hx + 18.0f * i, hy);
            in.mouseDown[0] = true;
            frameWith(in);
        }
        in = mouse(hx + 90.0f, hy);
        in.mouseReleased[0] = true;
        frameWith(in);
        const double after = forgeshape::constructionScene().activeBody().transform().values().positionX;
        check("dragging_the_x_handle_moves_the_body_along_x", after != before);
        check("one_drag_is_one_undo_step", forgeshape::constructionHistory().undoDepth() == undo + 1);
        editor.undo();
        check("undo_puts_it_back",
              forgeshape::constructionScene().activeBody().transform().values().positionX == before);
        editor.setTransformMode(-1);
    }

    // --- Sketch and Extrude with the mouse (`DESKTOP-R2`) -----------------
    const auto drag = [&](float x0, float y0, float x1, float y1) {
        fsui::InputState in = mouse(x0, y0);
        in.mousePressed[0] = in.mouseDown[0] = true;
        frameWith(in);
        for (int i = 1; i <= 6; ++i) {
            in = mouse(x0 + (x1 - x0) * i / 6.0f, y0 + (y1 - y0) * i / 6.0f);
            in.mouseDown[0] = true;
            frameWith(in);
        }
        in = mouse(x1, y1);
        in.mouseReleased[0] = true;
        frameWith(in);
    };
    const auto click = [&](float x, float y) {
        fsui::InputState in = mouse(x, y);
        frameWith(in);  // the pointer arrives (a chooser highlights)
        in.mousePressed[0] = in.mouseDown[0] = true;
        frameWith(in);
        in = mouse(x, y);
        in.mouseReleased[0] = true;
        frameWith(in);
    };
    // The CAD bootstrap: Close, then New CAD part is ONE sketch over an empty
    // scene; its first Extrude creates the project.
    editor.closeProject();
    frameWith(idle);
    editor.newSketch();
    frameWith(idle);
    check("new_cad_part_opens_a_sketch_over_an_empty_scene",
          forgeshape::sketchSession().active() && !forgeshape::constructionScene().hasProject());
    // The ribbon gives the Model tab up to the Sketch tab while drawing.
    check("a_sketch_brings_the_sketch_tab_and_its_tools",
          editor.widgetRect("ribbon_tab_Sketch", nullptr) &&
              editor.widgetRect("sk_tool_3", nullptr) && !editor.widgetRect("rb_add_0", nullptr));
    // An empty sketch has nothing to finish, so Finish is not drawn yet.
    check("finish_is_withdrawn_while_the_sketch_is_empty", !editor.widgetRect("sk_finish", nullptr));
    editor.setSketchTool(forgeshape::sketchToolIndex(forgeshape::SketchTool::Line));
    drag(640, 300, 700, 300);
    const size_t afterLine = forgeshape::sketchSession().sketch().entities.size();
    editor.undo();
    check("a_mouse_drag_draws_a_line_and_undo_takes_it_back",
          afterLine == 1 && forgeshape::sketchSession().sketch().entities.empty());
    editor.setSketchTool(forgeshape::sketchToolIndex(forgeshape::SketchTool::Rectangle));
    drag(700, 330, 900, 530);
    check("a_mouse_drag_draws_a_rectangle", forgeshape::sketchSession().sketch().entities.size() == 1);
    frameWith(idle);
    check("finish_appears_once_there_is_a_curve", editor.widgetRect("sk_finish", nullptr));
    snap("03-sketch-rectangle");
    editor.finishSketch();
    frameWith(idle);
    snap("04-sketch-ready");
    check("finish_sketch_chooses_the_one_region",
          forgeshape::sketchSession().state() == forgeshape::SketchSessionState::Ready &&
              forgeshape::sketchSession().selectionChosen());
    editor.commitSketch();
    for (int i = 0; i < 3; ++i) frameWith(idle);
    forgeshape::MassProperties block;
    const bool measuredBlock = measureActive(&block);
    check("extrude_creates_the_project_with_one_cad_part",
          forgeshape::constructionScene().hasProject() &&
              forgeshape::constructionScene().bodyCount() == 1 &&
              forgeshape::constructionScene().activeBody().cadOrNull() != nullptr &&
              measuredBlock && block.closed && block.volume > 0.0);
    check("a_new_project_starts_with_an_empty_history",
          forgeshape::constructionHistory().undoDepth() == 0);

    // A sketch on the part's front face, a circle, and a Cut of the SAME body.
    editor.newSketch();
    frameWith(idle);
    check("new_sketch_in_a_project_opens_the_support_chooser", forgeshape::supportChooser().active());
    const forgeshape::Vec3 front{static_cast<float>((block.boundsMin.x + block.boundsMax.x) * 0.5),
                                 static_cast<float>((block.boundsMin.y + block.boundsMax.y) * 0.5),
                                 static_cast<float>(block.boundsMax.z)};
    float fx = -1, fy = -1;
    const bool frontShows = screenOf(front, width, height, &fx, &fy);
    click(fx, fy);
    check("one_click_on_a_face_begins_a_sketch_on_it",
          frontShows && forgeshape::sketchSession().active() &&
              forgeshape::sketchSession().operationAvailable(forgeshape::CadFeatureOperation::Cut));
    const double radius = 0.2 * std::min(block.boundsMax.x - block.boundsMin.x,
                                         block.boundsMax.y - block.boundsMin.y);
    float cx = -1, cy = -1, ex = -1, ey = -1;
    screenOf(front, width, height, &cx, &cy);
    screenOf({front.x + static_cast<float>(radius), front.y, front.z}, width, height, &ex, &ey);
    editor.setSketchTool(forgeshape::sketchToolIndex(forgeshape::SketchTool::Circle));
    drag(cx, cy, ex, ey);
    editor.finishSketch();
    frameWith(idle);
    {
        std::lock_guard<std::mutex> lock(forgeshape::editorStateMutex());
        forgeshape::SketchSession& sketch = forgeshape::sketchSession();
        sketch.setOperation(forgeshape::CadFeatureOperation::Cut);
        sketch.setExtrude(0.5 * (block.boundsMax.z - block.boundsMin.z), sketch.extrude().direction);
    }
    for (int i = 0; i < 3; ++i) frameWith(idle);
    editor.requestCapture();
    for (int i = 0; i < 4 && !editor.captureReady(); ++i) frameWith(idle);
    if (editor.captureReady()) {
        uint32_t cw = 0, ch = 0;
        const std::vector<uint8_t>& shot = editor.capture(&cw, &ch);
        const std::string cutPreview = gallery + "05-face-cut-preview.png";
        fsdesktop::writePng(cutPreview, shot.data(), cw, ch, 4);
    }
    const size_t undoBeforeCut = forgeshape::constructionHistory().undoDepth();
    editor.commitSketch();
    for (int i = 0; i < 3; ++i) frameWith(idle);
    forgeshape::MassProperties pocketed;
    const bool measuredPocket = measureActive(&pocketed);
    const forgeshape::CadBody* cad = forgeshape::constructionScene().activeBody().cadOrNull();
    check("the_cut_removes_material_from_the_same_body",
          forgeshape::constructionScene().bodyCount() == 1 && cad != nullptr &&
              cad->state().laterFeatures.size() == 1 && measuredPocket &&
              pocketed.volume < block.volume - 1e-9);
    check("the_cut_is_one_undo_step",
          forgeshape::constructionHistory().undoDepth() == undoBeforeCut + 1);
    check("the_sketch_view_is_given_back", !forgeshape::sketchSession().active());
    snap("06-part-with-cut");

    // --- the ribbon, the view cube, the view bar, the History strip ------
    // (`WORKBENCH-RIBBON-R1`), every control found by its semantic id.
    const auto clickId = [&](const char* id) {
        // A fresh frame first: a control drawn before the act that moved it
        // stood elsewhere one frame ago.
        frameWith(idle);
        fsui::Rect r;
        if (!editor.widgetRect(id, &r)) return false;
        click(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
        return true;
    };
    frameWith(idle);
    check("the_model_ribbon_is_back_after_the_sketch",
          editor.widgetRect("rb_new_sketch", nullptr) && editor.widgetRect("rb_add_0", nullptr));
    const bool viewTab = clickId("ribbon_tab_View");
    // One home per act (`WORKBENCH-UX-R1`): the views live on the view bar,
    // the View tab keeps only what to show.
    check("the_view_tab_carries_show_and_no_second_set_of_views",
          viewTab && editor.widgetRect("rb_grid", nullptr) && !editor.widgetRect("rb_view_top", nullptr));
    clickId("ribbon_tab_Model");
    const bool topClicked = clickId("vb_top");
    check("the_view_bar_top_command_looks_straight_down",
          topClicked && forgeshape::editorCamera().alignedViewActive());
    const auto eyeDirection = []() {
        const forgeshape::CameraSnapshot snap = forgeshape::editorCamera().snapshot();
        return forgeshape::vec3Normalize(forgeshape::vec3Sub(snap.eye, snap.target));
    };
    const bool frontClicked = clickId("cube_+z");
    check("the_view_cube_z_end_is_the_front_view",
          frontClicked && eyeDirection().z > 0.9999f);
    const bool rightClicked = clickId("cube_+x");
    check("the_view_cube_x_end_is_the_right_view", rightClicked && eyeDirection().x > 0.9999f);
    clickId("vb_home");
    // Zoom far in, then Fit: the whole part is back on screen.
    {
        fsui::InputState in = mouse(800, 450);
        in.wheel = 12.0f;
        frameWith(in);
    }
    const bool fitClicked = clickId("vb_fit");
    frameWith(idle);
    bool allInside = fitClicked;
    {
        forgeshape::MassProperties now;
        measureActive(&now);
        for (int corner = 0; corner < 8; ++corner) {
            const forgeshape::Vec3 p{
                static_cast<float>((corner & 1) ? now.boundsMax.x : now.boundsMin.x),
                static_cast<float>((corner & 2) ? now.boundsMax.y : now.boundsMin.y),
                static_cast<float>((corner & 4) ? now.boundsMax.z : now.boundsMin.z)};
            float sx = -1, sy = -1;
            allInside = allInside && screenOf(p, width, height, &sx, &sy) && sx > 0 && sx < width &&
                        sy > 0 && sy < height;
        }
    }
    check("fit_frames_the_whole_part", allInside);
    // The History strip's back and forward arrows are Undo and Redo.
    const size_t depth = forgeshape::constructionHistory().undoDepth();
    const bool back = clickId("hist_back");
    for (int i = 0; i < 2; ++i) frameWith(idle);
    forgeshape::MassProperties undone;
    measureActive(&undone);
    check("history_back_undoes_the_cut",
          back && forgeshape::constructionHistory().undoDepth() == depth - 1 &&
              undone.volume > pocketed.volume + 1e-9);
    const bool forward = clickId("hist_forward");
    for (int i = 0; i < 2; ++i) frameWith(idle);
    forgeshape::MassProperties redone;
    measureActive(&redone);
    check("history_forward_redoes_it",
          forward && forgeshape::constructionHistory().undoDepth() == depth &&
              std::abs(redone.volume - pocketed.volume) < 1e-9);
    check("undo_and_redo_have_one_home", !editor.widgetRect("tb_undo", nullptr) &&
                                             editor.widgetRect("hist_back", nullptr) &&
                                             !editor.widgetRect("cube_home", nullptr) &&
                                             !editor.widgetRect("disp_grid", nullptr));
    // A field draws without binary noise, and focusing then leaving one
    // writes nothing.
    check("a_field_draws_twelve_significant_digits",
          fsui::Ui::formatFieldNumber(1.0000000000000002) == "1" &&
              fsui::Ui::formatFieldNumber(0.0004) == "0.0004" &&
              fsui::Ui::formatFieldNumber(-0.0) == "0" &&
              fsui::Ui::formatFieldNumber(-98765.4321098) == "-98765.4321098" &&
              fsui::Ui::formatFieldNumber(1.0e-7) == "0.0000001" &&
              fsui::Ui::formatFieldNumber(2500000.0) == "2500000");
    {
        frameWith(idle);
        const size_t depthBefore = forgeshape::constructionHistory().undoDepth();
        fsui::Rect field;
        const std::string fieldId =
            "tf_" + std::to_string(forgeshape::constructionScene().activeBodyId()) + "_0";
        const bool shown = editor.widgetRect(fieldId, &field) ||
                           (frameWith(idle), editor.widgetRect(fieldId, &field));
        if (shown) {
            click(field.x + field.w * 0.5f, field.y + field.h * 0.5f);
            fsui::InputState tab;
            tab.keys.push_back(fsui::Key::Tab);
            frameWith(tab);
            frameWith(idle);
        }
        check("an_untouched_field_writes_nothing",
              forgeshape::constructionHistory().undoDepth() == depthBefore);
    }
    clickId("ribbon_tab_View");
    snap("07-view-ribbon");
    clickId("ribbon_tab_Model");
    clickId("vb_home");

    editor.requestCapture();
    for (int i = 0; i < 4 && !editor.captureReady(); ++i) editor.frame(width, height, 1.0f, idle);
    check("a_frame_was_read_back", editor.captureReady());
    if (!editor.captureReady()) {
        std::printf("FORGESHAPE_DESKTOP_HEADLESS_FAIL:no_capture\n");
        return 1;
    }
    uint32_t w = 0, h = 0;
    const std::vector<uint8_t>& rgba = editor.capture(&w, &h);
    check("capture_is_window_sized", w == width && h == height);
    fsdesktop::writePng(out, rgba.data(), w, h, 4);
    // The toolbar is the chrome surface, the model is drawn in the middle, and
    // the floating panels stand at both edges.
    const Pixel toolbar = at(rgba, w, w / 2, 6);
    check("toolbar_is_the_chrome_surface", near(toolbar, 0x3A, 0x38, 0x35, 6));
    int modelPixels = 0;
    for (uint32_t y = h / 3; y < 2 * h / 3; y += 4) {
        for (uint32_t x = w / 3; x < 2 * w / 3; x += 4) {
            const Pixel p = at(rgba, w, x, y);
            if (!near(p, 0x30, 0x2E, 0x2B, 10)) ++modelPixels;
        }
    }
    check("the_model_is_drawn_in_the_viewport", modelPixels > 200);
    const Pixel browser = at(rgba, w, 120, 300);
    // Below the panel's last section, where nothing but the panel stands.
    const Pixel properties = at(rgba, w, w - 120, h - 140);
    check("the_browser_panel_stands_on_the_left", near(browser, 0x46, 0x43, 0x3F, 12));
    check("the_properties_panel_stands_on_the_right", near(properties, 0x46, 0x43, 0x3F, 12));
    editor.stop();
    std::printf("%s (%d failed) %s\n",
                failures ? "FORGESHAPE_DESKTOP_HEADLESS_FAIL" : "FORGESHAPE_DESKTOP_HEADLESS_OK",
                failures, out.c_str());
    return failures ? 1 : 0;
}
