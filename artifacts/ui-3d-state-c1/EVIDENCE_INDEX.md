# UI-3D-STATE-C1 — Evidence index

Every artifact this correction produced, and what it shows. The original audit's
evidence under `artifacts/ui-3d-state-audit-r1/` is **unchanged**: the failing
run it recorded is the "before" half of every comparison here.

**Device for every runtime artifact:** `ForgeShape_Stage006` / `emulator-5580`,
1080 × 2400, density 2.625, identity confirmed with `adb -s emulator-5580 emu avd
name`. `emulator-5554` was never contacted and was not attached.

## Ledgers

| file | what it is |
| --- | --- |
| `STATE_MATRIX_FINAL.tsv` | the original 117 lifecycle assertions, re-run unchanged against the final tree. 117 executed, **117 PASS, 0 FAIL**. The `expected` column is the audit's own `MUST_SHOW` / `MUST_HIDE` / `MAY_SHOW`; no expectation was rewritten |
| `SPATIAL_AFTER.tsv` | the re-run's spatial ledger, in the audit's own format. 72 rows: 60 measured (all PASS), 12 CLAMPED, 0 NOT_MEASURED |
| `SPATIAL_BEFORE_AFTER.tsv` | the two ledgers joined on `case + action + surface`, one row per attachment, carrying both errors and both verdicts. This is the file to read for the correction's effect |
| `NOTES.tsv` | the re-run's diagnostic reads, including `overlayPadding` at every dimension measurement — the padding the conversion now absorbs |
| `logs/audit-rerun.log` | the strict re-run: `OK (9 tests)` in 103.6 s |

## Reading `SPATIAL_BEFORE_AFTER.tsv`

- `before_verdict = FAIL, after_verdict = PASS` — 50 rows, the whole measured
  set. Median 48.87 → **0.16 dp**, max 282.53 → **0.25 dp**.
- `before_verdict = NOT_MEASURED` — 12 rows. The surface was absent before
  (`UI3D-F-003`) so nothing could be measured; 10 are now PASS and 2 are
  correctly CLAMPED because body B's anchor lies outside the viewport.
- `before_verdict = CLAMPED, after_verdict = CLAMPED` — 10 rows. A clamped
  surface is deliberately held inside the viewport, so its distance from an
  off-screen anchor is correct rather than stale; what moved is the bound, and
  every residual fell 60–75 % (`UI3D-F-006`).

## Screenshots

Paired `before_*` (from the audit, unchanged) and `after_*` (from the re-run).
The audit's mechanical overlay is on both: a green ring at the anchor native
reports, a red cross where the surface actually attached. In every `before_`
frame the two are one system-bar inset apart; in every `after_` frame they
coincide.

| pair | what it shows |
| --- | --- |
| `*_ui3d04_01_dimensions_open.png` | **F-003.** Before: the mode is open, the body is measurable, all three anchors project — and no number is drawn. After: all three stand on their leaders on the opening frame |
| `*_ui3d05_05_after_zoom_out.png` | **F-002.** Before: the body is a sliver near the centre while `2 m`, `1 m` and `0.5 m` sit far below and to the left. After: each number is on its own dimension line |
| `*_ui3d06_02_body_a.png` | **F-007.** Before: three numbers at body B's midpoints while the mode measures A (up to 282.53 dp). After: on the body being measured |
| `*_ui3d11_01_sculpt.png` | **F-004.** Before: the renderer has dropped the leaders while the shell still draws all three numbers, one on the sphere being sculpted. After: no Construction chrome in Sculpt |
| `*_ui3d10_01_committed.png` | **F-001 / F-006.** The retained `Edit Sketch` chip on a committed CAD Body, before and after the coordinate correction |
| `*_ui3d07_01_line_selected.png` | **F-005, still open.** Both frames show the selected Line's numeric chip and no extension lines, dimension line or ticks anywhere. The chip has moved onto the annotation; the annotation is still invisible |

## Tests

| file | role |
| --- | --- |
| `app/src/androidTest/.../Ui3dStateCorrectionTest.java` | the correction gate. 12 cases, `UI3DC1-02..11`. Asserts where the audit records; skips a clamped row using the audit's **own** `wouldClamp` predicate, and refuses to pass vacuously — every attachment assertion requires at least one measurable unclamped row |
| `app/src/androidTest/.../Ui3dStateAuditTest.java` | **unchanged.** Re-run strictly, with the original expectations |
| `app/src/androidTest/.../Ui3dAuditRecorder.java` | **unchanged.** Reused, including `centreOf`, `wouldClamp` and `placedAncestor` |
| `app/src/main/cpp/forgeshape_body_dimensions_selftest.cpp` | `DIM020M-17`, five new checks pinning the invariant the anchors-only read depends on: the anchors equal the drawn midpoints for **every** active axis, and the read fails closed on a non-positive camera scale, invalid bounds and a non-finite placement |

## Runs

| gate | result |
| --- | --- |
| `Ui3dStateCorrectionTest` | **OK (12 tests)**, 51.6 s |
| `Ui3dStateAuditTest` (strict re-run) | **OK (9 tests)**, 103.6 s |
| native self-tests on a clean debug launch | all 22 `*_SELFTEST_OK`, then `FORGESHAPE_NATIVE_VIEWPORT_OK`; body dimensions **102 checks** (was 97) |
| `gradlew :app:assembleDebug` | PASS |
| `gradlew :app:assembleRelease` | PASS |
| `scripts\verify-device-guards.ps1` | all PASS (`DEV2-01..07`, `DEV3-01..06`) |

No `-FullSharded` run was made and no aggregate marker is claimed.
