# FORGESHAPE MASTER PLAN

**Docs revision:** 4.1.0  
**As of:** 2026-09-07  
**Migration source:** `FORGESHAPE_MASTER_PLAN_v4.0.58.md` + `FORGESHAPE_PROJECT_STATE_v4.0.58.md` + `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58.md` + current SCULPT-H1 prompt + available `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md` + owner audit instruction `PROMPT_D_FORGESHAPE.md`.  
**Product code changed by this revision:** no.  
**Repository baseline:** accepted-from-report only; MIRROR-01 ended at short commit `3283089`, clean. The full 40-character commit was not supplied and remains to be resolved by the next coding preflight.  
**Canonical use:** this document owns current product/system contracts, roadmap, coordinator policy and active decision summaries. HISTORY owns event chronology. PROJECT STATE owns the short operational snapshot.

---

## 1. START HERE

### 1.1 Product

ForgeShape is a mobile-first 3D modelling application, currently implemented on Android with a platform-neutral C++ core, JNI as an adapter and a Vulkan renderer. The approved MVP combines exact Construction/CAD workflows with direct Sculpt workflows and portable project persistence. The current strategic thesis (`OQ-C`) is still not owner-resolved: CAD may be the primary differentiator on Android while Sculpt acts as a companion, but this remains a question rather than a fact.

The current user-visible product is no longer an empty UI shell. It can create and edit projects, persist them, import/export a bounded GLB subset, perform exact Construction transforms and dimensions, create CAD Sketch/Extrude content, sculpt mesh bodies, undo/redo Sculpt strokes, manage objects and use the accepted mobile workspace. The remaining roadmap is feature expansion and hardening, not proof that an application exists.

### 1.2 Current delivered capabilities and evidence boundary

| Capability | Current truth | Evidence / acceptance boundary |
|---|---|---|
| Home / New / Open | Full-screen Home, New CAD/Sculpt flow and Open are implemented. | CAD-A3-C2 technical PASS; OWNER accepted the current CAD-A3-C2 behavior. |
| `.forge` project persistence | Versioned self-contained save/load, corruption/version handling, autosave/recovery and transfer are implemented. | E2E-R1A/B technical PASS; `.forge` is native project truth, not interchange. |
| Construction | Primitive family, multi-object scene, exact transform gizmo, exact Body Dimensions/anchors/Relative Scale and world-plane Mirror are implemented. | Stage 020M PASS at `94daaa...`; MIRROR-01 PASS from report, full final hash still unresolved. |
| CAD / Sketch | World/face-supported Sketch, Line/Polyline/Rectangle/Circle, Arc/Spline authored truth, line dimension edit, repeated Extrude New Body and staged Edit Sketch are implemented. | CAD-A1/A2/A3/C1/C2 technical PASS; current CAD UX OWNER accepted. Booleans/Add-Cut/Revolve remain later. |
| Sculpt | Existing brush subset, current/retained Sculpt truth, Imported-Mesh Sculpt and separate bounded per-stroke Sculpt Undo/Redo are implemented. | SCULPT-UNDO-R0 OWNER accepted. `SCULPT-H1` is only remaining navigator/jump/hover UX. Stage 025 later adds Flatten/Crease/Mask. |
| Imported Mesh | Bounded static GLB import produces durable project objects with ObjectId, transform, `.forge` persistence and Sculpt entry. | IMPORT-01A OWNER real-file accepted; IMPORT-01B technical PASS. Broad/high-density import remains gated. |
| GLB interchange | Static export and bounded static import subset exist. | Internal roundtrip and real owner import are green. External engine/export OWNER checkpoint `GATE-E2E` remains deferred; do not generalize to universal interoperability. |
| Objects | Delete exists; Rename/Visibility/Lock/Duplicate are present in the clean baseline used by Stage 020M. | Direct Stage 018A completion report was not ingested in this chat; exact evidence provenance remains limited. Delete→Undo→Redo OWNER verdict is deferred. |
| UI / preferences | Accepted edge-hugging workspace, Settings Hub, five palettes, handedness, gizmo visual size/thickness and renderer selection outline exist. | UI-PREF OWNER accepted under explicit aggregate waiver; SEL-OUT technical feature green with OWNER visual review deferred. |
| Test runner | PlanOnly, ShardOnly, same-tree Resume, fingerprinted checkpoints, timing/budgets and failure classification exist. | TEST-RUNTIME-R1 PASS. Full-suite semantics remain strict when a full aggregate is actually required. |

### 1.3 Important not-yet-implemented scope

The nearest meaningful gaps are: Sculpt history navigator UX (`SCULPT-H1`), Flatten/Crease/Mask (`Stage 025`), stylus-gated X symmetry/pressure (`Stage 026`), density/remesh (`Stage 028`), multi-select/hierarchy (`Stage 018B/C`), Directional Scale/snap/grid stages blocked by owner questions or spatial infrastructure, CAD booleans/Add-Cut/Revolve behind `GATE-KERNEL`, UV MVP, final export validation and release hardening.

Broad universal import, FBX, cloud sync, Apple/Windows clients and custom workspace are not active near-term work.

### 1.4 Current execution

- Current exact full repository HEAD: **unknown to coordinator after MIRROR-01**; short final commit from report is `3283089`, tree reported clean.
- Current task contract: `SCULPT-H1`.
- Launch state of the previously issued SCULPT-H1 prompt: **UNKNOWN**; no execution report has been supplied. The revised prompt v2 contains a safe concurrency/baseline guard and supersedes the prior prompt only when the repository is still clean at the MIRROR commit.
- Next feature after SCULPT-H1 by current sequencing: `Stage 025`.

---

## 2. ROADMAP — RZECZYWISTA KOLEJNOŚĆ

### 2.1 Status model

Three axes are separate:

- **Work status:** `PLANNED / READY / IN_PROGRESS / BLOCKED / IMPLEMENTED / CLOSED / DEFERRED`.
- **Technical evidence:** `PASS / FAIL / PARTIAL / NOT_RUN / BLOCKED_ENV / WAIVED` plus its scope/source.
- **Owner acceptance:** `ACCEPTED / PENDING / DEFERRED / RETEST_PENDING / NOT_REQUIRED / REJECTED`.

`IMPLEMENTED` means product code exists while a required closure remains. `CLOSED` means the currently applicable technical/repository closure conditions are met; OWNER acceptance may still be `DEFERRED` when policy explicitly allows an OWNER-LATER checkpoint. A deferred priority is not a technical blocker.

### 2.2 Active and future queue

| Order | Task ID — stałe | Wynik dla użytkownika / usuwane ryzyko | Work status | Realne zależności | Warunek ukończenia | Dlaczego teraz |
|---:|---|---|---|---|---|---|
| 1 | SCULPT-H1 | Compact Sculpt history navigator/jump; optional real-stylus hover preview | READY | Existing SCULPT-UNDO-R0 history; no unresolved product decision | Navigator/jump implemented without replacing history; focused tests; clean commit | Direct user value; unblocks planned brush expansion without a test-only detour |
| 2 | Stage 025 | Add Flatten, Crease and Mask to reach the seven-brush MVP set | PLANNED | No hard technical blocker; sequenced after SCULPT-H1 by current roadmap | Three brushes usable with existing SculptHistory and bounded evidence | Next visible Sculpt capability after H1 |
| 3 | STYLUS-G1 | Obtain real stylus evidence for pressure/hover/palm/cancel semantics | PLANNED | Real compatible stylus hardware/input | Recorded real-device evidence or explicit environment blocker | Required before pressure work; not simulated by finger |
| 4 | Stage 026 | Sculpt Stroke X Symmetry + pressure where supported | BLOCKED | STYLUS-G1 | Symmetry works; pressure behavior matches accepted stylus contract | Extends direct sculpt workflow after basic brush set |
| 5 | Stage 027 | Isolate/hide, mesh preview and remaining non-history Sculpt workflow | PLANNED | Current Sculpt object/visibility contracts | Feature subset complete with proportional checks | Completes practical Sculpt workflow before topology density work |
| 6 | PERF-BASELINE | Repeatable benchmark + thresholds, GPU memory and trim policy | PLANNED | Reference device/environment | Reproducible benchmark and thresholds | Needed before spatial/topology optimization claims |
| 7 | SPATIAL-R1 | BVH/octree + threading/cancellation model | BLOCKED | PERF-BASELINE | Measured improvement and safe cancellation/thread contract | Shared prerequisite for snapping, booleans, isolate and high-density work |
| 8 | GATE-PERF-SCULPT | Numeric latency/memory target for denser Sculpt | BLOCKED | SPATIAL-R1 + current reference device | Numeric target and safe ceiling recorded | Prevents Stage 028 from discovering memory limits after implementation |
| 9 | Stage 028 | Subdivide/multires-style density + voxel-style remesh | BLOCKED | SPATIAL-R1 + GATE-PERF-SCULPT | Density/remesh meet accepted memory/latency target | Major Sculpt density capability |
| 10 | GATE-KERNEL | Boolean-kernel evidence gate | PLANNED | Current CAD data/topology contracts | Kernel strategy passes robustness/lineage/license/platform criteria or OWNER reduces scope | Needed before booleans/Add-Cut, not before basic CAD |
| 11 | Stage 024 | Union/Subtract/Intersect + operation-tree boolean infrastructure | BLOCKED | GATE-KERNEL | Robust deterministic booleans with persistent topology lineage | Unlocks Add/Cut and advanced CAD |
| 12 | CAD-A4 | Extrude Add/Cut + Revolve | BLOCKED | Add/Cut: Stage 024; Revolve technically unblocked but kept in same planned task until explicitly split | Bounded CAD-A4 acceptance contract | Preserves existing ID rather than inventing a silent split |
| 13 | Stage 018B | Multi-select + group transform | BLOCKED | OQ-04 | OWNER defines group pivot/space; implementation then passes focused history/transform checks | Scene workflow expansion |
| 14 | Stage 018C | Hierarchy / drag-drop / reparent | DEFERRED | Current sequencing after Stage 018B; no immediate hard blocker claimed | Hierarchy truth + descendant transform semantics accepted | Useful later; not ahead of core modeling/sculpt work |
| 15 | Stage 020D | Directional Scale symmetric/one-sided | BLOCKED | OQ-01 | OWNER resolves anchor/planar/uniform/multi-select/stickiness semantics | Reuses Stage 020M solver once product semantics are fixed |
| 16 | Stage 020S1 | Contact / Body Magnet Snap | BLOCKED | SPATIAL-R1 | Contact semantics + transaction-safe snapping | Direct manipulation improvement after spatial foundation |
| 17 | Stage 020S2 | Alignment Guides + magnetic alignment | BLOCKED | SPATIAL-R1 + OQ-03 | OWNER resolves candidates/threshold/Rotate/priority; implementation green | Alignment workflow after contact infrastructure |
| 18 | GRID-SNAP-01 | Construction Grid Snap | BLOCKED | OQ-06 | Increment/unit contract accepted and implemented | Exact-placement convenience |
| 19 | SCULPT-DIM-01 | Overall Sculpt body dimensions | BLOCKED | OQ-02 | History ownership for body-level edit while Sculpt active resolved | Do not mislabel body transforms as brush history |
| 20 | Stages 029–032 | UV seams/charts/unwrap/pack/preview MVP | PLANNED | OQ-A must be answered before accepting dependency strategy | UV MVP completed under accepted library/in-house policy | Later MVP export quality |
| 21 | Stage 033 | Final GLB export scope | PLANNED | Required modeling/UV scope available | Final GLB contract met | Final interchange path |
| 22 | Stage 034 | OBJ export | PLANNED | Relevant project truth and export policy | OBJ contract met | Secondary interchange |
| 23 | Stage 035 | Engine/interchange validation | BLOCKED | Stage 033 + Stage 034 | Controlled external validation complete | Closes interoperability evidence |
| 24 | Stages 036–040 | Release/device/performance/long-operation/final regression hardening | PLANNED | Feature set substantially closed | Release gates satisfied | Milestone hardening, not ordinary feature iteration |
| 25 | IMPORT-01-SCALE | Broad/high-density import scaling and later format expansion | BLOCKED | Stage 040 + high-density readiness gate | Supported ceiling/evidence established | Avoids unsupported million-triangle claims |
| 26 | CLOUD-R0 | Account/cloud architecture research | DEFERRED | Stable local .forge dependency is already satisfied; product priority is deferred | OWNER-approved backend/sync/conflict contract | No longer falsely blocked by completed E2E-R1A |
| 27 | CLOUD-H1 | Cross-device project library/sync | BLOCKED | CLOUD-R0 | Local-first conflict-safe sync | Long-term product direction |
| 28 | PORT-R0 | Cross-platform architecture spike | DEFERRED | Trigger: first Apple/desktop client is actually scheduled | Backend/UI/packaging decision backed by then-current platform evidence | Do not prebuild hypothetical ports |
| 29 | UI-LAYOUT-CUSTOM | User-customizable workspace | DEFERRED | Trigger: OWNER prioritizes customization after stable default workspace | Customization semantics accepted and implemented | Not ahead of modeling capability |

### 2.3 Owner checkpoints and deliberately deferred gates

| Checkpoint | Status | Return condition |
|---|---|---|
| `IMPORT-01B` Delete → Undo → Redo | OWNER `DEFERRED` | Include in the next combined OWNER real-device pack; do not infer from automation. |
| `GATE-E2E` external export verdict | OWNER `DEFERRED` | Controlled import of current sentinel/export in the chosen external engine when OWNER schedules it. Internal roundtrip does not replace this. |
| `GATE-USER-01` | OWNER `DEFERRED` | Run after another useful feature batch (at minimum SCULPT-H1/Stage025) rather than interrupting each stage. |
| `BACKUP-01` | operational status `NOT_VERIFIED` by coordinator | Verify private remote/push when OWNER/repo access makes it relevant; not a coding-stage blocker. |
| `GATE-SCOPE` / `OQ-E` | inactive until owner chooses trigger date | Do not invent a calendar gate. |

### 2.4 Dependency validation

Current active dependency graph has no intentional cycle in the queue above. Completed dependencies are treated as satisfied rather than left as false blockers. In particular, stable local `.forge` is already delivered, therefore `CLOUD-R0` is **DEFERRED by product priority**, not `BLOCKED-BY:E2E-R1A`. `Stage 025` no longer claims “Undo missing”: Sculpt Undo exists; H1 precedes it by current sequencing/UX completion, not because Undo is absent.

---

## 3. KATALOG PRODUKTU I SYSTEMÓW

### 3.1 Construction / transforms

Canonical authored Construction/CAD lengths and Position are binary64 metres; Rotation is binary64 degrees under the repository's existing rotation convention; Absolute Scale is positive binary64 unitless. Render/tessellation geometry is derived. Stage 020M dimensions use source-local extents × absolute scale, not render vertices/world AABB. Resize/anchor solver is pure and reusable, but Directional Scale semantics remain blocked by `OQ-01`.

MIRROR-01 creates a new Construction SceneObject across world XY/XZ/YZ without negative persisted scale. For current symmetric primitive sources, the accepted transform is `p' = F·p`, `R' = F·R·Qx`, `S'=S`, with a proper-rotation result and direct geometry-equivalence proof. Imported/Cad/Sculpt mirror is not part of R1.

Related IDs: `Stage 016`, `Stage 017`, `Stage 019`, `Stage 020R2`, `Stage 020R3`, `Stage 020M`, `MIRROR-01`, `Stage 020D`, `Stage 020S1`, `Stage 020S2`, `GRID-SNAP-01`.

### 3.2 Objects / scene / hierarchy

`SceneObject` is the durable project object. Placement is independent from representation source. ObjectId is stable project identity. Delete removes the real SceneObject and is one ProjectHistory transaction; Undo restores the same ObjectId/truth/transform; automated checks do not substitute for the deferred OWNER Delete verdict.

Stage 018A behavior is present in the later clean baseline: Rename, Visibility, Lock and Duplicate. Visibility/lock are durable SceneObject state. Hidden objects are not rendered/pickable; locked objects cannot be transformed; Duplicate uses a fresh ObjectId and does not clone renderer/history runtime caches. Multi-select/group transform (`Stage 018B`) and hierarchy/reparent (`Stage 018C`) remain later.

Related decisions: UI-OWNER-12, UI-OWNER-40, UI-OWNER-45.

### 3.3 CAD / Sketch

Construction is a semantic feature graph. Persistent face/edge/vertex references use stable semantic topology identity, never renderer triangle indices. Current CAD supports first flat Sketch, world/planar-face support, semantic TopoRef persistence, repeated Sketch→Extrude New Body, Line/Polyline/Rectangle/Circle, Arc/Spline authored truth, line length editing and staged Edit Sketch.

Booleans and Add/Cut wait for `GATE-KERNEL`. Revolve is part of CAD-A4 but is not technically dependent on booleans; it remains grouped under the existing task ID until an explicit split is authorized.

Related: ARCH-OWNER-13, ARCH-OWNER-14, UI-OWNER-04, UI-OWNER-13, UI-OWNER-46/47/48.

### 3.4 Sculpt

Current Sculpt mesh positions/topology are non-regenerable project truth; normals/adjacency/render caches are derived. ProjectHistory never stores Sculpt vertex state.

`SCULPT-UNDO-R0 / ARCH-OWNER-12` is authoritative: separate runtime delta history, bounded by the current implementation (reported 32 entries, 4 MiB/body, 1 MiB/entry), one completed stroke ≤1 entry, redo invalidation after a new stroke, Back→Resume retention, no `.forge` serialization, reopen keeps the current mesh but starts with empty stacks.

`SCULPT-H1` is **UX only**: compact scrollable navigator over existing retained history, up to five rows visible at once, jump-to-cursor through existing deltas, and non-destructive real-stylus hover preview if the current renderer/session contract can support it safely. The older UI-OWNER-41 “five actions” was the initial experiment and no longer overrides the implemented history capacity.

`Stage 025` adds Flatten/Crease/Mask to the target seven-brush set: Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask.

### 3.5 Persistence / recovery

`.forge` is ForgeShape's canonical self-contained native project document. It is explicit-endian/versioned/chunked and never contains Android paths, pointers, ABI-sized native structs, renderer/GPU caches or session Undo stacks. Construction stores semantic source/feature truth; Imported/Sculpt non-regenerable geometry stores required mesh truth. Decode validates into temporary state and atomically applies on success; failure mutates nothing.

Save/Open, process-death reopen, autosave/recovery and Scoped Storage transfer are delivered. There is no active “document model undecided” risk for Construction/CAD numeric truth: binary64 authored Construction/CAD truth is accepted. Remaining open questions are feature-specific, not the base persistence model.

### 3.6 Import / export

`.forge` and interchange are separate systems. Static GLB export uses metres 1:1, right-handed +Y-up; Rotation+Scale are baked into static mesh geometry, Translation remains at node level, and normals use inverse-transpose under non-uniform scale. Internal roundtrip is green; owner real-file static GLB import is green for the bounded supported subset.

Evidence boundary is explicit:
- internal exporter/parser equivalence: PASS;
- bounded ForgeShape device import/export paths: PASS in reported scope;
- owner real low-poly GLB import: ACCEPTED;
- external engine/export checkpoint `GATE-E2E`: still OWNER DEFERRED;
- universal GLB/OBJ/FBX/import interoperability: **not claimed**.

Broad/high-density import remains behind `IMPORT-01-SCALE` readiness, not the bounded existing Imported Mesh capability.

### 3.7 UI / preferences / selection

The workspace is viewport-first, mobile-first, with the accepted edge-hugging contextual rail. Settings owns persistent app preferences (five palettes, handedness, gizmo visual size/thickness); View/Overlay owns transient viewport toggles such as Grid and Selection Outline. App preferences never become `.forge` or ProjectHistory truth.

Selection Outline is a true renderer-derived mask/composite effect, not an Android bounding-box fake. It has technical evidence but no final FullSharded claim; its visual weight/toggle ergonomics remain OWNER LATER.

No broad UI redesign is authorized outside an explicit stage.

### 3.8 Renderer / input / lifetime

C++ domain/render data remains platform-neutral; Android View/Activity/JNI types are adapters. Renderer resources are derived and can rebuild after surface/device loss. JNI is event-driven/adaptor code, not domain authority. Current pointer transport supports semantic stylus identity, but real stylus evidence remains necessary before pressure work.

Known performance foundations still outstanding: coarse whole-mesh Sculpt publication (`F-06`), per-frame CAD face re-extraction (`F-07`), hostile CAD triangulation complexity (`F-09`), activeBody self-heal (`F-10`), representation branch debt (`F-13`), large JNI translation unit (`F-14`). Do not reopen closed F-08/F-11/F-15/F-16/F-18 without a new signal.

### 3.9 Cloud and portability

Local-first portable projects are the architectural foundation. Cloud sync is future infrastructure around portable project truth, not geometry truth. Stable local `.forge` is already delivered, so cloud research is not technically blocked by persistence; it is simply deferred by product priority.

The C++ core must remain free of unnecessary Android UI/filesystem/service dependencies. Future Apple/Windows renderer/UI choices belong to `PORT-R0` when a real port is scheduled; do not preselect Metal/MoltenVK/shared UI framework now.

### 3.10 Approved MVP scope and non-goals

Approved MVP retains exact Construction transforms, scene/object management, Sketch/Extrude/Revolve/booleans when gated, seven Sculpt brushes, Sculpt Undo/history UX, symmetry/pressure, subdivide/remesh, UV MVP, GLB/OBJ export, portable `.forge`, autosave/recovery and the accepted mobile workspace. The existing proposed release tiering remains `PROPOSED` under `OQ-B`, not an OWNER-approved scope reduction.

Explicit non-goals unless OWNER changes them: assemblies, general CAD constraint solver beyond bounded Sketch, NURBS, drafting, physics, ray tracing, full texture painting, rigging/animation, Blender-class retopology, marketplace/social/live collaboration, AI model generation, plugin ecosystem and universal-format support in one milestone.

---

## 4. ARCHITEKTURA I OGRANICZENIA

### 4.1 Layer boundaries

- Platform-neutral C++ owns geometry, Construction/CAD, Sculpt state/history, persistence semantics and transform math.
- JNI is an Android adapter; Java/UI does not become domain truth.
- Android surface/context/storage/input adapters may be replaced for future platforms without changing project truth.
- Renderer meshes/resources are derived from domain truth.

### 4.2 Durable representation model

SceneObject owns stable ObjectId, name, transform and durable object flags. Representation source is explicit/typed (Construction, Imported Mesh, Cad, with current/retained Sculpt state where allowed). Avoid representation booleans and duplicated per-representation command implementations.

Construction/CAD source/feature graph is regenerable semantic truth. Imported Mesh and Sculpt non-regenerable mesh data are project truth. Renderer triangle indices are never persistent CAD face identity.

### 4.3 Numeric and topology contracts

- Construction/CAD authored length + Position: binary64 metres.
- Rotation: binary64 degrees under existing convention; noncanonical authored values may be preserved where current contract says so.
- Absolute Scale: positive binary64 unitless.
- Sculpt local positions: float32 project truth.
- Stable topology: `TopoRef { producerFeature=(ObjectId, LocalFeatureId), entityKind, semanticToken, lineageToken }`; unresolved/ambiguous refs fail closed; no nearest-face retargeting.

### 4.4 History ownership

ProjectHistory owns scene/Construction/object transactions. It is runtime/session history and is not serialized. CAD feature graph is document truth, not the same thing as transaction history.

SculptHistory separately owns touched-vertex stroke deltas. No Sculpt vertex state enters ProjectHistory. No SculptHistory stack enters `.forge`, autosave, recovery or Save Copy.

### 4.5 File and interchange safety

`.forge` is self-contained native project truth. Standard GLB/OBJ/FBX are interchange and may be lossy. Any new decoder must validate lengths/counts/CRC/semantic bounds before applying live state. Failed decode/import must be non-mutating. Imported polygon geometry never receives fake Construction source semantics.

### 4.6 Dependency policy

A new substantial dependency requires a concrete need plus license, maintenance/security, Android ABI/build, APK/AAB size, native/16 KB behavior, performance/memory, format/data implications, CPU architectures and future portability review. Small local helpers do not trigger multi-day research. Kernel/UV/import dependencies remain explicit gates; do not silently add one inside unrelated feature work.

---

## 5. DECYZJE I POMYSŁY

### 5.1 Open decisions

| ID | Current unresolved question | Needed before |
|---|---|---|
| `OQ-A` | Does the no-third-party-dependency preference also govern boolean kernel and UV unwrap, or may a qualifying dependency be adopted? | `Stage 024` acceptance / UV dependency choice. |
| `OQ-B` | Keep the full approved MVP as v1 horizon or adopt proposed release tiering? | Scope/milestone decisions, not nearest SCULPT-H1. |
| `OQ-C` | One falsifiable target-user/value thesis: CAD-first Android gap vs another thesis. | `GATE-VIABILITY` / major long-range priority changes. |
| `OQ-01` | Directional Scale anchor/contact/planar/uniform/multi-select/stickiness semantics. | `Stage 020D`. |
| `OQ-02` | History ownership for body-level dimension/transform edit while Sculpt is active. | `SCULPT-DIM-01` — **not Stage 020M**. |
| `OQ-03` | Alignment candidate set, threshold space, Rotate meaning, Contact-vs-Alignment priority. | `Stage 020S2`. |
| `OQ-04` | Group pivot and World/Local orientation semantics. | `Stage 018B`. |
| `OQ-06` | Grid Snap increment/unit contract. | `GRID-SNAP-01`. |
| `OQ-E` | Calendar trigger for `GATE-SCOPE`. | Gate remains inert until OWNER answers. |
| `OQ-07` | Confirm live implemented Plane orientation/normal/extent semantics are intended. | Hygiene only; next task not blocked. |

Resolved historical questions are not kept active: early GLB export (`OQ-D`) is implemented; handedness (`OQ-05`) is implemented through UI-PREF.

### 5.2 Idea backlog

Record ideas immediately as `PROPOSED`; recording is not implementation authorization.

| IDEA ID | Full sense | Status | Source / reason | Destination / return condition |
|---|---|---|---|---|
| `IDEA-01` | Hierarchical decimal numbering for document sections, not work/decision identifiers. | `IMPLEMENTED` | OWNER idea from 2026-08-28; adopted only for headings. | Current document structure. |

New OWNER ideas must get an IDEA ID the same turn they are raised, with variants/source retained. Move to `IN_PLAN` only after the relevant product direction is accepted.

### 5.3 Decision index and retention

The formal decision source available during this migration was `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md` (72 decision headings). It is not reissued as a fifth user-facing file. Current active contracts are summarized in this MASTER; the table below preserves every available decision ID/topic/status plus the later TEST-OWNER-03 decision recorded in current coordinator truth. If a future task depends on nuance not expanded in this MASTER and the archived full body is unavailable, mark `BRAK ŹRÓDŁA` and ask/restore the source rather than reconstructing it from the title.

| Decision ID | Topic | Source status | Implementation/source note |
|---|---|---|---|
| `UI-OWNER-01` | Main editor interface / Editor Workspace UI | APPROVED | IMPLEMENTED(UI-R4C, accepted 2026-08-26) |
| `UI-OWNER-02` | Device scope | APPROVED | IMPLEMENTED(UI-R4B) |
| `UI-OWNER-03` | Export placement | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-04` | Sketch + Extrude MVP boundary | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-05` | destructive re-Freeze confirmation — LEGACY CURRENT IMPLEMENTATION | LEGACY-ACTIVE | IMPLEMENTED(legacy) |
| `UI-OWNER-06` | stylus pressure in Stage 015B | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-07` | visual language | APPROVED | PARTIAL(UI-R4C) |
| `UI-OWNER-08` | new-project mode choice | APPROVED | PARTIAL(UI-R2) |
| `UI-OWNER-09` | theme foundation — SUPERSEDED IN APPEARANCE COUNT BY UI-OWNER-22 | SUPERSEDED-BY:UI-OWNER-42 | IMPLEMENTED(UI-R3R2) |
| `UI-OWNER-10` | selection feedback | APPROVED | PARTIAL(UI-R2) |
| `UI-OWNER-11` | viewport visibility controls | APPROVED | PARTIAL(UI-R2) |
| `UI-OWNER-12` | scene hierarchy direction | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-13` | exact CAD sketch/feature direction | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-14` | premium interaction quality target | APPROVED | ONGOING |
| `UI-OWNER-15` | selective layered translucency / glass | APPROVED | IMPLEMENTED(UI-R4B) |
| `UI-OWNER-16` | unified motion language | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-17` | adaptive workspace quality | APPROVED | PARTIAL(UI-R4B) |
| `UI-OWNER-18` | UI performance and completion standard | APPROVED | ONGOING |
| `UI-OWNER-19` | screenshot-based visual acceptance | APPROVED | ONGOING |
| `UI-OWNER-20` | viewport-first floating composition | APPROVED | PARTIAL(UI-R4C) |
| `UI-OWNER-21` | border hierarchy reduction | APPROVED | PARTIAL(UI-R4C) |
| `UI-OWNER-22` | three approved dark palettes — COUNT SUPERSEDED BY UI-OWNER-42 | APPROVED | IMPLEMENTED(UI-R4B) |
| `UI-OWNER-23` | mobile direct-manipulation workspace | APPROVED | IMPLEMENTED(UI-R4C) |
| `UI-OWNER-24` | one-way Construction project → Sculpt project copy | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-25` | object creation from Objects `+` | APPROVED | IMPLEMENTED(UI-R4A) |
| `UI-OWNER-26` | topology vocabulary | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-27` | retire user-facing `Freeze` terminology | APPROVED | PARTIAL(UI-R4B) |
| `UI-OWNER-28` | minimum interactive touch target | APPROVED | IMPLEMENTED(UI-R4B) |
| `UI-OWNER-29` | current reversible Construction→Sculpt label | APPROVED | IMPLEMENTED(UI-R4B) |
| `UI-OWNER-30` | full transform gizmo reference and coordinate spaces | APPROVED | IMPLEMENTED(Stage 020R2, accepted 2026-08-29) |
| `UI-OWNER-31` | Surface Snap follows full transform gizmo | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-32` | gizmo appearance preferences | APPROVED-FUTURE | PARTIAL_OWNER_ACCEPTED(UI-PREF-R1: size+thickness; handle-style deferred by renderer contract) |
| `UI-OWNER-33` | Symmetric vs One-sided Scale | OPEN-QUESTIONS | NOT_IMPLEMENTED |
| `UI-OWNER-33A` | Absolute dimensions and Relative Scale multiplier | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-33B` | Body Dimensions mode, separate from dense gizmo manipulation | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-33C` | Dimensions in Sculpt | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-34` | Contact / Body Magnet Snap | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-35` | Alignment Guides + Magnetic Alignment | OPEN-QUESTIONS | NOT_IMPLEMENTED |
| `UI-OWNER-36` | stable default workspace + future Custom Layout | APPROVED | PARTIAL(UI-LAYOUT-R1) |
| `UI-OWNER-37` | Settings Hub ownership | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(UI-PREF-R1) |
| `UI-OWNER-38` | ForgeShape App/Project menu | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-39` | Home / no-project start flow | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-40` | Objects list commands, multi-selection and hierarchy | APPROVED | NOT_IMPLEMENTED |
| `UI-OWNER-41` | Sculpt Undo/Redo + history navigator + stylus hover preview | APPROVED-FUTURE | NOT_IMPLEMENTED |
| `UI-OWNER-42` | appearance target expanded to five palettes | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(UI-PREF-R1) |
| `UI-OWNER-43` | static layout stability before motion/morph polish | APPROVED | ONGOING |
| `UI-OWNER-44` | unified expandable right contextual rail | APPROVED | PARTIAL(UI-LAYOUT-R2) |
| `UI-OWNER-45` | delete a real project object from the Objects list | APPROVED | IMPLEMENTED_TECHNICAL(`04f28df49f5000e863628571cdf5f8f076dac7e0`; OWNER retest pending) |
| `UI-OWNER-46` | mobile Home flow and direct spatial sketch-support selection | APPROVED | IMPLEMENTED_TECHNICAL(`c5864c69902be127a12daa5ec81e86b0d07627bb`; OWNER real-device retest pending) |
| `UI-OWNER-47` | Home is a full-screen start page | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(`5cf981a9c89da8e99366592cb77b0f1cdff1c361`) |
| `UI-OWNER-48` | immediate flat Sketch + orientation navigator + on-canvas line length | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(`5cf981a9c89da8e99366592cb77b0f1cdff1c361`) |
| `ARCH-OWNER-01` | future Apple portability | APPROVED | ONGOING |
| `ARCH-OWNER-02` | same ForgeShape product across Android, Apple and desktop | APPROVED | ONGOING |
| `ARCH-OWNER-03` | renderer portability seam | APPROVED | ONGOING |
| `ARCH-OWNER-04` | local-first portable projects, account and cloud sync | APPROVED | NOT_IMPLEMENTED |
| `ARCH-OWNER-05` | simple hierarchical Editor Workspace source structure | APPROVED | IMPLEMENTED(UI-ARCH-R1, accepted 2026-08-28) |
| `ARCH-OWNER-06` | native `.forge` project vs standard interchange formats | APPROVED | PARTIAL(E2E-R1A) |
| `ARCH-OWNER-07` | default static interchange export bakes Rotation + Scale | APPROVED | IMPLEMENTED(E2E-R1C-C1) |
| `ARCH-OWNER-08` | bounded early GLB diagnostic import exception | APPROVED | IMPLEMENTED(GLB-IMPORT-R0) |
| `ARCH-OWNER-09` | external static GLB diagnostic preview compatibility | APPROVED | IMPLEMENTED(GLB-IMPORT-R1) |
| `ARCH-OWNER-10` | durable Imported Mesh object slice before broad import hardening | APPROVED | IMPLEMENTED(IMPORT-01A, accepted 2026-09-01) |
| `ARCH-OWNER-11` | Imported Mesh as source for current reversible Sculpt | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(`04f28df49f5000e863628571cdf5f8f076dac7e0`) |
| `ARCH-OWNER-12` | dedicated bounded Sculpt Undo/Redo history | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(`2bcfa3fa11fd96fa934887f16e55caefbee496d9`) |
| `ARCH-OWNER-13` | semantic planar-face references for associative Sketch support | APPROVED | IMPLEMENTED_TECHNICAL(`c5864c69902be127a12daa5ec81e86b0d07627bb`; OWNER real-device retest pending) |
| `ARCH-OWNER-14` | Arc/Spline authored truth and staged Edit Sketch | APPROVED | IMPLEMENTED_OWNER_ACCEPTED(`5cf981a9c89da8e99366592cb77b0f1cdff1c361`) |
| `INPUT-OWNER-01` | stylus-first interaction | APPROVED | PARTIAL(INPUT-R1) |
| `DOC-OWNER-01` | clear naming and explanatory code comments | APPROVED | ONGOING |
| `DOC-OWNER-02` | selective reading of large repository documents | APPROVED | ONGOING |
| `TEST-OWNER-02` | bounded regression runtime and stage-specific aggregate waiver | APPROVED | IMPLEMENTED(TEST-RUNTIME-R1 @ `538623c26e0122d97303150ebbb52eb3ea0d32b3`) |
| `TEST-OWNER-01` | authoritative exhaustive-sharded instrumented full suite | APPROVED | IMPLEMENTED(TEST-HARNESS-R1, accepted 2026-08-28) |
| `AUDIT-OWNER-02` | holistic application/code/docs audit and source-comment compression | APPROVED | COMPLETE_ACCEPTED(`af6a719…`; PASS-DEEP-AUDIT-R1-WITH-DEBT) |
| `AUDIT-OWNER-01` | post-Stage-020R2 UI coherence audit | APPROVED | IMPLEMENTED(UI-AUDIT2A, UI-AUDIT2B, UI-AUDIT2C) |
| `TEST-OWNER-03` | short ordinary-feature tests + OWNER LATER UX packs | APPROVED | ACTIVE in coordinator policy since v4.0.55 |

---

## 6. POLITYKI I WYMAGANIA PLATFORM

### 6.1 Orchestrator operating instructions — canonical

**READ.** Before every new coding prompt, read the whole current MASTER, whole PROJECT STATE, the issued task contract/new report and repo-local instructions relevant to the task. Read HISTORY thematically unless a conflict requires wider chronology. If context compaction makes version certainty unclear, reread the documents.

**RECONCILE.** For a report, compare every issued acceptance criterion with the agent claim and evidence. Separate agent statement from evidence. A missing required test is `NOT_RUN/PARTIAL`; an emulator/system-server/harness failure is not automatically a product bug. A repeated report does not create a second event.

**BUG → FIX.** Before authorizing a correction, record three things: (1) currently binding contract, (2) actual repro/test evidence, (3) why observed behavior differs. The v4.0.35–38 Sculpt Undo sequence is the canonical lesson: an intentionally absent feature was first misclassified as a regression, the live contract corrected that mistake, and OWNER later authorized Sculpt Undo as a new separate product decision.

**UPDATE → VALIDATE.** Update the canonical owner of the fact first, then all other active places that still state the old contract. Do not append a “new truth” while leaving contradictory active text above it. Re-read resulting MASTER and STATE. Mechanically validate unique active Order values, IDs, dependency cycles, baseline/revision consistency and retention mapping when tools are available.

**PLAN.** Choose the next task by user value, accepted milestone, available dependencies, root-cause family cost and deliberate deferrals. Do not schedule maintenance/audits merely because a minor detail was noticed. A correct audit may end with no code change.

**WIP.** Work status, technical evidence and OWNER acceptance are separate. Default maximum is one `IN_PROGRESS` task plus one additional `READY/IMPLEMENTED` item. OWNER may explicitly roll forward with deferred acceptance. There is no automatic 48-hour expiration that forces fake closure.

**Ideas.** Record a new idea immediately as `PROPOSED`; implementation still requires authorization/priority. Do not require a second confirmation when OWNER has already unambiguously authorized the same non-conflicting work.

**Anti-loop.** Tag work `FEATURE / BLOCKER / BUG / POLISH / MAINTENANCE / SPIKE` and keep a root-cause family. After two failed corrections for the same root cause, do not issue a third equivalent prompt: reassess assumptions/repro/source/environment or ask about priority. A no-op/preflight/closeout in the same family still counts toward the method review. The limit never permits accepting data loss/security/release-gate failure.

**Prompt handoff.** Each Claude Code prompt must state project/repo/task/prompt/docs revision, safe baseline verification, why now, repo instructions to read, binding contract, scope/exclusions once, one behavior→evidence acceptance map, proportional verification, document owners to update and a concise report. Claude Code returns to coordinator; it never autonomously starts the next stage.

### 6.2 Canonical test policy — TEST-POLICY v3

The three owner rules are reconciled by scope rather than choosing one arbitrarily:

- `TEST-OWNER-01` defines **what qualifies as an authoritative exhaustive FullSharded PASS**. It does not make FullSharded mandatory for every stage.
- `TEST-OWNER-02` bounds any required aggregate: target 90 min, hard stop 120 min, max two automatic fresh aggregate attempts, focused failing method/class/shard first, infrastructure classified separately, same-tree resume allowed only under the accepted runner fingerprint.
- `TEST-OWNER-03` selects the default gate for ordinary feature work: target ≤20 min, hard stop 30 min, no default FullSharded; automate high-risk correctness and defer broad visual/ergonomic matrices to OWNER LATER.

**Same-tree resume fingerprint implemented by TEST-RUNTIME-R1:** app APK SHA-256, test APK SHA-256, discovered inventory hash, deterministic partition hash, shard count, instrumentation target, device serial and guarded AVD/model identity. A product/test APK change invalidates previous shard PASS reuse. Timebox stops/blocks evidence; it never converts unrun tests into PASS.

**Ordinary UI/product stage:** focused domain/native/JVM tests as needed + one representative device smoke + minimum build sanity; target 20 / hard 30 minutes.

**High-risk persistence/concurrency/destructive/security/migration stage:** stronger targeted data-safety tests are mandatory and may justify a larger gate. State the reason.

**FullSharded:** reserve for release/milestone, high-risk cross-cutting runtime/persistence/concurrency changes or explicit OWNER/coordinator request. If used, only one tested fingerprint can contribute to `FULL_SHARDED_SUITE_PASS`.

**OWNER LATER:** visual quality, interaction feel, discoverability, all-theme/all-representation matrices and multi-feature workflows should usually be grouped into one later real-device pack instead of duplicated exhaustively by synthetic tests.

### 6.3 Device policy

- Primary guarded development/test AVD from recent accepted reports: `emulator-5580` / `ForgeShape_Stage006`.
- `emulator-5554` remains reserved and must not be contacted unless a later explicit decision changes this.
- Foreign attached emulators are not implicitly authorized.
- Real stylus claims require real compatible stylus input; finger simulation is not stylus evidence.

### 6.4 Platform readiness register

This register separates reported historical evidence from current external requirements. It does not claim a new repo build/test was performed by the coordinator.

| Requirement ID | Platform / SDK | Official source | Checked | Scope / deadline | Applies and why | Action / task | Evidence for current ForgeShape | Next review |
|---|---|---|---|---|---|---|---|---|
| `PLAT-ANDROID-16K` | Android native / 16 KB pages | https://developer.android.com/guide/practices/page-sizes | 2026-09-07 | Apps targeting API 35+ on Play must support 16 KB on 64-bit; from 2027-02-01 Play blocks updates without support. | APPLIES: ForgeShape ships C++/NDK `.so`. | Revalidate when toolchain/native dependency/ABI/packaging/final release artifact changes. | Gate P0: ELF/APK static/package proof; Gate P1: runtime `PAGE_SIZE=16384`; NDK r29 reported. `VERIFIED` for those historical artifacts only. | On next native dependency/toolchain change or before release. |
| `PLAT-ANDROID-TARGET` | Google Play target API | https://support.google.com/googleplay/android-developer/answer/11926878?hl=en | 2026-09-07 | Since 2026-08-31 new apps/updates must target Android 16 / API 36; existing-app availability has its own API-35 threshold/extension rules. | APPLIES if distributed through Google Play. | Verify live repo `targetSdk` before Play submission; do not confuse with minSdk/compileSdk/emulator API. | `NOT_VERIFIED` by coordinator; exact repo build values are intentionally repo-owned. | Before next Play submission; otherwise monthly watch. |
| `PLAT-APPLE-SDK` | iOS/iPadOS/macOS future port | https://developer.apple.com/news/upcoming-requirements/ | 2026-09-07 | Since 2026-04-28 App Store Connect uploads require Xcode 26+ with platform 26 SDKs. | WATCH: no Apple client is implemented. | Recheck at `PORT-R0` and before first Apple submission; do not create an Xcode/Metal project now. | `NOT_APPLICABLE` to current Android artifact; portability seams only. | Quarterly until port scheduled; then each release. |
| `PLAT-WINDOWS-STORE` | Windows future port | https://learn.microsoft.com/en-us/windows/apps/publish/store-policies | 2026-09-07 | Microsoft Store policies current source observed as v7.19 (effective 2025-10-14). | WATCH: no Windows client is implemented. | Recheck at `PORT-R0` and before Store distribution. | `NOT_APPLICABLE` to current Android artifact. | Quarterly until port scheduled. |

### 6.5 Risk/debt/watch register

| ID / item | Current status | Next trigger |
|---|---|---|
| F-06 whole Sculpt mesh publish per move | OPEN P2 architectural ceiling | PERF-BASELINE / Stage 028. |
| F-07 per-frame CAD face re-extraction | OPEN P2 | Measure before CAD complexity increase. |
| F-09 hostile CAD triangulation complexity | OPEN P2 | Untrusted/high-complexity CAD input stage. |
| F-10 activeBody silent self-heal | OPEN P3 | When selection/domain invariants are next touched. |
| F-13 representation dispatch branches | OPEN P3 | Next representation expansion/refactor with evidence. |
| F-14 large `jni.cpp` | OPEN P3 | Only if it blocks safe changes/review. |
| F-16 release self-tests linked | **CLOSED** by POST-AUDIT-HARDEN at `5cf981...`; later Stage 020M/MIRROR reports also found 0 release selftest symbols. | Reopen only on build/linkage/toolchain regression signal. |
| Numeric precision “undecided” | **CLOSED as stale debt wording**: binary64 Construction/CAD authored truth + float32 Sculpt mesh are binding. | Feature-specific precision questions remain separate. |
| SpatialSketchTest isolation | OPEN test debt | Next milestone/full aggregate or when that suite is touched; do not interrupt ordinary feature work. |
| External export engine verdict | OWNER DEFERRED | `GATE-E2E`. |
| Radius/Strength visual polish | OWNER LATER / P2 | Future Sculpt UI-linked stage, not a standalone reopen. |

---

## 7. INDEKS UKOŃCZONYCH ZADAŃ

Closed/implemented work leaves the future queue and is indexed here. History remains the evidence archive; this table is not a second report log.

| Task / milestone | User result / risk removed | Technical evidence / owner state | Evidence home |
|---|---|---|---|
| Gate P0 | NDK r29 + Git baseline + 16 KB static/package proof | PASS | History / Gate P0 |
| Gate P1 | Physical ARM64 + real PAGE_SIZE=16384 + Vulkan validation + mesh-density ladder | PASS; stylus sub-gate NOT_VERIFIED | History / Gate P1 |
| Stage 016 | Primitive family + exact Plane | PASS | History |
| Stage 017 | Multi-object SceneObject/ObjectId foundation | PASS | History |
| Stage 019 | Construction transaction/history foundation | PASS | History |
| Stage 020R2 | Full Construction transform gizmo + exact transform | PASS / OWNER accepted UI baseline | History |
| Stage 020R3 | World/display Sculpt brush metric fix | PASS | History |
| UI-SPEC-R0 | Measured static layout specification | OWNER ACCEPTED | History |
| UI-LAYOUT-R2 | Accepted edge-hugging workspace layout | PASS / OWNER accepted | History |
| CAD-R0-DATA | Semantic project/document/topology/.forge contract | CLOSED | History |
| E2E-R1A / Stage 021 | .forge v1 save/load + corpus | PASS | History |
| E2E-R1B / Stage 022 | Autosave/recovery/SAF/lifecycle diagnostics | PASS | History |
| E2E-R1C / Stage 023 | Early static GLB export | PASS | History |
| GLB-IMPORT-R0 | Diagnostic roundtrip preview | PASS | History |
| GLB-IMPORT-R1 | External static GLB preview subset | PASS / OWNER real-file accepted | History |
| IMPORT-01A | Durable Imported Mesh object + .forge | PASS / OWNER real-file accepted | History |
| IMPORT-01B | Imported Mesh Sculpt + Delete implementation | TECHNICAL PASS; OWNER Delete check DEFERRED | History |
| SCULPT-UNDO-R0 | Separate bounded Sculpt Undo/Redo delta history | PASS / OWNER accepted | History |
| CAD-A1/A2 | Sketch foundation + Extrude New Body | PASS | History |
| CAD-A3/C1/C2 + APP-H1 | Home, immediate Sketch, face support, Arc/Spline, Edit Sketch | PASS / CAD-A3-C2 OWNER accepted; some old rows had stale pending text | History |
| DEEP-AUDIT-R1 | Holistic audit | PASS WITH DEBT | History |
| POST-AUDIT-HARDEN-R1 | F-08/F-16/F-11/F-15/F-18 hardening | PASS | History |
| UI-PREF-R1 | Settings, handedness, five palettes, gizmo size/thickness | OWNER accepted with explicit aggregate waiver | History |
| TEST-RUNTIME-R1 | Shard-only/resume/fingerprint/budgets/failure classification | PASS | History |
| SEL-OUT-R1 | Renderer-derived persistent selection outline | Technical feature PASS; OWNER visual review deferred; no final FullSharded claim | History |
| Stage 018A | Rename/visibility/lock/duplicate | Present in later clean baseline; direct report not ingested in this chat | History provenance gap retained |
| Stage 020M | Construction Body Dimensions + Relative Scale | PASS / OWNER LATER | History @ 94daaa92494988e6604d8dcf2db6f8a535a9a9f3 |
| MIRROR-01 | Construction mirror XY/XZ/YZ world planes | PASS / OWNER LATER; final full hash omitted | History; short commit 3283089 |

---

## 8. NAWIGACJA HISTORYCZNA I MIGRACJA v4.0.58 → v4.1.0

### 8.1 Old-section → current-owner map

| Old v4.0.58 area | v4.1.0 destination | Migration operation | Semantic retention |
|---|---|---|---|
| §0 HOW TO READ | §6.1 + §8 | MERGE_WITH_ALIAS | Reading/integrity rules retained; magic-number checks removed. |
| §1 CURRENT EXECUTION | §1 + PROJECT STATE + §2 | MOVE | Current facts separated from roadmap/status axes. |
| §2 COORDINATOR | §6.1 | MERGE | Idea, prompt, report, WIP and anti-loop rules reconciled with owner overrides. |
| §3 PRODUCT | §1 + §3.10 | MOVE/MERGE | Approved MVP and non-goals retained; stale “no app” thesis text no longer used as current capability status. |
| §4 UX/UI | §3.7 + §6 + OWNER LATER | MOVE/MERGE | Accepted layout/UI contracts retained; historical load-variant diagnosis remains in HISTORY. |
| §5 ARCHITECTURE | §4 + §3 systems | MOVE | Domain/persistence/topology/portability contracts retained and stale risk labels corrected. |
| §6 QUALITY | §6.2–6.4 | MERGE | TEST-OWNER-01/02/03 scopes reconciled into one canonical policy. |
| §7 ROADMAP | §2 + §3 + §7 | SPLIT | Real unfinished order separated from scope catalogue and completed index. |
| §8 RISKS/OQ/IDEAS | §5 + §6.5 | MOVE | Open questions corrected; closed risks removed from active debt; ideas retained. |
| §9 PROCESS | §6.1–6.2 | MERGE/SUPERSEDE_WITH_LINK | 48h forced closure and “accept after 2 rounds” incentives removed; root-cause method review retained. |
| §10 APPENDICES | §8 + HISTORY | MOVE | Legacy aliases and migration evidence retained without magic counters. |

### 8.2 Audit card register — PROMPT_D_FORGESHAPE v2.0

| Card | Result after reconciliation | Changed/current location | Basis / remaining issue |
|---|---|---|---|
| D0 | DONE WITH SOURCE LIMIT | §§1–8 | Used current 4-file set + available OWNER_DECISIONS. No separate fifth/sixth registry due OWNER 4-file rule. |
| D1 | FIXED | §6.2 | One TEST POLICY: 01 defines PASS semantics, 02 aggregate runtime/resume, 03 ordinary-stage gate selection. |
| D2 | FIXED | §1.2–1.3 | Current capabilities explicitly include Save/Load, autosave, GLB, CAD/Sketch, Sculpt Undo, Home/Outline. Old empty-box diagnosis is historical only. |
| D3 | FIXED/PARTIAL EXTERNAL | §§3.5–3.6, 6.5 | binary64 contract current; F-16 closed; `.forge`/GLB evidence split by internal/device/OWNER/external-engine scope. External engine verdict still deferred. |
| D4 | FIXED | §2 | Unique unfinished Order 1..N; completed removed from future queue; CLOUD false blocker removed; Stage025 false “Undo missing” dependency removed. |
| D5 | FIXED | §§3.4, 5.1 | Current SculptHistory capacity retained; H1 reduced to navigator UX; OQ-02 points to SCULPT-DIM-01. |
| D6 | FIXED | §6.1 + HISTORY | BUG→FIX now requires binding contract, actual repro/test and discrepancy reason. Historical false-regression sequence preserved. |
| D7 | FIXED | §8.3–8.4 | Integrity compares actual ID sets/revisions and semantic retention; no magic 57/67/72 count as proof. |
| D8 | FIXED | §§2.1, 6.1 | Work/evidence/OWNER axes separate; no 48h forced closure; ideas recorded PROPOSED immediately without auto-implementation. |
| D9 | FIXED | §6.1 | Audits may correctly produce no code; correction limit triggers method review, not forced acceptance; no-op/closeout counts in root-cause family. |
| D10 | OBSOLETE AS TASK / LESSON RETAINED | §§1.2, 3.1, 7 | MIRROR-01 already completed after audited package. Its geometry/history/persistence contract remains recorded; it is not rewritten/re-run. |
| D11 | FIXED | §6.4 | PLATFORM READINESS register records historical NDK/16K evidence and current official requirements without claiming new product tests. |
| D12 | APPLIED | §§1.2, 2, 7 | Mirror capability recorded once; no automatic aggregate/closeout opened; next task selected from current roadmap. |

### 8.3 Task-ID retention inventory

Every task-like ID found in the prior roadmap/catalog/legacy area remains addressable below. A row here is a retention/navigation guarantee, not a second schedule.

| Prior ID | Operation | Current target |
|---|---|---|
| `APP-H1` | MOVE | §7 completed index + HISTORY |
| `BACKUP-01` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `BRIDGE-R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `CAD-A1` | MOVE | §7 completed index + HISTORY |
| `CAD-A2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `CAD-A3` | MOVE | §7 completed index + HISTORY |
| `CAD-A3-C1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `CAD-A3-C2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `CAD-A4` | KEEP | §2 active roadmap |
| `CAD-R0` | MOVE | §7 completed index + HISTORY |
| `CAD-R0-DATA` | MOVE | §7 completed index + HISTORY |
| `CAD-R0-UX` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `CLOUD-H1` | KEEP | §2 active roadmap |
| `CLOUD-R0` | KEEP | §2 active roadmap |
| `DEEP-AUDIT-R1` | MOVE | §7 completed index + HISTORY |
| `DEVICE-M1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `DOC-R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `DOC-R2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `E2E-R1` | MOVE | §7 completed index + HISTORY |
| `E2E-R1A` | MOVE | §7 completed index + HISTORY |
| `E2E-R1B` | MOVE | §7 completed index + HISTORY |
| `E2E-R1C` | MOVE | §7 completed index + HISTORY |
| `GATE-E2E` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `GATE-KERNEL` | KEEP | §2 active roadmap |
| `GATE-PERF-SCULPT` | KEEP | §2 active roadmap |
| `GATE-SCOPE` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `GATE-USER-01` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `GATE-VIABILITY` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `GLB-IMPORT-R0` | MOVE | §7 completed index + HISTORY |
| `GLB-IMPORT-R0/R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `GLB-IMPORT-R1` | MOVE | §7 completed index + HISTORY |
| `GRID-SNAP-01` | KEEP | §2 active roadmap |
| `IMPORT-01` | MOVE | §7 completed index + HISTORY |
| `IMPORT-01-SCALE` | KEEP | §2 active roadmap |
| `IMPORT-01A` | MOVE | §7 completed index + HISTORY |
| `IMPORT-01B` | MOVE | §7 completed index + HISTORY |
| `INPUT-R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `PERF-BASELINE` | KEEP | §2 active roadmap |
| `PORT-R0` | KEEP | §2 active roadmap |
| `POST-AUDIT-HARDEN-R1` | MOVE | §7 completed index + HISTORY |
| `SCULPT-DIM-01` | KEEP | §2 active roadmap |
| `SCULPT-H1` | KEEP | §2 active roadmap |
| `SCULPT-UNDO-R0` | MOVE | §7 completed index + HISTORY |
| `SPATIAL-R1` | KEEP | §2 active roadmap |
| `Stage 016` | MOVE | §7 completed index + HISTORY |
| `Stage 017` | MOVE | §7 completed index + HISTORY |
| `Stage 018A` | MOVE | §7 completed index + HISTORY |
| `Stage 018B` | KEEP | §2 active roadmap |
| `Stage 018C` | KEEP | §2 active roadmap |
| `Stage 019` | MOVE | §7 completed index + HISTORY |
| `Stage 020D` | KEEP | §2 active roadmap |
| `Stage 020M` | MOVE | §7 completed index + HISTORY |
| `Stage 021` | MOVE | §7 completed index + HISTORY |
| `Stage 022` | MOVE | §7 completed index + HISTORY |
| `Stage 023` | MOVE | §7 completed index + HISTORY |
| `Stage 024` | KEEP | §2 active roadmap |
| `Stage 025` | KEEP | §2 active roadmap |
| `Stage 026` | KEEP | §2 active roadmap |
| `Stage 027` | KEEP | §2 active roadmap |
| `Stage 028` | KEEP | §2 active roadmap |
| `Stage 029` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 030` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 031` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 032` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 033` | KEEP | §2 active roadmap |
| `Stage 034` | KEEP | §2 active roadmap |
| `Stage 035` | KEEP | §2 active roadmap |
| `Stage 036` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 037` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 038` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 039` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `Stage 040` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `STYLUS-G1` | KEEP | §2 active roadmap |
| `TEST-HARNESS-R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `TEST-RUNTIME-R1` | MOVE | §7 completed index + HISTORY |
| `UI-ARCH-AUDIT1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-ARCH-R1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-AUDIT1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-AUDIT2A/2B/2C` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-AUDIT2D` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-LAYOUT-CUSTOM` | KEEP | §2 active roadmap |
| `UI-LAYOUT-R2` | MOVE | §7 completed index + HISTORY |
| `UI-PREF` | MOVE | §7 completed index + HISTORY |
| `UI-PREF-R1` | MOVE | §7 completed index + HISTORY |
| `UI-PREF-R1-CLOSEOUT` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-PREF1` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-PREF2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-R2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-R3/R3R2` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-R4/R4A/R4B/R4C` | KEEP_WITH_LINK | §3 system catalogue / §8 historical navigation + HISTORY |
| `UI-SPEC-R0` | MOVE | §7 completed index + HISTORY |

### 8.4 Decision/idea retention control

- Input formal decision headings: **72** from available `OWNER_DECISIONS_v4.0.54`.
- Later coordinator/OWNER decision added after that file: `TEST-OWNER-03`, retained in §5.3 and §6.2.
- Idea IDs found in active prior master: `IDEA-01`, retained in §5.2.
- Retention proof is set/semantic mapping, not equality of a magic count. If a future source contains additional decision/idea IDs, add them through a new migration event rather than claiming this set was exhaustive for unavailable files.

### 8.5 Legacy aliases that remain valid

- `E2E-R1` = milestone containing `E2E-R1A/B/C`; old `Stage 021/022/023` map one-to-one to those sub-stages.
- `CAD-R0` = `CAD-R0-DATA` + `CAD-R0-UX`.
- `Stage 020S` = retired umbrella for `Stage 020S1` + `Stage 020S2`.
- `UI-PREF` = `UI-PREF1` + `UI-PREF2`, with combined execution `UI-PREF-R1`.
- `Forge Shell` = historical name for Editor Workspace.
- `Freeze` is legacy current-implementation terminology only; user-facing target follows `Start Sculpting` now and future `Create Sculpt Copy` at BRIDGE-R1.

### 8.6 Consistency result

- Active unfinished roadmap Order values are unique 1…29.
- Completed local-format dependency for CLOUD is no longer a false blocker.
- Stage025 does not claim Sculpt Undo is missing.
- OQ-02 no longer blocks completed Stage020M; it blocks SCULPT-DIM-01.
- Active test policy no longer simultaneously mandates FullSharded and forbids it for the same ordinary task.
- Precision and F-16 are not listed as unresolved after their accepted closures.
- Old `.forge`/GLB “never tested” assumptions are replaced with scoped evidence boundaries.
- MIRROR final full hash and current SCULPT-H1 launch state remain explicit unknowns, not invented facts.

**READ NEXT for SCULPT-H1:** current PROJECT STATE; repo `CLAUDE.md`; repo `PROJECT_STATUS.md`; SculptHistory/SculptSession implementation; Sculpt Undo/Redo UI; pointer/hover input path; renderer publication path; the revised current prompt.
