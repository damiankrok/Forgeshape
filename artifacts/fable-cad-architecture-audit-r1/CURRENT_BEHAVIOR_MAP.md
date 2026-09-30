# Current behaviour map (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

What ForgeShape does today, traced from source (`SOURCE_CONFIRMED` unless
tagged). Baseline `main = 509de02`. This is the mental model a developer needs
before reading the audits; the audits carry the line-level evidence.

## 1. The CAD mental model, in eight sentences

1. A **CAD Body** is a `SceneObject` whose truth is a **feature chain**: one
   base New Body feature (a sketch on a world plane or another body's planar
   face, plus an extrusion), then up to 15 later Add or Cut features, each
   with its OWN sketch on a planar face of an EARLIER feature of the same body
   (`cpp/forgeshape_cad_body.h:245-284`).
2. A **sketch** is authored entities (Line, Polyline, Rectangle, Circle, Arc,
   Spline) with per-entity ids; it has **no identity of its own** and lives by
   value inside its feature (`cad_body.h:268, :279`).
3. Closed loops are derived, fail closed by name, and become **regions** by
   nesting: every loop is one region (its interior minus its direct
   children); touching or crossing loops are never holes
   (`cpp/forgeshape_sketch_region.h:1-56`).
4. An **extrusion** selects regions by semantic identity (outer anchor + the
   holes it was chosen with), reaches two non-negative distances along ±N in
   one of three modes, on one side (One Side), and does one of three
   operations (`cad_body.h:123-157, :223-228`).
5. A **selection may not contain a region beside its own hole**
   (`sketch_region.cpp:329-334`), and a tap on such a region silently replaces
   the conflicting one (`cpp/forgeshape_sketch_session.cpp:1318-1327`).
6. Everything else — loops, regions, triangles, prisms, booleans, faces,
   frames, lineage — is **derived** through one path, `regenerateCadBody`,
   ordered and atomic, and never stored (`cad_body.cpp:586-729`).
7. The **sketch session** is volatile: nothing before its one-transaction
   commit is truth; Finish moves the session into Ready where the extrusion is
   authored at the geometry through one writer (`applyExtrudeFeature`); the
   preview IS the candidate the commit applies.
8. **Persistence** is `CADB` v1–v5 inside the all-or-nothing `.forge`
   document; a version is written only when a body needs it; 44 fixtures pin
   the bytes against an independent PowerShell encoder.

## 2. The extrude workflow, step by step

| User act | What happens | Where |
| --- | --- | --- |
| New Project → CAD | one volatile sketch session on XY seen along +Z, owning no `ObjectId` | `commitFirstCadProject` creates the project on the first Extrude (`cpp/forgeshape_project_bootstrap.cpp:19`) |
| New Sketch on a body | the spatial support chooser picks a planar face (or a world plane by name); a face of ANOTHER body → a new following body (`TopoRef`); a face of the SAME body → Add/Cut offered (`CadFeatureSupport`) | `forgeshape_jni.cpp:4036-4062`; `sketch_session.cpp:123-149` |
| Draw | single finger belongs to the sketch (never orbits); two fingers pan/pinch; creation, deletion, switching, Undo/Redo refused | `CLAUDE.md` sketch rule; `touchEvent` `jni:7595-7630` |
| Finish Sketch | loops → regions; auto-select ONLY when exactly one region exists in total (`:1256`); otherwise nothing chosen, no arrow, Extrude withdrawn, `AmbiguousProfile` on commit | `sketch_session.cpp:1215-1261` |
| Ready | the view tilts to a pose that can see the axis (`cadFeatureViewPose`); a single finger navigates; a tap that does not travel toggles the region under it; the arrow's head can be dragged (frozen basis, 1 mm floor, second pointer cancels) | `jni:339`; `sketch_session.cpp:783-880`; `cad_extrude_tool.cpp:273-414` |
| HUD | one row centred on the shaft midpoint: extent selector, exact value (tap to type), operation badge, Flip (One Side only); 48 dp hit boxes that are also the drawn boxes; glyph 24–32 dp with a 0.80–1.60 band; Tool Labels adds 11 sp captions | `CadExtrudeCanvasView.java`, `CadHudPresentation.java`, `cad_extrude_tool.h:185-188` |
| Preview | the target's draw item is substituted by the candidate mesh (or a New Body item appended), tinted by operation; an invalid candidate is named and Extrude withdrawn | `jni:1655-1709` |
| Extrude | New Body: one `addCadBody` in one `ScopedConstructionEdit`; Add/Cut: one `applyState` on the same `SceneObject`; one Undo either way | `sketch_session.cpp:1632-1737` |
| Later: Edit Sketch chip on the canvas | reopens the BASE sketch only | `CadExtrudeCanvasView.java:903-928` → `sketchBeginEdit` |
| Later: feature list | precision surface → Shape body → rows "N. Extrude — Add/Cut", withdrawn for a one-feature body; a row reopens that feature in Ready on its own extrusion; Add ↔ Cut switchable; regions re-toggleable; Finish is one step | `CadFeatureEditorView.java:233-269`; `sketch_session.cpp:231-287` |
| Later: typed base fields | the Shape panel's rectangle/circle/depth fields edit FEATURE 1 only through a second door (`cadApply*`) that still regenerates the whole chain | `jni:5731-5752` |
| Upstream edit | a size edit carries dependents (lineage over tokens, not sizes); a structural edit is refused by name for the dependent feature or body | `cpp/forgeshape_cad_feature.cpp:169-252` |

## 3. Three things the OWNER cannot do today, and exactly why

| Wish (O1–O3) | Why not | Audit |
| --- | --- | --- |
| Controls that read like a dimension annotation and shrink with the model | six floors hold the HUD at screen size; the 48 dp hit box is the drawn box; the value is ON the shaft, not beside a leader; the overlay and the HUD read `metersPerPixel` at two different depths | `EXTRUDE_HUD_AUDIT.md` |
| Extrude the rectangle material PLUS circle A (one hole left) | the selection rule refuses a region beside its own hole, and the tap path drops the ring instead of refusing; no union of regions exists | `PROFILE_SELECTION_AUDIT.md` |
| Find, show, reuse and re-edit Face Sketch B as a thing of its own | a sketch has no id, lives by value in its feature, is never drawn after commit, and the only canvas affordance reopens the base; the feature list is two surfaces deep | `SKETCH_FEATURE_HISTORY_AUDIT.md` |

## 4. Where each kind of state lives

| Kind | Examples | Owner |
| --- | --- | --- |
| Durable truth (`.forge`) | bodies, order, active, names/flags, placement, primitives' six parameter sets, Frozen Sculpt Mesh positions, `ImportedMesh` arrays, `CadBodyState` (sketches, extrusions, features, supports) | `ConstructionScene` and the representations; codec `forgeshape_project_document` |
| Session state (never serialized) | `SketchSession` draft and staged copy, candidate cache, view flip/turns, One Side preference, drag basis; `SupportChooser`; `BodyDimensionSession`; `GizmoSession`; `SculptSession`, mask, `SculptHistory`, Isolate; `ConstructionHistory`; `DisplaySettingsStore`; `EditorUiState`; 28 JNI globals | each session's own accessor; JNI for arbitration |
| Derived rendering data | loops, regions, hatch, overlay line list, prisms, kernel results, face tables, `RuntimeMesh`, render vertices, GPU buffers, the outline mask, preview meshes | `regenerateCadBody`, `SketchSession::overlay`, `MeshStore`, `Renderer` |
| Application state (SharedPreferences) | palette, handedness, gizmo scale/weight, Tool Labels | `AppPreferences` |
| Android presentation | every View; anchors converted by `ViewportAnchorSpace` from native projections | the child views; `EditorWorkspaceView` composes |

## 5. Evidence classification of this map

Every row above is `SOURCE_CONFIRMED` by the cited lines. `DEVICE_EVIDENCE`
exists for the HUD numbers, the region toggle and the same-body Add/Cut
(`artifacts/cad-vertical-slice-r1/OWNER_FINDINGS.md`). `TEST_CONFIRMED`
coverage per area is listed in each audit's last section. The two behaviours
this audit could not confirm at runtime are in `RUNTIME_VERIFICATION_GAPS.md`.
