# Deliberation record: seat H → seat E (FUNCTION-COUNCIL-R2, round 2)

## Round 2 — objections from seat H and responses

Seat E re-checked each objection against the source at `70a6c14` (read-only;
no build, no product edit). Corrections are applied in place above and in
`work/req_E.tsv`. Commands run: `sed`/`grep`/`awk` over the named files, and
`/tmp/claude-0/fc2-scratch/E/r2_tsv.py`, `r2_md.py` to apply the edits.

| # | Objection | Response | Re-verification | Status |
| --- | --- | --- | --- | --- |
| 1 | E-01 upheld; add: export writes a face-supported body that `snapshot` omits when its support does not resolve | CONCEDE (addition) | `forgeshape_scene.cpp:359-366` skips unresolved; `forgeshape_gltf_export.cpp:176-187` has no such check. The scene comment says a well-formed scene never reaches the branch, so reachability is E5. Added to E-01 and its fix (refuse by name) | RESOLVED |
| 2 | E-02 upheld; "those edits are lost" is E5 | CONCEDE | Sub-claim regraded E5 in E-02; the race is HYP-E7 | RESOLVED |
| 3 | E-03 upheld | AGREED | — | RESOLVED |
| 4 | E-04 upheld | AGREED | — | RESOLVED |
| 5 | E-06 upheld; fix must not change the lineage formula | CONCEDE (addition) | `DATA_PACKAGE_SPEC.md:555` states `(edgeLocalIndex & 0xFFFF)` in the lineage `code`. Fix text now says decode-side bound only | RESOLVED |
| 6 | E-07, E-09 upheld | AGREED | — | RESOLVED |
| 7 | E-08: owner decision, not plain defect | CONCEDE | CLAUDE.md "Discard retires the checkpoint" is literally what the code does. E-08 / DOC-048 now PARTIAL + owner decision, P3 | RESOLVED |
| 8 | JNI load mutates sketch/view/gesture before a post-decode refusal | CONCEDE | `forgeshape_jni.cpp:8463-8478` confirmed; refusals after it at `forgeshape_project_state.cpp:250-262` (edit in progress, validate, runtimeCanEvaluate) and E-05 class. Open rows reachable while sketching (`ProjectDrawerPolicy.java:109-111`); clean project skips the question; Java skips refresh on refusal (`EditorWorkspaceView.java:5490-5496`, `6095-6103`). Struck "Both stay fail-closed" from E-05; new E-12 / DOC-087 (P3, E2, DEFECT-CONFIRMED); DOC-011 and DOC-062 now PARTIAL; positive claim qualified to `loadProjectDocument`. My fuzz's "0 fail-closed violations" measured scene/fingerprint/body count only, so it does not contradict this | RESOLVED |
| 9 | Unsynchronized checkpoint I/O; two workers across a recreation | CONCEDE | `ProjectCheckpoint.write` static, no lock, one `.pending` path (`ProjectCheckpoint.java:47, 85-125`); fallback at `:112` deletes the valid file when the second rename fails; `read()` length at `:133` then open/readFully. `quitSafely` keeps due messages (`AutosaveController.java:283-289`). Added HYP-E6, HYP-E7 (mechanism E2, reachability E5); DOC-038 and DOC-041 now PARTIAL ("single-writer only"); journey J-E6 | RESOLVED |
| 10 | DOC-077 overstated | CONCEDE | `onRendererRestartRequired` (`EditorWorkspaceView.java:6147-6154`) has one production caller, `checkRendererLifecycle` from `onResume` (`ForgeShapeActivity.java:335-336, 371-382`); the class comment's "debug injection's follow-up" caller does not exist in the Activity. Split into DOC-077a (PASS-SOURCE-AND-RECORDED-RUNTIME, E3) and DOC-077b (PARTIAL, E2, deferred to next resume; cross-ref SEAT_H H-01) | RESOLVED |
| 11 | Fingerprint/encode skew (H-02) missing | CONCEDE | `AutosaveController.java:157, 173, 189`; JNI `projectFingerprint` and `encodeProject` lock separately. New E-13 / DOC-086, DEFECT-CONFIRMED, E2, P3 | RESOLVED |
| 12 | H-E1/DOC-084 graded inconsistently with H-02 | CONCEDE | `saveProjectToSlot` clear at `EditorWorkspaceView.java:5465` on UI thread; `noteProjectPersisted` posted behind the in-flight task (`AutosaveController.java:202-218`). Promoted to E-14; DOC-084 DEFECT-CONFIRMED, E2, P3 (timing E5), same grade as E-13 | RESOLVED |
| 13 | E-10/DOC-051 should be PARTIAL | CONCEDE | `closeProject` callers: `EditorWorkspaceView.java:2420, 2446, 2580, 2641, 2721`, all shell paths; no reachable wrong outcome shown. Now PARTIAL, P3/INFO. D7 stays listed as an open guard asymmetry | RESOLVED |
| 14 | DOC-039 horizon wrong | CONCEDE | Relabelled H0; PARTIAL with the concurrent-writer note | RESOLVED |
| 15 | "Refused by expected name 28/28" overstated | CONCEDE, with a count correction | `fc2/host_corpus_verdicts.txt`: 22 (not 24) of the 28 are `InvalidSemanticValue`; the others are BadPayload, ChecksumMismatch, Truncated, UnknownRequiredSection, UnresolvedReference, UnsupportedMajor (1 each). No expected-file argument was given. Claim reworded; DOC-010 graded E1 (refusal) / E2-E5 (cause) | RESOLVED (count differs from seat H by 2) |
| 16 | PROJECT 256/0 equals the device cap | CONCEDE | `kMaxProjectChecks = 256` (`forgeshape_jni.cpp:821`); `Recorder::check` drops past `max` (`forgeshape_project_selftest.cpp:21-32`); host harness allocates 8192 (`scripts/host-native-selftests.sh:104`), so the host emitting exactly 256 is the suite's true size today. Footnote added | RESOLVED |
| 17 | Load does not end the chooser, Dimensions, gizmo | CONCEDE | `loadProject` has no `supportChooser().cancel()` (closeProject does, `jni:8976`). Whether Java's post-load `dismissPrimarySurfacesExcept(null)` / `onNativeStateChanged` ends them was not traced. DOC-052 scope extended; those parts UNDETERMINED-SOURCE-GAP / E5. E-07 stands | UNRESOLVED (trace pending) |
| 18-21 | Positive claims upheld (writer canonical; fuzz at domain level; D1/D5 closed; release has no self-tests) | AGREED | `guard_exit=2` vs `RELEASE_SELFTEST_GUARD=PASS` stays an orchestrator question | UNRESOLVED (orchestrator: guard exit code) |
| 22 | H-E4: add render-thread half | CONCEDE, with a line correction | The render loop takes `g_stateMutex` per frame at `forgeshape_jni.cpp:1870-1872` (seat H's 1822-1826 is the `g_viewport.mutex` wait). `encodeProject` holds `g_stateMutex` (`:8391`). The autosave worker's encode freezes the viewport the same way. Kept as hypothesis; reachability E5, OWNER-PHYSICAL-UNVERIFIED | RESOLVED |

Totals: 15 objections conceded (incl. additions: 1, 2, 5, 7-17, 22), 0 rebutted, 7 agreed
without change. Two items stay open: objection 17's chooser/Dimensions trace
and the release-guard exit code.
