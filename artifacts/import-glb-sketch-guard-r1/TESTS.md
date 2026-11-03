# IMPORT-GLB-SKETCH-GUARD-R1 — TESTS (every attempt)

## Branch

`feature/import-glb-sketch-guard-r1` from `8eb94ce77124b788e32a1f864b089f891aa576d7`.

| Commit | What |
| --- | --- |
| `ac161fa` | RED: `BEFORE.md`, the device test, the CR-03 host re-run. Product unchanged. |
| `e33a1ee` | GREEN: product + host/JVM tests, and a device-test harness fix |
| `89a5777`, `207fa14`, `1d3c97b`, … | docs and evidence only |

Product and test tree are frozen at `e33a1ee`. `git diff --name-only e33a1ee
HEAD -- app scripts testdata .github` returns nothing.

Every run below is on an emulator. Nothing here is physical-device evidence.
There was no FullSharded run, nothing was merged and no PR was opened.

## 1. RED (baseline product `8eb94ce`)

| What | Result |
| --- | --- |
| Closeout CR-03 diagnostic, rebuilt (`BUILD_OK`) and run A..E, exit 0 | **A:** import accepted while the sketch is Editing (return 0, bodies 1→2, active 1→2, undo 0→1, fingerprint moved); the import's step is not undoable while sketching. **D:** Sculpt refused 1005 (control). **C** (chooser) and **E** (bootstrap): accepted, OWNER questions. |
| `CI DEVICE` `41490895211` on `ac161fa`: `ImportGlbSketchGuardTest` | **5/5 FAIL**, all product-caused. Startup 25/25 in order, 0 failure lines. sk01 and sk05: row offered while sketching. sk02, sk03, sk04: each import call accepted (18 bodies, undo +3). |
| Mutation on the GREEN tree (sketch branch of `importModeRefusal` removed) | `HOST_SUITE_FAIL GLTF_IMPORT (209 checks, 8 failed)`. Restored afterwards. |

## 2. GREEN, local (Linux x86_64, NDK 29.0.14206865)

Exit codes are in `evidence/local/run_status.txt`.

| Command | Result |
| --- | --- |
| `bash scripts/host-native-selftests.sh GLTF_IMPORT` (targeted, first) | 209/209 |
| `bash scripts/host-native-selftests.sh` — attempt 1, in the background | **exit 141 (SIGPIPE), empty log: it never ran.** Not a test result. Re-run in the foreground. |
| same, attempt 2 | `HOST_SELFTESTS_OK (4454 checks, 0 failed)`; 25/25 `HOST_SUITE_OK`; `CAD_R2_ORACLE_PASS=17`; GLTF_IMPORT 209 (was 189); PROJECT 256 (unchanged) |
| `./gradlew :app:testDebugUnitTest --rerun-tasks` | 199 tests, 0 skipped / failed / errors (196 + 3) |
| `./gradlew :app:assembleDebug :app:assembleDebugAndroidTest :app:assembleRelease` | exit 0 |
| `bash scripts/ci-release-selftest-guard.sh` | `RELEASE_SELFTEST_GUARD=PASS` |
| `bash scripts/host-forge-corpus-verdicts.sh` | `CORPUS_FIXTURES 72` |
| `git diff --check` | clean |

## 3. Adversarial review (multi-agent, read-only)

**Confirmed: one harness defect, fixed in `e33a1ee` before any GREEN run.**
The device test's "no id gap" assertion assumed the allocator's next id
equals the last body's id + 1. The `ObjectId` high-water mark survives
earlier classes' undos and deletes, so the assertion could fail whatever the
product did, depending on class order. It now asserts:
- the new ids are fresh and form one contiguous range;
- the first new id is active;
- and, through the snapshot's encoded bytes, which carry the high-water mark,
  that a refusal minted no id.

**Rejected:** "the 256-slot capacity check cannot fail on the device". True
by construction, but the host runner's 8192-slot recorder is where an overflow
is caught. Not a defect.

**Coverage gap:** one of the three review lenses returned an empty result.
The other two lenses and the verify pass covered the diff.

## 4. CI DEVICE (API 36 x86_64 emulator, swiftshader)

| Run | Head | Classes (new class first) | Result | Time | Artifact |
| --- | --- | --- | --- | --- | --- |
| `41490895211` | `ac161fa` (RED) | `ImportGlbSketchGuardTest` 5 | **0/5 — expected RED** | 133 s | in the run |
| `41492901369` | `e33a1ee` | `ImportGlbSketchGuardTest` 5, `ImportGlbSculptGuardTest` 3, `ImportedMeshDurableTest` 12, `SketchUxTest` 14, `SketchExtrudeTest` 9, `CadRetainedSketchOwnerTest` 22 | **65/65 PASS**, 0 failed, none `@Ignore`d | 1395 s | `13166114773` (`sha256:02b77f96…c737`) |

- **Startup.** Both runs: 25/25 tokens in order on the first capture,
  `NATIVE_VIEWPORT_OK`, `liblog_dropped=0`.
- **Facts from the GREEN run:**
  - `native_Editing`, `native_Ready` and `native_staged_Edit_Sketch` each
    `[1006, 1006, 1006] RefusedInSketch`;
  - `import_row_offered_editing=false`, `import_row_offered_ready=false`;
  - `late_picker_status` "Import is not available while a sketch is open.
    Your project and your sketch are unchanged."
- **Scope.** One bounded run of six classes. It is not an aggregate.

## 5. CI FAST (last, on the frozen tree)

`41497992257` on `207fa14` (product = `e33a1ee`): **success**. Details are in
`evidence/fast-run-41497992257/ARTIFACT.txt`:
- corpus parity 72/72;
- JVM 199/0;
- release guard PASS;
- runtime and device guards PASS;
- the ZIP digest matches GitHub's;
- `app-debug.apk` 17727519 bytes, SHA-256
  `841a9ce50431c0085fd9f0e31beb516667dc1faff146f7105412892299ab2ba7`.

## 6. Not executed

| Item | Reason |
| --- | --- |
| FullSharded / CI FULL SHARDED | needs OD-03 OWNER permission |
| Physical device, stylus, TalkBack, ARM64 timing | no device: UNVERIFIED |
| The real system picker UI (DocumentsUI) | another app's surface. A recording `ProjectTransferHost` received the real request, and the Activity's callback delivered the answer. |
| Chooser-only and Home import behaviour | OWNER questions Q2/Q3; observed in the CR-03 host re-run only |
