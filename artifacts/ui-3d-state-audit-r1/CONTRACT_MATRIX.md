# UI-3D-STATE-AUDIT-R1 — Expected State Matrix

The machine-readable twin of this file is `CONTRACT_MATRIX.tsv`, and it is what
`Ui3dStateAuditTest` writes its measured column against.

## Where an expectation comes from

Every `MUST_SHOW` / `MUST_HIDE` below is traced to a binding contract, not to
the shipped code's current `View.VISIBLE`. Four sources, in this order:

| source | what it settles |
| --- | --- |
| **C1** `CLAUDE.md`: *"A control that cannot succeed is not drawn"* | any surface whose act the domain refuses in this state is `MUST_HIDE` |
| **C2** `CLAUDE.md`: *"No surface owns the resting workspace"* and the UI-LAYOUT-R1 one-primary-surface rule | at most one of Objects / Add Primitive / precision / Display, and none at rest |
| **C3** `UI-OWNER-50` (this audit's own binding contract) | a contextual surface must disappear when its owning context is invalid, dismissed, deselected, deleted, switched away from, cancelled or replaced; it must never survive as ghost UI |
| **C4** the stage contract that created the surface (`CAD-UX-S1`, `CAD-EXT-R1`, `SKETCH-UX-R1`, Stage 020M, `APP-H1`, `SCULPT-FCM-R1`, `SCULPT-H1`, Stage 018A) | the surface's own stated owning context |

`MAY_SHOW` is used **only** where the binding contracts genuinely do not settle
the question, and each one carries the reason. It is never used to excuse a
result.

## The matrix

`S` numbers are the inventory ids in `DYNAMIC_SURFACE_INVENTORY.md`.

### A — no project / Home (`APP-H1`)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `home_no_project` | S01 Home page | MUST_SHOW | C4 `APP-H1`: `hasProject()` is the one answer |
| `home_no_project` | S06 Global Toolbar | MUST_HIDE | C4 — behind Home nothing is drawn or edited |
| `home_no_project` | S07 Tool Rail | MUST_HIDE | C4 |
| `home_no_project` | S08 Objects capsule | MUST_HIDE | C4 |
| `home_no_project` | S13 precision surface | MUST_HIDE | C2 |
| `home_no_project` | S19 dimension labels | MUST_HIDE | C3 — no body owns them |
| `home_no_project` | S35 extrude cluster | MUST_HIDE | C3 |
| `home_no_project` | S38 Edit Sketch chip | MUST_HIDE | C3 |
| `new_project_chooser` | S02 New Project chooser | MUST_SHOW | C4 |
| `new_project_chooser` | S01 Home page | MAY_SHOW | New Project REPLACES Home; whether the page underneath is torn down or merely covered is an implementation choice the contract does not settle |

### B — the first CAD sketch (`APP-H1` bootstrap)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `new_cad_sketch` | S28 sketch editor | MUST_SHOW | C4 |
| `new_cad_sketch` | S30 orientation navigator | MUST_SHOW | C4 `SKETCH-UX-R1` C |
| `new_cad_sketch` | S02 New Project chooser | MUST_HIDE | C3 — replaced |
| `new_cad_sketch` | S01 Home page | MUST_HIDE | C3 |
| `new_cad_sketch` | S12 Add Primitive | MUST_HIDE | C1 — creation is refused below JNI while a sketch is open |
| `new_cad_sketch` | S16 row command strip | MUST_HIDE | C1 — all five acts are refused while a sketch is open |
| `new_cad_sketch` | S35 extrude cluster | MUST_HIDE | C4 — the manipulator belongs to `Ready`, not `Editing` |
| `new_cad_sketch` | S19 dimension labels | MUST_HIDE | C3 |

### C — the staged extrusion (`CAD-UX-S1`, `CAD-EXT-R1`)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `sketch_ready` | S35 extrude cluster | MUST_SHOW | C4 |
| `sketch_ready` | S35 extent selector (One Side) | MUST_SHOW | C4 `CAD-EXT-R1` |
| `sketch_ready` | S35 Flip | MUST_SHOW | C4 — Flip is a One Side control |
| `sketch_ready` | S36 Side B value | MUST_HIDE | C4 — the second value exists in Two Sides alone |
| `sketch_ready` | S38 Edit Sketch chip | MUST_HIDE | C4 — the chip belongs to a committed body with no session open |
| `sketch_ready` | S29 profile chooser | MAY_SHOW | the contract does not require a chooser for a single-profile sketch |
| `extent_symmetric` | S35 Flip | MUST_HIDE | C1 + C4 — refused below JNI and ABSENT above it |
| `extent_symmetric` | S36 Side B value | MUST_HIDE | C4 — Symmetric is one distance and one number |
| `extent_two_sides` | S36 Side B value | MUST_SHOW | C4 |
| `extent_two_sides` | S35 Flip | MUST_HIDE | C1 + C4 |
| `extent_back_to_one_side` | S36 Side B value | MUST_HIDE | C3 — no stale second value may survive a mode transition |
| `extent_back_to_one_side` | S35 Flip | MUST_SHOW | C4 |
| `side_b_zeroed` | S36 Side B value | MAY_SHOW | Two Sides permits a zero side because the other carries the extent; whether a zero is drawn or withdrawn is not settled |

### D — the committed CAD body and the retained sketch

| state | surface | expected | source |
| --- | --- | --- | --- |
| `extrude_committed` | S35 extrude cluster | MUST_HIDE | C3 — the session ended |
| `extrude_committed` | S36 Side B value | MUST_HIDE | C3 |
| `extrude_committed` | S28 sketch editor | MUST_HIDE | C3 |
| `extrude_committed` | S30 orientation navigator | MUST_HIDE | C3 |
| `extrude_committed` | S31 line dimension | MUST_HIDE | C3 |
| `extrude_committed` | S38 Edit Sketch chip | MUST_SHOW | C4 `CAD-UX-S1` — one tap from the committed body |
| `edit_sketch` | S28 sketch editor | MUST_SHOW | C4 |
| `edit_sketch` | S38 Edit Sketch chip | MUST_HIDE | C3 — a session is open, so the chip's context is gone |
| `edit_sketch` | S39 CAD feature editor | MUST_HIDE | C3 — committed-body-only context |
| `edit_sketch` | S24 gizmo | MUST_HIDE | C1 — a body is not transformable while its sketch is open |
| `finish_again` | S35 extrude cluster | MUST_SHOW | C4 — the feature preview for the SAME body |
| `edit_sketch_cancelled` | S28 sketch editor | MUST_HIDE | C3 |
| `edit_sketch_cancelled` | S30 orientation navigator | MUST_HIDE | C3 |
| `edit_sketch_cancelled` | S31 line dimension | MUST_HIDE | C3 |
| `edit_sketch_cancelled` | S35 extrude cluster | MUST_HIDE | C3 |
| `edit_sketch_cancelled` | S38 Edit Sketch chip | MUST_SHOW | C4 — the body is committed again |

### E — the sketch's selected-Line dimension (`SKETCH-UX-R1` E)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `line_drawn` | S31 line dimension | MAY_SHOW | whether drawing a line also selects it is not settled by the contract |
| `line_selected` | S31 line dimension | MUST_SHOW | C4 — a selected straight Line carries a dimension |
| `length_applied` | S31 line dimension | MUST_SHOW | C3 — the selection did not change |
| `tool_changed` | S31 line dimension | MAY_SHOW | the contract says the annotation belongs to the selection, and does not settle whether changing tool clears the selection |
| `sketch_cancelled` | S31 line dimension | MUST_HIDE | C3 |

### F — Construction Body Dimensions (Stage 020M)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `dimensions_open` | S19 dimension labels | MUST_SHOW | C4 |
| `dimensions_open` | S22 anchor chips | MUST_SHOW | C4 |
| `dimensions_open` | S24 gizmo | MUST_HIDE | C4 — entering Dimensions withdraws the gizmo below JNI as well as above it |
| `dimensions_open` | S13 precision surface | MUST_HIDE | C2 |
| `after_move` / `after_rotate` / `after_scale` / `after_typed_dimension` / `after_undo` | S19 dimension labels | MUST_SHOW **and attached** | C3 + `UI-OWNER-50` — the label must remain on the transformed body's own dimension line |
| `after_orbit` / `after_pan` / `after_zoom_out` / `after_zoom_in` | S19 dimension labels | MUST_SHOW **and attached** | `UI-OWNER-50` — correctly attached while the camera orbits, pans or zooms |
| `dimensions_closed` | S19 dimension labels | MUST_HIDE | C3 |
| `dimensions_closed` | S22 anchor chips | MUST_HIDE | C3 |
| `dimensions_closed` | S24 gizmo | MUST_SHOW | C4 — the gizmo returns with the mode |
| `relative_scale_open` | S19 dimension labels | MUST_HIDE | C4 — Relative Scale closes Dimensions |
| `relative_scale_open` | S22 anchor chips | MUST_HIDE | C4 |
| `relative_scale_open` | S23 Relative Scale editor | MUST_SHOW | C4 |

### G — selection, visibility, lock, delete (Stage 018A, `UI-OWNER-45`)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `dimensions_on_body_B` | S19 dimension labels | MUST_SHOW (if the mode opened) | C4 |
| `switched_to_body_A` | S19 dimension labels | MAY_SHOW | the contract does not settle whether Dimensions survives a body switch; what it DOES settle is that no label of B may stand on A, which the spatial ledger measures |
| `active_body_hidden` | S19 dimension labels | MUST_HIDE | C1 + C4 — Dimensions refuses a hidden body, and the leaders are read off geometry that is not drawn |
| `active_body_locked` | S24 gizmo | MUST_HIDE | C4 Stage 018A — the gizmo is not activated over a locked body |
| `active_body_locked` | S19 dimension labels | MUST_HIDE | C4 — Dimensions refuses a locked body |
| `after_delete` | S19 dimension labels | MUST_HIDE | C3 — the owning object is gone |
| `after_undo_of_delete` | S08 Objects capsule | MUST_SHOW | C4 — the body is back |
| `after_redo_of_delete` | S19 dimension labels | MUST_HIDE | C3 |

### H — Sculpt (`SCULPT-FCM-R1`, `SCULPT-H1`)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `sculpt_entered` | S19 dimension labels | MUST_HIDE | C1 — Dimensions refuses in Sculpt (`SCULPT-DIM-01` is unbuilt) |
| `sculpt_entered` | S22 anchor chips | MUST_HIDE | C1 |
| `sculpt_entered` | S24 gizmo | MUST_HIDE | C4 |
| `sculpt_entered` | S25 transform selectors | MUST_HIDE | C4 — contextual to Transform, absent everywhere else |
| `sculpt_entered` | S12 Add Primitive | MUST_HIDE | C1 — creation is refused in Sculpt |
| `sculpt_entered` | S35 extrude cluster | MUST_HIDE | C3 |
| `sculpt_entered` | S38 Edit Sketch chip | MUST_HIDE | C3 |
| `sculpt_entered` | S40 brush controls | MUST_SHOW | C4 |
| `sculpt_entered` | S07 the seven brush rail entries | MUST_SHOW | C4 `SCULPT-FCM-R1` |
| `after_stroke` | S09 History navigator control | MUST_SHOW | C4 `SCULPT-H1` |
| `navigator_open` | S42 History navigator | MUST_SHOW | C4 |
| `navigator_closed` | S42 History navigator | MUST_HIDE | C3 |
| `after_mask_paint` | S41 Clear Mask | MUST_SHOW iff native says it is available | C1 — a control that cannot succeed is not drawn |
| `after_clear_mask` | S41 Clear Mask | MUST_HIDE iff native says it is unavailable | C1 |
| `back_to_construction` | S40 brush controls | MUST_HIDE | C4 |
| `back_to_construction` | S42 History navigator | MUST_HIDE | C3 |
| `back_to_construction` | S41 Clear Mask | MUST_HIDE | C3 |
| `back_to_construction` | S19 dimension labels | MUST_HIDE | C3 — nothing may be resurrected by leaving Sculpt |
| `resume_sculpt` | S40 brush controls | MUST_SHOW | C4 |
| `resume_sculpt` | S19 dimension labels | MUST_HIDE | C3 |
| `resume_sculpt` | S24 gizmo | MUST_HIDE | C4 |

### I — global dismissal and primary-surface exclusivity (UI-LAYOUT-R1)

| state | surface | expected | source |
| --- | --- | --- | --- |
| `objects_open` | S11 Objects popover | MAY_SHOW | a window wide enough gives Objects a permanent column, in which there is no popover to open |
| `precision_open` | S13 precision surface | MUST_SHOW | C2 |
| `precision_open` | S11 Objects popover | MUST_HIDE | C2 |
| `display_open` | S14 Display popover | MUST_SHOW | C2 |
| `display_open` | S13 precision surface | MUST_HIDE | C2 |
| `after_system_back` | S14 Display popover | MUST_HIDE | C4 — Back closes the topmost surface before it leaves |
| `shape_tool` | S25 transform selectors | MUST_HIDE | C4 |
| `shape_tool` | S22 Dimensions entry | MUST_HIDE | C4 — Dimensions lives under Transform |
| `shape_tool` | S24 gizmo | MUST_HIDE | C4 |

## Spatial attachment expectations

Every surface classed `WORLD_ANCHORED` or `FEATURE_ANCHORED` in the inventory
is held to one rule, from `UI-OWNER-50`:

> the surface's intended attachment point must stand within **4 dp** of the
> anchor native reports for it **at that instant**.

The intended attachment point is the **centre** of the placed box, because
`placeAt` in all three placing views (`BodyDimensionLabelsView`,
`SketchDimensionLabelView`, `CadExtrudeCanvasView`) subtracts half the measured
box from the anchor. There is no authored offset policy for any of them, so the
tolerance is measured against zero distance.

The one legitimate non-zero case is the deliberate **edge clamp** all three
apply so a value cannot stand half outside the window. A measurement taken while
the clamp bites is recorded with the verdict `CLAMPED` and excluded from the
error statistics, and the audit says so rather than counting it either way.

| surface | anchor read from | measured against |
| --- | --- | --- |
| S19 dimension labels X/Y/Z | `bodyDimensionLabelPoint(axis)` | the label chip's centre |
| S31 sketch line dimension | `sketchLineDimension` then `sketchScreenPoint` | `sketch_dimension_value`'s centre |
| S35 extrude cluster | `cadExtrudeToolState[CAD_EXTRUDE_LABEL_X/Y]` | the placed cluster's scaled centre |
| S36 Side B value | `cadExtrudeToolState[CAD_EXTRUDE_SECOND_LABEL_X/Y]` | the placed second cluster's scaled centre |
| S38 Edit Sketch chip | `cadBodySketchAnchor(bodyId)` | the chip's scaled centre |

`OVERLAY_GEOMETRY` surfaces (S21, S24, S26, S32, S34, S43) are drawn by the
renderer from the domain every frame, so they cannot hold a stale screen
coordinate by construction; they are audited for **visibility** and for the
renderer-style question in §4.5 instead of for attachment error.
