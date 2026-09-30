# External CAD benchmark (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Fetch date for every source: **2026-09-30**. Official documentation first;
community sources only where the docs are silent, marked **NON-AUTHORITATIVE**;
anything not fetched or not stated is **UNVERIFIED**, never guessed. Each row
separates **Fact** (quoted or near-verbatim from the source), **Source**
(URL, page title, version/date when shown) and **Interpretation** (ours).
The full per-product tables with every URL are the two research records this
file condenses; the URLs below are the load-bearing ones.

How the sources were reached (this matters for trust):

- Autodesk: `help.autodesk.com/cloudhelp/<year>/ENU/<product>/files/<GUID>.htm`
  serves static HTML; the `view/…?guid=` viewer is JavaScript-only. Every
  `forums.autodesk.com` thread and `autodesk.com/support` article returned 403.
- Shapr3D: `support.shapr3d.com` HTML is 403 (Cloudflare); the same help
  centre's public Zendesk JSON API returned the live article bodies with
  `updated_at` dates, so those are official text.
- Onshape: `cad.onshape.com/help` fetched directly ("Last Updated:
  September 24, 2026" on every page).
- FreeCAD: `wiki.freecad.org` is behind an anti-bot challenge; pages were read
  from the official `FreeCAD/FreeCAD-documentation` GitHub mirror (an automatic
  wiki conversion; sync date UNVERIFIED).

**One correction to the audit brief.** Shapr3D has **no iPhone app**: the
compatible-devices page lists Windows, iPadOS, macOS and visionOS only
("Android tablets, phones, and Chromebooks are not supported";
`https://www.shapr3d.com/compatible-devices`, "Compatible devices", published
2026-09-29). Its "Adaptive user interface" is selection-driven tool suggestion
on an iPad with Apple Pencil, not a phone layout. Staff on the community forum
(NON-AUTHORITATIVE) said in 2021 "we will never have an iPhone editor" and in
August 2026 that Pencil-less touch never gave a satisfactory experience. So the
honest phone comparison is **Onshape's iOS/Android app**; Shapr3D contributes
the **canvas-attached grammar** (label beside the arrow, boolean badge,
tap-empty-grid commit), not a phone layout.

## 1. B1 — Profile selection

| Product | Fact | Source | Interpretation |
| --- | --- | --- | --- |
| Fusion | "Select coplanar sketch profiles or faces to extrude. You can also select a **Sketch** in the **Browser** or **Timeline** to select all profiles and geometry in that sketch." A profile "is shaded blue when it is closed and can be used to Extrude". | `help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-REF-EXTRUDE.htm` "Extrude reference"; `SLD-EXTRUDE-SOLID.htm` "Extrude a solid body" step 2; `Fusion-Sketch/files/SKT-3D-SKETCH.htm` "Sketches in Fusion" | N coplanar profiles per Extrude; a loop inside a loop is TWO shaded profiles (ring and disk) and the user taps either or both; holes are not a user concept. Adjacent profiles becoming one body: forum snippet only ("you will get just one body"), NON-AUTHORITATIVE. Reselecting the profile in Edit Feature: probable, UNVERIFIED (support article 403). |
| Inventor | "If there is only one profile in the sketch, it is selected automatically. Otherwise, select a sketch profile." "You can use window select to quickly select multiple closed profiles within the same sketch." Older dialog: Profile "Single: Automatically selects region. To select a profile loop instead, click Select Other." "Nested: Selects multiple nested profiles." Edit: "If you click Profile to select a different profile, other values are not selectable until you select a valid profile." | `cloudhelp/2026/ENU/Inventor-Help/files/GUID-B0CFA8E7-…htm` "To Create Extruded Features" (2026); `cloudhelp/2015/…/GUID-4BBB5048-…htm` "Extrude dialog box" (2015); `cloudhelp/2016/…/GUID-FFA1F85A-…htm` "To Edit Sketched and Placed Features" (2016) | Auto-select only when unambiguous (ForgeShape's rule). REGION (hole excluded) vs LOOP (hole filled) is an explicit expert choice; regions are the default. Profile reselection inside Edit Feature is documented. |
| AutoCAD | PRESSPULL: "A bounded area is an area that contains no gaps. It can be formed by two or more overlapping objects." "Click within any closed area that is bounded by coplanar objects or edges". Multiple: Shift-select or `m`, "The distance is calculated from the last object you select." REGION: "Each closed loop is converted into a separate region. All crossing intersections and self-intersecting curves are rejected." BOUNDARY has "Island Detection". | `cloudhelp/2017/ENU/AutoCAD-MAC-Core/files/GUID-0E027B20-…htm`; `cloudhelp/2025/ENU/AutoCAD-Core/files/GUID-D696A745-…htm` "To Work With Pressing or Pulling Bounded Areas or Objects" (2025); `cloudhelp/2021/…/GUID-ADD055CD-…htm` "REGION (Command)" (2021); `cloudhelp/2024/ENU/AutoCAD-LT/files/GUID-5072D0D0-…htm` "BOUNDARY (Command)" | Pick by tapping INSIDE; bounded areas come from a planar arrangement (overlapping loops subdivide), which ForgeShape deliberately does not do. Holes are opt-in (island detection, SUBTRACT). PRESSPULL islands are undocumented and community reports call them inconsistent (NON-AUTHORITATIVE) — a warning about implicit hole semantics. |
| Shapr3D | "Select the face(s) or sketch profile(s) you want to extrude." Glossary: "Sketch profile — A region completely bounded by sketch lines and curves … selectable as its own entity". Tutorial: "Click inside the light blue shaded area, which is called a filling". Adjacent regions: "hold down shift, click here and click here. Then I should be able to drag this down, make the cut that goes all the way through." Holes: "some of these letters have internal loops and so if you're selecting … the outside, you'll also need to select inside these loops." Reselect: History card "Profile – Click/tap Edit… or Select… to choose a different sketch profile". Broken: "Select Fix.. Select a valid profile." | `support.shapr3d.com/hc/en-us/articles/7874453786908-Extrude` (updated 2026-08-26); `…/7644593398172-Glossary-of-terms` (2026-09-08); `…/13079145254172-Modify-features-with-Design-History` (2024-08-05); `…/13079130132508-Create-basic-3D-geometry` (2026-06-15); `…/11882948231196-Error-messages-in-History` (2024-04-26) | The selectable unit is the filled region, born from closure alone; the ring and the counter are separate fills; adjacent fills are picked additively and extrude as one solid with no merge question; the profile set is a retained, re-editable feature parameter; a broken reference fails visibly and is repaired by re-picking, never retargeted — the same fail-closed posture as `ProfileRegionMismatch`. |
| Onshape | "When Extrude Solid is selected at the time a sketch is open, Onshape automatically selects all the closed regions in the sketch, and if present, nested entities." Selection lists are visible and editable item by item. Tech Tip: when a sketch is hidden, a region tap "instead tries to select the face". | `cad.onshape.com/help/Content/extrude.htm` "Extrude" (2026-09-24); `…/sketch-tools.htm`; `…/dialogs.htm`; `onshape.com/en/resource-center/tech-tips/tech-tip-getting-the-right-selection` (2020-09-22, official secondary) | Multi-region by default; nested loops keep holes; the retained selection is always a visible list; hiding the sketch makes reselection a trap. |
| FreeCAD | Pad: "Select a single sketch or one or more faces". Sketcher: "The sketch must contain only closed contours … Contours can be nested, to create voids, but should not self-intersect or intersect other contours." Exclude a loop by making it construction geometry. | `wiki.freecad.org/PartDesign_Pad`, `…/Sketcher_Workbench` (via the official GitHub mirror; date UNVERIFIED) | Whole-sketch profile, no region picking; nesting-only holes; crossing loops invalid. The closest to ForgeShape's nesting model, and the least flexible. |

**Pattern.** Every reference lets the user select several coplanar regions in
one feature, treats a hole as its own region (Fusion, Shapr3D, Onshape,
Inventor "Nested") or as opt-in island detection (AutoCAD), extrudes the
selected set as ONE solid, and keeps the selection editable after the fact
(Inventor, Shapr3D documented; Fusion probable). None of them stores a merged
outline as identity; they store the picked areas and derive the boundary.

## 2. B2 — Sketch reuse

| Product | Fact | Source | Interpretation |
| --- | --- | --- | --- |
| Fusion | Sketches are Browser objects with visibility; "To edit the sketch, right-click the sketch node in the browser or timeline, then select **Edit Sketch**." Auto-hide after a feature is a PREFERENCE ("Auto hide sketch on feature creation", support-article snippet, page 403 → partly UNVERIFIED). No page says "consumed". | `Fusion-GetStarted/files/GS-THE-FUSION-INTERFACE.htm`; `Fusion-Sketch/files/GUID-0EEF7073-…htm` "Edit a sketch" | The sketch is never consumed, only hidden; reuse is implicit; the way back is an eye icon in one known place. |
| Inventor | Glossary: "Consumed sketch: A sketch incorporated into a feature … Shared sketch: A sketch used by more than one feature". "The consumed feature or sketch is nested under the feature, marked as shared, and visibility is turned off." "When a sketch or feature is reused, it is automatically shared." "After a feature consumes a shared sketch, you cannot delete the shared sketch." Caveat: "automatic consumption can result in unwanted deep nesting of browser nodes." | `cloudhelp/2018/…/GUID-3E0794C5-…htm` glossary; `cloudhelp/2022/…/GUID-42E2165A-…htm` "About Sharing Sketches and Features"; `cloudhelp/2024/…/GUID-CA9B7FB0-…htm` "To Share Sketches and Features" | Explicit consumption with explicit AND automatic sharing; a shared sketch is a dependency that cannot be deleted; the tree encodes ownership by nesting, and Inventor's own help names the cost. |
| AutoCAD | REGION deletes the originals "unless the system variable DELOBJ is set to 0"; EXTRUDE on a face "a separate extruded object is created". | `GUID-ADD055CD` REGION; `cloudhelp/2022/…/GUID-C3C37B02-…htm` "About Pressing or Pulling" | No sharing concept; no link between profile and solid afterwards. |
| Shapr3D | "Every sketch, body, plane … is defined as an item and is listed in the Items Manager" with a Visibility icon. Tutorial: one sketch drove two Extrudes at 20 mm and 80 mm; "we don't need our sketch anymore … we can just hide those"; later "double click that … add our cutout … for our first extrusion, we need to update our profile. So let's edit and then select our updated profile." "Get in the habit of turning things on and off rather than deleting items." | `…/7873936188956-Items-Manager` (2026-09-03); `…/13778812916636-Using-History-and-Extrude` (2026-07-25); `…/13079186051996-Extrude-with-Adaptive-User-Interface` (2026-09-25) | **Confirmed: one sketch, several features, different depths**, implicit sharing, the sketch stays visible until the user hides it, and each dependent's profile can be re-pointed after a sketch edit. |
| Onshape | "Sketches are automatically hidden once they are used by a feature" (Tech Tip); the sketch stays a Feature-list row with an eye icon and Edit / Show-Hide / Suppress in its context menu; a "Show dependencies" dialog lists parents/children. | Tech Tip (above); `…/PartStudio/features_and_parts_lists.htm`; `…/Home/context_menus.htm` | Hidden-not-consumed; reuse implicit by structure (interpretation: no page says "N features" in words). |
| FreeCAD | "Once sketches are used to generate a solid feature, they are automatically hidden"; the Pad "has claimed Sketch" as its tree child. Reuse by a second feature: not stated, UNVERIFIED. | `…/Sketcher_Workbench`; `…/Creating_a_simple_part_with_PartDesign` | Claimed-and-hidden; the least reuse-friendly model. |

**Pattern.** The sketch is a first-class retained object with its own
identity and visibility, hidden after use by a preference or a rule, never
consumed except by Inventor's explicit model, and reusable by later features
with the profile re-pickable per feature. ForgeShape's sketch-by-value inside
its feature (`SKETCH_FEATURE_HISTORY_AUDIT.md` §1) is the one design in this
table with NO sketch identity.

## 3. B3 — History and feature access

| Product | Fact | Source | Interpretation |
| --- | --- | --- | --- |
| Fusion | Timeline: "Lists operations performed in your design. Right-click operations in the timeline to make changes. Drag operations to change the order". Edit Feature from the Timeline; Edit Sketch from the sketch node. "Edit Profile Sketch" from a feature or a face: forum snippets only, NON-AUTHORITATIVE. | `GS-THE-FUSION-INTERFACE.htm`; `GS-WALKTHROUGH-DESIGN.htm` | Two locators (Browser = objects, Timeline = time) plus a canvas-first path; the Browser never nests a sketch under a feature. |
| Inventor | Browser: "manages access to feature and sketch editing"; End-of-Part marker moves the rollback point. "right-click the feature and choose Edit Feature. The feature sketch (if applicable) and the feature dialog box displays." "right-click the feature and choose Edit Sketch. The feature is temporarily hidden, and the sketch displays." Face pick mini-toolbar: "Edit Feature, Edit Sketch, or Create Sketch". | `cloudhelp/2017/…/GUID-36DB9904-…htm` "Part Browser Reference"; `GUID-FFA1F85A` (2016); `cloudhelp/2014/…/GUID-58B3C857-…htm` "Mini-toolbars for Direct Manipulation" | The feature's own context menu offers its sketch, so the user never expands a node to find it; canvas face pick mirrors the tree's menu. |
| Shapr3D | History (beta mid-2023, GA in 5.590, April 2024) is a right-edge sidebar: "Each step represents a feature or action with an expandable card that allows you to view or edit the corresponding parameters." Since 5.710 "tapping on a History step now preserves subsequent steps, preventing unintentional breakpoints"; rollback is an explicit draggable breakpoint. Step actions: Insert Breakpoint, Suppress, Zoom To, Rename, Duplicate, Delete. "Filter by: Related to selection"; unrelated steps collapse into one "Unrelated (n)" row (5.570). The sketch is its own step/item; the Extrude card only re-selects the profile. The History button shows an error badge even when closed. | `…/11567903089180-History` (2025-09-05); `…/13444210101788-5-590-…`; `…/15829270331292-5-710-History-sidebar-updates` (2024-09-10); `…/13026157040028-5-570-…` | **The card IS the editor** (no dialog); selecting ≠ rolling back; the CANVAS indexes the tree (tap a face → only its steps); faults surface on the closed panel's toggle. This is the mobile-appropriate model. |
| Onshape | "Each Feature created is stored parametrically, visible in the Feature list as its own entity." Editing rolls the model back with a "Final" peek; rollback bar. Mobile: "To open a Feature dialog on a mobile platform, use the three dot menu next to the Feature"; the Feature list is a handle-drawer, resizable, "On smaller screens (for example, a smart phone), the width and height can be expanded to the full screen size." | `…/Primer/creating_parts.htm` "Feature Basics"; `…/features_and_parts_lists.htm` | A phone feature list is a drawer with a per-row overflow menu, not a desktop tree. |
| FreeCAD | Tree under a Body; Tip = "the final representation of the Body as if the parametric history didn't exist", moving it rolls back and inserts. Double-click a feature → task panel; double-click a sketch → edit. Reordering "may be prevented if the object has dependencies … such as being attached to a face." | `…/PartDesign_Body`, `…/PartDesign_MoveTip`, `…/Feature_editing` | Linear list, rollback marker, and the same face-dependency rule that blocks ForgeShape's reorder (`RefusedHasDependents` pattern). |

**Pattern.** Sketch and feature are two rows; the feature reopens IN PLACE
(card or dialog) with its profile re-pickable; the sketch is edited as its own
row; the canvas is the index into the list (face → its features); on a phone
the list is a drawer or bottom sheet with a per-row overflow, never a
desktop tree.

## 4. B4 — Extrude manipulators, values, operations, extents

| Product | Fact | Source | Interpretation |
| --- | --- | --- | --- |
| Fusion | Dialog: Direction "One Side / Two Sides / Symmetric"; Symmetric measurement "Half Length: Extrudes the specified distance to each side. Whole Length: Extrudes half the specified distance to each side."; Flip "(All extent type only)"; Operation Join / Cut / Intersect / New Body / New Component; "Objects to Cut". Manipulators: "Use the manipulator handles to extrude … or enter exact values in the dialog." The on-canvas value box's screen behaviour is NOT in official help (UNVERIFIED); auto-Cut on intersection is a blog snippet (NON-AUTHORITATIVE). | `SLD-REF-EXTRUDE.htm`; `GUID-02F9ADA3-…htm` "Create solids with Press Pull"; `SLD-USE-MOVE-COPY.htm` | Drag and dialog are two doors to one parameter; Symmetric's stored meaning is a user toggle (the ambiguity ForgeShape avoids by storing per side); operation is a dropdown plus a target list. |
| Inventor | "Default / Flipped Direction / Symmetric: … using half the specified Distance A value in each direction / Asymmetric: … Distance A and Distance B". "Distance A … Dragging the manipulator will modify the value." Glossary: "A distance arrow to dynamically drag the extrusion distance"; "The value input box is used to enter numeric values". In-canvas display: "a mini-toolbar featuring command options, manipulators, and a value input box … display in-canvas close to a selected object"; "Pin Mini-Toolbar Position … remains stationary"; "By default, the mini-toolbars do not display." "During feature preview visible sketch dimensions can be edited without entering the sketch environment." | `GUID-B0CFA8E7` (2026); `GUID-3E0794C5` glossary; `cloudhelp/2019/…/GUID-B77F3FAC-…htm` "About Direct Manipulation"; `cloudhelp/2026/…/GUID-1FFC26DB-…htm` "To Display or Hide the Mini-Toolbars" | Positive distances + a direction flag (never a signed side); Symmetric stores a TOTAL (opposite of ForgeShape); the value box is screen-space chrome near the object that can be pinned; the panel is the truth and the canvas toolbar can be switched off; sketch dimensions are editable from the feature stage. Screen-vs-model sizing and hit areas: UNVERIFIED. |
| AutoCAD (dimensions) | Text Alignment: "Horizontal / Aligned with Dimension Line / ISO Standard: Aligns text with the dimension line when text is inside the extension lines, but aligns it horizontally when text is outside." DIMTIH/DIMTOH default "ON (imperial) or OFF (metric)": OFF aligns text with the line. DIMTAD: 0 centred (splits the line), 1 above by DIMGAP, 4 below. DIMSCALE "Sets the overall scale factor applied to dimensioning variables that specify sizes, distances, or offsets … does not affect measured lengths"; annotative styles force DIMSCALE 0 and derive the displayed height from a PAPER height per annotation scale. | `cloudhelp/2023/…/GUID-38DAEEF0-…htm` "Text Tab (Dimension Style Manager)"; `cloudhelp/2026/…/GUID-863065EC-…htm` "About Controlling the Location of Dimension Text"; DIMTIH `GUID-60CFA531`, DIMTOH `GUID-71B03AC9`, DIMTAD `GUID-60D1241D`, DIMSCALE `GUID-AEA309F0`; `cloudhelp/2023/…/GUID-4F448A62-…htm` "About Annotation Scale" | The technical-drawing grammar the OWNER asked for is a named POLICY: text aligned with its line (metric default) or horizontal, above or centred, offset by a gap; glyph sizes scale by ONE factor that never touches the measured value; readability is authored in paper/screen units. That is exactly the split `EXTRUDE_HUD_AUDIT.md` §4 C proposes. |
| Shapr3D | "Dimension labels appear next to a selected gizmo arrow and they display the current value … Touch and pen: Tap the dimension label to modify the value using the numpad." "Drag an arrow to move in a direction dynamically. Select an arrow to enter a precise value". Numeric entry: label → input field → in-app numpad/calculator → checkmark. "Optional: Use the Boolean badge to manually select a boolean action." Auto rules: New Body when no contact; Union when a connected profile is pulled away; Subtract when "pushed or extended directly into an existing solid body"; Intersect manual only. Card: Sides One-Sided/Symmetric; Extent Distance/To Object/Through All; Start From Profile/Offset/From Plane. Commit: "select an empty area of the grid to complete the tool." Screen-vs-model size, label rotation, hit sizes, second-pointer semantics: UNVERIFIED (not documented). | `…/7383914603548-Move-and-copy-items-using-the-gizmo` (2026-09-04); `…/7874038080668-Defining-and-calculating-numerical-values` (2026-07-17); `…/7874453786908-Extrude`; `…/10565066254108-Boolean-operations` (2026-05-26); `…/7873882619548-Adaptive-user-interface` (2026-06-23) | The live canvas set is deliberately small: arrow, label beside it, badge, draft arrow. Extent modes live in the card. Operation is INFERRED from direction and contact and shown on a badge the user can override — ForgeShape's "Cut points into the body" rule plus a readout. |
| Onshape | "Enter a depth measurement in the dialog, or use the manipulator arrow to add depth". Result type radios New / Add / Remove / Intersect; "a merge scope becomes mandatory". End types Blind / Up to next / face / part / vertex / Through all; "Check Symmetric … Alternately … check Second end position"; flip icon. Mobile: "Tap a number to open the number pad"; the dialog "will extend to full length. To collapse the dialog completely, tap the top section"; resizable with a remembered height. Whether a value is drawn AT the arrow: UNVERIFIED. | `…/extrude.htm`; `…/dialogs.htm` | Explicit operation and explicit target (never guessed); the same three extent shapes as ForgeShape (OneSide / Symmetric / TwoSides); on a phone the dialog is a collapsible bottom sheet and numbers open a custom pad. |
| FreeCAD | Pad types Dimension ("Symmetric to plane" = half each side), To last/first, Up to face, Two dimensions, Up to shape; "Negative values are not possible. Use the Reversed option instead." Pocket is the separate Cut command defaulting INTO the body. No on-canvas depth arrow documented. | `…/PartDesign_Pad`, `…/PartDesign_Pocket` | Sign is never a side; Cut-by-command with the into-body default. Nothing to borrow for direct manipulation. |

**Pattern.** Distances stay positive and a flag chooses the side; Symmetric's
stored number differs between vendors (so label the ForgeShape value "Each
side"); the value lives beside the arrow and is also typed in a panel field;
the operation is a badge or dropdown that may READ an inference from the
candidate; extent modes beyond distance live in the panel, not on the canvas;
commit is a tap on empty canvas with a checkmark fallback.

## 5. B5 — What a phone-only CAD app can take from this

1. **Region is the selectable unit, born from closure**; the ring and the
   disk are separate picks; adjacent picks extrude as one solid with no merge
   question (Fusion, Shapr3D, Onshape, Inventor). → `PROFILE_SELECTION_AUDIT.md` §5.
2. **Auto-select only when unambiguous; otherwise ask by tap, never by
   dialog** (Inventor; Onshape pre-selects ALL when opened from the sketch).
3. **Never consume the sketch; hide it by a rule the user can reverse in one
   known place; do not hide it by default on a phone** — Onshape's own Tech
   Tip documents the reselection trap.
4. **A sketch has its own identity and can drive several features**
   (Shapr3D tutorial: 20 mm and 80 mm from one sketch). → `OWNER_DECISIONS.md` D3.
5. **Value beside the arrow, editable by tapping it, with a custom number
   pad** (Shapr3D, Onshape mobile) — neither relies on the OS keyboard first.
6. **Operation is a small badge that reads an inference and allows an
   override** (Shapr3D); the target is explicit (Onshape's merge scope).
7. **Extent modes are panel options, not more arrows** (Shapr3D, Onshape).
8. **Commit by tapping empty canvas; checkmark as fallback** (Shapr3D).
9. **History is a card list and the card is the editor**; selecting a step
   does not roll back; rollback is an explicit breakpoint (Shapr3D 5.710;
   Onshape rollback bar; FreeCAD Tip). On a phone: a drawer or bottom sheet
   with a per-row overflow (Onshape mobile).
10. **The canvas indexes the tree**: tap a face → only its steps (Shapr3D
    "Related to selection"; Fusion "Edit Profile Sketch" from a face, forum
    only; Inventor face-pick mini-toolbar). A phone-sized history must be
    filterable from a tap on geometry.
11. **Annotation grammar is a named policy**: text aligned with its line or
    horizontal, above or centred, one glyph scale factor that never touches
    the value (AutoCAD DIMTIH/DIMTOH/DIMTAD/DIMSCALE).
12. **What no official source documents, so do not cite them for it**:
    whether any vendor's arrow is pixel-constant or model-scaled, whether the
    label rotates with geometry, any hit-target size in points or dp, and
    second-pointer or cancel semantics of a drag.

## 6. What this evidence does NOT license

- "Fusion does it better" — nothing above ranks products; each row is a
  behaviour with a source.
- Copying a desktop tree, a rollback bar or a dialog onto a phone; the
  phone-native evidence is Onshape's bottom sheet and drawer, plus Shapr3D's
  canvas grammar on a tablet.
- Any claim about manipulator pixel sizing or hit targets — every vendor is
  silent, so ForgeShape's numbers remain the OWNER's to set from the physical
  device.
