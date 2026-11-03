# CAD-R2 — DESIGN

Decisions taken before implementation, from `BEFORE.md`. The architecture
stays authoritative: native C++ owns every semantic decision, JNI is an
adapter with explicit slot layouts, Java holds presentation and volatile draft
only, the mesh is derived.

## 0. Format policy — **NO FORMAT CHANGE**

Every act this stage adds produces a `CadBodyState` the existing model already
admits and the existing codec already writes:

| New act | Resulting state | Written as |
| --- | --- | --- |
| Keep a new sketch on a body's own feature face | one more `CadSketchRecord` with `hasFeatureSupport`, consumed by no feature | `CADB` v6 (an unused sketch makes the state not legacy-representable) — exactly what `cad_sketch_shared`'s table layout already carries |
| Reuse a sketch for a later Add / Cut | `appendCadLaterFeature(op, sketchId, extrude)` — two features naming one sketch id | `CADB` v6 (`cad_sketch_shared`) |
| Delete an unused sketch | the record removed, `nextSketchId` unchanged | v6 (marks persisted) or, when the table returns to one-sketch-per-feature with the legacy marks, the legacy writer — the codec's own unchanged predicate decides |
| Edit an unused sketch | the record's entities replaced | unchanged layout |

No `CADB` version, section, code, enum value of the format, lineage rule or
fixture changes. The corpus stays 72/72 byte-identical. `CadStatus` (a runtime
JNI vocabulary, never persisted) gains two APPENDED codes; no existing code
moves.

### Blocked sub-feature — `BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH`

A retained sketch on a WORLD plane inside an EXISTING CAD body would be a
second ROOT sketch, and `validateCadSketchTable` refuses exactly that
(`SketchSupportInvalid`: "a second sketch on a workplane would be a second
answer to where the body is"). Admitting it needs a new durable meaning (a
body-local workplane sketch that is not the base's) and a validation change a
`CADB` v6 reader of an older build would not share. Per the prompt this is NOT
masked: a world-plane New Sketch keeps creating a NEW body (its existing
behaviour), and only face-supported retained sketches on the body's own
features are offered. Reproducer: host check `R2_SK_06` (a second root record
is refused by name).

Also out of scope and stated rather than disabled silently: reuse on a Revolve
body (`RevolveLaterFeatureUnsupported`, the existing R1 rule), Revolve
Add/Cut, Extrude↔Revolve conversion, feature reorder/suppression, cascading
delete.

## 1. The one flow

```
user act (History / Sketches / Steps row, Edit Sketch chip, drawer New Sketch)
   │  Java: reads native rows, offers ONLY the acts native permits
   ▼
JNI entry (lock, product-mode guards, explicit slot layout, length checks)
   ▼
native semantic validation      CadSketchInventory / removeUnusedCadSketch /
   │                            SketchSession::beginReuseSketch / beginEditSketch
   ▼
candidate                       SketchSession::candidateState() + evaluateCandidate()
   │  (latest-only, revisioned; the preview draws THIS evaluation)
   ▼
commit                          ONE ScopedConstructionEdit around ONE CadBody::applyState
   │  (whole-chain validate + regenerate; refusal writes nothing)
   ▼
history                         ConstructionHistory snapshot (marks restored by Undo)
   ▼
persistence                     unchanged codec (CADB v6 when not legacy-representable)
```

Cancel at any point before the commit drops the session's staged copy: no
history step, no revision, no fingerprint move, no autosave trigger.

## 2. API and ownership

### Native (new)

| Symbol | File | Owns |
| --- | --- | --- |
| `CadSketchInventory buildCadSketchInventory(const CadBodyState&, const CadRegenerationReport&)` | `forgeshape_cad_sketch_inventory.{h,cpp}` (new) | One row per retained sketch, ascending id: root / support kind / plane / support feature + face, entities, dimensions, consumers (feature ids, chain order), state (`Ok`, `Unused`, `Failed`, `NotRegenerated`, `SupportLost`), permitted acts and the refusal for each act not permitted. Derived on every read, stored nowhere. |
| `CadStatus removeUnusedCadSketch(const CadBodyState&, CadSketchId, CadBodyState*)` | same | Pure: refuses `SketchIsRoot`, `SketchInUse`, `SketchNotFound`; never lowers a mark. |
| `CadStatus deleteCadSketch(ConstructionScene&, ConstructionHistory&, ObjectId, CadSketchId)` | same | ONE `ScopedConstructionEdit` around ONE `applyState`; refused while an edit/sketch is open. |
| `SketchSession::beginReuseSketch(bodyId, state, sketchId, op, worldFrame)` | `forgeshape_sketch_session.{h,cpp}` | Stages a NEW later feature that extrudes an EXISTING sketch by id; opens directly in Ready; Back to Sketch refused (`SketchInUse` — the shared sketch is edited through Edit Sketch, which regenerates every consumer). Commits through the existing `commitIntoTarget`. |
| `SketchSession::beginEditSketch(bodyId, state, sketchId, worldFrame)` | same | A consumed sketch → `beginEditFeature(firstConsumer, startReady=false)` (the existing path, all consumers regenerate). An unused sketch → a SKETCH-ONLY session committed by `commitSketch`. |
| `SketchSession::commitSketch(scene, history, CadSketchId* out)` | same | Keep Sketch: a new face sketch on the producer's own feature face, or a sketch-only edit, as ONE `applyState`. No feature is created. |
| `SketchSession::canKeepSketch()` / `sketchOnly()` / `reuseSketchId()` | same | Chrome reads these; Java mirrors none. |
| `CadStatus::SketchInUse`, `CadStatus::SketchIsRoot` | `forgeshape_sketch.h` | Appended runtime codes 91, 92. |
| `forgeshape_cad_r2_selftest.{h,cpp}` | new | `R2_SK_*`, `R2_HIST_*`, `R2_PF_*`, `R2_SUP_*`, `R2_DATA_*` |
| `forgeshape_cad_oracle_selftest.{h,cpp}` | new | `R2_OR_*` independent oracles; prints `CAD_R2_ORACLE_PASS=<N>`. Both run INSIDE the CAD_FEATURE suite: no new startup token (25 stays 25). |

### JNI (new entry points; every array length checked, slots typed)

| Entry | Layout |
| --- | --- |
| `cadSketchInventory(long body, double[] header, double[] rows)` | header 4: status, row count, next sketch id, failed feature; row 20 slots (see `NativeViewport.SKETCH_INV_*`); returns count or -1 |
| `cadSketchConsumers(long body, long sketchId, long[] out)` | consumer feature ids in chain order; returns count or -1 |
| `cadDeleteSketch(long body, long sketchId)` | CAD_* code |
| `sketchBeginReuseSketch(long body, long sketchId, int operation)` | CAD_* code |
| `sketchBeginEditSketch(long body, long sketchId)` | CAD_* code |
| `sketchCommitSketch()` / `sketchCanKeepSketch()` / `sketchIsSketchOnly()` / `sketchReuseSketchId()` | code / boolean / boolean / long |

### Android

| Class | Change |
| --- | --- |
| `SketchBrowserPresentation` (new, pure Java) | Reads the inventory; row labels, `Unused`/`Used by` text, the act list per row, dependency text. Holds no state. |
| `FeatureHistoryView` | Three segments in ONE surface — **Steps** (the existing timeline, tap = edit, unchanged), **Sketches** (the Sketch Browser), **Features** (per-feature action menu). In Sketches/Features a row tap SELECTS it and an inline action strip below the list offers only the acts native permits (48 dp); a not-permitted act is ABSENT and its reason is printed (a control that cannot succeed is not drawn). |
| `EditorWorkspaceView` | Wires the acts to the native entries; `Keep Sketch` toolbar transition when `sketchCanKeepSketch()`; `Finish Sketch` withdrawn in a sketch-only session. |
| `CadStatusMessages` | Two new sentences. |

## 3. Snapshot / fingerprint before

`projectSemanticFingerprint` and the corpus digests are printed by the PROJECT
and CAD_FEATURE suites (`FORGESHAPE_PROJECT_GOLDEN_SHA256*`,
`FORGESHAPE_CAD_GOLDEN_SHA256_V5`). They are compared before/after in
`TESTS.md`; this stage must not move any of them.

## 4. Undo boundaries

| Act | History |
| --- | --- |
| Keep Sketch (new) | 1 step; Undo removes the record, Redo restores the SAME id (snapshot) |
| Edit unused sketch (Keep Sketch) | 1 step |
| Edit consumed sketch / feature | 1 step (existing `commitEdit`) |
| Reuse → Add/Cut | 1 step (existing `commitIntoTarget`) |
| Delete Sketch | 1 step; `nextSketchId` never lowered, so a later new sketch never reuses the id on this branch |
| Preview, Cancel, Back, segment switch, row select | 0 steps, no fingerprint move |

`SculptHistory` is never touched by any path here.

## 5. Tests planned

Host (inside CAD_FEATURE, no new token): `R2_SK_01..`, `R2_HIST_01..`,
`R2_PF_01..`, `R2_SUP_01..`, `R2_DATA_01..`, `R2_OR_01..` (≥ 30 logical
checks, ≥ 12 oracles). JVM: `SketchBrowserPresentationTest` (≥ 15). Device:
`CadRetainedSketchOwnerTest` (D01..D12) through real window touches.
