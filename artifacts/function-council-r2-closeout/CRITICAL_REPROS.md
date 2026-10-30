# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — Critical repros (CR-01 .. CR-07)

Audit only. No product, test, script, fixture, workflow or format file was changed. The machine-readable form of this file is `CRITICAL_REPRO_MATRIX.tsv`, with one row per symptom (48 rows).

## Method

- **Baseline:** product tree `70a6c14` (`= f3db953`). The audit branch `audit/function-council-r2-closeout-r1` was cut from `2e5d8fc`. `app/`, `scripts/`, `testdata/` and `.github/` do not differ between `70a6c14` and the audit HEAD.
- **How each claim was tested:**
  - Host C++ diagnostics link the product objects. They are built the way `scripts/host-native-selftests.sh` builds them: same flags, every `forgeshape_*.cpp` except jni and renderer, and vendored Manifold.
  - JNI bodies are not host-buildable. Where a JNI body matters, its guard-and-write sequence is copied verbatim and the copy is marked as a copy.
  - Java persistence classes run unmodified on a host JVM behind sandbox shims (CR-05).
  - Every claim is backed by recorded `.out` files and a `CMD.txt` with exact commands and exit codes, under `diagnostics/CR-0X/`.
  - Binaries were built in `/tmp/claude-0/fc2c-scratch/` and are not committed.
- **Classification vocabulary:**
  - **REPRODUCED:** run in this closeout, with output quoted.
  - **SOURCE-CONFIRMED-NOT-REPRODUCED:** established from source with `file:line`, not run end to end.
  - **REFUTED:** the claim was tested and does not hold.
  - **BLOCKED:** cannot be run here.
  - **OWNER-SEMANTICS-REQUIRED:** the written contracts do not decide it.
- **Confidence scale:**
  - E1: reproduced in this closeout, with output.
  - E2: source-derived proof (`file:line`).
  - E3: a historic recorded run.
  - E4: documentation only.
  - E5: inference.
  - E0: no evidence.
- **Device caveat:** no claim here rests on a device run made in this closeout.
  - Every E3 measurement comes from an emulator (CI DEVICE, x86_64, API 36). An emulator is never physical-device evidence.
  - The coordinator's focused CI DEVICE run (GlbExportTest, SpatialSketchTest, SketchExtrudeTest, CadCanvasExtrudeTest, CadPlanarFaceRuntimeTest, CadPlanarFaceOwnerCorrectionTest) is not visible to this writer and is not cited.

## Evidence spot-check (done before writing)

More than five cited evidence files were opened and the quoted lines were found in them:

- **CR-01:** `cr01_export_face.out` has 11 `verdict=MISMATCH` and 12 `verdict=MATCH`, and the deltas 3.0000 / 5.6561 / 4.3722. `glb_sha256.out` gives identical digests for `S1_cap.glb` and `S2_side.glb`.
- **CR-02:** `cr02_transform_face.out` has the `C1 VERDICT … worldMoved=0`, `C2 VERDICT … worldMoved=0`, `equalsPristine=0`, `ACTIVE:1` and `D4 … REFUSED:body_locked` lines.
- **CR-03:** `scenario_A..E.out` have `importGlbDurable_return=0` (A, B, C, E), `1005` (D), `REFUSED:in_sketch`, `jni_undo_while_chooser=MOVED` and E's `after_undo_2 hasProject=0 … redo=2`.
- **CR-04:** `cr04.out` has six `match=-1` lines, `RESULT failures=9`, and the controls' `match=2` and `nothing_to_do`.
- **CR-05:** `build_and_run.out` has 20 `=OBSERVED` and 0 `NOT_OBSERVED`, `writes=0 skippedUnchanged=1`, the A01, A02, A05, E02 and E02b tokens, the four `CONTROL_*` tokens, and `SAVE_COPY_CANCELLED`.
- **CR-06:** `cr06_native.out` has `setExtrudeSide(true,25)=Ok depth_after=25` and `camera_view_identical_to_B0=1` for B1, B2 and B3. `CommaDiag.out` has the `'25'` and `'10005'` canvas filter lines.
- **CR-07:** `cap_probe.out` has `AT_CAP` and `SIM 257 … recorded=256 failed=0`. `fp_oracle_diag.out` has `ulps=-1` and `tap(0,0)->2`. `host-runs/rcs.txt` gives exits 1/1/0/0/0/1/1, and the `HOST_SELFTESTS_FAIL (4392 checks, 2 failed)` lines are present. `contract_probe.out` has `fmadd` at aarch64 -O0 and -O2.

No cited file was missing and no quoted output was contradicted, so no symptom was downgraded.

**One erratum, not a downgrade.** The CR-01 section below abbreviates the shared digest as `9934da55…f3eb`. `glb_sha256.out` records the full digest as `9934da55978dd5c06184710c68c3bc98b63a8815c831f8708b79980ceb3f4ed9`, which ends in `…f4ed9`. The byte-identity claim stands. Only the abbreviation's tail is wrong, and it is left verbatim in the section as instructed.

**Note on the matrix.**
- The CR-05 rows from E-02 onward (E-02, E-02b, E-02c, A-06, A-03/A-07) were taken from the CR-05 section. The structured CR result for them was truncated.
- All CR-06 and CR-07 rows were taken from their sections.

## Summary

| Repro id | Symptom | Classification | Confidence | Evidence file |
| --- | --- | --- | --- | --- |
| CR-01-S1 | A dependent on the producer's far cap is exported at the origin and not on the cap | REPRODUCED | E1 | `diagnostics/CR-01/cr01_export_face.out; diagnostics/CR-01/parse_glb.out` |
| CR-01-S2 | A dependent on a side face loses both the face-frame rotation and the offset | REPRODUCED | E1 | `diagnostics/CR-01/cr01_export_face.out` |
| CR-01-S3 | The cap and side exports are byte-identical GLB files | REPRODUCED | E1 | `diagnostics/CR-01/glb_sha256.out` |
| CR-01-S4 | In a dependency chain every level is exported at the origin | REPRODUCED | E1 | `diagnostics/CR-01/cr01_export_face.out` |
| CR-01-S5 | Moving or rotating the producer has no effect on a dependent's export | REPRODUCED | E1 | `diagnostics/CR-01/cr01_export_face.out; diagnostics/CR-01/parse_glb.out` |
| CR-01-S6 | Committed face-supported fixtures export misplaced bodies (cap, side, mixed) | REPRODUCED | E1 | `diagnostics/CR-01/cr01_export_face.out; diagnostics/CR-01/parse_glb.out` |
| CR-01-S7 | Control: the exporter also misplaces independent bodies (world CAD, Construction primitive, rotated, scaled, translated) | REFUTED | E1 | `diagnostics/CR-01/cr01_export_face.out; diagnostics/CR-01/parse_glb.out` |
| CR-01-S8 | The expected behaviour (export at the derived world placement) is contractual and needs no new owner semantics | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-01/CMD.txt` |
| CR-02-S1 | (a-1) JNI typed-transform entry applyBoxTransform has no face-support guard | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-02/CMD.txt` |
| CR-02-S2 | (a-2) JNI gizmo activation (setGizmoActive) and touch drag start (gizmo.beginDrag) have no face-support guard | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-02/cr02_transform_face.out` |
| CR-02-S3 | (b) UI: Transform rail entry, gizmo 'offered' and the placement editor are drawn for a face-supported CAD body | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-02/CMD.txt` |
| CR-02-S4 | (c-1) Typed transform write on a face-supported body: undo step recorded, fingerprint changed, stored transform moved, drawn/picked placement unchanged | REPRODUCED | E1 | `diagnostics/CR-02/cr02_transform_face.out` |
| CR-02-S5 | (c-2) Gizmo drag on a face-supported body: gizmo drawn at stored pivot (0,0,0) while body drawn at (0,0,3); drag commits one undo step and moves only the stored transform | REPRODUCED | E1 | `diagnostics/CR-02/cr02_transform_face.out` |
| CR-02-S6 | (c-3) The invisible edit reaches .forge: bytes differ from a visually identical project and the decoder accepts the non-identity stored transform | REPRODUCED | E1 | `diagnostics/CR-02/cr02_transform_face.out` |
| CR-02-S7 | (B-03) ARCHITECTURE.md states a guard that does not exist | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-02/CMD.txt` |
| CR-03-S1 | S1: Durable GLB import accepted while a sketch is being drawn in an open project; creates a body, makes it active, records a Construction step, moves the fingerprint | REPRODUCED | E1 | `diagnostics/CR-03/scenario_A.out` |
| CR-03-S2 | S2: While sketching in an open project the Import row is drawn and one tap away through the ForgeShape mark | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-03/CMD.txt` |
| CR-03-S3 | S3: Importing during a sketch corrupts the open sketch, the project encoding or the later sketch commit | REFUTED | E1 | `diagnostics/CR-03/scenario_A.out` |
| CR-03-S4 | S4: The imported step cannot be undone while the sketch stays open (JNI Undo refuses in_sketch until Finish or Cancel) | REPRODUCED | E1 | `diagnostics/CR-03/scenario_B.out` |
| CR-03-S5 | S5: With the support chooser open (no sketch) import is accepted, and Construction Undo is also not refused | OWNER-SEMANTICS-REQUIRED | E1 | `diagnostics/CR-03/scenario_C.out` |
| CR-03-S6 | S6: CONTROL. Import in Sculpt mode is refused on the same path | REPRODUCED | E1 | `diagnostics/CR-03/scenario_D.out` |
| CR-03-S7 | S7: Import by direct call during the CAD bootstrap (no project, sketch open) creates a project outside loadProjectDocument with undo=1; two Undos return to an EMPTY scene with redo=2 | REPRODUCED | E1 | `diagnostics/CR-03/scenario_E.out` |
| CR-03-S8 | S8: The S7 bootstrap import is reachable from the UI | REFUTED | E2 | `diagnostics/CR-03/CMD.txt` |
| CR-04-S1 | Undo after a NotRetained stroke lands on a mesh state that never existed on the branch | REPRODUCED | E1 | `diagnostics/CR-04/cr04.out` |
| CR-04-S2 | hasEdits reports false while the mesh still carries the unretained deformation (Reset Sculpt then asks no confirmation; a save stores hasEdits=false) | REPRODUCED | E1 | `diagnostics/CR-04/cr04.out` |
| CR-04-S3 | Redo after that Undo does not restore the oversized stroke's state either; it lands on another state that never existed | REPRODUCED | E1 | `diagnostics/CR-04/cr04.out` |
| CR-04-S4 | The History navigator draws rows for states behind the unretained stroke that cannot be reached; a jump to row 0 lands on a state that never existed | REPRODUCED | E1 | `diagnostics/CR-04/cr04.out` |
| CR-04-S5 | Claimed part of the contract: the oversized stroke APPLIES and is said by name (NotRetained counted, nothing pushed) | REFUTED | E1 | `diagnostics/CR-04/cr04.out` |
| CR-05-S1 | A-01: 'Save and continue' before Open Saved Project overwrites the only saved project (A) with the live one (A+b), reopens it from the slot and empties Undo history | SOURCE-CONFIRMED-NOT-REPRODUCED + OWNER-SEMANTICS-REQUIRED (OD-06) | E2 (+E1 file effects in host transcript) | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S2 | A-02a: after Recover the recovered work is in neither slot nor checkpoint; backgrounding writes nothing and a second process death leaves nothing to recover | REPRODUCED + OWNER-SEMANTICS-REQUIRED (OD-05) | E1 (component) + E2 (workspace calls :5675-5679) | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S3 | A-02b: Recover followed by New Project asks nothing and closes recovered work that exists in no file | SOURCE-CONFIRMED-NOT-REPRODUCED + OWNER-SEMANTICS-REQUIRED (OD-05) | E2 (+E1 file state) | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S4 | A-05 / E-04: a palette change (any Activity recreation) forgets that the project was saved; projectDirty() flips false->true with fingerprint unchanged and New Project asks the unsaved question | SOURCE-CONFIRMED-NOT-REPRODUCED (contract status DEFECT, fail-safe direction) | E2 (+E1 support: reflection and transcript) | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S5 | E-02: recovery prompt offered over the open project after recreation in a process that started with no candidate | REPRODUCED (component) | E1 | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S6 | E-02b: a saved, unchanged project is offered as 'Unsaved work found' after two recreations | REPRODUCED (component) | E1 | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S7 | E-02c: old-worker onStop write races the new constructor's read; edits lost on Recover | BLOCKED | E5 | `diagnostics/CR-05/CMD.txt` |
| CR-05-S8 | A-06: a chosen Save Copy / Export destination after recreation writes nothing, shows no status and is logged as a cancel; an empty document is likely | SOURCE-CONFIRMED-NOT-REPRODUCED (contract status DEFECT) | E2 (empty file E5) | `diagnostics/CR-05/build_and_run.out` |
| CR-05-S9 | A-03 / A-07: saved slot unreachable from Home; an open sketch is dropped on leave | OWNER-SEMANTICS-REQUIRED | E2 | `diagnostics/CR-05/CMD.txt` |
| CR-06-S1 | F-05: typing '2,5' in the three canvas editors (extrude depth, line length, body dimension) commits 25 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 end-to-end; parser and model legs E1 | `diagnostics/CR-06/CommaDiag.out; diagnostics/CR-06/cr06_native.out` |
| CR-06-S2 | F-05b: typing '1 000,5' in a canvas editor commits 10005 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2; legs E1 | `diagnostics/CR-06/CommaDiag.out; diagnostics/CR-06/cr06_native.out` |
| CR-06-S3 | C-01 = F-02: orientation navigator Flip and +/-90 do not move the sketch camera | REPRODUCED | E1 | `diagnostics/CR-06/cr06_native.out` |
| CR-06-S4 | F-04: System Back during an in-project Revolve with no surface open backgrounds the app on API 36 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 for API 36; E5 for API 33-35 | `diagnostics/CR-06/CMD.txt` |
| CR-06-S5 | F-03: bootstrap Back with a panel open discards the sketch and returns Home with no question | OWNER-SEMANTICS-REQUIRED | behaviour E2 | `diagnostics/CR-06/CMD.txt` |
| CR-06-S6 | F-01: some chrome hit areas are narrower than 48 dp (sketch_navigator_flip 36.19, view_selection_outline_on 36.95, view_selection_outline_off 39.24, sketch_dimension_label 43.43 dp) | SOURCE-CONFIRMED-NOT-REPRODUCED | E3 measured (emulator, historic run 40797031704) + E2 | `diagnostics/CR-06/CMD.txt` |
| CR-07-S1 | PROJECT self-test suite is at its 256/256 device cap; a 257th (even failing) check is silently dropped and the token still says OK; none is lost today | REPRODUCED | E1 | `diagnostics/CR-07/cap_probe.out` |
| CR-07-S2 | eviction_is_deterministic_oldest_first (CONSTRUCTION_HISTORY) fails under FP contraction (FMA) | REPRODUCED | E1 host; arm64 device E2 (asm), physical unverified | `diagnostics/CR-07/host-runs/; diagnostics/CR-07/fp_oracle_diag.out` |
| CR-07-S3 | CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order (CAD_FEATURE) fails under FMA | REPRODUCED | E1 host; arm64 device E5 | `diagnostics/CR-07/host-runs/; diagnostics/CR-07/fp_oracle_diag.out` |
| CR-07-S4 | No fp-contract policy: NDK arm64 fuses even at -O0, CI is x86_64-only and the default host loop (g++ -O1) never fuses | REPRODUCED | E1 | `diagnostics/CR-07/contract_probe.out` |
| CR-07-S5 | CI FAST runs no host native suites and no corpus decode verdicts | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | `diagnostics/CR-07/CMD.txt` |

## CR-01 — GLB export writes a face-supported CAD Body at the origin, not where it is drawn

**Claim under test:** FUNCTION-COUNCIL-R2 OPP-01 / RC-01 (B-02 = E-01), rated P1. Claim: GLB export places a face-supported CAD Body (CAD-A3) at its stored identity transform instead of its derived world placement. The exported file therefore differs from what the viewport draws and picks.

**Baseline:** product tree `70a6c14`. `app/` has no diff between `70a6c14` and HEAD.

### Contract (quoted)

- `CLAUDE.md:249-254`: "A face-supported body's world placement is DERIVED (`ConstructionScene::resolveWorldModel` composes `producerWorldModel · faceFrame` every snapshot), so moving, rotating or editing the producer carries the dependent with it and no follow-state is stored; such a body is NOT independently transformable."
- `CLAUDE.md:1407-1408`: "it exports EVERY body, because a file quietly missing an object the user can see and move is worse than no file."
- `CLAUDE.md:1413-1420`: "A static export bakes `R` and `S` into the geometry and leaves `T` on the node … `Model = T · L` … The node writes a `translation` and nothing else."
- `app/src/main/cpp/forgeshape_gltf_export.h:140-142`: "Narrowed to float exactly once, here, from the same model matrix the renderer consumes, so the exported place is bit-for-bit the place drawn."
- `app/src/main/cpp/forgeshape_scene.h:160-164`: "A face-supported body's world placement is DERIVED from the producer this names, so `transform()` is not its placement."
- `PRODUCT.md:2070`: "**Every body in the scene**, each in the place, at the angle and at the size you put it."

Together these make the expected behaviour contractual, with no new owner semantics needed. Apply the ARCH-OWNER-07 split to the *derived* model:
- `T` is the resolved translation.
- `L` is the resolved linear part: a right-handed face frame times the producer's positive-determinant `L`, so `det > 0`.

### Root cause (source, E2)

- `forgeshape_gltf_export.cpp:178-187` takes `model`/`local` from `body.transform()` for every body.
- The renderer and picking draw from `snapshot()`, which calls `resolveWorldModel` (`forgeshape_scene.cpp:359,374`).
- `resolveWorldModel` returns `transform().modelMatrix()` only for independent bodies (`:294-297`).
- For a face-supported body it returns `producerWorldModel * cadFaceFrameMatrix(face)` (`:292-325`), recursively down a chain.
- JNI `exportGlb` calls this same `exportSceneAsGlb` on the live scene (`forgeshape_jni.cpp:8517`).

### Setup

The host program is `diagnostics/CR-01/cr01_export_face.cpp`. It links the product objects built exactly as `scripts/host-native-selftests.sh` builds them (same flags; every `forgeshape_*.cpp` except jni and renderer; vendored Manifold).

Scenes:
- **S1:** a 2×1×3 rectangle producer, plus a dependent on its CapFar.
- **S2:** the same producer, dependent on a Side face.
- **S3:** a chain A→B→C, each on the previous body's CapFar.
- **S4:** the producer moved to (1,2,3) and rotated (0,30,45), with a cap dependent and a side dependent.
- **C1 (control):** a world CAD body with full TRS and non-uniform scale.
- **C2 (control):** a Construction primitive with full TRS.

Read-only fixtures, loaded through `decodeProject` + `loadProjectDocument`: `cad_face_sketch_cap_v2`, `cad_face_sketch_side_v2`, `cad_face_chain_v2`, `mixed_cad_face_v2` and `cad_rectangle_v1`.

For each body the program prints the snapshot, resolveWorldModel, transform() and export translations. It also prints the expected world AABB (resolveWorldModel × the re-derived source mesh) against the exported one (baked positions + node T). The written `.glb` bytes are parsed independently by `parse_glb.py`.

### Commands

The full sequence is in `diagnostics/CR-01/CMD.txt`:

```
g++ $COMMON $D/cr01_export_face.cpp $S/obj/*.o $S/kernel/libmanifold.a -o $S/cr01        # exit 0
$S/cr01 $S/glb testdata/forge/v1/cad_face_sketch_cap_v2.forge … cad_rectangle_v1.forge     # exit 0
python3 -I $D/parse_glb.py $S/glb/*.glb                                                     # exit 0
```

### Actual output (quoted from `cr01_export_face.out` / `parse_glb.out`)

```
== CASE S1_cap
  body 2 kind=CAD faceSupported=1 resolved=1 inSnapshot=1
    snapshot.T=(0.0000,0.0000,3.0000) resolveWorldModel.T=(0.0000,0.0000,3.0000) transform().T=(0.0000,0.0000,0.0000) export.T=(0.0000,0.0000,0.0000)
    expected_world_aabb=[-0.2500,-0.2500,3.0000]..[0.2500,0.2500,3.5000]
    exported_world_aabb=[-0.2500,-0.2500,0.0000]..[0.2500,0.2500,0.5000]
    max_abs_aabb_delta=3.0000 verdict=MISMATCH
== CASE S2_side
  body 2 … resolveWorldModel.T=(0.0000,-0.5000,1.5000) … export.T=(0.0000,0.0000,0.0000)
    resolveWorldModel.col0=(1.000,0.000,0.000) col2=(0.000,-1.000,0.000)  export.local.col0=(1.000,0.000,0.000) col2=(0.000,0.000,1.000)
    max_abs_aabb_delta=1.2500 verdict=MISMATCH
== CASE S3_chain   body 2 delta=1.0000 MISMATCH; body 3 delta=1.5000 MISMATCH
== CASE S4_producer_moved_rotated
  body 1 (producer) … export.T=(1.0000,2.0000,3.0000) … max_abs_aabb_delta=0.0000 verdict=MATCH
  body 2 … resolveWorldModel.T=(2.0607,3.0607,5.5981) … export.T=(0.0000,0.0000,0.0000) … max_abs_aabb_delta=5.6561 verdict=MISMATCH
  body 3 … resolveWorldModel.T=(1.8839,2.1768,4.2990) … export.T=(0.0000,0.0000,0.0000) … max_abs_aabb_delta=4.3722 verdict=MISMATCH
== CASE C1_world_cad_trs     body 1 … max_abs_aabb_delta=0.0000 verdict=MATCH
== CASE C2_construction_trs  body 1 … max_abs_aabb_delta=0.0000 verdict=MATCH
F_cad_face_sketch_cap_v2 body 2 delta=1.5000 MISMATCH; F_cad_face_sketch_side_v2 body 2 delta=1.2500 MISMATCH;
F_cad_face_chain_v2 body 2 delta=1.7500, body 3 delta=2.0000 MISMATCH; F_mixed_cad_face_v2 body 4 delta=2.7500 MISMATCH;
F_cad_rectangle_v1 body 1 MATCH
```

`parse_glb.out` shows the bytes agree. For example, in `S4_producer_moved_rotated.glb`:
- node 1: `transform_keys=['translation'] translation=(0.0000,0.0000,0.0000)`, `world_aabb_from_bytes=[-0.2500,-0.2500,0.0000]..[0.2500,0.2500,0.5000]`.
- node 0 (the producer): `translation=(1.0000,2.0000,3.0000)`.

`glb_sha256.out`: `S1_cap.glb` and `S2_side.glb` are **byte-identical** (`9934da55…f3eb`, 2944 bytes). The file cannot tell a cap-supported dependent from a side-supported one.

Tallies: 11 `verdict=MISMATCH`, every one of them a `faceSupported=1` body, and that is all 11 face-supported bodies. 12 `verdict=MATCH`, every independent body. 1 Imported Mesh with no re-derived source; its translation equals the resolved one.

### Classification per symptom

| # | Symptom | Class | Conf. |
|---|---|---|---|
| 1 | Cap dependent exported at the origin (Δ 3.0 m) | REPRODUCED | E1 |
| 2 | Side dependent loses the face-frame rotation and offset (Δ 1.25) | REPRODUCED | E1 |
| 3 | Cap and side exports byte-identical | REPRODUCED | E1 |
| 4 | Chain: every level at the origin (Δ 1.0 / 1.5; fixture 1.75 / 2.0) | REPRODUCED | E1 |
| 5 | Producer move/rotate ignored by dependents (Δ 5.66 / 4.37) | REPRODUCED | E1 |
| 6 | Committed fixtures cap/side/chain/mixed misplaced on export | REPRODUCED | E1 |
| 7 | Control: independent bodies also misplaced | REFUTED (12/12 match) | E1 |
| 8 | Expected behaviour is contractual | SOURCE-CONFIRMED-NOT-REPRODUCED (contract reading) | E2 |

### Falsification criterion

The claim would have been refuted if any of these had held:
- For any face-supported body, `export.T` equalled `resolveWorldModel.T` and `max_abs_aabb_delta ≤ 1e-4`.
- The parsed node translation in the `.glb` equalled the resolved translation.
- `S1_cap.glb` and `S2_side.glb` differed.

None of these occurred.

### Controls

- A world CAD body with rotation, non-uniform scale and translation (C1) matches with Δ 0.
- A Construction primitive with full TRS (C2) matches with Δ 0.
- Every producer and root matches with Δ 0, including the rotated and moved S4 producer.
- `cad_rectangle_v1` and the Construction and world-CAD bodies in `mixed_cad_face_v2` match with Δ 0.
- Every GLB node writes only `translation`, so the T-on-node rule holds for independent bodies.

### Why existing tests miss it

- `forgeshape_gltf_export_selftest.cpp` contains 0 `addCadBody` calls.
- `CADR0_37` (`forgeshape_cad_selftest.cpp:1817-1852`) exports only a world-plane CAD body.
- The CAD-A3 self-tests assert `resolveWorldModel` but never export.
- `GlbExportTest` (androidTest) has no face-supported CAD case.
- The `cad_face_*` fixtures are exercised only by the codec and digest checks.
- For every body those tests build, `transform().modelMatrix()` equals `resolveWorldModel`, so the defect cannot show.

The coordinator's focused CI DEVICE run (which includes `GlbExportTest`) is not visible to this seat, and nothing here is claimed from it.

### Not tested

- A face-supported body whose *stored* transform is non-identity (if one could be loaded). The export would then apply that stale transform.
- Revolve-based or later-feature (Add/Cut) supported placements. Dependents of those would read the same `transform()` path (E2 only).

### Owner decision

None required. The fix direction follows from ARCH-OWNER-07 applied to the derived model. An optional, non-blocking confirmation: a dependent's GLB pivot would be its face-frame origin.

### Confidence and severity

E1: reproduced in this closeout, with numbers and parsed bytes. Severity P1 is upheld. The export reports Ok while it silently writes visible bodies at the wrong place and orientation, which is the outcome `CLAUDE.md:1407-1408` calls worse than no file.

Files: `artifacts/function-council-r2-closeout/diagnostics/CR-01/{cr01_export_face.cpp, cr01_export_face.out, parse_glb.py, parse_glb.out, glb_sha256.out, CMD.txt}`.


## CR-02 — A face-supported CAD Body accepts transform writes and gizmo drags that change no drawn placement

**Claim under test.** The previous audit's B-01 (P2) and B-03 said a face-supported CAD Body can be given a typed transform and a gizmo drag. Neither moves what is drawn, but each records an Undo step and dirties the project. They also said the documented guard does not exist.

**Baseline SHA.** Product tree `70a6c14`. `app/` is unchanged at the audit HEAD.

**Contract.**
- `CLAUDE.md:250-254`: "A face-supported body's world placement is DERIVED (`ConstructionScene::resolveWorldModel` composes `producerWorldModel · faceFrame` every snapshot), so moving, rotating or editing the producer carries the dependent with it and no follow-state is stored; such a body is NOT independently transformable (`SceneObject::isFaceSupportedCad`)."
- `ARCHITECTURE.md:2607-2608`: "`SceneObject::isFaceSupportedCad` marks such a body so the gizmo and the exact-value editors leave it alone".
- `CLAUDE.md` hard rule: "A control that cannot succeed is not drawn … The domain guard stays."

### (a) Entry from JNI — source only (E2, not executed)

- **Typed-transform entry.** `applyBoxTransform` (`forgeshape_jni.cpp:1333-1385`, wrapper at `2538-2556`) refuses only `!hasProject()` (`1349-1356`) and `activeBody().locked()` (`1357-1381`). It then opens `ScopedConstructionEdit` and calls `applyConstructionTransform` (`1383-1384`). That writes `constructionScene().activeBody().transform()` (`forgeshape_transform.cpp:395-409`).
- **Gizmo activation.** `setGizmoActive` (`forgeshape_jni.cpp:3833-3856`) refuses only Sculpt and Locked.
- **Touch drag.** The touch path starts `gizmo.beginDrag` when `gizmo.active() && Down && count==1` (`forgeshape_jni.cpp:9932-9934`).
- **Gizmo session.** `GizmoSession::snapshot` takes its pivot from the **stored** transform (`forgeshape_gizmo.cpp:1207-1209`), and `applyTarget` writes `body->transform()` (`forgeshape_gizmo.cpp:1424`).
- **Where the predicate is read.** `isFaceSupportedCad` (`forgeshape_scene.h:168`) has two product readers: Duplicate (`forgeshape_body_commands.cpp:205`) and the verification query `sceneActiveBodyIsFaceSupportedCad` (`forgeshape_jni.cpp:7676-7680`).
- **Not affected.** Body Dimensions and Relative Scale refuse a CAD Body (`RefusedRepresentation`, `forgeshape_body_dimensions.cpp:436`), so they are not a route in.

### (b) Entry from UI — source only (E2; no device or emulator run here)

- **Tool Rail.** The rail always carries the `tool_rail_place` (Transform) entry outside Sculpt and sketching: `[Shape, place]`, or `[place]` alone (`WorkspaceTrailingHostView.java:605-622`).
- **Gizmo.** It is offered when `!sculpting && !isSketching() && !bodyDimMode && tool==TRANSFORM && !locked && id!=NO_OBJECT` (`EditorWorkspaceView.java:3028-3033`). The trailing host's `transformOffered` uses the same terms (`3046-3049`). Neither has a face-support term.
- **Placement editor.** Under Transform the inspector hosts the placement editor (`EditorWorkspaceView.java:3713-3715`). It submits through `NativeViewport.applyBoxTransform` (`ConstructionPlacementEditorView.java:269`). It displays `boxTransform()`, which is the **stored** transform (`forgeshape_jni.cpp:2516-2530`), not the derived placement.
- **Java callers.** In Java product code, `FaceSupported` appears only in the native declaration (`NativeViewport.java:3194`).
- **Status.** The UI reachability is SOURCE-CONFIRMED-NOT-REPRODUCED. It is not upgraded to runtime.

### (c) Domain effect on the host — executed (E1)

**Setup.**
- The diagnostic uses the **process-scoped** objects the JNI uses: `constructionScene()`, `constructionHistory()`, `gizmoSession()`, `sculptSession()` and `applyConstructionTransform()`.
- `forgeshape_jni.cpp` cannot be built on the host. The guard-and-write sequences of `applyBoxTransform` (`forgeshape_jni.cpp:1349-1385`) and `setGizmoActive` (`forgeshape_jni.cpp:3833-3856`) are therefore copied verbatim, minus logging.
- Bodies:
  - producer id1: XY rectangle 2×1, depth 3;
  - dependent id2: sketch on the producer's `CapFar`, `faceSupported=1`;
  - control id3: world-plane CAD body, rectangle 1×1, depth 1.

**Commands.** These are in `artifacts/function-council-r2-closeout/diagnostics/CR-02/CMD.txt`. The build uses the flags and source list of `scripts/host-native-selftests.sh` (98 objects plus `libmanifold.a`). Results: `build_exit=0`, `run_exit=0`.

**Actual output.** From `cr02_transform_face.out`, quoted:

```
1 setup producer=1 dependent=2(faceSupported=1) control_world_cad=3(faceSupported=0) hasProject=1 undoDepth=0
1 pristine encode status=0 bytes=573 fnv=0x7ebc971c4ebdaac4
C1 active=2 applyBoxTransform-domain status=applied refusal=none
C1 before undoDepth=0 fingerprint=0xbac400174411ca29 stored.pos=(0.0000,0.0000,0.0000) stored.rot=(0.0000,0.0000,0.0000) resolved=1 world.t=(0.0000,0.0000,3.0000)
C1 after  undoDepth=1 fingerprint=0x4cc3bb2b8d3f249f stored.pos=(5.0000,0.0000,0.0000) stored.rot=(0.0000,0.0000,30.0000) resolved=1 world.t=(0.0000,0.0000,3.0000)
C1 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0
C1 save: encode status=0 bytes=573 fnv=0xed440e2232e2d767 equalsPristine=0; decode status=0 dependent stored pos=(5.0000,0.0000,0.0000) rotZ=30.0000
C1 fingerprint after undo equals pristine=1
C2 setGizmoActive(true) -> ACTIVE:1
C2 gizmo visible=1 pivot=(0.0000,0.0000,0.0000) vs drawn world.t=(0.0000,0.0000,3.0000)
C2 grab px=(822.7,934.7) hit=1 beginDrag=1
C2 updateDragAny=1 commitDrag(recorded)=1
C2 after  undoDepth=1 fingerprint=0xc0e4609a0979f1d3 stored.pos=(0.9038,0.0000,0.0000) stored.rot=(0.0000,0.0000,0.0000) resolved=1 world.t=(0.0000,0.0000,3.0000)
C2 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0
```

### (d) Controls (E1)

```
D1 control world-plane CAD active=3 status=applied refusal=none
D1 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=1      (world.t 0 -> 5)
D2 producer VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=1
D2 dependent VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=0 worldMoved=1  (world.t (0,0,3) -> (5,0,3))
D3 gizmo visible=1 pivot=(0.0000,0.0000,0.0000) vs drawn world.t=(0.0000,0.0000,0.0000)
D3 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=1
D4 locked dependent status=rejected refusal=BodyLocked setGizmoActive(true)->REFUSED:body_locked
D4 VERDICT undoDelta=0 fingerprintChanged=0 storedMoved=0 worldMoved=0
```

What the controls show:
- **D1 and D3.** An independent body accepts the same typed write and the same drag, and it moves.
- **D2.** `resolveWorldModel` is live: moving the producer carries the dependent. So C1 and C2 leaving `worldMoved=0` is not a stale read.
- **D4.** The existing guard mechanism works for Lock. It simply has no face-support term.

### Classification per symptom

| # | Symptom | Class | Confidence |
| --- | --- | --- | --- |
| a-1 | `applyBoxTransform` has no face-support guard | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| a-2 | `setGizmoActive` and the touch `beginDrag` have no face-support guard | SOURCE-CONFIRMED-NOT-REPRODUCED (the domain copy returned `ACTIVE:1`) | E2 |
| b | Transform rail entry, gizmo and placement editor are drawn for the body | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| c-1 | Typed write: +1 Undo, fingerprint changed, stored moved, drawn unchanged | REPRODUCED | E1 |
| c-2 | Gizmo pivot drawn 3 m from the body; drag: +1 Undo, fingerprint changed, drawn unchanged | REPRODUCED | E1 |
| c-3 | Edit reaches `.forge`: same-looking project, different bytes; decoder accepts | REPRODUCED | E1 |
| B-03 | `ARCHITECTURE.md:2607-2608` describes a guard that does not exist | SOURCE-CONFIRMED-NOT-REPRODUCED (doc divergence) | E2 |

**Shared cause with CR-01.** In both, consumers disagree about where a face-supported body's placement lives.
- The stored transform is treated as truth by the writers (this item), and by the fingerprint, the codec and the GLB exporter (CR-01).
- `resolveWorldModel` (`forgeshape_scene.cpp:293-326`) never reads it.
- These are two separate symptoms. Refusing the writes would **not** fix CR-01: with an untouched identity transform the exporter still writes (0,0,0) for a body drawn at (0,0,3).

**Falsification criterion.** Any of these would have refuted the claim:
- C1 printing `status=rejected` or `undoDelta=0`;
- C2 printing `REFUSED:…` or `beginDrag=0`;
- `worldMoved=1` on C1 or C2 (the write really moving the drawn body);
- `equalsPristine=1`;
- a grep hit for `isFaceSupportedCad` in the transform, gizmo or Java offered/inspector paths.

**Why existing tests miss it.**
- The gizmo and transform self-tests use only independent bodies.
- `forgeshape_cad_a3_selftest.cpp` checks that the dependent follows its producer, but never writes the dependent's own transform.
- The androidTests that touch face-supported bodies (`SpatialSketchTest`, `HomeFlowTest`, `CadA3VisualEvidenceTest`, `OwnerProjectShellSketchSupportTest`) assert `sceneActiveBodyIsFaceSupportedCad()==true` and never open Transform.
- The `CADB` v2 fixtures all carry an identity transform on the dependent.

**Owner decision.**
- No decision is needed for the guard: the contract already states the rule. The fix would be a named refusal in `applyBoxTransform` and in `setGizmoActive`/`beginDrag`, plus withdrawing Transform above JNI.
- OWNER-SEMANTICS-REQUIRED for files already saved with a non-identity stored transform on a face-supported body: should the decoder refuse it, ignore it, or canonicalise it? (Seat B's owner question 1.)

**Confidence.** E1 for the domain effect (c-1, c-2, c-3) and the controls. E2 for JNI and UI reachability (a, b) and B-03. No device or emulator evidence. The coordinator's focused CI DEVICE run is not seen and not claimed.

## CR-03 — Import GLB while a sketch (or the support chooser) is open

**Claim under test.** P2 from Council R2 (A-04 = C-02 = D-02 = E-03; SUMMARY.md:121-123): "Import GLB is offered and accepted while a sketch or the support chooser is open." This closeout goes further than "the JNI has no sketch guard". It measures what the accepted import actually does to the open sketch, the history, the fingerprint, the encoding and the sketch's own later commit.

**Baseline.** Product tree `70a6c14`. `app/src/main/cpp` and `app/src/main/java` are unchanged in `70a6c14..HEAD`.

### Contract
- `CLAUDE.md:227-231`: "While a sketch is being DRAWN … creation, deletion, body switching, freeze and Construction Undo/Redo are refused below JNI and withdrawn above it."
- `CLAUDE.md:303-308`: the first Extrude creates the project through `commitFirstCadProject` "… so the new project starts with an EMPTY history". `CLAUDE.md:1447-1448`: "an empty scene is Home, reached only by leaving the project".
- `CLAUDE.md:332-333`: "While the chooser is up the viewport belongs to choosing". The chooser has no explicit creation ban. The precedent is `freeformModeRefusalLocked` (`forgeshape_jni.cpp:5442-5446`), which refuses when `sketchSession().active() || supportChooser().active()`.
- Sibling guards that exist: `sceneAddBody` `in_sketch` (`forgeshape_jni.cpp:3246-3265`) and `runHistoryStep` `in_sketch` (`forgeshape_jni.cpp:8009-8015`).
- The import guard that exists: `forgeshape_jni.cpp:8594-8598` checks `sculptSession().inSculptMode()` and nothing else. `commitImportedGlbScene` (`forgeshape_import_commit.cpp:50-72`) takes no sketch or chooser parameter.

### UI (from source, not run)
- `EditorWorkspaceView.java:2823` calls `projectPopover.showImportAvailable(!sculpting)`, which hides the row only in Sculpt (`ProjectActionsPopoverView.java:202-206`).
- `ProjectDrawerPolicy.java:109-111` sets `markShown = projectOpen && !bootstrap`, so the ForgeShape mark stands while sketching in a project. `ProjectDrawerPolicy.java:162-163` routes `IMPORT_GLB -> onImportGlbRequested`. `EditorWorkspaceView.java:5835-5845` and `5925-5931` refuse only in Sculpt.
- So Import is drawn and one tap away while sketching or with the chooser up in a project: SOURCE-CONFIRMED, E2.
- In the CAD bootstrap the mark is GONE (`GlobalToolbarView.java:681-686`; `bootstrap` at `EditorWorkspaceView.java:2814-2820`), so a bootstrap import is not reachable from the UI.

### Setup and commands
The host diagnostic `diagnostics/CR-03/cr03_import_during_sketch.cpp` uses the process-scoped singletons the JNI uses. It replicates three JNI bodies call for call:
- `importGlbDurable` (8560-8609)
- `runHistoryStep`'s guards (7995-8020)
- `sketchCommit`'s branches (4487-4530)

Logging, the mutex and `endSketchView` are omitted; none of them changes domain state. The GLB is one default Box written by the product's own `exportSceneAsGlb` (1648 bytes). The project is seeded inside the session-init bracket. The sketch is XY with one rectangle added through the self-tests' `addSketchEntity` seam (state `Editing`, not finished).

Build: `bash artifacts/function-council-r2-closeout/diagnostics/CR-03/build.sh`. It uses the same COMMON flags and source list as `scripts/host-native-selftests.sh` and exits 0 with `BUILD_OK`.

Run: `/tmp/claude-0/fc2c-scratch/CR-03/cr03 {A|B|C|D|E}`. All five runs exit 0. See `CMD.txt`.

### Actual output (quoted)
Scenario A: sketch open, then import.
```
[before_import] hasProject=1 bodies=1 active=1 undo=0 redo=0 editInProgress=0 sketch_active=1 sketch_state=Editing sketch_entities=1 ... fingerprint=22a5519654955ab0
  import: parse=Ok commit=Ok
importGlbDurable_return=0 objects=1 firstObjectId=2 activeBodyId=2
[after_import] hasProject=1 bodies=2 active=2 undo=1 redo=0 editInProgress=0 sketch_active=1 sketch_state=Editing sketch_entities=1 ... fingerprint=937217cfca29dbcc
fingerprint_changed=1
  [after_import] encode=Ok bytes=1159 decode=Ok decoded_bodies=2
jni_undo_while_sketch=REFUSED:in_sketch
finish=Ok setExtrude=Ok selectionChosen=1
sketchCommit=Ok created=3 firstProjectPath=0
[after_commit] ... bodies=3 active=3 undo=2 ...
  [after_commit] encode=Ok bytes=1334 decode=Ok decoded_bodies=3
jni_undo_1=MOVED   -> bodies=2 (CAD body removed)
jni_undo_2=MOVED   -> bodies=1 (import removed)
jni_undo_3=NOTHING
```
Scenario B: sketch open, import, cancel the sketch, then Undo.
```
[after_sketch_cancel] ... bodies=2 active=2 undo=1 ...
jni_undo_1=MOVED
[after_undo_1] ... bodies=1 active=1 undo=0 redo=1 ...
```
Scenario C: support chooser open, then import.
```
[before_import] ... bodies=1 ... chooser_active=1 ...
importGlbDurable_return=0
[after_import] ... bodies=2 active=2 undo=1 ... chooser_active=1 ...
jni_undo_while_chooser=MOVED
```
Scenario D (CONTROL): Sculpt mode.
```
sculpt_source=1 freeze=1
  import: parse=Ok commit=RefusedInSculpt
importGlbDurable_return=1005 (expected 1005 = base + RefusedInSculpt)
[after_import] ... bodies=1 ... undo=0 ... sculpt=1 fingerprint=cd0cfc3922e022fd
fingerprint_changed=0
```
Scenario E: CAD bootstrap (no project) with a sketch open, by direct call.
```
[before_import] hasProject=0 bodies=0 ... sketch_active=1 sketch_state=Editing ...
importGlbDurable_return=0
[after_import] hasProject=1 bodies=1 active=1 undo=1 ...
sketchCommit=Ok created=2 firstProjectPath=0
[after_commit] ... bodies=2 ... undo=2
jni_undo_2=MOVED
[after_undo_2] hasProject=0 bodies=0 active=0 undo=0 redo=2 ...
```

### Classification per symptom
| # | Symptom | Class | Conf. |
| --- | --- | --- | --- |
| S1 | Import accepted while a sketch is being drawn. It creates a body, switches the active body to it, records one Construction step and moves the fingerprint. This contradicts `CLAUDE.md:227-231`. | REPRODUCED | E1 |
| S2 | The Import row is drawn and reachable while sketching in a project | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 |
| S3 | "The import corrupts the sketch, the encoding or the later commit". The sketch stays intact (Editing, 1 entity), encode and decode are Ok, the commit is Ok, and the two Undos peel exactly one step each. | REFUTED | E1 |
| S4 | The import's step cannot be undone while the sketch stays open (`REFUSED:in_sketch`). It can be undone after Cancel or Finish. | REPRODUCED | E1 |
| S5 | With the chooser open, import is accepted and Construction Undo also runs | OWNER-SEMANTICS-REQUIRED (behaviour E1) | E1 |
| S6 | Control: Sculpt import is refused (1005) and changes nothing | REPRODUCED (control holds) | E1 |
| S7 | Bootstrap import by direct call creates a project with `undo=1`, bypassing `commitFirstCadProject`. Two Undos return to an empty scene (`hasProject=0`, redo=2). | REPRODUCED (direct call only) | E1 |
| S8 | S7 reachable from the UI | REFUTED (mark withdrawn in bootstrap) | E2 |

Severity reading: the open defect is a guard and contract inconsistency (S1, S2, S4, S5). It is not data loss or corruption (S3 refuted). The P2 rating is reasonable. The fingerprint difference after Undo (for example `74e74b7c…` against the pre-import `22a55196…`) is consistent with the documented never-rolled-back `ObjectId` high-water mark. That reading is an inference (E5) and is not claimed as a defect.

### Falsification
- S1 would be refuted by a non-zero return, or by unchanged bodies, active body, undo depth or fingerprint after an import with `sketch_active=1`.
- S3 would have been confirmed by an encode/decode refusal, lost sketch entities, a commit refusal, or an Undo that removed both steps at once.
- S7 would be refuted by a refusal while `hasProject=0`.

### Controls
- Scenario D (Sculpt) on the identical path is refused, with no state change.
- Scenario B shows the import is an ordinary single Construction step once the sketch closes.

### Why existing tests miss it
`ImportGlbSculptGuardTest` is the only androidTest that calls `importGlbDurable`, and it covers Sculpt only. `forgeshape_gltf_import_selftest.cpp` calls `commitImportedGlbScene` but never opens `sketchSession()` or `supportChooser()`. No JVM test asserts Import visibility in a sketch state. The commit function's signature cannot observe either session.

### Owner decision
1. Should Import be refused below JNI (`in_sketch`, as `sceneAddBody` is) and withdrawn above it (`showImportAvailable(!sculpting && !sketching)`) while a sketch is open?
2. Is the support chooser a creation-in-progress state, as Freeform already treats it, so that Import and Construction Undo are refused while it is up?
3. Should `importGlbDurable` refuse when `!hasProject()`, as defence in depth (S7 is not reachable from the UI)?

### Confidence
- E1 for S1, S3-S7: reproduced in this closeout on the host, with output.
- E2 for S2 and S8: source.
- No device or emulator evidence is claimed.

## CR-04 — Sculpt Undo after a stroke too large to keep in history (NotRetained) lands on states that never existed

**Claim under test.** Council-R2 D-01 (P2; earlier `function-council-r1` D4): Undo after an over-cap (NotRetained) sculpt stroke lands on a mesh state that never existed, and `hasEdits` is cleared.

**Baseline.** Product tree `70a6c14` (`git diff --stat 70a6c14 HEAD -- app` is empty). The test runs on the host. No device or emulator was used.

**Contract (quoted).**
- `CLAUDE.md:1008-1010`: "a cancelled stroke whose positions stand records them too, because the deformation the user can see must be one they can take back."
- `CLAUDE.md:1017-1018`: "a stroke too large to retain still APPLIES and says so by name rather than silently. **`hasEdits` is a stored fact, not `undoDepth > 0`**"
- `CLAUDE.md:1024-1025`: "A Freeze or a destructive Reset from source clears it; Undo cannot cross that boundary."
- `CLAUDE.md:1034-1044`: "A ROW is a STATE and not an entry … landing the user on a state they did not tap is worse than a refusal they can see."
- `forgeshape_sculpt_history.h:163-168`: "A stroke larger than this still APPLIES … but it is not retained, and it says so by name".
- `forgeshape_sculpt_history.h:86-95`: `hasEdits` "is what the destructive-reset confirmation asks and what `.forge` stores".

**Setup.** `diagnostics/CR-04/cr04_notretained_undo.cpp` drives every stroke through the product path: `SculptSession::beginStroke/updateStroke/endStroke`, Clay at strength 1. The delta is built by `SculptStroke::buildDelta` and recorded by `SculptHistory::record`; no delta is written by hand.
- Mesh: a frozen 250×250 grid, 62,500 vertices at 0.01 m spacing, facing +Y. Camera 6 m above, viewport 1080×2400.
- Normal strokes: N at 24 px radius, about 150 vertices and about 4.3 KB each, all Recorded.
- Oversized stroke: one at 600 px radius, about 62,400 vertices and about 1.75 MB, over the 1 MiB `kMaxSculptHistoryEntryBytes`.
- After that: `undoStroke()`, `redoStroke()`, `jumpToHistoryCursor(0)`.
- After each step, the mesh is compared vertex by vertex against **every state that existed** (S0..Sn, S_big), giving max abs diff and the count of differing vertices.

The checks state the contract, not today's behaviour:
- K1: the stroke applies.
- K2: it is counted NotRetained and nothing is pushed.
- K3: the mesh after Undo equals an existing state. A refused Undo that leaves S_big standing also passes.
- K4: `hasEdits` after Undo equals (mesh ≠ S0).
- K5: Redo lands on an existing state.
- K6: a jump to row 0 lands on an existing state.
- K7: `hasEdits` after the jump equals (mesh ≠ S0).

**Commands.** These are in `diagnostics/CR-04/CMD.txt`:
1. Compile every `forgeshape_*.cpp` except `forgeshape_jni.cpp` and `forgeshape_renderer.cpp`, with the flags `scripts/host-native-selftests.sh` uses, into `libfs.a`.
2. Link with the cached vendored `libmanifold.a`.
3. Run `./cr04 > cr04.out` → exit 1. A second run is byte-identical (`cmp`).

**Actual output (quoted from `cr04.out`).**
```
=== SCENARIO SUBJECT_big_stroke_after_3_retained (normal=3 big=1)
INFO big stroke moved=1 verticesChanged=62416 payloadIfRetained~=1747776 cap=1048576 undoDepth=3 notRetained=1 hasEdits=1
PASS K1_oversized_stroke_applies
PASS K2_and_is_named_not_retained_and_not_pushed
INFO navigator before undo: cursor=3 stateCount=4
INFO undo=ok hasEdits=1
  afterUndo vs S2: maxAbs=0.024054 differingVertices=62268
  afterUndo vs S_big(or dup of Sn in control A): maxAbs=0.047676 differingVertices=148
  afterUndo nearest existing state maxAbs=0.024054 match=-1
FAIL K3_undo_lands_on_a_state_that_existed
PASS K4_hasEdits_truthful_after_undo
INFO redo=ok hasEdits=1
  afterRedo nearest existing state maxAbs=0.023930 match=-1
FAIL K5_redo_lands_on_a_state_that_existed
INFO jump0=ok hasEdits=0
  afterJump0 vs S0: maxAbs=0.024054 differingVertices=61968
  afterJump0 nearest existing state maxAbs=0.024054 match=-1
FAIL K6_navigator_row0_is_a_state_that_existed
FAIL K7_hasEdits_truthful_after_jump
=== SCENARIO SUBJECT_big_stroke_after_1_retained (normal=1 big=1)
INFO undo=ok hasEdits=0
  afterUndo nearest existing state maxAbs=0.024248 match=-1
FAIL K3_undo_lands_on_a_state_that_existed
INFO afterUndo differsFromS0=1 hasEdits=0
FAIL K4_hasEdits_truthful_after_undo
INFO redo=ok hasEdits=1
  afterRedo nearest existing state maxAbs=0.011322 match=-1
FAIL K5_redo_lands_on_a_state_that_existed
FAIL K6_navigator_row0_is_a_state_that_existed
FAIL K7_hasEdits_truthful_after_jump
RESULT failures=9
```

**Controls.** Both control scenarios pass every check:
- CONTROL_A (3 strokes, no oversized stroke): `afterUndo nearest existing state maxAbs=0.000000 match=2`, redo `match=3`, jump0 `match=0` with `hasEdits=0`. The comparison method itself is sound.
- CONTROL_B (oversized stroke on an EMPTY stack, the SCUNDO_23 shape): `undo=nothing_to_do`, mesh == S_big (`match=1`), `hasEdits=1`.

**Root cause (E2).**
- `SculptHistory::record` (`forgeshape_sculpt_history.cpp:80-101`) clears `redo_` (line 91) and returns `NotRetained` (line 96), but it **keeps `undo_`**.
- The remaining entries hold absolute positions and `hasEdits` captured *before* the unretained stroke.
- `undoStroke` / `redoStroke` (`forgeshape_sculpt.cpp:1640-1678`) write those values over the new geometry through `applyHistorySide`, then call `restoreEditedFlag`.
- A Freeze clears `undo_` (`forgeshape_sculpt.cpp:1447`). NotRetained does not.

**Classification per symptom.**

| # | Symptom | Classification | Confidence |
|---|---|---|---|
| 1 | Undo after NotRetained lands on a state that never existed (N=3 and N=1) | REPRODUCED | E1 |
| 2 | `hasEdits` false while the deformation stands: after one Undo when the top entry was the first after Freeze (N=1), and after a jump to row 0 (N=3) | REPRODUCED | E1 |
| 3 | Redo after that Undo also lands on a state that never existed, so S_big cannot be recovered (new beyond D-01) | REPRODUCED | E1 |
| 4 | The navigator shows `stateCount=n+1` rows past the unretained stroke, and row 0 is a state that never existed | REPRODUCED | E1 |
| 5 | The oversized stroke APPLIES and is named (`notRetained` counted, nothing pushed) | REFUTED as a defect (contract holds) | E1 |

Symptom 2 refines D-01: with ≥2 older entries a single Undo keeps `hasEdits=1`, which is truthful. D-01's "clears hasEdits" holds only once a step reaches an entry whose `beforeHasEdits=false`.

Consumers of the wrong `hasEdits` (E2):
- `SculptContextView.java:243`: Reset Sculpt skips its confirmation when `SCULPT_HAS_EDITS==0`.
- `forgeshape_project_state.cpp:230`: the `SCUL` section stores `hasEdits`.

**Falsification criterion.** The defect would be refuted if any subject scenario printed `match>=0` with `maxAbs=0.000000` after Undo, Redo or jump0. It would also be refuted by `undo=nothing_to_do` with the mesh equal to S_big (CONTROL_B's behaviour), or by a `hasEdits` value equal to `differsFromS0`. None of these occurred.

**Why the existing tests miss it.** `SCUNDO_23` (`forgeshape_sculpt_selftest.cpp:1774-1797`) has three gaps:
- It drives `SculptHistory` alone with synthetic deltas, never `SculptSession::undoStroke`, so it never applies an entry to geometry.
- Its precondition is an empty undo stack (`SCUNDO_23_precondition_a_redo_entry_exists`: `undoDepth()==0`, line 1781), the one shape in which the defect cannot appear. CONTROL_B shows that shape passes.
- Its check named `SCUNDO_23_an_oversized_stroke_is_refused_by_name` asserts `NotRetained`; the stroke is applied, not refused.

No JVM or androidTest file references NotRetained.

**Reachability.** One stroke must change more than about 37,449 vertices (1 MiB / 28 B), so it is reachable only on dense meshes such as an Imported Mesh, or a large brush on a dense frozen mesh. The product's own primitives (for example the 482-vertex sphere) cannot reach it. The status string `status_sculpt_stroke_not_retained` ("cannot be undone", `strings.xml:613`) is shown, but Undo stays enabled and produces the states above.

**Owner decision.** D-OD1 (OWNER-SEMANTICS-REQUIRED for the remedy only): on NotRetained, either clear the undo stack (Undo cannot cross it, like a Freeze) or refuse the stroke. Either remedy would satisfy K3–K7. The defect itself is decided by the quoted contract.

**Confidence.** E1 for symptoms 1–5 (reproduced in this closeout, output quoted). E2 for the root-cause lines and the consumers. Not run on any device. The coordinator's CI DEVICE run is not cited.


## CR-05 — Recovery, Save-and-continue and Activity recreation: where data protection is lost

**Claim under test.** The earlier dossier made five claims, all at E2: A-01 (P2), A-02 (P1), A-05 = E-04 (P3/P2), A-06 (P3) and E-02 (P2).
- After Recover, the recovered work is protected nowhere.
- "Save and continue" before Open Saved Project destroys the saved project.
- An Activity recreation forgets the saved baseline.
- The same recreation re-offers recovery over the live project.
- The same recreation loses a pending Save Copy or Export.

**Baseline.** `70a6c14`. `git diff 70a6c14 HEAD -- app/ | wc -l` = 0.

### State map (Java, `app/src/main/java/com/forgeshape/app/`)

| State | Owner / lifetime | Written by | Cleared by |
| --- | --- | --- | --- |
| Manual slot `files/project.forge` | disk | `saveProjectToSlot` `EditorWorkspaceView.java:5458-5472` (`ProjectSlot.write` :5461) | never |
| Recovery checkpoint `files/recovery.forge` | disk | `AutosaveController.performCheckpoint` `:145-198` (skip rule `:165`) | `ProjectCheckpoint.clear` on Save `:5467`, Open Saved `:5505`, Recover `:5675`, Discard-unsaved `:2688`, Discard-recovery `:5695` |
| Live document | native, per process | edits | `closeProject`, `loadProject` (fresh history) |
| `persistedFingerprint` / `everPersisted` | **View fields** `:271-272`, per Activity | `noteProjectPersisted` `:2605-2608`: Save `:5469`, Open Saved `:5507`, Recover `:5679`, Open File `:6106` | `noteProjectUnpersisted` `:2611-2614`; **a new View on recreation (Java defaults)** |
| `checkpointedFingerprint` / `everCheckpointed` | per `AutosaveController` instance; **a new one per workspace** `:1019` | write `:191-192`; `noteProjectPersisted` `:209-224` (Save, Open Saved, **Recover**) | instance discarded on recreation |
| `recoveryResolved` | **static**, per process (`EditorUiState.java:151`) | `:5592` (quarantine), `:5647` (test seam), `:5653` (Recover), `:5690` (Discard) — **only when a candidate existed** | process death; test seam `:5629` |
| `pendingCopyBytes` / `pendingGlbBytes` | **View fields** `:6004` / `:5778` | `:5720` / `:5763` before the picker | read and cleared in `:6048-6049` / `:5787-5788`; lost on recreation |
| Carried UI state | `EditorUiState.carryAcrossRecreation` (`ForgeShapeActivity.java:277`) | palette change only | — (none of the rows above is in it) |

**Recreation triggers.**
- A palette change: `requestTheme` → `recreate()`, `ForgeShapeActivity.java:268-280`.
- Any configuration change not listed in `configChanges="orientation|screenSize|smallestScreenSize|keyboardHidden|screenLayout|density"` (`AndroidManifest.xml:31`). Examples: `uiMode`, `locale`, `fontScale`, `keyboard`, `navigation`. That such changes recreate the Activity is platform knowledge (E4).
- Process death, which loses the statics as well.

Lifecycle order on a recreation:
1. `onStop`: immediate checkpoint on the OLD worker (`:353-359`).
2. `onDestroy`: `release()` → `quitSafely` (`:569-584`).
3. `onCreate`: a new workspace (`:51`), which builds a new `AutosaveController` (`:1019`) and runs `if (!recoveryResolved) offerRecoveryIfPresent()` (`:1027-1029`).

### Contracts (quoted)

- CLAUDE.md:1160 — "**Autosave protects work**; it never overwrites what the user chose to keep."
- CLAUDE.md:1172 — "**Nothing replaces the live project without the user saying so.**"
- CLAUDE.md:310-312 (APP-H1) — "Leaving a dirty project (New Project…, Open Saved Project, Open File…) asks Save / Discard / Cancel by FINGERPRINT against the last Save/Open/Recover; Save continues only if the slot was written".
- CLAUDE.md:932 / ARCHITECTURE.md:622 — a preference "moves no fingerprint, **dirties no project**".
- PRODUCT.md:193-194 — "A project you just saved, opened or recovered is not asked about."
- PRODUCT.md:2027 — "it asks — once — before anything is replaced".
- strings.xml:62 — "nothing is replaced until you decide".
- `EditorWorkspaceView.java:1021-1026` — "The recovery question is asked ONCE PER PROCESS … asking again there would present a second decision".
- `EditorUiState.java:139-145` — "an Activity recreation must not put a decision about the user's work back in front of them".
- `ProjectCheckpoint.java:150-157` — "Retires the checkpoint … after a successful Recover".
- `AutosaveController.java:200-208` — Recover "makes the live project something the user already has safely stored".
- `EditorWorkspaceView.java:5711-5713` — bytes are encoded first so the user never "watch[es] an empty file appear there".
- `EditorWorkspaceView.java:6052-6053` — the null-bytes branch is "Cancel. A no-op by construction: nothing was written".

### Setup and commands

All under `artifacts/function-council-r2-closeout/diagnostics/CR-05/`. Exact commands are in `CMD.txt`.

1. **Probe.** `androidjar_compile_probe.out`. The four persistence classes compile against `android-36/android.jar` (`javac_exit=0`). At run time `EditorUiState` works, but `android.os.HandlerThread (needed by AutosaveController:89): java.lang.RuntimeException: Stub!` and `android.content.Context is abstract`. `EditorWorkspaceView` and `ForgeShapeActivity` cannot run off-device at all.
2. **Host reproduction.** `bash build_and_run.sh` → `build_and_run.out` (`javac_exit=0`, `java_exit=0`, 20 × `=OBSERVED`, 0 × `NOT_OBSERVED`).
   - **Real product classes**, copied and verified `SAME` by sha256: `AutosaveController`, `ProjectCheckpoint`, `ProjectSlot`, `EditorUiState`, `LengthUnit`, `DiagnosticLog`.
   - **Shims** (`shim/`): a `Context` whose `getFilesDir` is a fresh temp dir per scenario, so no user file is touched; `Handler`, `HandlerThread`, `Looper` and `SystemClock`; a printing `Diagnostics`; and a **fake** `NativeViewport` in which the project is a byte string, the fingerprint is FNV-1a over it, and `loadProject` empties undo.
   - **Transcription** (`harness/WorkspaceTranscript.java`): the line-cited persistence methods of `EditorWorkspaceView`. `transcription_check.sh` → `transcription_check.out` prints every cited product line for comparison.

### Timelines and actual output (quoted from `build_and_run.out`)

**A-01 — Save and continue before Open Saved Project.**
```
03 Save Project                          live="A"   slot="A"   ckpt=absent dirty=false
05 autosave debounce drained             live="A+b" slot="A"   ckpt="A+b"  dirty=true
06 drawer: Open Saved Project            ... unsavedPrompt=true
07 unsaved question: Save and continue   live="A+b" slot="A+b" ckpt=absent dirty=false undo=0
A01_SLOT_NO_LONGER_HOLDS_A=OBSERVED
A01_A_EXISTS_IN_NO_FILE=OBSERVED
```
Protection is lost at step 07. `:5461` overwrites the slot, `:5467` deletes the checkpoint that still held A+b, and `:2656-2657` reopens the slot. The open becomes a reload that empties history; that a product load empties history rests on CLAUDE.md and FSR1B-05 (E3), because undo here comes from the fake native.

**A-02 — Recover.**
```
04 background + process death + cold launch  ckpt="W+1" recoveryPrompt=true
05 Recover                                   live="W+1" slot=absent ckpt=absent dirty=false everPersisted=true
06 background (onStop ... drained)           live="W+1" slot=absent ckpt=absent
autosave counters: writes=0 skippedUnchanged=1
07 second process death + cold launch        live=<home> slot=absent ckpt=absent recoveryPrompt=false
A02_RECOVERED_WORK_LOST_ON_SECOND_DEATH=OBSERVED
A02b_NEW_PROJECT_ASKED_NOTHING_AND_CLOSED_RECOVERED_WORK=OBSERVED
```
Protection is lost at step 05. `:5675` deletes the checkpoint; `:5676` and the real `AutosaveController.java:165` skip every later unchanged write.

**A-05 / E-04 — palette change after Save.**
```
03 Save Project                                 dirty=false everPersisted=true
04 Settings: choose another palette (recreate)  dirty=true  everPersisted=false
A05_FINGERPRINT_UNCHANGED=OBSERVED
A05_UNSAVED_QUESTION_OVER_A_SAVED_PROJECT=OBSERVED
```
Reflection on the real `EditorUiState` lists no baseline field.

**E-02 — no candidate at launch, then recreation.**
```
02 New Project (no recovery question ever asked)  resolved=false
03 edit -> L+e, autosave debounce drained         ckpt="L+e"
04 Settings: choose another palette (recreate)    live="L+e" recoveryPrompt=true
E02_RECOVERY_PROMPT_SHOWN_OVER_OPEN_PROJECT=OBSERVED
```

**E-02b — new in this closeout: saved, unchanged work offered as "Unsaved work found".**
```
02 Save Project (checkpoint retired)         slot="S" ckpt=absent dirty=false
03 palette change #1 (recreate)              dirty=true
04 app backgrounded (new controller's onStop) ckpt="S"
05 palette change #2 (recreate)              recoveryPrompt=true
E02b_SAVED_UNCHANGED_PROJECT_OFFERED_AS_UNSAVED_WORK=OBSERVED
```

**A-06 — transcript only.** `outcome=CANCELLED(no status shown) lastStatus=''` and `transfer SAVE_COPY_CANCELLED`.

### Classification per symptom

| Id | Symptom | Classification | Contract status | Conf. |
| --- | --- | --- | --- | --- |
| A-01 | Save and continue overwrites the slot it then opens | SOURCE-CONFIRMED-NOT-REPRODUCED (UI path); the host transcript's file effects come from the real `ProjectSlot` | **OWNER-SEMANTICS-REQUIRED (OD-06)** — APP-H1 literally requires Save there; strings.xml:62 says the opposite | E2 (+E1 transcript) |
| A-02a | After Recover, background writes no checkpoint; a 2nd death leaves nothing | **REPRODUCED** (component: real AutosaveController skip + ProjectCheckpoint) | **OWNER-SEMANTICS-REQUIRED (OD-05)** — PRODUCT.md:193-194, ProjectCheckpoint.java:150-157, FSR1B-05 endorse it; CLAUDE.md:1160 opposes | E1 |
| A-02b | Recover → New Project asks nothing, work closed | SOURCE-CONFIRMED-NOT-REPRODUCED | OWNER-SEMANTICS-REQUIRED (OD-05) | E2 |
| A-05/E-04 | Recreation flips `projectDirty()` false→true for a saved project | SOURCE-CONFIRMED-NOT-REPRODUCED (decisive values are View-field defaults; transcript and reflection agree) | **DEFECT** (CLAUDE.md:932 "dirties no project"; fail-safe direction) | E2 (+E1 support) |
| E-02 | Recovery prompt over the open project after recreation in a no-candidate process | **REPRODUCED** (component: real `EditorUiState.recoveryResolved` stays false, real checkpoint exists) | **DEFECT** (`:1021-1026`, `EditorUiState.java:139-145`) | E1 |
| E-02b | Saved, unchanged project offered as unsaved work | **REPRODUCED** (component: the new real `AutosaveController` writes the checkpoint) | **DEFECT** (strings.xml:218 "closed before this work was saved") | E1 |
| E-02c | Old-worker onStop write races the new constructor's read; edits lost on Recover | **BLOCKED** (needs device timing) | — | E5 |
| A-06 | Chosen destination after recreation → nothing written, no status, logged as a cancel; an empty document is likely | SOURCE-CONFIRMED-NOT-REPRODUCED | **DEFECT** against `:5711-5713` / `:6052-6053` | E2 (empty file E5) |
| A-03 / A-07 | Slot unreachable from Home; an open sketch is dropped on leave | OWNER-SEMANTICS-REQUIRED (OD-07, OD-10) | — | E2 |

**Correction to the earlier dossier (A-06).** A palette change cannot trigger A-06, because Settings is in-app and the system picker covers the app. The triggers are system configuration changes outside `configChanges` and process death while the picker is open.

### Falsification criteria
- **A-01** is refuted by `slot="A"` at row 07.
- **A-02a** is refuted by `ckpt="W+1"` / `writes>=1` at row 06, or by `recoveryPrompt=true` at row 07.
- **A-05** is refuted by `dirty=false` at row 04, or by a baseline field that survives recreation.
- **E-02** is refuted by `recoveryPrompt=false` at row 04, or by `offerRecoveryIfPresent` recording `resolved` on its no-candidate return. It does not: `transcription_check.out`, `:5579-5582`.
- **E-02b** is refuted by `ckpt=absent` at row 04.
- **A-06** is refuted by the pending bytes or kind surviving recreation, or by a status or document cleanup on null bytes.

### Controls (all `OBSERVED`)
- `CONTROL_OPEN_FILE_IS_CHECKPOINTED`. Open File (`:6104-6111`) leaves the document protected. This is an internal inconsistency with Recover: work that also lives in a user's file is protected, while recovered work that lives nowhere else is not.
- `CONTROL_ONE_EDIT_AFTER_RECOVER_RESTORES_A_CHECKPOINT`.
- `CONTROL_NO_QUESTION_WITHOUT_RECREATION` (A-05's leave path is not always dirty).
- `CONTROL_NO_PROMPT_WHEN_RESOLVED_WAS_RECORDED` (the case `fsr1b04` covers).

### Why existing tests miss it
- **They pin the opposite (A-02).** `ProjectAutosaveRecoveryTest.fsr1b05` (`:322-323`) and `ProjectProcessDeathTest.stage6` (`:349`) assert `!ProjectCheckpoint.exists` after Recover. No test backgrounds or kills the process after Recover without first editing.
- **Masked (E-02, E-02b).** Every case's `resetToBaselineConstruction` calls `dismissRecoveryPromptForTest` (`WorkspaceTestSupport.java:112`), which sets the static flag. So `fsr1b09` "nor present a recovery decision it already settled" (`:519`) never runs in a no-candidate process.
- **Blind (A-05).** `SettingsPreferencesTest.uiprefr1_09_10_11_37` "UIPREFR1-10: the dirty flag did not move" (`:294-295`) takes its baseline on a never-saved project, because `ensureConstructionProjectForTest` → `noteProjectUnpersisted` at `:2748`. Dirty is true before and after, so the flip cannot show.
- **Absent (A-01).** `HomeFlowTest.e2eAppH1_10` covers Save and continue only for Open File.
- **Absent (A-06).** `ProjectTransferTest` and `GlbExportTest` stage bytes through the `*ForTest` seams and never recreate between request and result.
- `EditorUiStateTest` covers the carry of the chooser, never `recoveryResolved` across a no-candidate launch.

### Owner decisions (not decided here)
- **OD-05**: Recover semantics. Options (a) keep, (b) unsaved until Save, (c) protected but not asked.
- **OD-06**: Save in the question before Open Saved Project.
- **OD-07**: the slot from Home.
- **OD-10**: a sketch as unsaved work.

A-05, E-02, E-02b and A-06 need no semantic decision: each violates a written contract.

### Confidence and limits
- **E1 at component level** for A-02a, E-02 and E-02b. The decisive values come from the unmodified product classes on a host JVM; the 2–3 `EditorWorkspaceView` calls feeding them are transcribed with line citations.
- **E2** for A-01, A-02b, A-05 and A-06.
- **E5** for E-02c.
- Nothing here ran on an emulator or a physical device. The coordinator's focused CI DEVICE run is not cited.

## CR-06 — Decimal comma, orientation navigator, System Back, 48 dp hit targets

**Claim under test.** Five P2 findings: F-05, C-01 = F-02, F-04, F-03 and F-01.
- F-05: three canvas editors drop the decimal comma.
- C-01 = F-02: navigator Flip and ±90° do not move the camera.
- F-04: Back during an in-project Revolve backgrounds the app on API 36.
- F-03: bootstrap Back discards the sketch while a panel is open.
- F-01: some chrome hit areas are narrower than 48 dp.

**Baseline SHA:** `70a6c14` (product tree; audit branch from `2e5d8fc`). The previous dossier is read-only.

### Contract
- `LengthUnit.java:124-129`: "Accepts `.` or `,` as the decimal separator". `ExactExpression.java:13`: "a comma is read as one, as `LengthUnit#parse` does".
- `CLAUDE.md:340-346`: "The orientation navigator … owns both … `SketchSession::viewFrame` derives the camera's frame from the authoring frame".
- `CLAUDE.md:313-314`: "Back is one step in every phase and the platform's own at Home". `EditorWorkspaceView.java:1169-1172`: "A Revolve in progress is one step inside Ready: Back returns to the extrusion".
- `CLAUDE.md:1434-1436`: "48 dp is the interactive floor for user-operated chrome, and it is the HIT AREA."

### Setup and commands
Full commands and exit codes are in `diagnostics/CR-06/CMD.txt`.
- Host objects come from the unmodified `scripts/host-native-selftests.sh`, run with `HOST_SELFTEST_OUT=/tmp/claude-0/fc2c-scratch/CR-06/host` and filter `SKETCH_UX`. Result: `HOST_SUITE_OK SKETCH_UX (118 checks, 0 failed)`, exit 0.
- `cr06_native.cpp` was linked against those objects (g++ exit 0, run exit 0) and writes `cr06_native.out`.
- `CommaDiag.java` was compiled with the UNMODIFIED product files `LengthUnit.java`, `ExactExpression.java` and `CadRevolvePresentation.java`, plus a constants-only `NativeViewport` stub. It ran under en_US, de_DE, pl_PL and fr_FR, each exit 0, and wrote `CommaDiag.out`. The four locale blocks are byte-identical apart from `default_locale`.
- `DigitsKeyListener` cannot run on the host because `android.jar` is stubs. It is MODELLED as string-mode `NumberKeyListener.filter`: every character outside the accepted set is dropped. The platform installs the key listener as an input filter on the field's Editable.

### 1. Decimal comma (F-05)
**UI → parser → model trace.**
- The three canvas fields set `DigitsKeyListener.getInstance("0123456789.-")`:
  - `CadExtrudeCanvasView.java:644`
  - `SketchDimensionLabelView.java:112`
  - `BodyDimensionLabelsView.java:127`
- Each then calls `LengthUnit.parse(raw)`, then `displayUnit().toMeters(typed).doubleValue()` (`:1284-1295`, `:265-276`, `:302-315`). The default unit is METERS (`EditorUiState.java:94`).
- The parsed value goes to `onExtrudeSideEntered`, `onSketchLineLengthEntered` or `onBodyDimensionEntered` (`EditorWorkspaceView.java:4110`, `:4079`, `:5062`).
- Those call `sketchSetExtrudeSide`, `sketchApplyLineLength` or `applyBodyDimension` (`forgeshape_jni.cpp:6283`, `:6700`, `:2713`).
- The four editors that keep `,` have it in their accepted set:
  - `NumericPropertyRow.java:109`: `"0123456789.,-"`
  - `CadRevolveAngleLabelView.java:87`: `"0123456789.,-"`
  - `SketchDimensionLabelsView.java:126`: `"0123456789.,-+*/()"`
  - `SketchModifyView.java:208`: `"0123456789.,-+*/()"`

**Actual output (`CommaDiag.java`, quoted):**
```
typed='2,5'
  pure LengthUnit.parse(unfiltered)        = 2.5 -> meters(double)=2.5 | mm unit -> meters=0.0025
  CANVAS  filter '0123456789.-' -> '25' -> LengthUnit.parse = 25 -> meters(double)=25.0 | mm unit -> meters=0.025
  ROW     filter '0123456789.,-' -> '2,5' -> LengthUnit.parse = 2.5 -> ... ; CadRevolvePresentation.parseDegrees = 2.5
  EXPR    filter '0123456789.,-+*/()' -> '2,5' -> ExactExpression.parse = 2.5
typed='1 000,5'
  pure LengthUnit.parse(unfiltered)        = NumberFormatException(Character   is neither a decimal digit number, decimal point, nor "e" notation exponential mark.)
  CANVAS  filter '0123456789.-' -> '10005' -> LengthUnit.parse = 10005 -> meters(double)=10005.0 | mm unit -> meters=10.005
  ROW     filter '0123456789.,-' -> '1000,5' -> LengthUnit.parse = 1000.5 ...
```

**Actual output (model leg, `cr06_native.cpp`, quoted):**
```
A1 extrude finish=Ok depth_before=1 setExtrudeSide(true,25)=Ok depth_after=25
A2 line applyLineLength(25)=Ok end=(15,20) length=25
A3 body applyBodyDimension(axis=0,25) status=0 scaleX 1 -> 12.5 undoDepth=1
A1 extrude finish=Ok depth_before=1 setExtrudeSide(true,2.5)=Ok depth_after=2.5
A2 line applyLineLength(2.5)=Ok end=(1.5,2) length=2.5
A3 body applyBodyDimension(axis=0,2.5) status=0 scaleX 1 -> 1.25 undoDepth=1
A1 extrude finish=Ok depth_before=1 setExtrudeSide(true,10005)=Ok depth_after=10005
```

**Result.**
- Typing "2,5" commits an extrude depth of 25 m, a line length of 25 m, or a body X size of 25 m (scale 12.5). Each is one Undo step, with no refusal and no message.
- "1 000,5" commits 10005 m.
- The parser itself is correct and locale-independent. The fault is the key-listener accepted set.

### 2. Navigator (C-01 = F-02)
`beginSketchView()` frames from `sketchSession().frame()` (`forgeshape_jni.cpp:313-320`). Flip and rotate change the view state and then call it (`:6612-6645`). The only non-test reader of `viewFrame()` is its own definition (`forgeshape_sketch_session.cpp:648`).

The probe copies the body of `beginSketchView` verbatim, because the JNI translation unit is not host-buildable. Viewport is 1080×2400, plane XY.
```
B0 camera(beginSketchView) eye=(0.0000,0.0000,250.0000) ... view=[1,0,-0,0,-0,1,-0,0,0,0,1,0,-0,-0,-250,1]
B1 viewFrame() u=(1,0,0) v=(-0,-1,-0) n=(-0,-0,-1)
B1 camera(beginSketchView) eye=(0.0000,0.0000,250.0000)
B1 control camera(viewFrame) eye=(0.0000,0.0000,-250.0000) ... view=[1,0,-0,0,0,-1,-0,0,0,0,-1,0,...]
B1 viewFlipped=1 camera_view_identical_to_B0=1 control_differs_from_B0=1
B2 quarterTurns=1 camera_view_identical_to_B0=1 control_differs_from_B0=1
B3 quarterTurns=3 camera_view_identical_to_B0=1 control_differs_from_B0=1
```
After Flip, +90 and −90 the camera's view matrix is bit-identical to the matrix before them. Framed from `viewFrame()`, the camera would have moved in every case.

### 3. System Back (F-04, F-03)
**Platform facts.**
- `targetSdk 36` (`app/build.gradle:14`). The CI emulator is `ro.build.version.sdk=36`.
- The manifest has no `android:enableOnBackInvokedCallback`.
- On API ≥ 33, `ForgeShapeActivity.installBackDismissal` creates `BackDismissal` (`:87-89`). It registers `OnBackInvokedCallback` at `PRIORITY_DEFAULT` ONLY while `workspace.hasDismissibleSurface()` is true (`:146-154`).
- On API 36 with target 36, `onBackPressed` (`:107-112`) is not called, so this callback is the only route.
- On API 33-35 without the opt-in, the legacy route probably still applies (E5).

**`hasDismissibleSurface()` (`EditorWorkspaceView.java:1112-1117`)** is `unsavedPrompt || settings || newProjectChooser || supportChooserActive || bootstrapVisible || expandedRow || topmostOpenSurface`.

- **Revolve inside a project, no surface open.** No term of the predicate is true. The callback is unregistered and the platform default runs (the task moves to the back). The Revolve branch (`:1173-1175`) is unreachable. **F-04 is source-confirmed.**
- **Revolve in the CAD bootstrap.** `bootstrapVisible()` is true, so the callback is registered. The Revolve check (`:1173`) runs before the bootstrap check (`:1177`), so Back ends the Revolve. This one is correct (control).
- **CAD bootstrap, sketching, a panel open.** The callback is registered. `bootstrapVisible()` leads to `onCancelSketchRequested()` (`:1177-1179`) before the anchored-surface branch (`:1201`). The sketch is discarded and Home returns with no question (`:4927-4940`). **F-03 is source-confirmed. The intended behaviour is the OWNER's call (OD-08).**
- **Adjacent (E2).** An in-project sketch with no surface open has no branch on either route.

### 4. Hit targets (F-01)
Measurements come from the recorded CI DEVICE `40797031704` (E3, emulator; never physical). Configuration:
- AVD `ForgeShape_CI_API36`, API 36, medium_phone, 1080×2400, density 420 (2.625 px/dp), portrait.
- Font scale not recorded (default assumed, E5).
- Sizes are `view.getWidth()/getHeight()` from `SketchUxVisualEvidenceTest.java:415-426` and `SelectionOutlineVisualEvidenceTest.java:730-742`. Both tests PASSED (32 tests, 0 failed).
- There is no TouchDelegate in `app/src/main/java`.

| Control (id) | Rect px | Width px → dp | Height px → dp | 48 dp | Source |
|---|---|---|---|---|---|
| `sketch_navigator_flip` | [603,544][698,670] | 95 → **36.19** | 126 → 48.00 | width FAIL | facts.txt:40 (measured) |
| `view_selection_outline_on` (LEFT handedness) | [724,1025][821,1151] | 97 → **36.95** | 126 → 48.00 | width FAIL | sel-out facts.txt:283 (measured) |
| `view_selection_outline_off` | [832,1025][935,1151] | 103 → **39.24** | 126 → 48.00 | width FAIL | :284 (measured) |
| `sketch_dimension_label` ("5 m") | [800,583][914,709] | 114 → **43.43** | 126 → 48.00 | width FAIL | facts.txt:63 (measured) |
| `field_sketch_line_length` | [638,583][890,709] | 252 → 96.00 | 126 → 48.00 | pass | facts.txt (measured) |
| `view_grid_on` / `view_grid_off` | not recorded | — | — | likely FAIL | DERIVED (E2/E5): same `EditorControlStyles.chip` |

The cause is the same for every failing row (E2): `EditorControlStyles.chip` (`:366-382`) sets `setMinimumHeight(control_height = 48dp)`, padding of 10 dp per side, and no minimum width.

### Classification per symptom
| Symptom | Classification | Confidence |
|---|---|---|
| F-05 comma → 25 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 end-to-end; parser and model legs E1 |
| F-05b "1 000,5" → 10005 | SOURCE-CONFIRMED-NOT-REPRODUCED | E2; legs E1 |
| C-01 = F-02 navigator | **REPRODUCED** | E1 (host) |
| F-04 in-project Revolve Back | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 for API 36; E5 for API 33-35 |
| F-03 bootstrap Back with panel | OWNER-SEMANTICS-REQUIRED | behaviour E2 |
| F-01 Flip, Outline chips, dimension label | SOURCE-CONFIRMED-NOT-REPRODUCED | E3 measured (emulator) + E2 |

### Falsification criteria
- **F-05:** refuted if the canvas filter string contained `,`, or if a device log showed `FORGESHAPE_EXTRUDE_SIDE ... meters=2.500000` after typing 2,5.
- **C-01:** refuted if `camera_view_identical_to_B0` printed 0.
- **F-04:** refuted by a device Back during an in-project Revolve that logs the Revolve end and keeps the activity foreground.
- **F-03:** refuted if Back with the precision surface open in the bootstrap closed the surface and kept the sketch.
- **F-01:** refuted by `width_px >= 126` at density 420, or by a hit-slop delegate.

### Controls
- "2.5" gives 2.5 on every path.
- The comma-keeping editors give 2.5.
- The four locales give identical output.
- The `viewFrame()` framing does move the camera, so the navigator probe can detect a reframe.
- The bootstrap Revolve Back is handled.
- Heights of every measured chip are 48 dp.

### Why existing tests miss it
- No test types `,` into the three canvas fields. Parser tests bypass the key listener.
- The `SKETCH_UX` self-test checks `viewFrame()` algebra only (`forgeshape_sketch_ux_selftest.cpp:490-536`).
- `SketchUxTest` asserts `SKETCH_VIEW_FLIPPED` state, not the camera (`SketchUxTest.java:200-205`).
- Only `uilr106` sends `KEYCODE_BACK`, and only for Add Primitive. `CadRevolveOwnerTest` uses the on-screen control.
- `EditorWorkspaceLegibilityTest.assertTouchFloor` covers six trailing-cluster ids (`:1115-1137`). The visual-evidence tests record rects without asserting.

**Existing seams that could exercise an item unmodified:**
- `SketchUxVisualEvidenceTest` and `SelectionOutlineVisualEvidenceTest` re-record the F-01 rects.
- No existing test exercises F-05, C-01, F-04 or F-03.

The coordinator's focused CI DEVICE run is not seen here and is not claimed.

### Owner decisions
- **OD-08:** whether bootstrap Back closes a panel first (F-03).
- **OD-09 / council:** what Back to Sketch and Edit Sketch restore after a navigator fix (C-01).
- **F-05, F-04 (in-project scope) and F-01** have no OWNER gate.

### Confidence
- E1: the navigator, and the parser and model legs of the comma.
- E2: the Back routes, and the root cause of the chip width.
- E3: the measured rects (emulator, historic run 40797031704).
- E5: API 33-35 Back behaviour, and font scale.

## CR-07 — Test infrastructure: the PROJECT suite is exactly at its device cap, FMA breaks two oracles, CI FAST runs no host suites

**Claim under test.** (1) The PROJECT device self-test suite fills its 256-entry result array, and a 257th check would be silently dropped. (2) Building with FP contraction (FMA) makes some self-test checks fail; for each one, is it an over-tight oracle or a real product numerical problem? (3) CI FAST runs neither the host suites nor the corpus decode verdicts.

**Baseline SHA.** Product tree `70a6c14` (audit HEAD `c51fdb3`; `app/`, `scripts/` and `.github/` are unchanged from it).

**Contract (quoted).**
- `app/src/main/cpp/forgeshape_jni.cpp:749-751`: "The recorder silently DROPS checks past its ceiling, which would read as a smaller passing suite rather than as a failure, so this stays well ahead of it."
- `forgeshape_jni.cpp:821-823`: `constexpr int kMaxProjectChecks = 256; static forgeshape::ProjectSelfTestResult results[kMaxProjectChecks];`
- `forgeshape_jni.cpp:871-875`: OK/FAIL token computed from the recorded `count` only.
- `forgeshape_project_selftest.cpp:21-33`: `void check(const char* name, bool ok) { if (n < max) { ... ++n; } }`, so an overflowing check is discarded with no flag.
- `CLAUDE.md:21`: "A clean debug launch emits **twenty-five** `*_SELFTEST_OK` tokens". `CLAUDE.md:89-90`: "`bash scripts/host-native-selftests.sh [filter]` runs all twenty-five suites on the host, with no device, and is the fast native loop."
- `forgeshape_picking.h:90-97`: "on arm64-v8a the compiler contracts the dot/cross products into fused multiply-adds (no `-ffp-contract=off` is set, and only arm64 has the instruction at our baseline) ... Widening is the fix rather than pinning the FP model".
- Build flags: `app/build.gradle:30-31` (`arguments '-DANDROID_STL=c++_static'`, `cppFlags '-std=c++17'`). `CMakeLists.txt` has no fp-contract flag; it sets only `:179 target_compile_options(forgeshape_manifold PRIVATE -O2 -w)`.

**Setup.** Linux x86_64 host with FMA. g++ 13.3.0, clang++ 18.1.3, NDK 29.0.14206865 clang 21.0.0. The unmodified `scripts/host-native-selftests.sh` was run with `CXX` wrapper scripts and `HOST_SELFTEST_OUT` under `/tmp/claude-0/fc2c-scratch/CR-07/`. Diagnostics are in `artifacts/function-council-r2-closeout/diagnostics/CR-07/`.

**Commands** (full list with exit codes in `CMD.txt`):
```
HOST_SELFTEST_OUT=$S/base bash scripts/host-native-selftests.sh                 # exit 0
g++ <script COMMON flags> cap_probe.cpp $S/base/obj/*.o $S/base/kernel/libmanifold.a -o $S/cap_probe; $S/cap_probe   # exit 0
for v in clang-fma-on clang-fma-fast clang-fma-off gcc-fma-fast clang-nofma gcc-O2-fma-fast clang-fma-on-O0; do
  CXX=$D/cxx-$v.sh HOST_SELFTEST_OUT=$S/$v bash scripts/host-native-selftests.sh; done
#   exits: clang-fma-on 1, clang-fma-fast 1, clang-fma-off 0, gcc-fma-fast(-O1) 0, clang-nofma 0, gcc-O2-fma-fast 1, clang-fma-on-O0 1
fp_oracle_diag.cpp built twice (g++ base objects; clang++ -mfma -ffp-contract=on objects)  # both exit 0
$NDK_CLANG --target={aarch64,x86_64}-none-linux-android26 -O0/-O2 -S contract_probe.cpp
grep -c host-native-selftests .github/workflows/*.yml ; grep -c host-forge-corpus-verdicts .github/workflows/*.yml
```

**Actual output (quoted).**

*(1) Cap probe* (`cap_probe.out`):
```
PROJECT                  host= 256 device_cap= 256 recorded_at_cap= 256 headroom=    0 AT_CAP
CAMERA                   host= 119 device_cap= 128 recorded_at_cap= 119 headroom=    9 ok
PROJECT host_last_check[255]=DAR1_02_rebinding_mid_stroke_drops_the_stroke_and_records_it_nowhere
PROJECT cap=255 recorded=255 failed=0 last=DAR1_02_the_loaded_mesh_is_exactly_the_document_s verdict=OK (check 'DAR1_02_rebinding_mid_stroke_drops_the_stroke_and_records_it_nowhere' silently absent)
SIM 257 checks (257th fails) cap=256 recorded=256 failed=0 device_token=FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)
```
`grep -c 'r\.check(' forgeshape_project_selftest.cpp` returns 256, and none of those calls sits inside a loop. **Today no check is lost**: the host and the device both count 256, and historic device logs show `FORGESHAPE_PROJECT_SELFTEST_OK (256 checks)` ten times since `artifacts/mirror-01`. The other 24 suites have 9 to 386 entries of headroom.

*(2) FP contraction matrix* (`host-runs/*.out`):
```
clang-fma-on / clang-fma-fast / gcc-O2-fma-fast / clang-fma-on-O0:
  FAIL CONSTRUCTION_HISTORY: eviction_is_deterministic_oldest_first
  FAIL CAD_FEATURE: CADFC2_PF_03b_BEFORE_the_lens_taps_to_a_whole_circle_by_anchor_order
HOST_SELFTESTS_FAIL (4392 checks, 2 failed)
clang-fma-off / clang-nofma / gcc-fma-fast(-O1) / base g++ -O1:
HOST_SELFTESTS_OK (4392 checks, 0 failed)
```
Root cause, measured on product functions (`fp_oracle_diag.out`):
```
# no FMA
HISTORY capacity=64 k=7 fed=0.57000000000000006 oracle=0.57000000000000006 equal=1 ulps=0
LENS loop anchor=1 area=3.121445152258052
LENS loop anchor=2 area=3.121445152258052
LENS a=1 b=2 tap(0,0)->1 (oracle expects a)
# clang++ -O1 -mfma -ffp-contract=on
HISTORY capacity=64 k=7 fed=0.56999999999999995 oracle=0.57000000000000006 equal=0 ulps=-1
LENS loop anchor=1 area=3.121445152258052
LENS loop anchor=2 area=3.1214451522580515
LENS a=1 b=2 tap(0,0)->2 (oracle expects a)
```
Compiler behaviour (`contract_probe.out`):
```
probe_aarch64-O0.s:  fmadd d0, d0, d1, d2;          (NDK arm64 Debug fuses)
probe_aarch64-O2.s:  fmadd d0, d0, d2, d1;
probe_x86_64-O0.s:   addsd; mulsd; addsd;           (Android x86_64 baseline: no FMA)
probe_a64_off.s:     fmul d1, d0, d1; fadd d0, d0, d1;   (-ffp-contract=off control)
g-O1.s: vmulsd; vaddsd   (g++ -O1 -mfma -ffp-contract=fast does NOT fuse)
g-O2.s: vfmadd132sd      c-O1.s: vfmadd213sd
```

*(3) CI FAST* (`.github/workflows/ci-fast.yml`):
- `:122-128`: Gradle unit tests and the three APK builds.
- `:130-131`: release self-test guard.
- `:133-163`: ".forge corpus parity (independent encoder vs committed fixtures)": the PowerShell builder regenerates the corpus, then `cmp` checks 72/72. That is encoder byte parity only; no fixture is decoded through the C++ codec.
- `:165-186`: device-free runner and guard scripts.
- `grep -c host-native-selftests` over the workflow files returns `0` for ci-device, ci-fast and ci-full-sharded, and so does `grep -c host-forge-corpus-verdicts`.
- CI DEVICE uses only `system-images;android-36;google_apis;x86_64` (`ci-device.yml:41`).

**Classification per symptom.**

| # | Symptom | Classification | Confidence | Product or oracle |
|---|---|---|---|---|
| 1 | PROJECT at 256/256 device cap; a 257th (even failing) check is silently dropped and the token still says OK. None is lost today. | REPRODUCED (host simulation of the identical Recorder and cap) | E1 | test infrastructure |
| 2 | `eviction_is_deterministic_oldest_first` fails under FMA | REPRODUCED (host) | E1 host; arm64 device E2 (asm), OWNER-PHYSICAL-UNVERIFIED | **false FAIL, over-tight oracle**: exact `==` between a fused loop value and a non-fused recomputation, 1 ulp apart; the product stores what it is given |
| 3 | `CADFC2_PF_03b_BEFORE_the_lens...` fails under FMA | REPRODUCED (host) | E1 host; arm64 device E5 | **false FAIL, over-tight oracle**: it asserts a documented equal-area tie-break "ordering accident", and mirrored areas differ by 1 ulp under FMA. The product tie is unreachable in practice: `sketchRegionAt` is used only without planar faces (`forgeshape_sketch_session.cpp:2392-2413`), and nested loops never tie (E2+E5) |
| 4 | No fp-contract policy; NDK arm64 fuses even at -O0; CI is x86_64-only; the default host loop (g++ -O1) never fuses | REPRODUCED (assembly) | E1 | infrastructure blind spot |
| 5 | CI FAST runs no host suites and no corpus decode verdicts | SOURCE-CONFIRMED-NOT-REPRODUCED | E2 | infrastructure gap |

**Falsification criteria.**
- (1) is refuted by a host PROJECT count below 256, or by the simulated 257-check Recorder reporting `recorded=257` or `failed=1`.
- (2) and (3) are refuted by any contracting build passing these checks, or by `fed == oracle` and identical lens areas under FMA. A failure in the non-FMA controls would instead make them product defects.
- (4) is refuted by an fp-contract flag in `build.gradle` or `CMakeLists.txt`, or by NDK aarch64 -O0 emitting separate fmul/fadd by default.
- (5) is refuted by any workflow step invoking either host script.

**Controls.**
- `clang++ -mfma -ffp-contract=off`, `clang++` without FMA, and baseline `g++ -O1` all pass 4392/0.
- `g++ -O1 -mfma -ffp-contract=fast` also passes, only because GCC forms no FMA at -O1; at -O2 it reproduces the same two failures.
- The 24 non-PROJECT suites all have headroom.
- The other 4390 checks pass under every contracting build.
- Corpus verdicts under FMA were recorded identical by seat G (E3, `function-council-r2/diagnostics/G/verdicts_fma.txt`); not re-run here.

**Why existing tests miss it.**
- No test asserts per-suite check counts, and the Recorder has no overflow signal.
- CI DEVICE is x86_64 only, and the Android x86_64 baseline has no FMA.
- The host suites are not in CI.
- The documented host loop uses g++ -O1, which cannot contract.
- No arm64 or physical-device lane exists. Emulator CI is never physical evidence.
- The focused CI DEVICE run being performed by the coordinator is not cited here.

**Owner decisions.**
- (a) Raise `kMaxProjectChecks`, and/or make the Recorder fail closed on overflow.
- (b) Choose the FP policy: fix the two oracles to be tolerance-based or same-site (consistent with the `forgeshape_picking.h:86-97` "widen, don't pin" policy), or pin `-ffp-contract=off`.
- (c) Whether to add host suites, an FMA host lane (clang++ -mfma) and the corpus-verdict diff to CI FAST. This audit makes no workflow change.

**Confidence.**
- Symptoms 1-4: E1 (reproduced in this closeout, with output).
- arm64 device outcome: E2 for the history check (NDK assembly), E5 for the lens check.
- Symptom 5: E2.
