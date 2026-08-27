# UI-AUDIT2D — Reference catalogue

Every row was returned by a Mobbin query recorded in
[`MOBBIN-INVOCATION-LOG.md`](MOBBIN-INVOCATION-LOG.md) and the image was examined.
"Pattern" describes what the screen *does*, read from the pixels. "ForgeShape
surface" names the surface this reference is used against in
[`README.md`](README.md).

Platform is **iOS phone** for every row — the Mobbin tool exposes no Android and
no tablet platform value. Nothing here is a tablet capture.

## Family 1 — floating / contextual toolbar over a canvas (Q01)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R01 | Apple Mail (markup) | Two tiers: a **selection-attached** action bar (fill, delete, text, lasso, duplicate, trash, overflow) floating directly above the selected drawing, and a **persistent** pen tray pinned to the bottom edge that does not move when selection changes. Utility group (undo/redo/overflow) pinned top-right. | Tool Rail; transform mode selector; trailing cluster | The clearest two-tier separation in the set: contextual actions ride the object, persistent tools own a fixed edge |
| R02 | Craft | Floating bottom toolbar carrying a size stepper, font control, alignment, shape and overflow; tapping the size stepper opens a **list anchored upward out of that control** (Small/Medium/Large/Extra Large, current state check-marked). Zoom % and undo/redo in a separate fixed top-right capsule. | Add Primitive; Objects popover; precision toggle | Confirms ForgeShape's `AnchoredSurfaceView` model independently, including growth direction from the invoking control |
| R03 | Freeform | Selection-attached toolbar above the selected object; separate fixed top-centre utility capsule (undo, share, overflow) and a confirm control top-right. A secondary detail card for the selection sits above the toolbar without covering it. | Objects capsule; anchored surfaces | Shows a contextual surface and a contextual toolbar coexisting with neither covering the other |
| R04 | Obsidian (canvas) | **Vertical right-edge rail**, top-anchored, split into two groups (settings/add/refresh/fit/zoom, then undo/redo) with a visible gap between groups. Selection-attached action bar over the object. Bottom nav fixed. | Tool Rail; history capsule; selector row | The closest structural analogue to ForgeShape's right-edge column — and it is **top-anchored**, not centre-anchored |
| R05 | Apple Notes (markup) | Selection toolbar with a colour popover stacked **above** it, both floating over the canvas; persistent pen tray at the bottom. Popover and toolbar are separate layers that do not overlap. | Display settings popover; Add Primitive | Stacked contextual surfaces that respect each other's space |
| R06 | Spotify (cover art) | Bottom row of five labelled icon tabs (Labels/Text/Image/Background/Stickers) permanently occupying the lower third. | Tool Rail (as an anti-pattern) | Included as a **rejected** pattern: a permanent edge-anchored surface |

## Family 2 — inspector / property panel (Q02)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R07 | Play | Property inspector sheet over a live canvas. Rows are `label` + **right-aligned value pill** (Width `Fill`, Height `Auto`, Scale `100%`, Padding `16`, Margin `0`, 3D Transform `Default`), a toggle row, and a `More` row. A **bottom tab bar segments the inspector by concern** (Text / Layout / Position / Appearance / Interactions). A bullet marks rows differing from default. | **Exact Transform (precision surface)** | The single most transferable inspector in the set: it solves the "too long to scroll" problem by *category*, not by scrolling |
| R08 | Freeform | Property popover (table borders) floating **above** the contextual toolbar: a 6-way icon segmented control for border style, a stroke-weight row with `2 pt` and a colour swatch, then a fill row. | Display settings popover; Exact Transform | Shows a dense property group fitting in one popover with no scroll at all |
| R09 | Photoroom | Object property sheet: title row with the object thumbnail, its name, and a **`Done` in the sheet header**; three labelled toggle rows with disclosure chevrons; a 2×2 grid of secondary actions (Duplicate, Lock object, Front, Back); a full-width `Save to Brand kit`. | Exact Transform; Objects popover | The commit affordance is in the header, never below a fold |
| R10 | Pinterest | Layer thumbnail strip down the **left edge**, selected layer ringed; contextual actions (Duplicate, Delete) for that layer at the bottom, labelled `Layer 2`. | future Objects/scene list | Object list as a spatial strip rather than a text tree |
| R11 | Spotify | Tabbed slider inspector (Style / Colour / Effects) with three unlabelled-axis sliders and right-aligned numeric readouts (100, 600, 0), plus a `Randomise`. | Sculpt Radius/Strength | Tabs used to keep a slider group short |

## Family 3 — numeric editing (Q03)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| **R12** | **Play** | A dedicated numeric-entry sheet, and the most instructive screen in this audit. Top row: **preset value chips** (`240 160 48 44 32`) and a close `×`. Middle: the value **`17` set very large and right-aligned**, with a dedicated **clear (⊗) button beside it**. Left of the operator column: a **unit segmented control (`PT` / `%`) sitting directly with the value it governs**. Then a numeric keypad with arithmetic operators. | **Exact Transform**: I-08 (sign clipped), I-09 (unit chips under Scale), uiaudit1-E (append not replace) | Answers three separate accepted ForgeShape findings with one composition |
| R13 | Crouton | Numeric pad with **unit chips inline above the keys** (Item / Tablespoon / Teaspoon / Cup), the composed value echoed as `1 Tbsp butter` above them, fraction shortcuts, and `Save` in the sheet header. | Exact Transform unit chips | Second independent instance of unit-adjacent-to-value |
| R14 | Alma | Value shown very large with the unit set immediately after it (`70` `kg`), and the **prior value quoted underneath** as `Current Weight: 70.0 kg`. Commit is a full-width button above the keyboard. | Exact Transform | Shows the previous value while editing — a precision-tool courtesy |
| R15 | Tinder | Wheel picker for the value with a **unit toggle (`ft/in` or `cm`) on its own row directly under it**, and a header `✓` commit. | Exact Transform unit chips | Third instance; also shows a unit switch that re-expresses rather than converts destructively |
| R16 | Alta | Inline edit where `Cancel` and `Save` are pinned in a bar **immediately above the keyboard**, so the commit is never occluded by the IME. | Exact Transform under IME (I-02) | Directly relevant to ForgeShape's P0: what to do when the IME takes the height |

## Family 4 — object / layer list (Q04)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R17 | Play | Indented layer tree with type icons; the selected row highlighted; a **floating contextual action bar** (raise, lower, hide, overflow) over the list; and a **transient toast naming the result: "Text Field 1 is Hidden"**. | Undo/Redo verdict (I-19); status line | The toast is a *verdict*, not an instruction — exactly what ForgeShape's chrome rule permits |
| R18 | Photoroom | Flat layer list in a sheet; each row has a thumbnail, a name, an eye toggle and a drag handle; `Done` in the sheet header; an opacity value carried in the row name (`White Smoke - 74%`). | future scene list; Exact Transform header | Commit in header again; state carried in the row itself |
| R19 | Notion | Property visibility split into **`Shown in table` / `Hidden in table` groups** with `Hide all` / `Show all` bulk actions and a search field. | Display settings popover | Grouping by state rather than one long toggle list |
| R20 | Bevel | Hidden data source rendered **dimmed in place** rather than removed from the list. | disabled-control semantics (I-18) | A disabled/hidden item stays visible and legibly inert |

## Family 5 — 3D / AR object manipulation (Q05, Q15)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R21 | Best Buy | AR product with a **wireframe bounding box** around it, a four-icon contextual toolbar attached to the box's top edge (Info, ?, ?, close), **in-scene dimension pills billboarded along the edges (`8.2"`, `2.2"`)**, a top-centre `3D` / `AR` segmented switch, and a bottom row of three labelled actions. | gizmo (I-04); Exact Transform; future CAD dimensions | The only reference in the set that shows measurement *in the scene* on a manipulable 3D object |
| R21b | Best Buy | The same screen with the dimension overlay **off**: bounding box and attached toolbar present, no dimension pills. | as R21 | Proves the reference treats on-scene dimensions as a toggleable overlay, not permanent chrome |
| R22 | Target | AR object with a **persistent on-screen D-pad** (4-way translate) flanked by two rotate arrows, plus a thumbnail, `Add to cart`, a shutter and a dimensions toggle. | gizmo — **rejected** | A nudge-pad substitute for direct manipulation; catalogued to be argued against |
| R23 | Apple Store | `AR` / `Object` segmented switch pinned top-centre, close left, share right; the model owns everything else; one attribute line at the bottom (`Color Cosmic Orange`). | Global Toolbar; transform mode selector | Minimum viable chrome over a 3D view |
| R24 | Crate & Barrel | Same top-centre segmented switch; a bottom sheet carries the object's identity and value. | Global Toolbar; Objects capsule | Second instance of the same convention |
| R25 | Tonal | Object mode with **only** the mode switch and a close control; no other chrome at all. | Hide UI | Confirms that a 3D view can carry near-zero chrome and still be usable |

## Family 6 — brush / parameter sliders (Q06)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R26 | Edits | Tool tab row, then a colour swatch row, then three named sliders (`Hue`, `Saturation`, `Brightness`) whose **tracks are painted with the value they control**. | Sculpt Radius/Strength | The track itself communicates the parameter |
| R27 | Google Arts & Culture | Brush **`Size` shown as a physical tapered wedge** whose thickness at the thumb is the actual brush width, beside named filter sliders. Undo and Clear as bordered text buttons. | **Sculpt Radius (I-16, the `120 px` unit)** | Removes the need for a unit entirely by showing the brush at true size |
| R28 | Telegram | Adjustment rows with the **signed value right-aligned in an accent colour** (`+0.72`, `−0.59`, `+0.54`); a detent tick marks the neutral point on the track. | Exact Transform (I-08 sign clipping); Sculpt sliders | Signed values that cannot lose their sign, and a visible neutral |
| R29 | Apple Photos | A **tick-ruler scrubber with a centre detent** as the numeric input; undo/redo pinned top-left in a fixed capsule; three labelled mode tabs at the bottom. | Rotate handle; future numeric scrubbing | Keyboard-free numeric adjustment sized for a thumb |
| R30 | Shopee | Slider with the **value floating above the thumb** and a `Reset` beside the track; tool chips below, the active one ringed. | Sculpt Radius/Strength | Value at the point of manipulation |
| R31 | eBay | A single parameter given a whole screen: title `Brightness`, `Apply` in the header, one slider at the bottom, image full-bleed. | Exact Transform | Commit in the header on a full-screen editor |

## Family 7 — history / undo (Q07)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| **R32** | **Wabi** | History rows **named by the act performed**: "Title changed to Meal planner recipe", "Cover image updated", "Primary content model updated", "Reverting back to version 2" — each with a version tag and a per-row revert control. | **Undo/Redo (I-19)** | The decisive reference: a history step is identified by *what it did*, not by a number |
| R33 | Canva | Version list with `Current version` + author + relative time, then autosaved entries with absolute timestamps, grouped `Today` / `This week`, some starred. | future project history | Naming and grouping of an autosave history |
| R34 | Mimo | `Current version` radio-marked, `Past versions` below with a restore control each; the whole thing behind a `History` tab. | future project history | History as one tab among peers, not a permanent surface |
| R35 | Obsidian | File recovery list: timestamp plus **byte size** per entry, with a search field. | future project history | Cheap, honest metadata per entry |
| R36 | Squarespace | Restore guarded by an explicit confirm dialog naming the object. | Reset Sculpt from Shape… | Confirms ForgeShape's existing re-Freeze confirmation is conventional |

## Family 8 — project / home hub (Q08, Q12)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R37 | Edits | `Projects` title, two-up thumbnail grid, each with name and **`1d · 64 MB` metadata**, a `+` FAB bottom-right, and a five-icon bottom nav. | future Project/App Hub | Metadata that matters to a creator: age and weight |
| R38 | Riveo | **`New Project` is a dashed-outline tile occupying the first grid cell**, a peer of the existing projects rather than a separate control. | future Project/App Hub | Creation lives in the grid, so the empty state and the populated state are the same layout |
| R39 | ElevenLabs | `Create new` as a full-width primary button directly under the title, then filter chips, then `Recent projects`. | future Project/App Hub | Creation promoted above browsing |
| R40 | CapCut | A large `New project` banner, then a project list with **thumbnail, date, size and duration** per row, and a tool shortcut grid above. | future Project/App Hub | List form with dense per-item metadata |
| R41 | Canva | Projects grid with per-item overflow menus and star toggles; item subtitle names the *document type* (`Square Sticker`, `Coaster (US/CA)`); an untitled item shows its **dimensions** (`40 × 40 px`) as its subtitle. | future Project/App Hub | For ForgeShape the analogue subtitle is the primitive kind and its dimensions |
| R42 | Google Drive | Mixed-type grid with a per-type badge on each tile and a per-item overflow. | future Project/App Hub | Type legibility at a glance |
| F01 | Play (flow) | New project: projects list with `+ New Project` → an **empty canvas with the object centred and all controls at the edges** → progressively filled. | future Project/App Hub; start chooser | The empty editor is the same layout as the full one |
| F02 | Unfold (flow) | `Choose a Starting Point` in an anchored sheet with three large equal tiles (Media / Template / Blank) → editor with `Tap to add template` in the empty canvas and a bottom content row. | start chooser | Independent confirmation of ForgeShape's two-option start chooser shape |

## Family 9 — settings / preferences (Q09)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| **R43** | **GitHub** | `Code Options`: a **live preview of the thing being configured sits at the top of the settings screen**, above the `Display` and `Editing` groups whose toggles change it. | future Settings Hub | The strongest idea for a Settings Hub in a visual tool: show the effect, do not describe it |
| R44 | Obsidian | Each setting carries a **one-line plain-language explanation** under its title; defaults are dropdowns (`Reading view`, `Live Preview`); grouped into cards by concern. | future Settings Hub; Display settings | Explanations that make a dense preference list readable |
| R45 | Google TV | Icon-led grouped rows; a row's **current value is its subtitle** (`Download quality` / `SD (Faster, saves space)`); disclosure and toggle rows mixed in one group. | future Settings Hub | Current value visible without opening the row |
| R46 | MasterClass | Sectioned by concern with all-caps section labels and a one-line explanation per setting. | future Settings Hub | Same principle, different visual weight |
| R47 | Wispr Flow | Grouped cards by concern (`Audio`, `Personalization`, `Data and privacy`), an upgrade row at top, some rows expandable. | future Settings Hub | Grouping vocabulary |
| R48 | Replika | Disclosure rows first, then a dense run of toggles; a `Version history` row shows its **current value as a subtitle** (`Stable`). | future Settings Hub | Mixed disclosure/toggle in one screen |

## Family 10 — mode switcher / segmented control (Q10)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R49 | Craft | `Select Cells` sheet offering a **scope choice** — `Cell` / `Column` / `Row` — as three equal labelled tiles, the active one filled, with a dismiss `×`; the affected geometry is highlighted in the document behind. | **World/Local coordinate-space selector** | A scope selector drawn as equal peers with the affected region shown — the closest analogue to World/Local |
| R50 | Google Photos | Contextual action row (Add photos / Edit caption / Remove photo / Replace photo) **above** a fixed two-tab bar (`Layout` and `Edit`); the tab bar never moves when the action row changes. | transform mode selector; Tool Rail | Contextual tier changes without displacing the persistent tier |
| R51 | Linktree | Horizontally scrolling icon+label mode chips (`Text`, `Layout`, `Thumbnail`, `Re…`), active one filled, form below. | Tool Rail | Overflow handled by scrolling, not by shrinking |
| R52 | Bears Gratitude | Inline formatting bar (`Heading` / `Text` / `Bold` + actions) pinned above the keyboard, active state filled. | selector row under IME (I-02) | A selector that survives the IME by moving with it |
| R53 | Posh | Four labelled icon+text mode tiles in a row (`Flyer`, `Video`, `Font`, `Theme`) with a primary action beneath. | Tool Rail | Icon+caption entries, matching ForgeShape's existing rail |

## Family 11 — contextual action menu (Q11)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| **R54** | **Freeform** | Object menu combining three registers in one surface: a **top icon row for spatial ordering** (`Back` / `Front`), a grouped list of acts (Cut, Copy, Duplicate, Lock), and **check-marked state rows** (`✓ Round Corners`, `✓ Constrain Proportions`) plus a `Style ›` submenu. | Display settings popover (I-06); future CAD constraints | Shows acts and *states* in one menu — and `Constrain Proportions` is literally a CAD constraint expressed for touch |
| R55 | Obsidian | Grouped action list, related acts banded together, **destructive `Delete` last and in red**, ellipsis on acts that open further UI (`Move file to…`, `Rename…`). | Reset Sculpt from Shape…; future menus | Conventional grouping and destructive placement; the `…` convention ForgeShape already uses |
| R56 | Craft | Menu anchored to its invoking control, mixing acts, a starred toggle, a `View ›` submenu and a red `Delete`. | anchored surfaces | Anchoring plus submenus without a second surface layer |
| R57 | Play | Icon row (cut/copy/paste/hide) above a list with submenu disclosures (`Properties ›`, `Add to New Stack ›`, `Auto Fill ›`). | future Objects menu | Two-register menu again |
| R58 | Notion | `Actions` sheet with **inline toggles inside the menu** (`Header row`, `Header column`), grouped acts, red `Delete`, and a footer line `Last edited by …`. | Display settings popover | Toggles living inside an action menu rather than in a separate settings surface |

## Family 12 — dense timeline tooling (Q13)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| R59 | GoPro Quik | Undo/redo and `Reset` in a fixed row that never moves; a **zoom readout `100%`**; a playhead time readout `00:01` pinned above the timeline; a bottom row of labelled tools; a coach line `Tap or hold to trim`. | history capsule; status line | A dense pro surface where the persistent controls hold still while the content scrolls under them |
| R60 | Netflix | Clip trimming with **`Start 1:00` and `End 1:15` printed beside the handles they belong to**, and the selected duration `0:15` printed on the selection itself. | **Exact Transform; gizmo drag feedback** | Numeric values placed at the handle that changes them |
| R61 | Shopee | A transient orange coach chip `Drag to resize the clip` attached to the object, plus labelled tool icons and a duration badge on the selection. | status line — **rejected** | An instruction, not a verdict; catalogued to be argued against |

## Family 13 — on-scene measurement (Q14)

| REF | app | pattern observed | ForgeShape surface | why selected |
| --- | --- | --- | --- | --- |
| **R62** | **Redfin** | AR measurement: **dimension labels drawn on the edges they measure** (`6'1"`, `7'11"`, `10"`), the measured region named and quantified in a corner card (`Kitchen, Living Room` / `550 ft²`), the active vertex marked, a transient instruction `Drag to edit the current point`, a small three-icon utility cluster top-right, and one large `+` primary action. | future CAD sketch/dimension UI; Exact Transform | The most CAD-like screen in the library; shows how a touch tool exposes exact numbers without a panel |
| R63 | Target | AR object with **one dimension pill per axis** (`16.0 in.`, `30.7 in.`, `12.1 in.`) floating beside the corresponding extent. | future dimension overlay | Per-axis labelling that maps directly onto a box primitive's three parameters |
| R64 | Bump | A single very large measurement readout under the measured object, and nothing else. | future dimension overlay | The minimal case: one number, no chrome |
