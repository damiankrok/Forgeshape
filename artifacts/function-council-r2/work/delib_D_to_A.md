# Deliberation record: Seat D objections to Seat A (FUNCTION-COUNCIL-R2, Round 2)

Audited tree `70a6c14` (`app/src/main` = `f3db953`). Files changed by this response: `artifacts/function-council-r2/SEAT_A.md` (findings re-graded in place + appended section) and `artifacts/function-council-r2/work/req_A.tsv` (rows APP-017, 021, 022, 023, 026, 030, 032, 037, 038, 039, 043, 044, 048, 057, 059, 060, 065, 066, 070, 076, 080, 082 edited; APP-087 added). Pre-edit copies: `/tmp/claude-0/fc2-scratch/seatA/SEAT_A.before_r2.md`, `/tmp/claude-0/fc2-scratch/seatA/req_A.before_r2.tsv`.

Re-verification done this round (read-only; nothing in the product tree touched):

```
cd /tmp/claude-0/fc2-scratch/D && ./diag_d2      # Seat D's binary, unchanged
INFO before load: p1Id=1 p2Id=1 active=1 selection=1
INFO after load: status=Ok activeBody=1 loadedCageIsP2=1 sessionActive=1 selection=1 firstSel=3
INFO pushPull after load -> Ok undo=1
grep -rnE "android/net/Uri|ContentResolver|content://" app/src/main/cpp --include=*.cpp --include=*.h -l | grep -v third_party | wc -l   -> 0
```

Plus source reads: `PROJECT_STATUS.md:4381-4388`, `ProjectAutosaveRecoveryTest.java:321-322`,
`PRODUCT.md:130-139, 188-197`, `strings.xml:62`, `EditorWorkspaceView.java:2440-2450, 2650-2678,
6104-6114`, `FreeformPresentation.java:125-135`, `SurfacePresentation.java:185-192`,
`forgeshape_jni.cpp:2262-2272, 8440-8490`, `fc2/release_guard.txt`, `fc2/gradle_unit_rerun.txt`,
JVM XML timestamps under `app/build/test-results/testDebugUnitTest/`.

| # | Objection | Response | Result | Status |
| --- | --- | --- | --- | --- |
| 1 | A-01: P1→P2, re-source to strings.xml:62 + PROJECT_STATUS "One project slot", APP-037 H0→H1 | Verified. The single-slot overwrite is an accepted Known Issue (`PROJECT_STATUS.md:4381-4388`), and Save is an explicit act. A further point strengthens D's horizon change: CLAUDE.md APP-H1 itself lists Open Saved Project among the leaves that ask Save / Discard / Cancel. Offering Save there is therefore the letter of the rule, and the fix needs owner decision 3. The defect stays: the requested Open becomes a reload of what was just saved, contrary to `unsaved_prompt`. | CONCEDED. DEFECT-CONFIRMED P2, H1, re-sourced. | RESOLVED |
| 2 | A-02: keep P1; add FSR1B-05 + PRODUCT.md:193-194 as conflicting pinned spec; fix needs owner decision 2; APP-057 is a conflict | Verified that FSR1B-05 asserts `!ProjectCheckpoint.exists` after Recover (`:321-322`). APP-057's "tests NONE" was wrong. Narrow rebuttal: A's coverage sentence "none leaves a project after Recover" stays accurate, because FSR1B-05 asserts retirement and does not leave or background afterwards. The sentence is kept and annotated. | CONCEDED (with that annotation). P1, DEFECT-CONFIRMED, APP-057 re-sourced as a conflict. | RESOLVED; owner decision 2 open |
| 3 | A-03: P2, BLOCKED-OWNER; PRODUCT.md:1990 is stale; APP-H1 trade-off | Verified: PRODUCT.md:133-138 says "exactly two" and `PROJECT_STATUS.md:4388` defers the library to APP-H1. The pre-APP-H1 lineage is recorded as Seat D's claim (E5, not traced in git by A). | CONCEDED. APP-032 BLOCKED-OWNER P2. | RESOLVED; owner decision 1 open |
| 4 | A-04 = D-02, merge, E1 + E2 | Agreed. | CONCEDED. Merged (APP-026). | RESOLVED |
| 5 | A-05: re-source to UI-PREF-R1 "dirties no project", P3 | Verified the rule text. The failure is fail-safe: an extra question, never a loss. | CONCEDED. P3, re-sourced (APP-039). | RESOLVED |
| 6 | A-06: E2 + E5, P3, correct the fix text | Agreed. Without a provider the empty-document consequence is platform knowledge, and re-encoding after process death is impossible. | CONCEDED. APP-043 P3, E2+E5, fix text corrected. | RESOLVED |
| 7 | A-07 / APP-038 → BLOCKED-OWNER, P3/INFO | Agreed: behaviour matches the sketch-volatility rule and no recorded requirement asks for the question. | CONCEDED. | RESOLVED; owner decision 4 open |
| 8 | APP-080 mis-sourced; duplicate; desktop is H2 | Verified that CLAUDE.md "Android / Vulkan safety" has no `configChanges` rule. | CONCEDED. APP-080 → H2 FUTURE-NOT-DUE, duplicate of APP-039 / APP-043, desktop handed to the portability seat. | RESOLVED |
| 9 | A-08 confirmed | No change. | — | RESOLVED |
| 10 | A-10 split | Agreed. The recovery question is a DOC-DIVERGENCE against PRODUCT.md:196. Sketch, Freeform and Dimensions are BLOCKED-OWNER under CLAUDE.md APP-H1. | CONCEDED. APP-060 → DOC-DIVERGENCE P3. | RESOLVED; owner decision 5 open for the mode cases |
| 11 | A-11 narrow to the New Freeform bypass; domain fallback to Seat D | Verified: line 2448 uses `freeform_refused_other` + the raw token, while `FreeformPresentation.java:132` / `SurfacePresentation.java:190` default to `*_refused_other`. | CONCEDED. APP-017 narrowed. | RESOLVED |
| 12 | A-12 = D-06, E1 suite level + E2 | Agreed. `host_selftests.txt` prints suite totals only. | CONCEDED. APP-048 merged. | RESOLVED |
| 13 | A-09 confirmed; A-13 only stale-after-clean-leave | Verified the comment at 6108-6110 ("exactly the case autosave is for"). | CONCEDED. APP-059 narrowed. | RESOLVED |
| 14 | Release guard obtained (PASS); APP-076 → PASS-SOURCE-AND-RECORDED-RUNTIME | Verified: `release_guard.txt` reads `RELEASE_SELFTEST_GUARD=PASS` (release 0/0 on both ABIs), and `debugMeshCommand` returns `JNI_FALSE` under `NDEBUG` (`forgeshape_jni.cpp:2267-2270`). | CONCEDED. Coverage table and "Unresolved dependencies" corrected. APP-076 is E2 (key path) + E1 (guard). | RESOLVED |
| 15 | JVM classes were re-executed fresh → E1 | Verified: `22 actionable tasks: 22 executed`, XML 2026-10-29T00:57:26Z. | CONCEDED. APP-021/022/023/066/070 JVM parts → E1. | RESOLVED |
| 16 | `_12` calls `syncFromNative()` itself | Agreed. | CONCEDED. Annotated; no regrade. | RESOLVED |
| 17 | A successful load keeps the Freeform edit session; add a row; H-A1 stays a low-impact hypothesis | Reproduced in this round (output above): `sessionActive=1 firstSel=3`, and pushPull acts on the new project. `loadProject` cancels only the sketch. | CONCEDED. New row **APP-087 DEFECT-CONFIRMED P3** (E1+E2). APP-030/044/082 annotated. H-A1 stays a hypothesis. | RESOLVED |
| 18 | Seeding claim holds, stronger than stated | No objection raised. | — | RESOLVED |
| 19 | Drawer claim holds at E1 | Grade updated. | — | RESOLVED |
| 20 | Import claim holds | No objection raised. | — | RESOLVED |
| 21 | Add the E2 grep for no Uri in the domain | Re-ran it: 0 files. | CONCEDED. Added. | RESOLVED |
| 22 | APP-065 must include "dirties no project" → PARTIAL | Agreed. A-05 is exactly that clause failing for a saved project. | CONCEDED. APP-065 PARTIAL P3, linked to A-05. | RESOLVED |
| 23 | Horizon labels | APP-037 → H1, APP-038 → BLOCKED-OWNER, APP-080 → H2, as above. | CONCEDED. | RESOLVED |
| 24 | Agreements | Noted. | — | RESOLVED |

Net after Round 2: no P1 except A-02. P2: A-01, A-03 (BLOCKED-OWNER), A-04. P3: A-05, A-06, A-07 (INFO), APP-087 and the remainder. The owner decisions still open are 1 (Home slot access), 2 (Recover semantics, conflicts with FSR1B-05), 3 (Save in the unsaved question before Open Saved Project), 4 (sketch as unsaved work) and 5 (Back inside in-project modes). No objection remains UNRESOLVED as a point of fact between seats A and D.
