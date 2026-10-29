# FUNCTION-COUNCIL-R2 — Risk register

Audited tree `70a6c14f703bfda2ab689b855126c1d12a541e41` (`app/src/main` =
`f3db953`). One row per root cause; member finding ids are the seats'
(`SEAT_A.md` … `SEAT_J.md`, post-Round-2) and the root-cause ids are
`COUNCIL_DELIBERATION.md` §4. Architecture mechanisms behind these rows:
`ARCHITECTURE_HEALTH.md`. Old Council items: `COUNCIL_DEBT_RECHECK.md`. Fresh
run ids: `TESTS.md` (coordinator; not present at write time) — run ids quoted
here are the ones the seats and `evidence/` record.

**Verification status.** `work/VERIFICATION.md` did **not exist** when this
register was written. "Standing" below is the seat verdict after Round 2, not
an adversarial verdict. A row whose finding `VERIFICATION.md` does not uphold
must be relabelled hypothesis/withdrawn. Items already withdrawn or regraded by
the deliberation (B-07, the C-03 Keep Sketch sub-claim, the J-05 revolve-axis
half, J-01/J-03 as defects, G-06 as a defect) are not risks here except where
they survive as an OWNER hold (§7).

## Scales (stated so every rating has a basis)

- **Likelihood.** **H** — reachable through an ordinary visible control in a
  normal session. **M** — reachable through a specific but ordinary sequence or
  configuration (a locale, a recreation, a crash then Recover). **L** — needs a
  rare condition (a race window, a size threshold, a crafted file, a specific
  ABI). Reachability that no seat reproduced is marked "(E5)".
- **Impact.** **H** — user work lost, or a saved/exported file silently wrong.
  **M** — wrong state or visible wrong behaviour the user can recover from
  (Undo, retry, re-enter), or a lost control. **L** — cosmetic, wording,
  evidence quality, documentation.
- **Owner** names an area, not a person: **OWNER** (product decision),
  **Native** (C++ domain/JNI), **Shell** (Android Java), **Test/CI**,
  **Docs**. A row may need an OWNER decision before an engineering owner can act.
- **Grades** E1–E5 as defined in the audit brief; emulator runs are E1 for the
  emulator only and are never a physical-device PASS.

## 1. Data loss and data integrity

| R | Risk (members) | Standing | L | I | Basis for L / I | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-01 | Recover retires the checkpoint and marks the project persisted, so recovered work is in neither the slot nor a checkpoint; leaving afterwards does not ask (A-02; `EditorWorkspaceView.java:5652-5687`) | DEFECT-CONFIRMED P1 per seats A/D; the shipped text `PRODUCT.md:193-194` and pinned assertion FSR1B-05 (`ProjectAutosaveRecoveryTest.java:321-322`) say the opposite → **BLOCKED-OWNER** (decision 2, disagreement D-2) | M | H | L: needs process death with unsaved edits, then Recover (E5 for frequency). I: the recovered edits exist only in memory | E2 | OWNER, then Shell | Decide Recover semantics; then keep the checkpoint until the next Save, or mark the recovered project dirty; add a test that leaves after Recover | OWNER decision 2; any change to `offerRecoveryIfPresent` / FSR1B-05 |
| R-02 | "Save and continue" before Open Saved Project overwrites the slot it then opens (A-01; `:2665-2678`, `:5458-5471`) | DEFECT-CONFIRMED P2 | M | H | L: dirty project + Open Saved Project + Save. I: the previously saved project is replaced by the live one | E2 | OWNER (decision 3: offer Save there at all), Shell | Withdraw Save from that question or route it to Save Copy; device test | OWNER decision 3 |
| R-03 | GLB export writes a face-supported body at its stored identity transform instead of the derived placement (E-01 = B-02; `forgeshape_gltf_export.cpp:178-180`) | DEFECT-CONFIRMED P1 | M | H | L: any project with a sketch on a body face, then Export. I: the exported file is geometrically wrong with no warning; fixtures `cad_face_sketch_cap_v2`, `_side_v2`, `cad_face_chain_v2` reproduce it | E1 (host) + E2 | Native | Export through `resolveWorldModel`; refuse an unresolved support; extend GLTF_EXPORT checks to dependents (TST-060) | fix lands; any new placement consumer |
| R-04 | Checkpoint/slot writers are not coordinated: fingerprint and bytes under two lock holds (E-13 = H-02); a checkpoint landing after Save cleared it (E-14); Save fallback leaving the only copy in an unread `.pending` (J-04); static unlocked `ProjectCheckpoint.write` with a delete-then-rename fallback across two controllers (HYP-E6); read racing a rename and quarantining a valid file (HYP-E7) | E-13, E-14, J-04 DEFECT-CONFIRMED P3; HYP-E6/E7 hypotheses | L | H | L: each needs a race window or a failed rename (E5). I: E-14 is a redundant prompt; HYP-E6 leaves no checkpoint; J-04 leaves the only copy unread | E2 (mechanism), E5 (reach) | Shell | One writer object per file with one lock; no delete before rename; fingerprint and bytes in one native call; journey J-E6 (`SEAT_E.md`) | a recreation-under-write test exists; any autosave change |
| R-05 | A checkpoint can capture an uncommitted mid-drag transform (`encodeProject` does not check `editInProgress`, `forgeshape_jni.cpp:8387-8410`) — gizmo and Freeform drags only (HJ-1) | hypothesis | L | M | L: finger still > 900 ms mid-drag, then process death. I: a placement the user cancelled is offered on recovery | E2 (guard absent), E5 (consequence) | Native | Skip or defer a checkpoint while an edit is open | process-death test with a held drag |
| R-06 | Rename not followed by a parent-directory fsync in `ProjectSlot`/`ProjectCheckpoint` (HJ-2 = H-E3 = H-G-03) | hypothesis | L | M | L: power loss right after a rename. I: the previous file survives (durability, not corruption) | E5 | Shell | fsync the directory after rename | a storage-failure test harness exists |
| R-07 | A project can be saved but not reopened: writers have no byte ceiling, readers refuse > 256 MiB (H-E2) | hypothesis (asymmetry proven) | L | H | L: needs a very large imported or sculpted project; Java heap may stop the encode first (E5). I: saved work cannot be opened | E2 asymmetry, E5 reach | OWNER (limit), Shell | One stated limit applied at write and read | OWNER sets a size policy |
| R-08 | Session memory lost on Activity recreation: saved baseline forgotten (A-05 = E-04 = FC-R1 D3); recovery question reappears over the live project (E-02); empty Save Copy/Export after recreation (A-06). Triggers include a palette change and every config change absent from `AndroidManifest.xml:31` (`uiMode`, `fontScale`, `locale`, `keyboard`, `navigation`) | DEFECT-CONFIRMED (E-02 P2; A-05 P3/P2 per disagreement D-3; A-06 P3) | M | M | L: palette change or a listed config change. I: extra or wrong questions; A-06 an empty file (E2+E5) | E2 (+E3 mechanism `fsr1b04`) | Shell | Carry the baseline through `EditorUiState`; set `recoveryResolved` on every path | fix lands; manifest `configChanges` edited |
| R-09 | Codec admits a document that decodes and validates but the load refuses (E-05, PlanarFaces body) and two encodings of one face token (E-06) | DEFECT-CONFIRMED P3 | L | M | L: crafted or edge files. I: "validate" says yes, open says no; one solid has two byte forms | E1 (`pf_gap`, `faceidx`) | Native, Docs (`DATA_PACKAGE_SPEC.md`) | Run the load's checks in validate; canonicalise the token | next `CADB` version or codec change |
| R-10 | Project establishment resets an incomplete, mis-ordered list: Freeform session and selection survive a load (E-07 = D-03 = APP-087); a refused load has already cancelled the sketch and moved the camera (E-12); support chooser not cancelled by load; Dimensions/gizmo after load undetermined (U-2); `closeProject` vs load asymmetry (E-10 = FC-R1 D7) | E-07, E-12 DEFECT-CONFIRMED P3; E-10 PARTIAL (disagreement D-8); U-2 UNDETERMINED-SOURCE-GAP | M | M | L: Open/New while a Freeform selection or chooser is up. I: stale selection on a new cage (`diag_d2`: `selection=[6]`) | E1 + E2 | Native | One reset-every-session function called by load and close, after decode succeeds | fix lands; any new process session added |
| R-11 | Undo after an over-cap (`NotRetained`) stroke lands on a state that never existed and clears `hasEdits` (D-01 = FC-R1 D4; `forgeshape_sculpt_history.cpp:91-97`) | DEFECT-CONFIRMED P2 | L | M | L: a stroke whose delta exceeds the 1 MiB per-entry cap. I: geometry the user never saw; edited flag wrong | E1 (`diag_d3`) | Native | Clear `undo_` (or mark a barrier) when a stroke is not retained; rename `SCUNDO_23` | fix lands |
| R-12 | A load can reopen a hidden sculpted body into Sculpt (E-09 = D-04) | recorded debt, PARTIAL | L | L | L: crafted file only. I: Sculpt over a hidden body | E1 (`hidden_sculpt`) | Native | Refuse or un-hide on load | `PROJECT_STATUS.md:4263-4269` item closed |

## 2. Product behaviour and UX

| R | Risk (members) | Standing | L | I | Basis | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-13 | A face-supported CAD body accepts a transform write and shows a gizmo: Undo step recorded, fingerprint changes, drawn model unchanged (B-01; RC-01) | DEFECT-CONFIRMED P2 | M | M | L: select a face-sketched body in Transform. I: an invisible edit that dirties the project; contradicts `CLAUDE.md` `CAD-A3` | E1 (host, re-run by seat I) + E2 | Native, Shell | Refuse below JNI and withdraw the gizmo; test TST-076 | fix lands |
| R-14 | Import GLB is accepted while a sketch is open (bodies 1→2, active body switches, sketch stays open) (RC-02: A-04, C-02, D-02, E-03); Resume Sculpt lacks a native sketch guard (D-07) | DEFECT-CONFIRMED P2 (D-07 P3) | M | M | L: the drawer's Import row is reachable in an in-project sketch. I: a commit target may change under the sketch (H-E5, E5) | E1 (A, D diags) + E2 | Native, Shell | Add sketch/chooser terms to the import guard and the chrome predicate | fix lands |
| R-15 | Sketch navigator Flip / ±90° change state but not the camera (RC-03: C-01 = F-02 = FC-R1 D2) | DEFECT-CONFIRMED P2; post-fix Back-to-Sketch view is an OWNER question (D-6) | H | L | L: every use of those controls. I: a control that does nothing visible | E1 (domain probe) + E2 | Native; OWNER for D-6 | Frame on `viewFrame()`; make `SketchUxTest` assert the camera | fix lands; OWNER answers D-6 |
| R-16 | Back routing not innermost-first: in the CAD bootstrap Back discards the sketch with a panel open (F-03); Back during an in-project Revolve backgrounds the app on API 36 (F-04 = CAD-115); in-project modes (A-10) | F-03, F-04 DEFECT-CONFIRMED P2 (E2, not reproduced); F-03 conflicts with `PRODUCT.md:196-198` → OWNER (D-5) | M | M | L: Back is the commonest dismissal. I: F-03 loses the bootstrap sketch; F-04 leaves the app | E2 | Shell; OWNER for F-03/A-10 | Add the revolve term; reorder the bootstrap branch after OWNER decides; platform-route Back tests beyond `uilr106` | OWNER decision 5 / D-5 |
| R-17 | Decimal comma dropped by three key listeners: "2,5" applies 25 (RC-09: F-05, ADV-067, CAD-025) | DEFECT-CONFIRMED P2 | M | M | L: comma-decimal locales or keyboards. I: a ×10 dimension, visible and undoable | E2 (defect), E1 (parser) | Shell | Accept the locale separator in the three listeners | fix lands |
| R-18 | Controls drawn and then refused (RC-10: D-05 Clear Mask over cap, D-08 Symmetry/level over budget, C-03 Add/Cut on a full 16-sketch table) | D-05, D-08 PARTIAL P3; C-03 DEFECT-CONFIRMED P3 | L | L | L: thresholds (~87k-vertex mask, 16 sketches). I: a refusal message instead of an absent control | E2 | Native, Shell | Availability predicates ask the same refusal | fix lands |
| R-19 | A renderer that stops mid-session is not reported until the next resume (H-01; only `onResume` polls, `ForgeShapeActivity.java:336`) | DEFECT-CONFIRMED P2 | L | M | L: device loss beyond the rebuild budget. I: a frozen or black viewport with no message until backgrounding | E2 | Shell | Poll or post the lifecycle on change | fix lands; renderer-recovery work |
| R-20 | Triangles with twice-area ≤ 1e-8 m² cannot be picked (H-10, absolute epsilon `forgeshape_picking.cpp:225-231`) | observation; requirement BLOCKED-OWNER | L | L | L: sub-0.1 mm faces (mm-scale models). I: taps refused on small visible faces | E1 (host measurement) | OWNER (requirement), Native | Relative epsilon | OWNER decision 4 (seat H) |
| R-22 | Unit policy: Freeform/Surface length fields seeded unitless (J-02, DEFECT-CONFIRMED P3); tap slop in two unit systems (G-05, DOC-DIVERGENCE) | as stated | M | L | L: any Freeform/Surface length edit in mm/cm display. I: a wrong default text, a differing "tap" per tool | E2 | Shell; OWNER for G-05 | Seed from `LengthUnit`; one stated slop unit | OWNER unit-policy decision |
| R-23 | Several chrome controls are below 48 dp in width (F-01; `EditorControlStyles.java:366-382`, e.g. `sketch_navigator_flip` 36.2 × 48 dp) | DEFECT-CONFIRMED P2 | H | M | L: always present. I: mis-taps on adjacent controls | E1 (emulator rect) + E2 | Shell, Test/CI (no 48 dp sweep, F-09 = I-08) | `setMinimumWidth` in `chip()`; a sweep test | fix lands |
| R-24 | Accessibility: screen-reader selected state undecided (F-07); focus ring contrast 2.48:1 (F-08); TalkBack, tablet, foldable, mouse, stylus unverified | F-07 BLOCKED-OWNER; F-08 P3; others OWNER-PHYSICAL-UNVERIFIED | M | M | L: any TalkBack user. I: state not announced; weak focus ring | E2 / E5 | OWNER, Shell | Decide F-07; raise ring contrast; a TalkBack pass on a device | OWNER decision; first physical accessibility session |
| R-25 | The saved slot cannot be reopened from Home (A-03, `HomeView.java:47-79`) | BLOCKED-OWNER (APP-H1 "exactly two Home actions") | H | M | L: every cold start with a saved project. I: the user must open a new project and then use the drawer | E2 | OWNER | Decision 1 | OWNER decision 1 |

## 3. Performance and resource use (all release-ARM figures OWNER-PHYSICAL-UNVERIFIED)

No repo record states a UI-thread, render-thread, frame or battery budget, so
every requirement here is **BLOCKED-OWNER** (`delib_A_to_H.md` O-2..O-5, O-9;
disagreement D-7). The risks stand on measurements.

| R | Risk | L | I | Basis | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-26 | CAD derivation under `g_stateMutex` on the UI thread (`sketchFinish`, `forgeshape_jni.cpp:4400-4420`) or the render thread (candidate, `:1733`): host x86 `finish()` 275–322 ms at 63×63, `evaluateCandidate` all faces 25.5–25.7 ms, overlay rebuild 4.2–4.5 ms per frame; emulator `finish_us` (125 placements + Finish) 4.2 s in `40797031704` (H-05) | M | M | L: large sketches. I: frozen viewport; ANR risk at release-ARM speed unknown | E1 (host, emulator) + E2 | OWNER (budget), Native | Off-thread derivation with a "deriving" state, or a measured budget and refusal | OWNER sets a budget; first physical measurement |
| R-27 | Sculpt: whole-mesh publish per sample and full render-mesh rebuild + upload + GPU drain per frame during a stroke (H-06): host 20 ms smooth refresh at 100k vertices; S25 render build 104 ms (E3) | M | M | L: any stroke on ≥100k vertices. I: stroke frame time plausibly ≥100 ms (E5) | E1 host + E3 | OWNER, Native | Incremental normals over the affected set, dirty-range upload | OWNER budget; physical stroke measurement |
| R-28 | Render loop never idles while a surface is attached (H-07 = FC-R1 I-5; `forgeshape_jni.cpp:1822-1826`) | H | M | L: always. I: battery/thermal unmeasured | E2 | OWNER, Native | Idle until an input, revision or animation | OWNER decision on frame pacing |
| R-29 | Face-supported bodies re-derive their face frame every frame (H-08 = F-07; 4.35 ms at depth 32, DEEP-AUDIT-R1) | L | L | L: deep face chains. I: frame cost | E2 + E3 | Native | Cache keyed by producer state | recorded debt closed |
| R-30 | Save and autosave encode under `g_stateMutex` (`forgeshape_jni.cpp:8391`); Save also fsyncs on the UI thread (H-E4) | L | M | L: large projects (E5). I: UI and viewport freeze for the encode | E2, E5 | Native, Shell | Snapshot under the lock, encode outside it | OWNER budget; large-project test |
| R-31 | Cold start rebuilds an equal-extent swapchain once (~0.5 s on SwiftShader, H-14); debug start blocks the UI thread ~8.4 s on self-tests holding `g_viewport.mutex` (H-15, debug only) | H | L | L: every start. I: first-frame latency; debug-only blocking | E1 (`40797031704`) | Native | Skip equal-extent rebuild | renderer lifecycle change |

## 4. Test, evidence and CI

| R | Risk | Standing | L | I | Basis | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-32 | Device self-test channel is lossy: results past a suite's cap are silently dropped and PROJECT is at 256/256 (I-01 = H-13); the CAD_FEATURE performance line is cut at 1023 chars (`forgeshape_jni.cpp:608`, H-03 = I-07); no step compares per-suite counts (I-14) | I-01 PARTIAL (latent, no current loss); H-03 DEFECT-CONFIRMED P3; I-14 PARTIAL | H | L | L: the next PROJECT check added. I: a check that never runs on device while CI stays green | E1 (`cap_probe.cpp`, `40797031704`) | Test/CI, Native | Raise caps; fail on `count == cap`; split long lines; committed count table | any self-test added |
| R-33 | Two oracles fail under FP contraction (arm64 default) and self-tests read uninitialised floats (G-01, G-04) | G-01 DEFECT-CONFIRMED P2; G-04 DEFECT-CONFIRMED P3 | M | L | L: every arm64 debug launch for the history check (E2 asm); lens check E5. I: expected FAIL lines can hide a real one; product geometry not shown wrong | E1 host + E2 | Native, Test/CI; OWNER for `-ffp-contract` | Runtime-computed oracles or tolerance; non-tied lens fixture; CI host run with `-mfma` | OWNER decision on `-ffp-contract`; first arm64 log |
| R-34 | CI FAST runs neither the host suites nor the corpus-verdict diff; domain platform-neutrality is unguarded (I-05 = G-02) | PARTIAL P2 | M | M | L: any domain change. I: a native regression or an Android include in the domain reaches CI only via a dispatched device run, behind R-32 | E2 | Test/CI | One FAST step running `host-native-selftests.sh` and the corpus verdicts | workflow change |
| R-35 | A task-branch push triggers no CI (`ci-fast.yml:12-17`), contrary to `CLAUDE.md` `CI-CLOUD-R1` and `docs/CI_CLOUD.md` (I-03) | DOC-DIVERGENCE P2 | H | M | L: every task branch. I: green status only when someone dispatches | E2 | OWNER (trigger policy), Test/CI | Add a branch pattern or correct the docs | OWNER decision |
| R-36 | Device evidence is older than the tree: 28 of 70 classes have an E1 emulator run in this audit, 11 a C2 run on the same product tree (E3), 31 rest on older trees; no `FULL_SHARDED_SUITE_PASS` on this tree (last `36607747079` on `fa0b6b4`); Tier-5 gate before merge to `main` pending (I-06, C-06, RC-14) | PARTIAL P2 (coverage, not product) | H | M | L: true today. I: a regression in the 31 classes would be unseen | E1/E3 (`FEATURE_INVENTORY.md` §0) | Test/CI, OWNER (Tier-5 gate) | Run the full sharded aggregate on the merge candidate | before any merge to `main` |
| R-37 | Most device touches bypass hit testing: `performClick()` 482 call sites; viewport-direct helpers 170; decor-root hit-checked input in 8 classes (~106 sites) (I-08 = F-09) | PARTIAL P2 | H | M | L: structural. I: chrome standing over a target (the F-01 class, chooser occlusion) is invisible to tests | E2 | Test/CI | Route helpers through the decor root and assert the hit target | harness change (Tier-5 trigger) |
| R-38 | The 25 suites re-run on every Activity start (~9.5 s each, 304 s of 667 s test time in `40797031704`); a cold start with pending input could ANR in debug (I-02; H-I-2) | PARTIAL + DOC-DIVERGENCE P2; H-I-2 hypothesis | H | L | L: every instrumented test. I: CI time, timeouts; debug-only | E1 (emulator) / E5 | Test/CI, Native | Run once per process | CI time budget hit |
| R-39 | A green `summary.json` does not carry the audit ledger's verdicts (I-13); an assumption-skipped test may count as passed (H-I-1) | I-13 PARTIAL; H-I-1 hypothesis | L | L | L: ledger-only failures. I: false green | E1 (ledger clean in `40797031704`) / E5 | Test/CI | Copy ledger counts into the summary; count skips | CI script change |
| R-40 | The release APK is never executed by any tier; 13 `debug*` JNI exports with `NDEBUG` stubs ship in the release `.so` (I-10) | IMPLEMENTED-UNTESTED | L | M | L: release-only behaviour differences. I: unseen until a user runs release | E1 (symbols) / E2 | Test/CI; OWNER (signing) | A release launch smoke as a dispatch input | release preparation |
| R-41 | Tests that pass while the behaviour they name is wrong: `SketchUxTest` navigator asserts state bits only (F-02, `:199-233`); `SCUNDO_23` asserts NotRetained only on an empty stack and calls an applied stroke "refused" (D-01); no test for the face-supported refusal (TST-076) or export bake of dependents (TST-060) | as members | H | M | L: the tests exist and are green. I: false assurance on R-03, R-11, R-13, R-15 | E2 | Test/CI | Assert the effect (camera pose, geometry, refusal) | with each fix |
| R-42 | Release-guard local provenance incomplete: first local run `guard_exit=2` (environment error), a later file reads PASS with no recorded command/exit (U-1, disagreement D-1) | UNDETERMINED for the local file; CI FAST `40796992679` PASS | L | L | I: audit-evidence quality only | E1 (CI) | coordinator (`TESTS.md`) | Record the rerun command and exit | `TESTS.md` written |
| R-43 | The adversarial verification file `work/VERIFICATION.md` was absent at synthesis; every defect here is seat-graded | audit-process risk | M | M | I: a finding that would not survive verification is presented as a seat-confirmed defect | E4 (file listing) | coordinator | Reconcile this register with `VERIFICATION.md` | `VERIFICATION.md` written |
| R-44 | Plan-source gap: Master v4.1.61, `OWNER_DECISIONS_*`, `MASTER_PLAN_HISTORY_*`, `PROJECT_STATE` unavailable, so H0/H1/H2 are reconstructed (PARTIAL-PLAN-SOURCE-GAP) | audit-process risk | M | M | I: an item may be mis-horizoned; a Master MVP item never recorded in the repo cannot appear | E4 (`BEFORE.md`) | OWNER | Supply the canonical plan; re-horizon `requirements_traceability.tsv` | canonical sources attached |
| R-45 | Consistency checks exist only as comments or off-CI scripts: shader ↔ C++ constants (H-17); `lock_context.js` for the `sculptSession()`-under-lock rule is not in any workflow | IMPLEMENTED-UNTESTED | L | M | L: a constant or lock-site edit. I: silent drift (outline/grid) or an unlocked read on the autosave thread | E2 | Test/CI | Add both to CI FAST | workflow change |

## 5. Physical-device and hardware (OWNER-PHYSICAL-UNVERIFIED)

`CLAUDE.md` hard rule: emulator CI closes no physical-device gate.

| R | Risk | L | I | Basis | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-46 | Stylus, pressure, hover, hardware GPU (non-SwiftShader Vulkan), 16 KB-page runtime, thermal and real-device performance are unverified on this tree. The 16 KB `LOAD` alignment of the libs is proven (E1, `readelf`); its runtime on a 16 KB-page device is not | M | M | L: the product's primary platform is physical phones. I: device-only faults reach users | E1 (alignment) / E5 | OWNER | A physical session on the merge candidate | OWNER schedules a device session |
| R-47 | A debug launch on arm64 prints `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_FAIL` (H-G-01a, asm E2 + host E1) and possibly `CAD_FEATURE` (H-G-01b, E5); `PROJECT_STATUS.md:4158-4163` records that the physical phone drops whole suites from logcat | H (if R-33 holds) | L | I: evidence pollution, product unaffected (`forgeshape_jni.cpp:526-534` only logs) | E2/E5 | Native, OWNER | Fix R-33; capture with `logcat -G 64M` | first arm64 debug capture |
| R-48 | Attaching/detaching a hardware keyboard recreates the Activity because `configChanges` lacks `keyboard`/`navigation` (H-G-02), feeding R-08 | L | M | L: keyboard use. I: R-08 consequences, surface rebuild | E2 (manifest), E5 (behaviour) | Shell | Add the config changes or fix R-08 | device test with a keyboard |

## 6. Portability and structure (H2 unless stated)

Desktop work is H2; nothing here is missing functionality. Detail:
`PORTABILITY_MATRIX.md`, `DESKTOP_PORT_STRATEGIES.md`, `DESKTOP_POC_GATE.md`.

| R | Risk | Standing | L | I | Grade | Owner | Mitigation | Revisit when |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| R-49 | A second shell would have to extract gesture arbitration from the JNI TU (G-07), replace one-document-per-process singletons and one global density (G-08), add mouse pan/zoom (G-09 = F-10), settle px vs density units (G-05), and the allocator's high-water mark leaks into re-encoded bytes across loads (G-06 = J-03, OD-J3) | FUTURE-NOT-DUE; OD-J3 BLOCKED-OWNER | L (no desktop target authorised) | M if a port starts | E1 (probes) / E2 | OWNER | `DESKTOP_POC_GATE.md` | OWNER authorises a desktop stage |
| R-50 | Maintainability: `forgeshape_jni.cpp` 10,337 lines (+28 % since FC-R1) and `EditorWorkspaceView.java` 6,655 (+22 %) concentrate the restated guards behind RC-02/04/07/08; 8 orphan natives and 2 dead helpers remain (J-05) | debt | M | M | E2 (sizes, orphans) + E5 (consequence) | Native, Shell | Extract the reset function, the guard set and Back routing as single owners when those defects are fixed; remove orphans | each of R-10, R-14, R-16, R-08 fixed |
| R-51 | Documentation drift: `ARCHITECTURE.md:214` window ownership (G-03), threading `:4475-4476` and `:3078-3083` omit render-thread derivation, "blocks until" vs 5 s (H-11), "`ConstructionObject`" in the lifecycle contract, `:2606-2608` face-supported guard (B-03), unnamed codec modules; `PROJECT_STATUS.md` lists FC-R1 D5 open and stale counts in six places (I-04, H-04); stale code comments (C-05, H-16); `CONTRACT_CONFLICTS.md` (cited by the deliberation) absent at write time | DOC-DIVERGENCE | H | L | E2 | Docs | One documentation task (not product backlog) | next owner-doc update |

## 7. OWNER holds (decisions that gate a risk above)

Collected from `COUNCIL_DELIBERATION.md` §6 and §5; none is resolved here.

| Decision | Gates | Source |
| --- | --- | --- |
| Recover semantics vs FSR1B-05 / `PRODUCT.md:193-194` | R-01 | A-02, D-2 |
| Home access to the saved slot | R-25 | A-03 |
| Save offered before Open Saved Project | R-02 | A-01 |
| An open sketch / staged edit as unsaved work when leaving | (A-07, P3/INFO) | A-07 |
| Back inside in-project modes and in the CAD bootstrap | R-16 | A-10, F-03, D-5 |
| Protection after Discard then a cancelled/refused Open | (E-08, PARTIAL) | E-08 |
| Navigator view after Back to Sketch / Edit Sketch once fixed | R-15 | C-01, D-6 |
| Kept sketches sharing the 16-slot table | R-18 | C-03, D-10 |
| Hidden-body editing; lock on Freeform (`BodyLocked`) and on face-supported dependents | (J-01, B-04, B-06, D-09, HJ-3; RC-17) | OD-J1, OD-J2 |
| Allocator in the fingerprint and bytes | R-49 | OD-J3 = G-06 |
| Tap-slop unit policy | R-22 | G-05 |
| `-ffp-contract` pin | R-33 | G-01 |
| CI trigger for task branches | R-35 | I-03 |
| Frame-pacing and derivation budgets | R-26..R-30 | H-05, H-07, D-7 |
| Screen-reader selected state | R-24 | F-07 |
| Sculpt status-line layout (overlapped by the brush column and Tool Rail) | (F-06, P3) | F-06 |
| Write/read size limit | R-07 | H-E2 |
| Pickability requirement for tiny faces | R-20 | H-10 |

## 8. Residual P3 / INFO items not given their own row

Each is a single, low-impact finding (L likelihood or L impact) recorded by its
seat; listed so the register is complete.

| Item | Summary | Standing | Grade |
| --- | --- | --- | --- |
| A-08 | Wrong failure text when the picker cannot launch for Open | P3 | E2 |
| A-09, A-12 (= D-06) | Documentation divergences in the shell flows | DOC-DIVERGENCE | E2 |
| A-11 | New Freeform refusal shows a raw token, bypassing `FreeformPresentation.refusalMessage` | P3 | E2 |
| A-13 | A stale recovery candidate can survive a clean leave | PARTIAL P3 | E2 |
| B-04 | Lock guard holds only at gizmo activation; Java refresh mitigates | PARTIAL P3 | E2 |
| B-05 | Derived names at the 96-byte cap truncated mid-suffix or dropped | P3 | E2 |
| C-04 | Out-of-range typed line length refused under the name `ZeroLengthLine` | PARTIAL P3 | E2 |
| F-11 | Settings page Back control reachable only after scrolling | INFO | E2 |
| F-12 | The status line carries instructions (`CLAUDE.md` "Chrome reports") | P3 | E2 |
| I-09 | Fixed sleeps in device tests (`settleLayout` 884 sites, 49 raw sleeps) | PARTIAL | E2 |
| I-11 | `EditorWorkspaceLegibilityTest` leaks `animator_duration_scale 0` on failure | DEFECT-CONFIRMED (test hygiene) | E2 |
| HYP-H1, HYP-H3, HYP-H4, HYP-H2 (visible part) | Overlay revision collision; mask depth-attachment hazard; picking cost on large imports; RestartRequired on a later attach | hypotheses | E5 |
