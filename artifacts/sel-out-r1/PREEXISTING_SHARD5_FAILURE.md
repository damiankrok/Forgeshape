# A pre-existing shard-5 failure, found by this stage and NOT caused by it

The authoritative `-FullSharded` aggregate returned `FULL_SHARDED_SUITE_FAIL`:
shards 1–4 PASS (112/112 each), shard 5 `ASSERTION_FAILURE` with two failures,
both in `SpatialSketchTest`:

```
1) faceSupportedSketchExtrudesADependentAndPersists(com.forgeshape.app.SpatialSketchTest)
   E2E-CADA3-12: the tap aims at a CAD face, not a world plane expected:<3> but was:<1>
2) aCylindricalSideIsRefusedAsASupport(com.forgeshape.app.SpatialSketchTest)
   the cap is expected:<3> but was:<-1>
```

The runner classified it `PRODUCT_TEST_FAILURE`, which is the correct
mechanical classification — there is attributable assertion evidence. Whether it
is attributable to **this stage** is a separate question, and it is not.

---

## What was measured

| step | command | result |
| --- | --- | --- |
| 1. the aggregate | `-FullSharded -ShardCount 5 -Fresh` | shards 1–4 PASS; shard 5 `ASSERTION_FAILURE`, 2 failures |
| 2. the class alone | `-TestClass ...SpatialSketchTest` | **OK (8 tests)** |
| 3. shard 5 exactly | `-FullSharded -ShardCount 5 -ShardOnly 5` | reproduces, **same two failures** — deterministic, not a flake |
| 4. immediate predecessor | `ProjectAutosaveRecoveryTest,SpatialSketchTest` | OK (22 tests) |
| 5. the sculpt predecessor | `ImportedMeshSculptTest,SpatialSketchTest` | OK (17 tests) |
| 6. the four chrome/layout predecessors | `...ChromeComposition,...Layout,...Legibility,...Mobile,SpatialSketchTest` | OK (65 tests) |
| 7. narrowing | `GlbImportPreviewTest,ImportedMeshSculptTest,ProjectAutosaveRecoveryTest,SpatialSketchTest` | reproduces |
| 8. **minimal repro** | `GlbImportPreviewTest,SpatialSketchTest` | **reproduces, 2 failures out of 31** |
| 9. **the same minimal repro at the clean baseline** `538623c2`, this stage's work stashed and the APKs rebuilt from it | `GlbImportPreviewTest,SpatialSketchTest` | **reproduces, identically — 2 failures out of 31** |

Step 9 is the decisive one. With **none** of this stage's code present — the
outline module, the shaders, the renderer passes, the display flag, the JNI
methods and both new test classes all absent — the two failures appear with the
same messages and the same expected/actual values.

## Why it is not this stage's

* it reproduces at the baseline commit with the work stashed (step 9);
* this stage's diff contains **no** CAD, sketch, picking, support-chooser,
  workplane, scene or construction file, and neither `SpatialSketchTest` nor
  `SketchTestSupport` is modified;
* the outline is presentation only. Picking and the support chooser are CPU ray
  casts against the scene, and no renderer state reaches either;
* `SpatialSketchTest` passes alone on this stage's build (step 2).

## What the defect actually is

Both assertions are *"a tap aimed at a CAD face landed somewhere else"*.
`tapWorld` projects a world point through `debugProjectWorld` and taps that
pixel; the support chooser then ray-casts. `SpatialSketchTest.setUp` calls
`resetToBaselineConstruction` and captures `baselineProject` — but a reset
restores the **active body**, it does not remove bodies an earlier class left in
the scene. Running after `GlbImportPreviewTest`, the class inherits a populated
scene, so the ray at the CAD body's far cap `(0, 0, 2)` meets something else
first: a world plane (kind 1) in one case and nothing eligible (-1) in the
other.

It is the same class of fragility this stage hit in its own new tap test —
`e2eSeloutr1_02_03` failed in class order and passed alone until it was made to
start from a fresh one-body project. That fix is in this stage's code because
the test is this stage's; `SpatialSketchTest` is not, and repairing it would be
scope expansion into `CAD-A3`'s suite.

## Why the aggregate was not re-run to a PASS

`FULL_SHARDED_SUITE_PASS` requires every shard to pass. Shard 5 fails
deterministically at the baseline too, so **no re-run of this stage can produce
that marker**, and the runner is right to refuse it: manufacturing one would
mean weakening the criteria, which `TEST-RUNTIME-R1` and this stage's brief both
forbid.

What is claimed instead, precisely:

* **shards 1–4: PASS, 448/448**, including shard 4, which carries
  `SelectionOutlineTest`, `EditorWorkspaceDisplayTest`, `ObjectsDeleteTest`,
  `ImportedMeshDurableTest` and `CadA3VisualEvidenceTest`;
* **shard 5: 110 of its 112 pass**; the two that fail fail identically without
  this stage;
* every other gate is green — see `TEST_RESULTS.md`.

The correct next action belongs to the coordinator, not to this stage: either a
small `CAD-A3` follow-up that isolates `SpatialSketchTest`'s scene the way this
stage isolated its own tap test, or an explicit owner waiver of the aggregate
gate for a failure proven to pre-date the work.

---

## OWNER disposition — waived, 2026-09-06

The coordinator's call was made and it is a **waiver**. The OWNER cancelled
further SEL-OUT closeout testing and directed that feature work continue, so:

* **no additional SEL-OUT test was run** after this document was written — not
  the aggregate, not a reshard, not a focused rerun;
* `FULL_SHARDED_SUITE_PASS` remains **not claimed** for SEL-OUT-R1, exactly as
  stated above. The waiver excuses the gate; it does not manufacture the marker,
  and no criterion was weakened to produce one;
* the SEL-OUT-R1 candidate was committed as-is, with the product behaviour
  unchanged from what the evidence in this directory measured.

The `SpatialSketchTest` isolation defect is **retained as known test debt** for a
later test-hardening batch. It is not repaired here, and because no aggregate is
run under the waiver it blocks nothing.
