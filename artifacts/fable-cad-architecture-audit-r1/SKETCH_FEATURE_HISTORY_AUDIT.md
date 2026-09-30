# Sketch / feature history audit (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Read-only. Baseline `main = 509de02` (the working tree is content-identical to
it). Paths: `cpp/` = `app/src/main/cpp`, `java/` = `app/src/main/java/com/forgeshape/app`.
Evidence tags: `SOURCE_CONFIRMED`, `TEST_CONFIRMED` (a test READ, not run),
`DEVICE_EVIDENCE` (a recorded CI DEVICE frame or fact), `DOC_ONLY`, `INFERRED`,
`UNVERIFIED`.

This answers OWNER observation **O3** ("retained sketches/features exist but
are not discoverable/reusable enough") and required conclusions **C4** and
**C5**.

## 1. The native object graph — Base, Add A, Cut B

For a body built as Base Sketch → Base Extrude (New Body), Face Sketch A →
Add A, Face Sketch B → Cut B, the retained truth is exactly this
(`SOURCE_CONFIRMED`, `cpp/forgeshape_cad_body.h:245-284`):

```
SceneObject (ObjectId N)                                  cpp/forgeshape_scene.h
 └─ CadBody                                               cad_body.h:395-445
     state_ : CadBodyState                                cad_body.h:277-284
       ├─ sketch  : CadSketch        ← Base Sketch  (feature id 1, IMPLICIT: no CadFeature record)
       │     plane XY|XZ|YZ, entities[], nextEntityId,
       │     hasFaceSupport / faceSupport : TopoRef  ← may name ANOTHER body's face (CAD-A3)
       ├─ extrude : ExtrudeFeature   ← Base Extrude, operation NewBody (implicit)
       └─ laterFeatures : vector<CadFeature>              cad_body.h:260-270
            [0] { featureId 2, operation Add,
                  support { featureId 1, CadFaceToken, lineageToken },
                  sketch  : CadSketch  (forced XY, no TopoRef)   ← Face Sketch A, BY VALUE
                  extrude : ExtrudeFeature }                     ← Add A
            [1] { featureId 3, operation Cut,
                  support { featureId ∈ {1,2}, face, lineageToken },
                  sketch  : CadSketch                            ← Face Sketch B, BY VALUE
                  extrude : ExtrudeFeature }                     ← Cut B
```

| Question | Answer | Evidence |
| --- | --- | --- |
| Where is each sketch stored? | Inline, by value: the base in `CadBodyState::sketch`, every later one in `CadFeature::sketch`. | `cad_body.h:268, :279` SOURCE_CONFIRMED |
| Do sketches have stable ids? | **No.** `CadSketch` has no id. The only ids inside a sketch are per-entity (`SketchEntityId`, `cpp/forgeshape_sketch.h:255-259`) plus `nextEntityId`. Only FEATURES have ids. | SOURCE_CONFIRMED |
| Are feature ids stable? | Yes: strictly ascending, base = `kCadFeatureId` (1), `nextCadFeatureId` = last + 1 (`cad_body.cpp:349-352`). There is no stored high-water mark; harmless only because no delete-feature API exists. | SOURCE_CONFIRMED |
| Does a feature own or reference its sketch? | **Owns** it (embedded member). | SOURCE_CONFIRMED |
| Can two features reference one sketch? | **No, structurally.** There is no sketch identity to reference. `CLAUDE.md:438` lists "multi-feature reuse of one sketch" as not delivered. | SOURCE_CONFIRMED |
| Is the `.forge` shape the same? | Yes: `CADB` v5 writes each later feature's `nextEntityId`, extrusion and entities inline per feature (`cpp/forgeshape_project_document.cpp:1145-1160`, read `:1797-1798`; `DATA_PACKAGE_SPEC.md:740-770`). No sketch table. | SOURCE_CONFIRMED |
| Base vs later feature | The base has no `CadFeature` record; `cadFeatureAt(state, 0)` synthesises a `CadFeatureView` with a null `support` (`cad_body.cpp:323-326`). Two support mechanisms exist by design: the BASE sketch may carry a cross-body `TopoRef` (`sketch.h:490-497`, read only through `SceneObject::cadFaceSupportOrNull`, `scene.h:128-131`); a LATER feature carries `CadFeatureSupport` to an earlier feature of the same body. `buildCadChainGeometry` refuses a later sketch that is not XY or has a TopoRef (`cad_feature.cpp:226-232`) and `candidateState` strips it (`sketch_session.cpp:1489-1494`). | SOURCE_CONFIRMED |

### 1.1 How a support reference survives regeneration

- The lineage signature is FNV-1a 64 over the first region's outer anchor, the
  face count, and each face's token code and eligibility
  (`cpp/forgeshape_cad_feature.cpp:169-177`). Sizes, depth and direction are
  NOT in it, so a size edit upstream keeps a dependent attached and a structural
  edit fails closed. `SOURCE_CONFIRMED`.
- Resolution on every regeneration (`cad_feature.cpp:233-252`): find the earlier
  `CadFeatureGeometry` by `support.featureId`; require equal signature; match
  the token with `sameCadFaceToken`; refuse `FeatureSupportInvalid` if missing
  or ineligible; otherwise `placement = face->frame`. No nearest-face fallback.
- Upstream SIZE edit: `TEST_CONFIRMED` by `CADVS_OPS_18` (`cad_feature_selftest.cpp:1538`),
  `CADVS_OPS_19` (`:1541`); `DEVICE_EVIDENCE` `feature_edit_roundtrip`
  (`CadVerticalSliceTest.java:680-696`, the Add rises to 1.75 m).
- Upstream STRUCTURAL edit: `TEST_CONFIRMED` by `CADVS_OPS_20` (`:1552`) and
  `CADVS_SES_30` (`:2109`, asserts `sameCadBodyState(before)` and unchanged undo depth).
- Three refusal names, one per situation: `FeatureSupportInvalid` (same-body
  support does not resolve), `SupportFaceLost` (an earlier Cut carved the face
  away, `cad_body.cpp:659-664`), `DependentFaceLost` (a DIFFERENT body's base
  sketch stands on a face this candidate would strip,
  `sketch_session.cpp:289-315`, checked by both `commitEdit` and `commitIntoTarget`).

### 1.2 Regeneration on an upstream edit

`regenerateCadBody` (`cad_body.cpp:586-729`) builds the whole chain geometry
first, then applies each later feature in order through ONE kernel boolean,
naming `failedFeatureId` on any refusal. `CadBody::applyState`
(`:780-806`) regenerates the requested state into scratch and writes nothing on
refusal (`TEST_CONFIRMED` `CADVS_OPS_22`, `:1580`). Editing feature *i*
therefore regenerates the whole chain, not *i..end* as `cad_body.h:56` says —
a documentary drift, not a behavioural defect (`SOURCE_CONFIRMED`: no
`throughFeatureId` caching in `regenerateCadBody`).

## 2. Every native edit door, and its JNI export

`SketchSession` is one process-scoped object (`sketch_session.h:704-708`).

| Entry point | What it stages | JNI export (`cpp/forgeshape_jni.cpp`) |
| --- | --- | --- |
| `begin(Workplane)` (`sketch_session.cpp:151-193`) | fresh world-plane sketch; `editingFeatureId_ = 0`, `operation_ = NewBody` | `sketchBegin` `:3993`; support chooser `:4040`; bootstrap (`EditorWorkspaceView.java:2195, :3207`) |
| `beginOnFace(frame, TopoRef, producerState*)` (`:123-149`) | `begin(XY)` + face support; with `producerState` stages `targetBodyId_`/`targetBaseState_` so Add/Cut are offered | no direct export; `confirmChosenSupportLocked` `:4036-4062` |
| `beginEdit(bodyId, state, frame)` (`:226-229`) | **one-line forwarder** to `beginEditFeature(…, kCadFeatureId, …, startReady=false)` | `sketchBeginEdit` `:5372-5436` |
| `beginEditFeature(bodyId, state, featureId, frame, startReady)` (`:231-287`) | validates the whole state, copies `sketch_`, `extrude_`, `operation_` and (later feature) `editingSupport_`; stages the whole chain as `targetBaseState_`; `startReady` lands in Ready | `sketchBeginEditFeature` `:5438-5487`, frame via `resolveCadFeatureWorldFrame` `:4909-4930` |
| `commit` (`:1632-1679`) | New Body: ONE `ScopedConstructionEdit` around `addCadBody`; Add/Cut → `commitIntoTarget` (`:1681-1737`, refuses if the target changed under the session) | `sketchCommit` `:4337-4395`; first project → `commitFirstCadProject` (`:4348-4356`) |
| `commitEdit` (`:317-390`) | evaluation → dependents check → ONE edit + `applyState(candidateState())` | `sketchCommitEdit` `:5504-5526` |
| `cancel` (`:195-224`) | drops everything | `sketchCancel` `:4189` |
| `candidateState` (`:1470-1512`) | THE assembly for all four cases (new body / base edit / later-feature edit / new later feature) | read by `evaluateCandidate` `:1581-1615` and both commits |
| reads | `cadFeatureCount` `:4938`, `cadFeatureInfo` `:4952` (9 slots per feature), `sketchEditingFeatureId` `:5489`, `sketchEditingBodyId` `:5496`, `cadBodySketchAnchor` `:5142` (**BASE only**, `:5171-5176`) | |

Facts about reopening (`SOURCE_CONFIRMED` unless tagged):

- Reopening a feature stages its sketch AND its extrusion AND its operation
  (`sketch_session.cpp:261-263`). `TEST_CONFIRMED` `CADVS_SES_25` (`:2054-2066`);
  `DEVICE_EVIDENCE` `feature_edit_roundtrip` (`CadVerticalSliceTest.java:658-662`).
- A later feature may switch Add ↔ Cut but never New Body
  (`operationAvailable`, `:1522-1524`; `setOperation(NewBody)` →
  `InvalidFeatureOperation`, `:1543-1547`). The base offers New Body only
  (`:1519-1521`). `TEST_CONFIRMED` `CADVS_SES_25`, `CADVS_SES_29`.
- The region selection can be changed on reopen: `reconcileRegionSelection`
  keeps a still-derivable selection (`:1237-1241`), and `toggleRegion` /
  `selectProfile` act on `extrude_` in Ready with no `editingFeatureId_` guard
  (`:1273-1353`). `INFERRED`; no test toggles a region inside an EDIT session
  (`RUNTIME_VERIFICATION_NEEDED`, see `RUNTIME_VERIFICATION_GAPS.md`).
- NOT editable: a feature's support face (`editingSupport_` is copied and never
  rewritten, `:264-266, :1497-1498`), its position in the chain, its id. There
  is no delete, reorder, suppress or insert-before API in `SketchSession`,
  `CadBody` or JNI (established by enumerating the export list).
- Base-only pre-chain doors are still live: `cadState` (`:5574-5600`) and
  `cadApplyExtrude/Rectangle/Circle` (`:5731-5750`, via `applyCadCandidate`
  `:5647-5688`) touch `candidate.sketch` / `candidate.extrude` — the base —
  and never `laterFeatures`. They regenerate the whole chain through the same
  `applyState`, so they are not a second truth; they are a second DOOR to one
  feature. A later feature's depth or radius has no typed field anywhere.
  `applyCadExtrude/Rectangle/Circle` in `cad_body.cpp:808-855` have no
  production caller (`INFERRED` from grep; self-tests only).

## 3. The Android path to each feature

| Surface | What it does | Evidence |
| --- | --- | --- |
| `CadFeatureEditorView.refreshFeatures` (`java/CadFeatureEditorView.java:233-269`) | the ONLY producer of feature rows; `if (count <= 1) GONE` (`:237-240`); one row per feature, `"%1$d. Extrude — %2$s"` (`res/values/strings_cad_vs.xml:39`), tag = featureId, tap → `host.onEditCadFeatureRequested(featureId)` | SOURCE_CONFIRMED |
| Where that view lives | inside the precision surface's CAD Shape body: `EditorWorkspaceView.showActiveInspectorBody` (`:3042-3079`) selects `cadEditor` when `sceneActiveBodyIsCad()` and the tool is not Transform (`:3069-3072`) | SOURCE_CONFIRMED |
| `EditorWorkspaceView.onEditCadFeatureRequested` (`:3782-3798`) | `featureId <= 1` → `onEditCadSketchRequested()` (base, `sketchBeginEdit`); else `sketchBeginEditFeature(bodyId, featureId, true)`. Active body only. | SOURCE_CONFIRMED |
| Canvas chip `cad_canvas_edit_sketch` (`java/CadExtrudeCanvasView.java:437-450, :903-928`) | shown over a committed CAD body with no session; anchored by `cadBodySketchAnchor` = the BASE sketch; tap → `EditorWorkspaceView:3520-3523` → base `sketchBeginEdit`. **Never names a later feature.** | SOURCE_CONFIRMED; `TEST_CONFIRMED` lifecycle `Ui3dStateAuditTest.ui3d10` (`:543-600`) |
| Objects capsule / rows | no CAD or feature code (`ObjectsSectionView.java`, grep) | SOURCE_CONFIRMED |
| Viewport pick | no pick resolves to a feature; `sketchBeginEditFeature` has exactly one Java caller (`EditorWorkspaceView.java:3789`) | SOURCE_CONFIRMED |
| Retained sketch visibility | `SketchSession::overlay()` returns EMPTY when inactive (`sketch_session.cpp:1748-1758`). No show-sketch / sketch-visibility concept exists anywhere (grep over cpp, java, strings, ids). Later features' sketches have **zero viewport presence**. | SOURCE_CONFIRMED |

Taps to reopen Cut B from the resting workspace, body already active
(`INFERRED` from the call graph): precision toggle (1) + row (1) = **2 taps**
best case; typically 3–4 (Shape rail entry if Transform is held; select the
body; scroll the capped bottom sheet, `PropertyInspectorView:29-34`).

## 4. Why later sketches feel inaccessible

The OWNER's feeling is explained by four `SOURCE_CONFIRMED` facts, none of
which is a native capability gap:

1. **The list is two surfaces deep and withdrawn for one-feature bodies.** It
   exists only inside the precision surface's Shape body and is GONE when
   `count <= 1`, so the user never sees "features" until the second one lands.
2. **The only canvas affordance reopens the base sketch.** One chip, one
   sketch, anchored by a base-only JNI read. A three-sketch body shows one chip.
3. **No retained sketch is ever drawn.** After commit the sketch lines vanish;
   there is no "show sketch", no ghosted profile, no sketch item. The user
   cannot point at Face Sketch B because nothing represents it.
4. **The typed Shape fields are base-only and do not say so.** `cadState` /
   `cadApply*` edit feature 1 while the panel reads as "the body".

Outcome class (from the prompt's list): **functionality exists but UI hides
it** for reopening; **a sketch only exists inside its feature and cannot be
reused** for reuse. Both hold at once.

## 5. Can one retained sketch drive more than one feature?

**No (C5).** `SOURCE_CONFIRMED`: a sketch is an embedded value with no identity
(`cad_body.h:268`); `candidateState` copies the session's `sketch_` into a new
`CadFeature` per commit (`sketch_session.cpp:1487-1508`); `CADB` v5 stores
sketches inline per feature. What is missing, in order of necessity:

1. **A sketch identity** distinct from a feature identity (a `sketchId` per
   body, allocated like `featureId`).
2. **A sketch table** in `CadBodyState` (`sketches[]`) with features holding
   a `sketchId` instead of a `CadSketch` value. The base's cross-body
   `TopoRef` and a later sketch's `CadFeatureSupport` would move onto the
   SKETCH record (a sketch is placed; a feature consumes a placed sketch),
   which is also where Fusion and Inventor put them (see `CAD_BENCHMARK.md`).
3. **A dependency rule**: a sketch with consumers cannot be deleted; editing a
   shared sketch re-validates every consumer's region selection
   (`ProfileRegionMismatch` per consumer, failing closed as today).
4. **A `CADB` v6** (sketch table + feature `sketchId`), with the v5 reader
   mapping each inline sketch to a fresh id, so every older fixture stays
   byte-identical on write only if the writer keeps emitting v5 for a body
   whose sketches are all single-consumer — the same "written only when
   needed" rule v2..v5 already use.
5. **Regions are already sketch-relative**, so nothing in
   `ProfileRegionRef` changes: two features selecting different regions of one
   sketch is exactly `extrudeRegions` per feature over one extraction.

This is an OWNER product decision first (`OWNER_DECISIONS.md` D3), because it
changes what "a sketch" is to the user; the engineering path above is bounded
and does not touch the kernel, the regions or the HUD.

## 6. Stale comments and documents (documentary drift, `SOURCE_CONFIRMED`)

| Location | Text | Verdict |
| --- | --- | --- |
| `java/CadFeatureEditorView.java:120-124` | "One control, because a CAD Body has exactly one sketch and needs no feature tree to say which." | **STALE**: the same class builds the feature list at `:139-142, :233-269`. |
| `java/CadFeatureEditorView.java:10-18` | "A CAD Body is a sketch extruded along its workplane's normal, so its editable truth is the profile's sizes … the depth and its direction." | Stale for a multi-feature body; the typed fields are base-only and the doc does not say so. |
| `cpp/forgeshape_cad_body.h:1-2, :18-23` | header line and first truth diagram | Stale; superseded by `:40-59` in the same file (two diagrams, one file). |
| `cpp/forgeshape_cad_body.h:56` | "Editing feature i regenerates i..end" | Not what `regenerateCadBody` does (whole chain). |
| `cpp/forgeshape_sketch_session.h:1-2, :16-18` | "the one commit that turns a sketch into a CAD Body" | Pre-chain header; `:237-248, :513-519` are current. |
| `cpp/forgeshape_sketch.h:300-301` | "every CAD body's single Sketch+Extrude feature" | "single" is now "first". |
| `ARCHITECTURE.md:160` | "one sketch, one linear extrusion" in the ownership table | STALE; `:2386-2391, :2651-2660` are current. |
| `CLAUDE.md:1216` | "*CAD Body* (a body made by extruding a sketch; its sketch and depth are editable)" | Stale vocabulary entry; `:1240` defines *feature* correctly. |
| `PRODUCT.md:757-768` | singular "its sketch and its extrusion" | `:779-785` is current. |
| `PRODUCT.md:1957` | "There is no **Mirror**" | STALE since `MIRROR-01` (already noted by FUNCTION-COUNCIL-R1). |

## 7. Tests that pin this area (READ, not run)

- Device: `CadVerticalSliceTest.feature_edit_roundtrip` (`:619`) reopens
  feature 2 (an Add) from the list, one Undo, upstream depth edit carries it,
  save/reopen. `SketchUxTest` `:439`, `CadExtrudeExtentTest.cadext07` `:317`,
  `Ui3dStateAuditTest.ui3d10` `:543` cover base Edit Sketch. **No device test
  reopens a Cut, reopens feature 3, or changes a region or the operation inside
  an edit session.**
- Native: `CADVS_OPS_11, 12, 13, 16, 18, 19, 20, 22, 26, 27, 28` (support and
  upstream edits), `CADVS_SES_25–30` (reopen, one step, undo/redo, base offers
  New Body only, structural base edit refused), `CADVS_IO_10–12, 15, 23`
  (persistence, lineage token format values).

## 8. Verdicts

- **C4 — retained and reopenable today?** Yes, natively complete and
  coherent: every feature's sketch, extrusion, operation and support are
  retained by value and reopen through one staged path
  (`SOURCE_CONFIRMED`, `TEST_CONFIRMED` for the base and an Add;
  `INFERRED` for a Cut and a third feature).
- **C5 — one sketch, several features?** No, structurally; §5 names what is
  missing.
- **Coherence.** One owner per sketch, one id space per level, one support
  mechanism per level, one staged edit path, one regeneration path. The
  incoherence is documentary (§6), plus one product-level duality the
  Function Council already raised: a face sketch on ANOTHER body makes a new
  following body; on the SAME body it offers Add/Cut. The user cannot tell
  those apart on the canvas (`OWNER_DECISIONS.md` D4).
