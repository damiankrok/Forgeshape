# Desktop and tablet plan

OWNER decision, 2026-11-04. Two facts change, and they change together:

1. **ForgeShape gets a Windows desktop application** — an installable,
   updatable `.exe` built from the SAME C++ core as the Android app, developed
   alongside it rather than after it. A project is one `.forge` document either
   app opens, so a later cloud sync moves one file and needs no second format.
2. **A large Android window gets a desktop-class workbench** — a Samsung Galaxy
   Tab S9 Ultra in landscape, or Samsung DeX, keeps its Browser (the scene and
   the active body's History) and its Properties panel docked at rest, each
   collapsible, with a mouse and a keyboard working as they do on a PC. The
   phone layout does not change.

The look follows the owner's references (SolidCraft, Shapr3D): a calm dark
workspace, the model in the middle, a feature tree on the left, the exact
values on the right, a command row on top. ForgeShape keeps its own palettes,
icons and vocabulary; nothing is copied from either product.

## Tablet workbench (`TABLET-WORKBENCH-R1`)

- **When.** `WorkspaceLayoutMode.EXPANDED` (window ≥ 840 dp wide and ≥ 480 dp
  tall) AND the window affords the Browser column with a 480 dp model between
  the columns (`WorkspaceLayoutMode.objectsDocked`). The decision stays window
  arithmetic, so a DeX window resized by the user crosses it like a rotation.
- **Browser (leading edge).** The existing docked Objects column grows into
  the Browser: Objects on top, and under it the active CAD or Surface body's
  History — the SAME `FeatureHistoryView` the History control opens on a
  phone, re-parented rather than copied. It is drawn exactly when the History
  control would be (a body with a chain, nothing sketched or sculpted), and an
  act chosen in it runs the one existing path.
- **Properties (trailing edge).** The precision surface is docked OPEN at rest
  beside the Tool Rail. Opening another surface does not dismiss it.
- **Collapse.** Each panel collapses with one control and comes back with one
  control; the choice is remembered for the session (presentation, never
  project truth, never an `AppPreferences` field).
- **Mouse and keyboard** (every window, not only the workbench). The wheel
  zooms about the pointer, the right button orbits, the middle button (or
  Shift with the right) pans, the left button keeps the one-finger meaning,
  and the hardware keys answer: Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z (Undo / Redo),
  Ctrl+S (Save), Delete (the active body), Esc (the topmost surface, one step
  as Back is). A key is one more caller of the ONE existing handler for its
  act, and a focused text field keeps its own keys.

## Windows desktop (`DESKTOP-R1` and after)

**One core.** The desktop target compiles the platform-neutral sources in
`app/src/main/cpp` unchanged; nothing is forked. What JNI does today as glue
moves, act by act, into a platform-neutral editor API that both shells call —
`forgeshape_editor_core.{h,cpp}` — so a rule is never implemented twice.

**The renderer's one platform seam.** `forgeshape_log.h` replaces the direct
`android/log.h` calls, and `forgeshape_render_surface.h` names the window a
swapchain is made for: an `ANativeWindow` on Android, an `HWND` on Windows, a
headless surface (`VK_EXT_headless_surface`) for automated checks. The Android
build is byte-for-byte the same Vulkan path.

**The desktop shell** (`desktop/`):

- a Win32 window, a Vulkan surface through the seam, a frame loop that draws
  the same `composeViewportFrame` the Android render thread draws;
- mouse navigation (the tablet's: right orbit, middle pan, wheel zoom about
  the pointer, the left button selecting and drawing), click selection through the same picking, the Windows keyboard
  shortcuts above, native Open / Save As / Export dialogs (`.forge`, `.glb`);
- **ForgeShape's own UI kit**, drawn by the renderer as screen-space quads
  from one atlas — no third-party UI library. Theme tokens are the same roles
  the Android `attrs.xml` names, the five palettes included, and a user theme
  file (`%APPDATA%\ForgeShape\theme.json`, read by the project's own JSON
  reader) can override any colour, size or radius, so the owner can restyle
  the application without a rebuild. Glyphs are rasterised by the platform's
  font API (GDI) into the atlas; icons are rasterised by ForgeShape's own path
  rasteriser from the Android vector drawables' `pathData`, so an icon has one
  source for both platforms.

**Installer and updates.** An Inno Setup script (a build-time tool, never part
of the product) packages `ForgeShape-Setup-<version>.exe`: a per-user install
under `%LOCALAPPDATA%\Programs\ForgeShape`, a Start-menu entry, an uninstaller,
and `.forge` registered to open in ForgeShape. A newer setup with the same
`AppId` installs over the old one, which is the update path; an in-app update
check needs a network permission the product deliberately does not hold and is
a later, separate decision.

**Continuous integration** (`.github/workflows/ci-desktop.yml`): a Windows job
builds the core and the shell with MSVC, runs every host self-test suite on
MSVC, and uploads the installer; a Linux job builds the shell's renderer path
headless against Mesa's software Vulkan driver and checks a drawn frame, which
is the runtime evidence a hosted runner without a GPU can give. Neither closes
a physical-hardware gate: a real Windows GPU, a pen and the owner's own PC are
UNVERIFIED until the owner runs the build.

**Order.**

| Stage | Delivers |
| --- | --- |
| `DESKTOP-R1` ✅ | window, renderer seam, navigation, selection, the UI kit's shell (top command row, Browser, Properties, status), New / Open / Save / Save As / Export GLB, primitives, Undo / Redo, Delete, Display settings, theme file, installer, CI |
| `DESKTOP-R2` ✅ | Sketch and Extrude (New Body / Add / Cut / Intersect) with the mouse. `SketchSession` already takes platform-neutral touch samples (`onTouch`, `toggleRegionAt`, `finish`, `commitSketch`); what moves out of JNI into the editor core is the sketch VIEW (`beginSketchView` / `endSketchView`, the feature view after Finish) and the first-project commit, so both shells frame and commit a sketch through one path |
| `WORKBENCH-RIBBON-R1` ✅ | the SolidCraft arrangement on both shells (OWNER 2026-11-04, after comparing the two): standard views and Fit in the core; on the desktop a title row, a ribbon of captioned commands in tabs and named groups, a History strip along the bottom whose back and forward are Undo and Redo (no rollback marker, by the owner's choice), a view cube and a view bar; on a tablet workbench the view cube, the view bar and the History strip |
| `WORKBENCH-UX-R1` ✅ | the look and function audited, researched and put to a design council, then five iterations on both shells (`artifacts/workbench-ux-r1/`): docked panels and one home per act, one selected treatment, noise-free fields; multisampling and feature edges; History steps that open their staged edit, sentences for every CAD refusal, the desktop's right-click menu and tool letters; live sizes while drawing, step glyphs, palm cancel; a view cube that reaches every view by tap, drag and TalkBack, an Isometric view, the desktop's full-window Home, project menu and keyboard sheet, tooltips on every icon-only Android control |
| `WORKBENCH-RIBBON-R2` ✅ | the tablet's ribbon (`WorkbenchRibbonView`): the workbench's captioned command row under the Global Toolbar in the desktop's tabs and groups, the face actions (Sketch here, Round, Bevel, Hole, Pattern, Mirror) in its contextual group with the docked face column standing down; the `Layout` preference (Automatic / Phone / Workbench) so a tablet can take the phone composition and a medium window the workbench; Export as… moved into the project drawer; captions under every trailing-host entry; representation-named default body labels from the core on both shells |
| `DESKTOP-R3` | every remaining CAD act (finishes, holes, patterns, revolve, measure, History editing), then Freeform, Surface and Sculpt with a mouse or pen |
| later | cloud: the same `.forge` document opened from a shared location by either application |
