# ForgeShape Master Plan v4.1.58

Date: 2026-10-06

## OWNER physical review result

The first combined physical review of `MODELING-FOUNDATIONS-R1` found three blocking regressions in the ordinary CAD workflow.

Classification:

`OWNER PHYSICAL REVIEW FAILED / CORRECTION REQUIRED / NOT MERGED`

The broader Parametric / Freeform / Surface work is not rejected. OWNER explicitly says the rest looks OK so far. The blockers are in project-shell discoverability, New Sketch support selection, and PlanarFace/NewBody behavior.

## OWNER feedback

Verbatim:

> zniknęła możliwość zrobienia nowego szkicu i wtedy wyboru powierzchni w momennie jak już mam jakiś obiekt 3d i chce wybrac powierzchnie gdzie chce szkicować oraz jest błąd jak chce zaznaczy te kształty wewnątrz zobacz na screenach. w lewym górnym rogu brakuje logo któe po wciśnięciu miało sie transformować w wusywany panel z opcjami i opcjami zapisu i wczytania , boteraz nawet nie ma przycisku utwoórz nowy podzas prac. reszta wygląda ok

Screenshot observations:
- one screen shows 16 selected extrusion regions and a complex multi-cell preview;
- another shows the error:
  `Those areas meet only at a point — add the area between them, or extrude them separately.`
- top-left chrome shows transition/Back-to-Home controls, but no ForgeShape mark/project drawer opener.

## Source audit — confirmed facts

### 1. New Sketch core path still exists

Current source still contains:
- `AddPrimitivePaletteView` → `New Sketch`;
- `New Sketch` enters the spatial support chooser directly;
- `SupportChooser::begin(bool allowFaces)` explicitly supports body-face picking for New Sketch inside an existing project;
- world planes and stable CAD face supports remain native concepts.

Therefore the missing workflow is primarily a product-shell/entry-point regression, not a missing native plane chooser.

### 2. Project actions still exist but are behind the wrong affordance

Current source:
- uses a generic top-right `ic_project` button;
- `ProjectActionsPopoverView` already owns:
  - New Project
  - Save Project
  - Open Saved Project
  - Save Copy
  - Open File
  - Settings/other project actions;
- `GlobalToolbarView.showBootstrap()` hides the project-actions button during bootstrap.

The OWNER-requested ForgeShape mark in the top-left is not the current editor affordance.

### 3. Existing product mark can be reused

`StartPageView` already uses:
`R.drawable.ic_forgeshape_mark`

No new brand asset is required.

### 4. Cut-created faces are still blanket-ineligible as sketch support

Current CAD feature derivation uses:
`featureEligible = operation != CadFeatureOperation::Cut`

for Extrude feature faces.

Thus all Cut-created faces are excluded from sketch support even when the resulting face is planar and semantically addressable.

This is the known CAD debt that now directly conflicts with OWNER's expectation:
after a 3D feature exists, New Sketch must allow choosing an eligible planar model face.

### 5. Point-only PlanarFace groups are already meant to work

Current `mergePlanarFaces()` already:
- partitions selected faces by shared boundary;
- treats pure point-only contacts as separate components.

Therefore the screenshot's `PlanarFacesTouchAtPoint` is NOT explained by the old simple point-contact limitation alone.

The exact cause is still unproven and must be reproduced. Likely categories:
- a true articulation/self-pinch inside one edge-connected selected group;
- incorrect fragment/group attribution on the OWNER-like sketch;
- stale/misattributed candidate status.

Do not change the message only.

## Binding correction scope

Task:

`MODELING-R1-OWNER-CORRECTION-PROJECT-SHELL-SKETCH-SUPPORT-PLANAR-R1`

Start from:
`feature/modeling-v9-parametric-freeform-surface-r1`
HEAD:
`7e0224056c18bcff2446487e526cd5a703b1d36b`

Create:
`feature/modeling-r1-owner-shell-sketch-fix-r1`

No integration first.

## Product decisions

### A. ForgeShape project drawer

Restore a persistent ForgeShape mark at the TOP-LEFT of the editor.

Tap:
- mark expands/morphs into a left-edge project drawer/surface;
- reuse existing project-action handlers;
- do not duplicate save/load semantics.

Open-project drawer must provide at least:
- New Project
- New Sketch
- Save Project
- Open Saved Project
- Save Copy
- Open File
- Settings

Existing secondary actions may remain grouped below.

The old generic top-right project icon should not remain as a second competing entry point.

### B. New Sketch

Inside an open project, `New Sketch` must be obvious from the project drawer and may also remain in Add Primitive.

It enters:
`Pick plane or face in 3D`.

Support:
- principal world planes;
- eligible planar semantic CAD faces.

Must cover at least:
- base Extrude planar caps/sides;
- Add-created planar caps/sides;
- Cut-created planar pocket/bottom/straight-side faces when stable semantic face refs can be resolved.

If partial-Revolve planar caps already have stable semantic refs/frames, make them eligible too; do not use renderer triangles.

Curved faces remain ineligible unless an exact planar support exists.

### C. PlanarFace NewBody

Selection remains a pure atomic set.

For NewBody:
- pure point-contact pieces must be extrudable as separate manifold shells;
- a false `PlanarFacesTouchAtPoint` must not block the OWNER-like inner-area selection.

If the reproducer exposes a true articulation/self-pinch inside one edge-connected group:
- deterministically decompose at articulation nodes into manifold shell groups when that preserves the exact selected area;
- do not silently drop selected cells;
- do not duplicate internal area;
- validate every resulting shell.

Add/Cut keep their own target-body semantics.

## Test policy

Lightweight:
- focused native/UI tests while fixing;
- one focused device class for the three OWNER blockers;
- one final small regression union;
- only then one FAST;
- exact APK.

No FullSharded.

## Gate

OWNER must physically re-review the corrected APK.

No merge before that.
