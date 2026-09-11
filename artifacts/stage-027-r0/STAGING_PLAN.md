# STAGE027-R0 — Staging plan

Smallest implementable slices, their dependencies and the files each would
touch. **Nothing here is started.** File lists are the audit's prediction from
reading the call graph, not a design that has been validated by writing code.

Slices are ordered so that each one is independently shippable and independently
verifiable, and so that a slice that the OWNER cuts does not strand the others.

---

## Dependency graph

```
S1 (guards)  ──┐
               ├──> S3 (Isolate UI)
S2 (Isolate core) ─┘

S4 (Hide-in-Sculpt)   depends on HIDE-1 only; independent of S1..S3
S5 (Mesh Preview)     depends on PREVIEW-1 only; independent of S1..S4
```

S1 is placed first deliberately: it is the only slice that fixes something that
is wrong today, and putting it after S2 would let Isolate mask `FINDING-A`
before the guard existed.

---

## S1 — The two guards (no new feature)

**Blocked by:** `GUARD-1`, `GUARD-2` (`RECOMMENDED_CONTRACT.md` §13).
**Delivers:** the Sculpt target really is fixed; Sculpt on a hidden body has a
stated answer.

| Subsystem | Likely files | Nature of change |
| --- | --- | --- |
| JNI touch dispatch | `app/src/main/cpp/forgeshape_jni.cpp` (~`:7250-7290`) | a named refusal on the tap→`setActiveBody` path while `inSculptMode()`; one log token |
| JNI sculpt entry | `app/src/main/cpp/forgeshape_jni.cpp` (`:2589-2640`, `:2696-2716`) | `visible()` consulted, refusal by name (`GUARD-2 (a)`) |
| Java availability | `app/src/main/java/com/forgeshape/app/EditorWorkspaceView.java` (~`:2500`), `GlobalToolbarView.java` | withdraw Start/Resume over a hidden body, if `GUARD-2 (a)` |
| Strings | `app/src/main/res/values/strings.xml` | one refusal verdict for the status line |

**Explicitly not touched:** the scene, the histories, the codec, the renderer.

**Risk.** `FINDING-A` is runtime-unverified in this audit. **S1 must begin by
reproducing it on the device**, not by fixing it — if it does not reproduce,
there is a guard this audit did not find and the analysis must be corrected
before any code changes.

---

## S2 — Isolate core (native, headless-testable)

**Blocked by:** nothing. **Delivers:** the domain half of Isolate, provable by a
native self-test with no device and no GPU.

| Subsystem | Likely files | Nature of change |
| --- | --- | --- |
| Scene | `forgeshape_scene.h` / `.cpp` (`snapshot()`, `:261-306`) | the exclusion arrives as a value the scene is **given**, so the scene learns no presentation concept; the `!visible()` skip and the new restriction sit in the same loop, keeping ONE enforcement point |
| Session state | `forgeshape_display.h` / `.cpp` **or** `forgeshape_sculpt.h` | one session-only, process-scoped flag + the isolated `ObjectId`; `DisplaySettingsStore` is the existing precedent |
| Self-test | `forgeshape_scene_selftest.cpp` (or a new `forgeshape_isolate_selftest.*`) | if a new suite: `CLAUDE.md` records **twenty-two** `*_SELFTEST_OK` tokens and that count and its emission order must be updated in `CLAUDE.md`, `PROJECT_STATUS.md` and `README.md` together |
| CMake | `app/src/main/cpp/CMakeLists.txt` | only if a new translation unit is added |

**Design constraint that must not be relaxed.** The filter goes **inside**
`snapshot()`. Filtering at `forgeshape_jni.cpp:1652` and
`forgeshape_selection.cpp:14-18` instead would create a second place that
decides what is in the scene — the exact drift `CLAUDE.md` forbids when it says
hidden is enforced in one place. See `VARIANTS.md` §1 A1 vs A2.

**Free consequences, to be asserted rather than assumed:** picking follows;
the selection outline follows; no upload, no `MeshRevision`, no rebuild.

---

## S3 — Isolate control (Android)

**Blocked by:** S2, and `UI-1`, `LIFE-1`.

| Subsystem | Likely files | Nature of change |
| --- | --- | --- |
| Control | `SculptContextView.java` **or** `DisplaySettingsPopoverView.java` | per `UI-1` |
| Ids | `app/src/main/res/values/ids.xml` | one id naming the act |
| Strings | `strings.xml` | two states + content descriptions; must obey the `UIR4B-15` vocabulary rule (never "Freeze") |
| Bridge | `NativeViewport.java` | one setter, one getter |
| Refresh | `EditorWorkspaceView.java` | read state on every refresh; clear on leaving Sculpt per `LIFE-1` |
| JNI | `forgeshape_jni.cpp` | two entry points, logged like `setGridVisible` |

**Must hold:** absent in Construction and Sketch (not disabled); 48 dp hit area;
concentric corner with its host; state read from native on every refresh so the
control and the viewport cannot become two answers.

---

## S4 — Hide reachable from Sculpt (only if `HIDE-1` says yes)

**Blocked by:** `HIDE-1`. **Independent of S1–S3.**

| Subsystem | Likely files | Nature of change |
| --- | --- | --- |
| JNI guard | `forgeshape_jni.cpp:3017-3019`, `:3071-3100` | split `objectCommandsBlockedByMode()` so Show/Hide alone is permitted in Sculpt; **every other command keeps the guard** |
| Objects UI | `ObjectsSectionView.java:325-336`, `EditorWorkspaceView.java:2551` | a narrower availability signal than the current all-or-nothing overflow withdrawal |

**The unresolved consequence that must be answered before this ships:** a
Construction history step taken in Sculpt cannot be undone until the user leaves
the mode, because Undo in Sculpt routes to `SculptHistory`
(`forgeshape_jni.cpp:5495-5512`). That is the guard's stated reason and S4
reopens it. The audit recommends **not** taking this slice.

---

## S5 — Mesh Preview (only if `PREVIEW-1` says P1 or P2)

**Blocked by:** `PREVIEW-1`. **Recommendation: split into its own stage.**

Recorded here so the coordinator can see why it is not a slice of Stage027:

**If P1 (polygon mode):** `forgeshape_renderer.cpp:601-608` (device creation
with a feature query), a fallback path for devices without `fillModeNonSolid`, a
new pipeline, `forgeshape_render_recovery` (the decision re-made on rebuild),
plus the display-settings and UI work. Device creation and render recovery are
the two places this product is least willing to churn.

**If P2 (CPU edge list):** a new edge-extraction module, a bound (the existing
`kMaxSketchOverlayVertices = 65536` does not cover a 4 M-vertex Imported Mesh),
a new buffer and probably a new pipeline, and per-revision CPU work on the
stroke path — a direct `PERF-BASELINE` risk on the hottest path in the product.

Either is larger than S1 + S2 + S3 combined, and both are the natural companion
to Stage028 density work rather than to an isolate.

---

## Files with ZERO expected diff across S1–S3

Stated so a reviewer can check the claim mechanically rather than read for it:

- `DATA_PACKAGE_SPEC.md` and every `.forge` fixture under `testdata/`
- `scripts/build-forge-corpus.ps1`
- `forgeshape_project_document.cpp`, `forgeshape_project_bytes.cpp`,
  `forgeshape_project_state.cpp`
- `forgeshape_history.*`, `forgeshape_sculpt_history.*`
- `forgeshape_gltf_export.*`, `forgeshape_gltf_import.*`
- every shader under `app/src/main/cpp/shaders/`
- `forgeshape_renderer.cpp` (S2/S3 should need no renderer change at all — if a
  draft needs one, the filter is in the wrong place)
- `app/build.gradle` (the NDK pin is untouched)

## Documentation each slice owes

Per the ownership table in `CLAUDE.md`, and only after runtime verification:

- `PROJECT_STATUS.md` — verified capability and the single next stage
- `ARCHITECTURE.md` — where the isolate value lives and why `snapshot()` owns it
- `PRODUCT.md` — the user-visible behaviour, including that `PRODUCT.md:1287-1288`
  ("the other bodies stay visible while you sculpt") becomes conditional
- `CLAUDE.md` — one durable rule, if and only if Isolate introduces an invariant
  worth pinning; and the self-test token count/order if S2 adds a suite
- `README.md` — only if the self-test token list changes
