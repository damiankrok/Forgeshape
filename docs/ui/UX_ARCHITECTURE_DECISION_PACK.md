# ForgeShape — UI/UX Architecture Decision Pack

**Revision:** Stage 015A-R — Nomad-first re-audit + CAD Sketch/Extrude shell concept
**Supersedes:** Stage 015A (first issue). Its option set survives only as Appendix A.
**Status:** proposal for owner review. **Nothing here is implemented.**
**Baseline:** PROJECT_STATUS v0.15.1, Gate P0 COMPLETE, NDK r29, clean tree at `c85d808`.

This document proposes. It does not describe the product. Everything below is
unbuilt; `PRODUCT.md` remains the only place that says what ForgeShape does.

---

## 1. Why this revision exists

The first issue of this pack was technically sound and **structurally wrong**.

Its measured audit findings were correct and are retained in full (§2). But its
option set was derived from the wrong reference class: the shells it proposed
were extrapolated from consumer, commerce and photo-editing apps, because those
are what a general UI-pattern library contains. That produced options optimised
for *forms over a canvas* rather than for *a tool in the hand over a model* —
and ForgeShape is the second thing.

The owner rejected that basis. This revision replaces it:

- **Primary structural reference:** the mobile sculpting workspace as
  exemplified by Nomad Sculpt — viewport-dominant, edge-mounted continuous
  controls, a compact tool rail, collapsible chrome. Used **structurally only**;
  no branding, iconography, colour, spacing or toolbar ordering is taken from it.
- **Secondary interaction reference:** Shapr3D-like sketch/profile/extrude
  *efficiency* — direct manipulation with an exact numeric escape hatch — without
  desktop-CAD complexity.
- **Consumer patterns are demoted** to micro-interaction evidence only
  (confirmation wording, chip shapes), never to shell evidence.

What this revision adds beyond a re-skin: **Sketch + Extrude is now planned MVP
scope**, so the shell must reserve real homes for it rather than treat it as a
distant "someday". No geometry is designed or implemented here.

---

## 2. Retained findings from the first audit

Measured on `emulator-5558` from `uiautomator` hierarchy dumps at three window
sizes. All still true; none is affected by the change of reference.

| | portrait 411×914 dp | landscape 914×411 dp | compact 360×640 dp |
| --- | --- | --- | --- |
| `SurfaceView` | `[0,0][1080,2400]` | `[0,0][2400,1080]` | `[0,0][720,1280]` |
| Construction panel | `[0,0][1080,1175]` | `[0,0][2400,1080]` | `[0,0][720,~880]` |
| **viewport unoccluded** | **51 %** | **0 %** | ~31 % |
| status line | visible | **clipped off-screen** | visible |

1. **Landscape is a hard failure.** The panel measures to the full window; the
   model is entirely occluded and the status line — the only channel for
   validation messages and the stale-source warning — lays out below the window
   bottom. There is **no `ScrollView` anywhere in the Android layer** to reach it.
2. **No responsive architecture exists.** `configChanges` absorbs
   `orientation|screenSize|screenLayout|density` and there is no
   `onConfigurationChanged`. Counts across the layer: `onConfigurationChanged` 0,
   `getResources().getConfiguration` 0, `ORIENTATION_LANDSCAPE` 0,
   `screenWidthDp` 0, `ScrollView` 0.
3. **No `WindowInsets` handling.** 0 occurrences. Survives only because the
   deprecated fullscreen theme hides the system bars; `targetSdk` 36 makes
   edge-to-edge the platform default, so the collision is latent, not theoretical.
4. **Almost no stable IDs.** `setId` twice in 2223 lines, both `RadioGroup`
   internals — which is why every runtime verification since Stage 007 has been
   driven by screen coordinates, and why `README.md` carries a pixel table that
   had to be rewritten in Stage 013.
5. **No Android UI automation at all.**
6. **The monolithic panels do not scale.** `ConstructionPanelView` is 1041 of
   2223 lines and already shows 5 radios + up to 3 fields + 3 unit radios + 2
   Apply buttons + 6 transform fields + Freeze + Resume + status simultaneously.
   Five primitives already forced the selector onto its own row in Stage 014.
7. **Structured Views remain the lower-risk toolkit** — re-confirmed in §12 with
   the same evidence, which the change of visual reference does not touch.

**And what must survive**, because it is load-bearing and proven: panels own no
truth and read native state back; both panels return `true` from `onTouchEvent`
so no touch leaks to the viewport; top-anchored editing + `adjustPan` keeps
fields and model visible without resizing the window (which would rebuild the
swapchain per keystroke session); `contentDescription` is already populated on 7
control families.

---

## 3. Nomad-first structural findings

Observed structurally from the current Nomad Sculpt mobile workspace. These are
*organising principles*, restated in ForgeShape's own terms. No visual asset,
icon, colour, metric or ordering is reproduced.

| # | Structural principle | Why it applies to ForgeShape | How ForgeShape adapts it |
| --- | --- | --- | --- |
| N1 | The viewport is essentially the whole screen; chrome lives on the **edges**, not in a slab | ForgeShape's slab occludes 49–100 % today | Edge rails + a peek-detent inspector; the `SurfaceView` stays full-bleed under everything |
| N2 | The two continuously-varied brush values sit at a screen **edge**, always reachable, adjustable mid-work without opening anything | Radius and Strength are exactly this pair, and are currently buried in a slab | Two vertical edge sliders with value chips, left edge, thumb-reachable |
| N3 | A compact **rail** holds tools; the active one is unmistakable | ForgeShape has 4 sculpt tools and will gain Select/Shape/Move/Sketch/Extrude | One rail, right edge, icon **+ label**, active = filled accent + border |
| N4 | A thin global strip holds only what is mode-independent | undo/redo, project, hierarchy, overflow have no home today | One 48 dp strip; the **mode chip** is its centre |
| N5 | Contextual toolbars appear **only** when the active tool needs them | Grab needs nothing extra; Sketch and Extrude need a lot | One contextual bar, bottom, whose content is a function of (mode, tool, selection) |
| N6 | The UI can be **collapsed** to leave a bare viewport | ForgeShape has no such affordance | A persistent hide/show control under the rail |
| N7 | Current tool and current state are obvious without opening anything | today the active tool is a button tint inside a slab | Active tool shown in the rail **and** named in the contextual bar |

**The one place ForgeShape must diverge from a pure sculpting shell:** a sculpt
app's values are *continuous and approximate* (a brush radius), so sliders are
sufficient. ForgeShape's Construction values are *exact and typed* (2.000000 m),
and the Apply boundary is a domain rule — shape republishes the mesh, placement
cannot. So the same shell must host a **numeric, committed** editor as a
first-class citizen, which is what §5 and §6 do and what no sculpting shell has
to solve.

---

## 4. Mobbin evidence — and an honest gap

Searched with the required filter (3D/sculpt/CAD, drawing/painting, stylus
creative tools). **Five searches. The result must be reported plainly:**

> **Mobbin's library contains no mobile 3D sculpting application and no CAD or
> parametric-sketch application.** Direct searches for a sculpting workspace
> returned a travel-book viewer, a screen-time app and a link-in-bio editor;
> searches for CAD dimension entry returned banking and clothing-size forms.
> Named searches for the reference apps return nothing of theirs.

This is why Nomad Sculpt — observed directly, not via Mobbin — is the primary
structural reference in §3. Mobbin contributes one genuinely primary reference
and two supplementary ones, and nothing else was used.

| Reference | Pattern | Why it applies | Rank |
| --- | --- | --- | --- |
| [Procreate Pocket — canvas](https://mobbin.com/screens/b2ec18be-03d2-40a5-830b-31eb1beec373) | Full-bleed canvas; a thin top strip carries tool + layer + colour; a **left-edge vertical slider** for the continuously-varied value; everything else transient | The closest real analogue available: a stylus-first creative tool on a phone, with a viewport-dominant shell and edge-mounted continuous controls. Corroborates N1, N2, N4 | **Primary** |
| [Procreate Pocket — Actions sheet](https://mobbin.com/screens/cbb5ba08-bb8c-442f-b992-26bc658cc22f) | Overflow as a **tabbed sheet** of icon+label tiles (Add / Canvas / Guides / Share) | Model for ForgeShape's overflow: reserves homes for project, export, settings and future operations without putting any of them in the shell | **Primary** |
| [Procreate Pocket — Brushes](https://mobbin.com/screens/ea1d9fba-8c84-48f8-b53e-fde80cf89f1e) | A **vertical category rail** beside a list, inside a sheet | Shows the rail idiom scaling past a handful of entries — relevant when the Construction rail gains Sketch and Extrude | Supplementary |
| [Obsidian — canvas](https://mobbin.com/screens/5af7d569-5f9e-433a-a94d-ddb11e5c1193) | **Right-edge vertical rail** of icon buttons with undo/redo grouped apart, plus a **contextual toolbar attached to the selection** | Nearest non-3D confirmation that rail + selection-attached actions works on a phone canvas. Corroborates N3, N5 | Supplementary |
| [ChatGPT — image select](https://mobbin.com/screens/4faa5bb2-28fe-48d0-86de-6e9b7430f274) | **Left-edge brush-size slider drawn as a tapered wedge** that previews the actual size | Direct evidence for the Radius control: show the value *as the thing it controls* | Supplementary |

**Micro-interaction only** (explicitly *not* shell evidence): destructive-action
confirmations that state the consequence and label the button with the verb
rather than *OK* — the pattern re-Freeze needs, since it discards sculpt work.

**Rejected outright:** thumbnail carousels (ForgeShape's primitives are
parametric, not a catalogue), full-height modal editors (never hide the model),
floating draggable palettes (they collide with the brush), drawer-as-primary
navigation (mode switching is frequent and reversible).

---

## 5. The ForgeShape Sculpt shell

**Wireframe: [WF-1](wireframes/WF-1-sculpt-phone-portrait.svg)** — phone portrait,
411 × 914 dp.

```
┌──────────────────────────────────────────────┐
│ ‹ Untitled      ● Sculpt ⌄     ↶ ↷  ⋯  ▤    │  48 dp global strip
├──────────────────────────────────────────────┤
│ ┌──┐                                  ┌────┐ │
│ │120 .60                              │Grab│ │  ← rail: active tool filled
│ │▐  ▐│         ╭─────────╮            ├────┤ │
│ │▐  ▐│        ╱  model    ╲   ⌒⌒      │Clay│ │  ← dashed circle = live
│ │●  ▐│       │    ( brush ) │         ├────┤ │     brush radius, true size
│ │▐  ●│        ╲     ⌣     ╱           │Smth│ │
│ │▐  ▐│         ╰─────────╯            ├────┤ │
│ │RAD STR                              │Infl│ │
│ └──┘                                  └────┘ │
│                                        (»)   │  ← hide UI
│                                              │
│ ┌──────────────────────────────────────────┐ │
│ │ ✓ Frozen mesh · 482 vertices             │ │  contextual bar
│ │   One finger sculpts · elsewhere orbits  │ │
│ │                          [Freeze again…] │ │  ← guarded, consequence stated
│ └──────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

- **Viewport is full-bleed** under everything. The `SurfaceView` is never
  resized, inset or padded — insets apply to chrome only, so the swapchain and
  the render target are untouched by any layout decision.
- **Radius and Strength are two vertical edge sliders**, left, with value chips
  at the top of each track. They are always live and never need opening. The
  brush radius is *also* drawn in the viewport at its true screen size, so the
  number and the thing agree.
- **The tool rail is right-edge**, icon + label, four tools. Active = filled
  accent with a 1.5 dp border. Placing it opposite the sliders means neither hand
  covers the other's control.
- **The contextual bar** carries only what this mode and tool need. For the four
  current brushes that is: the frozen-mesh state, the one-line gesture rule, and
  the guarded re-Freeze. It is also where **validation and the stale-source
  warning appear** — never an off-screen status line again.
- **Hide UI** collapses both rails and the contextual bar to a single restore
  chip, leaving the bare model. Nothing about the gesture model changes when
  hidden; the viewport simply gets the whole window.

**Chrome cost, portrait:** global strip 48 dp + rails at the edges (which
overlay, not displace) + contextual bar 52 dp. Unoccluded viewport ≈ **78 %**,
against 51 % today.

---

## 6. The ForgeShape Construction shell

**Wireframe: [WF-2](wireframes/WF-2-construction-phone-portrait.svg)** — phone
portrait, mode menu shown open.

Same shell language, same three regions, different content:

- **The rail switches to Construction tools:** Select, Shape, Move (gizmo,
  reserved), Sketch (reserved), Extrude (reserved). Reserved entries are drawn
  dimmed and are visible from day one, so the shell's shape does not change when
  they arrive.
- **The contextual inspector replaces the contextual bar**, at a **peek detent**
  of ~24 % of the window: primitive chooser chip, that primitive's exact fields,
  the mm/cm/m unit chips, a **collapsed Transform group** summarising
  `0,0,0 m · 0,0,0°`, and **Apply Shape**. Expanding Transform reveals six
  fields and **Apply Transform**.
- **The Apply boundary is preserved exactly.** Shape and placement keep separate
  commits because they have different consequences — one republishes the mesh,
  one cannot. The inspector never applies on edit.
- **Freeze lives on the mode seam, not as a button in a slab.** The mode chip
  opens Construction / Sculpt / UV / Export. The Sculpt entry reads
  *"Resume — 482 v frozen"* when a mesh exists and *"Freeze to Sculpt"* when none
  does, which makes the two genuinely different acts visibly different instead of
  two identically-styled buttons. Re-Freezing — the destructive one — is the
  guarded action in the Sculpt contextual bar (§5).
- **No permanent inspector consumes half the phone viewport.** Peek is ~24 %;
  full detent scrolls rather than grows; hidden leaves a handle.

**Unoccluded viewport, portrait, peek detent:** ≈ **68 %**.

---

## 7. Sketch + Extrude concept

**Wireframe: [WF-3](wireframes/WF-3-sketch-extrude.svg)** — the two steps side by
side.

**Planned MVP scope. No geometry is designed or implemented in this stage.**
Sketch is a **Construction tool/sub-mode**, not a top-level mode: you are still
constructing, with a different tool in hand.

| # | Step | Shell behaviour |
| --- | --- | --- |
| 1 | In Construction | rail shows Sketch |
| 2 | Choose Sketch | sub-mode banner replaces the contextual bar; rail switches to sketch tools |
| 3 | Choose a plane | Front / Top / Right chips in the banner. **A planar-face entry point is reserved** in the same control and is not built |
| 4 | View aligns | camera animates normal to the plane; the grid appears. This is a camera move, not a mode |
| 5 | Draw the profile | Line / Polyline, Rectangle, Circle in the rail; select/delete as the fourth entry |
| 6 | Exact values | live dimension callouts on the geometry **and** numeric fields in the contextual bar, sharing the mm/cm/m unit already in the product |
| 7 | Validate | the bar states closure explicitly: *"Profile closed — 4 segments"*, or names what is open |
| 8 | Select profile | **Extrude becomes immediately available** in the rail — the efficiency point of the whole flow |
| 9 | Drag the handle | an on-canvas arrow normal to the plane, direct manipulation |
| 10 | Exact distance | a numeric chip beside the handle, and the same value in the contextual bar. Drag and type are the same value |
| 11 | Create | a Construction body |

**Reserved, and only this:** Line/Polyline, Rectangle, Circle, select/delete,
grid/snap, exact numeric entry. **Explicitly not:** a geometric constraint
solver, assemblies, NURBS, or an engineering-drawing system.

**Extrude initially creates a new body.** *Add* and *Cut* appear in the operation
selector, drawn disabled, so the shell already shows where later booleans plug in
without implying they exist.

**The exactness rule carries over unchanged.** A sketch dimension and an extrusion
distance are authored values of the same kind as a box width: entered in the
display unit, converted exactly, validated natively, and never inferred back from
geometry.

---

## 8. Recommended shell, and the one alternative

### Recommended — **Forge Shell: edge rails + contextual inspector**

Everything in §5–§7. One shell class; three regions (global strip, edge rails,
contextual surface); mode and tool decide content, never structure.

**Why:**
- It is the only structure in which the four sculpt brushes and a numeric CAD
  inspector are *the same shell with different content*, which is the whole
  "mobile sculptor first + exact CAD layer in the same viewport" requirement.
- It fixes landscape by moving chrome to the **edges**, where a rail costs width
  (which landscape has) rather than height (which it does not) — see §9.
- It has real, visible homes for Sketch, Extrude, gizmo, hierarchy, undo and
  Export today, drawn dimmed, so their arrival changes content and not shape.
- Chrome collapses to nothing (N6), which no slab-based layout can do.
- It touches **nothing** about the proven native gesture arbitration.

**Costs, stated plainly:** it is the most surface to build; the rails overlay the
viewport, so their touch-opacity and their placement relative to likely stroke
areas are the one genuinely new risk; and the Construction inspector is a real
piece of engineering, not a rehousing.

### Strong alternative — **Single Sheet**

One bottom sheet with three detents carries *everything* — brush values, tool
selection, primitive fields — and there are no edge rails at all. The global
strip stays. Tool selection is a segmented row at the top of the sheet.

*Strengths:* markedly less to build; one surface to get right instead of three;
no overlay over the viewport at all, so zero new gesture risk; the numeric
inspector is naturally at home.
*Weaknesses:* it is **not a sculpting shell** — Radius and Strength are behind a
detent instead of under the thumb, which is the single most-used interaction in
Sculpt; landscape is fixed by rationing height rather than by using width; and
collapsing to a bare viewport means losing tool access entirely.

*Recommendation:* **Forge Shell**, unless the owner wants the smallest possible
step, in which case Single Sheet fixes the landscape defect for materially less
work at the cost of the sculpt-first feel.

---

## 9. Adaptive layout rules

Driven by **window** size, not display and not orientation, so split-window and
free-form are the same rule.

| class | width dp | inspector | rails |
| --- | --- | --- | --- |
| Compact | < 600 | bottom sheet, 3 detents | overlay on the viewport edges |
| Medium | 600–839 | bottom sheet, wider detents | overlay, edges |
| Expanded | ≥ 840 | **docked** side panel ≤ 280 dp | rail docked outside the viewport; hierarchy may dock opposite |

**Height override — this is what fixes the landscape failure.** When window
height < 480 dp (every phone landscape; 411 dp measured), the inspector is
**never docked at the bottom**. It becomes a right-side overlay ≤ 380 dp, and the
rails stay on the edges. The 2400 × 1080 case becomes rails + a right overlay
with the model fully visible between them.

- **Minimum viewport:** compact/medium ≥ **60 %** of window area unoccluded at
  peek detent. Expanded uses an absolute floor instead — **viewport ≥ 640 dp
  wide** — because a percentage is the wrong measure once the window is large
  (WF-4 leaves 712 dp, which is wider than an entire phone screen).
- **Detents:** hidden (handle only) → peek (the active tool's primaries) → full
  (everything, **scrolling**). Remembered per mode for the process lifetime.
- **Every inspector body scrolls.** The absence of a scroll container is the
  direct cause of the landscape defect.
- **Insets** are consumed explicitly and applied as padding to **chrome only**.
  The `SurfaceView` stays full-bleed; nothing about layout may resize the Vulkan
  surface.
- **Edge-to-edge** adopted deliberately, replacing the deprecated fullscreen
  theme.
- **IME:** keep `adjustPan`. When the IME is up, the inspector shows only the
  section being edited, and the rails hide. Portrait numeric entry keeps the
  behaviour that already works.
- **Configuration changes:** keep `configChanges` (so rotation does not destroy
  and recreate the Vulkan surface) and implement `onConfigurationChanged` to
  re-run the layout decision. The current state — absorbing them and doing
  nothing — is not an option.

**Tablet: [WF-4](wireframes/WF-4-tablet-landscape.svg)** — 1280 × 800 dp. Same
global strip, same rail, same edge sliders, all at the same size. The extra width
docks a hierarchy panel left and a tabbed inspector right. It is recognisably the
same application, and the controls did not grow — only the viewport did.

---

## 10. Inspector and hierarchy behaviour

**They are separate surfaces.** The inspector answers *what is this thing*; the
hierarchy answers *what things are there*. Conflating them is what makes editors
feel cluttered on a phone.

| | phone (compact/medium) | tablet (expanded) |
| --- | --- | --- |
| Inspector | bottom sheet, 3 detents, drag or tap handle; remembered per mode | docked side panel, tabbed (Brush / Object), always visible |
| Hierarchy | **transient** — opens from the global-strip panel icon as a full sheet, closes on selection | dockable and **pinnable**; pin state persists for the process |
| Operation / boolean history | a **section inside the hierarchy panel**, never in Sculpt | same, collapsed by default |
| Validation + stale-source | **inside the inspector or the contextual bar**, in the visible region | same |

**The hierarchy shows one object with two representations** — Construction Source
and Frozen Sculpt Mesh — not two objects. WF-4 draws this explicitly, because
getting it wrong would contradict the domain's central contract.

**The stale-source warning is never an off-screen status line again.** WF-4 shows
it as a bordered block in the inspector with the consequence stated and the
remedy (*Freeze again…*) attached to it.

---

## 11. Modes

| Mode | Status | Home |
| --- | --- | --- |
| **Construction** | exists | mode chip; rail = Select / Shape / Move / Sketch / Extrude |
| **Sculpt** | exists | mode chip; rail = Grab / Clay / Smooth / Inflate; entry carries the Freeze seam |
| **UV** | reserved | mode chip, drawn disabled with *"not yet"* |
| **Sketch** | planned | **not a mode** — a Construction tool/sub-mode with its own banner and rail |
| **Export** | reserved | **owner decision D3** — see below |

**Export re-evaluated.** Both readings remain defensible and the pack does not
choose:
- *As a global action* (in the mode chip's lower group, or the overflow sheet):
  export is a verb applied to the project, has no tools and no viewport
  interaction, and returns you where you were. This is the lighter reading and
  the one the wireframe draws.
- *As a top-level mode:* if export gains a preview, per-object selection,
  scale/unit choices and format options with their own viewport state, it earns
  a mode. That depends on export scope, which is not decided.

---

## 12. Views vs Compose

**No migration. The default recommendation stands: structured Views.** The change
of visual reference does not touch the evidence, and no new ForgeShape-specific
evidence has emerged that would justify the cost.

The decisive facts, unchanged and re-verified:

- `app/build.gradle` contains **no `dependencies { }` block at all**;
- `gradle.properties` sets `android.useAndroidX=false`;
- there are **0 Kotlin source files**.

Compose would require enabling AndroidX, the Kotlin Gradle plugin and stdlib, the
Compose compiler plugin pinned to the Kotlin version, and `activity-compose` +
`compose.ui` + `foundation` + `material3` + `runtime` with their transitive graph
— on the order of 30+ artifacts in a project that ships none. It would also place
Compose's pointer-input pipeline above the raw-`MotionEvent`-to-JNI path and the
Sculpt gesture arbitration, which are the most carefully proven behaviours in the
product. The two things Compose is genuinely better at here — `WindowInsets` and
window size classes — are a few dozen lines of Views, needed once, in one class.

**What "modernise the Views" concretely means** — this is the real deliverable of
the recommendation, and it is required for **any** shell option:

| Change | Fixes |
| --- | --- |
| Resource-backed dimensions, colours and text styles (`dimens.xml`, `colors.xml`, styles), replacing 58 inline `dp()` calls, 13 inline `sp` sizes and ~20 duplicated colour constants | audit item 8 |
| **Stable IDs** for every control, via `ids.xml` | audit items 4 and 5; unblocks all UI testing |
| Reusable components: `ToolRailView`, `EdgeSliderView`, `InspectorSheetView`, `NumericFieldRow`, `ModeChipView` | audit item 6 |
| One explicit **UI state holder** owning draft/presentation/layout state, separate from the views | §13 |
| `setOnApplyWindowInsetsListener` on the shell, applied to chrome only | audit item 3 |
| Width/height adaptive logic in one place + `onConfigurationChanged` | audit items 1 and 2 |
| Split `ConstructionPanelView` (1041 lines) into shell + inspector body + field-row component | audit item 6 |

If the owner later wants Compose ergonomics, the only safe shape is Compose for
**inspector content only**, hosted inside a Views shell that keeps the
`FrameLayout` + `SurfaceView` composition and the touch boundary. Not recommended
for 015B; recorded so the option is not lost.

---

## 13. State and event architecture

**Unchanged invariant: native geometry and product state are authoritative; the
UI owns draft, presentation and layout state only.**

| State | Owner |
| --- | --- |
| Product mode; active sculpt tool; brush radius/strength (clamped) | **native** `SculptSession` |
| Selected object; future hierarchy selection | **native** `SelectionController` |
| Active primitive kind, parameters, transform | **native** `ConstructionObject` |
| Validation verdicts and rejection reasons | **native**; UI renders them |
| Future: sketch entities, profile closure, extrusion distance, operation progress, undo availability | **native** |
| Draft primitive kind; field text mid-edit | **UI** |
| Display unit (mm/cm/m) — makes no native call | **UI** |
| Active rail selection *as drawn*, inspector detent, pinned-hierarchy state, hide-UI state, scroll positions | **UI** (layout state) |

The UI-owned list is complete; anything not on it is native. In particular the
**drawn** active tool is read back from native after the request, never assumed.

```
 gesture / control
      │
      ▼
 Android UI ─── draft / layout state ──► stays in the UI state holder, no native call
      │
      │ submit a whole section (Apply), or request a mode / tool / brush
      ▼
  thin JNI ── typed, one method per primitive; meters and degrees only
      │
      ▼
 native domain ── validate → update → publish, atomically, one entry point
      │                              │
      │ status + reason              ▼
      ▼                        MeshStore revision
 Android UI ◄── read back authoritative state         │
  (renders the verdict, never assumes it)             ▼
                                          renderer / picking → Vulkan
```

Two rules keep this directional: the UI never writes a value it has not received
a verdict for, and there is **no path from the renderer or `MeshStore` back into
the UI**. Both hold today; neither is relaxed.

---

## 14. Gesture ownership

**The proven native arbitration is preserved unchanged** — `g_strokePending`, the
8 px arming threshold, pending-then-promote, and the `hitsSculptMesh` probe that
provably cannot mutate the mesh. No proposal here moves any of it into Java.

| Input | Construction | Sculpt | Sketch (planned) | Owner and rule |
| --- | --- | --- | --- | --- |
| 1 finger, empty viewport | orbit | orbit | pan on the sketch plane | `CameraController`; tap-vs-drag by `SelectionController` |
| 1 finger, on object | tap selects; drag orbits | — | selects an entity | `SelectionController`, decided on Up |
| 1 finger, on Frozen Sculpt Mesh | n/a | **PENDING on Down** → promote at ≥ 8 px travel, else abandon | n/a | **native, unchanged** |
| 1 finger, Sketch draw | n/a | n/a | the sketch tool owns the gesture from Down | must be decided **on Down, below JNI**, exactly like the sculpt probe |
| Drag the Extrude handle | n/a | n/a | the handle owns the whole gesture from Down | hit-tested natively on Down; never in Java |
| Future gizmo handle | gizmo owns from Down | n/a | n/a | same rule — a handle hit is a Down-time native decision |
| Two fingers | pan + zoom | pan + zoom; **abandons a pending stroke, ends a live one cleanly** | pan + zoom | `CameraController` re-anchors on any pointer-set change |
| Tool rail | rail owns; **never reaches the viewport** | same | same | touch-opaque region (`onTouchEvent → true`) |
| Radius / Strength edge sliders | n/a | slider owns the whole gesture | n/a | touch-opaque; requests only, native clamps |
| Contextual inspector / sheet | inspector owns, including its scroll | same | same | touch-opaque; drag on the handle changes detent, never the camera |
| Hierarchy panel | panel owns | panel owns | panel owns | touch-opaque |
| Text field | field owns; a viewport `ACTION_DOWN` pulls focus and IME away | n/a | same | `takeFocusFromEditor`, as today |
| Stylus tip | as one finger | as one finger | as one finger | read `getToolType` **only** to prefer a stroke over an orbit on a mesh hit. **Pressure ignored** (owner decision D6) |
| System edge gestures | not intercepted | not intercepted | not intercepted | rails are inset from the true screen edge by the gesture inset so back-swipe still works |
| Long press | **not proposed anywhere** | | | reserved; adding one needs a Down-time decision like the others |

**The rule that must not break:** a gesture that becomes multi-touch navigation
never mutates the sculpt mesh — no vertex written, no `SculptRevision` minted, no
stroke committed. **Chrome-owned gestures never leak to the viewport**, which is
already true of both panels and must remain true of every new surface: rails,
sliders, sheet, hierarchy.

---

## 15. Android UI test contract

**Scope:** native self-tests keep geometry and math; integration smoke keeps the
Vulkan/input bridge; UI tests own shell behaviour, control visibility and the
UI→native call contract. **No Vulkan pixel assertions.**

**Prerequisite: stable IDs.** `contentDescription` already exists on 7 control
families and is the immediate bridge, but explicit IDs are the durable answer and
must land with the shell. Espresso's artifacts are `androidTest` scope and ship
nothing in the product APK — the one place 015B adds dependencies, adding none to
the app.

### Required identifiers

| Area | IDs |
| --- | --- |
| Modes | `mode_chip`, `mode_menu`, `mode_construction`, `mode_sculpt`, `mode_uv`, `mode_export` |
| Tool rail | `tool_rail`, `tool_rail_item_{grab,clay,smooth,inflate}`, `tool_rail_item_{select,shape,move,sketch,extrude}`, and `tool_rail_active` as a state, not a position |
| Brush | `brush_radius_slider`, `brush_strength_slider`, `brush_radius_value`, `brush_strength_value` |
| Inspector | `inspector_sheet`, `inspector_handle`, `inspector_detent` (state), `inspector_scroll` |
| Primitive | `primitive_chooser`, `primitive_option_{box,cylinder,sphere,cone,capsule}` |
| Exact fields | `field_{width,height,depth,diameter,total_height}`, `field_pos_{x,y,z}`, `field_rot_{x,y,z}`, `unit_chip_{mm,cm,m}` |
| Commits | `apply_shape`, `apply_transform` |
| Freeze | `freeze_to_sculpt`, `resume_sculpt`, `freeze_again`, `freeze_confirm_dialog` |
| Hierarchy | `hierarchy_toggle`, `hierarchy_panel`, `hierarchy_pin`, `hierarchy_item_object`, `hierarchy_item_{construction_source,sculpt_mesh}` |
| Sketch (future) | `sketch_banner`, `sketch_plane_{front,top,right}`, `sketch_tool_{line,rect,circle,select}`, `sketch_snap_toggle`, `sketch_closure_status`, `sketch_done` |
| Extrude (future) | `extrude_action`, `extrude_distance_field`, `extrude_op_{new,add,cut}`, `extrude_create` |
| Status | `status_message`, `stale_source_warning` |

### Stage 015B test coverage

| # | Test | Asserts |
| --- | --- | --- |
| U1 | Shell/mode visibility | exactly one inspector body shown; matches `NativeViewport.productMode()`, not what was tapped |
| U2 | Tool rail | tapping each tool highlights the tool **native reports**; Radius/Strength unchanged across switches |
| U3 | Primitive field switching | each kind shows only its own fields; **zero native calls** |
| U4 | Unit switching | mm→cm→m rewrites text, makes **no native call**, native values read back bit-identical |
| U5 | Invalid input | blank / non-numeric / non-positive shows an error naming the field; no native call |
| U6 | Rejection surfaced | a natively rejected value (capsule `totalHeight < diameter`) renders the domain's reason; UI does not restate the rule |
| U7 | IME exact input | field focus raises the IME, the edited section stays visible, typed value applies |
| U8 | Freeze / Resume | Resume absent before first Freeze, present after; re-Freeze shows `freeze_confirm_dialog` before discarding |
| U9 | **Adaptive layout** at 411×914, 914×411, 360×640, 600 dp split | viewport ≥ 60 % (compact/medium) or ≥ 640 dp wide (expanded); every primary action reachable; **no control clipped** |
| U10 | **Chrome gestures do not leak** — drag on rail, slider, sheet, hierarchy | **no** camera state change and **no** `SculptRevision` |
| U11 | Inspector collapse | detent transitions hidden ↔ peek ↔ full; content scrolls at full; detent survives a mode round trip |
| U12 | Insets | no control intersects system-bar, cutout or gesture insets in any configuration |

**No core UI test may depend on pixel coordinates.** U9 and U10 are the two that
would have caught the defects this audit found by hand.

---

## 16. Visual language

Original to ForgeShape, evolved from the palette already in the product rather
than replacing it, so the shell reads as the same application.

| Role | Value | Use |
| --- | --- | --- |
| Viewport | `#10151C` → `#080B10` | dark neutral, never black; the model is the brightest thing on screen |
| Chrome surface | `#131922` | strip, sheet, docks |
| Chrome overlay | `#121820` @ 86–94 % | rails over the viewport — translucent **only** where the text stays legible |
| Elevated / control | `#1C2430`, `#1B222C` | chips, fields |
| Hairline | `#2A3441` | the only divider |
| Text primary / secondary / disabled | `#E8EDF4` / `#8B99AC` / `#4A545F` | disabled is legible enough to read a reserved feature's name |
| **Accent** | `#4C8FD8`, active fill `#20456E` | **one** accent, reserved for active state and primary commit |
| Exact-value / measurement | `#E7C26B` | dimension callouts, extrusion distance, stale-source warning |
| Success / error | `#7BD88F` / `#FF8A7A` | closure confirmed; rejection |

- **Selected state is unmistakable:** filled accent + 1.5 dp accent border +
  brightened label. Never colour alone.
- **Icon + label everywhere learning cost matters** — the whole tool rail. Icons
  alone only where the meaning is universal (undo, overflow).
- **Touch targets ≥ 44 dp.** Rail items are 52 × 72 dp.
- **Translucency only where readable.** Rails are translucent because the model
  behind them is informative; sheets and docks are opaque because text sits on
  them. No decorative blur, no glassmorphism for its own sake.
- **Density:** professional, not desktop-CAD. Two edge sliders, one rail, one
  contextual surface — never three panels competing on a phone.

---

## 17. Implementation impact and risk

| | scope | risk |
| --- | --- | --- |
| `EditorShellView` + region placement + detents | ~500–700 lines | low; additive |
| Insets + size class + `onConfigurationChanged` | ~150–250 lines | low, but must never pad the `SurfaceView` |
| `ToolRailView`, `EdgeSliderView`, `InspectorSheetView` | ~400–600 lines | low |
| Rehousing the two panels into inspector bodies | mostly deletion | **medium — where proven behaviour can be lost.** Native read-back and touch-opacity must carry over verbatim |
| Resource extraction, IDs, splitting the 1041-line class | mechanical | low |
| Espresso harness + U1–U12 | new `androidTest` source set | low |
| Rails overlaying the viewport | — | **highest single risk**: touch-opacity and placement versus likely stroke areas |
| Sketch/Extrude shell surfaces | reserved only in 015B | none yet — geometry is a later stage |

**Risks to state plainly.** The current UI is runtime-verified across fourteen
stages with **no automated test protecting any of it**, so the test harness
should land with or before the shell, not after. Keeping `configChanges` is
deliberate: dropping it would destroy and recreate the Vulkan surface on every
rotation. And nothing in this pack touches native code — if an option appears to
need a JNI change, that is a signal it is drifting into the domain.

---

## 18. Owner decisions required

**D1 — Adopt the Forge Shell (edge rails + contextual inspector)?**
*Recommendation: yes.* The alternative, Single Sheet, is materially cheaper and
carries no viewport-overlay risk, but it is not a sculpting shell: it puts Radius
and Strength behind a detent, which is the most-used interaction in Sculpt.

**D2 — Primary device: phone, tablet, or both?**
Still load-bearing, though less than before: the Forge Shell is phone-first by
construction and scales up, so this no longer flips the recommendation. It
decides whether 015B builds the **expanded** class (docked hierarchy + docked
inspector, WF-4) or defers it.

**D3 — Is Export a global action or a top-level mode?**
Depends on export scope, which is not decided. Both readings are drawn as
defensible in §11; the wireframe shows the lighter one.

**D4 — Confirm the Sketch/Extrude MVP boundary.**
Proposed: Line/Polyline, Rectangle, Circle, select/delete, grid/snap, exact
numeric entry, extrude-to-new-body, with Add/Cut reserved for later booleans, and
planar-face sketching reserved. Confirm, or move a line.

**D5 — Does re-Freeze get a confirmation?**
*Recommendation: yes*, stating the consequence and labelling the button with the
verb. It is the only irreversible act in the product and currently has no guard.

**D6 — Is stylus pressure in scope for 015B?**
*Recommendation: no.* Read `getToolType` only, to prefer a stroke over an orbit.
Pressure waits for a stage that pays for it.

---

## Appendix A — superseded option set (Stage 015A, first issue)

Recorded for traceability only. **Not recommended and not to be built.**

- **Option A — Docked Inspector:** top strip + bottom mode bar + docked detented
  inspector. Cheapest; permanent chrome heavy for a phone; weak tablet payoff.
- **Option B — Rail + Contextual Inspector:** the direction that survives, in
  amended form, as the recommended Forge Shell. What changed: the rail is now
  sculpt-first and tool-bearing rather than mode-bearing (modes moved to the
  global-strip chip), Radius/Strength moved to a dedicated edge, a hide-UI
  affordance was added, and Sketch/Extrude got real reserved homes.
- **Option C — Viewport-Maximal / Transient Chrome:** maximum viewport, all
  chrome transient. Rejected: worst for entering exact numbers, which is the
  entire Construction workflow, and it hid state Stages 008–014 deliberately made
  visible.

The first issue's Mobbin section drew on commerce and photo-editing apps as shell
evidence. That basis is withdrawn; §4 replaces it.

---

## Appendix B — what was not done

Per scope: no Stage 015B implementation, no sketch or extrude geometry, no Plane,
no native change, no Compose dependency, no multi-object, no booleans, no
persistence or export, no new sculpt tools. The only changes in this stage are
this document, four wireframes, and the `PROJECT_STATUS.md` entry recording it.
