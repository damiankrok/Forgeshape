# STAGE027-R0 — Test plan for a future Stage027

Maps each proposed behaviour to the **cheapest evidence that can actually
falsify it**, and keeps ordinary verification inside `TEST-OWNER-03`: focused
device evidence, a ≈20-minute target, **no `-FullSharded`**, no aggregate, no
screenshot matrix. Nothing here was run in this audit.

Three tiers, cheapest first:

- **N** — native self-test, no device, no GPU (`*_SELFTEST_OK` token)
- **J** — JVM unit test
- **D** — instrumented device test, `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass ...`

---

## 0. Before any code — reproduce the two findings

Both are `SOURCE-CONFIRMED, RUNTIME-UNVERIFIED`. If either fails to reproduce,
this audit is wrong about it and the analysis must be corrected before a fix is
written.

| # | Claim | Tier | Evidence |
| --- | --- | --- | --- |
| R-1 | `FINDING-A`: in Sculpt, a tap on another visible body changes the active body | D | two bodies apart; enter Sculpt on A; tap B off A's silhouette; assert `sceneActiveBodyId()` moved — and assert the absence of `FORGESHAPE_SCENE_SELECT_REFUSED:in_sculpt_mode` in the log, which is what proves the row guard was bypassed rather than merely not triggered |
| R-2 | `FINDING-B`: Sculpt is enterable on a hidden body | D | hide the active body; press Start Sculpting; assert the mode entered and `sculptState` reports a frozen mesh while `snapshot()` draws nothing |

---

## 1. S1 — the two guards

| # | Behaviour | Tier | Evidence |
| --- | --- | --- | --- |
| G1-1 | A Sculpt tap on another body does **not** change the active body | D | repeat R-1; assert the id is unmoved and the named refusal is logged |
| G1-2 | A Sculpt tap that misses still **navigates** | D | the same tap as a drag orbits; `orbitCount` advances — this is the regression the guard could break |
| G1-3 | A tap that **hits** the sculpt mesh still strokes | D | `SculptRevision` advances; the existing sculpt suites already cover the shape of this |
| G1-4 | No sculpt vertex moved by a refused tap | D | `SculptRevision` unmoved across the gesture — the `CLAUDE.md` multi-touch-navigation rule restated for this path |
| G2-1 | Start/Resume Sculpt over a hidden body is refused by name | D | assert the log token and that the mode did not change |
| G2-2 | The control is **absent** over a hidden body | D | by semantic id, never by coordinate |
| G2-3 | A refused entry changes nothing | N + D | no freeze, no revision, no history entry, no fingerprint movement |

## 2. S2 — Isolate core (native; the bulk of the proof lives here)

| # | Behaviour | Tier | Evidence |
| --- | --- | --- | --- |
| I-1 | An isolated snapshot contains exactly the isolated body | N | build a scene of ≥3 bodies; `snapshot().size() == 1`; the id is right |
| I-2 | Isolate and hidden **compose** — an isolated body that is hidden is still absent | N | the two conditions in one loop, not two lists |
| I-3 | Picking follows, because it reads the same list | N | `pickSceneSnapshot` over the isolated snapshot misses the excluded body along a ray that hits it un-isolated |
| I-4 | Clearing isolate restores the full list exactly | N | set equality with the pre-isolate snapshot, by id |
| I-5 | **No durable visibility was written** | N | every `SceneObject::visible()` identical before and after an isolate cycle — the central Variant A vs B claim |
| I-6 | A body the user hid **stays hidden** after an isolate cycle | N | the case Variant B gets wrong |
| I-7 | No `ProjectHistory` step | N | undo/redo depth unmoved across isolate on and off |
| I-8 | No `SculptHistory` entry | N | entry count and cursor unmoved |
| I-9 | No `MeshRevision`, no publish, no rebuild | N | per-body revision unmoved |
| I-10 | Isolating an unknown or absent id is refused by name, never silently applied | N | fail-closed, on the product's own terms |
| I-11 | Isolate on an empty scene (Home) is a refusal, not a crash | N | `hasProject()` is the one answer |

## 3. S2 — persistence (the claim that Isolate is not truth)

| # | Behaviour | Tier | Evidence |
| --- | --- | --- | --- |
| P-1 | `.forge` bytes identical with Isolate on and off | N or D | encode → isolate → encode → **byte-for-byte** compare, the `SettingsPreferencesTest` pattern |
| P-2 | `projectSemanticFingerprint` unmoved | N | the value, not a proxy |
| P-3 | No checkpoint is provoked | D | `awaitIdle` as the barrier — **never a sleep for the debounce** |
| P-4 | Save/Open round trip is unaffected | D | existing project suites, unchanged |
| P-5 | The whole `.forge` corpus is untouched | — | `git diff --stat` over `testdata/` is empty; **thirty-six** fixtures unchanged |

## 4. S3 — the control

| # | Behaviour | Tier | Evidence |
| --- | --- | --- | --- |
| U-1 | Present in Sculpt, **absent** in Construction and Sketch | D | by semantic id |
| U-2 | 48 dp hit area | D | the existing measurement helper |
| U-3 | State survives rotation and HOME/resume | D | the `Grid` / `Selection Outline` pattern |
| U-4 | Cleared on Back to Construction (and per `LIFE-1` on Resume) | D | read native state after each transition |
| U-5 | The control and the viewport never disagree | D | toggle, rotate, re-read — the control is refreshed from native, never from a Java mirror |
| U-6 | Accessible label states the current state, not colour alone | D | content description assertion |
| U-7 | Works identically over an Imported Mesh | D | `IMPORT-01B` parity, one case |

## 5. Interaction with delivered behaviour (regression surface)

| # | Behaviour | Tier | Evidence |
| --- | --- | --- | --- |
| X-1 | Selection outline still correct on the isolated body | D | `SelectionOutlineVisualEvidenceTest` pattern — the differential amber scan over a region derived from native seams, never a hard-coded rectangle |
| X-2 | Stage025 mask presentation unchanged | D | the mask tint still reads on the isolated body |
| X-3 | Sculpt Undo/Redo and the History navigator unchanged | D | existing suites |
| X-4 | Grid and Selection Outline toggles unchanged | D | existing display suites |
| X-5 | Export writes **every** body, isolate or not | N | the exporter reads the scene, not the snapshot — assert it, because "exports every body" is a `CLAUDE.md` rule and an isolate is exactly the thing that could break it |
| X-6 | Render recovery unaffected | N | `forgeshape_render_recovery` is CPU policy; Isolate is CPU state |

**X-5 is the highest-value single case in this plan.** It is the one place where
a plausible implementation (filtering something the exporter also reads) would
silently produce a `.glb` missing objects the user can see.

## 6. If `PREVIEW-1` selects P1 or P2

Recorded, not planned — `STAGING_PLAN.md` §5 recommends this be its own stage.

| # | Behaviour | Tier |
| --- | --- | --- |
| W-1 | Device without `fillModeNonSolid` falls back by name, never draws nothing | D, and it needs hardware the project may not have |
| W-2 | Edge count bound enforced, refusal stated (P2) | N |
| W-3 | Stroke latency unregressed on the largest supported mesh (P2) | D + timing |
| W-4 | The overlay reaches no `.forge` byte, revision or history step | N |

---

## Budget and policy

- **Target ≈20 min, hard stop 30 min** of automated verification per slice,
  per `TEST-OWNER-03`.
- **No `-FullSharded`**, no aggregate marker, no screenshot matrix. Focused
  evidence cannot stand in for the exhaustive gate and must not claim to.
- Device evidence on `ForgeShape_Stage006` / **`emulator-5580`** only, started
  by `scripts\start-forgeshape-emulator.ps1`. **Never `emulator-5554`.**
  Confirm identity with `adb -s emulator-5580 emu avd name` before trusting a
  serial.
- `adb -s emulator-5580 logcat -G 64M` **before** any startup capture, confirmed
  with `logcat -g`. A missing self-test token with zero failures is a dropped
  capture until a larger buffer proves otherwise.
- If S2 adds a self-test suite, the **twenty-two** `*_SELFTEST_OK` token count
  and its emission order change in `CLAUDE.md`, `PROJECT_STATUS.md` and
  `README.md` together.
- Known standing test debt, unrelated but it will bite a future aggregate: the
  `SpatialSketchTest` suite-isolation defect, and no aggregate has been run since
  `SEL-OUT-R1` (`PROJECT_STATUS.md:4288-4296`).
- Most of the Isolate proof is **native** (tier N) and needs no device at all —
  the standalone NDK-clang runner makes I-1..I-11 and P-1..P-2 a seconds-long
  loop.
