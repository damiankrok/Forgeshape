# CAD-R2-C1 — SUMMARY (post-implementation quality gate)

Read `BEFORE.md` first. Track evidence: `PERF_PROTOCOL.md`, `PERF_ANALYSIS.md`,
`PERF_RAW.tsv`, `PERF_RAW_SUITE.tsv` (A); `HIT_OWNERSHIP.md` (B); `TESTS.md`
(all gates and every device attempt).

## Refs

| Ref | SHA |
| --- | --- |
| Base (exact, the R2 head) | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` |
| Branch | `feature/cad-r2-c1-quality-gate-r1` |
| Last product commit (frozen product tree) | `cf340f0` |
| Device union | `40752389411` 67/67 on `cf340f0` + `40760227555` 55/55 on `e81639b` (app tree = `cf340f0`) — **122/122** |
| Final CI FAST | `40766013297` on `911f5c3` (docs only after `cf340f0`), success |
| OWNER APK | `ci-fast-evidence` 12849379257, `app-debug.apk` 17,552,535 B, SHA-256 `fd7a11a9a7b1bbf7748b46e2d71c71a2a830b54f59e8bd7f9011113d968b1dd5`, expires 2026-11-11T20:52:13Z |

`main` `6c9f156`, the R2 branch `ea24d20` and the R2 predecessor `636797e`
were not touched. No merge, rebase, cherry-pick, force push, FullSharded run or
format change.

## Per-risk outcome

| Track | Outcome | Product change |
| --- | --- | --- |
| A — performance | `eval_all_us`, `drafting_trim100_us`, `revolve_square_max_us`: **NO-REPRO-STABLE** for the product code. The suite-level gap reproduces (+32.5 % / +38.2 %, 24 + 24 ABBA runs) but follows the host binary's LINK LAYOUT: the same R2 code is 25 % faster in another link of itself, a product-only link removes the A/B gap (+1.3 % / +1.0 %), the 32-byte branch mitigation removes it in the original link (−1.1 % / −1.6 %, p 0.76 / 0.63), and ablating R2's only new work in the path changes nothing (its direct cost: 1.1 µs) | none |
| B — chooser ownership | **BUG FIXED**: a tap on a surface that cannot carry a sketch (curved Cut wall, revolved body) fell through to a world plane BEHIND it, and a second tap opened a sketch there. A plane IN FRONT keeps the tap (**INTENDED-VISIBLE-PLANE-PRIORITY**, asserted exactly) | `forgeshape_support_chooser.cpp` (`2e4e962`) |
| C — font scale / lifecycle | **BUG FIXED**: at system font 1.5 in portrait the History surface grew over the body's projected sketch anchor and the canvas Edit Sketch chip stood on History's own Edit Sketch act and took its tap. Now 1.0 / 1.3 / 1.5 × portrait / landscape all pass (48 dp, in-window, apart, own centre, detail clear) | `CadExtrudeCanvasView`, `EditorWorkspaceView` (`cf340f0`) |

## Product changes (complete)

1. `app/src/main/cpp/forgeshape_support_chooser.cpp` — in `resolve`, a body
   surface nearer than the best plane hit (by > 1 mm) occludes that plane; an
   eligible face then replaces the answer as before. No eligibility, lineage,
   token, retargeting or renderer change. Header comment of `select` updated.
   Reproducer: R2 device D04 (`curved_kind=2`, a YZ sketch opened); host
   C1_CH_02/05/06 fail before, pass after.
2. `CadExtrudeCanvasView.setRetainedChipWithdrawnWhen` + one condition in
   `showRetainedSketchChip`; `EditorWorkspaceView` wires it to
   `featureHistory::isOpen` and refreshes the canvas when History opens or
   closes. Reproducer: device run `40744870935` D17 (`cad_canvas_edit_sketch
   [477,1137 126x126]` over `history_act_edit_sketch (403,1139 - 707,1265)`).

Docs: `ARCHITECTURE.md` (chooser ownership sentence), `CLAUDE.md` (the chip
rule gains History; the nearest-target rule), `PRODUCT.md` (verified
behaviour), `PROJECT_STATUS.md`, this folder. Tests: host
`forgeshape_cad_r2_selftest.cpp` (+6 C1_CH), device
`CadRetainedSketchOwnerTest` (D04 strengthened, D15–D18 new). Evidence-only
harness under `artifacts/cad-r2-c1-quality-gate/perf/` (not built by Gradle
or CMake, not in any APK).

## A2 world-plane retained sketch — DESIGN HOLD (no implementation)

`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH` stands; `R2_SK_06`
still proves the refusal (a second root record is `SketchSupportInvalid`,
`canKeepSketch` false, `commitSketch` `NotSketching`), and device D18 proves
Keep Sketch is not drawn on a world-plane sketch and native refuses it. The
choice is the OWNER's:

| Option | Data / format | Compatibility | UX risk |
| --- | --- | --- | --- |
| **0. Keep the hold** | none | none | a construction sketch on XY/XZ/YZ inside a body still needs a face of the body |
| **1. Additional ROOT sketches** (a sketch record placed on a world workplane, owned by the body, no feature) | `CadSketchRecord` gains a world placement for non-base records; `validateCadSketchTable` accepts more than one root; needs a new `CADB` version (v9) carrying it, written only when such a sketch exists | v1..v8 files unchanged; an older build must REFUSE v9 (required section at an unknown version), never drop the sketch | the body's own transform would carry a "world" sketch — the user must understand it is body-local, not world-fixed |
| **2. Body-local construction planes** (an authored plane frame owned by the body; a sketch stands on the plane by id) | a new authored entity type (plane id, frame), new `CADB` version, a reference rule (plane must exist; delete refused while used) | as option 1, plus a new dependency edge to validate on load | a new concept to teach (custom workplanes were explicitly "not this stage" before) |
| **3. Project-level sketches** (a sketch owned by no body) | a new project section beside `CADB`; a sketch that is not a body breaks "a project is its bodies" | every reader must learn a non-body object; Home/History/fingerprint all change | largest: the Objects capsule, delete, duplicate and History all need a non-body row |

Recommendation for the decision (not a choice): option 1 is the smallest
format delta that keeps every existing invariant (one body owns its sketches,
ids per forward branch, all-or-nothing load); option 2 is the more general
answer and should only be taken together with custom construction planes.

## Known issues / OWNER later

- **Pick crack (pre-existing):** a ray through the exact edge two triangles
  share can miss both at float precision with a far orthographic eye
  (`HIT_OWNERSHIP.md`); such a pixel resolves as if no body were there. Belongs
  to picking, not this stage.
- **Host perf layout sensitivity (pre-existing):** the arrangement hot path
  swings ~25–35 % with code placement on JCC-affected x86 hosts, so single-shot
  host-suite timings across commits that change the link are not comparable.
  Device (ARM64) timing was not measured here.
- A refused chooser tap clears the previous aim (as a tap on empty space always
  has) rather than keeping it; chosen so an intervening miss can never make the
  next tap a confirm. Recorded as an interpretation of "chooser state
  unchanged" (the chooser stays active; no sketch, no project change).
- All device evidence is the CI emulator (swiftshader). Stylus hardware,
  hover, a hardware GPU, 16 KB pages and real-device performance stay OWNER
  physical checks.

## OWNER physical test pack

1. Block with a round Cut and a rectangular Cut. New Sketch: tap the round
   pocket's curved wall from several angles, twice each — nothing opens; tap a
   plane square where it stands in FRONT of the block — it highlights, a second
   tap opens a sketch on that plane; tap the rectangular pocket floor and wall
   — they open face sketches.
2. Aim a face, orbit with one finger, then confirm — still that face.
3. Settings → Display → Font size at the largest step; portrait and landscape:
   History → Sketches → select an unused sketch: Edit / Add / Cut / Delete
   readable and each tappable; the floating Edit Sketch control is not over
   them; close History — the control returns.
4. Keep Sketch, Use for Add, Delete unused, Edit Feature + Cancel at the
   largest font; Undo after each.
5. With a stylus: the same chooser and History taps.
6. Same project on a real phone: time the 3969-face / large-grid edits by feel
   against the previous APK (performance on ARM64 is unmeasured here).
