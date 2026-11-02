# IMPORT-GLB-SKETCH-GUARD-R1 — ACCEPTANCE MATRIX

## Evidence sources

| Label | Source | Head |
| --- | --- | --- |
| RED-host | the closeout CR-03 diagnostic re-run on the baseline (`evidence/red/cr03_scenario_*.out`); the JNI import body it copies is byte-identical at `8eb94ce` | baseline |
| RED-device | `CI DEVICE` `41490895211`, `ImportGlbSketchGuardTest` on the unchanged product: 5/5 FAIL | `ac161fa` |
| RED-mutation | removing the sketch branch of `importModeRefusal` fails 8 of the new host checks (`evidence/red/host_mutation_sketch_branch_removed.txt`) | GREEN tree |
| GREEN-host | `HOST_SELFTESTS_OK (4454 checks, 0 failed)`; GLTF_IMPORT 209/209 | — |
| GREEN-JVM | 199/0 | — |
| GREEN-device | `CI DEVICE` `41492901369`, 65/65 | `e33a1ee` |

## Host contract (GLTF_IMPORT suite, `forgeshape_gltf_import_selftest.cpp`, `runImportSketchGuardChecks`)

All facts come from a real `SketchSession` / `SculptSession` over a project
with a Construction body and a CAD body. The GLB is a valid file built by
`buildMiniGlb(twoTriangleSheet())`.

| ID | Requirement | Checks | RED | GREEN |
| --- | --- | --- | --- | --- |
| IMPORT-SK-01 | Valid GLB refused in Editing by its stable name. Bodies, active body, allocator, Undo/Redo, fingerprint and encoded bytes are identical. No partial report. Still Editing. | `IMPORT_SK_01_*` (4) | RED-host A: accepted (return 0, bodies 1→2, active 1→2, undo 0→1, fingerprint moved). Mutation: 3 of 4 fail. | pass |
| IMPORT-SK-02 | Refused in Ready. Ready state, selection and draft intact. Project unchanged. | `IMPORT_SK_02_*` (3) | mutation: 2 fail | pass |
| IMPORT-SK-03 | Staged Edit Sketch refuses and keeps its draft. Afterwards it commits as an edit of the SAME body, and a second staged edit cancels with the project unchanged. | `IMPORT_SK_03_*` (3) | mutation: 2 fail | pass |
| IMPORT-SK-04 | Once Inactive, the same GLB imports: one Imported body, active, one Undo, `firstObjectId` = the allocator's next id (no gap). Encode → decode → encode is byte-identical. | `IMPORT_SK_04_*` (3) | — | pass |
| IMPORT-SK-05 | Sculpt still `RefusedInSculpt`, with precedence over a sketch. A malformed GLB is still a FILE refusal. | `IMPORT_SK_05_*` (2) | RED-host D: 1005 (control) | pass |
| IMPORT-SK-06 | Five repeated refusals: no change, no id minted. | `IMPORT_SK_06_*` (1) | mutation: fails | pass |
| IMPORT-SK-07 | Ordinals 0..5 unchanged and `RefusedInSketch` = 6. No mode → no refusal. Every status has a name (`IMP01A_08_every_commit_status_has_a_name` extended). | `IMPORT_SK_07_*` (2) + IMP01A_08 | — | pass |
| capacity | The suite fits its 256-slot device recorder (209 checks). | `IMPSK_gltf_import_suite_fits_its_256_slot_device_recorder` | — | pass |
| format / ABI | No `.forge`, `CADB` or codec change. Corpus 72 decoder verdicts. FAST parity 72/72. Release guard PASS. | local + FAST | — | pass |

## JVM (`ProjectDrawerPolicyTest`, 9 → 12)

| Test | What it covers |
| --- | --- |
| `importIsOfferedExactlyOutsideSculptAndOutsideAnOpenSketch` | truth table of `importShown` |
| `theModeRefusalsKeepTheirWireCodes` | 1000 / 1005 / 1006 |
| `everyImportDoorAsksTheSketchQuestionAndSaysItsOwnSentence` | both callbacks guard; the bytes seam branches on 1006; the row follows the policy; the sentence exists, says "unchanged" and never "Freeze" |

## Android real path (`ImportGlbSketchGuardTest`)

| Case | Scenario | How it is driven | RED (`41490895211`) | GREEN (`41492901369`) |
| --- | --- | --- | --- | --- |
| sk01 | (a) Import row absent in Editing and Ready; back after Cancel | drawer opened by a REAL tap on the mark (decor-root MotionEvents) | FAIL: "not offered while a sketch is Editing" | pass; facts `import_row_offered_editing=false`, `..._ready=false` |
| sk02 | (b) Picker requested LEGALLY by real taps (mark, then Import row; a recording `ProjectTransferHost` stands in for the system picker), then a sketch begins, then the picker answers through `onOpenGlbDocumentChosen` (the Activity's callback), 3 times. Neutral, then the same file imports after Cancel. | real taps + the callback the Activity calls; the system picker UI itself is NOT shown | FAIL: 18 bodies created, undo 0→3 | pass; status "Import is not available while a sketch is open. Your project and your sketch are unchanged." |
| sk03 | (c) Bytes seam ×3 and direct JNI ×3 in Editing and in Ready, against a snapshot of fingerprint, ids, active body, Undo/Redo, encoded project, sketch state and entity count. The same draft then commits (one body, one step). (d) The same GLB imports after. | direct calls | FAIL: bytes seam imported 18 bodies | pass; `[1006,1006,1006] RefusedInSketch` in both |
| sk04 | Staged Edit Sketch: refused ×6, Cancel leaves the committed body byte-identical, then a legal import | direct calls | FAIL | pass; `[1006,1006,1006] RefusedInSketch` |
| sk05 | (f) Portrait and landscape × font 1.0 and 1.5: row offered with no sketch, absent while sketching | real tap on the mark; screenshots | FAIL: "font 1.0 orientation 1: not offered while sketching" | pass; 4 captures |
| (e) | Existing import in Construction and the Sculpt refusal | `ImportedMeshDurableTest` 12, `ImportGlbSculptGuardTest` 3 | — | pass |
| — | Sketch regression | `SketchUxTest` 14, `SketchExtrudeTest` 9 (its import after an extrude included), `CadRetainedSketchOwnerTest` 22 | — | pass |

## Held open (not tested as behaviour, by design)

- **Chooser-only import (CR-03 Q2):** unchanged, still accepted.
- **Home / no-project direct call (CR-03 Q3):** unchanged.
- **Physical device, stylus, TalkBack:** UNVERIFIED.
