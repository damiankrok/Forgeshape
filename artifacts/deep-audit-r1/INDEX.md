# DEEP-AUDIT-R1 — evidence index

A read-first deep audit of ForgeShape at baseline `be5b729c354a9876e24fbef54c630ee21fbe95d3` (clean). Audit before changing; automatic fixes limited to comment compression, stale current-truth doc correction, audit tooling, missing tests proving findings, and proven-P1 local behavior fixes.

**Result: `PASS-DEEP-AUDIT-R1-WITH-DEBT`.** Three proven P1 defects were found and fixed with tests; the remaining findings are P2/P3 debt, reported not implemented.

## Documents

| File | Owns |
| --- | --- |
| `BASELINE.md` | git baseline, file/line/comment/test census, APK sizes, deps, toolchain, device |
| `REPOSITORY_MAP.md` | layout, native dependency direction, the Part-A representation-assumption searches |
| `ARCHITECTURE.md` | the architecture as audited, and where the code disagreed with the docs |
| `FINDINGS.md` | the ranked finding table (P0–P3) and the cleared suspicions |
| `AUDIT_COVERAGE_MATRIX.md` | what was inspected at what depth; tools recorded NOT AVAILABLE |
| `NATIVE_CPP_JNI.md` | ownership, integer/FP safety, error paths, the 155-row JNI matrix |
| `THREADING_CONCURRENCY.md` | threads, locks, lock order, the two concurrency findings |
| `ANDROID_LIFECYCLE.md` | configuration model, callback discipline, renderer lifecycle |
| `PERSISTENCE_DATA.md` | `.forge` codec, corpus parity, all-or-nothing load, negatives, F-01 |
| `CAD_GEOMETRY_NUMERICS.md` | sketch/profile/triangulation/arc/spline/faces/TopoRef/Edit Sketch invariants |
| `SCULPT_HISTORY.md` | sculpt + per-body history invariants, aggregate-memory estimate, F-02 |
| `IMPORT_EXPORT.md` | GLB import/export/roundtrip/preview, F-03 |
| `RENDERER_GPU.md` | surface convention, per-body resources, recovery, carried debt |
| `PERFORMANCE_MEMORY.md` | measured device figures + reasoned bounds, F-06/F-07/F-09 |
| `SECURITY_FILE_HANDLING.md` | manifest, attack surface, diagnostics/privacy, licenses |
| `UI_INPUT_ACCESSIBILITY.md` | ids, arbitration, 48 dp floor, accessibility |
| `BUILD_RELEASE_ABI.md` | build matrix, ELF/symbol inspection, F-16 |
| `TEST_QUALITY.md` | inventory, TEST_TRUST_MATRIX, the brief's revisit items, the new DAR1 tests |
| `TEST_RESULTS.md` | native/JVM/build/guard/corpus/aggregate results on `d783d4b` |
| `COMMENT_AUDIT.md` | policy, corrected stale statements, before/after metrics, the >15-line whitelist |
| `MARKDOWN_TRUTH_MATRIX.md` | every tracked `.md` classified; the corrected root-doc contradictions |
| `FUTURE_CONTRACT_MATRIX.md` | every forward claim classified IMPLEMENTED / DEFERRED / SUPERSEDED / STALE |
| `DEAD_CODE_TODO.md` | the one non-TODO, unreferenced declarations, the release-symbol finding |
| `FULL_SHARDED.txt` | the authoritative exhaustive-sharded instrumented aggregate |
| `DEVICE_STARTUP.txt`, `STANDALONE_RUNNER.txt`, `STANDALONE_RUNNER_RED.txt`, `DEVICE_GUARDS.txt`, `CORPUS_VERIFYONLY.txt` | raw evidence logs |
| `tools/` | `jni_matrix.js`, `comment_inventory.js`, `standalone_runner_main.cpp` |

## Findings summary

| Sev | Count | IDs |
| --- | ---: | --- |
| P0 | 0 | — |
| P1 (fixed) | 3 | F-01 fingerprint omits arc/spline; F-02 stroke attributed to loaded body; F-03 4-byte name aborts CheckJNI |
| P2 (reported) | 5 | F-06 sculpt whole-mesh republish; F-07 per-frame face re-extraction under lock; F-08 unlocked sculpt-target rebind race; F-09 O(n³)/O(P²) on hostile CADB; F-16 self-tests linked into release |
| P3 (reported) | 6 | F-10 activeBody self-heal; F-11 touchEvent exception-check order; F-13 five representation branches unmerged; F-14 jni.cpp size; F-15 accidental external linkage; F-18 dead reserved-chip styling |
| P3 (fixed) | 2 | F-12 stale docs; F-17 stale comments |

## Commits (on `be5b729c`)

1. `247c4c7` chore(audit): compress source comments and remove stale narration
2. `7b8ef02` docs: reconcile ForgeShape current truth
3. `ab5dc8d` test(audit): add targeted audit regressions
4. `d783d4b` fix(audit): close three deep-audit findings in the native domain and JNI
5. docs(audit): publish deep audit evidence (this commit)

Attribution note: commits 1–4 carry `Co-Authored-By: Claude Fable 5.1` per the audit brief; a mid-session harness update changed the attribution to Claude Opus 4.8, which commit 5 carries.
