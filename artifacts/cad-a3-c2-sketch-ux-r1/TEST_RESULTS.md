# Focused test matrix `CADUXR1-01..40`

Two halves. What can be settled without a device — the curve mathematics, the
exact-length semantics, the view frames, the staged edit session, the `CADB` v3
round trip — is the native suite `forgeshape_sketch_ux_selftest.cpp`, which runs
on every debug launch as `FORGESHAPE_SKETCH_UX_SELFTEST_OK` (50 checks). What
only a real window can say is the instrumented `SketchUxTest` (14 cases).

| ID | Requirement | Evidence | Result |
| --- | --- | --- | --- |
| CADUXR1-01 | Home is a full-screen page, not a modal overlay | `SketchUxTest.cadUxR1_01_02_03` measures Home's bounds against the window's: equal width, equal height, flush to both edges | PASS |
| CADUXR1-02 | editor chrome absent/inactive on Home | same case: `global_toolbar`, `objects_capsule` and the sketch navigator all not shown | PASS |
| CADUXR1-03 | Home logo/title and New/Open visible | same case: `start_page_wordmark` shown and above `home_new_project`; headline shown; both actions shown | PASS |
| CADUXR1-04 | New Project CAD/Sculpt navigation | `cadUxR1_04`: the page is full-window, Home is **not** visible under it, CAD/Sculpt/Back all shown, Back returns to Home | PASS |
| CADUXR1-05 | New CAD enters Sketch immediately | `cadUxR1_05_06_07`: `SKETCH_EDITING` straight after `new_project_cad`, with the support chooser inactive | PASS |
| CADUXR1-06 | default support XY/+Z | same case: plane XY, flip 0, quarter turns 0; native `CADUXR1_06_08` asserts the frame vectors exactly | PASS |
| CADUXR1-07 | no floating-plane chooser in the normal first-sketch path | same case, plus `HomeFlowTest.e2eAppH1_03` which drives the whole journey with no plane tap | PASS |
| CADUXR1-08 | navigator selects XY +Z/-Z | `cadUxR1_08_09_10` presses `sketch_navigator_flip` both ways; native `CADUXR1_08` asserts the flipped frame is right-handed and unmirrored | PASS |
| CADUXR1-09 | navigator selects XZ +Y/-Y | same pair; native `CADUXR1_09_14` asserts the re-based authoring frame equals `workplaneFrame(XZ)` | PASS |
| CADUXR1-10 | navigator selects YZ +X/-X | same pair; native `CADUXR1_10` for YZ | PASS |
| CADUXR1-11 | +90 degree screen rotation exact | native `CADUXR1_11_12`: u becomes (0,-1,0) and v becomes (1,0,0), right-handed; device `cadUxR1_11_12_13` asserts the quarter-turn count | PASS |
| CADUXR1-12 | -90 degree screen rotation exact | same case: two -90 from +90 wraps to 3, and a fourth returns to 0 | PASS |
| CADUXR1-13 | screen rotation does not mutate authored coordinates | native `CADUXR1_13` compares the whole `CadSketch` with `sameCadSketch` after four turns; device compares the selected line's four values bit for bit | PASS |
| CADUXR1-14 | support switching allowed while sketch empty | `cadUxR1_14_15`: chips enabled, the switchable flag set, the switch lands | PASS |
| CADUXR1-15 | support switching after geometry cannot silently remap | same case: chips disabled, and `sketchSetSupportPlane` called directly returns `CAD_SKETCH_NOT_EMPTY` with the plane and the entity unmoved | PASS |
| CADUXR1-16 | face-supported sketch remains TopoRef-attached | native `CADUXR1_16`: a face sketch refuses a world plane (`InvalidWorkplane`), keeps its producer id, and still rotates; device `SpatialSketchTest` (8) drives the real face path | PASS |
| CADUXR1-17 | Line selected shows dimension overlay | `cadUxR1_17..22`: `sketchLineDimension` true, the label shown; native `CADUXR1_17` also asserts the label anchor stands exactly the annotation offset clear of the stroke | PASS |
| CADUXR1-18 | displayed line length numerically correct | same case: a 3-4-5 line reports exactly 5.0 | PASS |
| CADUXR1-19 | tap dimension opens numeric edit | same case: the editor opens, the field is shown, and the editor covers less than a quarter of the viewport | PASS |
| CADUXR1-20 | typed length keeps P0 fixed | same case: after typing 10, P0 is still exactly (0,0); native `CADUXR1_20_21_22` to 1e-12 | PASS |
| CADUXR1-21 | typed length preserves direction | same pair: the direction ratio is unchanged and both components keep their sign | PASS |
| CADUXR1-22 | typed length moves P1 to exact distance | same pair: (6,8), exactly 10 from P0 | PASS |
| CADUXR1-23 | invalid line length non-mutating | `cadUxR1_23` drives `0`, `-4`, `banana` and empty through the real field: no coordinate moves, the editor stays open for correction, and a valid value then closes it. Native `CADUXR1_23` adds NaN, infinity and out-of-range | PASS |
| CADUXR1-24 | line edit may invalidate profile and Extrude refuses | `cadUxR1_24` builds a triangle, shortens one line, presses Finish Sketch: stays editing with `CAD_OPEN_PROFILE`, no project created | PASS |
| CADUXR1-25 | Arc create/select/delete | `cadUxR1_25_27` draws one through the real two-step gesture and asserts the entity kind; native `CADUXR1_25` covers the geometry, the middle-point decision, and the collinear/degenerate refusals including a far-from-origin triple | PASS |
| CADUXR1-26 | Arc persistence deterministic | native `CADUXR1_26`: encode, decode, `sameProjectDocument`, and re-encoding the decoded document reproduces the same bytes | PASS |
| CADUXR1-27 | Arc closed-profile participation | native `CADUXR1_27`: an arc and a line close exactly one profile of area pi/2 and generate a mesh; a lone arc is `OpenProfile`; the line's side face is eligible and every arc facet is not | PASS |
| CADUXR1-28 | Spline create/select/delete/control edit | `cadUxR1_28_30` draws, selects, deletes and redraws one; native `CADUXR1_28` covers the refusals and the point cap, and `CADUXR1_29` proves the curve interpolates its authored points, which is what makes a control edit meaningful | PASS |
| CADUXR1-29 | Spline persistence deterministic | native `CADUXR1_29` round trip, same terms as the arc | PASS |
| CADUXR1-30 | Spline closed-profile participation | native `CADUXR1_30` and the device case's extrude | PASS |
| CADUXR1-31 | curve tessellation deterministic/bounded | native `CADUXR1_31`: two runs bit-identical, every arc point exactly on the circle, segment counts inside the min/max bounds, a denser sweep gives more segments, and a chain past `kMaxProfileVertices` is refused rather than grown | PASS |
| CADUXR1-32 | Edit Sketch opens authored committed sketch | `cadUxR1_32..35`: `sketchEditingBodyId` is the body, the sketch carries its entity, the navigator is up | PASS |
| CADUXR1-33 | Edit Sketch Cancel exact non-mutation | same case: the staged change never reaches the body, and Cancel leaves both the body and the history depth untouched. Native `CADUXR1_33` likewise | PASS |
| CADUXR1-34 | Edit Sketch Finish one ProjectHistory step | same case: the body takes the edit, undo depth +1 exactly, body count still 1 | PASS |
| CADUXR1-35 | Undo/Redo Edit Sketch exact | same case: Undo restores the previous width, Redo the edited one | PASS |
| CADUXR1-36 | dependent TopoRef regeneration survives sketch edit | native `CADUXR1_36`: a supported (size) edit keeps the dependent resolving; an edit that would strip the referenced face is refused `DependentFaceLost` with the producer unchanged | PASS |
| CADUXR1-37 | CADB v1/v2 compatibility unchanged | native `CADUXR1_37`: a curveless CAD project still writes v1; a curve code inside a section declaring v2 is refused. All 22 older corpus digests unchanged — see `CORPUS.md` | PASS |
| CADUXR1-38 | independent v3 corpus parity | native `CADUXR1_38`: six digests asserted against the PowerShell encoder's bytes, and both corrupt fixtures refused `InvalidSemanticValue` by the ordinary decoder | PASS |
| CADUXR1-39 | Sculpt/Imported regressions unchanged | `cadUxR1_39` drives New Project to Sculpt (project created, Sculpt mode, seeding is not a user act); the full-sharded aggregate covers the rest | PASS |
| CADUXR1-40 | no solver/booleans/custom planes introduced | `cadUxR1_40` asserts the sketch rail is exactly the seven named tools. No constraint, boolean, fillet, chamfer or custom-plane code, control or string exists — see the scope statement in `INDEX.md` | PASS |

## The native suite

`FORGESHAPE_SKETCH_UX_SELFTEST_OK (50 checks)` on every debug launch, and 0
failures in all twenty suites (2970 checks) — see `INDEX.md`.

## Four defects found while verifying

Two in the product, one in an older test's premise, one latent in the shared
test infrastructure.

1. **Home stayed visible under the New Project page.** As two opaque full-window
   pages they were stacked, with two focusable action sets in the view tree at
   once. Found by `CADUXR1-04`, fixed in `refreshShellPhase`.
2. **The navigator covered the Tool Rail's first entries.** Found by eye on the
   first device run and now asserted by measured bounds in `CADUXR1-08`; the
   navigator is inset by the right contextual host's own width.
3. **`EditorWorkspaceThemeTest` asserted Home has a raised panel** — the exact
   composition the owner rejected. Its `R1B2-13` case was rewritten to ask what
   a PAGE must satisfy (an opaque ground, readable contrast on it, full-window
   bounds) while keeping the scrim-role assertions, which are still real: the
   scrim is what the unsaved-changes and recovery questions stand on.
4. **A latent order dependency in `resetToBaselineConstruction`**, exposed for
   the first time by this stage's two new test classes changing the shard split.
   `CadA3VisualEvidenceTest` ends with a CAD-ONLY project, and the shared reset
   only APPENDED a Construction body when none existed — which lands at the end,
   leaving a CAD Body as body 0. `ImportedMeshDurableTest.imp01a14` then
   selected body 0 expecting a Construction Body and correctly found no sculpt
   transition on it.

   The reset's own comment already named this failure mode ("a reset that
   silently did nothing would hand the next case a baseline it never
   established"), so the fix was made there rather than in the test that
   tripped over it: a project with no Construction Body at all **is not a
   baseline**, so the reset now closes it and lets a fresh one be created.
   Closing writes nothing and is exactly what Back to Home does. Verified by
   running the two classes back to back, which reproduced the failure before
   the fix and passes after it.

## Aggregate

See `FULL_SHARDED.txt`.
