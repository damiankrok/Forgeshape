# CAD-R2 — SUMMARY (retained sketches, parametric history, quality)

Read `BEFORE.md` and `DESIGN.md` first; `ACCEPTANCE_MATRIX.md` holds the
per-item verdicts and `TESTS.md` the raw gate results.

## Refs

| Ref | SHA |
| --- | --- |
| Base (exact) | `636797e518898c31c4cd691d453db2833ef00896` |
| Task branch | `feature/cad-r2-retained-sketch-parametric-ux-r1` |
| Last native / Java product commit | `5127cfc` (plus the Java constant and D13 harness in `5dd7d97`) |
| Device union head (part A) | `f706e92` |
| Device union part B head | `7d2d7b4` (test / docs only after `f706e92`) |
| Final CI FAST head | `b747651` (docs only after part B) |

No merge, rebase, cherry-pick, force push or history rewrite; no other branch
touched; no FullSharded.

## Gates

Host `HOST_SELFTESTS_OK (4369 checks, 0 failed)` (25 suites; baseline 4304);
JVM 196/196; release guard PASS (both ABIs); corpus 72/72; DEVICE union
**118/118** (`CI DEVICE` 40683528533 67/67 + 40689211847 51/51);
`CI FAST` **40694343402** success.

**OWNER APK:** `ci-fast-evidence` artifact **12819336381**,
`app/build/outputs/apk/debug/app-debug.apk`, 17,545,911 bytes, SHA-256
`043203e18733d10e28985bf86a4cb41ed87fc3c3fd743b391fbc20a6ad5ac272`; expires
2026-11-11T13:19:04Z.

## What the user gets

- **History → Sketches.** Every retained sketch of the active CAD body is
  listed in native order by durable id, with its support (plane or feature
  face), entity and dimension counts, its consumers, and its state (used,
  unused, failed, not rebuilt, support lost). Acts are drawn only when native
  accepts them: **Edit Sketch**, **Use for Add**, **Use for Cut**, **Delete
  Sketch**. A refused act shows native's own reason (`Sketch is used by
  Extrude N`, `The first sketch cannot be removed`).
- **Keep Sketch.** A sketch drawn on a face of the body's own features can be
  finished WITHOUT a feature and stays in the table (one Undo).
- **Reuse.** Use for Add / Cut appends a feature naming the EXISTING sketch by
  id — never a copy — through the ordinary staged extrude (one Undo).
- **Safe delete.** Only an unused, non-root sketch; never cascades; the id is
  never minted again.
- **Features segment.** Edit Feature / Edit Sketch by durable id; the staged
  edit names the first failing feature and marks later ones not rebuilt; Fix
  or Cancel; Cancel writes nothing.
- **Select All / Clear** for the regions or faces of the open sketch, through
  the one selection evaluator.

## Door → native (P4.2: one command, every door)

| Door | Native entry |
| --- | --- |
| History Sketches · Edit Sketch | `sketchBeginEditSketch` → `SketchSession::beginEditSketch` (used → first consumer's staged edit; unused → sketch-only session) |
| History Features · Edit Feature | `sketchBeginEditFeature` (unchanged entry) |
| History Sketches · Use for Add / Cut | `sketchBeginReuseSketch` → `SketchSession::beginReuseSketch` |
| History Sketches · Delete Sketch | `cadDeleteSketch` → `deleteCadSketch` (one `ScopedConstructionEdit`) |
| Trailing host · Keep Sketch | `sketchCommitSketch` → `SketchSession::commitSketch` |
| Retained-sketch Edit Sketch chip | the existing body edit path (`sketchBeginEdit`), unchanged; withdrawn during the chooser |
| Drawer · New Sketch | Add Primitive's spatial chooser path, unchanged |
| Sketch editor · Select All / Clear | `sketchSelectAllAreas` / `sketchClearAreaSelection` |
| Every row / act decision | `cadSketchInventory` (+ `cadSketchConsumers`) → `buildCadSketchInventory` |

## Format

**No format change.** No `CADB` version, section, code, fixture or lineage
rule moved; corpus 72/72 unchanged. `CadStatus` (runtime JNI vocabulary)
gained two APPENDED codes, `SketchInUse` (91) and `SketchIsRoot` (92).
A retained sketch on a WORLD plane would need a second root, which the
durable model forbids: **BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH**
(reproducer `R2_SK_06`; refused `SketchSupportInvalid`, nothing written).

## Product defects found and fixed

1. A sketch-only edit's candidate was built as a new one-feature body
   (`R2_SK_08`) — `candidateState` resolves reuse and sketch-only first.
2. `CadBody::applyState` accepted a structural upstream edit that stranded a
   kept sketch, a state the codec then refuses to save (`R2_SUP_05`) —
   `applyState` and the candidate now run
   `validateCadStateBeyondRegeneration`.
3. Debug CAD_FEATURE device record cap (512) was below the suite's check
   count, so device runs silently dropped checks — cap 1024.
4. In landscape the History act strip wrapped below 48 dp (device D12) — acts
   and detail now stand above the scrolling list.

## Diff inventory (`636797e..HEAD`)

Native: `forgeshape_cad_sketch_inventory.{h,cpp}` (new),
`forgeshape_cad_r2_selftest.{h,cpp}` (new), `forgeshape_cad_oracle_selftest.{h,cpp}`
(new), `forgeshape_sketch_session.{h,cpp}`, `forgeshape_cad_feature.{h,cpp}`,
`forgeshape_cad_body.{h,cpp}`, `forgeshape_sketch.{h,cpp}`,
`forgeshape_cad_feature_selftest.cpp` (wiring), `forgeshape_jni.cpp`,
`CMakeLists.txt`. Java: `SketchBrowserPresentation`, `SketchBrowserText`
(new), `FeatureHistoryView`, `EditorWorkspaceView`, `WorkspaceTrailingHostView`,
`GlobalToolbarView`, `SketchEditorView`, `NativeViewport`, `CadStatusMessages`.
Resources: `ids_cad_r2.xml`, `strings_cad_r2.xml`, `ic_sketch_keep.xml`. Tests:
`SketchBrowserPresentationTest` (JVM 17), `CadRetainedSketchOwnerTest`
(device D01..D14). Docs: `ARCHITECTURE.md`, `CLAUDE.md`, `PRODUCT.md`,
`PROJECT_STATUS.md`, this folder. Nothing in the renderer, the codec, the
corpus, the kernel, CI workflows or Gradle changed.

## Performance (`perf_same_machine.md`, 11 interleaved runs each)

Region, Add, Add + Cut and codec round trip are unchanged within noise.
**Recorded performance risk:** `eval_all` median +31.5 % and
`drafting_trim100` +35.1 % on the same container. Removing the new validation
did not move `eval_all`, and the trim code is untouched, so the cause is not
attributed to this stage's code (container noise / binary layout suspected),
but it is not disproven either and stays open for a physical-device timing.

## Known issues / OWNER later

- World-plane retained sketch: BLOCKED-DESIGN-DECISION (above).
- Revolve Add / Cut reuse: refused by name — out of scope.
- Font scale > 1.0 not exercised on device.
- D04's curved-wall pixel resolved the YZ world plane rather than a face; the
  curved wall's ineligibility is proven on the host (`R2_SUP_03`).
- All device evidence is the CI emulator; stylus, hover, hardware GPU and
  real-device performance remain OWNER physical-device checks.

## OWNER physical checklist

1. Open a CAD body with Add + Cut; History → Sketches lists each sketch with
   its consumers; acts appear only where they work.
2. New Sketch on the Cut floor → Keep Sketch → it is listed as Unused; Undo
   removes it, Redo restores it.
3. Use for Add on that sketch → extrude → one Undo removes only the feature.
4. Delete an unused sketch; try deleting a used one and the first one — both
   refused with a readable reason.
5. Edit an upstream width so a later Cut fails → issue named, Cancel restores.
6. Select All / Clear on a grid sketch; extrude; save; reopen.
7. Rotate the device with the History surface open; every act stays ≥ 48 dp.
