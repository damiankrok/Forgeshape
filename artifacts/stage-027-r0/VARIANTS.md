# STAGE027-R0 — Material variants

Every variant below is scored against live source, not against the historical
phrase. Costs are stated as what would have to change, never as an estimate of
effort. Nothing here is selected; `RECOMMENDED_CONTRACT.md` selects.

---

## 1. ISOLATE

The user value is established in `CURRENT_TRUTH.md` §2.3 and is mechanical, not
aesthetic: the stroke hit test consults **only the active body's own triangles**
(`forgeshape_sculpt.cpp:1471-1492`), so an occluding body hides the sculpt
target from the eye without blocking the brush. The user sculpts what they
cannot see. Isolate is the answer to that.

### Variant A — transient viewport isolate (session-only presentation)

**What it is.** One native, process-scoped, session-only flag — the
`DisplaySettingsStore` shape (`forgeshape_display.h`) — that makes the frame's
scene list the active body alone. It rewrites no `SceneObject::visible_`.

| Consequence | Answer |
| --- | --- |
| Domain truth changed | none |
| `.forge` bytes | none; no section, no version, no fixture |
| Project fingerprint | unmoved |
| `ProjectHistory` | no step |
| `SculptHistory` | no entry |
| Undo/Redo | neither history sees it; Undo means what it meant |
| Picking | must follow, or "drawn" and "picked" split into two truths |
| Selection outline | follows for free — the mask pass rasterises the same list |
| Rebuild / upload | **zero**; the snapshot copies `shared_ptr`s and matrices only |
| Per-frame mesh work | none |
| New GPU resources | none |
| Stage025 mask presentation | unaffected — mask is a vertex attribute on the drawn body |
| Exit restores | nothing to restore: durable visibility was never touched |
| Crash / kill mid-isolate | reopens un-isolated; nothing is lost |
| Portability seam | native-side policy value; a desktop shell reads the same flag |

**The one real design question it raises.** Where does the filter live? Two
sub-shapes, materially different:

- **A1 — filter at the seam.** Two call sites change: `forgeshape_jni.cpp:1652`
  (renderer) and `forgeshape_selection.cpp:14-18` (picking). Smallest diff.
  **Risk:** it creates a *second* place that decides what is in the scene,
  beside `snapshot()`, which is exactly the drift `CLAUDE.md` forbids when it
  says hidden is enforced in ONE place.
- **A2 — filter inside `snapshot()`.** `ConstructionScene::snapshot()` skips a
  body that is hidden **or** excluded by the active isolate. One place, one
  fact, renderer and picker correct together with no second predicate, the
  outline correct for free, and the `CLAUDE.md` invariant strengthened rather
  than weakened. **Cost:** `ConstructionScene` learns about an isolate, which is
  a presentation concept — unless the isolate is expressed as a value the scene
  is *given* (an `ObjectId` it is told to restrict to) rather than a session it
  reaches out to read.

A2-with-an-injected-value is the only sub-shape that keeps the single-list
invariant intact. **A1 is not recommended and is recorded so the choice is
visible.**

**Residual risk (must be stated in the contract, not discovered later).**
Isolating in Sculpt would *incidentally* make `FINDING-A`
(`CURRENT_TRUTH.md` §4.1) unreachable, because the stray tap would have nothing
else to hit. That is a side effect, not a fix, and must not be allowed to stand
in for the guard.

### Variant B — isolate implemented by durable visibility edits

**What it is.** Isolate hides every other body by writing
`SceneObject::setVisible(false)` through `setSceneBodyVisible`, and un-isolate
writes them back.

| Consequence | Answer |
| --- | --- |
| Domain truth changed | **yes**, every non-target body |
| `.forge` bytes | **yes** — `SCNE` is pushed to v2 for a project that had no hidden body |
| Project fingerprint | **moves**, so autosave writes a checkpoint of an isolate |
| `ProjectHistory` | one step per body, or one transaction for all of them |
| Undo/Redo | Undo now steps through an isolate, which the user does not think of as an edit |
| Blocked today | `sceneSetBodyVisible` **refuses in Sculpt** (`forgeshape_jni.cpp:3078`); Variant B must weaken or bypass that guard |
| Restore-on-exit | must remember the **pre-isolate** visibility of every body, or a body the user had hidden on purpose comes back visible |
| Where that memory lives | nowhere today — it is new session state, so Variant B needs Variant A's state *as well as* the durable writes |
| Crash mid-isolate | **the project is left isolated**, with the user's own hidden bodies possibly lost |
| Save mid-isolate | the user saves an isolate and does not know it |

**Assessment.** Variant B is strictly more expensive than Variant A *and* needs
Variant A's session state anyway to restore correctly. It converts a viewing
choice into project truth, it fights an existing guard rather than using one,
and it makes crash-safety a new problem. It also collides with the
`Construction history` rule that a step holds bounded Construction-domain
state — recording an isolate as history is recording a camera decision.

**Rejected on source grounds, not on preference.** Recorded here because
`S027R0-04` requires two materially different Isolate semantics to be evaluated
with their data/history/lifecycle consequences, and because the rejection
reasoning is the argument the coordinator needs.

### Variant C — "isolate" delivered as durable Hide made reachable in Sculpt

**What it is.** Not an isolate at all. Stage027 lifts the Sculpt half of
`objectCommandsBlockedByMode()` for **Show/Hide only**, restores the row's
Show/Hide control in Sculpt, and the user hides what is in the way by hand.

| Consequence | Answer |
| --- | --- |
| New concept | **none** — one existing durable command, reachable in one more place |
| Domain truth | yes, and the user chose it explicitly, which is the difference from Variant B |
| `ProjectHistory` | one step, exactly as in Construction |
| Undo in Sculpt | **collides**: Undo in Sculpt means `SculptHistory` (`forgeshape_jni.cpp:5495-5512`), so a Construction step taken in Sculpt **cannot be undone until the user leaves** — the exact reason the guard exists |
| Guard rationale | `ObjectsSectionView.java:305-310` states it: acts refused in Sculpt are refused because Undo is refused there too |
| Multi-body scenes | N taps to clear N occluders, and N more to restore |

**Assessment.** Cheap and honest, but it reopens a guard whose stated reason is
still true, and it answers the occlusion problem with manual labour. Credible
enough to survive as a **fork**, not as a rejection: see
`RECOMMENDED_CONTRACT.md`.

---

## 2. HIDE — reconciliation

**The finding: Hide is already delivered and is not missing anything except
reach.**

- durable, representation-neutral, one owner (`SceneObject::visible_`);
- one enforcement point (`snapshot()`), so drawn and picked are one fact;
- one transaction, one Undo (`setSceneBodyVisible`);
- persisted, versioned and fail-closed (`SCNE` v2);
- a row control with a Show/Hide label read from native on every refresh.

The historical phrase says "Isolate/**hide**", which reads as if hide were
unbuilt. At the time the phrase was written it may well have been; at
`6808fae` it is built. **Stage027 must therefore not create a second Hide.**

The three possible readings, and the verdict on each:

| Reading | Verdict |
| --- | --- |
| Build a Hide command | `ALREADY_IMPLEMENTED` — remove from Stage027 implementation scope |
| Make the existing durable Hide reachable from Sculpt | credible — this is Variant C above, and it is an `OWNER_DECISION_REQUIRED` fork |
| Add a sculpt-local temporary hide *beside* durable hide | **rejected** — two user controls backed by contradictory visibility truths in one Objects row is precisely what `CLAUDE.md` forbids, and the row has one Show/Hide label with nowhere to say which kind it means |

If a transient exclusion is wanted, it must be **Isolate** (one control, one
meaning, whole-session, obviously temporary), never a second per-row Hide.

---

## 3. MESH PREVIEW

The phrase is undefined by any current source. Four readings were tested against
the renderer; two survive as credible, two are rejected outright.

### M1 — topology / wireframe edge overlay over the sculpt mesh

**User value.** High and specific to sculpting: it is the only way to see where
the mesh is dense, where a brush has stretched triangles, and how much resolution
is left before Stage028 density work exists.

**Implementation cost — two sub-paths, both real:**

- **M1a, polygon mode.** Requires `fillModeNonSolid` at device creation.
  `forgeshape_renderer.cpp:601-608` enables **no features at all**
  (`pEnabledFeatures` is null), and `:2567-2570` names this feature as one
  ForgeShape deliberately does not request. So M1a = a device-creation change +
  a feature query + a no-feature fallback path, on a feature that is optional on
  Android GPUs and whose absence cannot be discovered until runtime. It also
  touches `forgeshape_render_recovery`, because a device rebuild must make the
  same decision again.
- **M1b, CPU edge line list.** Build unique mesh edges and draw them through the
  existing `LINE_LIST` pipeline. **Blocked by a stated bound:**
  `kMaxSketchOverlayVertices = 65536` (`forgeshape_sketch_overlay.h:93`) = 32 768
  segments. A frozen sphere fits (~1 440 edges). An Imported Mesh at
  `kMaxMeshVertices = 4 000 000` (`forgeshape_mesh.h:65`) does not, by three
  orders of magnitude. M1b therefore needs either its own buffer and pipeline
  with its own bound, or a stated refusal above some vertex count — and a
  feature that silently stops working on large meshes is worse than an absent
  one.

**GPU/memory cost.** M1a: none beyond a pipeline. M1b: one edge buffer per
sculpt revision, rebuilt on **every stroke** unless it is regenerated lazily —
and a stroke moves positions but not topology, so the index side is stable and
only the position side needs re-upload. That is a real optimisation and also a
real piece of new machinery.

**Picking.** None. An overlay is never picked.

**Conflict with Stage028.** M1 **displays** density; it does not change it.
Showing topology is not remeshing, and nothing in M1 requires a subdivision,
decimation or dyntopo path. It does, however, make the *absence* of Stage028
highly visible, which is a product-sequencing question for the OWNER and not a
technical objection.

**Conflict with `PERF-BASELINE`.** M1b adds per-revision CPU work on the stroke
path, which is the hottest path in the product. This is the one place Stage027
could regress sculpt feel, and it is the strongest argument for M1a or for
deferring M1 entirely.

### M2 — alternate shading to reveal form (already delivered)

`ShadingModel::MatCap` exists, is described in `forgeshape_display.h` as *"the
high-readability sculpt/form view"*, is in the Display popover and is
session-only. `SurfaceShading::Flat` additionally reveals facets.

**Verdict: `ALREADY_IMPLEMENTED`.** If "mesh preview" meant "a way to look at
the form differently while sculpting", the product has two such controls and
Stage027 has nothing to build.

### M3 — a separate derived preview mesh / LOD

**Rejected.** A second derived mesh beside the Frozen Sculpt Mesh is a second
geometry product with its own revision, its own staleness question and its own
publication path — and any generation rule for it (decimation, subdivision) is
Stage028 work arriving early under another name. It is also the one variant that
risks a *second* truth about what the body is.

### M4 — the Imported Mesh Preview, promoted

**Rejected outright.** `forgeshape_import_preview.h:1-40` and `CLAUDE.md` both
state this is a **diagnostic with no user-facing control at all**, deliberately
removed because *"two visible ways to open a `.glb` that did different things to
the project is exactly the confusion that removal prevents"*. It is also
unrelated to sculpting: it previews an import, not a mesh under a brush. It is
listed only because the word "preview" appears in both, and that coincidence is
exactly the kind of thing that turns a diagnostic into a shipped feature by
accident.

### Mesh Preview summary

| Variant | Verdict |
| --- | --- |
| M1a wireframe by polygon mode | credible; needs a device feature ForgeShape does not enable, plus a fallback |
| M1b wireframe by CPU edge list | credible; bounded by 65 536 overlay vertices and adds stroke-path CPU work |
| M2 alternate shading | `ALREADY_IMPLEMENTED` (MatCap, Flat) |
| M3 derived preview mesh / LOD | rejected — Stage028 under another name |
| M4 Imported Mesh Preview promoted | rejected — a diagnostic, unrelated, deliberately controlless |

---

## 4. "REMAINING NON-HISTORY SCULPT WORKFLOW" — classification

Each item was searched for in live source and current docs and classified.

| Item | Class | Source |
| --- | --- | --- |
| Sculpt Undo / Redo over whole strokes | `ALREADY_IMPLEMENTED` | `forgeshape_sculpt_history.h`; `PRODUCT.md:1913-1917` |
| Sculpt History navigator | `ALREADY_IMPLEMENTED` | `SculptHistoryNavigatorView`; `SCULPT-H1` |
| Mask paint + Clear Mask | `ALREADY_IMPLEMENTED` | `SCULPT-FCM-R1`; `PRODUCT.md:1447-1474` |
| Seven brushes | `ALREADY_IMPLEMENTED` | `SculptTool`; `PRODUCT.md:1407-1446` |
| Radius / Strength edge controls | `ALREADY_IMPLEMENTED` | `BrushEdgeControlsView` |
| Back / Resume, both vocabularies | `ALREADY_IMPLEMENTED` | `PRODUCT.md:1492-1501` |
| Reset Sculpt from Shape / from Imported Mesh | `ALREADY_IMPLEMENTED` | `SculptContextView` |
| Stale-source warning | `ALREADY_IMPLEMENTED` | `SculptContextView`; `markSourceStale` |
| Imported-Mesh sculpt parity | `ALREADY_IMPLEMENTED` | `IMPORT-01B`; `PRODUCT.md:1348-1385` |
| Durable Hide / Show | `ALREADY_IMPLEMENTED` | Stage 018A; `CURRENT_TRUTH.md` §1 |
| Selection outline | `ALREADY_IMPLEMENTED` | `SEL-OUT-R1` (note `FINDING-C`: `PRODUCT.md` is stale) |
| **Isolate (any form)** | **`STAGE027_CANDIDATE`** | no product source exists; the only `isolate*` hits are test helpers (`ObjectsDeleteTest.java:601`, `EditorWorkspaceObjectsTest.java:561`) that move bodies apart — scaffolding, not a feature |
| **Hide reachable from Sculpt** | **`STAGE027_CANDIDATE` (fork)** | `forgeshape_jni.cpp:3017-3019`; `EditorWorkspaceView.java:2551` |
| **`FINDING-A` viewport-tap body switch in Sculpt** | **`STAGE027_CANDIDATE`** (defect, not a feature) | `forgeshape_jni.cpp:7250-7284` vs `:2800-2806` and `PRODUCT.md:1286-1290` |
| **`FINDING-B` Sculpt reachable on a hidden body** | **`STAGE027_CANDIDATE`** | `forgeshape_jni.cpp:2589-2640`, `:2696-2716` |
| Wireframe / topology preview | `STAGE027_CANDIDATE` **if** the OWNER defines "mesh preview" as M1 | §3 above |
| Sculpt dimensions | `OWNED_BY_OTHER_TASK` — `SCULPT-DIM-01`, blocked by **OQ-02** | `PROJECT_STATUS.md:4242-4244` |
| Stylus hover preview | `OWNED_BY_OTHER_TASK` — deferred, blocked by the autosave-fingerprint race | `PROJECT_STATUS.md:4232-4238` |
| Stylus pressure / tilt | `OWNED_BY_OTHER_TASK` — `STYLUS-G1` / Stage026 | prompt §3 |
| Remesh, subdivide, dyntopo, density | `OWNED_BY_OTHER_TASK` — Stage028 | prompt §3 |
| Symmetry (X mirror strokes) | `OWNED_BY_OTHER_TASK` — explicitly not `MIRROR-01` | `CLAUDE.md` mirror rule |
| Mask Invert / Grow / Shrink / Blur, mask by topology, mask persistence | `OWNED_BY_OTHER_TASK` | `CLAUDE.md` mask rule; `PROJECT_STATUS.md:4247-4249` |
| Body switching **allowed** in Sculpt | `NO_SOURCE / DO_NOT_INVENT` | the current rule is the opposite, deliberately (`forgeshape_jni.cpp:2800-2806`) |
| Multi-select in Sculpt | `NO_SOURCE / DO_NOT_INVENT` | `PRODUCT.md:1929-1934`; `CLAUDE.md` non-goals |
| Focus-on-selection / frame-the-body camera | `NO_SOURCE / DO_NOT_INVENT` for Stage027 | `PRODUCT.md:1874-1877` lists it as unimplemented, unattached to any stage |
| Brush presets, layers, new brushes | `NO_SOURCE / DO_NOT_INVENT` | `CLAUDE.md` one-brush-kernel rule |
| Sculpt-local visibility persisted to `.forge` | `NO_SOURCE / DO_NOT_INVENT` | no section, no version, no fixture, no requirement |
