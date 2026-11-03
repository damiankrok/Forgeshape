# FUNCTION-COUNCIL-R2 — Prioritised backlog

Audited tree `70a6c14` (`app/src/main` = `f3db953`). This is a research output: **nothing in
it is started or authorised**.

- The long text of every item lives in `OPPORTUNITIES.md`, linked here by `OPP-nn` / `IDEA-nn`.
- Decisions are in `OWNER_DECISIONS_REQUIRED.md` (`OD-nn`).
- Root causes (`RC-nn`) are in `COUNCIL_DELIBERATION.md` §4, and contract rows (`CC-nn`) in
  `CONTRACT_CONFLICTS.md`.
- Fresh executions and run ids are owned by `TESTS.md`.

**Verification standing (supersedes `OPPORTUNITIES.md` §0, which predates it).**
`work/VERIFICATION.md` exists. All **30 panel-verified P0–P2 findings survived** (at least 2 of
3 lenses), giving 23 distinct root causes. The 8 lower-ranked P2 findings and every P3 keep
their seat grade only, and are marked "seat" below. **10 of 12** strongest positive claims were
judged overstated: they stand at E2 (source) and are not upgraded to PASS-REPRODUCED. That
matters for the "leave alone" list (§5).

**Legend.**

- Severity: P0–P3 after Round 2.
- Evidence:
  - E1 means reproduced in this audit.
  - E2 means source or mechanism read on this tree.
  - "panel n/3" means the lenses upheld it n times.
- Effort: XS < 2 h, S ≤ 1 day, M 2–4 days, L 1–2 weeks, XL > 2 weeks, with confidence H/M/L.
- Gate: **F** format, **Li** licence, **CI** CI change, **O** OWNER decision. "—" means none.
- **OWNER-FIRST** marks an item that cannot be scheduled until its OD is made.

There is **no P0**: no crash, no corruption of a stored `.forge`, and no proven silent loss of
saved data.

---

## 1. Class 1 — verified defects (de-duplicated by root cause)

### 1A. P1 / P2 findings that survived verification (plus one seat-graded latent P2)

| Rank | Id | Title | Sev. | Evidence | Root-cause file | Effort | Risk | Gate | Depends on |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | OPP-01 (RC-01; B-02 = E-01) | Export writes a face-supported CAD body at the origin | P1 | E1 (`export_face`, `diag_face_supported`); panel 3/3 ×2 | `cpp/forgeshape_gltf_export.cpp` (`captureGlbExportScene`) | S, H | Low | — | none |
| 2 | OPP-G1 (RC-06; A-02) | Recovered work is in neither slot nor checkpoint after Recover | P1 | E2; panel 3/3 | `EditorWorkspaceView.java:5652-5687` | S, M | Medium (inverts FSR1B-05 under some options) | **O: OD-05 — OWNER-FIRST** | OD-05 |
| 3 | OPP-02 (RC-01; B-01, B-03) | A face-supported CAD body accepts a transform write and a gizmo, records Undo and dirties the project, but does not move | P2 | E1 host; panel 3/3 (B-01, B-03) | `cpp/forgeshape_jni.cpp` (`applyBoxTransform`, gizmo activation), `cpp/forgeshape_gizmo.cpp`, `EditorWorkspaceView.java:3028-3033`, `WorkspaceTrailingHostView.java:605-620` | S, M | Low | — (OD-16 only for a decoder change) | OPP-01 in the same stage (same RC) |
| 4 | OPP-03 (RC-02; A-04 = C-02 = D-02 = E-03) | Import GLB offered and accepted while a sketch or chooser is open | P2 | E1 ×2; panel 3/3 ×4 | `cpp/forgeshape_jni.cpp` (`importGlbDurable`), `EditorWorkspaceView.java:2823` | S, H | Low | — | none |
| 5 | OPP-04 (RC-11; D-01) | Undo after a NotRetained stroke lands on a state that never existed and clears `hasEdits` | P2 | E1 (`diag_d3`); panel 3/3 | `cpp/forgeshape_sculpt_history.cpp` (`record`) | XS, H | Low | — (OD-22 only for "refuse") | none |
| 6 | OPP-09 + OPP-14 + OPP-15 (RC-04; E-02, A-05 = E-04, A-06) | Session memory in View fields is lost on recreation: recovery asked again over a live project, the saved baseline forgotten, picker results dropped | P2 | E2 + E3 mechanism; panel 3/3 (E-02, A-05, E-04, A-06) | `EditorWorkspaceView.java`, `EditorUiState.java` | S–M, M | Low–Medium | — | none |
| 7 | OPP-05 (RC-09; F-05) | Three canvas editors drop the decimal comma: "2,5" becomes 25 | P2 | E2 (+ E1 parser); panel 3/3 | `CadExtrudeCanvasView.java:644`, `SketchDimensionLabelView.java:112`, `BodyDimensionLabelsView.java:127` | XS, H | Low | — | none |
| 8 | OPP-06 (RC-03; C-01 = F-02) | Navigator Flip and ±90° do not move the camera | P2 | E1 probe + E2; panel 3/3 ×2 | `cpp/forgeshape_jni.cpp` (`beginSketchView`), `SketchUxTest` | S, M | Medium (CAD-UX-S1-C1 view contract) | — for option (a); O: OD-09 for (b) | none |
| 9 | OPP-08 (RC-08; F-04) | Back during an in-project Revolve backgrounds the app on API 36 | P2 | E2 (route `uilr106`); panel 3/3 | `EditorWorkspaceView.java:1112-1117` | S, M | Low–Medium | — (in-project scope only) | none |
| 10 | OPP-10 (RC-12; H-01) | A renderer that stops mid-session is not reported until the next resume | P2 | E2; panel 3/3 | `ForgeShapeActivity.java`, `EditorWorkspaceView.java:6147-6155` | S, M | Low | — | none |
| 11 | OPP-12 (RC-13; I-01 = H-13) | A device self-test suite silently drops checks past its cap; PROJECT is at 256/256 | P2 latent | E1 (`cap_probe`); seat only (below the panel cut) | `Recorder::check` callers; caps in `forgeshape_jni.cpp` (`:821`) | S, H | Low | — | none; **must precede** any stage that adds PROJECT checks |
| 12 | OPP-11 (RC-13; G-01) | Two self-test oracles fail under FP contraction (arm64 default) | P2 (evidence) | E1 host `-mfma` + E2 asm; panel 3/3 | `forgeshape_history_selftest.cpp:474-480`, `forgeshape_cad_feature_selftest.cpp:5392-5394` | S, M | Low | — for (a); O: OD-17 for a pin | none |
| 13 | OPP-07 (RC-20; F-01) | Chrome hit areas below 48 dp wide (Flip 36.2 dp, Outline chips 37/39 dp, dimension label 43.4 dp) | P2 | E1 rects; panel 3/3 | `EditorControlStyles.java:366-382`, `SketchOrientationNavigatorView.java:134-137`, `DisplaySettingsPopoverView.java`, `SketchDimensionLabelView.java` | S, M | Medium (layout reflow; `uilr*` assertions) | — | best with OPP-33's sweep helper |
| 14 | OPP-G2 (RC-06; A-01 = X-A01) | "Save and continue" before Open Saved Project overwrites the slot it then opens | P2 | E2; panel 3/3 ×2 | `EditorWorkspaceView.java:2665-2678, 5458-5471` | S, M | Low | **O: OD-06 — OWNER-FIRST** | OD-06 |
| 15 | OPP-G3 (RC-08; F-03) | Bootstrap Back discards the drawn sketch while a panel is open | P2 | E2; panel 3/3 | `EditorWorkspaceView.java:1177-1184` | S, M | Low | **O: OD-08 — OWNER-FIRST** | OD-08; shares OPP-37 |
| 16 | OPP-31 (RC-13; G-02 = I-05) | Platform neutrality and 4392 native checks are not run by any CI workflow | P2 | E2; panel 2/3 | `.github/workflows/ci-fast.yml` | S, H | Low | **CI + O: OD-18** | OD-18 |
| 17 | C-06 (RC-14) | Core sketch device classes not re-verified on the audited tree | P2 (coverage) | panel 2/3. Partly closed: `40800569636` re-ran `SketchUxTest`, `CadVerticalSliceTest`, `CadExtrudeExtentTest` (`TESTS.md`) | evidence currency, no product file | S, H | Low | — (focused CI DEVICE); O: OD-03 only for an aggregate | none |
| 18 | A-03 → IDEA-05 (RC-06) | The saved project cannot be reopened from Home | P2 BLOCKED-OWNER | E2; panel 2/3 | `HomeView.java:47-79`, `ProjectSlot.java:63` | S, H | Low | **O: OD-07 — OWNER-FIRST** | OD-07 |
| 19 | A-07 (RC-06) | Leaving discards an open sketch or staged edit without a question | P3/INFO BLOCKED-OWNER | E2; panel 2/3 | `EditorWorkspaceView.java:2597-2602` | S, M | Low | **O: OD-10 — OWNER-FIRST** | OD-10 |
| 20 | OPP-40 (RC-15; H-05) | At-cap Finish runs 275–322 ms (host) synchronously under `g_stateMutex` | P2 risk | E1 host timing + E2; panel 2/3 | `cpp/forgeshape_jni.cpp:4400-4420, 1733, 5098` | L, L | High (threading) | **O: OD-19 — OWNER-FIRST** | OD-19; a release-ARM measurement |
| 21 | OPP-39 (RC-15; H-06) | Whole-mesh normal refresh and upload per stroke sample | P2 risk | E1 host + E2; panel 2/3 | `cpp/forgeshape_renderer.cpp:1119-1122, 749, 868` | L, L | Medium (renderer) | **O: OD-19 — OWNER-FIRST** | OD-19 |

### 1B. P3 defects (seat-graded; below the panel cut, not panel-verified)

| Rank | Id | Title | Sev. | Evidence | Root-cause file | Effort | Risk | Gate | Depends on |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 22 | OPP-13 (RC-07) | Freeform session and selection survive opening another project | P3 | E1 (`diag_d2`) | `cpp/forgeshape_jni.cpp` `loadProject` / `closeProject` | XS, H | Low | — | general fix OPP-35 |
| 23 | OPP-16 (RC-07; CC-17) | The JNI load wrapper cancels the sketch and moves the camera before a refusal | P3 | E2 | `cpp/forgeshape_jni.cpp` `loadProject` | S, M | Low | — | with OPP-13 |
| 24 | OPP-17 (RC-16; CC-19) | A PlanarFaces chain that does not regenerate passes validation but fails load | P3 | E1 (`pf_gap`) | `cpp/forgeshape_project_document.cpp:927-940` | S, M | Low | — (no format change) | OPP-12 if PROJECT checks are added |
| 25 | OPP-18 (RC-16; CC-18) | The codec accepts two encodings of one face token | P3 | E1 (`faceidx`) | `cpp/forgeshape_sketch.cpp:734-749` | S, H | Low | F (spec wording only; **lineage formula unchanged**; optional refusal fixture through the PowerShell builder) | OPP-12 |
| 26 | OPP-19 (RC-05) | Checkpoint fingerprint and bytes are read under two locks; clear/write race; `.pending` fallback is never read | P3 | E2 (timing E5) | `AutosaveController.java`, `ProjectSlot.java`, `ProjectCheckpoint.java` | M, M | Medium | — (OD-30 shapes the HJ-1 part) | OD-30 for that part |
| 27 | OPP-20 (RC-10) | Add/Cut offered on a full 16-record sketch table | P3 | E2 | `cpp/forgeshape_sketch_session.cpp:2895-2933` | XS, H | Low | — (OD-14 only for a policy change) | none |
| 28 | OPP-21 (RC-10; CC-08) | Clear Mask over the cap, and Freeform Symmetry ON / level over budget, are drawn and then refused | P3 | E2 | `cpp/forgeshape_sculpt.cpp:1687-1690`; Freeform availability | S, M | Low | — (OD-23 only for an alternative) | OPP-36 pattern |
| 29 | OPP-22 | Out-of-range Line length refused as `ZeroLengthLine` | P3 | E1 (P4) | `cpp/forgeshape_sketch_session.cpp:825-828` | XS, H | Low | — | none |
| 30 | OPP-23 (RC-18) | Freeform / Surface length rows seeded with unitless literals | P3 | E1 arithmetic + E2 | `FreeformEditorView.java`, `SurfaceEditorView.java`, `EditorWorkspaceView.java:6403-6416` | S, H | Low | — | none |
| 31 | OPP-24 (RC-13; H-03 = I-07) | Device `CAD_FEATURE_PERFORMANCE` line truncated at 1023 chars | P3 | E1 (`40797031704`) | `cpp/forgeshape_jni.cpp:608` | XS, H | Low | — | none |
| 32 | OPP-25 (G-04) | Self-tests read uninitialised floats after a failed projection | P3 | E1 (`-Wall -Wextra`) | `forgeshape_cad_feature_selftest.cpp`, `forgeshape_sketch_ux_selftest.cpp` | XS, H | Low | — | none |
| 33 | OPP-28 (D-07) | Resume Sculpt has no native sketch guard | P3 | E2 | `cpp/forgeshape_jni.cpp` resume entry | XS, M | Low | — | none |
| 34 | OPP-26 (I-11) | `EditorWorkspaceLegibilityTest` leaks `animator_duration_scale 0` on failure | P3 | E2 | androidTest `@After` | XS, H | Low | — | none |
| 35 | OPP-27 (A-08) | Open-picker launch failure says "Could not write the copy" | P3 | E2 | `EditorWorkspaceView.java:2090, 2652` | XS, H | Low | — | none |
| 36 | OPP-30 (A-11) | Raw refusal token on New Freeform; non-Sculpt projects described as "in Construction" | P3 | E2 | `EditorWorkspaceView.java:2448, 6190-6198` | XS, H | Low | — | none |
| 37 | OPP-29 (B-05) | Derived names at the 96-byte cap truncated mid-suffix | P3 | E2 | `cpp/forgeshape_body_commands.cpp` | S, M | Low | **O: OD-31 — OWNER-FIRST** | OD-31 |

---

## 2. Class 2 — quality, UX and architecture inside current contracts

None of these adds a feature, a format version or a library.

| Rank | Id | Title | Sev. | Evidence | Root-cause file | Effort | Risk | Gate | Depends on |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | OPP-34 (I-14, I-13) | Per-suite expected-count table, so a suite that loses checks fails; ledger counts into `summary.json` | — | E2 | `scripts/ci-device-smoke.sh` | S, H | Low | CI (script only) | OPP-12 |
| 2 | OPP-35 (RC-07) | One `resetProcessSessionsForProjectChange()` for load (after success) and close | — | E2 (+ E1 OPP-13) | `cpp/forgeshape_jni.cpp:8462-8480, 8968-8990` | M, M | Medium | — | trace the Java post-load path (U-2) first; absorbs OPP-13 / OPP-16 |
| 3 | OPP-36 (RC-10) | Availability asks the same predicate the refusal does | — | E2 | sculpt, sketch session, Freeform session | M, M | Low | — | OPP-20 / OPP-21 first |
| 4 | OPP-37 (RC-08) | Derive `hasDismissibleSurface` from the list `dismissTopmostSurface` walks | — | E2 | `EditorWorkspaceView.java:1112-1229` | S, M | Medium | O: OD-08 (bootstrap half), OD-28 (mode half) | OPP-08 |
| 5 | OPP-33 (RC-14; I-08 = F-09) | Device taps through hit testing; 48 dp sweep helper; `KEYCODE_BACK` per phase | — | E2 | `SketchTestSupport.java`, `WorkspaceTestSupport.java`, `EditorWorkspaceLegibilityTest.java` | M, M | Medium (Tier-5 trigger) | O: OD-03 (timing) | none |
| 6 | OPP-41 (RC-21) | One documentation batch: stale counts, tokens, fixtures, comments (CC-21..CC-49) | — | E2 | PROJECT_STATUS, README, `docs/CI_CLOUD.md`, ARCHITECTURE, PRODUCT, headers | S, H | Low | CC-12 text waits on OD-18 | after the code fixes it describes |
| 7 | OPP-42 | Remove 8 orphan natives and 2 dead helpers; probe stubbed `debug*` exports in the release guard | — | E2 | `cpp/forgeshape_jni.cpp`, `scripts/ci-release-selftest-guard.sh` | S, H | Low | — | none |
| 8 | OPP-38 (H-08) | Cache face-frame resolution, keyed by the producer revision | — | E3 (4.35 ms at depth 32) | `cpp/forgeshape_scene.cpp:281-326` | M, M | Medium (invalidation) | — | after OPP-01 / OPP-02 (same resolver) |
| 9 | OPP-44 (H-17, H-10, H-14) | Shader/C++ constant parity check; relative pick threshold; skip the equal-extent rebuild | — | E1 / E2 | `shaders/*`, `cpp/forgeshape_picking.cpp`, renderer | S each | Low–Medium | O: OD-19 for the H-10 threshold only | none |
| 10 | OPP-43 (I-09) | Replace `sleep(250)` settles with condition waits in the hottest classes | — | E2 | androidTest support | M, M | Low | — | none |
| 11 | OPP-45 (F-07) | TalkBack selected state and a polite live region | — | E2 | `EditorControlStyles.java:433-439`, `ToolRailView.java:242` | S, H | Low | O: OD-21 | OD-21 |
| 12 | OPP-32 (I-02) | Self-tests once per process | — | E1 (`40797031704`) | `cpp/forgeshape_jni.cpp:2083-2126` | S, M | Low | O: OD-26 | OD-26; precedes OD-03 |

## 3. Class 3 — IDEA ONLY (not authorised, not proposed for the next stage)

| Id | Idea | Gate | Size |
| --- | --- | --- | --- |
| IDEA-01 | World-plane retained sketch, A2 options 1–3 | O: OD-01; F: `CADB` v9 (not authorised) | L–XL |
| IDEA-02 | Desktop research spike (Linux, viewport-only PoC) | O: OD-04, OD-17; Li for S3 | L / XL |
| IDEA-03 | Android desktop-mode input | O: OD-04, OD-20 | M–L |
| IDEA-04 | Multi-document context (per-document allocator) | O: OD-13 | L |
| IDEA-05 | Open Saved Project from Home | O: OD-07 (also Class 1 rank 18) | S |
| IDEA-06 | Real-stylus hover preview in Sculpt | needs an autosave-suspend concept | M |
| IDEA-07 | Draw-on-change frame pacing | O: OD-19 | M |
| IDEA-08 | Release-variant runtime smoke; arm64 automated run | O / CI: OD-18 | S–M |
| IDEA-09 | Accessibility specification | O: OD-21 | M |
| IDEA-10 | Write-side `.forge` size limit | O: OD-24 | S |
| IDEA-11 | Revolve Add/Cut, later Revolve, curved-surface trim, T-Spline refinement, NURBS | each its own stage; most need a format version | XL |

**Counts.** Class 1 = **37** (1A: 21, of which 20 were panel-verified and 1 is seat-graded;
1B: 16 P3 rows). Class 2 = **12**. Class 3 = **11**.

---

## 4. Task slices (small, independently executable)

Each slice is one task branch and one review. There is no combined prompt. "Host" means
`bash scripts/host-native-selftests.sh` (plus `host-forge-corpus-verdicts.sh` where the codec
is touched). "JVM" means the unit tests. "Device" means a focused `CI DEVICE` dispatch of the
named classes, and is never cited as physical or aggregate evidence. Every slice keeps all 72
corpus verdicts and all 25 startup tokens unchanged unless it says otherwise.

| Slice | OPP | Scope | Files | Acceptance | Tests |
| --- | --- | --- | --- | --- | --- |
| **T-01** | OPP-01 | Export takes `Model` from `resolveWorldModel`, splits it into `T` and `L` as today, and refuses by name when it does not resolve | `forgeshape_gltf_export.cpp` (+ `.h` doc line) | `cad_face_sketch_cap_v2` body 2 exports at `(1.5,0.5,0)`; `_side_v2` with its rotation; `_chain_v2` at `y=1.0` / `1.5`; every independent-body export is byte-identical before and after | Host GLTF_EXPORT (93/128, room for new checks); device `GlbExportTest` |
| **T-02** | OPP-02 | Refuse a transform write and gizmo activation on `isFaceSupportedCad` below JNI with a named status; withdraw Transform and the gizmo above JNI; correct `ARCHITECTURE.md:2606-2608` only if the code now matches it | `forgeshape_jni.cpp` (`applyBoxTransform`, gizmo path), `forgeshape_gizmo.cpp`, `EditorWorkspaceView.java`, `WorkspaceTrailingHostView.java` | Write refused, undo depth and fingerprint unchanged, no gizmo, no Transform entry; an independent body is unaffected | Host SCENE (175/256) or GIZMO (176/256); JVM presentation if a predicate moves; device `EditorWorkspaceGizmoTest` plus one new face-supported case |
| T-03 | OPP-03 | `importGlbDurable` refuses while a sketch or chooser is active; the drawer row is withdrawn | `forgeshape_jni.cpp`, `forgeshape_import_commit.*`, `EditorWorkspaceView.java:2823, 5835-5859` | Import during a sketch: refused, bodies, active body and undo depth unchanged, sketch still active; row absent | Host GLTF_IMPORT; device `ImportGlbSculptGuardTest` plus a sketch case |
| T-04 | OPP-04 | Clear `undo_` on NotRetained; rename `SCUNDO_23`; add `SCUNDO_23b` with a non-empty undo stack | `forgeshape_sculpt_history.cpp`, `forgeshape_sculpt_selftest.cpp` | Undo after an over-cap stroke is refused (`NothingToUndo`); the stroke stays applied | Host SCULPT (cap 768); device `SculptUndoTest` |
| T-05 | OPP-05 | Add `,` to three key listeners (optionally one shared factory) | three Java views | Typing `2,5` gives 2.5 in each editor | JVM `LengthUnitTest`; device: one typed-comma case |
| T-06 | OPP-06 (a) | `beginSketchView` frames `viewFrame()`; view state is reset on `backToEditing` / Edit Sketch | `forgeshape_jni.cpp:313-320`, `forgeshape_sketch_session.cpp` | Flip and ±90° change the camera pose; Back to Sketch reinstalls the aligned view | Host SKETCH_UX (118/256); device `SketchUxTest` with a camera-pose assertion (removes CC-47's false green) |
| T-07 | OPP-07 | `chip()` gets a minimum width equal to the control height; width floor for the navigator Flip | `EditorControlStyles.java`, `SketchOrientationNavigatorView.java`, `DisplaySettingsPopoverView.java`, `SketchDimensionLabelView.java` | Every listed control ≥ 48 dp in both axes at font 1.0 / 1.3 / 1.5 | Device `EditorWorkspaceLegibilityTest`, `Ui3dStateAuditTest`, visual-evidence classes |
| T-08 | OPP-08 | Add the Revolve term (in-project only; leave the bootstrap order for OD-08) | `EditorWorkspaceView.java:1112-1117` | API 36 Back during Revolve returns to the extrusion | Device: a `KEYCODE_BACK` case like `uilr106` in `CadRevolveOwnerTest` |
| T-09 | OPP-09 + OPP-14 | Move `recoveryResolved` semantics, `everPersisted` and `persistedFingerprint` into `EditorUiState` | `EditorWorkspaceView.java`, `EditorUiState.java` | After a palette change: no unsaved question on a saved project, and no recovery question over a live project | JVM if extractable; device `SettingsPreferencesTest` and `ProjectAutosaveRecoveryTest` cases |
| T-10 | OPP-10 | Poll `rendererLifecycle()` in the existing refresh path | `ForgeShapeActivity.java`, `EditorWorkspaceView.java` | Injected loss to RestartRequired shows the message without a resume | Device `DiagnosticsAndRendererLossTest` via `debugInjectDeviceLoss` (never a real loss) |
| T-11 | OPP-11 (a) | Same-expression or tolerant oracles; a non-tied lens fixture | two selftest files | Host 0 failed with and without `-mfma` | Host, both ways |
| T-12 | OPP-12 (a) | `count == cap` fails the suite; raise the PROJECT and CAMERA caps | `forgeshape_jni.cpp` suite runners | A suite that hits its cap reports FAIL by name | Host; one device startup (25 tokens) |
| T-13 | OPP-15 | Report "nothing was written" and delete the empty document | `EditorWorkspaceView.java:5778, 6004, 6050-6055` | No silent empty file | Device `ProjectTransferTest` |
| T-14 | OPP-13 + OPP-16 | End the Freeform session on load and close; cancel session state only after load succeeds | `forgeshape_jni.cpp` | A refused load leaves the sketch and camera unchanged; a Freeform session does not cross projects | Host PROJECT (**needs T-12 first**: 256/256) or a JNI-level device case in `JniBoundaryHardeningTest` |
| T-15 | OPP-17 | Regenerate chains with later features during validation; correct two comments | `forgeshape_project_document.cpp`, `forgeshape_project_state.h` | `pf_gap` refused at validate, not at load | Host (after T-12); corpus verdicts unchanged |
| T-16 | OPP-18 | Decode-side bound `edgeLocalIndex ≤ 0xFFFF`; spec sentence in §7c | `forgeshape_sketch.cpp`, `DATA_PACKAGE_SPEC.md` | The two-encoding probe is refused; 72 fixtures unchanged | Host, corpus verdicts; CI FAST parity |
| T-17 | OPP-20 | Gate Add/Cut on table size; name `TooManySketches` | `forgeshape_sketch_session.cpp` | Full table: Add/Cut not offered | Host CAD_FEATURE (638/1024) |
| T-18 | OPP-21 (a) | `canClearMask` includes the size test; Freeform symmetry and level availability ask the refusal predicate; correct `ARCHITECTURE.md:3678-3679` | `forgeshape_sculpt.cpp`, Freeform session | No drawn-then-refused control | Host SCULPT, FREEFORM |
| T-19 | OPP-22 + OPP-24 + OPP-25 | Three XS native hygiene fixes | `forgeshape_sketch_session.cpp:825-828`, `forgeshape_jni.cpp:608`, two selftests | `OutOfRange` named; performance line < 1000 chars; `-Wall` clean | Host; one device startup log |
| T-20 | OPP-23 | Unit-aware seeding and captions for Freeform / Surface rows | the two editor views | 0.25 m shows as 250 mm in mm | JVM; device `FreeformOwnerTest`, `SurfaceOwnerTest` |
| T-21 | OPP-26 + OPP-27 + OPP-30 | Test hygiene and two user strings | test `@After`; `EditorWorkspaceView.java` | Strings correct; setting restored after a failure | JVM; device `EditorWorkspaceLegibilityTest` |
| T-22 | OPP-28 | Native `in_sketch` refusal on Resume Sculpt | `forgeshape_jni.cpp` | Refused by name | Device `Stage027SculptWorkflowTest` |
| T-23 | OPP-41 | Documentation batch | owner docs | Every CC-21..CC-49 stale row corrected | none (docs) |

## 5. Leave alone (correct behaviour with strong tests)

Do not refactor these opportunistically. Where a positive claim was judged **overstated**
(`work/VERIFICATION.md`), the behaviour is still correct at E2. Overstated means its proof
skips the real UI path, which is a coverage note and not a reason to touch the code.

- **Representation exclusivity and the Sculpt boundary.** CAD, Freeform and Surface never
  become a sculpt source; `SCUL` over `CADB` / `FRFM` / `SURF` is refused. This positive claim
  **stood**.
- **Freeform Symmetry.** Bit-exact reflection; an on-plane vertex stays on the plane. This
  positive claim **stood**.
- **Import in Sculpt guard (FC-R1 D1).** Closed and pinned by `ImportGlbSculptGuardTest`, green
  in `40800569636`.
- **Overlay revision on a camera-only rebuild (FC-R1 D5).** Closed and pinned by
  `CADFC1_S4_a_a_zoom_rebuild…`.
- **The `.forge` codec core and corpus.** 72/72 parity in CI FAST `40796992679` and in host
  corpus verdicts. Fix only the two narrow holes (OPP-17, OPP-18). **Do not change the CAD-A3
  lineage formula** (`DATA_PACKAGE_SPEC.md` §7c; the PowerShell builder re-implements it).
- **GLB export bake for independent bodies.** The `T`/`L` split and the `MirroredTransform` /
  `SingularTransform` refusals (GLTF_EXPORT 93/0). T-01 changes only where `Model` comes from.
- **Sculpt history caps, navigator and mask.** SCULPT 655/0; `SculptUndoTest`,
  `SculptHistoryNavigatorTest`. Only the NotRetained branch changes (T-04).
- **CAD region and face selection, the arrangement and the Revolve solid.** CAD_FEATURE 638/0,
  17 geometry oracles; `CadVerticalSliceTest` and `CadExtrudeExtentTest` green on this tree.
- **Mirror** (MIRROR 62/0, `MirrorSmokeTest`), **object commands** (`ObjectCommandsTest`,
  `ObjectsDeleteTest`), **body dimensions** (BODY_DIMENSIONS 102/0) and **preferences byte
  identity** (`SettingsPreferencesTest` 15/15) are all green in `40805804385`.
- **Device and release guards.** `scripts/verify-device-guards.ps1`, the release self-test guard
  (PASS) and the emulator port and serial rules.
- **Build flags and toolchain.** NDK `29.0.14206865`, no `-ffp-contract` pin (OD-17), Manifold
  vendored unmodified.

---

## Best next bounded stage

**Recommended: `CAD-FACE-SUPPORTED-PLACEMENT-R1`.** It is not started. It closes root cause
RC-01: OPP-01 (export) and OPP-02 (transform and gizmo guard), which are slices T-01 and T-02.

**Rationale**

- RC-01 holds the only **schedulable P1**: OPP-01, E1, panel 3/3 twice, with no OWNER gate. The
  other P1, OPP-G1, waits on OD-05.
- It is the highest-ranked P2 with the same cause: OPP-02, E1 host, panel 3/3.
- Both are one root cause. Consumers read `body.transform()` instead of
  `ConstructionScene::resolveWorldModel`, and nothing outside Duplicate reads
  `isFaceSupportedCad`. One stage closes both.
- Both are explicit CLAUDE.md rules: "exports EVERY body", and CAD-A3 "NOT independently
  transformable". So **no OWNER decision, no format change, no new fixture, no library and no
  CI change** is needed (CC-02, CC-03: "OWNER? no").
- Effort is two S slices. There is headroom in the suites they touch: GLTF_EXPORT 93/128, SCENE
  175/256, GIZMO 176/256 and CAD_A3 66/128. It does **not** touch PROJECT (256/256), so it does
  not wait on T-12.
- A wrong GLB is the one defect class that leaves the product silently: a receiving tool shows
  a wrong assembly with no error.

**Scope**

1. T-01: `captureGlbExportScene` derives each body's `Model` through `resolveWorldModel`, splits
   it into node `translation` and baked `L` exactly as `ARCH-OWNER-07` requires, and refuses by
   a named status when a face-supported placement does not resolve. It does not fall back to
   the stored transform.
2. T-02: a named refusal below JNI for a transform write and for gizmo activation on a
   face-supported CAD body. Transform and the gizmo are withdrawn above JNI ("a control that
   cannot succeed is not drawn"). The B-03 sentence (`ARCHITECTURE.md:2606-2608`) becomes true,
   and that is the only owner-doc edit, plus the `PROJECT_STATUS.md` entry the stage owes.

**Explicit non-scope**

- No decoder change for an already stored non-identity transform (OD-16). Such files keep
  loading.
- No "offset from face" placement.
- No face-frame cache (OPP-38).
- No other export change: Sculpt export, OBJ / FBX, and node `matrix` stay out.
- No change to `resolveWorldModel`, the lineage token, `CADB`, any fixture or any startup
  token.
- No fix of the other Class 1 items (import guard, RC-04, navigator). Each is its own slice.
- No FullSharded run and no physical-device claim.
- It does **not** replace or pre-empt the OWNER review (OD-27). It is built on a task branch
  from the tree carrying `70a6c14`'s product, and it **merges nothing** until the OD-27 verdict
  and the branch rules in `PROJECT_STATUS.md` Next Stage allow it.

**Acceptance criteria**

1. Export of `cad_face_sketch_cap_v2` writes body 2's node translation as `(1.5, 0.5, 0)`, and
   its baked positions equal the drawn positions. `cad_face_sketch_side_v2` carries its face
   rotation in the baked `L`. `cad_face_chain_v2` bodies 2 and 3 sit at `y = 1.0` and `1.5`. All
   compared against `resolveWorldModel` within the suite's existing float tolerance.
2. Every export of a project without face-supported bodies is **byte-identical** before and
   after (host GLTF_EXPORT digest checks).
3. On a face-supported body:
   - a transform write returns a named refusal;
   - undo depth, fingerprint and checkpoint state are unchanged;
   - the gizmo does not activate;
   - the Transform entry and gizmo are absent in the chrome.

   An independent body and a producer that has dependents still transform normally.
4. The resolved world model of the dependent is unchanged by any attempted write.
5. Host `HOST_SELFTESTS_OK` with 0 failed. Every touched suite stays below its cap, and the
   count is recorded in `TESTS.md`.

**Test gate**

- **Host:** `bash scripts/host-native-selftests.sh` (all 25 suites, 0 failed, new checks in
  GLTF_EXPORT and SCENE or GIZMO), and `host-forge-corpus-verdicts.sh` (72 verdicts unchanged).
- **JVM:** the unit suite with `--rerun-tasks` (196/0/0 baseline in `TESTS.md`, plus any
  presentation test added for the withdrawn Transform entry).
- **Release self-test guard:** PASS.
- **CI FAST:** green, with corpus parity 72/72.
- **Focused CI DEVICE:** dispatch `GlbExportTest`, `EditorWorkspaceGizmoTest`,
  `SpatialSketchTest`, `ObjectCommandsTest`, `CadVerticalSliceTest`, and the new face-supported
  transform case. Expect 25/25 startup tokens in order. This is emulator-only evidence.
- **Not in the gate:** FullSharded. It runs only if the OWNER gates it (OD-03). Physical-device
  evidence stays with OD-27.
