// The desktop shell's automated check (`DESKTOP-R1`): the whole desktop editor
// -- the shared core, the shared renderer and ForgeShape's own interface --
// driven headless on a Linux runner through Mesa's software Vulkan driver, a
// frame read back and checked, and written as evidence.
//
// It proves the renderer's desktop path and the interface draw; it does not
// prove a Windows GPU, a real window or the owner's PC, which stay UNVERIFIED
// until the owner runs the installer.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "desktop_editor.h"
#include "forgeshape_render_surface.h"
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
    // Home first: one frame, then a CAD part, a cylinder beside it, a frame.
    for (int i = 0; i < 2; ++i) editor.frame(width, height, 1.0f, idle);
    editor.newCadPart();
    editor.addPrimitive(1);  // a cylinder
    for (int i = 0; i < 3; ++i) editor.frame(width, height, 1.0f, idle);
    // Save and re-open through the core's own codec: the same bytes come back.
    editor.saveProject();
    check("save_wrote_a_forge_document", platform.lastSaved.size() > 16
                                             && platform.lastSaved[0] == 'F');
    const std::vector<uint8_t> saved = platform.lastSaved;
    editor.openProjectBytes(saved, "headless.forge");
    editor.saveProject();
    check("reopen_encodes_identically", platform.lastSaved == saved);

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
    const Pixel properties = at(rgba, w, w - 120, 300);
    check("the_browser_panel_stands_on_the_left", near(browser, 0x46, 0x43, 0x3F, 12));
    check("the_properties_panel_stands_on_the_right", near(properties, 0x46, 0x43, 0x3F, 12));
    editor.stop();
    std::printf("%s (%d failed) %s\n",
                failures ? "FORGESHAPE_DESKTOP_HEADLESS_FAIL" : "FORGESHAPE_DESKTOP_HEADLESS_OK",
                failures, out.c_str());
    return failures ? 1 : 0;
}
