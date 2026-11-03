p='/home/user/Forgeshape/artifacts/function-council-r2/SEAT_I.md'
t=open(p,encoding='utf-8').read()
def r(old,new):
    global t
    assert t.count(old)==1, old[:60]
    t=t.replace(old,new)
r("""- **Impact.** The next PROJECT check would run on the host and be silently
  skipped on the device""","""- **Impact (latent).** PROJECT has 0 headroom, but no check is lost on this
  tree: the host harness (cap 8192) also records exactly 256. The defect is
  that an overflow would be SILENT: the next PROJECT check would run on the
  host and be skipped on the device""")
r("""  (`artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md:76`, E3).""","""  (`artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md:76`, E3).
  Other suites close to their caps (host count / device cap, E1/E2): CAMERA
  119/128, CONSTRUCTION_BOX 100/128 (`forgeshape_jni.cpp:923`), DYNAMIC_MESH
  91/128 (`:902`), MIRROR 62/128 (`:567`).""")
r("""### I-02 — P2 — The 25 suites re-run on every Activity start, ~8.4 s each on the CI emulator, once per instrumented test""","""### I-02 — P2 — The 25 suites re-run on every Activity start, ~8.4 s each on the CI emulator, once per instrumented test (PARTIAL + DOC-DIVERGENCE; revised in round 2)""")
r("""- **Impact.** About 46 % of device test time in this run is self-test
  re-execution; per-test cost is ~20.8 s against FC-R1's 6.2 s; a 723-test
  aggregate would not fit the 120-minute hard stop (E5). In a debug build, any
  cold Activity creation in a live process blocks the main thread ~9 s. Release
  is unaffected (suites compiled out).
- **Verdict.** DEFECT-CONFIRMED (debug path / test cost; the code contradicts its
  own "once per PROCESS" comment).""","""- **Cause (revised).** The per-start re-run is not new: the same
  `NativeViewport.stop()` stood at `fa0b6b4` (`ForgeShapeActivity.java:581-582`),
  whose 629-test aggregate took 65.65 min. It is the MULTIPLIER; the growth is
  suite time — CAD_FEATURE alone takes 7.5 s of the ~8.4 s on the device
  (141 checks in the old status table, 638 now).
- **Impact.** About 46 % of device test time in this run is self-test
  re-execution (304.4 s of 667 s). The 32 tests are CAD owner classes, so their
  20.8 s per test is not representative. Projection (E5, assumes every test
  cold-starts the Activity, unverified for the whole inventory): suite
  re-execution alone is 723 × 9.51 s (304.4/32) ≈ 6 877 s ≈ 114.6 min, 95 % of
  the 120-min hard stop; the aggregate then exceeds it if the non-suite time
  over all 723 tests exceeds ≈ 5.4 min (≈ 0.45 s per test). In this run the
  non-suite time was 362.6 s / 32 ≈ 11.3 s per test. In a debug build, any
  cold Activity creation in a live process blocks the main thread ~9 s. Release
  is unaffected (suites compiled out).
- **Verdict.** PARTIAL (test cost, debug-path latency). CLAUDE.md requires only
  "run once from `NativeViewport.start()` … never per frame", and the code meets
  that. DOC-DIVERGENCE for the `ForgeShapeActivity.java:33-36` comment ("Started
  once per PROCESS"). Consistent with `TST-007`.""")
r("""- **Impact.** An undispatched task branch has no CI evidence while the docs say
  it does.""","""- **Impact.** An undispatched task branch with no open PR to `main` has no CI
  evidence while the docs say it does; a task branch with an open PR to `main`
  IS tested (`pull_request: [main]`). `docs/CI_CLOUD.md` is inconsistent with
  itself: `:12` "Push it; GitHub Actions tests it" against its own table at
  `:22-23` ("every push to `main`, every PR to `main`"), which is correct.""")
r("""### I-04 — P3 — Stale counts in five places""","""### I-04 — P3 — Stale counts in six places""")
r("""  - `scripts/ci-device-smoke.sh:57`: "The twenty-four startup suites" above a
    25-token list.""","""  - `scripts/ci-device-smoke.sh:57`: "The twenty-four startup suites" above a
    25-token list.
  - `.github/workflows/ci-device.yml:2`: header comment "(23 self-test
    tokens…)" (25) — added in round 2 (seat B).""")
r("""- **Verdict.** PARTIAL. Not a defect: the PROJECT_STATUS records state focused
  unions honestly, and the Tier-5 run before the v6→main merge is still pending
  (BLOCKED-OWNER).""","""- **Requirement (revised).** The source is the Tier-5 gate before merging to
  `main` (`PROJECT_STATUS.md` Next Stage :4851-4865, Tier-5 policy :2545), H1 —
  not CLAUDE.md's documentation rule.
- **Construction-domain classes with no run on `f3db953`:**
  `EditorWorkspaceGizmoTest` (46), `EditorWorkspaceHistoryTest` (20),
  `EditorWorkspaceObjectsTest` (10), `ObjectsDeleteTest` (8),
  `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest` (1 each).
- **Verdict.** PARTIAL. Not a defect: the PROJECT_STATUS records state focused
  unions honestly, and the Tier-5 run before the v6→main merge is still pending
  (BLOCKED-OWNER).""")
r("""  Only `ModelingOwnerRig.touchView` checks the control is the hit target
  (`ModelingOwnerRig.java:139-149`) and `realGesture` enters at the decor root
  (`:215-235`) — 112 sites in 9 classes.
- **Impact.** The CLAUDE.md rule "a surface may never partially cover another
  live control" and "the nearest chooser target owns a tap" are only provable by
  the 9 rig classes.""","""  Decor-root, hit-checked input exists in 8 test classes (corrected in round 2):
  4 through `ModelingOwnerRig` (`touchView`/`touchId` check the hit target,
  `:119-149`; `realGesture`/`realDrag` enter at the decor root, `:215-257`) —
  `CadParametricHistoryOwnerTest`, `CadRetainedSketchOwnerTest`,
  `FreeformOwnerTest`, `SurfaceOwnerTest` — and 4 with their own decor-root
  `realGesture` — `CadFillPickR2Test`, `CadMultiFaceOwnerTest`,
  `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`. About 106 call sites by
  re-count. The rig's `press()` is `performClick()` and is unchecked.
  `EditorWorkspaceGizmoTest` dispatches drags to the viewport view (`:2126`,
  `:2138`).
- **Impact.** The CLAUDE.md rule "a surface may never partially cover another
  live control" and "the nearest chooser target owns a tap" are only provable by
  those 8 classes. No gizmo test could detect chrome standing over a gizmo
  handle.""")
r("""| Suite results are order- and repetition-independent | seat-I `rev_order` (E1) |""","""| Suite results are order- and repetition-independent WITH AN EMPTY PROCESS SCENE (host) | seat-I `rev_order` (E1). Untested: `forgeshape_primitive_selftest.cpp:124-128` short-circuits to true when `constructionScene().hasProject()`, so on a warm Activity restart with a project open (I-02) it passes vacuously (E2) |""")
r("""| Release `.so` has 0 self-test symbols/strings on both ABIs; debug control > 0 | local guard PASS; `CI FAST` `40796992679` (E1); all 35 self-test TUs only in the debug CMake list (E2) |""","""| Release `.so` has 0 self-test symbols/strings on both ABIs; debug control > 0 | `CI FAST` `40796992679` (E1); all 35 self-test TUs only in the debug CMake list (E2). The local run is supporting only: `run_status.txt` records `guard_exit=2` (precondition-failure path, `ci-release-selftest-guard.sh:29/37/58`) at 00:54:48; `release_guard.txt` (mtime 00:57) says PASS, but the rerun's command and exit code were not recorded |""")
open(p,'w',encoding='utf-8').write(t)
