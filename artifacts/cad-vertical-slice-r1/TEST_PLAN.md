# Test plan — CAD Vertical Slice R1

Every case below is linked to its source. The results are recorded in
`TEST_EVIDENCE.md`.

## 1. Native self-tests

Suite: `CAD_FEATURE`, 141 checks. Source:
`app/src/main/cpp/forgeshape_cad_feature_selftest.cpp`.

The suite runs in two places:

- **At every debug launch**, emitting `FORGESHAPE_CAD_FEATURE_SELFTEST_OK`. It
  is the 23rd startup token and the last one.
- **On the host**, via `bash scripts/host-native-selftests.sh [CAD_FEATURE]`.

| Prompt §17.1 area | Checks |
| --- | --- |
| Kernel gate (§9.3) | `CADVS_K00..K13` and `K07b`. See `KERNEL_GATE.md` §4. |
| Region extraction: single rectangle, single circle, rectangle ⊃ circle, disjoint regions, deeper nesting, CW/CCW, self-intersection, tangent/coincident, stable refs across edits, canonical order | `CADVS_REG_01..26` |
| Extrusion of regions with holes: triangulation, wall winding, watertight edges, One Side / Symmetric / Two Sides, finite and in range, legacy mesh unchanged | `CADVS_EXT_01..16` |
| Operation truth: New Body, Add, Cut canonical; invalid code; no target; disjoint Add; missing Cut; total Cut | `CADVS_OPS_01..10`, `CADVS_SES_10`, `_13` |
| Feature chain: legacy, base + Add, base + Cut, base + Add + Cut, upstream edit regenerates downstream, downstream invalidation fails closed, id stable, capture/restore bit-exact, deterministic, bounds | `CADVS_OPS_11..29`, `CADVS_SES_11..30` |
| Session flows with a real camera: taps, toggles, candidate revisions, commit, Undo/Redo, feature edit, direction-by-operation | `CADVS_SES_01..31` |
| Persistence: v1–v4 unchanged, v5 round trip, invalid cases refused, digests pinned to the independent encoder, lineage tokens, fingerprint | `CADVS_IO_01..23` |
| Performance (bounded, every run) | `CADVS_PERF_01` → `FORGESHAPE_CAD_FEATURE_PERFORMANCE` |

**Every older suite still runs unchanged**, except for two checks whose premise
this milestone replaced. Both were restated rather than weakened:

- **`CADR0_21`.** Before: the outer loop of a nest was refused. Now: the outer
  loop is a region with the circle as its hole, and a legacy state that names
  the rectangle without that hole is refused `ProfileRegionMismatch`.
- **`CADUXS1_09_d`.** Before: 60 dp × 0.80 = 48 dp. Now: the 0.80–1.60 band is
  pinned by value, and the 48 dp hit floor moved to the JVM, where the HUD
  actually decides it.

**Supporting checks:**

- **Mutation proof.** The suite was compiled once with seven deliberately wrong
  expectations. They covered volume, status, failing feature id, a byte offset,
  cache identity and hatch count, and every one failed.
- **Release guard.** `scripts/ci-release-selftest-guard.sh` now also matches
  `CADVS_`. It requires 0 self-test symbols and 0 strings in release, and more
  than 0 of both in debug.

## 2. JVM tests

Source: `app/src/test/java/com/forgeshape/app/`.

| Prompt §17.2 item | Test |
| --- | --- |
| Tool Labels default OFF | `AppPreferencesTest.theDefaultsAreTheProductAsItShipped` |
| Preference round trip, forgiving read, `withToolLabels` | `AppPreferencesTest` (13 tests) |
| Extent icon mapping, operation icon mapping, offered operations by bitmask, Flip only in One Side | `CadHudPresentationTest` (16 tests) |
| Glyph 24–32 dp at every scale 0.8–1.6; hit ≥ 48 dp always, and independent of the glyph; smaller than the old 60 dp pills | `CadHudPresentationTest` |
| Sketch rail present while drawing, absent in Ready; navigator and Line dimension the same; Back to Sketch only in Ready | `SketchChromePolicyTest` |
| Precision surface not auto-opened at Finish | `SketchChromePolicyTest.finishSketchLeavesThePrecisionSurfaceCollapsed` |
| Operation and extent controls absent in unrelated modes | `CadHudPresentationTest` (bitmask → offered). On the device: `CadCanvasExtrudeTest._08` and `CadVerticalSliceTest.operation_refusal`. |

## 3. Device instrumentation

New class: `app/src/androidTest/java/com/forgeshape/app/CadVerticalSliceTest.java`.

| Prompt §17.3 case | What it asserts |
| --- | --- |
| `owner_rectangle_circle_region` | Two regions after Finish, none chosen, no arrow, the toolbar's Extrude absent and a commit refused. A tap in the ring selects the rectangle-with-hole; the PREVIEW candidate measures ring area × depth in one shell. A tap on the disk switches to it: new candidate revision, cylinder volume. A tap back returns to the ring. The committed body's volume and X extent are checked. |
| `compact_extrude_hud` | The cluster is ≤ 360 dp wide and under 3 % of the viewport (was 6.78 %). Extent, operation and Flip are icon controls: ≥ 48 dp hit, 24–32 dp glyph, unscaled, with descriptions. The value is ≤ 4 dp from the shaft anchor (was 99.5 dp), and still is after an orbit. The palette controls meet the same floor. Tool Labels: OFF by default; ON shows captions and stays ≤ 360 dp; it survives Activity recreation and writes no project byte. |
| `new_body_then_add_same_body` | Add and Cut are offered on a face. Add preview: valid, targeting the base. The commit returns the SAME id, the body count is unchanged, one Undo step, two features. Volume 4 + area × 0.5, max Y 1.5. Undo and Redo are exact. |
| `same_body_cut` | Choosing Cut points the extrusion into the body. The commit returns the same id, the count is unchanged, one step. Volume 4 − area × 0.5, bounds unchanged. Undo and Redo are exact. |
| `operation_refusal` | A world plane offers New Body only; Add is refused `OPERATION_NEEDS_TARGET` and no Add or Cut control is drawn. A Cut flipped outward is previewed as `CUT_NO_INTERSECTION`, the toolbar's Extrude is absent, the operation badge names the reason, and the commit is refused. A disjoint Add is `ADD_DISJOINT`. The project bytes are identical afterwards. |
| `feature_edit_roundtrip` | The feature list shows 2 rows. Row 2 reopens the Add in place (Ready, operation Add). Re-extruding at 0.25 is one step, same body; Undo and Redo are exact. An UPSTREAM base depth edit (1 → 1.5) carries the Add to the new cap: volume 6 + area × 0.25, max Y 1.75. Save, reopen, same volume and triangle count, re-encoded byte-identically. |
| `ui_context_withdrawal` | While drawing, the navigator and the rail are shown. In Ready, the navigator, the drawing tools and the Line dimension are absent; the precision surface is collapsed; Back to Sketch, Cancel, the precision toggle, Extrude and the HUD stand. Back to Sketch restores the drawing chrome. The precision surface opens on request and lists the region. |

Each case writes its facts and captures to
`files/evidence/cad-vertical-slice-r1-after/`. `CI DEVICE` pulls them into
`test-evidence/`.

## 4. Existing regressions kept (prompt §17.4)

These classes were kept, and updated only where this milestone deliberately
changed a contract. None was deleted, and no assertion was loosened beyond the
new contract.

- **`CadCanvasExtrudeTest`**
  - `_01`: the badge's content description says New Body. The glyph is not a
    text label. Every hit ≥ 48 dp and unscaled; this replaces the 60 dp ×
    0.80 arithmetic.
  - `_08`: on a WORLD-plane sketch Add and Cut are not drawn, and are refused
    by name. This replaces "Add and Cut do not exist".
- **`CadExtrudeExtentTest`**: the 48 dp floor is asserted on the one extent
  control at rest. The three choices live in its palette, and their floor is
  asserted with the palette open by `CadVerticalSliceTest`.
- **`SketchExtrudeTest`**: the precision surface is collapsed at Finish. The
  test opens it with its toggle, then asserts the region row and the depth
  field as before.
- **`SettingsPreferencesTest`**: sixteen preference rows plus Back. Tool Labels
  is OFF and chosen. `plantForVerification` gained the Tool Labels argument.
- **`Ui3dStateAuditTest`**: in Ready, the extent control is MUST_SHOW, and the
  navigator and the drawing tool are MUST_HIDE.
- **Unchanged and re-run:** `SpatialSketchTest` (face sketch → New Body is
  still the default), `SketchUxTest`, `HomeFlowTest`, `Ui3dStateCorrectionTest`
  (anchored controls).
- **Retired:** `CadVerticalSliceBeforeTest` — the Phase 0 reproduction, retired
  by the commit that fixes what it reproduced. Its evidence stays in `before/`.

## 5. Gates

- **`CI FAST`** on the candidate SHA:
  - build: debug, release, androidTest;
  - JVM tests;
  - the release guard;
  - corpus parity: all 44 fixtures regenerated by the PowerShell encoder,
    byte-identical;
  - runner and guard checks.
- **`CI DEVICE`** on the same SHA:
  - 23/23 startup tokens in order, `NATIVE_VIEWPORT_OK`, 0 failures;
  - the focused class list, `CadVerticalSliceTest` first, then the regressions
    above;
  - it is dispatched with `test_class` set to a comma-separated list, and is
    focused-subset evidence only.
- **Attempt limit.** At most two fresh attempts on the final candidate.
- **Milestone aggregate (`-FullSharded`).** It is a Windows PowerShell runner
  driving an isolated local AVD. It cannot run in this cloud session, so it is
  reported as BLOCKED by an unavailable harness, never self-waived.
