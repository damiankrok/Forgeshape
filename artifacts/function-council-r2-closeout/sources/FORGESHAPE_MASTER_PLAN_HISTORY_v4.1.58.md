# ForgeShape Master Plan History v4.1.58

Date: 2026-10-06

## First physical review of Modeling R1

OWNER tested the combined Modeling R1 APK and found three blocking CAD workflow regressions.

### 1. Missing New Sketch workflow

The expected existing-project flow:
`New Sketch -> pick plane/model face`
is no longer discoverable/reachable in the UI.

Source audit confirms the underlying support chooser still exists and explicitly supports face picking in existing projects.

### 2. Inner PlanarFace selection/extrude refusal

A complex sketch produces:
`Those areas meet only at a point...`

Current arrangement code already partitions pure point-only contacts into separate components, so the exact cause must be reproduced rather than assumed.

### 3. Missing ForgeShape project mark/drawer

The editor currently uses a generic project icon at the top-right and hides it in bootstrap.

OWNER's intended shell is a top-left ForgeShape mark expanding into the project actions panel.

The existing project action logic can be reused.

## Additional source finding

Cut-created CAD faces are still blanket-ineligible for sketch support.
That old debt is now promoted into the physical-review correction because the user explicitly expects sketch-on-face after 3D modelling.

## Decision

No integration.

Create one combined correction task and return one corrected APK for physical re-review.
