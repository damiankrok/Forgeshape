## Round 2 — objections from seat F and responses

Product tree re-checked at `70a6c14`. `git diff --quiet 70a6c14 HEAD -- app/src/main` succeeded. No product file was modified.

Conceded: 18 of 19 objections (one in part). Rebutted: 1 (#6's commit figures, in part). Nothing remains UNRESOLVED between the seats. Two items stay open for the OWNER or the run owner (end of this section).

| # | Objection | Response | Re-verification | Status |
|---|---|---|---|---|
| 1 | C-01: merge with F-02, cross-reference F-01, and relabel "every caller keeps working" as E5 | CONCEDED | View state is reset only in `begin()` and `cancel()` (`forgeshape_sketch_session.cpp:225,270`). `backToEditing` (`:2275`) does not reset it. With `viewFrame()`, Back to Sketch and Edit Sketch would therefore reinstall the flipped view. SEAT_C C-01 and CAD-013 are amended. | RESOLVED; the OWNER statement on the post-fix Back behaviour stays open |
| 2 | C-02: record the reachable UI path and widen the Java fix to include the chooser | CONCEDED | `GlobalToolbarView.java:685` hides `markGroup` only in the bootstrap. `onProjectActionsRequested` (`EditorWorkspaceView.java:5411`) has no sketch guard. `:2823` gates on `!sculpting` only. | RESOLVED, P2 |
| 3 | C-03: the Keep Sketch half is wrong, and the Add/Cut half's E1 is overstated | CONCEDED | `keepsOnProducer` (`:545-552`) checks `target.sketches.size() < kMaxCadSketches`. Probe P6 re-implements `operationAvailable` and `candidateState` by hand. The sub-claim is withdrawn. CAD-063 is regraded to E2, with E1 only for the domain refusals. The deliberate preview-time refusal (`:2889-2892`) is noted. | RESOLVED, P3, OWNER question kept |
| 4 | Add F-05 (comma dropped, "2,5" becomes 25 m) to CAD-025 and raise the row to P2 | CONCEDED | `SketchDimensionLabelView.java:112` uses `DigitsKeyListener.getInstance("0123456789.-")`. CAD-025 is now P2 via F-05; C-04 itself stays P3. F-05 is E2: the filter behaviour is platform-documented and was not reproduced on a device by this seat. | RESOLVED |
| 5 | A third stale comment, `OverlappingHoles` | CONCEDED | Confirmed at `forgeshape_sketch.h:144-145`. C-05 and CAD-108 now list three comments. | RESOLVED |
| 6 | C-06 is overtaken by `40800569636`, and the "126 commits" figure is mis-stated | CONCEDED, with a numeric correction | `summary.json` and the junit XML show SketchUxTest 14, CadVerticalSliceTest 9 and CadExtrudeExtentTest 5, all with 0 failures. C-06 and CAD-112 are narrowed to the five still-stale classes and J-C1 is updated. 126 is the main..HEAD count. My run of `git log --oneline --since=2026-10-06 70a6c14` gives 75 commits in total and 26 on `app/src/main`, not 28 and 77; the difference is likely the time-zone boundary. SEAT_C now records my figures. | RESOLVED |
| 7 | Host suites E1 | No objection | `run_status.txt` shows `host_exit=0`. | — |
| 8 | CAD-107 should state that "no dedicated test" lets the drift return | CONCEDED | Added as a coverage note. The verdict is unchanged. | RESOLVED |
| 9 | Revolve angles | No objection | — | — |
| 10 | Chooser nearest target and seam fix | No objection (D22 is a simulated stylus; CAD-113 covers it) | — | — |
| 11 | The 80 JVM tests are pure policy | CONCEDED | The positive-claims row now says the tests are policy-only. | RESOLVED |
| 12 | CAD-018, 073, 110 and 076 are over-graded | CONCEDED | CAD-018 becomes PASS-SOURCE-AND-RECORDED-RUNTIME (E2+E3). CAD-073 is annotated "policy E1; view binding E2/E3". CAD-110 is limited to the data model. CAD-076 becomes native E1 (pending reconciliation) plus Java E2, verdict PASS-SOURCE-AND-RECORDED-RUNTIME. The timing was confirmed: `release_guard.txt` mtime 00:57:29 postdates `guard_exit=2` at 00:54:48. The Java `FORGESHAPE_CAD_HUD_TOUCH:` gate is `CadExtrudeCanvasView.java:249-250,537`. | RESOLVED; the guard rerun needs run-owner reconciliation |
| 13 | CAD-101 has the wrong horizon | CONCEDED | `GlobalToolbarView.java:896` withdraws Start Sculpting with `!cad`. Native refusal is at `forgeshape_jni.cpp:2930`. CAD-101 becomes H0/H1 PASS-SOURCE-AND-RECORDED-RUNTIME (E1 native + E2 control). A new row, CAD-114 (H2 FUTURE-NOT-DUE), covers CAD → Sculpt itself. | RESOLVED |
| 14 | CAD-089 BLOCKED-OWNER | No objection | — | — |
| 15 | CAD-001's Back path (F-03) | CONCEDED | In `dismissTopmostSurface`, the `bootstrapVisible()` branch (`:1177-1184`) runs before `topmostOpenSurface()` (`:1201`), so Back cancels the sketch even when the precision surface is open. CAD-001 becomes PARTIAL on the Back path (E2, not reproduced). The project-cost half stands. | RESOLVED |
| 16 | No requirement row for Back during an in-project Revolve (F-04) | CONCEDED | `hasDismissibleSurface` (`:1112-1117`) has no revolve term. `BackDismissal` registers the callback only while that answer is true (`ForgeShapeActivity.java:87-98,142-155`), and `targetSdk 36`. New row CAD-115: H1, DEFECT-CONFIRMED (E2), P2, cross-referenced to F-04. | RESOLVED |
| 17 | F-05 on CAD-051, 055 and 075 | CONCEDED | `CadExtrudeCanvasView.java:644` uses `"0123456789.-"`, while `NumericPropertyRow.java:109` uses `"0123456789.,-"` and `LengthUnit.java:129` maps `,` to `.`. So the comment at `EditorWorkspaceView.java:4103-4105` is false for "2,5". The dependencies are recorded. | RESOLVED |
| 18 | CAD-024's touch half (F-01) | CONCEDED | `EditorControlStyles.chip` sets only a minimum height (`:377`). `SketchDimensionLabelView` adds no minimum width, whereas `SketchDimensionLabelsView.java:177` does. F-01's 43.4 dp measurement is Seat F's E3 and was not re-measured by this seat. CAD-024 becomes PARTIAL. | RESOLVED |
| 19 | Merge J-C6 with J-F3 | CONCEDED | J-C6 is marked merged. | RESOLVED |

Open items for others (not seat disputes):
- **OWNER:** whether Back to Sketch and Edit Sketch should restore the aligned view or the user's flipped and rotated view once C-01 is fixed.
- **OWNER:** whether kept sketches share the 16-slot table (C-03).
- **Run owner:** reconcile `guard_exit=2` with the later PASS in `release_guard.txt`.
