# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — Proposed next single stage

**Status: PROPOSED, NOT AUTHORISED, NOT STARTED, NOT IMPLEMENTED HERE.**
This closeout is audit-only. No file under `app/`, `scripts/`, `testdata/`, `.github/`,
Gradle, `DATA_PACKAGE_SPEC.md`, `CLAUDE.md`, `ARCHITECTURE.md`, `PRODUCT.md` or
`PROJECT_STATUS.md` was changed to write this proposal, and nothing in it has been built,
run or merged. It exists so the OWNER can authorise (or decline) exactly one bounded stage.

## The stage

**`CAD-FACE-SUPPORTED-PLACEMENT-R1`** — make every consumer of a face-supported CAD Body's
placement read the one derived placement (`ConstructionScene::resolveWorldModel`), for GLB
export and for transform writes.

## Does the closeout evidence confirm or contradict the earlier recommendation?

The previous dossier (`artifacts/function-council-r2/PRIORITISED_BACKLOG.md`, "Best next
bounded stage") recommended the same stage. The closeout's critical repros **confirm** it
and contradict nothing in it:

- **CR-01 (export) is re-reproduced at E1 in this closeout.** Run in this closeout on the host
  through the same call JNI `exportGlb` makes (`captureGlbExportScene` +
  `exportSceneAsGlb`): all 11 face-supported bodies MISMATCH and all 12 independent bodies
  MATCH (`diagnostics/CR-01/cr01_export_face.out`), including four committed `CADB` v2
  fixtures that decode and load Ok. An independent parse of the written bytes shows every
  dependent node at `translation=(0,0,0)` (`parse_glb.out`), and the cap-supported and
  side-supported exports are byte-identical (`9934da55…`, `glb_sha256.out`). The exporter
  reports `Ok` throughout — the outcome CLAUDE.md calls worse than no file.
- **CR-02 (transform) is E1 for the domain effect.** A typed write and a gizmo drag on a
  dependent each record one Undo, move the fingerprint and the stored transform, and move
  nothing drawn (`C1/C2 VERDICT undoDelta=1 fingerprintChanged=1 storedMoved=1 worldMoved=0`);
  the gizmo pivot is drawn at `(0,0,0)` while the body is drawn at `(0,0,3)`; the junk value
  reaches `.forge` and decodes (`diagnostics/CR-02/cr02_transform_face.out`).
- **The closeout sharpens one point of scope:** the two symptoms are one cause but are
  **not** fixed by each other (`CRITICAL_REPROS.md` CR-02 "Shared cause"). Refusing writes
  leaves the exporter writing `(0,0,0)` for a body drawn at `(0,0,3)`; fixing export alone
  leaves the product minting invisible Undo steps and fingerprint changes. So both halves
  stay in one stage.
- **Nothing in CR-03..CR-07 outranks it on evidence without a gate.** The other P1
  (Recover, CR-05-S2, now E1 component) is behind **OD-05**. The smaller XS item (NotRetained
  Undo, CR-04, E1) is P2 and the closeout records its remedy choice as OWNER-SEMANTICS
  (D-OD1 / OD-22). CR-07-S1 (PROJECT suite at 256/256) does not block this stage because it
  touches no PROJECT check.

Decision: **keep `CAD-FACE-SUPPORTED-PLACEMENT-R1`** as the one proposed stage.

## Why it is the smallest stage worth authorising

- It closes the only **P1 with no OWNER gate** (`DECISION_GATES.md` Part 1 item 1) and the
  P2 with the same root cause (item 3) — RC-01a/b/c, aliases OPP-01, OPP-02, B-01, B-02 =
  E-01, B-03.
- The expected behaviour is already written down, so it needs **no OWNER decision, no
  format change, no new fixture, no library and no CI change**: CLAUDE.md "exports EVERY
  body", CAD-A3 "a face-supported body's world placement is DERIVED … NOT independently
  transformable (`SceneObject::isFaceSupportedCad`)", and ARCH-OWNER-07's `Model = T · L`
  split.
- It is two S-sized slices (T-01, T-02 in the previous backlog) in suites with headroom:
  GLTF_EXPORT 93/128, SCENE 175/256, GIZMO 176/256, CAD_A3 66/128 (host counts from
  `evidence/local/host_selftests.txt`; caps from `forgeshape_jni.cpp`). It adds no PROJECT
  check (that suite is at 256/256).

## Scope

1. **Export (T-01).** `captureGlbExportScene` (`app/src/main/cpp/forgeshape_gltf_export.cpp`,
   today `:178-187` reads `body.transform().modelMatrix()`, `localMatrix()` and
   `normalMatrix()`) takes each body's `Model` from `ConstructionScene::resolveWorldModel`,
   then splits it into node `translation` and baked `L` exactly as ARCH-OWNER-07 requires,
   with normals by the inverse transpose of that `L`. When a face-supported placement does
   not resolve, the export **refuses by a named `GlbExportStatus`** and writes no file; it
   never falls back to the stored transform. The header and in-code comments that say the
   exported place is "bit-for-bit the place drawn" become true for every body.
2. **Transform guard (T-02), below JNI.** A named refusal for a face-supported CAD Body in
   every transform write path the closeout traced:
   - the typed write `applyBoxTransform` (`forgeshape_jni.cpp:1349-1385`, which today
     refuses only no-project and locked);
   - gizmo activation `setGizmoActive` (`:3833-3856`) and the touch drag start
     (`gizmo.beginDrag`, `:9932-9934`; `forgeshape_gizmo.cpp:1197-1212, 1415-1445`).

   A refusal records no Undo step, moves no fingerprint, and leaves the stored transform
   untouched.
3. **Transform guard, above JNI** ("a control that cannot succeed is not drawn"): for a
   face-supported active body the Transform rail entry, the gizmo offer and the placement
   editor are absent (`WorkspaceTrailingHostView.java:605-622`,
   `EditorWorkspaceView.java:3028-3033, 3046-3049, 3713-3715`,
   `ConstructionPlacementEditorView.java:269`). The native guard stays regardless.
4. **Owner documents the stage owes.** `ARCHITECTURE.md:2607-2608` (which states this guard
   already exists) becomes true and is touched only if wording must change;
   `PROJECT_STATUS.md` gets the stage entry; `PRODUCT.md` only for behaviour runtime-verified
   by the stage.

## Explicit non-scope

- **No decoder change** for a non-identity stored transform already in a saved file
  (**OD-16** stays open). Such files keep loading exactly as today; after this stage that
  value is inert for drawing (already) and for export (new). The stage does not refuse,
  ignore-by-rewrite or canonicalise it.
- No "offset from face" placement, no new placement semantics, no change to
  `resolveWorldModel`, the face frames, the CAD-A3 lineage token, `CADB` (any version), the
  `.forge` layout, any corpus fixture or any of the 25 startup tokens.
- No face-frame cache (OPP-38), no Lock semantics for dependents (**OD-29**), no
  hidden-body rule (**OD-12**).
- No other export change: Freeform/Surface/Sculpt export paths, OBJ/FBX, node `matrix` or
  `rotation`/`scale` stay as they are.
- None of the other Part 1 items (import guard, NotRetained Undo, recreation state,
  navigator, decimal comma, Back, 48 dp, test caps). Each is its own slice.
- No FullSharded run, no aggregate claim (**OD-03**), no physical-device claim (**OD-27**).
- No merge: built on a task branch from the tree carrying `70a6c14`'s product; it lands only
  after the OD-27 verdict and the branch rules in `PROJECT_STATUS.md` Next Stage allow it.

## Acceptance criteria

Numbers are the closeout's own measured `resolveWorldModel` values
(`diagnostics/CR-01/cr01_export_face.out`), compared within the suite's existing float
tolerance.

Criterion 1 is stated in two tiers on purpose. **1a is pivot-independent**: it compares
world geometry, which is the same whichever point of a dependent the GLB node carries as
its pivot, so it holds whatever the OWNER says about the pivot and is the binding gate.
**1b fixes node translations**, which presumes the pivot is the image of the body's local
origin under `resolveWorldModel` (the face-frame origin carried by the producer) — the
reading the OWNER gate below asks to confirm. 1b is binding only once that is confirmed.

1a. **Fixtures export where they are drawn (binding, any pivot).** For every vertex `p` of
   every exported body, the world position the GLB states — node `translation` + the baked
   position — equals `resolveWorldModel · p`, and every exported normal equals the
   inverse-transpose of `resolveWorldModel`'s linear part applied to the body-local normal,
   both within tolerance. Measured world AABBs the export must reproduce:
   - `cad_face_sketch_cap_v2` body 2: `[1.25,-0.25,0]..[2.25,0.75,0.5]` (today exported
     `[-0.25,-0.75,0]..[0.75,0.25,0.5]`, delta 1.5).
   - `cad_face_sketch_side_v2` body 2: `[0.75,-0.25,0.75]..[1.0,0.25,1.25]` (today delta
     1.25), i.e. the face rotation is carried (resolved column 0 `(0,1,0)`, column 2
     `(1,0,0)`).
   - `cad_face_chain_v2` bodies 2 and 3: `[-1.0607,1,-1.0607]..[1.0607,1.5,1.0607]` and
     `[-0.5,1.5,-0.5]..[0.5,1.75,0.5]` (today deltas 1.75 and 2.0), i.e. the producer's 45°
     turn is carried (resolved column 0 `(0.707,0,-0.707)`).
   - `mixed_cad_face_v2` body 4: `[-2.75,0.75,1.5]..[-2.25,1.75,2.0]` (today delta 2.75).
   - A moved and rotated producer carries its dependents into the export (closeout scene
     S4: body 2 AABB `[1.7308,2.7308,5.4731]..[2.5673,3.5673,6.1561]`, body 3
     `[1.6907,1.8422,4.0258]..[2.2185,2.3700,4.5722]`; today deltas 5.6561 and 4.3722).
   - The cap-supported and side-supported exports are no longer byte-identical (their world
     geometry differs, so this holds for any pivot).
1b. **Node translations (binding only after the OWNER confirms the pivot).** If the pivot
   is the face-frame origin, each dependent's node `translation` equals
   `resolveWorldModel`'s translation column and the baked `L` is its linear part:
   `cad_face_sketch_cap_v2` body 2 `(1.5, 0.5, 0)`; `cad_face_sketch_side_v2` body 2
   `(1, 0, 1)`; `cad_face_chain_v2` bodies 2 and 3 `(0, 1, 0)` and `(0, 1.5, 0)`;
   `mixed_cad_face_v2` body 4 `(-2.5, 1.25, 1.5)`; S4 body 2 `(2.0607, 3.0607, 5.5981)`
   and body 3 `(1.8839, 2.1768, 4.2990)`. Today every one of them is `(0,0,0)`. If the OWNER
   chooses a different pivot, 1b is rewritten to that pivot and 1a is unchanged.
2. **Independent bodies are byte-identical before and after.** Every export of a project
   with no face-supported body (world-plane CAD, Construction primitives, rotated / scaled /
   translated bodies, Imported Mesh) produces the same bytes as on `70a6c14`.
3. **An unresolvable face-supported placement refuses export by name** and writes nothing.
4. **Transform on a face-supported body is refused by name** for the typed write, gizmo
   activation and drag start; undo depth, fingerprint, stored transform and checkpoint state
   are unchanged; the resolved world model is unchanged.
5. **Chrome:** with a face-supported active body the Transform entry, gizmo and placement
   editor are absent; with an independent body, and with a producer that has dependents,
   they are present and work as before (a producer move still carries its dependents).
6. Host `HOST_SELFTESTS_OK` with 0 failed; every touched suite stays below its cap and the
   new counts are recorded.

## Tests

- **Host (fast native loop):** `bash scripts/host-native-selftests.sh` — all 25 suites,
  0 failed. New checks in GLTF_EXPORT (the fixture and scene cases above, the refusal, and
  the independent-body byte identity), and in SCENE or GIZMO (the transform and gizmo
  refusals, Undo/fingerprint unchanged, producer still movable). `CR-01` and `CR-02`
  diagnostics flip from MISMATCH / `storedMoved=1` to MATCH / refused.
  `bash scripts/host-forge-corpus-verdicts.sh`: all 72 verdicts unchanged.
- **JVM:** the unit suite with `--rerun-tasks`, plus a presentation test for the withdrawn
  Transform entry if the availability predicate moves into a testable class.
- **Release self-test guard:** PASS.
- **CI FAST:** green, with `.forge` encoder parity 72/72.
- **Focused CI DEVICE (emulator only; never physical, never an aggregate):**
  `GlbExportTest` (plus one face-supported export case), `EditorWorkspaceGizmoTest` (plus
  one face-supported refusal case), `SpatialSketchTest`, `ObjectCommandsTest`,
  `CadVerticalSliceTest`. Expect 25/25 startup tokens in order and
  `FORGESHAPE_NATIVE_VIEWPORT_OK`. Note: if `EditorWorkspaceMobileTest` is added to the
  dispatch, its stale `uir4a04` assertion fails independently of this stage
  (`evidence/device-run-40983965648/TRIAGE.md`).
- **Not in the gate:** FullSharded (**OD-03**), any physical-device evidence (**OD-27**).

## OWNER gate

- **To start:** OWNER authorisation of this stage by name. No OWNER *semantic* decision is
  required first (CR-01-S8, CR-02 "Owner decision": the contract already states the rule).
  The pivot question does not block the start: acceptance criterion 1a (world geometry
  equal to `resolveWorldModel`) is binding for any pivot. It **does** gate criterion 1b:
  the OWNER is asked to confirm that a dependent's GLB node pivot is its face-frame origin,
  i.e. the translation column of `resolveWorldModel`. That is the natural reading of
  ARCH-OWNER-07 (`Model = T · L`, bake about the body's local origin, never a bounds
  centre) applied to the derived model, and it is the only split of that model with no
  extra offset. But ARCH-OWNER-07 was approved for an authored transform, so this closeout
  records the confirmation as open rather than assuming it (`CRITICAL_REPROS.md` CR-01
  "Owner decision"). Until it is given, 1b is not a merge condition; if the OWNER chooses
  another pivot, 1b is rewritten to it and nothing else in this stage changes.
- **To merge:** the **OD-27** physical-review verdict and the `PROJECT_STATUS.md` Next Stage
  branch rules; no push to `main` without explicit integration authorisation.
- **Left open on purpose:** **OD-16** (decoder policy), **OD-29** (Lock on a dependent).

## Statement

This stage is **not implemented** in FUNCTION-COUNCIL-R2-CLOSEOUT-R1. No product code,
test, fixture, script, workflow or owner document was changed to propose it, and no result
in this file comes from running it.
