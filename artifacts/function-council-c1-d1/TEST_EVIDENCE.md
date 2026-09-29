# D1: test evidence under TEST-OWNER-04 (the lean tiers)

**FullSharded NOT RUN.** No aggregate, no `-FullSharded`, no broad device
battery.

## Tier 0: static

| Check | Result |
| --- | --- |
| `git diff --check` | clean on every commit |
| Diff scope `a1b50f2..f29f482` outside `artifacts/` and docs | 9 files: one new androidTest class; `forgeshape_import_commit.{h,cpp}`, `forgeshape_jni.cpp`, `forgeshape_gltf_import_selftest.cpp`; `EditorWorkspaceView`, `NativeViewport`, `ProjectActionsPopoverView`; one string |
| Scripts or workflows changed | none, so the device-guard verification was not required. It ran inside CI FAST anyway (`DEV2-01..07`, `DEV3-01..06` PASS) |

## Tier 1: host native contracts (developer evidence, g++ on Linux)

`bash scripts/host-native-selftests.sh <filter>`, run on the baseline and on
the candidate.

| Filter | Baseline `a1b50f2` | Candidate `f29f482` |
| --- | --- | --- |
| `GLTF` | GLTF_EXPORT 93 + GLTF_IMPORT 189 = **282**, 0 failed | **282**, 0 failed |
| `SCENE` | **175**, 0 failed | **175**, 0 failed |
| `PROJECT` | **256**, 0 failed | **256**, 0 failed |

The count is unchanged on purpose. The new stable token `RefusedInSculpt` is
asserted by extending the existing
`IMP01A_08_every_commit_status_has_a_name` check rather than by adding one.

The host build leaves out `forgeshape_jni.cpp`, so the JNI guard itself has no
Tier 1 answer. It is proved on the device (Tier 2).

## Tier 3: CI FAST, candidate `f29f482`

Run `36643103708`, job `109659741759`, `workflow_dispatch`: **success** in
3 min 50 s (23:03:26 → 23:07:16 UTC).

- Build, JVM unit tests, and the debug, androidTest and release APKs:
  `BUILD SUCCESSFUL in 2m 46s`.
- `RELEASE_SELFTEST_GUARD=PASS` (release: 0 self-test symbols and strings;
  debug: 23 symbols).
- `FORGE_CORPUS_PARITY=PASS (44/44 byte-identical)`.
- `TESTRUNTIME-01..26`, `THR1-01..10`, `DEV2-01..07` and `DEV3-01..06` all
  PASS.

## Tier 2: one focused CI DEVICE dispatch, candidate `f29f482`

One union list, with the D1 class FIRST:
`ImportGlbSculptGuardTest, ImportedMeshSculptTest, GlbImportExternalR1Test,
JniBoundaryHardeningTest`.

| Attempt | Run / job | Wall | Result |
| --- | --- | --- | --- |
| 1 | `36643101662` / `109659734380` | 7 min 40 s | **`DEVICE_STARTUP_UNRESOLVED`** before any test body ran. See the classification below. |
| 2 (re-run of the same run and commit) | `36643101662` / `109663937803` | 11 min 44 s | **PASS**, detailed below |

Attempt 2 in detail:

- Startup capture 1: 23/23 tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure
  lines, 0 liblog drops.
- `OK (31 tests)`, 0 failed, 211 s of test time.

| Class | Tests | Failed |
| --- | ---: | ---: |
| `ImportGlbSculptGuardTest` (new; D1-02, D1-03..10, D1-04) | 3 | 0 |
| `ImportedMeshSculptTest` | 9 | 0 |
| `GlbImportExternalR1Test` | 14 | 0 |
| `JniBoundaryHardeningTest` | 5 | 0 |
| **Total** | **31** | **0** |

**Budget.** From the dispatch at 23:03:21 to attempt 2's end at 23:29:28 is
about 26 minutes. That is over the 20-minute target because of the
infrastructure re-run, and inside the 30-minute hard stop. The complete
attempt alone took 11 min 44 s, inside the preferred 15 minutes.

### Attempt 1 classification: infrastructure (a dropped startup capture), not product

`startup-captures.txt` (kept under `after-attempt1-run-36643101662/`):

```
capture=1 tokens=22/23 … failure_lines=0 liblog_dropped=27
capture=2 tokens=17/23 … failure_lines=0 liblog_dropped=952
capture=3 tokens=22/23 … failure_lines=0 liblog_dropped=51
settled_seconds=182 load1=4.61 (target < 2.5)
```

- **Each capture lost a different token.** Capture 1 lost
  `SCULPT_BRUSH_KERNEL`, capture 3 lost `CONSTRUCTION_BOX`, and capture 2
  lost six others.
- **Across the three captures, all 23 are present.** That includes
  `GLTF_IMPORT_SELFTEST_OK` in every capture, the suite this change touched.
- **No failure lines, viewport OK.** Every capture had zero failure lines, and
  the viewport reported OK every time.
- **The host was still loaded.** `load1` was 4.61 against a target below 2.5.

This is the dropped-capture pattern `CLAUDE.md` describes: a missing token with
zero failures is a dropped capture until a better capture proves otherwise.
The baseline run `36641798987` showed the same effect in its captures 1 and 2
(20/23 and 21/23 with drops) before capture 3 was complete.

The job died before any test body ran, which is one of the cases where one
re-run is allowed. That re-run was spent once, on the same commit, and passed
with a complete first capture and no drops.

## Attempts used

- **One complete focused attempt**, attempt 2, which was green. The limit is
  two.
- **One incomplete infrastructure attempt**, attempt 1, where no test ran.
- **One reproduction run on the baseline** (`36641798987`, `BEFORE.md`). The
  task requires it, and it is not a candidate attempt.
- **Nothing was retried to green.** Attempt 2 is the one allowed re-run of a
  job that never reached a test.
