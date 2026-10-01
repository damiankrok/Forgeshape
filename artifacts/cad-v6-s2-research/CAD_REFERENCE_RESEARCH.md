# CAD reference research — fill selection, picking, Extrude HUD

Task: `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1` (read-only). Baseline `origin/main`
`6c9f156`, branch `feature/cad-v6-sketch-face-r1` `bbae765`.

Every quotation below was re-read from the live official page on 2026-10-01
(Autodesk pages fetched directly; Shapr3D pages from the official support
centre). A **claim** is what the page says. An **inference** is ours and is
labelled as one. Nothing here claims what any product's kernel does with a
selection the page does not describe — in particular no page below says what
happens when two chosen profiles meet only at a point.

---

## 1. AutoCAD — `PRESSPULL`

| Source | URL |
| --- | --- |
| *To Work With Pressing or Pulling Bounded Areas* (AutoCAD for Mac 2018) | https://help.autodesk.com/cloudhelp/2018/ENU/AutoCAD-MAC-Core/files/GUID-0E027B20-D155-45AE-8A7C-58C64FBF1899.htm |
| *To Work With Pressing or Pulling Bounded Areas or Objects* (AutoCAD 2025) | https://help.autodesk.com/cloudhelp/2025/ENU/AutoCAD-Core/files/GUID-D696A745-3984-405C-90BE-D523E78E524F.htm |
| *PRESSPULL (Command)* (AutoCAD 2016) | https://help.autodesk.com/cloudhelp/2016/ENU/AutoCAD-Core/files/GUID-9D072BB2-D97F-41DB-8414-41BC83A16EFA.htm |

**Claims (verbatim):**
- "A bounded area is an area that contains no gaps. It can be formed by two or more overlapping objects." (2018 Mac)
- "Click any closed area that is bounded by coplanar objects or edges." (2025)
- "Press and hold Shift as you select the object, face, or area" / "Then enter m (Multiple) and continue to make selections." (2025)
- "Objects used for a multiple press or pull operation must all support the same type of behavior (extrusion or offset)." (2025)
- Selection table: "Inside a bounded area — Extrudes to create a 3D solid"; option "Multiple — Specifies that you want to make more than one selection. You can also Shift+click to make multiple selections." (2016)

**What it proves.** The pick unit is the bounded AREA under the click, which
overlapping coplanar objects form; it is independent of which source object
drew which edge. Several areas can be gathered into one operation.

**What it does NOT prove.** Nothing about areas that meet only at a point, nor
about whether a multi-area press/pull is one boolean or several. The one
stated multi-selection constraint is behavioural (all extrude or all offset),
not topological.

**ForgeShape lesson.** A tap names the atomic face under the finger — the
`PlanarFaceRef` the arrangement already derives. That is already true; what is
NOT true today is that a set of them can be gathered freely (see
`FILL_SELECTION_MODEL.md`).

---

## 2. Autodesk Inventor — Extrude

| Source | URL |
| --- | --- |
| *To Create Extruded Features* (Inventor 2026) | https://help.autodesk.com/cloudhelp/2026/ENU/Inventor-Help/files/GUID-B0CFA8E7-D3F5-48A9-BE6F-C8DF668301BD.htm |
| *About Direct Manipulation of Objects* (Inventor 2019) | https://help.autodesk.com/cloudhelp/2019/ENU/Inventor-Help/files/GUID-B77F3FAC-5837-4C06-823A-735DB508248B.htm |

**Claims (verbatim):**
- "Valid geometry can be: A new sketch, with one or more profiles, that have not been used to create any features."
- "If there is only one profile in the sketch, it is selected automatically. Otherwise, select a sketch profile."
- "You can use window select to quickly select multiple closed profiles within the same sketch."
- "Important: During feature preview visible sketch dimensions can be edited without entering the sketch environment."
- In-canvas display: "It typically contains a mini-toolbar featuring command options, manipulators, and a value input box." "The value input box at the top of the mini-toolbar specifies numeric values for modeling and editing operations." "The mini-toolbars display in-canvas close to a selected object in the graphics window." (2019)

**What it proves.** The feature's input is a SET of profiles from one sketch,
built by selecting (including window select). The auto-select rule is exactly
ForgeShape's "Finish auto-selects only when one area exists". Dimensions stay
editable during the preview. The in-canvas instrument is small (options,
manipulator, value) and stands *near the object*.

**What it does NOT prove.** No statement that selecting a profile can be
refused because of the other profiles already selected, and none that it
cannot. No statement about point-touching profiles. The 2019 mini-toolbar page
does not say the toolbar is drawn in world space or in screen space.

**ForgeShape lesson.** Selection is a set the user builds; validity of the
operation is a property of the feature, judged in its preview. Exact values sit
close to the manipulator.

---

## 3. Autodesk Fusion — Extrude

| Source | URL |
| --- | --- |
| *Extrude a solid body* | https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-EXTRUDE-SOLID.htm |
| *Extrude reference* | https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-REF-EXTRUDE.htm |
| *Create solids with Press Pull* | https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/GUID-02F9ADA3-7556-42A9-8AD1-552728D537AB.htm |

**Claims (verbatim):**
- "In the canvas, select one or more coplanar sketch profiles or planar faces." (Extrude a solid body)
- "Or in the Browser or Timeline, select a Sketch to select all profiles and geometry in that sketch, including sketch text, in one step."
- The dialog carries Type, Start, Direction (One Side / Two Sides / Symmetric, with Half Length / Whole Length), Extent Type (Distance / To Object / All), Flip, Taper Angle and Operation (Join / Cut / Intersect / New Body / New Component).
- "Use the manipulator handles to extrude, fillet, or offset geometry, or enter exact values in the dialog." (Press Pull)

**What it proves.** Input is one or more coplanar profiles or planar faces.
The in-canvas manipulator and the dialog are two doors to one value. The
detailed options (direction, extent, operation, taper) live in the DIALOG.

**What it does NOT prove.** The current *Extrude a solid body* and *Extrude
reference* pages do not themselves describe the in-canvas arrow, its value
box, or where either is drawn; the manipulator statement is from *Press Pull*.
Nothing about point-touching profiles.

**ForgeShape lesson.** Keep the in-canvas instrument focused on distance; the
operation and extent choices belong in a small badge that opens a palette (or
the precision surface), not in a permanent arrow-attached plate.

---

## 4. Shapr3D

| Source | URL |
| --- | --- |
| *Glossary of terms* | https://support.shapr3d.com/hc/en-us/articles/7644593398172-Glossary-of-terms |
| *Extrude* | https://support.shapr3d.com/hc/en-us/articles/7874453786908-Extrude |
| *Move and copy items using the gizmo* | https://support.shapr3d.com/hc/en-us/articles/7383914603548-Move-and-copy-items-using-the-gizmo |

**Claims (verbatim):**
- Sketch profile: "A region completely bounded by sketch lines and curves, forming a closed planar shape with edges and vertices. In Shapr3D, a sketch profile is selectable as its own entity and can be used as input for modeling tools such as Extrude or Revolve to create 3D bodies." (Glossary)
- "Select the face(s) or sketch profile(s) you want to extrude." "Use the gizmo to extrude the selected items to your desired distance." "Optional: Use the Boolean badge to manually select a boolean action." (Extrude)
- "The type of Boolean operation created is automated based on geometric conditions." "An intersection is not applied automatically and must be selected manually from the Boolean badge menu." (Extrude)
- History settings list Profile, Sides (One-Sided / Symmetric), Extent (Distance / To Object / Through All), Distance, Draft Angle. (Extrude)
- "Dimension labels appear next to a selected gizmo arrow and they display the current value for the movement." "Touch and pen: Tap the dimension label to modify the value using the numpad." (Gizmo)
- "The gizmo center snaps to existing geometry such as axes, faces, edges, sketch profiles, and construction geometry, allowing you to define a new center for any rotational movement or reorient the gizmo." (Gizmo)

**What it proves.** On a touch-first CAD product: the bounded region is a
first-class selectable entity independent of the curves that bound it;
Extrude takes several; distance is a gizmo; the value is a label next to the
arrow, tapped to type; the operation is ONE small badge with a menu; the
gizmo's centre and orientation follow geometry.

**What it does NOT prove.** Whether the Boolean badge is drawn in world space
or screen space; how the badge behaves at viewport edges; what happens for
profiles touching at a point.

**ForgeShape lesson.** World/geometry-attached arrow + readable value label
beside it + ONE contextual badge. This is the target shape (see
`HUD3D_OPTIONS.md`).

---

## 5. Cross-reference summary

| Question | AutoCAD | Inventor | Fusion | Shapr3D | ForgeShape today |
| --- | --- | --- | --- | --- | --- |
| Pick unit | bounded area | profile | profile / planar face | sketch profile | atomic planar face (correct) |
| Multi-select | Shift / Multiple | window select | one or more | face(s)/profile(s) | yes, but each ADD is merge-validated (wrong) |
| Selection refused by other selections | not stated | not stated | not stated | not stated | yes — `PlanarFacesTouchAtPoint` at tap |
| Where the value is | command line | mini-toolbar near object | dialog + manipulator | label beside the gizmo arrow | label above the leader (works) |
| Where operation choice is | n/a | dialog | dialog | Boolean badge | 3-glyph plate past the arrow (rejected) |

**Inference, labelled:** none of the four documents a selection model in which
choosing one area can be refused because of a different area already chosen;
all four describe selection as gathering. Deciding whether the result is
buildable is described (where it is described at all) as part of the
operation. That is the separation `FILL_SELECTION_MODEL.md` adopts.
