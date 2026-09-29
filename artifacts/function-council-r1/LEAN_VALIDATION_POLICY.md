# Lean validation policy — proposal (FUNCTION-COUNCIL-R1)

**Status: PROPOSAL.** This page changes no workflow, script, test or rule. It
describes how ordinary feature work could get a meaningful automated answer in
about 10–20 minutes and then go to the OWNER, with the exhaustive aggregate
kept for rare, named situations. Adopting it, in whole or in part, is the
coordinator's and the OWNER's decision (`COUNCIL_FINDINGS.md`, OWNER DECISION
REQUIRED).

The evidence behind every number is in `TEST_COST_AUDIT.md`.

## 1. What each layer costs today (measured)

| Layer | Measured cost | Source |
| --- | --- | --- |
| `CI FAST` | **4 min 26 s** job: build + JVM + four APKs 3 min 36 s, corpus parity 7 s, runner and device-guard checks 14 s, SDK 11 s | run `36603387133`, job step timings |
| `CI DEVICE`, fixed part | **~8 min** before the class's own tests count: SDK and image 39 s, APK build 2 min 9 s, AVD create + boot + load settle + 23-token startup proof ~4.3 min | run `36547148093` (one 1-test class, 8 min 14 s job); five DEVICE runs put the fixed part at 7.9–8.4 min |
| Device test | **6.2 s per test** on average in the aggregate (3925 s / 629), 3.9–16 s by class; a focused dispatch ran about 1.1–1.3× the aggregate's per-class time | C4 `logcat-final.txt`, `TestRunner` start/finish pairs |
| `CI FULL SHARDED` | **72 min 59 s** job; 65.65 min in five shards run **one after another on one emulator** | run `36607747079`, `FULLSHARDED_C4.md` §6 |
| Host native suites | not timed in this audit (it builds nothing); the whole binary is rebuilt only when a source is newer, and the vendored kernel is compiled once and cached | `scripts/host-native-selftests.sh:28-60` |

Two facts shape the whole policy:

- **Every `CI DEVICE` dispatch already runs all 23 native suites on the
  Android ABI.** They run at debug startup and the job requires all 23 tokens
  in order. A focused device run is therefore never "only one class".
- **`CI FAST` does not run the native suites at all** (`ci-fast.yml:122-129`
  runs the JVM tests and builds APKs; no step calls
  `host-native-selftests.sh`). A native regression first appears ~8 minutes
  into a DEVICE job.

## 2. The tiers

### Tier 0 — static, seconds to 2 minutes

Run locally before pushing.

```
git diff --check
git diff --stat origin/main...HEAD
pwsh scripts/verify-device-guards.ps1          # only when a script changed
pwsh scripts/test-instrumented-sharding.ps1    # only when the runner changed
pwsh scripts/test-instrumented-runtime.ps1     # only when the runner changed
```

### Tier 1 — contracts, 10 minutes at most

Host native suites for the touched subsystem, plus the JVM classes that own the
touched presentation rule. The host filter is ONE substring per run
(`scripts/host-native-selftests.sh:100`), so pick the widest substring that covers the
subsystem.

| Subsystem touched | Host filter(s) | JVM classes |
| --- | --- | --- |
| CAD sketch, regions, features, kernel | `CAD` (matches `CAD`, `CAD_A3`, `CAD_FEATURE`), `SKETCH_UX` | `CadHudPresentationTest`, `SketchChromePolicyTest` |
| Sculpt | `SCULPT`, `SCENE` | — |
| Objects / Construction | `CONSTRUCTION` (five suites), `GIZMO`, `SCENE`, `BODY_DIMENSIONS`, `MIRROR`, `PICKING` | `EditorUiStateTest` |
| Persistence / `.forge` | `PROJECT` — plus corpus parity (`pwsh scripts/build-forge-corpus.ps1` into a scratch directory, byte-compared) | — |
| Renderer / outline / shading | `RENDER` (matches `RENDER_SHADING`, `RENDER_RECOVERY`), `DYNAMIC_MESH` | `DisplaySettingsContractTest` |
| UI chrome, layout, input | `CAMERA` | `WorkspaceLayoutModeTest`, `ChromeMotionTest`, `AppThemeTest`, `PointerSemanticsTest`, `EditorUiStateTest` |
| Preferences | — | `AppPreferencesTest`, `AppThemeTest` |
| Import / export | `GLTF` (both), `PROJECT` | — |
| Project shell | `SCENE`, `PROJECT` | `EditorUiStateTest` |

```
bash scripts/host-native-selftests.sh CAD
bash scripts/host-native-selftests.sh SKETCH_UX
./gradlew :app:testDebugUnitTest --tests com.forgeshape.app.CadHudPresentationTest
```

The host run leaves out `forgeshape_jni.cpp` and `forgeshape_renderer.cpp`
(`scripts/host-native-selftests.sh:8-11`, `:43`), so a JNI or renderer change has no Tier 1
answer; it goes straight to Tier 2.

### Tier 2 — one focused `CI DEVICE` dispatch, 15 minutes preferred, 20 target, 30 hard stop

One dispatch per task, with a comma-separated `test_class` list for the touched
subsystem. `am instrument -e class` takes a list (`ci-device-smoke.sh:413`), and
the CAD slice already ran six classes in one dispatch.

| Subsystem | Classes | Measured test time in C4 | Expected job wall |
| --- | --- | ---: | ---: |
| CAD sketch / feature | `CadVerticalSliceTest`, `SketchUxTest` | 3.3 min | ~12 min |
| … + support, drag, extent | add `SpatialSketchTest`, `CadCanvasExtrudeTest`, `CadExtrudeExtentTest`, `SketchExtrudeTest` | 7.4 min | ~17 min |
| Sculpt | `SculptUndoTest`, `SculptBrushStage025Test` | 2.7 min | ~11 min |
| … + navigator, isolate, imported, retention | add `SculptHistoryNavigatorTest`, `Stage027SculptWorkflowTest`, `ImportedMeshSculptTest`, `EditorWorkspaceSculptRetentionTest` | 5.8 min | ~15 min |
| Objects / Construction | `ObjectsDeleteTest`, `ObjectCommandsTest`, `MirrorSmokeTest`, `BodyDimensionsSmokeTest` | 1.5 min | ~10 min |
| … + gizmo or transform code | add `EditorWorkspaceGizmoTest` | +4.0 min | ~15 min |
| Persistence / project files | `ProjectAutosaveRecoveryTest`, `ProjectProcessDeathTest`, `ProjectTransferTest` | 2.8 min | ~12 min |
| Renderer / outline | `SelectionOutlineTest`, `RendererCounterContinuityTest`, `DiagnosticsAndRendererLossTest` | 2.7 min | ~11 min |
| UI chrome / input | `Ui3dStateCorrectionTest`, `EditorWorkspacePointerTest` | 2.1 min | ~11 min |
| Preferences | `SettingsPreferencesTest` | 1.8 min | ~10 min |
| Import / export | `ImportedMeshDurableTest`, `GlbExportTest` | 2.8 min | ~12 min |
| Project shell | `HomeFlowTest`, `EditorWorkspaceLifecycleTest` | 1.5 min | ~10 min |
| Any JNI signature change | add `JniBoundaryHardeningTest` | 0.4 min | +0.5 min |

Rules for Tier 2:

1. **One dispatch, a union list.** `CI DEVICE` cancels an in-progress run on
   the same ref (`ci-device.yml:27-29`, `cancel-in-progress: true`). Two
   subsystems go into ONE list, not two dispatches.
2. **Order in the list matters today.** Classes inherit the process's scene
   (`TEST_COST_AUDIT.md` §6). Until the baseline reset is hardened, put the
   class that proves the new behaviour FIRST.
3. **A red focused class is investigated, never retried to green.** Re-run
   that class alone at most once, and only to separate an order dependence
   from a product defect.
4. **Visual-evidence classes are not an ordinary-task gate.** Run them when the
   OWNER needs frames; their artifact is the frames (Tier 4).

### Tier 3 — `CI FAST`

It runs on every push to `main` and every PR to `main` already. For a task
branch, dispatch it when the build, native code, persistence, the release APK
or a script changed, and always before integration. It runs in parallel with
the Tier 2 dispatch, so it adds no wall time.

### Tier 4 — OWNER hands-on review

Required for anything visual, ergonomic, discoverable or about feel: glyphs,
tints, the HUD, rail order, the mask overlay, Flatten and Crease feel,
one-handed reach, wording. Automation does not replace it, and a green device
run is never cited as evidence for it. The visual-evidence classes
(`SelectionOutlineVisualEvidenceTest`, `UiPrefVisualEvidenceTest`,
`SketchUxVisualEvidenceTest`, `CadA3VisualEvidenceTest`,
`Ui3dStateAuditTest`, `Ui3dDimensionVisibilityTest`) produce frames for it on
demand. Physical-device gates (stylus, pressure, hover, a hardware GPU,
16 KB-page ARM64, real-device performance) are Tier 4 on a physical device
and are never closed by an emulator.

### Tier 5 — `CI FULL SHARDED`, rare and named

Only with explicit coordinator authorization, and only for one of these:

1. a release or beta candidate;
2. a shared layout or ABI change that many classes cross: `MeshVertex` or
   `RenderVertex`, a Vulkan pipeline's vertex input, a JNI signature used
   across features;
3. a `.forge` section or version, or any corpus fixture;
4. the kernel boundary (`forgeshape_cad_kernel.*`) or the vendored library;
5. the test harness itself: `WorkspaceTestSupport`, `SketchTestSupport`, the
   baseline reset, the runner or its partition;
6. focused evidence that points to cross-class or order coupling;
7. a release decision whose last aggregate is older than the tree it would
   vouch for.

It is never the answer to one failing class. Run a `plan` first, then one
`fresh` attempt; the runner's two-attempt rule and 90/120-minute budgets stand.

## 3. What an ordinary task costs under this policy

| Step | Wall time |
| --- | --- |
| Tier 0 + Tier 1 on the host | ~2–8 min (the first host build compiles everything once) |
| Push; `CI FAST` and one Tier 2 `CI DEVICE` dispatch in parallel | ~10–17 min, set by the DEVICE job |
| **Automated answer** | **~12–20 min** from push, ~15–25 min end to end |
| OWNER hands-on review | the OWNER's time |

That compares with ~73 min for one aggregate job, which in C1–C3 produced its
answer only after 33–60 minutes and then failed on test defects rather than
product defects.

## 4. Changes that would make the tiers cheaper (not made here)

Each is a separate, authorised task; none is needed to adopt the tiers.

| Change | Saves | Risk |
| --- | --- | --- |
| Add `bash scripts/host-native-selftests.sh` to `CI FAST` | puts all 3757 native checks in front of every push, minutes before the DEVICE job | low: host g++; "developer evidence" status of the host run should be restated |
| A `subsystem` preset input on `CI DEVICE` that expands to the Tier 2 lists | retyping, wrong lists | low |
| Harden `resetToBaselineConstruction` to close and reopen by default | the order-dependence class of failure (C2 and C3), and the need to order lists | medium: every class inherits it, so it is itself a Tier 5 trigger |
| Replace the five fixed-delay captures and `settleLayout`'s 250 ms sleep with event waits | flakes; ~3 min of sleeping in the aggregate | low per class |
| Build the APKs once and fan the five shards out to five parallel device jobs | FullSharded wall from ~73 min to ~20–25 min | medium: the aggregate fingerprint needs the SAME APK bytes on every job, so the APKs must be built once and shared, never rebuilt per VM (each VM signs with a fresh debug key) |
| Balance shards by measured time instead of test count | nothing while shards run one after another; only useful with the fan-out above | low |
