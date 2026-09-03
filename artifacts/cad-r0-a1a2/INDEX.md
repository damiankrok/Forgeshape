# CAD-R0-A1A2 - evidence index

**Result: `PASS-CAD-R0-A1A2-OWNER-RETEST-READY`.**

`Choose workplane -> create/edit a 2D sketch -> detect a closed profile ->
Extrude -> a durable parametric CAD Body -> edit dimensions later -> Undo/Redo
-> Save/Open .forge` works end to end in the real app, through the real chrome
and real MotionEvents, on the isolated AVD.

| | |
| --- | --- |
| Baseline HEAD | `2bcfa3fa11fd96fa934887f16e55caefbee496d9` (matched exactly; tree clean) |
| Native suites | **18/18**, 2845 checks, zero failures (122 new `CADR0_*`, 11 new project checks) |
| JVM | 70/70 |
| Builds | debug and release, `arm64-v8a` + `x86_64` |
| Device guards | `DEV2-01..07`, `DEV3-01..06` PASS |
| `.forge` corpus | sixteen fixtures; the twelve older ones byte-identical; four new ones pinned by two independent encoders at identical digests |
| Focused device suite | `SketchExtrudeTest` **OK (9 tests, E2E-CADR0-01..16)** |
| Authoritative aggregate | **`FULL_SHARDED_SUITE_PASS`** - 35 classes / 491 tests / 5 shards (100 + 98 + 99 + 97 + 97), missing = duplicates = unexpected = 0 |
| Device | `emulator-5580`, confirmed `ForgeShape_Stage006`. `emulator-5554` never contacted. |

## Files

| File | What it holds |
| --- | --- |
| `DOMAIN_MODEL.md` | the CAD architecture, identities, truth vs derived, bounds, the status vocabulary |
| `WORKPLANE_CONTRACT.md` | the three frames, the mapping, the camera it is read from |
| `SKETCH_ENTITY_CONTRACT.md` | the four entities, their rules, numeric editing |
| `PROFILE_EXTRACTION.md` | candidates, loop rules in order, nesting, determinism, multiple profiles |
| `TRIANGULATION.md` | bounded deterministic ear clipping; circle tessellation |
| `EXTRUDE_CONTRACT.md` | the watertight, canonically wound New-Body extrusion |
| `PARAMETRIC_REGENERATION.md` | the one path, where it runs, atomicity |
| `DATA_CONTRACT.md` | why `CADB` is a required section on `IMPT`'s terms and not a `CONS` extension |
| `CORPUS.md` | the four fixtures with bytes and digests |
| `TEST_RESULTS.md` | the `CADR0-01..40` table and every gate |
| `DEVICE_E2E.md` | the `E2E-CADR0-01..16` table, and the two runs before the passing one |
| `PERFORMANCE.md` | bounded timings, warm and cold |
| `NATIVE_SELFTEST_CADR0.txt` | a clean debug launch: all eighteen `*_SELFTEST_OK`, the 122 `CADR0_*` passes, the eleven digests, the performance line |
| `FOCUSED_SKETCH_EXTRUDE.txt` | the focused device run |
| `DEVICE_SKETCH_TRACE.txt` | the `FORGESHAPE_SKETCH_*` / `FORGESHAPE_CAD_APPLY` lines that run left |
| `FULL_SHARDED.txt` | the passing aggregate, including `DISCOVERY_PASS` and every `SHARD_RESULT` |
| `FULL_SHARDED_RUN1_FAIL.txt` | the first aggregate, which failed one pre-existing test; see below |

## The one aggregate failure, and what it was

The first full run failed `EditorWorkspaceMobileTest.uir4a04` (shard 4, 1 of
97). That test counted the palette's creation actions one nesting level deep
and regardless of visibility, so with the plane chooser inside the palette it
counted the four hidden plane controls and none of the tiles. The palette now
legitimately offers seven creation actions - the six primitives and New Sketch,
which the brief named - so the test was updated to count VISIBLE clickable
leaves recursively, to expect seven, to assert New Sketch is offered, and to
assert the plane chooser is not on screen until asked for. Its purpose - no
disabled placeholder - is unchanged and still enforced. That is test code, so
the aggregate was rerun once from shard 1 on the final tree and passed.

## Decisions worth the owner's eye

1. **A third representation** rather than a `CONS` extension: a CAD Body has
   no primitive and no fixed geometry, so it is its own, on the Imported Mesh's
   terms. `DATA_CONTRACT.md`.
2. **CAD -> Sculpt is not this stage.** Refused by name, control absent, `SCUL`
   over `CADB` refused. The three decisions it needs are in `ARCHITECTURE.md`.
3. **Select is a sketch tool**, so a tap in the viewport has exactly one
   meaning at a time; and **one finger never orbits while sketching** - two
   fingers pan and pinch.
4. **A nested profile is refused as a hole**, with the inner one still
   extrudable; overlapping profiles are both kept.
5. **The sketch is volatile until Extrude**: no checkpoint, no fingerprint,
   no `.forge` byte before the one-transaction commit.
6. **The chrome added is inside the accepted structure**: a second group in
   the palette, entries on the same rail, a group in the same trailing host,
   one toolbar transition per sketch state (no abbreviated form), two more
   bodies for the same precision surface.

## Deviations and remaining debt

- CAD -> Sculpt, polygon-profile point editing, an adaptive sketch grid, the
  top view's pitch clamp, and edit-session recovery are recorded as bounded
  debt in `PROJECT_STATUS.md`.
- Process death mid-sketch loses the sketch by contract (R0 rule). Process
  death across a SAVE is covered by the existing persistence E2E machinery;
  `E2E-CADR0-11` proves the document half in-process.
- No screenshots were taken: geometry is proved by the native suites and the
  device traces, and no evidence script may locate a control by coordinate.
