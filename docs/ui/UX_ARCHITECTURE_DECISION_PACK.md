# ForgeShape — UI/UX Architecture Decision Pack

**Stage:** 015A — UI/UX Audit + Architecture Decision Pack
**Status:** proposal for owner review. **Nothing here is implemented.**
**Baseline:** PROJECT_STATUS v0.15.0, Gate P0 COMPLETE, NDK r29, HEAD `35b2866`.

This document proposes. It does not describe the product. Every behaviour named
in the future tense is unbuilt, and `PRODUCT.md` remains the only place that says
what ForgeShape actually does.

---

## 1. Current UI audit

Audited from live code (`ForgeShapeActivity`, `ForgeShapeSurfaceView`,
`ConstructionPanelView`, `SculptPanelView`, `AndroidManifest.xml`) and from
`uiautomator` hierarchy dumps taken on `emulator-5558` at three window sizes.
No product behaviour was changed to perform the audit.

### 1.1 Measured facts

| | portrait 1080×2400 (411×914 dp) | landscape 2400×1080 (914×411 dp) | compact 720×1280 @320 (360×640 dp) |
| --- | --- | --- | --- |
| `SurfaceView` bounds | `[0,0][1080,2400]` | `[0,0][2400,1080]` | `[0,0][720,1280]` |
| Construction panel bounds | `[0,0][1080,1175]` | `[0,0][2400,1080]` | `[0,0][720,~880]` |
| **viewport left unoccluded** | **51 %** | **0 %** | ~31 % |
| status line | visible | **clipped off-screen** | visible |

With the IME raised in portrait, the panel holds 1175 px and the keyboard takes
from ≈1520 px, leaving a **≈345 px (14 %) horizontal slit** of live viewport.

Android layer totals 2223 lines of Java, of which `ConstructionPanelView` is
**1041** — 47 % of the whole shell in one class.

### 1.2 Top issues, worst first

**A1 — Landscape is unusable, and it is a hard failure, not a degradation.**
The panel is `WRAP_CONTENT` at `Gravity.TOP` inside a `FrameLayout` with no
scroll container and no height ceiling. In landscape it measures to the full
2400×1080 window: the model is completely occluded, and the status line — the
*only* channel for validation messages and for the stale-source warning — is
laid out below the window bottom and is unreachable. There is no gesture that
recovers it, because there is no `ScrollView` anywhere in the Android layer.

**A2 — The Activity cannot react to configuration at all.**
`android:configChanges="orientation|screenSize|keyboardHidden|screenLayout|density"`
absorbs every relevant change, and there is no `onConfigurationChanged`
override. `syncMode()` runs only in `onCreate` and `onResume`. So a rotation or a
split-window resize re-measures the existing views and nothing else — no layout
decision is ever re-made. Grep counts across the Android layer:
`onConfigurationChanged` 0, `getResources().getConfiguration` 0,
`ORIENTATION_LANDSCAPE` 0, `screenWidthDp` 0. **There is no breakpoint logic to
fix; there is none.**

**A3 — No window-inset handling whatsoever.**
`WindowInsets` 0, `setOnApplyWindowInsetsListener` 0. The panel is flush at
`y = 0`, with the first control (`Shape` label) at `y = 21`. This survives today
only because `Theme.Material.NoActionBar.Fullscreen` hides the system bars on
this emulator. On a device with a camera cutout or with system bars shown, the
shape selector sits underneath them. `targetSdk` is 36, where edge-to-edge is the
platform default, so this is a latent collision rather than a hypothetical one.

**A4 — The property slab does not scale, and it is already at its limit.**
Five primitives already forced the shape selector onto its own full-width row
(Stage 014) to avoid clipping the fifth. The panel currently shows, always and
all at once: 5 primitive radios, up to 3 dimension fields, 3 unit radios, Apply
Shape, 3 position fields, 3 rotation fields, Apply Transform, Freeze, Resume,
status. Adding Plane, then a UV section, then Export, then an outliner has no
room to go. In landscape each of the three field columns is ~760 px wide to hold
the string `2`.

**A5 — Two modes, two whole panels, and no shared shell.**
`ConstructionPanelView` and `SculptPanelView` are independent `LinearLayout`
subclasses that duplicate their own colour constants, `dp()`, background
builders, status-line handling and touch-swallowing. Mode switching is
`setVisibility(GONE/VISIBLE)` on two unrelated trees. A third mode (UV) or a
fourth (Export) means a third and fourth copy.

**A6 — Freeze / Resume Sculpt are undiscoverable and unguarded.**
Both are plain buttons at the bottom of a long slab, visually identical to
*Apply Transform*. Yet **re-Freezing silently discards every sculpted vertex** —
the single most destructive act in the product — and there is no confirmation, no
consequence statement and no undo. `Resume Sculpt` appears only after a first
Freeze, so the control set changes shape with hidden state.

**A7 — No stable identifiers for testing.**
`setId` appears twice in the entire Android layer, and both are `RadioGroup`
internals. Every runtime verification in Stages 007–014 and Gate P0 was driven by
**screen coordinates**, which is why `README.md` carries a table of pixel
positions that must be rewritten whenever a row is added — it already was, in
Stage 013. There are zero automated tests of the Android layer.

**A8 — Everything is a literal in Java.**
`dp()` is called 58 times and 13 `setTextSize` calls carry inline sp values;
colours are 20-odd `0xFF…` constants duplicated across the two panels. This is at
least density-correct, so it is not the classic hard-coded-pixel bug — but there
is no single place where a spacing scale, a type scale or a palette lives.

### 1.3 What is genuinely good and must survive

These are load-bearing decisions, not accidents, and every option below preserves
them:

- **Panels own no truth.** Both read authoritative values back from native code
  and can only submit a whole section and accept the verdict. `syncMode()` reads
  `NativeViewport.productMode()` rather than assuming its own request succeeded.
- **The touch boundary is airtight.** Both panels return `true` from
  `onTouchEvent`, so an unclaimed touch inside a panel can never fall through and
  orbit the camera or deform the mesh.
- **Top anchoring plus `adjustPan` is correct** and should not be discarded: the
  keyboard rises from the bottom, so the fields *and* the model both stay visible
  and the window is never resized (which would rebuild the swapchain per keystroke
  session). The measured portrait IME case works.
- **`contentDescription` is already populated** on 7 control families (shape
  radios, every dimension field, unit radios, tool buttons, sliders, Back). This
  is a usable semantic hook that the test strategy in §10 builds on directly.

---

## 2. Mobbin findings

Researched via Mobbin Pro across creative/editor apps: 3D and AR scene editors,
canvas/drawing tools, photo editors, and destructive-action confirmations.

### 2.1 Patterns worth adopting

**One parameter at a time, selected by a segmented row.**
[Depop — Adjust](https://mobbin.com/screens/6ddbe876-edc9-4d7b-ba12-90646da8aa7f)
puts *Brightness / Contrast / Saturation* in a segmented control and shows a
single slider for whichever is selected, with one primary *Apply*. This is
directly the ForgeShape problem: five primitives × up to three parameters is a
slab today, and it becomes a chooser plus one row under this pattern. It also
matches what ForgeShape already does correctly at the domain level — one
parameter row on screen at a time, always the drafted kind's.

**Two-tier chrome: persistent mode row + transient contextual actions.**
[Depop — canvas](https://mobbin.com/screens/ad71cacc-8e4b-4241-906a-b9c20db2fcad)
keeps a persistent bottom row (*Add item / Templates / Background*) while a
floating pill of contextual actions appears near the selected object. The
separation of "where am I" from "what can I do to this thing" is exactly the
Construction/Sculpt-versus-object-commands split ForgeShape needs when multi-object
and gizmos arrive.

**Inspector as a bottom sheet over a live canvas.**
[Photoroom — layers](https://mobbin.com/screens/4f6f5fc0-1ff1-41d9-8350-a564b8b1b5c7)
keeps the canvas visible above a *Layers* sheet with an explicit *Done*. The
canvas is never fully occluded, and the sheet is the natural future home for an
outliner.

**Compact top bar reserved for global, mode-independent actions.**
[Photoroom](https://mobbin.com/screens/581c0f2a-743c-4464-bb1a-c5ac3c2c2625) and
[Genie](https://mobbin.com/screens/f90f518f-da57-41f2-9605-879b9a1c8c71) both put
close / undo / redo / share in a thin top strip that does not change with the
tool. That is the reserved home for undo-redo and project state.

**Contextual actions that orbit the selection, not the screen.**
[IKEA — room planner](https://mobbin.com/screens/82103236-c744-4665-a180-6664d7574c18)
shows *Remove / Duplicate / Rotate / Move back* only when something is selected,
and an *Expand* affordance for the fuller inspector. This is a 3D scene editor
solving ForgeShape's exact selection problem.

**Destructive actions state the consequence and label the verb.**
[Alta](https://mobbin.com/screens/529ac58a-c13e-426a-99a9-d57f190b97c4) ("Deleted
items cannot be recovered", button *Delete*) and
[Posh](https://mobbin.com/screens/58d710bc-8afe-49e0-90ff-1f5d846066ba) ("This
can't be undone") never label the confirm button *OK*. **Re-Freeze must adopt
this**: it discards sculpted vertices and today says nothing.

### 2.2 Patterns explicitly rejected

- **Bottom carousels of visual thumbnails** (IKEA, Replika). ForgeShape's
  primitives are *parametric*, not a catalogue; a thumbnail strip implies picking
  an asset, and it would waste the scarcest resource in landscape — height.
- **Full-height modal editors** (Instagram avatar, Posh). Any sheet that hides the
  model breaks the one thing ForgeShape is: you must see the shape change when you
  change the shape.
- **Floating draggable tool palettes** (desktop-style). They collide with the
  brush, and every pixel they occupy is a place a stroke cannot start.
- **A hamburger/drawer as the primary home for modes.** Construction↔Sculpt is a
  frequent, reversible switch; burying it costs two taps every time.
- **Nomad Sculpt's shell specifically.** Not studied for imitation and not
  reproduced: no iconography, no layout, no visual identity is taken from it or
  from any Mobbin screen. The patterns above are structural conventions
  (segmented parameter chooser, bottom sheet, top action strip), redrawn for
  ForgeShape's own domain vocabulary.

### 2.3 ForgeShape-original adaptations

- **The Apply boundary has no analogue in any app studied.** Every reference edits
  live and continuously. ForgeShape's Apply Shape / Apply Transform split is a
  *domain* rule (one republishes the mesh, one cannot) and must survive the
  redesign intact — so ForgeShape's inspector needs a commit affordance and a
  rejection channel that a photo editor never needs.
- **Two representations of one object** (Construction Source / Frozen Sculpt Mesh)
  is not a layer stack and must not be drawn as one. An outliner later shows one
  object with two representations, not two objects.
- **The unit selector (mm/cm/m) is presentation-only and makes no native call.**
  No reference has this; it is ForgeShape's own and belongs next to the numbers.

---

## 3. ForgeShape design principles

1. **The viewport is the product.** Chrome justifies itself against occluded
   pixels. Target: **never below 55 % of the window unoccluded**, in every
   configuration, with the IME the single allowed exception.
2. **Nothing on screen is authoritative.** The UI drafts and submits; native code
   decides and is read back. This is already true and is non-negotiable.
3. **One parameter's meaning is never positional-by-mode.** Carried up from the
   typed JNI boundary: a field on screen means one thing.
4. **Separate truths get separate commits.** Shape and placement keep separate
   Apply actions because they have different consequences.
5. **A destructive act names its consequence.** Re-Freeze discards sculpt work and
   must say so before doing it.
6. **A gesture is decided once, on Down, below the JNI boundary.** Java never
   arbitrates navigation versus brush.
7. **Every control is addressable by a stable id, not a coordinate.**
8. **The shell is one shell.** Modes contribute content to it; they do not each
   bring their own window.

---

## 4. Shell model — homes for now and later

```
┌───────────────────────────────────────────────────────────┐
│ TOP STRIP   project · undo/redo · [status/progress]       │  global, mode-independent
├───────────────────────────────────────────────────────────┤
│                                                           │
│                     V I E W P O R T                       │  never fully occluded
│                                                           │
│        ┌───────────────────────────────┐                  │
│        │ contextual selection actions  │                  │  only when something is selected
│        └───────────────────────────────┘                  │
├───────────────────────────────────────────────────────────┤
│ MODE BAR    Construction │ Sculpt │ (UV) │ (Export)       │  the four top-level homes
├───────────────────────────────────────────────────────────┤
│ INSPECTOR   the active mode's parameters                  │  collapsible, detented
└───────────────────────────────────────────────────────────┘
```

| Concern | Home | Status |
| --- | --- | --- |
| Construction | mode bar entry → inspector: kind chooser + that kind's params + transform | exists, to be rehoused |
| Sculpt | mode bar entry → inspector: tool row + Radius/Strength | exists, to be rehoused |
| UV | mode bar entry (disabled until it exists) | **reserved** |
| Export | mode bar entry or top-strip action (see §12 decision D4) | **reserved** |
| Hierarchy / outliner | inspector sheet, second detent | **reserved** |
| Object commands (duplicate/delete) | contextual selection actions over viewport | **reserved** |
| Undo / redo | top strip, mode-independent | **reserved** |
| Gizmo options (translate/rotate/scale, space) | contextual actions when a gizmo is active | **reserved** |
| Snapping / symmetry | inspector header of the mode that owns it | **reserved** |
| Boolean history / tree | outliner detent | **reserved** |
| Status / progress | top strip; blocking progress as a scrim over the viewport | partly exists (status line) |
| Project / save state | top strip left | **reserved** |
| Freeze / Resume Sculpt | the seam *between* Construction and Sculpt — see below | exists, to be rehoused |

**Where Freeze belongs.** It is not a Construction property and not a Sculpt tool;
it is the transition between two representations. Proposal: it lives on the
**Sculpt mode-bar entry itself** — tapping Sculpt with nothing frozen offers
*Freeze to Sculpt*; with something frozen it resumes, and *Freeze again* is a
guarded action inside the Sculpt inspector header carrying the §2.1 consequence
statement. That makes the two paths visibly different acts instead of two
identical buttons.

---

## 5. Adaptive layout rules

Driven by the **window** size (not the display, not the orientation), so
split-window and free-form are handled by the same rule.

| class | width dp | policy |
| --- | --- | --- |
| **Compact** | < 600 | Mode bar + inspector docked to the **bottom** as a detented sheet. Viewport above. |
| **Medium** | 600–839 | Same as compact, inspector may be wider than full-bleed and side-anchored. |
| **Expanded** | ≥ 840 | Inspector becomes a **persistent side panel** (≤ 360 dp); outliner may occupy a second persistent panel only if width ≥ 1200 dp. |

**Height override — this is the rule that fixes A1.** When window **height
< 480 dp** (which is every phone landscape, 411 dp measured), the inspector is
**never docked**. It becomes an overlay sheet at ≤ 60 % height, or a side panel if
width ≥ 840 dp. This single rule is what makes landscape recoverable.

- **Minimum viewport:** ≥ 55 % of window area unoccluded in every class. If the
  inspector's natural content would break that, it scrolls — it does not grow.
- **Collapse:** the inspector has three detents — *hidden* (a handle only),
  *peek* (the active mode's primary row), *full* (everything, scrolling). Detent
  is remembered per mode for the process lifetime.
- **Scrolling is mandatory.** Every inspector body is inside a scroll container.
  A1 exists only because no such container exists today.
- **Insets:** the shell consumes system-bar, cutout and IME insets explicitly and
  applies them as padding to the chrome, never to the `SurfaceView` — the Vulkan
  surface stays full-bleed so the render target and the swapchain are unaffected.
- **Edge-to-edge:** adopt it deliberately rather than relying on the deprecated
  fullscreen theme.
- **IME:** keep `adjustPan`. Keep top-anchored *editing* content. When the IME is
  up, the inspector shows only the section being edited.
- **Config changes:** either drop `orientation|screenSize|screenLayout` from
  `configChanges` and let the Activity rebuild, or keep them and implement
  `onConfigurationChanged` to re-run the layout decision. **The current state —
  absorbing them and doing nothing — is not an option.** Keeping them is
  preferable: it avoids destroying and recreating the Vulkan surface on rotation.

---

## 6. Shell options

### Option A — "Docked Inspector" (evolution of today)

One shell class. Thin top strip; the viewport fills the rest; a bottom mode bar
(Construction / Sculpt / UV / Export) and, under it, a docked inspector holding
the active mode's controls in a scroll container with the three detents from §5.

- *Phone portrait:* top strip ~48 dp, viewport, mode bar ~56 dp, inspector at peek
  (~120 dp) or full (≤ 45 %).
- *Landscape:* inspector auto-collapses to peek; full detent overlays at 60 % and
  the viewport is dimmed but visible behind it.
- *Tablet:* inspector docks right as a 320 dp persistent panel.
- *Construction:* mode bar → Construction; inspector shows kind chooser, that
  kind's fields, unit, Apply Shape; transform is a second collapsible group.
- *Sculpt:* inspector shows tool row + Radius/Strength; peek shows just the tools.
- *Hierarchy later:* a second detent above the inspector content.
- *UV/Export later:* new mode-bar entries; the inspector is already generic.
- *Gestures:* unchanged. The inspector is a touch-opaque region exactly as the
  panels are today; nothing about arbitration moves.
- *Complexity:* **lowest.** One new container class plus a mode-content interface;
  the two existing panels become inspector bodies almost unchanged.
- *Migration risk:* **low.** Both panels keep their native read-back logic.
- *Views/Compose:* comfortable in Views.
- *Accessibility/IME:* good — existing `contentDescription`s carry over; the
  bottom dock is the natural thumb zone.
- *Strengths:* fixes A1–A3 and A5 with the least work; nothing proven is
  disturbed; reserves every future home adequately.
- *Weaknesses:* a bottom dock plus a mode bar is a lot of permanent chrome on a
  411×914 dp phone; the tablet story is "the same thing, wider" and does not
  really exploit the space; contextual object actions have no strong home.

### Option B — "Rail + Contextual Inspector"

A persistent thin **rail** carries the modes (icon + label, ~72 dp) — bottom edge
on compact, **left edge** on expanded. The inspector is a separate surface that
appears adjacent to the rail and is *contextual*: it shows the active mode's
parameters, or the selected object's, or the active gizmo's. Contextual selection
actions float over the viewport near the selection (IKEA/Depop pattern).

- *Phone portrait:* bottom rail; inspector as a detented sheet above it.
- *Landscape:* rail moves to the **left edge** (this is the pattern's payoff — a
  vertical rail costs width, which landscape has, instead of height, which it does
  not); inspector as a right-side overlay ≤ 380 dp.
- *Tablet:* left rail + persistent right inspector + optional outliner — a real
  three-pane editor.
- *Construction:* rail → Construction; inspector = kind chooser + params +
  transform group; selection actions over the viewport when an object is picked.
- *Sculpt:* rail → Sculpt; inspector = tools + brush; brush values also reachable
  as a transient radial/slider near the finger later.
- *Hierarchy later:* first-class — a rail entry or the inspector's outliner tab.
- *UV/Export later:* rail entries; UV gets a viewport of its own naturally.
- *Gestures:* the floating contextual actions are the one genuine new risk — they
  sit **over** the viewport and must be touch-opaque and must never overlap where
  a stroke is likely. Rule: they appear only in Construction mode with a selection,
  never in Sculpt.
- *Complexity:* **highest.** Rail + inspector + contextual overlay + placement
  logic, and a real orientation-dependent rail position.
- *Migration risk:* **medium.** The existing panels split into "inspector body"
  and "mode identity"; the contextual overlay is entirely new.
- *Views/Compose:* doable in Views; the contextual overlay placement is the
  fiddly part either way.
- *Accessibility/IME:* rail is easy; the floating overlay needs careful traversal
  order and must not trap focus.
- *Strengths:* the only option that genuinely scales to hierarchy + gizmos +
  multi-object + UV without another redesign; **the only one that solves landscape
  by using width instead of rationing height**; best tablet/stylus story.
- *Weaknesses:* most work; most new surface to get wrong; the floating overlay is
  the one place where new UI can collide with the proven gesture model.

### Option C — "Viewport-Maximal / Transient Chrome"

Almost no permanent chrome. A single compact strip at the bottom holds the active
mode and the *one* control most relevant right now. Everything else is transient:
tapping the mode chip opens a mode switcher; tapping a parameter chip opens a
one-parameter editor (Depop *Adjust* pattern) that closes on commit.

- *Phone portrait:* ~85 % viewport at rest.
- *Landscape:* the same strip; ~80 % viewport. Landscape is fixed almost for free
  because there is nothing to occlude with.
- *Tablet:* the strip can float centred; extra space simply goes to the viewport
  unless the user opens something.
- *Construction:* chips read e.g. `Box · 2 × 1 × 0.5 m`; tapping a chip opens that
  one field with the unit selector and Apply.
- *Sculpt:* chips read `Grab · 120 px · 0.60`; tapping opens that one slider.
- *Hierarchy later:* a transient full sheet.
- *UV/Export later:* mode switcher entries.
- *Gestures:* best of the three — the smallest region where a touch is not a
  viewport touch.
- *Complexity:* **medium.** Few surfaces but a lot of transient-state choreography
  and many more taps to specify.
- *Migration risk:* **medium-high.** It genuinely discards the current panel
  layout; the Construction editing flow is redesigned, not rehoused. Entering
  three box dimensions becomes three open/edit/commit cycles unless a "all fields"
  escape hatch is kept — which starts pulling Option A back in.
- *Views/Compose:* fine in Views.
- *Accessibility/IME:* **weakest.** Transient surfaces are harder to announce, and
  a screen-reader user pays the extra taps too.
- *Strengths:* maximum viewport in every configuration; fixes A1 almost trivially;
  most "3D app"-feeling.
- *Weaknesses:* worst for *entering numbers*, which is the entire Construction
  workflow and the thing ForgeShape is most exacting about; hides state that
  Stage 008–014 deliberately made visible (validation messages, the stale-source
  warning); poorest reserved home for an outliner.

### 6.1 Comparison

| | A — Docked | B — Rail | C — Transient |
| --- | --- | --- | --- |
| fixes landscape (A1) | yes, by rationing height | **yes, by using width** | yes, by removing chrome |
| viewport at rest, phone portrait | ~60 % | ~62 % | **~85 %** |
| viewport at rest, phone landscape | ~55 % | **~70 %** | **~80 %** |
| numeric entry workflow | **best** | good | worst |
| scales to hierarchy + gizmo + multi-object | adequate | **best** | poor |
| tablet / stylus payoff | low | **high** | low |
| risk to proven gesture arbitration | **none** | one new overlay | **none** |
| implementation cost | **low** | high | medium |
| discards proven UI work | little | some | most |

---

## 7. Views vs Compose

Evaluated for the **shell only**. No migration is proposed for 015A, and none is
performed.

### 7.1 The decisive finding

**ForgeShape currently has zero runtime dependencies.** `app/build.gradle`
contains no `dependencies { }` block at all, `gradle.properties` sets
`android.useAndroidX=false`, and there are **0 Kotlin source files**. Adopting
Compose is therefore not a UI-style choice; it requires, minimally:

- enabling AndroidX (`android.useAndroidX=true`) and Jetifier considerations;
- the Kotlin Gradle plugin and the Kotlin stdlib — a second language in a
  deliberately Java + C++17 codebase;
- the Compose compiler plugin, pinned against the Kotlin version;
- `androidx.activity:activity-compose`, `androidx.compose.ui`,
  `compose.foundation`, `compose.material3`, `compose.runtime`, and their
  transitive graph — on the order of 30+ artifacts.

That is a material change to what ships in the APK, in a project whose stated
rule is that no third-party runtime library participates in the running product.
Compose is Google's, not third-party, so the rule is not automatically violated —
but the decision is the owner's and is listed in §12.

### 7.2 ForgeShape-specific risk assessment

| risk | Views | Compose |
| --- | --- | --- |
| `SurfaceView` composition | Native. Today's `FrameLayout` z-order is already proven across every stage. | `AndroidView { }` works, but Compose draws into its own hardware layer; a `SurfaceView` punches through the window and z-ordering against Compose content is a known sharp edge (`setZOrderOnTop` interacts badly with overlays). |
| Raw `MotionEvent` / stylus path | **Unchanged and proven.** `onTouchEvent` hands the whole event, all pointers, to one JNI call. | The `SurfaceView` inside `AndroidView` keeps its own `onTouchEvent`, so the path survives — but Compose's pointer-input pipeline sits above it, and any Compose gesture modifier on an ancestor can consume or delay events. This is the highest-value proven behaviour in the product to put at risk. |
| Focus / IME | Working today (`takeFocusFromEditor`, `adjustPan`). | Compose text-field focus + a hosted `SurfaceView` is a well-known friction point; `takeFocusFromEditor` would need rewriting against Compose's focus system. |
| Lifecycle | `surfaceDestroyed` blocking until the `ANativeWindow` is released is a hard rule and is currently a plain `SurfaceHolder.Callback`. | Unchanged if the `SurfaceView` is hosted, but composition lifecycle adds a layer to reason about. |
| Insets | Must be written; nothing exists. | Better out of the box (`WindowInsets` API), a genuine Compose advantage. |
| Adaptive layout | Must be written; nothing exists. | Better out of the box (`WindowSizeClass`), a genuine Compose advantage. |
| State ownership | Manual read-back, already correct. | Idiomatic, but Compose's pull toward hoisted `State` is a *temptation* to cache native truth in the UI — the exact invariant §8 protects. |
| UI testability | Espresso, needs `setId`/`contentDescription`. `contentDescription` already exists. | `compose-ui-test` is excellent — but it requires AndroidX test artifacts either way, so the delta is smaller than it looks. |
| Build impact | none | Kotlin + Compose compiler + 30+ artifacts; longer builds; a version-alignment burden the project has never had. |

### 7.3 Recommendation

**Structured Views**, with one bounded exception.

The two things Compose is genuinely better at here — insets and window size
classes — are each a few dozen lines of Views code, and both are needed exactly
once, in one shell class. Against that, Compose introduces a second language, a
compiler plugin, AndroidX and a 30-artifact dependency graph into a project that
currently has none, and puts the single most carefully proven behaviour in the
product — the raw-`MotionEvent`-to-JNI path and the Sculpt gesture arbitration —
on top of a pointer pipeline it does not control. That trade is not favourable at
this size: the Android layer is 2223 lines.

The bounded exception, if the owner wants Compose's ergonomics later: adopt it
**only** for inspector *content* (fields, sliders, chooser rows) hosted inside a
Views shell that keeps the `FrameLayout` + `SurfaceView` composition and the touch
boundary. That contains the risk to the part of the UI that has no gesture
responsibility. It is not recommended for 015B, only noted as the safe shape if
the answer changes.

**What "structured Views" means concretely** (this is the real content of the
recommendation, and it addresses A5/A7/A8 regardless of option):

- one `EditorShellView` owning chrome placement, insets, size class and detents;
- a `ModeContent` interface implemented by `ConstructionInspector` and
  `SculptInspector` — the current panels, minus their private shell duties;
- one `Tokens` class for the spacing scale, type scale and palette (removes the
  duplicated constants);
- stable `View` ids for every control, via a generated id resource or constants.

---

## 8. State and event architecture

### 8.1 Ownership

| State | Owner | UI role |
| --- | --- | --- |
| Active product mode | **native** `SculptSession` | reads back via `productMode()`; may only request |
| Selected object | **native** `SelectionController` | reads back; renders selection chrome |
| Active primitive kind (authoritative) | **native** `ConstructionObject` | reads back |
| Active sculpt tool | **native** `SculptSession` | reads back; may only request |
| Brush radius / strength | **native** (clamped) | reads back; slider is a request |
| Primitive parameters, transform | **native** (double meters/degrees) | reads back; submits whole sections |
| **Draft primitive kind** | **UI** | which fields are on screen; not the object's kind |
| **Field text, in-progress edits** | **UI** | never authoritative |
| **Display unit (mm/cm/m)** | **UI** | no native call at all |
| **Inspector detent, active mode tab, scroll position** | **UI** | pure presentation, process-scoped |
| Validation errors | **native decides**, UI renders | UI refuses only what it can name from the text alone |
| Operation progress (future) | **native** | UI renders a scrim |
| Hierarchy selection (future) | **native** (extends `SelectionController`) | reads back |
| Undo/redo availability (future) | **native** | UI renders enablement only |

**Invariant, restated:** Android presentation state is never authoritative
geometry truth. The UI may hold *drafts* and *view state* and nothing else. The
list above is the complete allowed UI-owned set; anything not on it is native.

### 8.2 Directional flow

```
 user gesture / control
        │
        ▼
  Android UI  ──── draft ────►  (stays in the View; no native call)
        │
        │ submit a WHOLE section (Apply) or request a mode/tool
        ▼
   thin JNI  ── typed, one method per primitive; meters + degrees only
        │
        ▼
 native domain  ── validate → update → publish, atomically, one entry point
        │                                   │
        │ status + reason                   ▼
        │                              MeshStore revision
        ▼                                   │
  Android UI  ◄── read back authoritative state          ▼
   (renders verdict, never assumes it)          renderer / picking → Vulkan
```

Two rules make this a *directional* flow rather than a round trip:
**(1)** the UI never writes a value it did not receive a verdict for, and
**(2)** there is no path from renderer or `MeshStore` back into the UI — the UI
learns what happened from the domain's return value and from explicit state
reads, never by measuring the mesh. Both hold today and neither is relaxed.

---

## 9. Gesture ownership matrix

Current native arbitration is **preserved unchanged**. Nothing below alters
`g_strokePending`, the 8 px arming threshold, or the pending-then-promote rule
proven in Stage 013 and re-verified in Gate P0.

| Input | Construction | Sculpt | UV (future) | Owner / arbitration |
| --- | --- | --- | --- | --- |
| 1 finger, empty viewport | orbit | orbit | pan | `CameraController`; tap-vs-drag by `SelectionController` |
| 1 finger, on object | tap → select; drag → orbit | — | select shell/island | `SelectionController` decides on Up |
| 1 finger, on Frozen Sculpt Mesh | n/a | **PENDING on Down** → promote to stroke at ≥ 8 px travel, else abandon | n/a | **native, unchanged**; probe `hitsSculptMesh` cannot mutate |
| 2 fingers, anywhere | pan + zoom | pan + zoom; **abandons a pending stroke, ends a live one cleanly** | pan + zoom | `CameraController` re-anchors on any pointer-set change |
| Stylus tip | as 1 finger | as 1 finger | as 1 finger | proposal: no new path in 015B. `getToolType` is read **only** to *prefer* stroke over orbit on a sculpt-mesh hit; pressure ignored until a stage pays for it |
| Gizmo handle hit (future) | gizmo owns the whole gesture from Down | n/a | n/a | must be decided on Down, below JNI, like the sculpt probe — **never in Java** |
| Panel / inspector scroll | inspector owns; **never reaches viewport** | same | same | inspector is touch-opaque (`onTouchEvent → true`), as today |
| Slider drag | slider owns for the whole gesture | same | same | inspector; requests only, native clamps |
| Text field | field owns; viewport `ACTION_DOWN` pulls focus and IME away | n/a | same | `takeFocusFromEditor`, as today |
| Contextual overlay (Option B only) | overlay owns; touch-opaque; **suppressed entirely in Sculpt** | never shown | n/a | must not overlap likely stroke area |
| Long press | **not proposed.** No long-press anywhere in 015B | | | reserved; adding one would need a Down-time decision like the sculpt probe |

**The rule that must not be broken:** a gesture that becomes multi-touch
navigation never mutates the sculpt mesh — no vertex written, no `SculptRevision`
minted, no stroke committed. Every proposal here keeps that decision native and
on Down.

---

## 10. Android UI test strategy

**Scope boundary.** Native/core self-tests keep geometry and math. Integration
smoke keeps the Vulkan/input bridge. Android UI tests own **only** shell
behaviour, control visibility, and the UI→native call contract. **No Vulkan pixel
assertions.**

**Enabler, required first:** stable identifiers for every control. Today
`setId` appears twice. `contentDescription` already exists on 7 control families
and is the cheapest bridge — Espresso can match it immediately — but explicit ids
are the durable answer and should land with the shell.

**Dependency note:** Espresso requires AndroidX test artifacts. These are
`androidTestImplementation` only and do **not** ship in the product APK — worth
stating explicitly given the zero-dependency baseline. This is the one place
015B must add dependencies, and it adds none to the shipped app.

Minimum sustainable coverage:

| # | Test | Asserts |
| --- | --- | --- |
| T1 | Mode/shell visibility | exactly one inspector body is displayed; it matches `NativeViewport.productMode()`, not what was tapped |
| T2 | Primitive field switching | selecting each of the 5 kinds shows exactly that kind's fields and hides all others; **zero native calls** are made |
| T3 | Unit switching is presentation-only | mm→cm→m rewrites the text and makes **no native call**; native parameters read back bit-identical |
| T4 | Invalid input is visible | blank / non-numeric / non-positive dimension shows an error naming the field, and no native call occurs |
| T5 | Rejection is surfaced | a natively rejected value (capsule `totalHeight < diameter`) renders the domain's reason; the UI does not restate the rule itself |
| T6 | Freeze / Resume controls | Resume is absent before the first Freeze and present after; re-Freeze shows the consequence confirmation before discarding |
| T7 | Sculpt tool selection | tapping each tool highlights the tool **native reports**, and Radius/Strength are unchanged across tool switches |
| T8 | Responsive layout state | at 411×914, 914×411, 360×640 and a 600 dp split window: viewport ≥ 55 % unoccluded, and every primary action is reachable (this is the regression test for A1) |
| T9 | Panel gestures do not leak | a drag inside the inspector produces **no** camera state change and **no** `SculptRevision` — the airtight-boundary test |
| T10 | Insets | no control intersects the system-bar or cutout insets in any configuration |

T8 and T9 are the two that would have caught the defects this audit found.

---

## 11. Implementation impact and risk

| | scope | risk |
| --- | --- | --- |
| New `EditorShellView` + `ModeContent` seam | ~400–600 lines | low; additive |
| Insets + size-class + detent logic | ~150–250 lines | low, but must not pad the `SurfaceView` |
| Rehousing the two panels | mostly deletion of their shell duties | **medium — this is where proven behaviour can be lost.** The native read-back and touch-opacity must be carried over verbatim |
| Stable ids + `Tokens` | mechanical | low |
| Espresso harness + T1–T10 | new `androidTest` source set | low; no product-APK dependencies |
| Option B contextual overlay | additional | **highest single risk in the pack** — new UI over the viewport |
| Rewriting the README coordinate table | docs | low, and it stops being needed once ids land |

**Risks to state plainly:**
- The current UI is *runtime-verified* through fourteen stages. Any rehousing
  risks silently dropping a behaviour that no automated test protects — which is
  precisely why §10 should land **with** or **before** the shell, not after.
- `configChanges` currently prevents Activity recreation on rotation, which also
  protects the Vulkan surface. Changing it would destroy and recreate the surface
  on every rotation. Recommendation: keep it, implement `onConfigurationChanged`.
- Nothing in this pack touches native code. If an option seems to require a JNI
  change, that is a signal the option is drifting into the domain.

---

## 12. Owner decisions required

These are genuine product decisions. None is a naming or padding question.

**D1 — Which shell direction?** A (Docked), B (Rail), or C (Transient).
*Recommendation: **B**, because it is the only one that solves landscape by
spending width instead of rationing height, and the only one with real homes for
hierarchy, gizmos and multi-object — all of which are on the roadmap. **A** is
the strongest alternative and is markedly cheaper and lower-risk; choose A if you
want the landscape defect fixed soon with minimal disturbance.*

**D2 — What is ForgeShape's primary device?** Phone-first, tablet-first, or
genuinely both. This materially changes D1: **if phone-only, the recommendation
flips from B to A**, because B's rail and three-pane layout earn their cost on a
tablet and not on a 411 dp phone. This is the single most load-bearing answer in
the pack.

**D3 — Views or Compose?** *Recommendation: **structured Views**, on the evidence
that Compose means adding Kotlin, AndroidX and 30+ artifacts to a project with
zero runtime dependencies, and layering a pointer pipeline over the most
carefully proven behaviour in the product.* Say if you want Compose anyway, or
want the bounded hybrid (§7.3) reserved.

**D4 — Is Export a mode or an action?** A mode-bar entry (sits beside
Construction/Sculpt, has its own inspector) or a top-strip action (opens a sheet,
returns you where you were). Affects how many top-level homes the shell reserves.

**D5 — Does re-Freeze get a confirmation?** *Recommendation: yes*, with the
consequence stated ("this discards the sculpt work on this mesh") and a verb-
labelled button. It is the only irreversible act in the product and currently has
no guard. Say no if you consider the extra tap worse than the loss.

**D6 — Is stylus pressure in scope for 015B?** *Recommendation: no.* Read
`getToolType` only to prefer a stroke over an orbit; leave pressure until a stage
pays for it. Confirm, or say pressure matters now.

---

## 13. What was not done

Deliberately, per the stage scope: no Plane, no multi-object, no hierarchy, no
undo, no gizmo, no booleans, no persistence/export, no Sculpt features, no
Compose migration, no dependency added, no production UI change, and no start on
Stage 015B. The only file changes in this stage are this document, the four audit
screenshots it cites, and the `PROJECT_STATUS.md` entry recording it.
