# BEFORE — source facts, verified before any product edit

Task `MODELING-R1-OWNER-CORRECTION-PROJECT-SHELL-SKETCH-SUPPORT-PLANAR-R1`.

## Baseline (verified with `git fetch` + `git rev-parse`)

| ref | expected | found |
| --- | --- | --- |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` | same |
| `origin/feature/cad-v6-sketch-face-r1` | `bbae765645a318f83451e9a61b6a45cf2cb17e33` | same |
| `origin/feature/cad-v8-sketch-drafting-toolkit-r1` | `a3643b5330f28f3590ba75f1a89c39e2424b3eed` | same |
| `origin/feature/modeling-v9-parametric-freeform-surface-r1` | `7e0224056c18bcff2446487e526cd5a703b1d36b` | same |

Task branch `feature/modeling-r1-owner-shell-sketch-fix-r1` created at exactly
`7e0224056c18bcff2446487e526cd5a703b1d36b`. No existing branch was moved.

## 3.1 New Sketch still exists — TRUE

`AddPrimitivePaletteView` builds a `New Sketch` tile (`R.id.add_sketch`,
`ic_sketch_new`) whose click calls `listener.onNewSketchSpatial()`, i.e.
DIRECTLY the spatial support chooser (`UI-OWNER-46`); the by-name plane list
stays behind `sketch_plane_by_name`. The palette is reached ONLY from the
Objects capsule's plus (`ObjectsSectionView`, `R.id.add_body`) — there is no
New Sketch control in the Global Toolbar or anywhere the OWNER looked for one,
which is what "zniknęła możliwość zrobienia nowego szkicu" reports: the act
exists, its entry is buried in a creation palette whose name says "Add
Primitive".

## 3.2 Face support still exists — TRUE

`SupportChooser::begin(bool allowFaces)` (`forgeshape_support_chooser.h:58`)
documents `allowFaces` as true for New Sketch inside an existing project, and
`EditorWorkspaceView.onNewSketchSpatial()` calls
`NativeViewport.supportChooserBegin(true)`. `SupportChooser::resolve` returns
a world plane (the nearest of the three ±H squares the ray hits) or, through
`pickSceneSnapshot` and `cadFaceRangesFromMesh` over the body's PUBLISHED
regeneration, a CAD face `TopoRef{producer, featureId, CadFaceToken,
lineage}` — never a triangle index — but only when the range is `eligible`.

## 3.3 Current project affordance is wrong for the OWNER contract — TRUE

`GlobalToolbarView` builds `projectActionsButton`
(`R.id.project_actions_button`, `R.drawable.ic_project`) inside the TRAILING
utility group (top-right, after Export, before Display and Hide UI), and
`showBootstrap(true)` sets it `GONE`. Nothing stands in the top-left corner but
the editing group (context label + one transition).

## 3.4 Project actions already exist — TRUE

`ProjectActionsPopoverView.OnProjectAction` owns `onNewProjectRequested`,
`onSaveProjectRequested`, `onOpenProjectRequested`, `onSaveCopyRequested`,
`onOpenFileRequested`, `onShareDiagnosticsRequested`, `onImportGlbRequested`
and `onSettingsRequested`, implemented once by `EditorWorkspaceView`. The
drawer reuses exactly these handlers.

## 3.5 The ForgeShape mark already exists — TRUE

`StartPageView.buildMasthead` draws `R.drawable.ic_forgeshape_mark` at
`R.dimen.start_page_mark_size` beside the product name. The drawer opener
reuses that drawable.

## 3.6 Cut face-support debt — TRUE

`forgeshape_cad_feature.cpp`: `deriveFeature` (line 285) and
`derivePlanarFeature` (line 229) both compute
`const bool featureEligible = operation != CadFeatureOperation::Cut;` and pass
it to every cap and side, so EVERY face a Cut produces is ineligible: the
support chooser `break`s on `!rg.eligible`, `sketchPlacement` refuses
`FeatureSupportInvalid`, `validateCadFaceSupport` refuses. Two further facts
the fix must respect:

- A Cut face's FRAME is the TOOL prism's outward frame (pocket bottom normal
  pointing deeper into the material, walls pointing into the material). It was
  never used for placement while ineligible, so it was never wrong in effect;
  it would be wrong the moment one is eligible.
- Eligibility is MIXED INTO the lineage signature (§7c, `featureSignature`),
  and `scripts/build-forge-corpus.ps1` (`Get-CadFeatureTopologySignature
  -Cut`) and `DATA_PACKAGE_SPEC.md` §7g ("every face of a Cut is ineligible")
  state the Cut rule as part of the FORMAT. Changing the bit would change every
  Cut feature's lineage token.
- Partial Revolve caps: tokens are stable (`CapPlane`/`CapFar`), but the frame
  is a placeholder (`cap.frame = g->placement`, the sketch frame, "only has to
  be finite") — NOT the exact planar frame of the cap.

## 3.7 Point-contact selection rule — TRUE, and the OWNER failure is a real pinch

`mergePlanarFaces` partitions the chosen faces by SHARED FRAGMENT
(`partitionSelectedPlanarFacesBySharedBoundary`) and merges each group alone,
so two groups meeting only at a point are separate components. But inside ONE
edge-connected group `mergeEdgeConnectedFaces` keeps a `nodeUsed` set across
all of the group's union loops and returns `PinchedSelection`
(→ `PlanarFacesTouchAtPoint`, "Those areas meet only at a point…") the first
time a loop revisits a node. That is reproduced deterministically in
`REPRO_BEFORE.md`: selecting every cell of an OWNER-like sketch EXCEPT two
inner cells that touch at one node is one edge-connected group whose union
boundary passes that node twice — 45 of the 253 "all but two" selections of the
23-cell sketch are refused this way, with the selection intact (21 chosen).

## UI facts that bound the fix

- The editing group's leading child today is the context label; the
  transition fits by arithmetic on the row (`fitTransitionToRow`), utility
  group never squeezed. A fixed-width mark must be added to that arithmetic.
- The bootstrap (`showBootstrap(true)`) withdraws the project control because
  there is no project; the drawer must keep Back to Home reachable there.
