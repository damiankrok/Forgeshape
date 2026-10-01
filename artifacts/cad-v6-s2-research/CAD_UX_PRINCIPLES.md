# CAD UX principles — evaluated

Task `CAD-V6-S2-RESEARCH-FILL-PICK-HUD3D-R1`. Each principle is marked ADOPT,
MODIFY or REJECT, with its source. "Doc" means an official page quoted in
`CAD_REFERENCE_RESEARCH.md`. "Evidence" means this task's source reading and
scratch reproduction.

| # | principle | verdict | source / evidence | binding form for ForgeShape |
| --- | --- | --- | --- | --- |
| 1 | The bounded profile/cell is the selection entity | **ADOPT** | Doc: AutoCAD "bounded area … formed by two or more overlapping objects"; Shapr3D "a region completely bounded … selectable as its own entity". Evidence: `PlanarFaceRef` already names atomic faces by entity ids and cut ordinals. | A tap names the atomic face under the finger. A source loop, circle or spline is never the unit, and the infinite exterior is never a face. |
| 2 | Selection is independent from operation validity | **ADOPT** | Doc: none of the four products describes refusing one area because of another (inference, labelled). Evidence: `togglePlanarFace` merge-validates every add (`forgeshape_sketch_session.cpp:1652-1668`); the kernel accepts point-touching shells (K1–K6). | `FaceSelectionSet ≠ ExtrusionToolComponents`. The tap checks the cap only. |
| 3 | The user builds a selection set incrementally | **ADOPT** | Doc: AutoCAD Shift / Multiple; Inventor window select of "multiple closed profiles"; Fusion "one or more coplanar sketch profiles"; Shapr3D "face(s) or sketch profile(s)". | Toggle in any order. The result depends only on which faces were tapped an odd number of times. |
| 4 | Operation refusal happens at preview/commit, not at the tap that builds the selection | **ADOPT** | Doc: Inventor "During feature preview visible sketch dimensions can be edited" (the preview is where the feature is judged). Evidence: the OWNER's 7 → 6 → refused sequence reproduces exactly from the tap-time merge. | Refusals are named on the preview (status line + badge), the selection is never trimmed, and Extrude is withdrawn while invalid. |
| 5 | The in-canvas gizmo should be minimal | **ADOPT** | Doc: Fusion keeps Direction / Extent / Operation in the dialog; Shapr3D shows a gizmo + ONE Boolean badge. | Arrow + value + ONE operation badge. The extent and Flip move into the palette the badge opens. |
| 6 | The exact value stays close to the manipulator | **ADOPT** | Doc: Shapr3D "Dimension labels appear next to a selected gizmo arrow"; Inventor "mini-toolbars display in-canvas close to a selected object". Evidence: the OWNER accepts today's leader + value as attached. | Keep the leader and value unchanged. |
| 7 | Detailed operation choices belong in a badge/palette/property surface | **ADOPT** | Doc: Fusion dialog; Shapr3D "Boolean badge menu"; Shapr3D History settings (Sides, Extent). | Badge → existing palette; precision surface as the always-available fallback. |
| 8 | The visual gizmo anchor is geometry/world-derived | **MODIFY** | Doc: Shapr3D "gizmo center snaps to existing geometry … reorient the gizmo" (shows the principle, not ForgeShape's rule). Evidence: the leader reads as attached because it is world geometry; the plate does not because it is placed after projection. | Anchor, orientation and size are world-derived (axial billboard in the leader's plane; the one per-frame manipulator scale). MODIFIED with two explicit presentation rules: hidden below `kCadFeatureViewMinAxisSine`, and hidden whole when off-viewport. There is no screen-edge rescue, and the hit area keeps the 48 dp floor. |
| 9 | Visible interactive cells are selectable from either side unless genuinely edge-on or occluded by intentional policy | **ADOPT** | Evidence: front ≡ back, value for value (`PICKING_AUDIT.md` §3); the real misses are routing (orbit during a tap) and HUD intercept, not side. | Never add plane culling. Resolve a tap against the camera captured at Down. Only the drawn arrow HEAD claims a still tap. Edge-on is a stated threshold (sin 3°), refused with a token. |
| 10 | The draw and pick contract must agree | **ADOPT** | Evidence: the arrow already shares `cadExtrudeArrowPoint` for draw, hit and anchor. Today's panel proxy claims its AABB, which is more than it draws. | Every interactive HUD element claims exactly what it draws, plus the 48 dp floor and nothing else. The dock's corners come from the one native frame function that also decides visibility. |

## Consequences that are rules, not tuning

- No selection tap is ever refused because of another selected cell.
- No HUD control is placed by a screen-edge fitting policy.
- No HUD proxy claims a touch outside its drawn shape plus the 48 dp floor.
- No picking change touches `intersectRayPlane` or `buildPickRay`. Both are
  proven correct and shared with the gizmo.
