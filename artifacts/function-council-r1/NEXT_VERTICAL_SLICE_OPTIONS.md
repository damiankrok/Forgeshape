# Candidate next vertical slices (FUNCTION-COUNCIL-R1)

Options, not a plan. Nothing here is authorised, and the order below is not a
ranking. Where the evidence makes one option a prerequisite of another, the
dependency is stated; otherwise the choice is the OWNER's.

Every option is written against the lean validation policy
(`LEAN_VALIDATION_POLICY.md`): it names the Tier 2 list that would prove it and
says whether it would trip a Tier 5 trigger.

## A prerequisite the evidence does establish

Three defects found by this audit sit on paths the options below would touch
(`COUNCIL_FINDINGS.md` §1): **D1** Import GLB unguarded in Sculpt, **D2** the
navigator's Flip and turns not moving the camera, **D3** a palette change
forgetting that the project was saved. Each is small. D1 should be fixed before
any option that touches Sculpt or import, D2 before any option that touches
the sketch view (Options 2–5), D3 before any Settings or project-shell work.
They can ride with Option 1 or go first as their own short task; either way
each needs one focused device case (Tier 2), not an aggregate.

## Option 1 — Validation-harness hardening (tests and CI only)

- **What.** Make the process stop being the fixture: `resetToBaselineConstruction`
  closes and reopens by default (the C3 Mirror pattern), with
  `EditorWorkspaceObjectsTest` the explicit opt-out it already documents; the
  five fixed-delay captures wait for presented frames; the host native suites
  run in `CI FAST`; `CI DEVICE` gains subsystem presets; the six OWNER-material
  classes move to an on-demand dispatch.
- **Why the evidence points here.** All nine tests that stopped the C1–C3
  aggregates were test defects; none was a product defect
  (`TEST_COST_AUDIT.md` §6.5). The order rule in the Tier 2 policy exists only
  because of the inherited scene.
- **Product impact.** None. No `app/src/main` change is needed.
- **Validation.** It changes the shared test support, so it is itself a Tier 5
  trigger: one fresh aggregate at the end, which is also the new baseline.
- **Prerequisite?** Not for adopting the tiers (ordering the lists is the
  workaround). It is the only option that removes a recurring cost from every
  later option.

## Option 2 — Through All, with Cut-by-default direction polish (CAD, small)

- **What.** A fourth extent mode whose distance is DERIVED at regeneration from
  the target body's extent along the feature normal (`POST_AUDIT.md` §4 item 1).
- **User value.** The most common pocket and through-hole, with no number to
  guess.
- **Depends on.** Nothing new: the kernel, the chain and the per-triangle face
  tags exist. A Cut that misses or removes everything is already refused by
  name.
- **Format.** A `CADB` v6 extent code → Tier 5 trigger (format), plus the usual
  per-version cost: spec section, codec predicate and reader flag, PowerShell
  encoder, new fixtures, digests (`COUNCIL_FINDINGS.md`, persistence).
- **Tier 2.** CAD list (`CadVerticalSliceTest`, `SketchUxTest`,
  `CadExtrudeExtentTest`) + `PROJECT` and `CAD` host suites + corpus parity.

## Option 3 — Revolve on the same region and operation model (CAD, medium)

- **What.** A second feature kind, `{regions, axis reference, angle,
  operation}`, through the same New Body / Add / Cut rules, the same kernel
  union and difference, and the same face-tag table; a revolved side is a
  curved face and therefore never a sketch support (`POST_AUDIT.md` §4 item 2).
- **User value.** Turned parts (shafts, knobs, bottles) are unreachable today.
- **Depends on.** An axis reference. A sketch line as the axis is enough for a
  first slice; projected edges (Option 4) would later add body edges as axes.
- **Format.** A `CADB` feature-kind code → Tier 5 trigger (format).
- **Tier 2.** CAD list + `CAD`, `CAD_FEATURE`, `PROJECT` host suites + corpus
  parity.

## Option 4 — Sketch precision: projected edges and inference snapping (CAD, medium)

- **What.** Project the support face's boundary into a face sketch as
  NON-authored reference geometry the endpoint snap can use; add horizontal,
  vertical, parallel and midpoint inference snaps (`POST_AUDIT.md` §4 item 3).
- **User value.** Bosses and pockets placed exactly relative to the body they
  change: the precision gap an OWNER meets first after Add and Cut.
- **Depends on.** A reference-entity kind excluded from regions and from
  `.forge` truth. If references stay non-authored, **no format change** and no
  Tier 5 trigger.
- **Tier 2.** CAD list with `SpatialSketchTest` (face support) + `SKETCH_UX`,
  `CAD_A3` host suites.

## Option 5 — Feature-chain management: delete, suppress, reorder (CAD, medium)

- **What.** The chain is ordered, bounded and reopenable today, but a feature
  cannot be deleted, suppressed or moved (`PRODUCT.md`, "Not yet implemented").
  Each needs a dependency rule first: deleting or suppressing a feature a later
  feature stands on is refused, on the `RefusedHasDependents` pattern.
- **User value.** The feature list becomes a tree the user can edit, which is
  the Inventor/Fusion mental model the chain was built for.
- **Format.** Delete and reorder need none (they are edits of an existing
  chain); **suppress** needs a stored flag → `CADB` v6. If Option 2 or 3 is also
  planned, one v6 bump can carry both, which halves the per-version cost.
- **Tier 2.** CAD list + `CONSTRUCTION_HISTORY` host suite (one act, one Undo).

## Option 6 — Sculpt density: partial upload and a spatial index (Sculpt, medium)

- **What.** Sculpt publication republishes the whole mesh per move, and the
  affected set and the brush pick are linear scans (`PROJECT_STATUS.md`,
  "Sculpt cost model"; the renderer seat found every upload also drains the GPU, `forgeshape_renderer.cpp:868-986`). A dirty-range
  upload and one shared spatial index would let the product sculpt denser meshes.
- **User value.** Detail. Today's sphere seed is ~500 vertices, which the
  recorded measurement says is free; the cost appears only with density.
- **Depends on.** A real-device measurement baseline: emulator frame times say
  nothing about a phone (SwiftShader). So this option needs the OWNER's
  physical device for its acceptance, which makes it mostly Tier 4 (OWNER, physical device).
- **Format.** None.
- **Tier 2.** Sculpt list + `SCULPT`, `RENDER` host suites; a `RenderVertex`
  layout change would be a Tier 5 trigger.

## Not offered as options, and why

- **Stylus pressure, hover preview, symmetry (Sculpt).** `STYLUS-G1` is
  BLOCKED on physical stylus hardware, and the hover preview is blocked on the
  autosave fingerprint read (`PROJECT_STATUS.md` Next Stage).
- **Directional Scale (Stage 020D) and Sculpt dimensions (`SCULPT-DIM-01`).**
  Blocked by the OWNER questions OQ-01 and OQ-02.
- **Fillet, chamfer, shell.** Need a B-rep kernel: a second third-party
  library needs its own gate (`CLAUDE.md`, hard rules).
- **CAD → Sculpt.** Deliberately refused by name today; it is a product
  decision, not a missing piece of plumbing.
