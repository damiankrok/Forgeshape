# FUNCTION-COUNCIL-R2 — Opportunities (catalogue)

Tree `70a6c14` (`app/src/main` = `f3db953`). This file is the **catalogue**: one
card per root cause, with the fields each candidate needs. The ORDER, the small
task slices, the "leave alone" list and the recommended next stage are in
`PRIORITISED_BACKLOG.md`. Decisions only the OWNER can make are in
`OWNER_DECISIONS_REQUIRED.md` (ids `OD-nn`). Findings, root causes (`RC-nn`) and
contract rows (`CC-nn`) are defined in `COUNCIL_DELIBERATION.md`,
`CONTRACT_CONFLICTS.md` and the seat files; they are linked here, not restated.
Fresh test executions and CI run ids are owned by `TESTS.md` (coordinator, not
yet written when this file was synthesised). Run ids below are the ones the
seats cite; the evidence folders under `evidence/` hold the downloaded copies.

## 0. Standing of every finding (read first)

- **`work/VERIFICATION.md` did not exist at synthesis time** (checked again for
  this file: `ls artifacts/function-council-r2/work/` lists only `delib_*.md`
  and `req_*.tsv`). So **no finding here is labelled "survived adversarial
  verification"**. A finding is entered as a class-1 defect only when all of
  these hold:
  1. it is DEFECT-CONFIRMED after Round 2 (`COUNCIL_DELIBERATION.md` §1, §4);
  2. it was not withdrawn or regraded to BLOCKED-OWNER / FUTURE-NOT-DUE
     (`COUNCIL_DELIBERATION.md` §0 table);
  3. its grade is E1 or E2 (no E5-only defect is listed).
- For seven of them the synthesis re-read the cited source on this tree
  ("synth re-read", E2): `forgeshape_gltf_export.cpp:177-186` (export reads
  `body.transform()`), `forgeshape_jni.cpp:313-320` (`beginSketchView` frames
  `frame()`), `forgeshape_jni.cpp:8586-8600` (`importGlbDurable` guards Sculpt
  only), `forgeshape_sculpt_history.cpp:91-97` (`NotRetained` keeps `undo_`),
  the three `DigitsKeyListener("0123456789.-")` sites, `EditorWorkspaceView.java:5675-5679`
  (Recover retires the checkpoint), and `isFaceSupportedCad` callers (only
  `forgeshape_body_commands.cpp:205` and the JNI query at `forgeshape_jni.cpp:7679`).
- **When `VERIFICATION.md` is written**, any finding it does not uphold must be
  relabelled hypothesis / withdrawn in this file and in the backlog.
- Withdrawn or regraded, and therefore NOT listed as defects: B-07, the C-03
  Keep Sketch sub-claim, the J-05 revolve-axis half, J-01 (→ OD-12), J-03 / G-06
  (→ OD-13), E-08 (→ OD-11), A-03 (→ OD-07), A-07 (→ OD-10), H-05 / H-06 / H-07
  (→ OD-19, no budget recorded).

## 1. Field legend

| Field | Meaning |
| --- | --- |
| Link | finding ids (seat), `RC-nn`, `CC-nn`, requirement row in `work/req_*.tsv` where the seat named one |
| Horizon | H0 / H1 / H2, with the repo source (no Master Plan available: `BEFORE.md`) |
| Evidence | grade + the reproducer or file:line; E1 only when produced in this audit |
| Root-cause owner | the file that must change |
| Android effect | what a user on the shipped product sees |
| PC relevance | relevance to a future desktop client (H2, unauthorised: `ARCHITECTURE.md:195-201`) |
| Effort | XS (< 2 h) · S (≤ 1 day) · M (2–4 days) · L (1–2 weeks) · XL (> 2 weeks), with confidence H/M/L |
| Decisions | format / licence / CI / OWNER decisions needed before the change |

---

## CLASS 1 — Verified defects and unsatisfied already-approved behaviour

There is **no P0** (no crash, no corruption of a stored `.forge`, no silent loss
of saved data proven at E1). Ordering is in `PRIORITISED_BACKLOG.md` §1.

### OPP-01 — Export writes a face-supported CAD body at the origin (P1)

| Field | Value |
| --- | --- |
| Link | E-01 (= B-02), RC-01, CC-03; TST-060 (export bake tested only for independent bodies) |
| Horizon | H0 — CLAUDE.md Export rule "it exports EVERY body"; `forgeshape_gltf_export.h:141` "the exported place is bit-for-bit the place drawn"; CAD-A3 derived placement |
| User problem | A body built on a face of another body (any New Body from a face sketch) exports at `(0,0,0)`, unrotated, with no error. The receiving tool shows a wrong assembly. |
| Evidence | E1 (seat E `export_face`): `cad_face_sketch_cap_v2` body 2 exported `(0,0,0)`, drawn `(1.5,0.5,0)`; `cad_face_sketch_side_v2` `(0,0,0)` vs `(1,0,1)` + rotation; `cad_face_chain_v2` bodies 2/3 vs `y=1.0`/`1.5`. Re-run by seat I on seat B's binary (`diag_face_supported`, export body 2 `(0,0,0)`). Synth re-read: `forgeshape_gltf_export.cpp:177-186` reads `body.transform()`. |
| Root-cause owner | `app/src/main/cpp/forgeshape_gltf_export.cpp` (`captureGlbExportScene`); placement truth is `ConstructionScene::resolveWorldModel` (`forgeshape_scene.cpp:277-330`) |
| Alternatives | (a) take `Model` from `resolveWorldModel`, split `T` / `L` as today, refuse by name when it does not resolve — consistent with CLAUDE.md; (b) refuse face-supported bodies from export — contradicts "exports EVERY body"; (c) bake into a node `matrix` — contradicts ARCH-OWNER-07 ("translation and nothing else"). Recommended: (a). |
| Android effect | Wrong GLB for every face-supported body; the `.forge` is unaffected. |
| PC relevance | High: the export path is platform-neutral and would ship unchanged in any desktop client. |
| Effort | S, confidence H |
| Risk | Low. `L` of a face frame is a proper rotation (det +1), so `MirroredTransform` / `SingularTransform` refusals stay meaningful; the gate is "every other fixture exports byte-identical before/after". |
| Prerequisites | none. GLTF_EXPORT has headroom (93 of cap 128, `forgeshape_jni.cpp:671`). |
| Decisions | none (CC-03 "OWNER? no"). No format change. |
| Best next slice | T-01 (`PRIORITISED_BACKLOG.md` §4); part of the recommended stage. |

### OPP-02 — A face-supported CAD body accepts transform writes and a gizmo (P2)

| Field | Value |
| --- | --- |
| Link | B-01, B-03 (DOC-DIVERGENCE `ARCHITECTURE.md:2606-2608`), RC-01, CC-02, TST-076 (no test) |
| Horizon | H0/H1 — CLAUDE.md CAD-A3 "such a body is NOT independently transformable (`SceneObject::isFaceSupportedCad`)" |
| User problem | Transform is offered; the gizmo is drawn away from the body; a typed or dragged value records an Undo step, marks the project unsaved and writes a checkpoint, while the body does not move. |
| Evidence | E1 (seat B `diag_face_supported`, re-run by seat I): `gizmo visible=1`, `applyStatus=Applied`, `undoDepth 0->1`, `fingerprintChanged=1`, resolved model still `(0,0,3)`. Synth re-read: `isFaceSupportedCad` is read only by Duplicate and an unused JNI query. Not reproduced on a device. |
| Root-cause owner | `forgeshape_jni.cpp` `applyBoxTransform` (`:1335-1385`) and the gizmo activation / touch path (`:9932-9934`); `forgeshape_gizmo.cpp` `beginDrag`; Java `EditorWorkspaceView.java:3028-3033` (gizmo `offered`), `WorkspaceTrailingHostView.java:605-620` (Transform entry) |
| Alternatives | (a) refuse below JNI with a named status and withdraw above it — the rule as written; (b) an "offset from face" placement — new durable semantics, a format change, H2. Recommended: (a). |
| Android effect | Confusing no-op instrument; spurious dirty state; non-canonical transform bytes in `.forge` (`validateProjectDocument` checks only `transformValid`, `forgeshape_project_document.cpp:739`). |
| PC relevance | Medium: the guard is domain/JNI; a desktop shell would inherit the fixed domain guard. |
| Effort | S, confidence M (two layers + one device case) |
| Risk | Low; existing files that already carry a non-identity stored transform still load (the decoder question is OD-16, out of scope). |
| Prerequisites | OPP-01 in the same stage is preferred (one root cause, RC-01). |
| Decisions | none for the guard. OD-16 only if the decoder should also refuse such bytes. |
| Best next slice | T-02; part of the recommended stage. |

### OPP-03 — Import GLB is offered and accepted while a sketch or the support chooser is open (P2)

| Field | Value |
| --- | --- |
| Link | A-04 = C-02 = D-02 = E-03, ADV-046, RC-02; recorded debt in `FUNCTION-COUNCIL-C1-D1` (not a regression) |
| Horizon | H0 — CLAUDE.md CAD-R0-A1A2 "creation, deletion, body switching … are refused below JNI and withdrawn above it" while a sketch is drawn |
| User problem | Mid-sketch, the mark's drawer still offers Import; the import creates bodies, switches the active body and records a Construction step under the open sketch. |
| Evidence | E1 twice: seat A `diag_import_during_sketch` (`commit=Ok bodies 1→2 active 1→2 undo_depth=1`), seat D `diag_d6` (sketch still active). E2 for JNI/Java. Synth re-read of `forgeshape_jni.cpp:8586-8600`. |
| Root-cause owner | `forgeshape_jni.cpp` `importGlbDurable`; `EditorWorkspaceView.java:2823` (`showImportAvailable(!sculpting)`) and `:5835-5859` (picker result) |
| Alternatives | (a) refuse `RefusedInSketch` when `sketchSession().active() || supportChooser().active()` under the same lock, and withdraw the row; (b) end the sketch and then import — contradicts sketch volatility (would cancel user work). Recommended: (a). |
| Android effect | A Construction step inside a sketch; the staged commit still targets `targetBodyId_` (C-02, hypothesis for the UI path). |
| PC relevance | Low-medium (domain guard reused). |
| Effort | S, confidence H |
| Risk | Low. New `ImportCommitStatus` value is code, not format. |
| Prerequisites | none |
| Decisions | none (seat E OD-3 offered "allow as end of sketch"; the rule already answers it). |
| Best next slice | T-03 |

### OPP-04 — Undo after an over-cap (NotRetained) stroke lands on a state that never existed (P2)

| Field | Value |
| --- | --- |
| Link | D-01, FC-R1 D4, RC-11; CC-46 (`SCUNDO_23` misnamed) |
| Horizon | H0 — ARCH-OWNER-12 (Sculpt history walks real states; Undo cannot cross a Freeze) |
| User problem | After a stroke touching > ~37,449 vertices, Undo produces a mesh that mixes two states and clears `hasEdits`; Reset Sculpt then discards visible work without the confirmation (`SculptContextView.java:243`). |
| Evidence | E1 (`diag_d3.out`): `undo_lands_on_a_state_that_existed=0`, `hasEdits=0`. Synth re-read `forgeshape_sculpt_history.cpp:91-97`. |
| Root-cause owner | `forgeshape_sculpt_history.cpp` `SculptHistory::record` |
| Alternatives | (a) also clear `undo_` on NotRetained (Undo cannot cross an unretained stroke, as it cannot cross a Freeze); (b) refuse the stroke (OD-22). Recommended default: (a). |
| Android effect | Only on dense imported meshes with a large radius. |
| PC relevance | Domain code, shared. |
| Effort | XS, confidence H (+ host check `SCUNDO_23b` with a non-empty undo stack; SCULPT cap 768 has room) |
| Risk | Low; the stroke itself still applies, as CLAUDE.md requires. |
| Decisions | OD-22 only if (b) is preferred. |
| Best next slice | T-04 |

### OPP-05 — Three canvas editors drop the decimal comma: "2,5" becomes 25 (P2)

| Field | Value |
| --- | --- |
| Link | F-05, ADV-067, CC-44, RC-09 |
| Horizon | H1 — same feature accepts `,` in the precision surface (`NumericPropertyRow.java:109`); `LengthUnit.parse` reads `,` as `.` |
| User problem | An order-of-magnitude error in authored truth (extrude depth, second distance, Line length, body dimension) with no visible refusal. |
| Evidence | Defect E2 (synth re-read: `CadExtrudeCanvasView.java:644`, `SketchDimensionLabelView.java:112`, `BodyDimensionLabelsView.java:127` use `"0123456789.-"`; four other fields allow `,`); parser E1 (`LengthUnitTest.acceptsACommaDecimalSeparatorAndASignedValue`, fresh JVM). Which IMEs emit `,` under `TYPE_NUMBER_FLAG_DECIMAL` is unverified (H-F2). |
| Root-cause owner | the three Java views above |
| Alternatives | (a) add `,` to the three sets; (b) one shared factory for numeric key listeners so the sets cannot drift (Class-2 follow-up). |
| Android effect | Locale-dependent wrong values. |
| PC relevance | Low (Android IME specific). |
| Effort | XS, confidence H (+ one device case typing `2,5`) |
| Decisions | none |
| Best next slice | T-05 |

### OPP-06 — Orientation navigator Flip and ±90° do not move the camera (P2)

| Field | Value |
| --- | --- |
| Link | C-01 = F-02 = FC-R1 D2, RC-03, CC-01, CC-27, CC-47 (false-green `SketchUxTest:199-233`) |
| Horizon | H1 — CLAUDE.md SKETCH-UX-R1 C ("`SketchSession::viewFrame` derives the camera's frame") |
| User problem | Two drawn controls change only their label. |
| Evidence | E1 (seat C probe P1: `frame()_unchanged_view_after_flip=1 … viewFrame()_differs=1`) + E2 (synth re-read `forgeshape_jni.cpp:318-319`); E1 screen diff (seat F, consistent, not proof alone). |
| Root-cause owner | `forgeshape_jni.cpp` `beginSketchView`; `SketchUxTest` (asserts state bits only) |
| Alternatives | (a) frame `viewFrame()` and reset view state on `backToEditing` / Edit Sketch, keeping CAD-UX-S1-C1's "exact aligned view"; (b) frame `viewFrame()` and keep flip/roll across those paths; (c) withdraw the two controls until decided. (a) vs (b) is OD-09. |
| Android effect | Non-functional chrome in every sketch. |
| PC relevance | Low. |
| Effort | S, confidence M (11 `beginSketchView` callers; add a camera-pose assertion) |
| Risk | Medium: touches the sketch view contract CAD-UX-S1-C1 pins. |
| Decisions | OD-09 before shipping (b); (a) needs none. |
| Best next slice | T-06 |

### OPP-07 — Chrome hit areas below 48 dp in width (P2)

| Field | Value |
| --- | --- |
| Link | F-01, RC-20, CAD-024; coverage gap F-09 = I-08 |
| Horizon | H0 — CLAUDE.md "48 dp is the interactive floor … it is the HIT AREA" |
| User problem | Mis-taps; `sketch_navigator_flip` 36.2 dp sits 4 dp from two 48 dp arrows. |
| Evidence | E1 (rects in this audit's CI DEVICE captures, see `TESTS.md`): Flip 95×126 px = 36.2 dp; Selection Outline On/Off 37.0 / 39.2 dp; `sketch_dimension_label` 43.4 dp. Cause E2: `EditorControlStyles.chip()` (`:366-382`) sets only a minimum height. |
| Root-cause owner | `EditorControlStyles.java`, `SketchOrientationNavigatorView.java:134-137`, `DisplaySettingsPopoverView.java:230-299`, `SketchDimensionLabelView.java:79-90` |
| Alternatives | (a) `setMinimumWidth(control_height)` in `chip()` plus a width floor for the navigator's weighted Flip; (b) per-site padding. (a) is one change. |
| Android effect | Precision of touch on the smallest chips. |
| PC relevance | None directly. |
| Effort | S, confidence M (layout reflow across palettes / font scales; re-run the visual-evidence classes) |
| Risk | Medium: widths reflow the navigator and Display popover; `uilr*` layout assertions may move. |
| Decisions | none |
| Best next slice | T-07 (with the sweep helper, OPP-21) |

### OPP-08 — On API 36 / targetSdk 36, Back during an in-project Revolve backgrounds the app (P2)

| Field | Value |
| --- | --- |
| Link | F-04 = CAD-115, RC-08, CC-06 (b) |
| Horizon | H0/H1 — APP-H1 "Back is one step"; PRODUCT.md "Back returns to the extrusion" |
| User problem | Same key means two things by API level; on current Android the app leaves with a revolve staged. |
| Evidence | E2 (`hasDismissibleSurface` `EditorWorkspaceView.java:1112-1117` has no revolve term; the callback is armed only while it is true, `ForgeShapeActivity.java:121-160`); route evidence `uilr106` (fresh PASS). API 33–35 UNVERIFIED (E5). Not reproduced. |
| Root-cause owner | `EditorWorkspaceView.java` (`hasDismissibleSurface` vs `dismissTopmostSurface`) |
| Alternatives | (a) add the revolve term; (b) derive `hasDismissibleSurface()` from the same ordered list `dismissTopmostSurface()` walks (prevents the next divergence — palettes, editors). Recommended: (b), scoped to non-bootstrap phases so OD-08 is not pre-empted. |
| Android effect | Lost staged revolve intent (not project data). |
| PC relevance | None. |
| Effort | S, confidence M (+ a `KEYCODE_BACK` device case as `uilr106` does) |
| Decisions | none for the revolve term; the bootstrap order is OD-08. |
| Best next slice | T-08 |

### OPP-09 — The recovery question can reappear over the live project after a recreation (P2)

| Field | Value |
| --- | --- |
| Link | E-02, RC-04, CC-05 |
| Horizon | H0 — PRODUCT.md:2027 "it asks — once"; CLAUDE.md "Nothing replaces the live project without the user saying so" |
| User problem | After a palette change or system dark-mode toggle, "Recover / Discard" stands over the open project; Recover reloads the checkpoint and clears the live Undo history. |
| Evidence | E2 (`recoveryResolved` set only when a candidate existed, `EditorWorkspaceView.java:1027-1029`, `:5579-5582`; `EditorUiState.java:151`) + E3 mechanism (`fsr1b04`). "Edits lost" is E5. |
| Root-cause owner | `EditorWorkspaceView.java`, `EditorUiState.java` |
| Alternatives | (a) mark resolved after the first offer in a process whatever its outcome; (b) offer only when `!NativeViewport.projectOpen()`. (b) is stricter and matches "never over the live project". |
| Android effect | A destructive question in the wrong context. |
| PC relevance | Low. |
| Effort | XS–S, confidence H |
| Decisions | none (CC-05 "no"); seat E's (a)/(b) choice is an implementation choice. |
| Best next slice | T-09, together with OPP-14 (same root cause RC-04) |

### OPP-10 — A renderer that stops mid-session is not reported until the next resume (P2)

| Field | Value |
| --- | --- |
| Link | H-01, RC-12, CC-43 (stale javadoc) |
| Horizon | H0 — CLAUDE.md GPU rule ("rather than presenting a black viewport") |
| User problem | A dead viewport with no message until the user leaves and returns. Autosave still protects edits. |
| Evidence | E2 (only production caller of `checkRendererLifecycle` is `onResume`, `ForgeShapeActivity.java:336`; the render loop only stores an atomic, `forgeshape_jni.cpp:2049-2066`). Not reproduced (the debug injection seam is the supported route). |
| Root-cause owner | `ForgeShapeActivity.java`, `EditorWorkspaceView.java:6147-6155` |
| Alternatives | (a) poll `rendererLifecycle()` in the existing refresh path; (b) a low-rate Choreographer tick; (c) a native → Java callback (new JNI upcall, more surface). Recommended (a) or (b). |
| Effort | S, confidence M (device case through `debugInjectDeviceLoss`, never a real loss on the emulator) |
| Decisions | none |
| Best next slice | T-10 |

### OPP-11 — Two self-test oracles fail under FP contraction (arm64 debug default) (P2, evidence)

| Field | Value |
| --- | --- |
| Link | G-01, RC-13 |
| Horizon | H0 — the 25-token clean launch is the stated evidence contract (CLAUDE.md "Commands") |
| User problem | None for the product; an arm64 debug launch is expected to print `FORGESHAPE_CONSTRUCTION_HISTORY_SELFTEST_FAIL`, which can hide a real FAIL. |
| Evidence | E1 host (`clang++ -O1 -mfma` → `HOST_SELFTESTS_FAIL (4392 checks, 2 failed)`; `-ffp-contract=off` → 0 failed) + E2 arm64 assembly (`fmadd` at `arm64_forgeshape_history_selftest.s:3967`). Device outcome E5 / OWNER-PHYSICAL-UNVERIFIED. |
| Root-cause owner | `forgeshape_history_selftest.cpp:474-480`, `forgeshape_cad_feature_selftest.cpp:5392-5394` |
| Alternatives | (a) compute each oracle through the same runtime expression, or compare with a stated tolerance, and give the lens check a non-tied fixture; (b) pin `-ffp-contract=off` (OD-17; `PROJECT_STATUS.md:2447-2449` argues against it). Recommended (a); it needs no decision. |
| PC relevance | High: any contracting desktop ABI (Windows-on-ARM, AArch64 Linux). |
| Effort | S, confidence M |
| Decisions | OD-17 only for (b) or for an FMA CI job. |
| Best next slice | T-11 |

### OPP-12 — A device self-test suite silently drops checks past its cap (P2, latent)

| Field | Value |
| --- | --- |
| Link | I-01 = H-13, RC-13, I-14 |
| Horizon | H0 (evidence integrity) |
| Problem | PROJECT is 256/256 (`forgeshape_jni.cpp:821`), 0 headroom; the next PROJECT check would be skipped on device silently. Precedent: CAD_FEATURE lost 49 checks at cap 512 (`artifacts/cad-r2-retained-sketch-parametric-ux/TESTS.md:76`). |
| Evidence | E1 (seat I `cap_probe`: `cap=256 recorded=256`); no current loss. |
| Root-cause owner | `Recorder::check` pattern in all suites; caps in `forgeshape_jni.cpp` |
| Alternatives | (a) `count == cap` fails the suite, raise PROJECT/CAMERA caps; (b) committed expected-count table checked by `ci-device-smoke.sh` (I-14). Both are cheap; (a) is the guard, (b) the regression net. |
| Effort | S, confidence H |
| Decisions | none (test infrastructure) |
| Best next slice | T-12 — must precede any stage that adds PROJECT checks |

### OPP-13 to OPP-30 — P3 verified defects (compact)

| ID | Finding / RC | Problem | Evidence | Owner file | Fix | Effort | Decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| OPP-13 | D-03 = E-07 = APP-087, RC-07 | Freeform edit session and selection survive opening another project | E1 (`diag_d2`: `sessionActive=1 firstSel=3`) | `forgeshape_jni.cpp` `loadProject` / `closeProject` | end the Freeform session in both; see OPP-23 for the general fix | XS, H | none |
| OPP-14 | A-05 = E-04 = FC-R1 D3, RC-04 | the "saved" baseline is lost on every recreation (palette change, `uiMode`/`fontScale`/`locale`) → spurious unsaved question | E2 (`EditorWorkspaceView.java:271-272` not in `carryAcrossRecreation`; `AndroidManifest.xml:31`) | `EditorWorkspaceView.java`, `EditorUiState.java` | move `everPersisted` / `persistedFingerprint` into `EditorUiState` | S, H | none (severity P3 vs P2 is disagreement D-3; mechanism agreed) |
| OPP-15 | A-06, RC-04 | Save Copy / Export after a recreation while the picker is open leaves an empty file and logs a cancel | E2 + E5 trigger | `EditorWorkspaceView.java:5778, 6004, 6050-6055, 5788-5793` | report "nothing was written" and delete the empty document | S, M | none |
| OPP-16 | E-12, RC-07, CC-17 | the JNI load wrapper cancels the sketch and moves the camera before a post-decode refusal | E2 (`forgeshape_jni.cpp:8463-8478` vs `forgeshape_project_state.cpp:250-262`) | `forgeshape_jni.cpp` `loadProject` | cancel session state only after `loadProjectDocument` succeeds (or stage a dry-run first) | S, M | none |
| OPP-17 | E-05, RC-16, CC-19 | a PlanarFaces body whose later feature does not regenerate passes decode/validate but fails load (recovery offered then refused) | E1 (`pf_gap`) | `forgeshape_project_document.cpp:927-940`, `forgeshape_project_state.{h,cpp}` comments | regenerate chains with later features in validation; correct the two comments | S, M | none |
| OPP-18 | E-06, RC-16, CC-18 | the codec accepts two encodings of one face token (`edgeLocalIndex > 0xFFFF`) | E1 (`faceidx`, equal fingerprint `e5ebec3e274d33d4`, different bytes) | `forgeshape_sketch.cpp:734-749` readers | decode-side bound only; state it in `DATA_PACKAGE_SPEC.md` §7c; **do not change the lineage formula** | S, H | spec wording only; no version, no fixture change (a new refusal fixture would be optional and needs the PowerShell builder) |
| OPP-19 | E-13 = H-02, E-14, J-04, RC-05 | checkpoint fingerprint and bytes read under two locks; an in-flight checkpoint can land after Save cleared it; Save fallback leaves the only copy in an unread `.pending` | E2 (timing E5) | `AutosaveController.java:157,173`; `ProjectSlot.java:98-150`; `ProjectCheckpoint.java` | one native call returning fingerprint + bytes under one lock; generation counter for clear; read `.pending` as fallback or never delete before replace | M, M | none |
| OPP-20 | C-03, RC-10 | Add/Cut offered on a full 16-record sketch table; preview says `SketchNotFound` | E2 (P6 re-implements the code) | `forgeshape_sketch_session.cpp:2932-2933, 2895-2902` | gate on `sketches.size() < kMaxCadSketches`; name `TooManySketches` | XS, H | OD-14 only if the table policy changes |
| OPP-21 | D-05 = FC-R1 D6, D-08, RC-10, CC-08 | Clear Mask (mask > ~87k vertices) and Freeform Symmetry ON / level over budget are drawn then refused | E2 | `forgeshape_sculpt.cpp:1687-1690`; Freeform availability | availability asks the same predicate the act refuses on; also correct `ARCHITECTURE.md:3678-3679` | S, M | OD-23 for what an over-cap Clear Mask should do |
| OPP-22 | C-04 | out-of-range Line length refused as `ZeroLengthLine` ("must be positive") | E1 (probe P4) | `forgeshape_sketch_session.cpp:825-828` | return `OutOfRange` with its own string | XS, H | none |
| OPP-23 | J-02, RC-18 | Freeform / Surface length rows seeded with unitless literals, read in the display unit (0.25 → 0.25 mm in mm) | E1 arithmetic (`j3_unit_probe.out`) + E2 wiring | `FreeformEditorView.java:123-125, 282-285`; `SurfaceEditorView.java:108,152,181,499-516`; `EditorWorkspaceView.java:6403-6416` | unit caption, seed through `unit.format`, convert on unit change | S, H | none |
| OPP-24 | H-03 = I-07 | device `FORGESHAPE_CAD_FEATURE_PERFORMANCE` truncated at 1023 chars (`CAD_R2_ORACLE_PASS` lost) | E1 (CI DEVICE `40797031704`) | `forgeshape_jni.cpp:608` | split into lines < 1000 chars | XS, H | none |
| OPP-25 | G-04 | self-tests read uninitialised floats after a failed projection (UB in evidence code) | E1 (`clang++ -Wall -Wextra`: 10 warnings) | `forgeshape_cad_feature_selftest.cpp:5548-5550, 5594`; `forgeshape_sketch_ux_selftest.cpp:1474-1480` | initialise to 0 | XS, H | none |
| OPP-26 | I-11 | `EditorWorkspaceLegibilityTest` leaks `animator_duration_scale 0` on failure | E2 | test `:783/:798`, `@After` `:86-96` | restore in `@After` | XS, H | none |
| OPP-27 | A-08 | Open-picker launch failure shows "Could not write the copy" | E2 | `EditorWorkspaceView.java:2090, 2652` | dedicated string | XS, H | none |
| OPP-28 | D-07 | Resume Sculpt has no native sketch guard (chrome-only) | E2 | `forgeshape_jni.cpp` resume entry | refuse `in_sketch` below JNI | XS, M | none |
| OPP-29 | B-05, CC-45 | derived names at the 96-byte cap truncated mid-suffix or dropped; comment cites a non-existent `LengthFilter` | E2 | `forgeshape_body_commands.cpp:~177` | truncate the base, keep the suffix (or refuse) | S, M | B OD-3 (name policy) — low stakes |
| OPP-30 | A-11 | New Freeform refusal shows a raw token; non-Sculpt projects summarised "in Construction" | E2 | `EditorWorkspaceView.java:2448, 6190-6198` | route through `FreeformPresentation.refusalMessage`; mode word per representation | XS, H | none |

**Gated defects** — mechanism confirmed, but the fix contradicts another owner
text, so the fix waits on a decision (not schedulable until then):

| ID | Finding | Mechanism | Blocks on |
| --- | --- | --- | --- |
| OPP-G1 | A-02 (seat P1) — recovered work is in neither slot nor checkpoint after Recover | E2, synth re-read `EditorWorkspaceView.java:5675-5679`; the code comment says it is deliberate | OD-05 (inverts FSR1B-05 and `PRODUCT.md:193-194`) |
| OPP-G2 | A-01 (P2) — "Save and continue" before Open Saved Project overwrites the slot it then opens | E2 (`:2665-2678`, `:5458-5471`, `:2656-2657`) | OD-06 (APP-H1 lists Open Saved Project among the leaves that ask Save/Discard/Cancel) |
| OPP-G3 | F-03 (P2) — bootstrap Back discards the drawn sketch with a panel open | E2 (`:1177-1184` before `:1201`) | OD-08 (`PRODUCT.md:196-198` vs `ForgeShapeActivity.java:68-72`) |

---

## CLASS 2 — Quality, UX and architecture improvements inside current contracts

None of these adds a feature, a format version or a library. Several need a CI
or OWNER yes, named in the row.

| ID | Link | Problem | Proposal | Owner file(s) | Android effect | PC relevance | Effort | Risk | Decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| OPP-31 | I-05 = G-02, CC-15 | platform neutrality and 4392 native checks reach CI only through a device debug launch behind caps | add `scripts/host-native-selftests.sh` and `host-forge-corpus-verdicts.sh` steps to CI FAST (g++ on the Ubuntu runner) | `.github/workflows/ci-fast.yml` | none at runtime; earlier red | High (this IS the desktop compile gate G1-1/G1-2) | S, H | Low; ~minutes of runner time | OD-18 (seat I asked approval; ARCHITECTURE.md:208 already requires "must stay asserted") |
| OPP-32 | I-02, CC-16 | 25 suites re-run per Activity start (~9.5 s each; 304.4 s of 667 s in `40797031704`); projected to threaten the 120-min Tier-5 stop | process-static "suites ran" flag in `start()`; optionally move CAD_FEATURE perf blocks host-only | `forgeshape_jni.cpp:2083-2126`, `ForgeShapeActivity.java:33-36` comment | debug cold-start latency only; release unaffected | Low | S, M | Low; changes what device startup proves per test (still once per process) | OD-26 (debug-path product change) |
| OPP-33 | I-08 = F-09, CC-07, CC-47, CC-49 | most device taps bypass hit testing; no 48 dp sweep; Back tested through the platform route only for Add Primitive | route `SketchTestSupport` taps through the decor root with a hit-target assertion; one view-tree sweep helper for ≥ 48 dp per captured state; `KEYCODE_BACK` cases per phase | `app/src/androidTest/.../SketchTestSupport.java`, `WorkspaceTestSupport.java`, `EditorWorkspaceLegibilityTest.java` | would have caught F-01, F-03, F-04, F-06 | None | M, M | Harness change is a Tier-5 trigger (I-08) | none beyond OD-03 timing |
| OPP-34 | I-14, I-13 | a suite that loses checks stays green; ledger verdicts not in `summary.json` | committed per-suite expected-count table (fail on decrease); copy ledger counts into `summary.json` | `scripts/ci-device-smoke.sh` | none | Low | S, H | Low | none |
| OPP-35 | RC-07, OPP-13/16, U-2 | five project routes reset hand-written, incomplete lists of process sessions (Freeform, chooser, gizmo, Dimensions) | one `resetProcessSessionsForProjectChange()` called by load (after success) and close; trace the Java post-load path (U-2) first | `forgeshape_jni.cpp:8462-8480, 8968-8990` | removes a class of stale-session bugs | Medium (this is the "application core" extraction G-07 needs) | M, M | Medium | none |
| OPP-36 | RC-10 | "a control that cannot succeed is not drawn" is met by convention per control | availability functions call the same predicate as the refusal (no separate list) — start with Clear Mask, Add/Cut table, Freeform Symmetry | sculpt, sketch session, Freeform session | fewer drawn-then-refused controls | Low | M, M | Low | none |
| OPP-37 | F-04 root, RC-08 | `hasDismissibleSurface` and `dismissTopmostSurface` are two lists | derive one from the other (part of OPP-08) | `EditorWorkspaceView.java:1112-1229` | Back consistency | None | S | Medium (OD-08 for bootstrap order) | OD-08 |
| OPP-38 | H-08 (FC-R1 F-07) | face-supported bodies re-derive face frames every frame (4.35 ms at depth 32, E3) | cache the resolved frame keyed by the producer's `CadBodyState` revision | `forgeshape_scene.cpp:281-326` | frame time on deep chains | Medium | M, M | Medium (cache invalidation) | none; recorded debt `PROJECT_STATUS.md:4679` |
| OPP-39 | H-06 | whole-mesh normal refresh + upload per frame during a stroke (20 ms host at 100k vertices) | incremental normals over the stroke's affected set + dirty-range upload | `forgeshape_renderer.cpp:1119-1122, 749, 868` | sculpt smoothness on large meshes | Medium | L, L | Medium (renderer) | OD-19 (no budget recorded) |
| OPP-40 | H-05 | at-cap Finish 275–322 ms host, synchronous under `g_stateMutex` | measure release-ARM first; then off-thread derivation with a "deriving" state | `forgeshape_jni.cpp:4400-4420, 1733, 5098` | jank/ANR risk (OWNER-PHYSICAL-UNVERIFIED) | Medium | L, L | High (threading) | OD-19 |
| OPP-41 | RC-21, CC-21..CC-49 (doc rows) | stale counts, comments and docs (D5 listed open, 23 tokens, 57 fixtures, `metersPerPixel` sentence, `ARCHITECTURE.md:214`, `:2606-2608`, three `CadStatus` comments, selection "rigid" comments, Activity "once per PROCESS") | one documentation-only batch, after the code fixes it depends on | PROJECT_STATUS, README, docs/CI_CLOUD.md, ARCHITECTURE, PRODUCT, headers | none | G-03 matters to a surface seam | S, H | Low | CC-12 text depends on OD-18 |
| OPP-42 | J-05, `COUNCIL_DEBT_RECHECK.md` §2.3 | 8 orphan natives, 2 dead helpers; 13 `debug*` exports stubbed in release | remove orphans or cover them; keep stubs but add them to the release guard's probe | `forgeshape_jni.cpp`, `forgeshape_sketch_region.cpp:50`, `forgeshape_sketch.cpp:235`, `scripts/ci-release-selftest-guard.sh` | smaller JNI surface | Low | S, H | Low | none |
| OPP-43 | I-09 | 884 `settleLayout` sites with `sleep(250)`; 17.5 s on `STATUS_HOLD_MS` | replace with idle/condition waits in the hottest classes | androidTest support | test time | None | M, M | Low | none |
| OPP-44 | H-17, H-10, H-14 | shader/C++ constant parity by comment; triangles ≤ 1e-8 m² unpickable; one equal-extent swapchain rebuild at cold start | a host check reading the shader constant; relative pick threshold; skip equal-extent rebuild | `shaders/*`, `forgeshape_picking.cpp`, renderer | small | Medium (shader parity) | S each | Low–Medium | H-10 threshold: OD-19 sibling (min feature size) |
| OPP-45 | F-07 | selected state and status verdicts not announced by TalkBack | `setSelected` in `setChipActive` and the rail; polite live region on `status_message` | `EditorControlStyles.java:433-439`, `ToolRailView.java:242` | screen-reader usability | Medium (desktop accessibility trees) | S, H | Low | OD-21 (no accessibility spec exists; H2 until decided — low-risk, can be done as hygiene if the OWNER agrees) |

---

## CLASS 3 — IDEA ONLY (future features and desktop expansion)

**Nothing in this class is authorised by this audit.** Each needs an OWNER
decision and its own stage; several need a format version or a library gate.
The following are explicitly **not authorised and not proposed for the next
stage**: Fillet, Chamfer, Hole, Shell, Pattern, STEP / IGES (or any new
interchange), a constraint solver or persistent sketch relations, Through All /
To Object / Intersect, `CADB` v9, feature reorder or suppression, project-level
sketches, a new or second geometry kernel, a desktop release
(`PROJECT_STATUS.md` Next Stage "Do not start …"; CLAUDE.md hard rules).

| ID | Idea | Source / link | What it would need | PC relevance | Rough size |
| --- | --- | --- | --- | --- | --- |
| IDEA-01 | World-plane retained sketch (A2) options 1 or 2 | `artifacts/cad-r2-c1-quality-gate/SUMMARY.md` §A2; R2_SK_06 proves today's refusal | OD-01; options 1/2 need a new `CADB` version (v9 — **not authorised**), builder parity, fixtures | Neutral | L–XL |
| IDEA-02 | Desktop research spike (Linux first, viewport-only PoC) | `DESKTOP_POC_GATE.md` G0–G5, `DESKTOP_PORT_STRATEGIES.md` | OD-04 (G0-1 spike, G0-2 shell family; S3 needs a library gate), OD-17 FP policy; surface seam split in the renderer; must not change any Android byte (A7) | Core | L (PoC), XL (product) |
| IDEA-03 | Android desktop-mode input (wheel zoom, secondary-button pan, hover, hardware keyboard) | F-10, G-09, `forgeshape_input.h` | OD-04 (ChromeOS / desktop windowing as targets); new input vocabulary; one unit policy for thresholds (OD-20) | High (same vocabulary a desktop needs) | M–L |
| IDEA-04 | Multi-document context object (per-document allocator) | `DESKTOP_PORT_STRATEGIES.md` §2.4, G-06 | OD-13 first; wiring change across singletons | Core for desktop | L |
| IDEA-05 | Open Saved Project from Home | A-03, CC-11 | OD-07 (APP-H1 "exactly two" Home actions) | Low | S |
| IDEA-06 | Real-stylus hover preview in Sculpt | PROJECT_STATUS Next Stage "DEFERRED" | an autosave-suspend concept (checkpoint reads the fingerprint on its worker) | Low | M |
| IDEA-07 | Draw-on-change frame pacing | H-07 | OD-19; evidence captures depend on presented-frame counts | Medium (laptop battery) | M |
| IDEA-08 | Release-variant runtime smoke and an arm64 automated run | I-10, PORT-032 | OD-18 (signing; runner) | Medium | S–M |
| IDEA-09 | Accessibility specification (TalkBack, form factors) | F-07, UI-073/074, UI-047 | OD-21 | Medium | M |
| IDEA-10 | Write-side `.forge` size limit equal to the 256 MiB read limit | seat E OD-4 (H-E2) | OD-24 | Low | S |
| IDEA-11 | Revolve Add / Cut, later Revolve, Surface trim of curved surfaces, T-Spline refinement, general NURBS | CLAUDE.md "Not this stage" rows | each its own stage and, for most, a format version | Neutral | XL |

The A2 options and the desktop gate are decision records, not plans to start:
see `OWNER_DECISIONS_REQUIRED.md` OD-01 and OD-04.
