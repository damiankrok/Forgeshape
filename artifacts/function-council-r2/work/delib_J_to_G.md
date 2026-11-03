# Deliberation — seat J → seat G (FUNCTION-COUNCIL-R2, Round 2)

Files corrected: `artifacts/function-council-r2/SEAT_G.md`, `artifacts/function-council-r2/work/req_G.tsv` (77 lines, 19 columns preserved).


Re-verification commands (read-only; scratch in `/tmp/claude-0/fc2-scratch/G/`):
`sed -n` over `ProjectSlot.java:95-155`, `ProjectCheckpoint.java:100-125`, `forgeshape_jni.cpp:520-536,600-615`,
`forgeshape_freeform_session.{h:40-46,cpp:636-642}`, `forgeshape_sketch_session.h:205-211`, `forgeshape_gizmo.h:225-232`,
`forgeshape_history_selftest.cpp:460-482`, `PROJECT_STATUS.md:2968-2973,4155-4165`;
`grep -rn '\.pending\|PENDING' app/src/main/java`; `ls -l --time-style=full-iso` on the fc2 scratchpad and APK outputs;
`git diff --stat fa0b6b4 70a6c14 -- AutosaveController.java ProjectSlot.java ProjectCheckpoint.java forgeshape_project_state.cpp`;
`unzip -o -q app/build/outputs/apk/release/app-release-unsigned.apk 'lib/*' -d /tmp/claude-0/fc2-scratch/G/rel && readelf -lW .../lib/<abi>/libforgeshape_native.so | grep LOAD`
(release APK sha256 `3b4d3194c68cf19725fa360e73d297b6dbf52fef1c55d549ed3534ba250c614c`). Pre-Round-2 copies: `SEAT_G.before_r2.md`, `req_G.before_r2.tsv` in the scratch dir.

| # | Objection (seat J) | Re-verification | Response | Change made | Status |
| --- | --- | --- | --- | --- | --- |
| 1 | G-01 stands; split device impact; cite `PROJECT_STATUS.md:4158-4163`; FAIL only logs | `history_selftest.cpp:466-480` (`overflow` is `const size_t`, oracle folded); `jni.cpp:526-534`, `:605-613` only `FS_LOGE`; `PROJECT_STATUS.md:4158-4163` logcat drops whole suites | **CONCEDE** | G-01 Impact split: history = E2 arm64 asm + E1 host; lens = E1 host emulation, arm64 E5. H-G-01 → H-G-01a / H-G-01b. Gate P1 cannot refute (4158-4163), replacing the weaker shallow-repo argument. Impact restated as evidence pollution, product unaffected; P2 kept. PORT-031 evidence cell split | RESOLVED |
| 2 | G-02 confirmed | — | Agreed | none | RESOLVED |
| 3 | G-03 confirmed | — | Agreed | none | RESOLVED |
| 4 | G-04 confirmed, no verdict flips | — | Agreed | none | RESOLVED |
| 5 | G-05 / PORT-028: drop gizmo; 24 px requirement exists; real mismatch Freeform vs sketch | `gizmo.h:225-231` hit-corridor comment; `freeform_session.h:43-45` "the sketch's own tap slop"; `.cpp:640` `× gizmoPixelsPerReferenceUnit()`; `sketch_session.h:208-210` `kSketchTapSlopPixels = 24.0f` | **CONCEDE** | G-05 retitled DOC-DIVERGENCE P3 (E2), gizmo removed, CLAUDE.md `CAD-V6-S2-CORRECTION-FILL-PICK-R2` cited as H1 source; desktop-DPI part FUTURE-NOT-DUE. PORT-028 narrowed and regraded; owner decision 4 rewritten | RESOLVED |
| 6 | G-06 / PORT-010: requirement invented; overlap with J-03 | `DATA_PACKAGE_SPEC.md:42`, `:1563-1566` as quoted by J; SEAT_J J-03 (`project_state.cpp:753`) | **CONCEDE** | PORT-010 source = "derived (seat G)", H2, FUTURE-NOT-DUE, no gap; cross-ref J-03; owner decision 3 merged with OD-J3 | RESOLVED |
| 7 | PORT-048 PASS contradicts J-04 and source | `ProjectSlot.java:110-114` `slot.delete() && pending.renameTo(slot)`; `ProjectCheckpoint.java:107-122` same; `read` (:130-150) reads the slot only; grep: no reader of `*.pending` | **CONCEDE** — my "a failed write keeps the previous valid file" was wrong for the delete-then-retry path | PORT-048 → PARTIAL, P3, E2, owning finding SEAT_J J-04; expected_behaviour "keeps a READABLE valid file"; evidence "recorded" removed | RESOLVED |
| 8 | PORT-048/-050 cite "recorded" with no run id | SEAT_J:126-129, :377: `FULL_SHARDED_SUITE_PASS` `36607747079` on `fa0b6b4` (`ProjectAutosaveRecoveryTest` FSR1B-01..09). `git diff --stat fa0b6b4 70a6c14`: the three Java persistence files are identical; `forgeshape_project_state.cpp` changed (+231/−14) | **CONCEDE** (named the run) | PORT-050 names `36607747079` @`fa0b6b4`, grade "E2 + E3 (older tree)"; Java scheduling unchanged since that run, native fingerprint changed so that part is E2 only; HJ-1 cross-referenced (statement unchanged). PORT-048 handled in #7 | RESOLVED |
| 9 | PORT-003/005/011/013/053/054/069 PASS-REPRODUCED on E2 | grep-only evidence, no runtime | **CONCEDE** | all seven → IMPLEMENTED-UNTESTED, E2 | RESOLVED |
| 10 | PORT-072 can be E1 | My own `readelf -lW` on the fresh release APK: every LOAD 0x4000 on arm64-v8a and x86_64 (reproduces J) | **CONCEDE** | PORT-072 → PASS-REPRODUCED, E1 for alignment; 16 KB runtime OWNER-PHYSICAL-UNVERIFIED; positive-claims row added | RESOLVED |
| 11 | "byte-deterministic across STLs" overstated | `COMMANDS.txt` #2 is `-fsyntax-only` for libc++; #7/#8/#11 libstdc++ | **CONCEDE** | positive claim narrowed to g++13 / clang18 with libstdc++ (E1); STL claim E5 (no libc++ encode, no named device digest run cited) | RESOLVED |
| 12 | Resolve `guard_exit=2` | `run_status.txt` 00:54:48; `release_guard.txt` mtime 00:57:29 > `gradle_unit_rerun.txt` 00:57:27; `app-debug.apk` mtime 2026-10-28 21:20:06 | **CONCEDE** | positive claim states the PASS is from the rerun, and that the debug rows read the 21:20 debug APK; open question marked RESOLVED | RESOLVED |
| 13 | Horizons: PORT-031 → H0; PORT-032 → H2 no gap; PORT-056 → H2; PORT-067 E3 vs E1 | CLAUDE.md "Commands"; `PROJECT_STATUS.md:2624`; no requirement found for PORT-056; `TEST_COVERAGE_MATRIX.md:19` grades CI FAST `40796992679` E1 | **CONCEDE** | PORT-031 H0; PORT-032 H2, BLOCKED-OWNER, severity "-"; PORT-056 H2, FUTURE-NOT-DUE, severity "-" (hypothesis H-G-02 kept); PORT-067 E1 | RESOLVED |
| 14 | G-07/08/09, H-G-03, H-G-05 no objection; link H-G-03 to HJ-2 | — | Agreed | H-G-03 now names SEAT_J HJ-2 | RESOLVED |

Tally: 14 objections, 14 conceded (4 of them agreements with no change), 0 rebutted, 0 UNRESOLVED.
