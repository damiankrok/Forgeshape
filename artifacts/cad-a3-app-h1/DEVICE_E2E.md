# Device E2E - `E2E-APPH1` and `E2E-CADA3` (`CAD-A3-C1`)

Three suites on `emulator-5580` (confirmed `ForgeShape_Stage006` by
`emu avd name` on every runner invocation; `emulator-5554` never contacted),
real MotionEvents through the production `SurfaceView`, every viewport pixel
asked for from `debugProjectWorld` / `sketchScreenPoint` and never written
down, every control found by its semantic id. The raw focused logs are
`FOCUSED_HOME_SPATIAL.txt` (the final passing run) and the earlier rounds
listed under *Runs before the passing one*.

`HomeFlowTest` asserts on every journey, in `@After`, that
`debugActiveBodyMisuseCount()` did not move: native never read an active body
while no project was open.

## The brief's Part F cases

| # | Case | Suite / test | Result |
| --- | --- | --- | --- |
| 1 | cold launch → Home → no default primitive / editor | `HomeFlowTest.e2eAppH1_01` (Home visible, `projectOpen` false, 0 bodies, no selection, toolbar not shown, no bootstrap, no history, fingerprint 0, `encodeProject` null, exactly two clickable Home actions) | PASS |
| 2 | Home → New Project → CAD | `e2eAppH1_02` (CAD and Sculpt offered, three clickables incl. Cancel, choosing creates nothing, Cancel returns Home) and `e2eAppH1_03` (CAD lands directly in the spatial chooser) | PASS |
| 3 | New CAD → tap XY plane spatially → Rectangle → Finish → Extrude → first durable body / editor | `e2eAppH1_03`: `back_to_home` shown, project control and Objects capsule withdrawn, `projectOpen` false through the sketch; after Extrude: project open, exactly one CAD body, active, world-supported, undo = redo = 0, Construction mode, fingerprint non-zero, ordinary editor, project dirty | PASS |
| 4 | Home → New Project → Sculpt → valid Sculpt project | `e2eAppH1_04`: one seeded body, Sculpt mode, 482-vertex sphere, Construction history empty, Sculpt Undo empty; a real stroke → depth 1 → `historyUndo` → depth 0 | PASS |
| 5 | Home → Open File → open saved CAD dependency project | `e2eAppH1_05`: a producer + face-supported dependent built through the real chrome, saved to a file, Home, Open File through the recording picker host and `onOpenProjectDocumentChosen(file Uri)`; both bodies back, dependent still face-supported, producer delete still refused, not dirty, history empty | PASS |
| 6 | Open File cancel → remains Home | `e2eAppH1_06`: picker asked once, null Uri, Home stays, no project, Home's status line says so | PASS |
| 7 | corrupt Open → remains safe / non-mutating | `e2eAppH1_07`: a damaged file and a missing file each leave Home standing, create nothing, and write their verdict on Home | PASS |
| 8 | dirty Editor → New Project → Cancel preserves project | `e2eAppH1_08`: `project_new` over a dirty project asks; Cancel opens nothing, the project stays open and its bytes are identical | PASS |
| 9 | dirty Editor → New Project → Discard leaves and opens chooser | `e2eAppH1_09`: Discard closes the project (0 bodies), opens the chooser, retires the checkpoint; Cancel from there lands at Home | PASS |
| 10 | dirty Editor → Open File → Save persists old project before opening | `e2eAppH1_10`: Save writes the dirty bytes to the slot, clears dirty, then asks the picker with the project still live; a second dirty edit → Open File → Cancel leaves the project exactly where it was | PASS |
| 11 | New CAD spatial XY/XZ/YZ through real MotionEvents | `SpatialSketchTest.everyWorldPlaneIsSelectableSpatially`: for each plane a first tap aims (`supportChooserSelectedKind` = that plane, sketch inactive) and a second tap begins the sketch on it | PASS |
| 12 | existing CAD → New Sketch → tap planar cap physically | `faceSupportedSketchExtrudesADependentAndPersists`: New Sketch lands directly in the chooser; first tap on A's far cap aims at kind 3 (a CAD face); second tap begins a sketch on it | PASS |
| 13 | side-face support selection | `aSideFaceSupportsASketch`: a tap on the +X side of a 2×2×2 extrusion aims at a face; the extruded dependent is face-supported | PASS |
| 14 | circle cylindrical side refusal | `aCylindricalSideIsRefusedAsASupport`: a tap on the curved side of an extruded circle never resolves to a face and begins nothing; its planar cap does | PASS |
| 15 | dependent survives Save / reopen | `faceSupportedSketchExtrudesADependentAndPersists`: `encodeProject` → `loadProject`, both bodies back, the dependent still face-supported, the producer delete still refused | PASS |
| 16 | parent parameter edit keeps dependent attached | same test: the producer's depth edited 2 → 3 through `cadApplyExtrude`; the dependent remains face-supported and its new position projects on screen | PASS |
| 17 | producer Delete refusal with dependent | same test: `DELETE_REFUSED_HAS_DEPENDENTS`, both bodies remain; `HomeFlowTest.e2eAppH1_05` and the evidence suite repeat it after a reopen | PASS |
| 18 | adaptive grid changes with zoom; typed value bypasses snap | `adaptiveGridFollowsZoomAndTypedValuesBypassSnap`: two real two-finger pinches in opposite directions move `sketchGridStep` both ways to 1/2/5·10^k steps; a drag-placed rectangle then takes a typed 1.2345 × 0.777 exactly | PASS (final run) |
| hover | stylus hover highlights, never commits | `stylusHoverHighlightsATargetWithoutCommitting`: a synthesized stylus `ACTION_HOVER_MOVE` through the viewport's real generic-motion dispatch lights the XZ target (native answers its kind, the view reports the highlight), selects nothing and begins nothing; a hover off every target lights nothing; a real finger tap-tap then commits on that plane. **Hardware hover is not claimed**: the authoritative emulator has no stylus | PASS (synthesized hover) |
| back | deterministic Back in every shell phase | `e2eAppH1_11`: Home → not ours; chooser → Cancel; bootstrap sketch → the plane chooser; bootstrap chooser → Home; unsaved question → Cancel, never Discard | PASS |
| restore | Home, the chooser and the bootstrap survive a recreation | `e2eAppH1_12`: `recreate()` at Home lands at Home; `recreate()` inside the bootstrap chooser lands in the bootstrap with Back to Home shown | PASS |
| fallback | the by-name plane list remains | `byNamePlaneListRemainsTheAccessibilityFallback`: `sketch_plane_by_name` opens the three named planes beside the spatial row; choosing YZ begins a sketch on YZ | PASS |

## Suites

| Suite | Tests | Result |
| --- | --- | --- |
| `HomeFlowTest` | 12 | OK |
| `SpatialSketchTest` | 8 | OK (final run) |
| `CadA3VisualEvidenceTest` | 1 | OK — ten captures, `VISUAL_EVIDENCE.md` |
| `SketchExtrudeTest` (by-name path now via `sketch_plane_by_name`) | 9 | OK (final run) |
| `EditorWorkspaceProjectActionsTest`, `ImportedMeshDurableTest`, `EditorWorkspaceGizmoTest`, `EditorWorkspaceThemeTest`, `EditorWorkspaceChromeCompositionTest`, `EditorWorkspaceObjectsTest`, `ProjectAutosaveRecoveryTest` (the classes the Home change touched) | 7 + 12 + 46 + 19 + 12 + 10 + 14 | OK in the focused round-2 run |

## Runs before the passing one

- **Round 1** (21 tests, 3 failures, all in the new assertions): the
  dirty-project helper re-applied an identical box after a Save and so was
  honestly clean (fixed: a different size per edit); the dependent-creation
  step count ignored the producer's own creation step (fixed: relative);
  the pinch direction assumption (fixed: two pinches, order asserted).
- **Round 2** (ten classes): every class passed up to `SketchExtrudeTest`,
  where the emulator's `system_server` crashed (`INSTRUMENTATION_ABORTED:
  System has crashed`, `Can't find service: package`) — the device failure the
  status document already records, not the product; the guest recovered on
  its own and `SketchExtrudeTest` was rerun in round 4. One assertion
  failure: the grid's pre-sample 0.25 m fallback is not a 1/2/5 step (fixed:
  niceness asserted on the camera-sampled steps).
- **Round 3** (20 tests, 1 failure): the typed-value check read the
  rectangle's centre slot instead of its width (fixed: slots +2 / +3).
- **Round 4**: `SpatialSketchTest` + `SketchExtrudeTest`, then the evidence
  capture — see `FOCUSED_HOME_SPATIAL.txt` and `VISUAL_EVIDENCE.md`.
- **Aggregate run 1** (`FULL_SHARDED_RUN1_FAIL.txt`): discovery 37 classes /
  504 tests, missing = duplicates = unexpected = 0; shard 1 failed two
  pre-existing cases (`DiagnosticsAndRendererLossTest.e2er1b07`, a box Apply
  refused; `EditorWorkspaceChromeCompositionTest.uir4c01`, Start Sculpting
  absent). Cause: the shared baseline reset selected "the first body that is
  not imported" — after the evidence journey that is a **CAD** body, whose box
  Apply is refused with nothing asserting it, so the next classes inherited a
  CAD body as their baseline box. `WorkspaceTestSupport` now selects by
  representation and seeds a Construction body when the project has none; the
  three classes were rerun focused (OK, 22 tests) and the whole aggregate
  rerun from shard 1. A test-support defect, not the product.
- **Aggregate run 2** (`FULL_SHARDED_RUN2_FAIL.txt`): shard 1 passed 100/100;
  shard 2 failed one pre-existing case,
  `EditorWorkspacePointerTest.inr117_tapSelectionIsUnchanged`: the centre tap
  selected a larger sphere body another class had left at the origin, and the
  case's "no geometry changed" snapshot reads the ACTIVE body, so a legitimate
  selection change read as a geometry change. Order-dependent and exposed by
  the reshuffled shards (37 classes / 504 tests). The case now asserts the
  body count and re-selects the baseline body by id before comparing, which
  is the claim it always made; the whole aggregate was rerun from shard 1.
  A test defect, not the product.
- **Aggregate run 3** (`FULL_SHARDED_RUN3_FAIL.txt`): shard 1 passed 100/100;
  shard 2 failed `EditorWorkspaceGestureTest.ui11` once — "the inspector must
  sit clear of the keyboard: bottom 1496 vs usable 1440". The same case passed
  in run 2 on the identical tree: the case waited for the chrome inset to
  reach the keyboard and then measured the inspector sheet on that same frame,
  while the sheet was still animating to its place. Its wait now includes the
  sheet's own bounds (bounded, twelve settles) and the aggregate was rerun.
- **Aggregate run 4** (`FULL_SHARDED_RUN4_FAIL.txt`): the same case failed
  with the identical numbers after the extended wait — so not a race but a
  device state. The case shows the real keyboard and, when the guest refuses
  `SHOW_IMPLICIT`, falls back to a synthetic 40 % inset (2400 × 0.40 = 960,
  usable 1440); the guest had been refusing its IME since the
  `system_server` crash in focused round 2, and in that synthetic geometry the
  sheet cannot clear the inset. The remedy the status document already records
  for post-crash guest state — a real reboot of the isolated AVD
  (`adb -s emulator-5580 reboot`, `emu avd name` reconfirmed
  `ForgeShape_Stage006`, LatinIME listed) — was applied and the whole
  aggregate rerun from shard 1. Device infrastructure, not the product; the
  extended wait from run 3 is kept because it is correct either way.
- **Aggregate run 5** (`FULL_SHARDED_RUN5_FAIL.txt`): shards 1–3 passed
  (100 + 104 + 99, the IME case included). Shard 4 stopped on
  `EditorWorkspaceMobileTest.uir4a04`, which counts the palette's visible
  actions and expected seven; the by-name plane fallback (`UI-OWNER-46`'s
  accessibility route) is an eighth. The expected consequence of the product
  change; the case now names that control and expects eight. The whole
  aggregate was rerun from shard 1.
- **Aggregate run 6** (`FULL_SHARDED.txt`): **`FULL_SHARDED_SUITE_PASS`** —
  37 classes / 504 tests, shards 100 + 104 + 99 + 101 + 100 all PASS,
  missing = duplicates = unexpected = execution_missing = 0, on the final
  runtime/test tree.
