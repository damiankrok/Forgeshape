# FUNCTION-COUNCIL-R2 — Source register (Seat A)

Audited tree: `70a6c14f703bfda2ab689b855126c1d12a541e41` (C2 head; `app/src/main`
byte-identical to `f3db953`). Branch `audit/function-council-r2-full-product-ux-platform-r1`.
Line counts are `wc -l` on this checkout; "last touched" is `git log -1 --format='%h %cs' -- <file>`
run during this audit (2026-10-29).

## 1. Authoritative repository documents

| Source | Lines | Last touched | Owns (per `CLAUDE.md` "Documentation ownership") | Used by Seat A for |
| --- | ---: | --- | --- | --- |
| `CLAUDE.md` | 1822 | `911f5c3` 2026-10-28 | durable rules, rule IDs, UI vocabulary | APP-H1, `MODELING-R1-OWNER-CORRECTION` (mark/drawer), `UI-PREF-R1` (Settings), autosave / recovery / transfer / export / import rules, "control that cannot succeed is not drawn", "Chrome reports, it does not instruct", CAD sketch refusal list |
| `PROJECT_STATUS.md` | 5061 | `70a6c14` 2026-10-28 | status, Owner Decision Baseline (§ line 2486), Known Issues (§4018), Technical Debt (§4216), Next Stage (§4851) | horizon reconstruction (UI-OWNER-01..06/32/37/42/45..48, ARCH-OWNER-07..14), recorded test runs, FUNCTION-COUNCIL-R1 debt status |
| `PRODUCT.md` | 2348 | `9144169` 2026-10-28 | runtime-verified user-visible behaviour | "Starting a project" (§ lines 131-202), "System Back" (§299), "Saving and reopening" (§1988), "When ForgeShape closes before you saved" (§2019), "Moving a project on and off the device" (§2043), "Sending the model to another program" (§2058) |
| `ARCHITECTURE.md` | 5588 | `9144169` 2026-10-28 | production architecture | System Back (§236), unsaved-changes question (§1148) — spot-checked only |
| `README.md` | 840 | `7e02240` 2026-10-06 | tooling / build / verify | host self-test and corpus commands |
| `DATA_PACKAGE_SPEC.md` | 1852 | `94a21dd` 2026-10-06 | `.forge` layout | not used directly (format is Seat-owned elsewhere); cited only for "failed load changes nothing" |
| `docs/CI_CLOUD.md` | 177 | `3e2443f` 2026-10-05 | CI FAST / DEVICE / FULL SHARDED | evidence-run interpretation |
| `docs/ui/wireframes/WF-1-sculpt-phone-portrait.svg` | (svg) | `fd88db8` 2026-09-27 | wireframe | shell layout reference only |
| `docs/ui/wireframes/WF-2-construction-phone-portrait.svg` | (svg) | `fd88db8` 2026-09-27 | wireframe | shell layout reference only |
| `docs/ui/wireframes/WF-3-sketch-extrude.svg` | (svg) | `fd88db8` 2026-09-27 | wireframe | sketch journey reference only |
| `docs/ui/wireframes/WF-4-tablet-landscape.svg` | (svg) | `fd88db8` 2026-09-27 | wireframe | tablet shell reference only |

## 2. Stage dossiers and prior audits consulted

| Dossier | Lines | Last touched | Used for | Status in this audit |
| --- | ---: | --- | --- | --- |
| `artifacts/function-council-r2/BEFORE.md` | 93 | `9cfc3f5` 2026-10-29 | snapshot, inventory, source gaps | authoritative for this audit's preflight |
| `artifacts/cad-a3-app-h1/HOME_FLOW.md` | 105 | `fd88db8` 2026-09-27 | APP-H1 shell design (Home, chooser, unsaved prompt) | historical; its "partial scrim / tap-tap plane" description is superseded by `SKETCH-UX-R1` |
| `artifacts/cad-a3-app-h1/BOOTSTRAP_SESSION.md` | 106 | `fd88db8` 2026-09-27 | native bootstrap half | historical |
| `artifacts/cad-a3-app-h1/DEVICE_E2E.md` | 120 | `fd88db8` 2026-09-27 | E2E-APPH1 journeys | historical |
| `artifacts/modeling-foundations-r1/SUMMARY.md` | 145 | `7e02240` 2026-10-06 | Freeform / Surface New Project; CI DEVICE `37472535341` | E3 source for FreeformOwnerTest / SurfaceOwnerTest |
| `artifacts/modeling-r1-owner-shell-sketch-fix/SUMMARY.md`, `OWNER_FEEDBACK.md` | 167 / 31 | `636797e` / `6157f4d` 2026-10-06 | mark + drawer (`MODELING-R1-OWNER-CORRECTION`) | E3 |
| `artifacts/cad-r2-c2-picking-seam/SUMMARY.md`, `TESTS.md` | 84 / 103 | `70a6c14` 2026-10-28 | last DEVICE runs `40782861799` (HomeFlowTest, EditorWorkspaceProjectActionsTest, OwnerProjectShellSketchSupportTest) and `40776968165` | E3 for this product tree |
| `artifacts/cad-r2-c1-quality-gate/SUMMARY.md` | 106 | `08406cf` 2026-10-28 | previous device union | E3 (older head, same shell code) |
| `artifacts/cad-vertical-slice-r1/FULLSHARDED_C4.md` | 247 | `a1b50f2` 2026-09-29 | last `FULL_SHARDED_SUITE_PASS` (`CI FULL SHARDED 36607747079` on `fa0b6b4`) | last full-sharded aggregate (OLDER tree). The persistence / transfer / settings classes now also have fresh focused evidence on this tree: CI DEVICE `40805804385` (§3b) |
| `artifacts/function-council-r1/COUNCIL_FINDINGS.md`, `SUMMARY.md` | 211 / 90 | `ba2e2e4` 2026-09-29 | D1–D7 | re-checked on this tree (see SEAT_A.md); not promoted without re-check |
| `artifacts/function-council-c1-d1/SUMMARY.md` | 106 | `509de02` 2026-09-29 | D1 fix, "Import GLB during an open sketch" recorded debt | re-checked: still present (A-04) |
| `artifacts/fable-cad-architecture-audit-r1/OWNER_DECISIONS.md` | 114 | `83c79e0` 2026-09-30 | audit-local owner decisions | NOT a canonical OWNER_DECISIONS file; not used as authority |
| `artifacts/cad-r2-retained-sketch-parametric-ux/BEFORE.md` | 96 | `a802541` 2026-10-28 | names `FORGESHAPE_PROJECT_STATE_v4.1.62.md` and `..._MASTER_PLAN_ADDENDUM_v4.1.62.md` as "not attached" | confirms the gap |

## 3. Fresh evidence produced in this audit (read, not fabricated)

All paths are relative to `artifacts/function-council-r2/` and persist in the repository checkout.
The canonical test record for every run below is `TESTS.md` (written by the coordinator); this
table only registers the sources. Session-scratchpad paths used in earlier drafts
(`/tmp/claude-0/.../scratchpad/fc2/*`) do not persist and are superseded by `evidence/local/*`.

### 3a. Local host / JVM / guard runs (`evidence/local/`)

| Artifact | Producer | Content used |
| --- | --- | --- |
| `evidence/local/host_selftests.txt` | coordinator, `host_exit=0` 2026-10-29 00:53:46Z (`evidence/local/run_status.txt`) | `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 `HOST_SUITE_OK` suites, `CAD_R2_ORACLE_PASS=17` (17 oracles) |
| `evidence/local/host_corpus_verdicts.txt` | coordinator, `corpus_verdicts_exit=0` | `CORPUS_FIXTURES 72`, one verdict per fixture |
| `evidence/local/gradle_build_tail.txt` | coordinator, `gradle_exit=0` | `BUILD SUCCESSFUL` (first run; `:app:testDebugUnitTest` was UP-TO-DATE there, so it is NOT the JVM evidence) |
| `evidence/local/gradle_unit_rerun_tail.txt` | coordinator, fresh `--rerun-tasks` | `BUILD SUCCESSFUL`, `22 actionable tasks: 22 executed`; JVM result 196 tests / 0 failures / 0 skipped as recorded in `TESTS.md` (the tail itself does not print the count) |
| `evidence/local/release_guard.txt` | coordinator, rerun | `RELEASE_SELFTEST_GUARD=PASS` (release x86_64 / arm64-v8a: 0 `SelfTests` symbols, 0 self-test strings). The earlier attempt (`guard_exit=2` in `run_status.txt`, `ANDROID_HOME/ANDROID_SDK_ROOT is not set`) was an environment error, not a product verdict, and is superseded by this PASS |
| `evidence/local/run_status.txt` | coordinator | exit codes and timestamps of the first local pass |
| `/tmp/claude-0/fc2-scratch/seatA/diag_import_during_sketch.cpp` (+ binary) | Seat A | E1 for A-04 (see SEAT_A.md for the exact command and output); scratch path, NOT persisted |

### 3b. GitHub Actions runs on audited tree `70a6c14` (emulator only)

| Run | Workflow | Evidence directory | Result used |
| --- | --- | --- | --- |
| `40796992679` | CI FAST | `evidence/fast-run-40796992679/` | success; `forge-corpus-parity.txt`: `FORGE_CORPUS_PARITY=PASS (72/72 byte-identical)`; `release-selftest-guard.md`, `verify-device-guards.txt`, `toolchain-manifest.txt` |
| `40797031704` | CI DEVICE | `evidence/device-run-40797031704/` | 32/32 (`instrumentation-counts.txt`: `run=32 failed=0`); visual-evidence classes; 25/25 startup tokens in order (`selftest-tokens-found.txt`, `summary.json`) |
| `40800569636` | CI DEVICE | `evidence/device-run-40800569636/` | 77/77 (`run=77 failed=0`); Surface / Freeform / Sculpt / Import-sculpt / CAD vertical slice / extent / SketchUx classes; 25/25 startup tokens in order |
| `40805804385` | CI DEVICE | `evidence/device-run-40805804385/` | 90/90 (`run=90 failed=0`); `ProjectAutosaveRecoveryTest`, `ProjectTransferTest`, `ProjectProcessDeathTest`, `ImportedMeshDurableTest`, `ObjectsDeleteTest`, `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest`, `EditorWorkspaceLifecycleTest`, `JniBoundaryHardeningTest`, `DiagnosticsAndRendererLossTest`, `SettingsPreferencesTest`; 25/25 startup tokens in order (`summary.json`) |

Limits: every device run is on the `ForgeShape_CI_API36` emulator (swiftshader). Per `CLAUDE.md`
(`CI-CLOUD-R1`) none of them closes a physical-device gate (stylus, pressure, hover, hardware GPU,
16 KB pages, real-device performance). No `-FullSharded` aggregate was run in this audit, so
`FULL_SHARDED_SUITE_PASS` is still only the older `36607747079` on `fa0b6b4` (§2).

## 4. Source gaps (NOT AVAILABLE) and what each limits

| Missing source | Evidence of absence | Sections of this audit it limits |
| --- | --- | --- |
| `FORGESHAPE_MASTER_PLAN_v4.1.61.md` (canonical Master) | `BEFORE.md` §"Authoritative sources" (filesystem search) | Every H0/H1/H2 horizon assignment in `work/req_A.tsv` is RECONSTRUCTED from repo records; an MVP item the Master lists but the repo never recorded cannot appear (onboarding/tutorial, recent files, templates, account, cloud, desktop keyboard map). Rows APP-077..APP-079 are therefore `FUTURE-NOT-DUE` / `BLOCKED-OWNER` on repo evidence only. |
| `FORGESHAPE_OWNER_DECISIONS_*` | BEFORE.md | Owner rationale for APP-H1 "exactly two Home actions" (A-03), for "recovered is not asked about" (A-02) and for Back semantics in editor modes (A-10) cannot be checked against the decision text; only the `PROJECT_STATUS.md` Owner Decision Baseline summary and `CLAUDE.md` restatements were available. |
| `FORGESHAPE_MASTER_PLAN_HISTORY_*` | BEFORE.md | Cannot tell whether H1 items (Freeform/Surface New Project, drawer) superseded earlier Master scope or were added beside it; horizon of APP-016/APP-018/APP-021 is H1 by repo stage id only. |
| `FORGESHAPE_PROJECT_STATE.md` v4.1.62 + `MASTER_PLAN_ADDENDUM_v4.1.62` | named in `cad-r2-retained-sketch-parametric-ux/BEFORE.md:23-24` as not attached | The latest owner state beyond `PROJECT_STATUS.md` 0.102.0 is unknown; any owner verdict issued after 2026-10-28 is not reflected. |
| Prior full Master v4.1.0 | BEFORE.md | No baseline to detect silent scope drops between Master versions. |

Consequence: Seat A's result is bounded by **PARTIAL-PLAN-SOURCE-GAP**; every horizon label carries
its repo source (file + section / rule id) in `work/req_A.tsv` column `source`.
