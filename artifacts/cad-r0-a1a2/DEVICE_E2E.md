# Device E2E - `E2E-CADR0-01..16`

Suite: `SketchExtrudeTest` (9 JUnit methods, 16 cases). Run through
`scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.SketchExtrudeTest`
on `emulator-5580`, confirmed `ForgeShape_Stage006` by `emu avd name`;
`emulator-5554` never contacted. Result: **OK (9 tests)** - see
`FOCUSED_SKETCH_EXTRUDE.txt`. The sketch trace the run left in logcat is in
`DEVICE_SKETCH_TRACE.txt`.

Every viewport gesture is a real `MotionEvent` dispatched to the
`SurfaceView`, through `NativeViewport.touchEvent` and the sketch arbitration
in JNI, and every pixel it uses was asked for from `sketchScreenPoint` or
`gizmoHandlePoint` - the same projection native code unprojects with. No
control is located by coordinate.

| Case | What is driven | What is asserted | Method |
| --- | --- | --- | --- |
| E2E-CADR0-01 | Open the palette, tap New Sketch | the plane chooser appears inside the palette; no sketch has begun; all three planes offered | `e2eCadr0_01and02_...` |
| E2E-CADR0-02 | Tap XZ | the session is Editing on XZ; nothing created, nothing recorded; the toolbar reads *Sketch XZ*; Finish Sketch visible, Extrude and Start Sculpting gone; Cancel under the rail; Undo/Redo and creation withdrawn; the five sketch tools on the rail at the 48 dp floor with the held tool read back from native; no gizmo | same |
| E2E-CADR0-03 | Rectangle tool, a real drag (-1,-0.5)->(1,0.5) | one rectangle 2.0 x 1.0, grid-snapped exactly, selected | `e2eCadr0_03to08_...` |
| E2E-CADR0-04 | Finish Sketch (toolbar) | Ready; Extrude is the one transition; Back to Sketch offered; the precision surface opened on one profile and a depth field | same |
| E2E-CADR0-05 | Type depth `2.5`, tap the pinned Extrude | one new body; the session is over; the body is a CAD Body with depth 2.5 from a 2 x 1 rectangle profile; a mesh revision is published | same |
| E2E-CADR0-06 | Open Objects | a row for the new id; toolbar reads *CAD Body*; Start Sculpting absent; Undo/Redo and creation back | same |
| E2E-CADR0-07 | Undo (chrome) | the body is gone; no CAD state | same |
| E2E-CADR0-08 | Redo (chrome) | the same body is back; the `.forge` bytes equal those before Undo; creation was exactly one step | same |
| E2E-CADR0-09 | Shape tool on the CAD Body, type width 4 / height 3, Apply | the sizes apply as one step; the mesh revision moved; a zero width is refused and records nothing | `e2eCadr0_09and10_...` |
| E2E-CADR0-10 | Type depth 0.75, Apply; Undo x2, Redo x2 | the depth applies as one more step; Undo restores depth then sizes; Redo reapplies both | same |
| E2E-CADR0-11 | encodeProject, validateProject, a later edit, loadProject of the saved bytes | the fingerprint follows the edit and returns on load; bytes round-trip; the body reloads as a CAD Body with depth 1.5, still editable, with a fresh history | `e2eCadr0_11_...` |
| E2E-CADR0-12 | YZ plane, Circle tool, a real drag from the origin to (0.75, 0) | one circle r 0.75; extruded 1.25 as a circle-profile CAD Body on YZ; its radius editable later | `e2eCadr0_12_...` |
| E2E-CADR0-13 | Polyline tool, four real taps, a fifth on the first point | the polyline closes on its first point (4 points, closed); extruded 0.5 as a 4-vertex polygon profile | `e2eCadr0_13_...` |
| E2E-CADR0-14 | Polyline of three points ended open; Finish Sketch; Delete entity; Finish again; Cancel Sketch | Finish refused `OpenProfile` and stays Editing with Extrude absent; the empty sketch refused `NoClosedProfile`; Cancel ends the session with the project byte-for-byte unchanged, the fingerprint unchanged and nothing recorded | `e2eCadr0_14_...` |
| E2E-CADR0-15 | Transform tool on the CAD Body; a real drag along the X handle; then a depth edit | the gizmo is visible; the drag moved the placement; the depth edit leaves the placement exactly where the drag put it and is its own step | `e2eCadr0_15_...` |
| E2E-CADR0-16 | With a CAD Body in the scene: import an external GLB through the document path, freeze the imported body, a real Grab stroke, Undo, Redo | the stroke is one sculpt step; Undo takes it back and Redo restores it; the CAD Body is still there | `e2eCadr0_16_...` |

Process death across a save is covered by the existing
`scripts\run-project-persistence-e2e.ps1` machinery (instrumentation cannot
kill its own process); `E2E-CADR0-11` proves the document half - encode,
validate, reload, fingerprint, bytes - on the same bytes that machinery writes.

Two runs preceded the passing one and are recorded honestly: the first cast
the depth field to the wrong view type (`NumericPropertyRow` where the id is
on its `EditText`), a test defect; the second's E2E-CADR0-16 used a fixture the
stroke's pixel did not land on, and was rewritten to the same asset-based
import the Delete suite already proves. Neither was a product defect.
