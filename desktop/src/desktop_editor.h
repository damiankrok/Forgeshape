// The Windows desktop application's editor (`DESKTOP-R1`): ForgeShape's own
// interface (ui/), the shared renderer and the shared core, with no platform
// type in it. The platform shell (main_win32.cpp, or main_headless.cpp for the
// automated check) owns the window, feeds input samples and answers the few
// questions only a platform can: which file the user picked, where settings
// live.
//
// What it changes is project truth only through the core's own acts -- the
// same functions the Android JNI calls: loadProjectDocument, encodeProjectV1,
// exportSceneAsGlb, applySceneBodyTransform, applyConstructionPrimitive,
// convertPrimitiveToCadPart, deleteSceneBody, createFreeformBody and the one
// ConstructionHistory -- so a project saved here opens on a phone and the
// reverse, byte for byte.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "forgeshape_render_surface.h"
#include "ui/atlas.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace forgeshape {
struct CadTimelineRow;
class Renderer;
}

namespace fsdesktop {

// What only the platform can do. Every call is synchronous and may show a
// native dialog.
class DesktopPlatform {
public:
    virtual ~DesktopPlatform() = default;
    // A file the user chose to open; false when cancelled or unreadable.
    virtual bool openFile(const char* filterName, const char* extension,
                          std::vector<uint8_t>* bytes, std::string* displayName) = 0;
    // Writes `bytes` to a file the user chose; `suggested` is a file name.
    virtual bool saveFile(const char* filterName, const char* extension,
                          const std::string& suggested, const std::vector<uint8_t>& bytes,
                          std::string* displayName) = 0;
    virtual void setTitle(const std::string& utf8) = 0;
    virtual void requestQuit() = 0;
    // Where the per-user theme file lives (may be empty).
    virtual std::string settingsDirectory() = 0;
    // The project has changes no Save holds: 0 cancel, 1 save, 2 discard.
    virtual int askSaveChanges() { return 2; }
};

class DesktopEditor {
public:
    DesktopEditor();
    ~DesktopEditor();

    // Creates the renderer for `window`; false when Vulkan cannot present.
    bool start(forgeshape::PlatformWindow window, DesktopPlatform* platform,
               const std::vector<std::string>& fontCandidates,
               const std::vector<std::string>& boldFontCandidates);
    void stop();

    // The window's size changed (pixels).
    void resized();

    // One frame: lays out and draws the interface from `input`, routes what it
    // did not take to the viewport, and presents. `scale` is the window's DPI
    // scale (1.0 at 96 dpi).
    bool frame(int width, int height, float scale, const fsui::InputState& input);

    // Keyboard shortcuts the platform recognised (Ctrl+Z, ...); false when the
    // interface is typing into a field and the key is the field's.
    bool shortcut(char key, bool ctrl, bool shift);

    // The headless check and "Save view as image": the next frame read back.
    void requestCapture();
    bool captureReady() const;
    const std::vector<uint8_t>& capture(uint32_t* width, uint32_t* height) const;

    // Acts, also reachable from the interface.
    void newCadPart();
    void newFreeform();
    void openProject();
    void openProjectBytes(const std::vector<uint8_t>& bytes, const std::string& name);
    void saveProject();
    void exportGlb();
    void undo();
    void redo();
    void addPrimitive(int kind);
    void makeCadPart();
    void deleteActive();
    void closeProject();
    void reloadTheme();
    // Move, Rotate or Scale (0..2), or -1 to put the transform handles away.
    void setTransformMode(int mode);

    // Sketch and Extrude (`DESKTOP-R2`), through the core's one sketch path
    // (`forgeshape_editor_core.h`): the same begin, view, touch, Finish and
    // commit the Android shell takes.
    // With no project: the CAD bootstrap, a sketch on XY whose first Extrude
    // creates the project. Inside one: the spatial support chooser.
    void newSketch();
    // A sketch on a world plane by name (0 XY, 1 XZ, 2 YZ).
    void sketchOnPlane(int plane);
    void setSketchTool(int tool);
    void finishSketch();
    void backToSketch();
    // Extrude: the one commit, one Undo.
    void commitSketch();
    void cancelSketch();
    // Opens the staged edit a History row stands for (`WORKBENCH-UX-R1`): a
    // sketch row its sketch, a feature row its extrusion. True when opened.
    bool editHistoryRow(const forgeshape::CadTimelineRow& row);

    // The view (`WORKBENCH-RIBBON-R1`): a standard view by index
    // (`forgeshape::StandardView`), and Fit, framing everything drawn.
    void setStandardView(int view);
    void fitAll();

    // Where a control stood in the last frame, by its semantic id; the
    // headless check finds controls this way and never by a coordinate.
    bool widgetRect(const std::string& id, fsui::Rect* out) const;

    const std::string& status() const { return status_; }

    // Whether the open project differs from what the last New, Open or Save
    // left, by the core's own semantic fingerprint (the Android rule).
    bool hasUnsavedChanges() const;
    // Asks the platform what to do with unsaved changes before the project is
    // replaced or the window closes; false when the user cancelled.
    bool settleUnsavedChanges();

private:
    void buildInterface(int width, int height);
    void buildHome(int width, int height);
    void buildTitleBar(const fsui::Rect& bar);
    void buildRibbon(const fsui::Rect& bar);
    void buildModelRibbon(const fsui::Rect& area);
    void buildViewRibbon(const fsui::Rect& area);
    void buildHistoryStrip(const fsui::Rect& bar);
    void buildViewCube(const fsui::Rect& viewport);
    void buildViewBar(const fsui::Rect& viewport);
    void buildBrowser(const fsui::Rect& panel);
    void buildProperties(const fsui::Rect& panel);
    void buildStatusBar(const fsui::Rect& bar);
    void buildSketchRibbon(const fsui::Rect& area);
    void buildSketchProperties(const fsui::Rect& panel);
    void buildChooserProperties(const fsui::Rect& panel);
    // Routes the mouse while a sketch or the support chooser owns the
    // viewport; false when neither does.
    bool routeSketchInput(int width, int height, bool overUi);
    void sketchKeys();
    void routeViewportInput(int width, int height);
    void report(const std::string& message, bool error = false);
    void refreshTitle();
    std::string bodyLabel(uint64_t objectId) const;

    std::unique_ptr<forgeshape::Renderer> renderer_;
    DesktopPlatform* platform_ = nullptr;
    fsui::Theme theme_;
    fsui::Atlas atlas_;
    std::unique_ptr<fsui::Ui> ui_;
    fsui::InputState input_;
    float scale_ = 1.0f;

    // Viewport navigation state (presentation only).
    int dragButton_ = -1;
    // The context menu (`WORKBENCH-UX-R1`): a still right click on a body
    // opens its acts at the pointer. Presentation; it closes on any act, on
    // Esc and on a click elsewhere.
    bool contextOpen_ = false;
    float contextX_ = 0, contextY_ = 0;
    uint64_t contextBody_ = 0;
    void buildContextMenu(int width, int height);
    // The live size of a shape being drawn, beside the pointer.
    void buildLivePlacementLabel(int width, int height);
public:
    // The last live size label drawn, for the headless check.
    const std::string& liveLabelForTest() const { return liveLabel_; }
private:
    std::string liveLabel_;
    float dragLastX_ = 0, dragLastY_ = 0;
    float pressX_ = 0, pressY_ = 0;
    bool dragTravelled_ = false;
    // A transform handle captured by the left button (the gizmo's own drag).
    bool gizmoDragging_ = false;
    // The left button inside a sketch: one captured pointer, Down to Up.
    bool sketchPointer_ = false;
    float sketchLastX_ = 0, sketchLastY_ = 0;
    fsui::Rect viewportRect_;

    // The ribbon's chosen tab: 0 Model, 1 View, 2 Sketch (while one is open).
    int ribbonTab_ = 0;
    bool browserOpen_ = true;
    bool propertiesOpen_ = true;
    bool homeVisible_ = true;
    std::string projectName_;
    std::string status_;
    bool statusError_ = false;
    bool captureRequested_ = false;
    uint64_t savedFingerprint_ = 0;
    void noteSaved();
};

}  // namespace fsdesktop
