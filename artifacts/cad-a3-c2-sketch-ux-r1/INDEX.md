# CAD-A3-C2 / SKETCH-UX-R1 — evidence index

**Result: `PASS-CAD-A3-C2-SKETCH-UX-R1-OWNER-RETEST-READY`** — see
`TEST_RESULTS.md` for the per-criterion table, `FULL_SHARDED.txt` for the
authoritative aggregate.

An OWNER-requested UX correction plus an approved capability expansion. The
owner rejected Home as a card floating over an empty viewport, and rejected a
first sketch that begins by tapping floating planes in an empty 3D world.
`OWNER_FAILURE.md` reproduces both, explains why they looked that way, and
records what changed and what deliberately did not.

The journey the owner will review:

`launch -> a full-screen Home -> New Project -> CAD -> an immediate flat XY
sketch -> choose a plane and a view from the orientation navigator -> draw a
Line, an Arc, a Spline -> select a Line, read its technical dimension, type an
exact length -> Extrude = the first body and the project -> Edit Sketch ->
Finish = one Undo`

with the later face-picking New Sketch, New Sculpt, Open File and the
unsaved-changes guard unchanged beside it.

## The documents

| File | What it owns |
| --- | --- |
| `OWNER_FAILURE.md` | the owner's two observations, reproduced, explained and corrected |
| `HOME_PAGE.md` | `StartPageView`, why a page and not a modal, insets, the motif |
| `ORIENTATION_NAVIGATOR.md` | the six orientations, the roll, why the view is not the plane, the switching rules |
| `LINE_DIMENSION_CONTRACT.md` | the annotation, the label, and exactly what a typed length means |
| `CURVE_DOMAIN.md` | Arc and Spline: what is authored, what is derived, the one chain walker |
| `EDIT_SKETCH.md` | the staged session, the one-transaction Finish, the dependency refusal |
| `DATA_CONTRACT.md` | why `CADB` v3 and not something else; compatibility both ways |
| `CORPUS.md` | the six new fixtures, their digests, and the 22 unchanged ones |
| `TEST_RESULTS.md` | `CADUXR1-01..40`, and the two defects the tests found |
| `DEVICE_E2E.md` | `E2E-CADUXR1-01..16` and where each is driven |
| `VISUAL_EVIDENCE_C2.md` | twelve composed-display frames with measured facts |
| `OWNER_CONTACT_SHEET_C2.png` | those twelve frames, four per row |
| `frames/` | the full-resolution captures and `facts.txt` |
| `FULL_SHARDED.txt` | the authoritative exhaustive-sharded aggregate |

## Verification summary

| Gate | Result |
| --- | --- |
| Native self-tests | **20/20 suites, 2970 checks, 0 failures** on device; `FORGESHAPE_SKETCH_UX_SELFTEST_OK (50 checks)` is the new one, `FORGESHAPE_NATIVE_VIEWPORT_OK` after them |
| Standalone native runner | 18 platform-neutral suites, 2523 checks, 0 failures |
| JVM unit tests | BUILD SUCCESSFUL |
| Build matrix | debug and release, `arm64-v8a` and `x86_64` |
| Device guards | all `DEV2-01..07` and `DEV3-01..06` PASS |
| `.forge` corpus | 28 fixtures; the 22 older ones byte-for-byte unchanged; the 6 new ones agree between the production codec and the independent PowerShell encoder |
| Focused matrix | `CADUXR1-01..40` all PASS |
| Device E2E | `E2E-CADUXR1-01..16` all PASS (step 15 with a stated note) |
| Authoritative aggregate | `FULL_SHARDED_SUITE_PASS` — discovery PASS (39 classes, 519 tests, missing=0, duplicates=0, unexpected=0), all 5 shards PASS with exact expected counts. `FULL_SHARDED.txt` |

The final launch, on the final tree:

```
FORGESHAPE_SKETCH_UX_PERFORMANCE arc_tess=218us/200 spline_tess=273us/200
                                 curve_profile=1958us/50 curve_regen=1333us/50
FORGESHAPE_SKETCH_UX_SELFTEST_OK (50 checks)
FORGESHAPE_NATIVE_VIEWPORT_OK
```

About 1.1 us to tessellate an arc, 1.4 us a spline, 39 us to extract a curve
profile and 27 us to regenerate a curve body's mesh — all bounded, and all far
below anything a frame notices.

## No aesthetic approval is claimed

The twelve frames are for the owner to look at. Nothing in this evidence
asserts that the start page, the navigator's placement or the dimension's
legibility are *right* — only that they are what the brief asked for, that they
behave as specified, and that they are measurable. What the review is for is
what no emulator settles: how all of it reads on real hardware, at real sizes,
under a real finger and stylus.

## Deviations and remaining debt

1. **Live length during Line creation** (brief E2, "preferred"). The dimension
   is shown for a **selected** Line, which the brief makes mandatory. A live
   readout during the drag was not added: the drag's rubber band is already in
   the overlay, and adding a second chrome view that follows a moving finger
   would have to be positioned every `ACTION_MOVE` — a per-event layout pass on
   the gesture path. The brief allows this as a bounded deviation with an
   explanation; this is it.
2. **A curve-specific save/kill/reopen device case** (brief L15). Not added.
   The bytes are proven bit-exact by `CADUXR1-26`/`29` and pinned against an
   independent encoder by `CADUXR1-38`; the kill/reopen machinery is
   representation-neutral and already covered by `ProjectProcessDeathTest`. See
   `DEVICE_E2E.md`.
3. **Tablet layout** (brief I2). No separate tablet capture is claimed: the
   harness has no second display, and the layout suites simulate window sizes
   for the chrome rather than a real tablet. The start page caps and centres its
   content on a wide window, and the navigator caps its width; both are
   exercised by the existing width-class layout suites.
4. **A spline's authored points are edited by redrawing**, not by dragging an
   individual control point. The curve interpolates its points and the format
   stores them, so per-point dragging is a gesture on top of an existing truth
   rather than new truth; it was not part of the brief's minimum and is not
   started.

## Scope held

Not implemented, and no code, control or string for any of them exists:
booleans / Add / Cut, fillet, chamfer, shell, revolve, taper, a geometric or
dimensional constraint solver, custom construction planes, sketches on curved
faces, sketches on Imported or Sculpted surfaces, projected edges, CAD to
Sculpt, a feature-tree redesign, cloud or recents, a desktop ribbon, and
freeform surface modelling.
