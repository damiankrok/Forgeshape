# CAD-R2 — TESTS

Raw commands and results, in the order run. Container: Linux x86_64, g++ 13
(host suites), OpenJDK 21 driving the Gradle 8.14.3 wrapper, a local Android
SDK with the pinned NDK `29.0.14206865`, `platforms;android-36`,
`build-tools;36.1.0`, `cmake;3.22.1` (installed in the container for this
stage; `local.properties` is git-ignored and never committed). CI runs on
GitHub-hosted `ubuntu-24.04` (JDK 17) as `docs/CI_CLOUD.md` describes.

## Phase 0 — baseline on `636797e`

| Gate | Command | Result |
| --- | --- | --- |
| Host | `bash scripts/host-native-selftests.sh` | `HOST_SELFTESTS_OK (4304 checks, 0 failed)`, 25 suites, 2 m 32 s (`host_baseline_suites.txt`) |
| JVM | `./gradlew :app:testDebugUnitTest` | 179 tests, 0 failures, 0 errors (first attempt: Maven Central HTTP 429 — environment, retried unchanged) |

## Host (candidate)

`bash scripts/host-native-selftests.sh` → **`HOST_SELFTESTS_OK (4369 checks, 0
failed)`**, the same 25 suites (`host_candidate_suites.txt`). CAD_FEATURE
561 → 626 checks; every other suite's count is unchanged. The 65 new checks,
all inside CAD_FEATURE (no new startup token):

| Group | File | Checks | Case ids |
| --- | --- | --- | --- |
| Sketch lifecycle | `forgeshape_cad_r2_selftest.cpp` | 18 | `R2_SK_01..18` |
| Staged edits / history | same | 6 | `R2_HIST_01..06` |
| Planar faces / regions | same | 14 | `R2_PF_01..14` |
| Supports | same | 6 | `R2_SUP_01..06` |
| Persistence | same | 4 | `R2_DATA_01..04` |
| Independent oracles | `forgeshape_cad_oracle_selftest.cpp` | 17 | `R2_OR_01..17`, line `CAD_R2_ORACLE_PASS=17` |

A failing check prints `CADR2_CHECK_FAILED <id>`; a failing oracle prints one
`CADR2_ORACLE_FAIL <id> expected=… measured=… repro=<inputs>` line, so a
failure is one reproducible input.

### The 17 oracles and their independent formulas

Measured from the published float mesh by code in the oracle file only
(divergence-theorem volume, triangle-area sum, bounds, position-welded and
by-index directed-edge closure, union-find components). Exact polygonal
tolerance: 1e-5 relative (float32 positions). Chordal tolerance: the mesh
never exceeds the analytic value and falls short by at most
`1 - sin(step)/step`, with `step` from the product's own segment count.

| Id | Shape | Expected (independent) |
| --- | --- | --- |
| OR-01 | box 2 × 1.5 × 0.7 | V = whd, A = 2(wh+wd+hd), bounds, closed, 1 shell |
| OR-02 | 3 × 2 with a 1 × 0.8 hole, d 0.5 | V = (WH−wh)d, A = 2(WH−wh) + 2(W+H)d + 2(w+h)d |
| OR-03 | two disjoint rectangles | V₁+V₂, 2 shells |
| OR-04 | Add boss 0.6² × 0.5 | V₁+V₂, A₁+A₂−2·footprint, bounds z ≤ 1.5 |
| OR-05 | + Cut pocket 0.6² × 0.4 | V − pocket, A + perimeter·depth |
| OR-06 | Symmetric 0.3/side | V = wh·2d, bounds −d..d |
| OR-07 | Two Sides A 0.5, B 0.25 | V = wh(A+B), bounds −B..A |
| OR-08 | One Side reversed | same V, bounds −d..0, positive (outward) |
| OR-09 | full Revolve, r 1..2, h 1 | polygon (N/2)sin(2π/N)(r₂²−r₁²)h exact; π(r₂²−r₁²)h chordal |
| OR-10 | 90° Revolve | (N/2)sin(θ/N)(r₂²−r₁²)h exact; θ/2(r₂²−r₁²)h chordal |
| OR-11 | circle r 0.8 extruded 1 | 32-gon (N/2)r²sin(2π/N) exact; πr² chordal |
| OR-12 | 4 × 3 grid minus a hole and a notch | V = 10·d, A = 2·10 + (14−1+3+4)·d |
| OR-13 | two cells touching at a node | 2 shells by index, each closed; exactly 1 welded edge shared in space |
| OR-14 | Add on the +x side face | V + 0.4²·0.3, max x = 1 + 0.3 |
| OR-15 | upstream width 2 → 3 under Add + Cut | V = 3·2·1 + 0.18 − 0.144, bounds |
| OR-16 | OR-05 under a rigid placement (rot 30/45/60, t (3,−2,0.5)) | V and A unchanged (1e-4 rel) |
| OR-17 | OR-05 encode → decode → regenerate | identical V, A, triangle count |

### Failures found and fixed on the host (each a reproducer first)

| Found | Kind | Fix |
| --- | --- | --- |
| `R2_SK_08` crashed / `HighWaterInvalid`: a sketch-only edit's candidate was built as a one-feature NEW body | **product** | `candidateState` resolves reuse and sketch-only sessions first |
| `R2_SUP_05`: a structural upstream edit (boss rectangle → circle) under a kept sketch was ACCEPTED by `CadBody::applyState` and would have committed a state the codec refuses to save | **product (latent, reachable with Keep Sketch)** | `applyState` and the candidate evaluation judge what regeneration does not (`validateCadStateBeyondRegeneration`) |
| `R2_SK_12` read the evaluation after `cancel()` reset it | harness | copy the evaluation |
| `R2_OR_09..11` sat exactly on the chordal bound | oracle contract | the polygon/curve ratio IS sin(step)/step; 1e-5 float slack |
| `R2_OR_12` expected a second hole where the cell touches the outline (a notch) | oracle input | expectation states the notch |
| `R2_OR_13` position-welded closure reports the vertical edge two touching shells share | oracle definition | closure and components measured by index; the shared welded edge asserted as exactly one |
| Device CAD_FEATURE record cap 512 < 561 checks: 49 checks were silently unrecorded on the device | **product (debug)** | cap 1024 |

## JVM

`./gradlew :app:testDebugUnitTest` → **196 tests, 0 failures, 0 errors**
(179 + `SketchBrowserPresentationTest` 17: `R2-JVM-01..17` — native order and
durable ids, usage, consumer copies, act lists per native bits, root/used
refusals kept, feature acts by id, selection reconcile by id, idempotent reads,
segments per body kind, malformed reads, strip wrap at 236/100/30 dp, the
48 dp floor over widths 48..900 dp, marks, appended status codes, slot layout).

## Builds and release guard (local)

`./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease`
→ BUILD SUCCESSFUL. `ANDROID_HOME=/root/android-sdk bash
scripts/ci-release-selftest-guard.sh` → `RELEASE_SELFTEST_GUARD=PASS`
(release x86_64 / arm64-v8a: 0 `SelfTests` symbols, 0 self-test strings;
debug control: 35 symbols, 398 strings on both ABIs).

## Device (CI DEVICE, `CadRetainedSketchOwnerTest`, real window touches)

| Run | Head | Result | Cause | Remediation |
| --- | --- | --- | --- | --- |
| 40675745459 | `311b749` | 7/12 | **product** D12: in landscape the second line of acts measured 34 px (< 48 dp); harness D04/D06: a point at x < 0 seen from +x is behind the chooser's translucent YZ plane (correct product behaviour); harness D08: pocket drawn at the sketch origin; harness D09: lower-left cell under chrome | acts above the list (`5127cfc`); camera from −x; pocket at (0.6, 0.6); real two-finger pan before a blocked cell |
| 40678573702 | `5127cfc` | 10/12 | harness D04: the curved-wall pixel lay on y = 0, where the XZ plane quad (correctly) takes the tap; harness D06: second sketch at y < 0 behind the XZ quad | wall point 30° off the axis; second sketch at (0.75, 0.3) |
| **40680946023** | `f706e92` | **PASS 13/13** (D01..D13) | | |

Startup on every run: 25/25 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure
lines. Evidence artifacts: `ci-device-evidence` 12814405176 (run 40680946023).

## Final DEVICE union (bounded; split in two by the workflow's 2700 s cap)

| Part | Run | Head | Classes | Result | Evidence |
| --- | --- | --- | --- | --- | --- |
| A | **40683528533** | `f706e92` | `OwnerProjectShellSketchSupportTest`, `CadMultiFaceOwnerTest`, `CadFillPickR2Test`, `CadHud3dOwnerTest`, `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`, `CadParametricHistoryOwnerTest`, `HomeFlowTest`, `EditorWorkspaceProjectActionsTest` | **OK (67 tests)**, 0 failed | `ci-device-evidence` 12815577156 |
| B | **40689211847** | `7d2d7b4` (docs only after `1b7507e`, test only after `f706e92`) | `CadRetainedSketchOwnerTest` (D01..D14), `EditorWorkspaceCorrectionTest` | **OK (51 tests)**, 0 failed, 1265 s | `ci-device-evidence` 12818690455 |

Union: **118 / 118**, both parts 25/25 startup tokens in order,
`NATIVE_VIEWPORT_OK`, 0 failure lines, `FORGESHAPE_CI_DEVICE_RESULT=PASS`.
The product native/Java code is identical in both heads (`5127cfc` + the
`5dd7d97` Java constant). D14 facts: `d14.preview=[4.0, 1.0]` (Select All →
four regions, one valid candidate), cleared capture `d14_cleared.png`.
