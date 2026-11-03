# FORGESHAPE MASTER PLAN

**Version:** 4.0.58
**Updated:** 2026-09-01
**Supersedes:** v3.69.0 (2026-08-28) — reorganized, not compacted. No owner decision was deleted.
**v4.0.1 corrected twenty defects found by the coordinator's review of v4.0.0 (10.5). v4.0.2 closes the seven that review found only partly fixed, plus four inconsistencies the fixes themselves introduced (10.6). v4.0.3 records the UI-LAYOUT-R2 technical report and opens its required visual acceptance gate. v4.0.4 corrects the unsupported v4.0.3 claim that `UI-SPEC-R0` had already been accepted: the evidence-closeout agent could find no accepted mock in the working tree or Git history, so the owner artifact is again the single required next step. v4.0.5 records a second no-op closeout stop: the post-acceptance prompt was launched with its `ACCEPTED_UI_SPEC_R0_PATH` placeholder still unfilled, so the agent correctly performed no measurement, capture, build, device contact or commit and the execution state did not advance. v4.0.6 records a third correct precondition stop of the FUTURE-RUN prompt (all three OWNER INPUT placeholders were still literals) and a coordinator audit of the live repository Markdown snapshot, which found documentation drift but no new product defect. v4.0.7 records the bounded repo-documentation reconciliation as a reported PASS at docs-only HEAD `f4416e7`: `PROJECT_STATUS.md` and `CLAUDE.md` were corrected, while `ARCHITECTURE.md`, `PRODUCT.md` and `README.md` were reported unchanged. v4.0.8 records a fourth correct `PRECONDITION-NOT-MET` closeout stop and upgrades the reported repository HEAD from short form to the full hash `f4416e750b53004df3a7e6cccb038e32b9d8dd29`; execution state otherwise does not advance. v4.0.9 records the owner's explicit acceptance of `UI-SPEC-R0`, creates the independent binding HTML mock with SHA-256 `9f1e866795a2b2169273a7db62807e544747e673758598779a35fd25553dbe64`, closes the owner precondition, and makes the same-stage R2 measured evidence closeout executable. v4.0.10 records the measured closeout FAIL at evidence-only HEAD `40e8b10`, the owner's visual rejection of the old 132–152 dp host-width direction after seeing the overlays, reopens `UI-SPEC-R0` for an owner revision, and forbids correcting product geometry to the now-disfavored R0 numbers. v4.0.11 records the owner's explicit delegation of final dimensional tuning to the coordinator, accepts Revision 1 of `UI-SPEC-R0` with an edge-hugging 72 dp rail / 8 dp inset and cell tops 112/88/108 dp, and releases one focused R2 correction round for the independently measured Exact/Details host-translation defect. v4.0.12 records a no-op `PRECONDITION-NOT-MET` caused solely by the correction prompt requiring a separately transferred Revision 1 file after the owner had explicitly requested a prompt-only handoff; no correction round was consumed. Execution authority is now transmitted self-contained inside the coordinator prompt, while `UI-SPEC-R0` remains an OWNER artifact and the agent is forbidden to create or infer it. v4.0.13 records correction round 1 as a reported technical PASS at final HEAD `40d1f4490ef874344f0b101da2e290053028f675`: the right host remains edge-hugging in all 66 cell×state rows, zero intra-cell top/right/width drift is reported, the Exact/Details inward translation is removed, the full suite expands to 330/330, and the stage remains OPEN only for owner/coordinator visual review. v4.0.14 records the evidence-only owner-review bundle at HEAD `71bab57e4e12a91c5f1c7b8cec6a5af9b92569b7` and the coordinator's visual PASS after direct inspection of the supplied contact sheet / review pairs: the rail remains narrow and edge-hugging, Exact/Details stays inboard without overlap or unnecessary viewport loss, and no new in-scope visual defect is identified. `UI-LAYOUT-R2` is COMPLETE, correction round 2 is retired unused, and the UI moratorium is active. v4.0.15 accepts the reported docs-only final acceptance sync at HEAD `14de5ee58f061b36774f22cccc88187f4c0e85bf`, completes coordinator-owned `CAD-R0-DATA`, fixes the stale MASTER footer/version-note-count defect found during this bump, and releases self-contained `E2E-R1A / Stage 021` for the first versioned `.forge` save/load implementation. v4.0.16 records owner approval of the native-project/interchange boundary: `.forge` is a self-contained cross-device ForgeShape project format that may be semantically compact where exact reconstruction is deterministic, while GLB/glTF, OBJ, FBX and other deliberately selected standard formats are separate staged import/export concerns. The decision does not add OBJ/FBX/import to E2E-R1A or change MVP sequencing; CAD-R0-DATA Decision v2 and the Stage 021 prompt are updated only to make this boundary explicit. v4.0.17 records E2E-R1A as a reported product/test PASS at final HEAD `a52be2464c7b1ca101026e05be9727b16da33627`, including `.forge` v1 save/open, process-death reopen, 342/342 FullSharded and 2163 native checks, but holds final stage closure because the owner's screenshot still shows two FullSharded background shell tasks running after the report. v4.0.18 records the owner's explicit operational override that those background tasks are already finished and are to be ignored; the cleanup-only hold is withdrawn, E2E-R1A becomes COMPLETE without further test rerun, and E2E-R1B / Stage 022 becomes the single READY coding stage. v4.0.19 accepts E2E-R1B as a reported PASS at short final HEAD `678af45` (the report omitted the full 40-character hash): autosave/recovery, Scoped Storage `.forge` transfer, lifecycle persistence resilience, injected renderer device-loss rebuild and bounded offline diagnostics are reported green; FullSharded expands to 382/382 and native to 2199 checks. E2E-R1B is COMPLETE and E2E-R1C / Stage 023 becomes the single READY coding stage. The Stage 023 preflight must resolve and record the full baseline HEAD, prove the existing metres/+Y/right-handed world contract before export coding, and stop rather than invent an axis conversion if repo truth disagrees. v4.0.20 accepts E2E-R1C as a reported PASS at final HEAD `cdc184b999bae716809f632c11fe771ebbb98c2b`: early deterministic GLB 2.0 export ships with 1:1 metres, repo-proven right-handed +Y-up zero-conversion coordinates, body-local geometry plus renderer-equivalent node transforms, Construction/Sculpt export, SAF write path, two owner-ready sentinel GLBs and a 399/399 authoritative FullSharded PASS. E2E-R1C is COMPLETE. The only next gate is OWNER `GATE-E2E`; no further product stage may start before the owner imports and judges the sentinel GLB in Godot. v4.0.21 accepts the read-only `PASS-GATE-HANDOFF`: both committed sentinel GLBs exist byte-for-byte at the reported Stage-023 HEAD with pinned hashes and an owner checklist. This does not close `GATE-E2E`; the external Godot verdict is still OWNER work. One non-blocking documentation wording defect is recorded: the existing gate note says the Construction sentinel forms a 'rising' line although the bound Y translations are negative, so the expected line descends in Y as X/Z increase. v4.0.22 records OWNER approval of a revised static-interchange transform policy after Blender visual review: GLB export must bake body Rotation + Scale into mesh geometry, export identity rotation and unit scale, and retain body Translation/pivot at node level. This is standards-conformant glTF and is intended to reduce importer-dependent transform presentation while preserving the ForgeShape pivot. The current Stage-023 implementation is reopened for one focused correction before OWNER GATE-E2E; OBJ/FBX remain out of scope. v4.0.23 accepts E2E-R1C-C1 as a reported PASS at final HEAD `868995d135b4582693b10cd486abc7418b45c5a9`: the exporter now bakes `Rz·Ry·Rx·S` into positions, uses inverse-transpose normals, keeps Translation/pivot on the node, writes no node matrix/rotation/scale, regenerates both sentinels with new authoritative hashes, and passes 406/406 FullSharded plus 2292 native checks. E2E-R1C is COMPLETE again; GATE-E2E is OPEN / OWNER on the corrected sentinels only. v4.0.24 records OWNER approval of one bounded exception to the post-MVP import gate: `GLB-IMPORT-R0`, a diagnostic session-only GLB Imported Mesh Preview used to distinguish exporter defects from external-importer coordinate/presentation differences. It imports only the static triangle subset ForgeShape currently exports, never fabricates Construction Source, never becomes `.forge` project truth, and exists solely before GATE-E2E. Broad/durable import remains post-MVP under the original readiness gate. v4.0.25 accepts `PASS-GLB-IMPORT-R0-ROUNDTRIP-EQUIVALENT` at final HEAD `27ec0bcc377d90fb69daa3b7b1332723ca4fe94b`: independent parsing of the actual corrected GLB bytes reproduces the ForgeShape source world geometry with maximum position delta exactly `0.0 m` for Construction and Sculpt, while normals agree within floating-point tolerance. The bounded diagnostic import is COMPLETE, production import remains post-MVP, and OWNER `GATE-E2E` is OPEN again. The remaining Blender visual discrepancy is therefore not evidence of a ForgeShape source→GLB byte mismatch; it must be classified by controlled external-import checks before any exporter correction. v4.0.26 records OWNER approval of `ARCH-OWNER-09`: extend the existing session-only diagnostic importer just enough to open a real external static GLB from Nomad Sculpt. The owner sample `1 lowpoly.glb` is 142,820 bytes (SHA-256 `59fd1bfe8473e9b78849170ef6c02023d45bf769a509b7ab1974127360ff3bd6`), GLB 2.0 / Nomad 11, one node with a matrix, one mesh with seven triangle primitives, 1,987 POSITION vertices, 7,908 uint32 indices / 2,636 triangles, no NORMAL accessor, COLOR_0/COLOR_1/TEXCOORD_0, one double-sided material and Nomad extras. `GLB-IMPORT-R1` is the single READY stage; it remains session-only preview, not durable production import. v4.0.27 accepts `PASS-GLB-IMPORT-R1-OWNER-RETEST-READY` at final HEAD `504f97d6b087bd0103762a53dd4ac515433e95dd`: matrix/TRS, multi-primitive static meshes, generated normals, double-sided preview culling and safe ignored color/UV/extras are reported implemented; a deterministic Nomad-like fixture passes all focused/device/regression gates, including 443/443 FullSharded and 2431 native checks. The exact owner binary was correctly not fabricated or searched for, so `GLB-IMPORT-R1` is technically COMPLETE but remains OWNER-RETEST-PENDING on the real `1 lowpoly.glb`. No further coding stage is READY before that manual file test. v4.0.28 records the OWNER's real-file result: the supplied low-poly GLB opens normally in ForgeShape, so `GLB-IMPORT-R1` is OWNER-accepted for its intended external static preview scope. The missing Objects-list entry, gizmo, persistence and Sculpt are confirmed design limits of preview mode, not R1 defects. OWNER then explicitly approves `ARCH-OWNER-10` and an early bounded `IMPORT-01A` production slice: imported GLB geometry becomes real durable `Imported Mesh` objects with ObjectId, Objects-list selection, gizmo transform, Undo/Redo and self-contained `.forge` persistence. This is an explicit sequencing/scope override of the earlier post-MVP timing only for `IMPORT-01A`; the high-density/performance readiness gate still governs broad import scaling, and imported-mesh Sculpt remains a separate `IMPORT-01B` stage after 01A. v4.0.29 records an incomplete but accepted IMPORT-01A foundation checkpoint at commit `bdf3f59`: `SceneObject` now owns placement independently of Construction, body representation is explicit, `ImportedMesh` domain truth exists, parser result types are separated from project truth, and existing `.forge` golden bytes reportedly remain unchanged. No user-visible IMPORT-01A capability is complete yet. The stage remains RUNNING and must resume from `bdf3f59` to implement durable import, mixed-scene history, self-contained `.forge` Imported Mesh persistence, Objects-list/gizmo integration and full verification. v4.0.30 accepts `PASS-IMPORT-01A-OWNER-RETEST-READY` at final clean HEAD `4c296f5415aa6955c2cc77e84d063c9ae5a59d47`: durable Imported Mesh objects now participate in Objects selection, Move/Rotate/Scale gizmo, one-step import history, Undo/Redo and self-contained `.forge` persistence; mixed Construction/Sculpt/Imported projects pass manual Save/Open, Save Copy, autosave and recovery; legacy corpus hashes remain unchanged; native expands to 2528 checks and FullSharded to 455/455. IMPORT-01A is technically COMPLETE and waits only for OWNER durable-import retest of the real `1 lowpoly.glb` before IMPORT-01B can open. v4.0.31 accepts `PASS-ARCH-HEALTH-REVIEW-WITH-DEBT` at final clean HEAD `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802`: the architecture review found the repository broadly modular enough for continued feature work, corrected one concrete JNI/autosave data-race hazard, removed one redundant Construction tessellation per publish, corrected stale comments/docs, and recorded non-blocking representation/UI/renderer/data-contract debts. The review used full native/JVM/build coverage plus five focused device suites rather than FullSharded, which is accepted as proportional for the bounded corrections. v4.0.32 records OWNER durable acceptance of the real `1 lowpoly.glb`: the imported body appears as a normal project object, Move/Rotate/Scale gizmo works, and the saved `.forge` reopens with the imported object intact. `IMPORT-01A` is therefore COMPLETE. OWNER also approves `ARCH-OWNER-11` for Imported Mesh → current reversible Sculpt semantics and `UI-OWNER-45` for deleting real project objects from the Objects list. `IMPORT-01B` is the single READY coding stage and includes only this bounded delete capability; rename/visibility/lock/duplicate remain in their later stage. v4.0.33 records an `EVIDENCE-BLOCKED` IMPORT-01B continuation: the candidate implementation was present only in the dirty worktree on HEAD `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802`, with focused/native/JVM/build/device evidence green but without final aggregate/clean-tree closure. v4.0.34 accepts `PASS-IMPORT-01B-OWNER-RETEST-READY` at final clean HEAD `04f28df49f5000e863628571cdf5f8f076dac7e0`: the unchanged candidate tree passed authoritative FullSharded 33 classes / 472 tests / 472 PASS with all integrity counters zero after a bounded recovery of the ForgeShape AVD, then the full stage was committed. Imported Mesh reversible Sculpt, `IMPT + SCUL`, real SceneObject Delete with exact Undo/Redo, deterministic selection fallback and renderer pruning are therefore technically complete. OWNER real-file Sculpt + Delete retest is now the single next action. A non-blocking tooling debt is recorded: the emulator launcher lacks an explicit cold-boot mode and may resume a wedged snapshot. v4.0.35 recorded an attempted IMPORT-01B-C1 correction for missing Sculpt Undo/Redo. v4.0.36 corrects that premise after live-repository review by Claude: current ForgeShape explicitly documents `Sculpt has no Undo`, Sculpt vertices/revisions are deliberately excluded from project history steps, and the Undo/Redo controls are intentionally not drawn in Sculpt. Therefore the OWNER observation was expected current behavior, not a regression. IMPORT-01B-C1 is cancelled with no product change. v4.0.37 accepts the agent's confirming closeout report: the repository independently proves the old invariant in CLAUDE.md, ARCHITECTURE.md, PRODUCT.md, EditorWorkspaceView and JNI; IMP01B-06 intentionally asserts Undo was unavailable in Sculpt. v4.0.38 records an explicit OWNER product override: `ARCH-OWNER-12` authorizes a **separate bounded Sculpt Undo/Redo history**, distinct from project/object history. The old hard rule `Sculpt has no undo` is superseded; the still-binding rule becomes that **project history never stores Sculpt vertex state**, while Sculpt owns its own volatile per-stroke history. v4.0.39 accepts `PASS-SCULPT-UNDO-R0-OWNER-RETEST-READY` at final clean HEAD `2bcfa3fa11fd96fa934887f16e55caefbee496d9`: a dedicated `SculptHistory` is implemented inside retained Sculpt state, one completed stroke produces one bounded delta entry, Undo/Redo is contextual in Sculpt, Construction and Imported Sculpt share the same mechanism, source truth and SceneObject transforms stay immutable, revisions remain monotonic, Back→Resume retains runtime history, and `.forge`/autosave/recovery intentionally serialize only the current Sculpt mesh—not the Undo stacks. The final runtime/test tree passes authoritative FullSharded 34 classes / 482 tests / 482 PASS. The cancelled C1 artifacts were safely removed in preflight. OWNER then confirmed real-device Sculpt Undo/Redo. v4.0.40 accepted the large CAD-R0-A1A2 technical PASS at `3a00e0be6c528c9f96ae1f004bdaf70bc67afb82` with 491/491 FullSharded. v4.0.41 records a new explicit OWNER UX/architecture direction: implement the real Home/project entry (`Open File`, `New Project` → `CAD`/`Sculpt`), replace list-first CAD plane choice with direct spatial XY/XZ/YZ targets, and let subsequent sketches choose supported planar CAD faces physically by stylus/finger. Stable face-supported sketches must resolve through semantic feature-owned topology references, never persisted render-triangle indices. Fresh CAD-R0-UX research is captured in `FORGESHAPE_CAD_R0_UX_DECISION_v1.md`; `CAD-A3` is released as the single READY coding stage and folds in APP-H1 implementation.** The reorganization is unchanged throughout.
**Plan file set (carry all three between coordinator conversations):**

| File | Owns | Read when |
|---|---|---|
| `FORGESHAPE_MASTER_PLAN_v4.0.58.md` (this file) | current state, coordinator rules, product/UX/architecture/quality contracts, roadmap, risks, process | always, in full, once per new coordinator conversation |
| `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md` | all 72 binding `*-OWNER-*` decisions, verbatim, with status headers | on demand, by ID |
| `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58.md` | report archive + all 133 version notes | when reconstructing why something was decided; never during normal stage work |

Plus, at any moment, **exactly one** active coding-agent prompt `.md`.

---

## 0. HOW TO READ THIS PLAN

### 0.1 The one rule that makes this document trustworthy

**Only section 1 (`CURRENT EXECUTION`) may assert what is happening now.**

Precisely, and this is the enforceable form of the rule:

- **Forbidden outside section 1:** any *status claim* about a stage, gate or decision — that it is running, current, active, next, in progress, on hold, or awaiting something. Examples of the forbidden shape: `Stage 020R2 — CURRENT`, `UI-AUDIT2B is currently running`, `UI-R3 is current`, `Current next:`, `RUNNING work`, `the running R2 stage`.
- **Allowed everywhere:** the words themselves used as ordinary language about things other than stage status — "current repo truth outranks stale text", "the current renderer", "the next paragraph", "current device evidence". The rule is about claims, not vocabulary.
- **One exception inside this file:** the roadmap table in 7.1 carries a `Status` column drawn from the closed vocabulary in 0.3. That column *is* status, and 7.1 is its only other home. Prose in 7.2 carries no status at all.
- **Scope of the rule:** this MASTER PLAN, plus the `Status` and `Implementation` rows of decision metadata. **`FORGESHAPE_MASTER_PLAN_HISTORY` is exempt in full** — it is an archive of statements that were true when written, and rewriting them would falsify the record. **Decision bodies in `FORGESHAPE_OWNER_DECISIONS` are also exempt**, because they are preserved verbatim; where a body says "next" or "current", that is the wording of the day and the decision's metadata row is the authority. Both exemptions are stated in those files too.
- Sections 0.1, 10.1 and 10.4 of this file quote historical wording on purpose, inside quotation marks or backticks, and are exempt for that reason.

If you find a status claim outside those places, that is a defect: fix it in the same edit and record it in the version note. The detector in 10.3 implements exactly this rule, no more and no less.

This rule exists because v3.69.0 contained four competing "now" statements at once (§9 `UI-LAYOUT-R2 READY`, §8 `Stage 020R2 — CURRENT`, §19 `UI-R3 is current`, §20 `UI-AUDIT2B is currently running`), three of them stale by nine plan versions. A fresh coordinator reading linearly could pick up a real-looking, wrong `CURRENT`.

### 0.2 Table of contents

| § | Section | What lives here | What does NOT live here |
|---|---|---|---|
| 0 | How to read this plan | map, status vocabulary, ID index, legacy ID mapping | any product content |
| 1 | **Current execution** | verified HEAD, active stage, the single next step, awaited input | roadmap beyond the next step |
| 2 | Coordinator — identity, behaviour, working rules | who the coordinator is, idea-intake protocol, handoff, prompt authoring, plan integrity, documentation ownership, naming policy | product decisions |
| 3 | Product | north star, differentiation, MVP scope, non-goals, import policy | UI composition |
| 4 | UX / UI | layout specification requirement, measurable layout DoD, premium quality governance, visual acceptance procedure | the decisions themselves (see `OWNER_DECISIONS`) |
| 5 | Architecture | domain authority, Android boundary, rendering, persistence/document model, cross-platform, cloud, dependency policy | stage order |
| 6 | Quality / verification | device safety, Git, toolchain, proportional test gates, completed platform gates P0/P1 | visual acceptance (see 4.4) |
| 7 | **Roadmap** | the single binding execution order, stage catalogue by area, existential gates, dependency graph | stage status outside the 7.1 table (status otherwise lives only in 1) |
| 8 | Risks, debt, open questions, idea inbox | risk register, technical debt, unanswered questions blocking stages, unfiled owner ideas | fixes (those are stages) |
| 9 | Process | WIP limits, closure definitions, work types, budgets, retrospectives | anything product |
| 10 | Appendices | legacy ID map, backfilled roadmap nodes, integrity commands, v4.0.0 migration record | current truth |

### 0.3 Closed status vocabulary

These are the only status words permitted in these files. Prose statuses ("technically complete but visually on hold pending…") are forbidden — v3.69.0 had eleven such variants and they could not be sorted or counted.

**Stage status:**

| Status | Meaning |
|---|---|
| `PLANNED` | in the roadmap, not started, no blocking open question |
| `BLOCKED-BY:<ID>` | cannot start until that stage/gate/question resolves |
| `READY` | prompt may be written now |
| `RUNNING` | a coding-agent prompt is executing (max 1, see 9.1 W-1) |
| `GATE` | not a stage but a pass/fail checkpoint over one or more stages; what it covers is named in the roadmap's `What` column, never inside the status cell |
| `OPEN` | implemented, awaiting a verdict (max 1, see 9.1 W-1) |
| `COMPLETE` | PASS verdict, no adjectives, debts moved to 8.2 |
| `ACCEPTED-AS-IS` | closed with known, recorded shortcomings; those shortcomings are owned by another stage or by 8.2 |
| `CANCELLED` | will not be executed; reason recorded |
| `ARCHIVE` | historical, kept for traceability only |

**Decision status** (used in `OWNER_DECISIONS`): `APPROVED`, `APPROVED-FUTURE`, `LEGACY-ACTIVE`, `OPEN-QUESTIONS`, `SUPERSEDED-BY:<ID>`, `WITHDRAWN`.

Time-boxed statuses `TECHNICAL PASS`, `VISUAL HOLD`, `PARTIAL PASS`, `CLOSEOUT HOLD` are no longer stage statuses. See 9.1 W-3. `HELD` is not a status either — a stage waiting for something is `BLOCKED-BY:<ID>`.

**Executor — a separate axis from status.** Status says whether a work item can start; the executor says *who starts it*. Never encode one in the other.

| Executor | Meaning |
|---|---|
| `OWNER` | the owner produces it. No coding-agent prompt is ever written for it. `UI-SPEC-R0` is one; so is the remote backup in 1.6. |
| `COORDINATOR` | the coordinator produces it: a research or decision document, no product code. `CAD-R0-DATA`, `CAD-R0-UX`, `CLOUD-R0`, `PORT-R0`. |
| `AGENT` | a coding-agent stage with a prompt, tests and a report. Everything else. |

`READY` on an `OWNER` or `COORDINATOR` item means the owner or coordinator may begin. It never means "write a prompt".

### 0.4 Identifier rules

Three separate layers. Never mix them — mixing them is what made v3.69.0 hard to navigate.

| Layer | Purpose | Mutability | Example |
|---|---|---|---|
| **Section number** | navigation inside this file | freely renumberable, max 3 levels | `4.2` |
| **Work / decision ID** | identifies a thing across time, prompts, commits, artifacts and test IDs | **immutable, never reissued, gaps allowed** | `UI-OWNER-44`, `Stage 020R2`, `E2E-R1` |
| **Relation** | how two things relate | explicit named fields, never encoded in a number | `Revises:`, `Extends:`, `Supersedes:`, `Blocked-by:` |

Consequences, binding:

1. **Never renumber an existing work ID.** These IDs live outside this file — in prompt filenames (`AGENT_STAGE_020R2_FULL_TRANSFORM_GIZMO_COMPACT.md`), artifact paths (`artifacts/uiaudit2a/`), test IDs (`S020R2-01..22`, `UILR1-15`, `POSTARCH-V01..V07`, `VUI-01..VUI-14`) and commit messages. The coding agent does not read this plan (2.2), so these IDs are the only bridge between plan and repository.
2. **Cross-references cite IDs, not section numbers**, wherever the target has an ID. Section references are allowed only for policy sections of this file.
3. **"We are going back to redo feature X" is written `Revises: <ID>`**, never as a deeper number and never as a new sub-letter.
4. New stage IDs use the shape `AREA-NN` or `AREA-Rn` (`E2E-R1`, `SPATIAL-R1`, `CAD-R0-DATA`). `R<n>` means a revision of the same work. **A trailing uppercase letter means planned decomposition and attaches to the whole ID**, so a milestone `E2E-R1` decomposes into `E2E-R1A`, `E2E-R1B`, `E2E-R1C`, exactly as `Stage 018` decomposes into `018A/B/C`. Lowercase suffixes are not used.

### 0.5 ID index — where to find what

**Owner decisions (67)** — all in `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md`, sorted by family:
`UI-OWNER-01 … UI-OWNER-46` (plus `33A`, `33B`, `33C`) · `ARCH-OWNER-01 … 13` · `INPUT-OWNER-01` · `DOC-OWNER-01`, `DOC-OWNER-02` · `TEST-OWNER-01` · `AUDIT-OWNER-01`.

**Quick decision lookup by topic:**

| Topic | Decision IDs |
|---|---|
| Editor Workspace / viewport-first shell | UI-OWNER-01, 07, 20, 23, 43, 44 |
| Adaptive phone/tablet | UI-OWNER-02, 17 |
| Themes / palettes | UI-OWNER-09 (foundation), 22 (**exact hex values**), 42 (**count = 5**) |
| Motion / transitions | UI-OWNER-16, 43 |
| Selection feedback / outline | UI-OWNER-10, 11 |
| Transform gizmo & coordinate spaces | UI-OWNER-30 |
| Scale semantics (directional / dimensions / relative) | UI-OWNER-33, 33A, 33B, 33C |
| Snapping / alignment | UI-OWNER-31, 34, 35 |
| Objects, multi-select, hierarchy | UI-OWNER-12, 25, 40 |
| Sculpt history | UI-OWNER-41 |
| Topology vocabulary | UI-OWNER-26 |
| Construction → Sculpt transition | UI-OWNER-05 (legacy), 24 (target), 27 (wording), 29 (current label) |
| CAD / Sketch boundary | UI-OWNER-04, 13 |
| Settings / app menu / home | UI-OWNER-36, 37, 38, 39 |
| Touch targets / input | UI-OWNER-28, INPUT-OWNER-01 |
| Visual acceptance process | UI-OWNER-14, 18, 19, AUDIT-OWNER-01 |
| Portability / native project vs interchange / cloud | ARCH-OWNER-01, 02, 03, 04, 06 |
| UI source structure | ARCH-OWNER-05 |
| Test infrastructure | TEST-OWNER-01 |
| Documentation / naming | DOC-OWNER-01, DOC-OWNER-02 |

**Stage/gate IDs** are indexed in 7.2, with the legacy mapping in 10.1.

### 0.6 Reading budget

| Class | Content | When |
|---|---|---|
| Always | this file, in full | once per new coordinator conversation |
| On demand | one decision by ID; the report archive entry for the stage being closed | during a stage |
| Never during normal work | `HISTORY` version notes | only when reconstructing rationale |

---

## 1. CURRENT EXECUTION

*The only section in these three files written in the present tense.*

### 1.1 Verified repository state

- Latest ingested report: **`PASS-MIRROR-01-OWNER-LATER-READY`**.
- Mirror baseline: `94daaa92494988e6604d8dcf2db6f8a535a9a9f3`, clean.
- Mirror final commit was reported only as short hash **`3283089`**; tree reported clean.
- The report did not provide the required full 40-character final HEAD. Do not fabricate it.
- Next stage preflight must resolve the full hash for `3283089` before editing and verify current HEAD matches that commit.
- Mirror implementation/tests are otherwise internally coherent.
- Automated verification: about 18 minutes, inside TEST-OWNER-03 target.
- No FullSharded was run.
- No `.forge` schema change.

Coordinator verdict:

**`MIRROR-01 = TECHNICAL COMPLETE / REPOSITORY COMPLETE BY CLEAN-TREE EVIDENCE / OWNER LATER`**

with one reporting debt: exact full final HEAD must be resolved at next preflight.

### 1.2 MIRROR-01 accepted contract

Accepted implementation:

- one Construction SceneObject mirrored across world XY / XZ / YZ;
- original unchanged;
- fresh ObjectId;
- one ProjectHistory step;
- Undo removes only mirror;
- Redo restores same mirrored ObjectId;
- positive Absolute Scale only;
- no persisted negative scale;
- world reflection represented as `R' = F · R · Qx`;
- determinant +1 proved;
- direct world-geometry reflection equivalence tested;
- source Construction truth cloned through existing Stage 018A capture/restore path;
- visibility/lock policy reused;
- no source retessellation;
- no renderer-only mirror truth;
- no new `.forge` schema;
- Imported/Cad/Sculpt mirror explicitly refused.

UI:
- Mirror added to Objects overflow;
- compact XY/XZ/YZ chooser;
- chooser cancel creates no history step;
- five row commands wrap to two lines rather than shrinking below touch-target floor.

### 1.3 Mirror evidence assessment

Reported green:

- `FORGESHAPE_MIRROR_SELFTEST_OK`: 62 checks;
- native startup: 22 selftest tokens / 3,335 checks / 0 failures;
- `MirrorSmokeTest`: PASS;
- `ObjectCommandsTest`: PASS;
- debug/release builds PASS;
- release self-test symbols remain 0;
- device guards PASS;
- corpus unchanged.

The report text is partially duplicated/truncated, but the core implementation/evidence statements are consistent enough to accept.

No OWNER visual acceptance is inferred.

### 1.4 TEST-OWNER-03 remains binding

Ordinary feature stages:
- automated target <=20 minutes;
- hard stop 30 minutes;
- no FullSharded by default;
- automate correctness/invariants;
- move visual/ergonomic matrices to OWNER LATER.

MIRROR-01 complied at about 18 minutes automated.

### 1.5 SCULPT-H1 reconciliation

`SCULPT-H1` is now READY.

Core Sculpt Undo/Redo already exists under `ARCH-OWNER-12 / SCULPT-UNDO-R0` with:
- separate SculptHistory owner;
- bounded delta history;
- redo invalidation;
- Back→Resume retention;
- no serialization of history stacks.

Therefore SCULPT-H1 must **not** replace or shrink that history implementation.

The old UI-OWNER-41 “initial five-action capacity” is interpreted as the original experiment. Current core retention remains authoritative.

SCULPT-H1 now implements the missing UX only:
- compact history navigator adjacent to Sculpt Undo/Redo;
- a viewport showing up to 5 recent rows at once, scrollable over the currently retained SculptHistory entries;
- tap/jump to a committed history cursor;
- new stroke after jump invalidates abandoned redo branch through existing history semantics;
- stylus-hover preview on supported hover hardware, non-destructive;
- finger-only devices get no fake hover.

No snapshot-based second history system.

### 1.6 Stage 025 dependency

The original Stage 025 blocker said “no new brushes before undo”.

Core Sculpt Undo/Redo is already complete, so the safety dependency is already satisfied.

However the roadmap order remains:

`SCULPT-H1 → Stage 025`

for one final compact history UX pass before adding Flatten / Crease / Mask.

No extra testing/tooling stage is inserted between them.

### 1.7 OWNER LATER TEST PACK

Accumulated OWNER checks:

- Delete → Undo → Redo;
- Settings / five palettes / handedness / gizmo size+thickness;
- Selection Outline visual weight/toggle/representations;
- Stage 018A Rename / Visibility / Lock / Duplicate UX;
- Stage 020M dimensions/anchors/Relative Scale UX;
- MIRROR-01 plane chooser, two-line Objects command strip, visual correctness and Undo feel;
- SCULPT-H1 after implementation:
  - navigator placement/readability;
  - jump workflow feel;
  - stylus hover-preview feel;
  - scrolling with several strokes;
  - interaction with existing brush rail.

Do not force a manual OWNER session yet.

### 1.8 Current WIP

- Stage 020M: COMPLETE / OWNER LATER.
- MIRROR-01: COMPLETE / OWNER LATER; full final hash pending preflight resolution.
- SCULPT-H1: READY.
- Stage 025: PLANNED next after SCULPT-H1.
- `IMPORT-01B` OWNER Delete verdict: PARKED.
- `SCULPT-DIM-01`: BLOCKED-BY:OQ-02.
- `Stage 020D`: BLOCKED-BY:OQ-01.
- RUNNING 0 / READY 1.

### 1.9 Single next step

Run:

`OPUS_5_HIGH_SCULPT_H1_HISTORY_NAVIGATOR.md`


---

## 2. COORDINATOR — IDENTITY, BEHAVIOUR AND WORKING RULES

### 2.1 Who you are

You are the **ForgeShape coordinator**. You are not the coding agent and you never edit the application yourself.

You own:
- orchestration truth: what is verified, what is running, what comes next;
- this plan set and every edit to it;
- the roadmap and its order;
- the record of owner decisions;
- coding-agent prompts — writing them, one at a time;
- verdicts on agent reports.

You do not own:
- repository technical truth. That belongs to the live repository and to `PROJECT_STATUS.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `CLAUDE.md`, `README.md`;
- product direction. That belongs to the owner. You may recommend; you may not decide.

How you behave:

1. **Evidence over assertion.** A report that declares itself COMPLETE is a claim, not a result. Check the claim against the contract in the prompt. v3.43.0 is the reference case: Stage 020 was rejected as COMPLETE because the report itself admitted the X/Y rings were not true world-space rotations. Keep doing exactly that.
2. **Never silently authorise an owner-gated action.** If a decision is marked `OPEN-QUESTIONS`, the stage does not start.
3. **Never expand scope inside a stage.** A good idea discovered mid-stage goes to 8.5, not into the running prompt.
4. **One active prompt.** See 9.1 W-1.
5. **Ask the owner targeted questions** whenever an unresolved product/UX decision would materially improve the next prompt — but ask in plain language, with the concrete visible effect in the app, and with a recommendation. Never ask the owner to choose between unexplained internal labels.
6. **Update the plan before generating the next prompt.** Always in that order.
7. **Be honest about what did not happen.** If a documentation obligation, an integrity check or an evidence item was skipped, say so in the version note rather than asserting "all preserved".

### 2.2 Source hierarchy

For implementation truth, in order:
1. live repository and runtime evidence;
2. repository-owned `PROJECT_STATUS.md`;
3. `ARCHITECTURE.md`, `PRODUCT.md`, `CLAUDE.md`, `README.md`;
4. this plan set — **for orchestration and roadmap only**.

This plan set is external to `D:\TRAVELAPPS\ForgeShape`. The coding agent must not request, read, copy or maintain it unless a specific prompt explicitly quotes a requirement from it.

Because of that last rule, this plan must not become a mirror of the repository. v3.69.0 drifted here: it carried full commit hashes, test totals, class names, LOC counts and mathematical formulas that belong in `PROJECT_STATUS.md` / `ARCHITECTURE.md`. v4.0.0 keeps only what orchestration needs: the current HEAD, verdicts, and the invariants that a prompt must restate. When you find yourself copying a repository fact into this plan, ask whether a prompt would ever quote it. If not, leave it in the repository.

### 2.3 Idea-intake protocol — what to do when the owner writes an idea

**Binding, and the most frequently violated rule in practice.**

When the owner writes an idea, a feature wish, a complaint about the app, or "wouldn't it be good if…", that is **not** an instruction to implement it, and **not** an instruction to change the running stage. It is material for the plan.

Do this, in order:

1. **Do not touch the active prompt.** An idea never modifies a running stage. If the owner explicitly wants the running stage stopped, they must say so; then you record it as `CANCELLED` with a reason.
2. **Play it back in one sentence,** in plain language, and name what it touches (product / UX / architecture / process) and which existing IDs it collides with or extends.
3. **Ask at most three questions,** and only questions whose answer changes *where the idea lands* or *what it displaces*. Not questions you can answer from the plan.
4. **Classify it** as exactly one of:
   - a new owner decision → new `*-OWNER-NN` ID in `OWNER_DECISIONS`;
   - an extension of an existing decision → amend that decision, add `Extends:`/`Supersedes:` fields, never silently rewrite its body;
   - a new roadmap node → new stage ID in 7.2 and a position in 7.1;
   - an open question → 8.3;
   - a non-goal → 3.5;
   - already covered → name the ID and say so; do not create a duplicate.
5. **Propose the destination out loud** — file, section, ID, and, if it changes the order, exactly which stage it displaces and by how much. Say what it costs.
6. **Write it in only after the owner confirms.** Then add a version note in `HISTORY`.
7. If the owner says "just remember this for later", file it in **8.5 Idea inbox** with status `PROPOSED`. Never file an unconfirmed idea as `APPROVED`, and never invent a decision ID for it.
8. **Never implement on the same turn the idea arrived.** The gap between "the owner mentioned it" and "the agent is building it" must always contain an explicit owner confirmation.

The reason this rule is binding: between 2026-08-26 and 2026-08-28 the plan grew by 17 versions and 13 stage activities while the product gained zero features. Ideas that arrive as conversation and leave as code are the mechanism by which that happens.

### 2.4 Coordinator conversation handoff

When a coordinator conversation becomes too large or slow, start a fresh one and provide exactly:
- `FORGESHAPE_MASTER_PLAN_v*.md`, `FORGESHAPE_OWNER_DECISIONS_v*.md`, `FORGESHAPE_MASTER_PLAN_HISTORY_v*.md`;
- the active coding-agent prompt `.md`, if any;
- one line identifying the new assistant as ForgeShape coordinator.

The new coordinator must:
1. read this MASTER PLAN in full, once;
2. read the active prompt in full;
3. read `OWNER_DECISIONS` on demand by ID — not end to end, unless auditing;
4. not read `HISTORY` unless reconstructing rationale;
5. not rely on memory of any previous conversation;
6. not require a separate `FORGESHAPE_PROJECT_STATE.md`; if one exists for harness compatibility, treat it as a non-canonical generated mirror;
7. wait for the running agent report if 1.3 says a stage is `RUNNING`;
8. update the plan after every report, before writing the next prompt;
9. keep exactly one active prompt;
10. preserve owner decisions and the long-term roadmap even when an agent report suggests a different next stage.

### 2.5 Plan integrity — no autonomous compaction

**OWNER RULE — 2026-08-27, unchanged and still binding: do not shorten this plan autonomously.**

The plan is allowed to be large because it carries owner decisions and the protected long-term roadmap. Before publishing every new version:

1. use the previous canonical intact files as the base;
2. update targeted sections and append the new version note;
3. never reconstruct the plan from a shortened derivative;
4. compare heading sets before and after;
5. verify that every `UI-OWNER-*`, `ARCH-OWNER-*`, `INPUT-OWNER-*`, `DOC-OWNER-*`, `TEST-OWNER-*`, `AUDIT-OWNER-*` entry and every CAD/cloud/persistence/UV/export section still exists unless the owner explicitly superseded it;
6. record any intentional deletion or supersession explicitly;
7. use file length as an integrity signal only, never as a reason to delete owner content.

A sudden large drop in size is a **recovery warning**, not a cleanup success.

New in v4.0.0 — the rule is now mechanically checkable, because prose enforced by the same model that breaks it is not enforcement. Run the four commands in 10.3 before publishing any version, and append their one-line result to the version note:

```
heading-set diff vs <prev>: +N / −N | OWNER-IDs 57→57 | competing-now hits outside §1: 0
```

If you did not run them, write "integrity check: not run" in the note. Do not write that content was preserved unless you checked.

### 2.6 Prompt authoring — canonical policy

This is the single source of prompt-writing rules. The standalone `FORGESHAPE_PROMPT_AUTHORING_RULES.md` is deprecated since v3.46.0 and must not be carried as a second policy source.

For every coordinator → coding-agent prompt:

1. **One contract, one place.** State a product/architecture rule once. Tests and acceptance prove or reference it; they do not restate it.
2. **Use matrices/tables for variants.** Axes, modes and layouts do not become repeated prose.
3. **Test risks, not sentences.** Prefer parameterized coverage when compact/short/expanded share one production path.
4. **Acceptance is a summary,** not a second test suite.
5. **Runtime walkthrough is runtime-only.** Do not repeat deterministic unit/instrumented proof by hand.
6. **Preflight asks unresolved questions,** it does not inventory architecture the repo docs already own.
7. **Keep indispensable math exact and the explanation short.** Preserve formulas, invariants, failure/degeneracy rules.
8. **Out-of-scope appears once.**
9. **Delegate durable boilerplate to repo rules.** Device safety, Git hygiene and stable harness policy belong to `CLAUDE.md` unless the stage changes them.
10. **Evidence-first reports.** Result, baseline/final, commits, key decisions, test totals, PASS/FAIL evidence, artifacts, deviations/debt, exactly one next step.
11. **Screenshot sets are risk-based** — normally 6–10 views.
12. **Lock semantics, not arbitrary implementation details.**
13. **Do not design the next stage inside the current prompt.** Preserve only the seam needed now.
14. **Size target ~1,800–2,500 words.** This is the single size metric; the old "150–350 lines" phrasing is retired.
15. **Prefer ~15–30 meaningful test IDs**, and never more than 30 (9.2 R-2).
16. **Every paragraph must change execution, verification, safety or reporting.** Otherwise delete it.
17. **Current repo truth outranks prompt boilerplate.** A prompt is a stage delta.
18. **Avoid coding-agent token taxes.** No wholesale large-document reads, no re-explaining settled decisions.

Additionally, since v3.69.0, every prompt must carry its documentation obligations explicitly:
- read `CLAUDE.md` first;
- read the current-summary/top of `PROJECT_STATUS.md`;
- heading-map based selective reading of the relevant `ARCHITECTURE.md` / `PRODUCT.md` sections;
- mandatory current-truth update of `PROJECT_STATUS.md` when the stage completes technically;
- mandatory `ARCHITECTURE.md` update for implemented invariants;
- `PRODUCT.md` update only where current user-facing behaviour text would otherwise become false, and never claiming owner visual acceptance before the review;
- no `README.md`/`CLAUDE.md` churn unless their owned truth actually changes;
- concise responsibility/invariant comments in touched production code where ownership is non-obvious;
- a final report section `Documentation delta`: file → heading → truth changed / no-change reason.

New in v4.0.0:
- a **layout-touching prompt must reference an accepted mock** and restate its numbers (4.2). A layout prompt without numbers is not publishable;
- a prompt must declare its **work type** (`SPIKE` or production stage) and its **round budget** (9.2 R-1, R-6);
- a prompt must declare its **test gate tier** (9.2 R-5) rather than defaulting to the full suite.

Before publishing, run the redundancy pass: same rule twice? historical context not needed for execution? acceptance repeating tests? walkthrough repeating tests? stable repo rule copied unnecessarily? future feature described beyond the seam needed now? implementation over-prescribed before live inspection? If yes, compact before delivery.

### 2.7 Report requirements

A coding-agent report must contain: result; baseline and final commit; diff/files; implementation delta; commands and tests run; a `Test ID | PASS/FAIL | short evidence` table; deviations and debt; documentation delta if current truth changed; exactly one next step.

A report is not accepted until its verdict is written into the report archive in `HISTORY`. A stage cannot be `COMPLETE` in section 1 without a corresponding archive entry.

### 2.8 Pre-prompt consistency checklist

Before generating any prompt, verify:

- section 1 matches the latest accepted report;
- no *status claim* exists outside section 1 — the rule as stated in 0.1, not the words themselves (10.3, command 4);
- roadmap 7.1, section 1 and the stage's entry in 7.2 agree;
- no completed stage is described as a future blocker;
- current production technologies are not listed as unverified candidates;
- each durable fact has exactly one owner (2.10);
- the prompt is a delta against repository docs, not a copy of this plan;
- large repo docs are routed through introduction + headings + task-relevant ranges;
- no owner-gated action is silently authorised;
- WIP limits in 9.1 permit a new prompt at all.

### 2.9 Planning guardrails

- live repository truth outranks stale historical text here;
- completed stages must not remain described as blockers;
- missing report attribution is never reconstructed from memory;
- owner-approved direction remains binding even if a roadmap section omitted it;
- technical PASS is not final visual acceptance for major UI work;
- long documents may exceed 2000 lines; duplication and poor navigability are the problem, not raw line count;
- before every prompt, reconcile repo status, active decisions and roadmap dependencies.

### 2.10 Documentation ownership — one fact, one owner

| Durable fact | Owner |
|---|---|
| current technical baseline / stage result | `PROJECT_STATUS.md` |
| architecture and invariants | `ARCHITECTURE.md` |
| accepted product behaviour | `PRODUCT.md` |
| Claude working rules for the repo | `CLAUDE.md` |
| build/run/test commands | `README.md` |
| `.forge` format | `DATA_PACKAGE_SPEC.md`, once the format exists |
| roadmap, orchestration, coordinator policy | this MASTER PLAN |
| owner decisions | `FORGESHAPE_OWNER_DECISIONS` |
| version history and report archive | `FORGESHAPE_MASTER_PLAN_HISTORY` |

Inside this plan set the same rule applies, and v4.0.0 enforces it. These policies now have exactly one home each:

| Policy | Canonical home | Everywhere else |
|---|---|---|
| selective reading of large documents | 2.11 | one-line pointer |
| naming / terminology / code comments | 2.12 | pointer (DOC-OWNER-01 keeps the owner's approval, not a second copy of the rules) |
| premium UI quality | 4.3 | pointer |
| visual acceptance procedure | 4.4 | pointer |
| prompt authoring | 2.6 | pointer |
| execution order | 7.1 | nowhere else — 7.2 describes scope only, with no order and no status |

Do not copy full agent reports into multiple documents.

### 2.11 Selective reading of large documents — canonical

Core repository documents may exceed 2000 physical lines when durable truth requires it. **There is no project-wide hard line cap.** The quality rule is: avoid stage diaries and duplication; keep a current-state summary near the top; keep predictable headings; make long documents selectively readable.

For every non-trivial coding-agent task:
1. read `CLAUDE.md`;
2. read the top/current-state portion of `PROJECT_STATUS.md`;
3. for any large `ARCHITECTURE.md` / `PRODUCT.md` / `PROJECT_STATUS.md`, inspect the introduction and heading map first;
4. locate relevant headings with a line-numbered heading search (`grep -n '^#' <file>`);
5. read only the line ranges that own the task's contracts;
6. expand to adjacent sections only when dependencies require it.

Never ask an agent to read an entire large document by default. Never store fixed line numbers in another document — derive them at task start. A whole document may be read when the task itself is a whole-document audit or when cross-cutting architecture genuinely requires it.

Owner approval for this policy: DOC-OWNER-02.

### 2.12 Naming, terminology and code comments — canonical

Owner approval: DOC-OWNER-01.

**Owner-facing language.** Plain descriptive name first, project shorthand in parentheses afterwards: `główny interfejs edytora (Editor Workspace UI)`, `pierwsza kompletna wersja funkcjonalna (MVP)`, `etap kontrolny (gate)`, `panel właściwości (Inspector)`.

**Code naming.** Names explain responsibility without project-history knowledge. Prefer `EditorWorkspaceView`, `GlobalToolbarView`, `ToolRailView`, `PropertyInspectorView`, `NumericPropertyRow`, `ConstructionModeController`, `SculptModeController`, `SketchModeController`, `SketchPlane`, `SketchProfile`, `ExtrudeFeature`, `SceneObject`, `ConstructionBody`, `PointerSample`. Avoid `ForgeShellManager`, `FeatureLayer`, `StuffController`, `GenericToolHost`, `Handler2`, and names derived only from stage numbers. Stage numbers belong in reports and history, not in production type names — and, per 0.4, not in prose either: write "the alignment-guide work" rather than "the Stage 020S design space".

**Domain terminology — one term per concept:** `Construction Mode`, `Sculpt Mode`, `Sketch Mode`, `Property Inspector`, `Tool Rail`, `Global Toolbar`, `Editor Workspace`, `Construction Body`.

Two terminology defects inherited from v3.69.0, now resolved:
- **the right-hand surface had five names** (`Tool Rail`, "right contextual rail", "trailing cluster", "right host", `WorkspaceTrailingHostView`). Canonical from v4.0.0: the user-facing concept is the **right contextual rail**; the implementing class is `WorkspaceTrailingHostView`; `Tool Rail` remains the name for edge tool selection only. No other synonym is permitted in new prompts;
- **`Frozen Sculpt Mesh` / `Freeze`** remains a legitimate *internal* term for the legacy data path, and is forbidden in user-facing copy (UI-OWNER-27). It disappears entirely when BRIDGE-R1 lands.

**Comments in code.** Required for non-obvious architectural intent and invariants: why a responsibility is native rather than Android UI; why a field is authoritative rather than derived; why a gesture is consumed or cancelled; why a layout branch exists; why an operation is deferred; why a platform-neutral boundary exists; why a destructive action requires confirmation. Not required for restating syntax. Bad: `// Increment revision`. Good: `// Transform-only edits must not change MeshRevision because the local RuntimeMesh is unchanged.`

For non-trivial core/public types and functions document responsibility, ownership/lifetime where non-obvious, input/output units, important invariants, and thread/lifecycle constraints.

When old names are superseded: do not mass-rename working code; rename only when touching that area or when ambiguity creates real maintenance risk; update tests and docs together; never perform a broad cosmetic rename inside an unrelated stage.

### 2.13 Owner communication

When presenting a decision: plain-language name → one sentence on what it means → the concrete visible effect in the app → your recommendation → shorthand last, if useful.

When rejecting work, express the rejection **as a delta against a specification** — "the rail is 88 dp and the spec says 72", "zone B has the wrong owner" — never as "still doesn't feel premium". An unfalsifiable rejection cannot be acted on and produces another round. This is a rule for both sides of the conversation and the coordinator is responsible for enforcing it.

---

## 3. PRODUCT

### 3.1 North star

ForgeShape is an offline native Android 3D authoring application with two closely related but distinct authoring workspaces:

`Exact CAD-like Construction Project`
→ `save/version Construction source`
→ `Create Sculpt Copy` (one-way project derivation)
→ `mobile Sculpt Project`
→ `UV`
→ `game-engine export`

A project may also start directly as Sculpt.

The long-term product is **not** a reversible "CAD mode ↔ Sculpt mode" over one mutable representation. Construction/CAD remains an editable source document; Sculpt is a derived polygonal authoring document. To edit CAD after sculpting: reopen and edit the saved Construction project, then create a new Sculpt copy. The Sculpt project does not expose `Back to Construction` as an editing-mode switch once the one-way bridge exists.

It is not intended to be a full CAD suite, a Blender replacement, or a literal Nomad/Sculpt+ UI clone.

### 3.2 Differentiation and success conditions

**Construction:** exact editable primitive parameters; exact dimensions/transforms; direct gizmo manipulation with precision values as a secondary path; hierarchy, snapping, mirror, booleans and operation history; iterative Sketch → Extrude → Revolve feature authoring; durable project persistence before one-way Sculpt derivation.

**Sculpt:** viewport-first mobile workflow; derived polygonal scene independent from the saved Construction project; direct touch/stylus deformation; seven MVP brushes; later Subdivide/multires-style density controls and remesh/topology tools; no requirement to retain an in-session editable CAD representation.

The application is successful only if all four are true:
1. Construction is exact and predictable.
2. Transform and tool interaction is direct enough for phone/tablet use.
3. Sculpt interaction is immediate enough to feel like a mobile sculptor.
4. Crossing from Construction to Sculpt never destroys the saved editable Construction project.

Note, carried from the 2026-08-28 councils and relevant to every scope argument: **none of these four conditions mentions booleans or UV.** Any claim that booleans or automatic UV must be in v1 has to be argued against this list, not assumed.

### 3.3 Product thesis — OPEN, owner decision required

The plan has never contained a persona, a named competitor, a price, a store category, or a sentence explaining why a user picks ForgeShape over an alternative they already have. Section 3.2 answers "what does it do", not "why would anyone choose it".

The thesis proposed by the councils, to be **written down or refuted** by the owner (`OQ-C` in 8.3):

> The only real defensible gap is that **nothing of the Shapr3D class exists on Android.** Nomad Sculpt is on Android, mature, cheap and excellent — ForgeShape cannot win at sculpting and does not need to. Exact, dimensional, stylus-driven modelling on an Android tablet is a real gap: prop and modular-kit blockouts where dimensions matter, made on a tablet rather than at a desk.

If that is the thesis, **CAD is the product and Sculpt is the companion**, and the execution order must reflect it. If it is not the thesis, the owner must write the one that is — because without it, every ordering decision is a guess. This is deliberately recorded as an unanswered question, not as an approved decision.

### 3.4 Explicit MVP scope

The following is the **full approved MVP scope**, unchanged from v3.69.0 §16. It is preserved in full. What v4.0.0 adds is an explicit warning attached to it, and a proposed tiering in 3.4.2 that the owner has not yet accepted.

#### 3.4.1 Approved MVP scope — unchanged

**Construction:** project; mm/cm/m; viewport/camera/select; box, cube, cylinder, sphere, cone, capsule, plane; exact dimensions/position/rotation/scale; full transform gizmo Move/Rotate/Scale; World/Local Move + Rotate; local-only Scale; planar Move XY/XZ/YZ; planar Scale XY/XZ/YZ + uniform Scale; Surface Snap; hierarchy; duplicate/delete/hide/lock/group; grid snap; Construction Mirror/Symmetry; standard Sketch planes; Line/Polyline; Rectangle; Circle; select/delete Sketch entities; exact supported Sketch dimensions; closed-profile detection; Extrude New Body; repeated Sketch → Extrude; Sketch on supported planar faces/work planes; Extrude Add/Cut once boolean infrastructure exists; Revolve around an explicit sketch axis/edge; Union/Subtract/Intersect; operation tree/history; Undo/Redo; versioned `.forge` Construction/Sculpt project save/load; one-way `Create Sculpt Copy` after durable Construction save; autosave; crash recovery.

**Sculpt:** exactly seven brushes — Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask. Also Radius/Strength; stylus pressure if available; Sculpt Stroke X Symmetry; Subdivide/multires-style density increase; simple remesh/voxel-style topology rebuild; Sculpt Undo/Redo + compact history navigator (initial five-action experiment); isolate/hide; preview.

**UV:** seams; auto charts; pack; preview; overlap; margin; target resolution; approximate texel density; regenerate.

**Export:** GLB/glTF; OBJ; scale/units; pivot; normals/smoothing; UV; basic materials; names; hierarchy where supported; optional merge; triangulation; counts. **Approved interoperability direction (ARCH-OWNER-06):** standard interchange is a separate subsystem from `.forge`; FBX and other deliberately selected common formats may be added in later stages after format-specific feasibility/licensing/dependency review. This approval does not pull FBX or broad format support into the current MVP sequence.

#### 3.4.2 Feasibility warning attached to that scope

Combined with 5.7 ("no third-party geometry/UV/import runtime dependency is currently accepted"), the scope above requires writing **three separate geometry engines** from scratch:

| Component | Independent expert estimate |
|---|---|
| Triangle-mesh boolean kernel (Stage 024) | 4–9 months of one person's work, without a robustness guarantee. Requires exact geometric predicates (adaptive arithmetic; float32 is insufficient), coplanar-face handling, self-intersection handling, snap-rounding, inside/outside classification, constrained retriangulation. Only realistic library candidate: **Manifold** (Apache-2.0, C++17, self-contained). |
| Automatic UV unwrap + packing (Stages 029–032) | 3–6 months. Segmentation + parameterization (LSCM/ABF++/SLIM — a sparse solver of your own) + packing (NP-hard). Only realistic library candidate: **xatlas** (MIT, dependency-free). |
| Voxel remesh / Subdivide (Stage 028) | Feasible in-house: dual contouring / surface nets over an SDF field is a 2–3 week prototype. Real risk is memory — 512³ = 134M cells; without a sparse structure this OOMs on 8 GB phones. |

Whole-scope estimate for one person: **18–36 months**, and the spread comes almost entirely from those three kernels.

#### 3.4.3 Proposed release tiering — PROPOSED, not approved (`OQ-B`)

| Release | Scope |
|---|---|
| **v1** | six primitives + gizmo (exists) → `Stage 018A` (rename/hide/lock/delete/duplicate) → **`.forge` save/load + autosave + crash recovery** → Sketch on three world planes + Extrude New Body (no Add/Cut, no Revolve) → seven brushes + Sculpt undo + X symmetry + Subdivide → **GLB export** (no OBJ) → **no automatic UV** (export preserves procedural primitive UVs; sculpted meshes get an honest "unwrap in Blender" message) |
| **v1.1** | booleans (with a library after audit, or not at all), Revolve, Extrude Add/Cut, remesh, OBJ, `020D/020M/020S1/020S2`, `UI-PREF`, automatic UV |
| **v2** | cloud/hub, `Stage 018B/C`, import, texel density/packing, multires, custom layout, five palettes, iOS/desktop |

Nothing above is deleted from the MVP by this table. It is a sequencing proposal awaiting `OQ-B`.

### 3.5 Explicit non-goals

Unless the owner changes scope: full CAD sketcher / general constraint solver beyond the bounded Sketch + Extrude + Revolve layer; assemblies; NURBS; 2D drafting; physics; advanced rendering / ray tracing; full texture painting; procedural material system; Blender-class retopology; rigging/animation/bones; advanced baking; marketplace/social/cloud/multiplayer; AI model generation; plugin ecosystem; **broad universal-format support in one milestone**; FBX in the first export milestone. ARCH-OWNER-06 approves staged standard interoperability, not a universal importer/exporter.

Import is owner-resolved as post-MVP — see 3.6.

### 3.6 Import — staged; bounded durable GLB slice now OWNER-authorized

**OWNER DECISION 2026-08-19:** broad/durable import is post-MVP.

**OWNER EXCEPTION 2026-08-31 (`ARCH-OWNER-08`):** before `GATE-E2E`, ForgeShape may implement a session-only **GLB Imported Mesh Preview** solely to diagnose the current ForgeShape-export → external-import discrepancy.

**OWNER EXTENSION 2026-08-31 (`ARCH-OWNER-09`):** that same non-persistent preview may be widened one step to open a real external static low-poly GLB using node matrix/TRS, multiple triangle primitives, generated normals when NORMAL is absent, safe ignored vertex attributes and double-sided preview culling. This remains diagnostic compatibility, not production import.

`ARCH-OWNER-10` (2026-09-01) explicitly changes the timing for the **first durable static GLB slice only**. `IMPORT-01A` may now create real Imported Mesh project objects with list/selection/gizmo/persistence. The former post-MVP timing continues to govern broad/high-density import scaling and other formats.

Still out of scope for `IMPORT-01A`:
- broad arbitrary GLB/OBJ importer;
- Assimp or broad importer dependency;
- reconstruction of Construction Source from polygon files;
- imported-mesh Sculpt/vertex editing;
- materials/textures/animation/skinning;
- million-triangle capability claims without the readiness evidence below.

`GLB-IMPORT-R0` is now COMPLETE on reported roundtrip-equivalent evidence and may:
- read one binary GLB selected through SAF;
- parse only the static embedded-buffer triangle subset ForgeShape itself emits;
- display it as a temporary `Imported Mesh Preview`;
- switch Source ↔ Imported preview for visual diagnosis;
- compute independent world-space roundtrip comparisons;
- clear the preview without touching project truth.


`GLB-IMPORT-R1` may additionally:
- parse a static external node `matrix` or TRS and bake it into preview geometry;
- accept multiple TRIANGLES primitives;
- generate deterministic vertex normals if NORMAL is absent;
- validate-but-ignore COLOR/TEXCOORD attributes;
- respect material `doubleSided` for preview culling;
- ignore extras metadata;
- expose a real external `Import GLB…` SAF action;
- remain session-only, non-editable and excluded from all project persistence.


`IMPORT-01A` is the OWNER-authorized bounded durable slice:
- reuse the R1 static parser subset;
- each supported top-level mesh node becomes one durable `Imported Mesh` object;
- primitives remain internal draw batches;
- real ObjectId / Objects-list selection / gizmo Move-Rotate-Scale / Undo-Redo;
- self-contained `.forge` persistence of non-regenerable mesh truth;
- no fake Construction Source;
- no Sculpt yet.

`IMPORT-01B` is the planned next semantic step after 01A acceptance: allow an Imported Mesh to enter Sculpt without pretending it came from parametric Construction. It is not authorized for implementation in 01A.

**Broad/high-density import readiness gate** — scaling the importer beyond this bounded functional slice, claiming million-triangle capability, or opening the general production import ceiling still requires:

1. final MVP regression is stable;
2. physical ARM64 device testing exists;
3. controlled high-density mesh tests have been run;
4. testing intentionally reaches the million-triangle range on capable reference hardware;
5. memory, viewport, picking, lifecycle and Sculpt behaviour are understood;
6. an evidence-based supported ceiling is documented.

The owner's target includes models with multiple millions of triangles. That is a **capability target to validate**, not an accepted performance guarantee.

`IMPORT-01A` begins durable GLB object import now under ARCH-OWNER-10. OBJ may follow only in a later authorized stage; FBX and other standard formats still require their own feasibility/licensing/dependency decision. Durable imported polygon geometry is a scene mesh object / `Imported Mesh`; it never pretends to have parametric Construction Source. Any future importer dependency still requires Android build/ABI, 16 KB, license/security/size and performance/memory audits.

Durable import should converge on `Objects + → Add from file` inside the same creation surface (UI-OWNER-25). `IMPORT-01A` may initially reuse the existing Project `Import GLB…` entry if moving the action would create unrelated UI work; object semantics, not menu relocation, are the acceptance boundary for this stage.


---

## 4. UX / UI

The binding UX decisions themselves live in `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md`. This section owns only the *process and specification* contracts that govern how UI work is defined and accepted.

### 4.1 Workspace principles — pointers, not copies

| Principle | Owner decision |
|---|---|
| viewport-first Editor Workspace, floating/contextual chrome | UI-OWNER-01, 20, 23 |
| one unified expandable right contextual rail; bottom surfaces never relocate unrelated chrome; IME never changes rail grammar | **UI-OWNER-44** |
| stable default anchors before customization | UI-OWNER-36 |
| static layout accepted before any motion work | **UI-OWNER-43** |
| adaptive compact / short / expanded composition | UI-OWNER-02, 17 |
| 48 dp minimum interactive hit area | UI-OWNER-28 |
| border discipline, tonal hierarchy | UI-OWNER-21 |
| selective translucency | UI-OWNER-15 |
| motion language (deferred) | UI-OWNER-16 |
| appearance system: three dark palettes now, five as the target | UI-OWNER-22 (values), UI-OWNER-42 (count) |

Preserve from work already accepted, in every future UI stage: native/domain transform semantics; Construction history; Sculpt correctness; surface exclusivity; System Back behaviour; exact numeric precision and signed-leading visibility; replace/select editing; unitless Scale; pinned/reachable Apply; ≥48 dp interaction protection; the improved gizmo baseline; current reduced-motion and interruption behaviour.

### 4.2 `UI-SPEC-R0` — the layout specification requirement

**New in v4.0.0 and binding for every layout-touching stage from now on.**

#### Why

Five consecutive UI rounds were rejected. The decisive evidence is v3.62.0: *"the existing `UILR1-15` stability test encoded a weaker contract than the owner intended."* The agent implemented and tested exactly what was written; what was written was not what was meant. That is the definition of a missing specification, not an execution defect.

Mechanically confirmed: in all of v3.69.0, the only durable dimensional value describing the interface was `48 dp`. The one design artifact that ever existed (`WF-1..WF-4`, commit `b0561ab`) was superseded, and DOC-R2 deleted `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md`, rescuing two invariants from it. The project deliberately traded drawings for prose. **Prose does not measure.**

#### The artifact

One deliverable: a **layout mock** — a static offline file (single HTML or SVG), at 1:1 scale in the owner's device dp (e.g. 411×914 dp for 1080×2340), opened **on that same phone**, in portrait, next to the build. Zero production code, zero tests, zero commits under `app/src/main`.

It must contain:

1. **A zone table**: zone → owner → priority → collision rule (who hides, who shrinks, who never yields).
2. **Numbers**: rail width; safe margins; radius family; spacing scale (4/8/12/16/24); type scale; z-order.
3. **Every state as its own frame**: resting → Shape/Transform → Move/Rotate/Scale → World/Local → Exact → Exact+IME → close; Sculpt resting → Details → close.
4. **Three configurations**: compact portrait, short landscape, expanded — at font scale 1.0 and 1.3.
5. **A load variant** (4.5) with placeholders for the tenants that do not exist yet.

#### Measurable definition of done for layout

Replaces "does it feel premium" with tests:

1. every state in the matrix has a **declared rectangle** for every persistent element;
2. `Δ resting bounds = 0 dp` for every element outside the surface being opened — on open *and* on close;
3. `top`, `right` and `width` of the right contextual rail are **identical across all states within each configuration/font-scale cell**. `UI-SPEC-R0` may define different fixed values for different responsive cells; Revision 1 shares width/right across all six cells and varies only the accepted top by window class;
4. **overlay diff**: device screenshot laid over the accepted mock; anchor deviation **≤ 4 dp**;
5. **≥48 dp** on every target in every state — not only at rest. The P0 finding from UI-AUDIT2A was precisely the constrained-height case.

Points 2–5 are automatable with the existing `semantic-id/runtime-bounds` harness. This is what turns 4.3 from an opinion into a test.

#### Round limit: two

Round 1 implements to the mock. Round 2 corrects **measured deviations from the mock**. If a third round is needed, that is proof the **mock** is wrong, not the code: stop coding and go back to the mock.

#### Screenshots or video?

Video is the right tool for **motion, gesture and timing** — UI-OWNER-43 points 5–6 are unchanged there. For **static layout** video is the wrong tool: you cannot measure 4 dp from it, and you can tire of it and reject the whole thing on feel. Static layout is reviewed as **a fixed screenshot set plus the mock overlay**: two images side by side, not 119 seconds of recording.

### 4.3 Premium UI / motion quality — canonical

Target: **CALM + SPATIAL + DYNAMIC + RESPONSIVE + PREMIUM** (UI-OWNER-14). A technically clean set of rounded rectangles is not sufficient.

Premium ForgeShape UI should: make the viewport feel continuous and primary; use floating/contextual chrome rather than conventional app bars where practical; use a small number of clear surface levels instead of visible borders around everything; distinguish primary commit actions, selection states, editable fields and tertiary controls by role; size panels to useful content rather than available screen height; keep adaptive layouts compositionally intentional, never a stretched phone or an empty desktop column; preserve icon-language continuity across compact/short/expanded; make Sculpt edge controls feel authored as part of the same system; preserve the three approved dark palettes as distinct tonal systems; use accent sparingly; keep exact numeric work more legible than decorative material effects.

Reference quality — a bar, never a cloning target: Apple/iOS for proportion, hierarchy, materials and transition precision; Flighty for spatial continuity and microinteraction quality; Nomad Sculpt for viewport-first creative-tool density. Never copy branded layouts, icons or assets.

Border discipline: borders are for focus, edit affordance, selected state and accessibility. General hierarchy comes from spacing, tonal surfaces, typography, elevation, radius and restrained accent.

Expanded/tablet: extra width exposes useful simultaneous context, but persistent surfaces stay content-aware. A mostly empty full-height Objects/Inspector slab is not a premium use of large-screen space.

**Constraint added in v4.0.0:** this section may not be used as an acceptance gate on its own. Every stage that will be judged against it must first have the measurable contract from 4.2. Otherwise it is an unfalsifiable gate — and an unfalsifiable gate produced five rejections in nine days.

### 4.4 Visual acceptance procedure — canonical

Owner approval: UI-OWNER-19. Automated PASS is necessary but never sufficient for major UI work.

1. the agent completes implementation and automated/runtime verification;
2. the defined screenshot set is captured across the relevant configurations and themes;
3. for a layout stage, the overlay diff against the accepted mock is produced (4.2);
4. the owner supplies screenshots (and video where motion/gesture/timing is in scope);
5. the coordinator evaluates composition, spacing, hierarchy, typography, iconography, material/depth, viewport dominance and visual noise **against the specification**;
6. if quality is insufficient, create one focused correction stage — never silently call the UI final;
7. rejection is expressed as a delta against the specification (2.13);
8. maximum two correction rounds per defect (9.2 R-4). "Good enough" is a valid verdict, recorded as `ACCEPTED-AS-IS` with the residue in 8.2.

Required walkthrough for the current workspace acceptance: compact portrait continuous walkthrough — resting → Shape/Transform → Move/Rotate/Scale → World/Local → Exact → IME → close; bottom controls before/during/after the sheet; Sculpt resting → Details → close; short landscape; expanded/tablet.

No motion or morph stage begins until the static layout is visually accepted (UI-OWNER-43).

### 4.5 The empty-box risk and the load variant

You cannot finish the layout of a tool whose noun is incomplete. The app has no save, no export, no hierarchy, no multi-select, no Sketch mode, no operation tree, no UV, no project hub. **Each of those is a future tenant of the same screen** — and the largest, Sketch, has an unknown control vocabulary because `CAD-R0-UX` deliberately defers that research.

The plan reserves *when*, not *where*: UI-OWNER-38 reserves the top-left for the app/file menu, UI-OWNER-37 for the settings gear, UI-OWNER-39 for Home, `UI-LAYOUT-CUSTOM` defers personalization. **A reservation in prose is not a reservation in pixels.**

Cheap mitigation, mandatory inside `UI-SPEC-R0`: the mock includes a **load variant** — the same frames with placeholders for a file bar + gear in the top-left, an Objects list with hierarchy and multi-select checkboxes, a Sketch rail (tools + dimensions + constraints), an operation tree, a UV tab and an export screen. **If one fixed-width right rail cannot hold the Sketch vocabulary, you learn it now instead of after `CAD-A1`.** That is one extra mock page, not a stage.

### 4.6 UX items with no home in v3.69.0 — now scheduled

Mechanically verified against v3.69.0: the strings `onboarding`, `tutorial`, `empty state`, `first-run`, `error message`, `sample project`, `user test`, `beta tester` and `backup` appeared **zero** times in 3612 lines.

| Missing item | Now owned by |
|---|---|
| first run / onboarding / empty state | `UI-SPEC-R0` as a "no project" frame; implementation in `E2E-R1` / `APP-H1` |
| sample projects (2–3 `.forge` files shipped in the APK) — doubles as a format regression fixture | `E2E-R1` |
| error and failure messages: out of memory, corrupt file, file from a newer version, export failed, no space | `E2E-R1` and `Stage 039` |
| units and scale beyond `mm/cm/m`: what one scene unit means on export (glTF is metres), scale presets, default primitive size, units in Sculpt, inches, Y-up/Z-up conversion | owner decision inside `E2E-R1` |
| a "just let me sculpt something quickly" path — starting mesh list (sphere, cylinder, blockout) | `SCULPT-H1` / `Stage 025` |
| Android file storage: Scoped Storage/SAF, where `.forge` lives, sharing, surviving uninstall, "share GLB" | `E2E-R1` |
| accessibility as a standing DoD item, not an audit finding (font scale 1.3, contrast) | 4.2 DoD |
| handedness and thumb reach — the whole layout assumes the right side | `UI-SPEC-R0` constraint (mirrored zones) + `UI-PREF1`; recorded as `OQ-05` |
| landscape as a composition, not merely a test configuration | its own mock frame |
| gesture ownership vs chrome: which edges belong to the viewport, which to surfaces, what System Back does in every state | `UI-SPEC-R0` |
| progress and cancellation language for long operations | `E2E-R1` and `Stage 039` |
| release: name, icon, store listing, privacy policy, pricing model | scheduled alongside `GATE-USER-01` |

---

## 5. ARCHITECTURE

### 5.1 Domain authority

Geometry, Construction/CAD, Sculpt, history, project serialization and core transform math remain platform-neutral C++. No Android View/Activity/JNI type becomes domain truth. JNI is an Android adapter, never the domain API. Android platform code stays an adapter/shell around platform-neutral product state. Input crossing into native code trends toward platform-neutral semantic pointer/tool data. Domain values, geometry and history never become Java layout truth.

Owner decisions: ARCH-OWNER-01, ARCH-OWNER-02.

### 5.2 Android UI boundary

The Editor Workspace source must stay simple enough that a layout change does not require coordinated edits across unrelated classes: a clear composition root; stable child-surface hosts; one state/visibility coordinator for mutually exclusive contextual surfaces; one adaptive-layout policy owner; one inset/IME policy owner; child Views render their own content and expose semantic state/actions but never reposition unrelated siblings; layout reproducible from current semantic state rather than hidden mutation history; shared shell components between Construction and Sculpt; responsibility-based names and concise invariant comments; no speculative generic UI frameworks.

Owner decision: ARCH-OWNER-05. Realized by `UI-ARCH-R1` — `WorkspaceTrailingHostView` now owns right-cluster composition and presentation.

### 5.3 Rendering and portability seam

Camera/scene/render data are platform-neutral. Android surface/context creation stays at the platform boundary. Shader/material concepts do not depend on Android UI classes. The Android renderer may keep evolving without a premature backend rewrite; the future Apple/desktop backend choice is a dedicated spike (`PORT-R0`), not a decision frozen now.

Owner decision: ARCH-OWNER-03.

### 5.4 Persistence and the document-model question

`.forge` is a versioned, portable, endian/ABI/platform-independent ForgeShape project format. No Android filesystem path, Java serialization type, native struct image, pointer, `size_t`, renderer cache or GPU resource may become project truth. Exact decision record: `FORGESHAPE_CAD_R0_DATA_DECISION_v3.md` (supersedes v1 only by adding ARCH-OWNER-06 portability/interchange clarification).

**Construction document model:** declarative semantic feature graph. It is not a saved render-mesh snapshot, a chronological UI-event log or the current Undo/Redo stack. The primitive-only body is the degenerate first case: one `PrimitiveSource` feature with stable per-body identity `(ObjectId, LocalFeatureId=1)` plus separate body placement. Future Sketch/Extrude/Revolve/Boolean nodes extend the graph rather than replacing v1 with a different document concept. The current domain's six remembered primitive parameter sets are preserved as body state because kind-switch memory is existing behaviour.

**Representation boundary:** Construction semantic features/parameters are canonical; evaluated triangle meshes are derived. Future CAD features that require persistent faces/edges evaluate through a topology-capable B-rep-style boundary, but kernel-private B-rep bytes are not `.forge` v1 truth and no kernel is selected here. Sculpt is intentionally different: the editable local Sculpt mesh positions + index topology are project truth; normals, adjacency, renderer geometry, mesh revisions and GPU resources are derived.

**Numeric contract:** Construction/CAD authored lengths and position are binary64 metres; rotation is binary64 degrees and preserves noncanonical authored values; Scale is positive binary64 unitless. Current Sculpt local positions are preserved as float32. The render/tessellation path may remain float32 derived data. Any future exact-kernel arithmetic is an evaluator implementation detail rather than a change to project numeric truth.

**Persistent topology identity:** future face/edge/vertex references use `TopoRef { producerFeature=(ObjectId,LocalFeatureId), entityKind, semanticToken, lineageToken }`. Primitive features expose deterministic semantic names where possible; topology-changing features provide generated/modified/deleted lineage. Zero/multiple resolution is an explicit broken/ambiguous reference and never silently retargets to the nearest geometry. A render triangle index is never CAD face identity.

**`.forge` v1 envelope:** ForgeShape-owned chunked binary, no new dependency. File magic `FORGESH1`; explicit little-endian scalars; 28-byte versioned file header; 24-byte FourCC section headers with payload length and CRC-32. `SCNE` owns project-neutral ordered body ids/placement; `CONS` owns Construction semantic source data; `SCUL` owns Sculpt mesh truth. Unknown optional sections are skipped; unknown required sections, incompatible major versions, invalid lengths/CRC/flags or semantic values fail explicitly. Decode/validate into temporary state, then atomically replace the live project; failed load mutates nothing. Writer output is deterministic.

**History:** session `ConstructionHistory` is not serialized. Future CAD feature history is document truth because it defines the model. A successful load starts a fresh session undo history; a failed load preserves the existing one.

**Migration honesty:** there is no pre-v1 production `.forge`. v1 creates permanent golden fixtures and a version-dispatch seam; no fake v0 migration is claimed. The first real schema bump must retain v1 fixtures and add a real older-version compatibility/migration test.

**Platform boundary:** E2E-R1A may use one Android app-private durable slot through a narrow adapter, while the codec/document layer stays platform-neutral. Scoped Storage/SAF, Save As, project library, autosave and recovery belong to later stages.

**Cross-device / interchange boundary (ARCH-OWNER-06):** `.forge` is the native, self-contained project document exchanged between ForgeShape installations. It may omit derived data when the receiving compatible ForgeShape version can deterministically reconstruct the exact project semantics; it must carry the canonical data that cannot be reconstructed (for current Sculpt, the editable float32 mesh positions/topology and required mesh properties). A `.forge` file must not depend on local filesystem paths, display geometry, Android identifiers, ABI, pointer width, CPU endianness, GPU resources or renderer caches. Standard formats such as GLB/glTF, OBJ, FBX and future explicitly approved formats are **interchange**, not project truth; their import/export pipelines are staged separately and may be representation-lossy. Stage 021 implements none of them.


**Kernel decision remains evidence-gated.** `CAD-R0-DATA` defines candidate families and `GATE-KERNEL` criteria but does not answer `OQ-A`. Candidate families include OCCT, Parasolid, Spatial ACIS, Manifold and CGAL. Adoption of any external dependency still requires the separate license/build/ABI/16 KB/performance/maintenance decision in 5.7.

### 5.5 Cross-platform target

Target client families: Android phone/tablet; iPhone/iPad; macOS; Windows. "Same app" means the same product model and project format, the same account/project library, the same geometry/CAD/Sculpt semantics, a closely related responsive UI language, and platform-appropriate pointer/stylus/keyboard/window behaviour. It does **not** require one UI binary or one widget toolkit. Apple UI may be native Apple UI; sharing the Android View layer is not a goal.

Not required now: iOS/iPadOS app, Xcode project, Metal backend, MoltenVK dependency, shared cross-platform UI framework.

Owner decisions: ARCH-OWNER-01, ARCH-OWNER-02, ARCH-OWNER-03.

### 5.6 Cloud

Local-first. Dependency order is binding: (1) make `.forge` a stable, versioned, portable local format; (2) add local project-library semantics; (3) only then account/auth and cloud sync around that portable model; (4) only later consider collaborative editing.

Cloud is infrastructure, not geometry truth: native domain state must not depend on a vendor SDK; backend choice is deferred to `CLOUD-R0`; store project metadata/revisions separately from large blobs where useful; use a local cache plus explicit revisions; never silently merge two independently edited geometry graphs — the first cloud version detects conflict and forks/preserves rather than corrupting.

Account UX: local/no-account use remains possible; sign-in enables sync and the cross-device library; Google Sign-In is desired; Apple-client auth must be rechecked against then-current App Store rules — as of 2026-08, guideline 4.8 requires an equivalent privacy-preserving login alongside a primary third-party login unless an exception applies.

Owner decision: ARCH-OWNER-04.

### 5.7 Dependency policy

Current runtime stack: Android platform APIs; Android NDK native runtime; Vulkan `libvulkan`; `libandroid`; `liblog`; the C/C++ runtime; ForgeShape-authored code.

**No third-party geometry/UV/import runtime dependency is currently accepted.**

Any future geometry/UV/import library requires a dedicated decision covering necessity, license, Android ABI/build integration, APK size, 16 KB compatibility, performance/memory, and maintenance/security. Exact build-tool versions belong to the repository, not here. Durable facts: the stable NDK r29 migration is complete; Vulkan/JNI/CMake are established technology, not candidates; no dependency or build-system churn belongs in an unrelated feature stage.

**Open owner decision `OQ-A`:** whether this zero-dependency rule also governs the boolean kernel and UV unwrap. "Yes" costs an estimated 12–18 extra months (3.4.2). The only realistic candidates are `Manifold` (Apache-2.0) and `xatlas` (MIT). This should be settled by the evidence from `GATE-KERNEL`, not by declaration.

---

## 6. QUALITY AND VERIFICATION

### 6.1 Device safety — durable

Reserved and prohibited: `emulator-5554`. Never probe, query, clean, stop or otherwise contact it.

Required: every adb call uses an explicit `adb -s <serial>`; never bare `adb devices`; never an unscoped `connected*AndroidTest` fan-out; emulator launch uses an explicit safe port from the current repo-approved workflow; verify AVD identity with the emulator/AVD identity command rather than assuming a port identifies the intended AVD.

Latest verified safe runtime evidence: ForgeShape-owned `ForgeShape_Stage006`, explicit port 5580 / `emulator-5580`, identity confirmed by `adb -s emulator-5580 emu avd name`. This is evidence, not permission to assume that emulator is always available — every device stage reads the current repo device instructions first.

Known environment issue: a corrupted Quickboot snapshot can leave QEMU/console alive while the guest never boots and adbd never connects. Do not destructively repair snapshots/userdata without task need and approval.

This discipline has already paid for itself: one violation cost two remediation stages (`Stage 016-R`, `016-R2`).

### 6.2 Git

Local Git is active. **No remote and no push unless the owner separately approves** — and 1.6 asks the owner to approve exactly that, today, because the absence of an off-machine copy is currently the project's largest unmitigated risk.

Every implementation stage begins from a clean tree with baseline/status/diff checks, uses focused commits, and reports baseline and final commit IDs. Do not recreate historical inventories that Git already owns. Do not invent owner identity/config.

### 6.3 Toolchain

Exact build-tool versions are owned by the live repository. Durable constraint: NDK r29 migration is complete; Android/NDK/Vulkan/JNI/CMake remain the production stack; future dependency changes require explicit justification and the relevant ABI / 16 KB / license checks.

### 6.4 Test policy and proportional gates

What is worth its cost and stays unchanged:
- native self-tests (grown 1695 → 1809 → 1961 → 2043) for transforms, history and brush metrics — seconds to run, and they protect the C++ core that is destined for porting; they caught the world-space rotation bug;
- the 59 JVM tests — trivial cost;
- the device-safety rules in 6.1;
- video-based acceptance for motion and interaction (UI-OWNER-43).

What was rigour without return, and is now bounded:
- four independent UI audits (`UI-AUDIT2A/2B/2C/2D`) — four stages, SKILL-PROOF artifacts with SHA-256, 49 screenshots, 10 measurement records; 2D's conclusions never reached the coordinator; net product effect: **zero**. Audits are now limited by 9.1 W-7;
- full 300-test instrumented runs on stages with `PRODUCT_DIFF_COUNT=0`;
- 300 tests for purely layout stages — they do not measure what gets rejected (`UILR1-15` is the proof).

**Proportional test gate — binding (9.2 R-5):**

| Diff touches | Required verification |
|---|---|
| native / domain / history / input | full run: native + JVM 59 + instrumented 300 |
| layout / presentation only | JVM + native + a named instrumented subset (~40 IDs) + video or screenshot set |
| `PRODUCT_DIFF_COUNT = 0` | build + smoke only |

The authoritative full-suite definition remains TEST-OWNER-01: an exhaustive-sharded instrumented run counts as a full-suite PASS only when the runner itself proves complete discovery, deterministic non-overlapping partitioning, zero omissions and duplicates, and PASS for every shard. Any assertion failure, instrumentation abort or missing result fails the aggregate, and infrastructure recovery reruns the whole sharded job.

Observation to keep in mind when setting gates: **rigour is currently inversely correlated with risk.** The most heavily verified area is layout — reversible and cheap to fix. The least verified are persistence and export — the only places where a user's work can be destroyed irreversibly. `E2E-R1` must not inherit a lighter gate than a rail-width correction.

### 6.5 Visual acceptance

Owned by 4.4. Automated layout tests cannot certify visual quality; and, symmetrically, from v4.0.0 an unfalsifiable aesthetic gate cannot block a stage that met its measured contract (4.2).

### 6.6 Gate P0 — COMPLETE

Accepted: local Git initialized and baseline committed; stable NDK r29 pinned; full established regression passing under r29; native ELF + APK 16 KB static/package checks passing; normal runtime smoke passing. The old "runtime 16 KB UNVERIFIED" note was closed by dedicated real 16 KB runtime evidence during Gate P1. Do not repeat or reopen without a new compatibility reason.

### 6.7 Gate P1 — COMPLETE

Accepted at `159af08`, covering:

- **P1-A physical ARM64** — build/install/launch on a real ARM64 device with an explicit approved serial; Vulkan viewport, lifecycle, orientation; Perspective/Orthographic; Studio/MatCap; Construction solid + Plane, picking and transforms; Freeze/Resume plus real Sculpt strokes; multitouch smoke.
- **P1-B real 16 KB runtime** — environment reporting `PAGE_SIZE=16384`, native load, Vulkan viewport and basic Construction/Sculpt interaction. Never silently install a large 16 KB system image; report the required image/device and wait for approval.
- **P1-C Vulkan validation** — bounded debug/testing validation path, no production runtime dependency; swapchain lifecycle, orientation, mesh upload, shading, picking-related rendering, Construction/Sculpt transition; no ForgeShape-caused validation ERROR in the exercised scope; warnings classified, not ignored.
- **P1-D stylus smoke** — remains explicitly **UNVERIFIED**; real compatible stylus hardware/input was unavailable. Never simulate finger input and call it stylus proof. This gap is now owned by `STYLUS-G1` (7.2), which must run before Stage 026 pressure work.
- **P1-E mesh-density ladder** — 10k / 50k / 100k measured on the physical device, with 250k and 500k allowed only while memory/stability remain safe. Measured per tier: source/render vertex/index/triangle counts; mesh generation/publication; render-normal build; upload time/capacity growth/reuse; frame behaviour; picking latency; Sculpt stroke CPU time; memory footprint; lifecycle/recreate stability; thermal symptoms. Escalation stops on crash/OOM/severe instability, reporting the last stable and first degraded tier. The measured ceiling is never a marketing limit.
- **P1-F acceptance** — physical ARM64 PASS, real 16 KB runtime PASS, Vulkan validation PASS, mesh-density baseline PASS, regression green, no product feature expansion.

`PERF-BASELINE` (7.2) extends the P1-E ladder into a repeatable benchmark with regression thresholds, which P1 deliberately did not provide.

---

## 7. ROADMAP

**This section is the single owner of execution order.** Section 7.2 describes stage *scope* only — no order, no status. Section 1 is the only place that says what is happening now.

### 7.1 Binding execution order — v4.0.0

Owner-approved on 2026-08-29. This replaces the v3.69.0 protected roadmap, which is preserved for traceability in 7.4.

**The reason for the change, in one sentence:** persistence sat at position 13 and export at ~32, so the project had spent eleven stages polishing the interface of an application that cannot keep anything the user makes — and the absence of saving is what sustains the UI rejection loop, because it removes every feedback channel except the screenshot.

| # | ID | What | Executor | Status | Why here |
|---:|---|---|---|---|---|
| 0 | `BACKUP-01` | Private remote repository + push of the code + this plan set under Git. **Do this today.** | `OWNER` | `READY` | Outside the stage queue. Risk R1; already materialised once on the plan itself. |
| 1 | `UI-SPEC-R0` | Offline 1:1 layout mock with zone table, numbers, all states, 3 configurations, 2 font scales, load variant. No production code. | `OWNER` | `COMPLETE` | Revision 1 accepted by owner delegation: edge-hugging 72 dp rail, 8 dp inset, tops 112/88/108 dp; former wide R0 retained only as measurement history. |
| 2 | `UI-LAYOUT-R2` | Implement the accepted static geometry on `WorkspaceTrailingHostView` to the mock. Max 2 rounds. | `AGENT` | `COMPLETE` | Correction round 1 technical PASS + coordinator visual PASS on the owner-review bundle; accepted edge rail remains 68 dp runtime within 72±4, right=8 dp without state drift. Round 2 retired unused. |
| — | — | **owner/coordinator static visual acceptance → UI MORATORIUM** | `OWNER` | — | **PASSED / ACTIVE.** No further UI stage may run without a shipped product feature between them. |
| 3 | `CAD-R0-DATA` | Research gate: semantic document/feature graph, Construction-vs-Sculpt truth, precision, topological identity, `.forge` envelope and `GATE-KERNEL` criteria/candidates. Does **not** select the kernel or answer `OQ-A`. | `COORDINATOR` | `COMPLETE` | Decision v2 retains the v1 persistence model and adds the owner-approved rule that `.forge` is self-contained cross-device project truth while standard interchange formats remain separate staged pipelines. |
| 4a | `E2E-R1A` | `Stage 021` — `.forge` save/load v1, stable file corpus, corruption/version compatibility tests and process-death reopen proof. | `AGENT` | `COMPLETE` | Reported product/test PASS at `a52be246`; owner explicitly waived the stale background-task cleanup because those jobs are already finished. |
| 4b | `E2E-R1B` | `Stage 022` — autosave, crash recovery, Scoped Storage, load/save error messages, offline diagnostics. | `AGENT` | `COMPLETE` | Reported PASS at final short HEAD `678af45`; FullSharded 382/382, native 2199. Full 40-char final hash was omitted from the report and must be resolved at the next preflight. |
| 4c | `E2E-R1C` | `Stage 023` — early GLB export vertical slice, the units and up-axis decision, 2 sample projects in the APK, export error messages. | `AGENT` | `COMPLETE` | ARCH-OWNER-07 correction PASS reported at `868995d`; node R/S identity, R/S baked into geometry, 406/406 FullSharded. |
| 4d | `GLB-IMPORT-R0` | Bounded diagnostic GLB import preview: current ForgeShape-export subset only, session-only Imported Mesh Preview, Source↔Imported comparison, no project persistence/editing. | `AGENT` | `COMPLETE` | Reported roundtrip-equivalent at `27ec0bc`; max world-position delta `0.0 m`. Does not replace post-MVP `IMPORT-01`. |
| 4e | `GLB-IMPORT-R1` | External static low-poly GLB preview compatibility: node matrix/TRS, multiple primitives, generated normals, safe ignored attributes, double-sided culling, real Import GLB SAF flow. | `AGENT` | `COMPLETE` | Technical PASS at `504f97d`; OWNER then confirmed the real low-poly GLB opens normally. |
| 4f | `IMPORT-01A` | Durable Imported Mesh object: real ObjectId, Objects-list selection, gizmo transforms, Undo/Redo, self-contained `.forge` persistence for the current static GLB subset. | `AGENT` | `COMPLETE` | Technical PASS plus OWNER real-file durable import/gizmo/Save→reopen acceptance. |
| 4g | `IMPORT-01B` | Imported Mesh reversible Sculpt + real SceneObject Delete. | `AGENT` | `OPEN` | Technical PASS; OWNER real-file Import/Sculpt PASS; only manual Delete/Undo/Redo confirmation remains. Sculpt Undo now exists separately under SCULPT-UNDO-R0. |
| 4h | `SCULPT-UNDO-R0` | Dedicated bounded runtime Sculpt Undo/Redo history, separate from project history, shared by Construction and Imported Sculpt. | `AGENT` | `COMPLETE` | Technical PASS at `2bcfa3fa11fd96fa934887f16e55caefbee496d9`; OWNER confirmed real-device Undo/Redo. |
| — | `GATE-E2E` | Pass/fail checkpoint over persistence/export/import diagnostics: internal roundtrip is equivalent; R1 technical compatibility is green; OWNER external-engine/export verdict remains. | `OWNER` | `OPEN` | 7.3 |
| 5 | `DEVICE-M1` | Vulkan driver matrix across three GPU families (Adreno / Mali / Xclipse). | `AGENT` | `BLOCKED-BY:GATE-E2E` | Driver diversity found here costs days; found at `Stage 036` it costs a release. |
| 6 | `GATE-USER-01` | Three people from outside the project, 30 minutes, "make and export a simple shape", unaided. | `OWNER` | `GATE` | Replaces the fifth UI iteration with the only feedback that is not the author's own taste. |
| 7 | `SCULPT-H1` | Remaining Sculpt history navigator UX only; core Sculpt Undo/Redo was delivered early by SCULPT-UNDO-R0. | `AGENT` | `READY` | Do not reimplement the stroke history; decide later whether a visible navigator beyond Undo/Redo is still worth MVP scope. |
| 8 | `Stage 025` | Flatten, Crease, Mask. | `AGENT` | `BLOCKED-BY:SCULPT-H1` | Only after undo exists. |
| 9 | `Stage 018A` | Rename, visibility, lock, duplicate; generic Delete is delivered early under UI-OWNER-45 / IMPORT-01B. | `AGENT` | `COMPLETE-IN-BASELINE` | Direct completion report not ingested in this chat; functionality is referenced by the clean Stage 020M baseline. | OWNER sequencing override; reduced-test policy TEST-OWNER-03. | Do not reimplement Delete here unless later requirements expand its semantics. |
| 10 | `PERF-BASELINE` | Repeatable benchmark with regression thresholds; GPU memory budget and `onTrimMemory` policy. | `AGENT` | `PLANNED` | The plan forbids optimizing without evidence and had no mechanism to collect it. |
| 11 | `SPATIAL-R1` | Spatial acceleration structure (BVH/octree) plus a threading and long-operation cancellation model. | `AGENT` | `BLOCKED-BY:PERF-BASELINE` | Its absence blocks five later stages at once (8.2). |
| 12 | `BRIDGE-R1` | One-way Create Sculpt Copy; remove the legacy Freeze/Resume path. | `AGENT` | `BLOCKED-BY:GATE-E2E` | Requires durable persistence to be data-safe (UI-OWNER-24). |
| 13 | `CAD-R0-UX` | Fresh mobile CAD interaction research + owner direction, recorded in `FORGESHAPE_CAD_R0_UX_DECISION_v1.md`. | `COORDINATOR` | `COMPLETE` | Direct spatial world-plane/planar-face selection; viewport-first mobile adaptation; adopt/adapt/defer/reject recorded. |
| 14 | `CAD-A1` → `CAD-A2` | Sketch foundation; Extrude New Body. | `AGENT` | `COMPLETE` | Delivered by CAD-R0-A1A2 at `3a00e0be6c528c9f96ae1f004bdaf70bc67afb82`; 491/491 FullSharded. |
| 14b | `CAD-A3` | APP-H1 mobile Home/project entry + direct spatial world-plane/planar-face Sketch support + repeated Sketch/Extrude New Body with semantic TopoRef persistence. | `AGENT` | `COMPLETE` | Technical COMPLETE at `c5864c69902be127a12daa5ec81e86b0d07627bb`; FullSharded 504/504; OWNER real-device retest pending. |
| 14c | `CAD-A3-C1` | Close Home/New Project bootstrap, independent CADB-v2 corpus, real device E2E and visual evidence without reopening the green face-sketch core. | `AGENT` | `COMPLETE` | PASS at `c5864c69902be127a12daa5ec81e86b0d07627bb`; transient bootstrap, independent v2 corpus, device matrix and visual evidence complete. |
| 14d | `CAD-A3-C2` | Full-screen Home + immediate flat first Sketch + top-right orientation navigator + on-canvas line dimensions + Arc/Spline + staged Edit Sketch. | `AGENT` | `COMPLETE` | Technical PASS at `be5b729c`; FullSharded 519/519; OWNER real-device retest pending. |
| 14e | `DEEP-AUDIT-R1` | Holistic code/product/data/UI/build/test/docs audit + comment compression + current-truth reconciliation. | `AGENT` | `COMPLETE` | PASS WITH DEBT; three P1 fixed; FullSharded 519/519; current-truth/docs/comment audit complete. |
| 14f | `POST-AUDIT-HARDEN-R1` | Bounded post-audit hardening: F-08 JNI sculpt-reader synchronization, F-16 release selftest exclusion, F-11 exception ordering, F-15/F-18 tiny dead/linkage cleanup. | `AGENT` | `COMPLETE` | PASS at `5cf981a9c89da8e99366592cb77b0f1cdff1c361`; FullSharded 524/524; final tree clean. |
| 15 | `STYLUS-G1` → `Stage 026` | Physical stylus gate; then X symmetry and pressure. | `AGENT` | `PLANNED` | Pressure, palm and cancel semantics must not be discovered in final beta hardening. |
| 16 | `GATE-PERF-SCULPT` → `Stage 028` | Numeric sculpt performance target; then Subdivide and voxel remesh. | `AGENT` | `BLOCKED-BY:SPATIAL-R1` | Subdivide must not be discovered to exhaust memory. |
| 17 | `Stage 020M` | Body Dimensions + Relative Scale (Construction). | `AGENT` | `COMPLETE` | Technical/repository complete at `94daaa9`; OWNER LATER. | Approved and preserved — but worth less than the ability to save a file. |
| 17 | `SCULPT-DIM-01` | Overall bounds dimensions on a Sculpt body (UI-OWNER-33C), split out of `Stage 020M` in v4.0.2 because it has its own blocking question and one ID may not carry two statuses. | `AGENT` | `BLOCKED-BY:OQ-02` | History ownership of a Body-level edit made while Sculpt is active must be defined first. |
| 17 | `MIRROR-01` | Construction Mirror/Symmetry. | `AGENT` | `COMPLETE` | R1 bounded to current Construction primitives across XY/XZ/YZ world planes; no negative persisted scale. | In MVP scope since the beginning, without a stage until v4.0.0. |
| 17 | `UI-PREF1` | Settings Hub, persistent preferences, stable default anchors. | `AGENT` | `COMPLETE` | Also delivers the persistent-settings layer that handedness (`OQ-05`) needs. |
| 17a | `UI-PREF-R1` | Combined execution of UI-PREF1 + UI-PREF2: Settings Hub, persistent handedness, five palettes, gizmo appearance preferences. | `AGENT` | `COMPLETE` | OWNER sequencing override while Delete verdict is parked; Opus 5 High; no Delete/history semantics changes. |
| 17b | `UI-PREF-R1-CLOSEOUT` | Preserve dirty UI-PREF-R1 candidate, run only focused post-race verification, record OWNER FullSharded waiver, commit and clean tree. | `AGENT` | `COMPLETE` | No full aggregate; no new feature behavior. |
| 17c | `TEST-RUNTIME-R1` | FullSharded runner runtime hardening: shard-only/resume, per-shard timing, bounded attempts/timeouts, infra/product classification. | `AGENT` | `COMPLETE` | Implements TEST-OWNER-02 after product tree is clean. |
| 17 | `UI-PREF2` | Gizmo appearance preferences; the five-palette target. | `AGENT` | `COMPLETE` | No architectural cost; can land at any time. |
| 17 | `Stage 018C` | Parent/child hierarchy, drag/drop/reparent. | `AGENT` | `PLANNED` | Convenience for scenes nobody can build yet. |
| 17 | `SEL-OUT-R1` | Selection Outline renderer stage. | `AGENT` | `COMPLETE-IN-BASELINE` | OWNER LATER visual review; no final FullSharded claim retained. | Required by UI-OWNER-10/11 and unscheduled until v4.0.0. |
| 17d | `SEL-OUT-R1-CLOSEOUT` | Historical proposed closeout for prior dirty outline candidate. | `AGENT` | `CANCELLED-BY-OWNER` | Superseded by later clean baseline; no longer actionable. | No FullSharded; follows TEST-OWNER-03. |
| 18 | `Stage 020D` | Directional Scale — symmetric vs one-sided. | `AGENT` | `BLOCKED-BY:OQ-01` | Anchor, planar, multi-selection and stickiness semantics are unanswered. |
| 18 | `Stage 020S1` | Contact / Body Magnet Snap. | `AGENT` | `BLOCKED-BY:SPATIAL-R1` | Semantics settled by UI-OWNER-34; what it lacks is a spatial structure. |
| 18 | `Stage 020S2` | Alignment Guides + magnetic alignment. | `AGENT` | `BLOCKED-BY:SPATIAL-R1` | Also `BLOCKED-BY:OQ-03`; see 7.5 for both relations. |
| 18 | `GRID-SNAP-01` | Construction Grid Snap. | `AGENT` | `BLOCKED-BY:OQ-06` | Increment/unit contract is unanswered. |
| 18 | `Stage 018B` | Multi-select and group transform. | `AGENT` | `BLOCKED-BY:OQ-04` | Group pivot and orientation contract is unanswered. |
| — | `GATE-KERNEL` | Evidence spike over `Stage 024` and Extrude Add/Cut only: robust/deterministic Boolean corpus incl. coplanar/tangent/sliver/chained cases, topology lineage, Android/16 KB/portability/license checks; canonical box−cylinder coplanar case <200 ms on reference physical ARM64. | `AGENT` | `GATE` | Candidate families: OCCT, Parasolid, ACIS, Manifold, CGAL; none pre-approved. FAIL forces owner `OQ-A`: qualifying dependency or cut booleans/Add-Cut from MVP. |
| 19 | `Stage 027`, remesh completion, `Stages 029–032`, `Stage 033`, `Stage 034`, `Stage 035` | Sculpt workflow completion, UV MVP, final GLB, OBJ, engine validation. | `AGENT` | `PLANNED` |
| 19 | `Stage 024`, `CAD-A4` | Booleans and operation tree; Extrude Add/Cut and Revolve. | `AGENT` | `BLOCKED-BY:GATE-KERNEL` | The two largest technical risks — booleans and UV — must not stand on the critical path to the first working export. |
| 20 | `Stages 036–040` | Device matrix, stylus hardening, performance and memory, long operations, full MVP regression and beta hardening. | `AGENT` | `PLANNED` | Hardening comes after the feature set is closed. |
| 21 | `APP-H1` | Project/App hub and home flow. | `AGENT` | `COMPLETE` | Technical implementation delivered by CAD-A3-C1 at `c5864c69902be127a12daa5ec81e86b0d07627bb`; OWNER Home/New/Open retest pending. |
| 21 | `UI-LAYOUT-CUSTOM` | User-customizable workspace. | `AGENT` | `PLANNED` | Follows stable defaults and preference persistence. |
| 21 | `CLOUD-R0` | Account/cloud architecture research. | `COORDINATOR` | `BLOCKED-BY:E2E-R1A` | ARCH-OWNER-04 requires a stable local format first. |
| 21 | `CLOUD-H1` | Cross-device project library and sync. | `AGENT` | `BLOCKED-BY:CLOUD-R0` | Syncing projects nobody wants yet is the most expensive possible way to deliver zero value. |
| 21 | `PORT-R0` | Cross-platform architecture spike. | `COORDINATOR` | `PLANNED` | Required before any first Apple/desktop client, whenever that is scheduled. |
| 22 | `IMPORT-01-SCALE` | Broad/high-density import hardening, supported mesh ceiling, later-format expansion after the bounded 01A/01B slices. | `AGENT` | `BLOCKED-BY:Stage 040` | Physical-device/high-density readiness gate still controls million-triangle claims and broad import scale. |

Movements and their one-line justifications:

- **`020D/020M/020S1/020S2` down:** four stages improving how you nudge a block the user cannot save; `020S1/020S2` are additionally blocked by the missing spatial structure.
- **`UI-PREF` down:** appearance preferences carry no architectural cost and can be added at any time; they also need a persistent-settings layer that arrives more cheaply alongside `.forge`.
- **`SCULPT-H1` up:** see position 6.
- **`018A` up, `018B/C` down:** delete/rename blocks real sessions; multi-select and hierarchy drag-and-drop are conveniences for scenes nobody can build yet.
- **cloud/hub below CAD:** see position 15.
- **`CAD-R0`/`CAD-A1`/`CAD-A2` up by roughly thirteen positions**, and split into `-DATA` (now) and `-UX` (later).
- **booleans and UV last:** the two biggest technical risks and the only ones that may genuinely require a library against 5.7.

Explicitly unchanged by this reordering: **no owner decision is cancelled.** Every decision that was approved remains approved; several are simply executed later. UI-OWNER-31's phrase "the next transform-placement capability" is historical wording, not an ordering instruction — order lives only here.

### 7.2 Stage catalogue — scope only

*No status and no order in this section. Status is in 7.1 and section 1.*

#### 7.2.1 Completed foundations

| ID | Scope |
|---|---|
| `Gate P0` | Git baseline, NDK r29, 16 KB static/package proof. See 6.6. |
| `Gate P1` | Physical ARM64, real 16 KB runtime, Vulkan validation, mesh-density ladder. See 6.7. Stylus remains UNVERIFIED. |
| `Stage 016` | Exact Plane plus completion of the MVP primitive family (Box, Cylinder, Sphere, Cone, Capsule, Plane); one coherent typed primitive system; shared tessellation constants, finite/bounds/winding helpers and validation conventions. Explicitly not: property reflection framework, generic CAD feature system, booleans, broad scene architecture. |
| `Stage 017` | Multi-object scene foundation: stable ObjectIds, ordered root scene, renderer/picking iterating the scene, per-body source/mesh state. |
| `INPUT-R1` | Finger/stylus pointer identity transported through the native input boundary. |
| `Stage 019` | Native Construction transaction/history foundation with begin/update/commit/cancel, capacity 64, domain-state snapshots, atomic Exact Shape/Transform/Add operations, stable ObjectId restore, multi-object replay, redo invalidation. Sculpt history is separate. |
| `Stage 020R2` | Full Construction transform gizmo: Move X/Y/Z, plane Move XY/XZ/YZ, Rotate X/Y/Z, Scale X/Y/Z, planar Scale, uniform Scale, World/Local for Move+Rotate, Local-only Scale, exact numeric Position/Rotation/Scale, one drag = one history step. UE5 is an interaction reference, not a pixel-copy target. |
| `Stage 020R3` | World/display brush metric corrected under non-uniform Body Scale; Clay/Inflate normal deposition corrected; no Scale baking. |
| `UI-R2`, `UI-R3/R3R2`, `UI-R4/R4A/R4B/R4C` | The workspace redesign program that established the accepted viewport-first mobile workspace. |
| `UI-ARCH-AUDIT1`, `UI-ARCH-R1` | Read-only source audit, then the bounded structural extraction of `WorkspaceTrailingHostView`. |
| `UI-AUDIT1`, `UI-AUDIT2A/2B/2C` | Independent design/motion audits. `UI-AUDIT2D` (Mobbin) was cancelled by the owner. |
| `DOC-R1`, `DOC-R2` | Documentation reconciliation and compaction. |
| `TEST-HARNESS-R1` | Runner-owned exhaustive-sharded instrumented full-suite mode. |

#### 7.2.2 UI program

| ID | Scope |
|---|---|
| `UI-SPEC-R0` | **New in v4.0.0.** The layout mock and its zone/number contract. Full definition in 4.2. Deliverable is a drawing, not a build. |
| `UI-LAYOUT-R2` | One right contextual surface per mode with fixed top, fixed right, fixed width and one radius/padding family, expanding downward only; no detached child tile may remain for controls that semantically belong to that context; Construction Transform content (Shape/Transform, Move/Rotate/Scale, applicable World/Local, exact precision access) lives inside that same surface; Sculpt uses the same external host grammar for brush/tool controls and its contextual actions. For Exact Transform and Sculpt Details: opening a bottom surface must not move Body/Objects/history controls upward — conflicting controls hide or suspend, closing restores exact resting bounds, and no temporary alternate coordinates are used. With the numeric keyboard visible the right host keeps its fixed top/right/width and stays vertical, with no detached horizontal selector bar; available-height pressure is absorbed inside the precision surface and/or by hiding non-essential host content in place. Static parity before motion: no new morph, transition or animated reflow in this stage. Full contract: UI-OWNER-44. Numbers come from `UI-SPEC-R0`. |
| `UI-PREF1` | Settings Hub and default UI stability: stable default anchors, gear-owned persistent settings. UI-OWNER-37. |
| `UI-PREF2` | Appearance preferences: gizmo size/thickness/style, the five-palette target. UI-OWNER-32, UI-OWNER-42. |
| `UI-LAYOUT-CUSTOM` | Future user-customizable workspace. Follows stable defaults plus preference persistence; slot vs free-drag and per-orientation semantics remain owner-gated. UI-OWNER-36. |
| `SEL-OUT-R1` | **New node in v4.0.0.** Selection Outline as a renderer stage. It has been required product direction since UI-OWNER-10/11 but had no stage; cheap Android/View-layer approximations do not satisfy the contract. |
| `APP-H1` | Project/App hub and home flow: top-left ForgeShape/app menu, Settings route, New/Open/Save/Import/Export exposed only when each capability is real, no-project Home flow. UI-OWNER-38, UI-OWNER-39. |

#### 7.2.3 Construction transform and placement

| ID | Scope |
|---|---|
| `Stage 020D` | Directional Scale — symmetric vs one-sided. Current pivot-centered scaling stays as one mode; one-sided scaling uses six directional handles and anchors the opposite side. Exact compound Position+Scale semantics need owner confirmation first (UI-OWNER-33, `OQ-01`). |
| `Stage 020M` | Body Dimensions + Relative Scale: no dense gizmo while Dimensions mode is active; local oriented X/Y/Z bounds lines with exact labels; exact overall dimension updates Absolute Scale; per-axis `− side / center / + side` anchor; Relative Scale starts at `(1,1,1)` each activation and multiplies stored Absolute Scale; Sculpt may later expose overall bounds dimensions; shares one resize/anchor solver with Directional Scale. UI-OWNER-33A/33B/33C. |
| `Stage 020S1` | Contact / Body Magnet Snap for Move and Scale: surface/bounds-aware contact, visible contact cue, transaction-safe, following Directional Scale semantics. UI-OWNER-31, UI-OWNER-34. |
| `Stage 020S2` | Alignment Guides + magnetic alignment: dashed guides for non-contact alignment, Move/Scale first, Rotate semantics owner-gated, distinct from contact snap and grid snap. UI-OWNER-35, `OQ-03`. |
| `GRID-SNAP-01` | **New node in v4.0.0.** Construction Grid Snap. It is inside the approved MVP scope (3.4.1) but v3.69.0 contained only the sentence "Grid Snap remains separate" and no owning stage. Requires an increment/unit contract before implementation; the quantization hook was deliberately left clean in Stage 020. |
| `MIRROR-01` | **New node in v4.0.0.** Construction Mirror/Symmetry. Also inside the approved MVP scope, also without a stage until now. Must not be conflated with Sculpt Stroke X Symmetry (UI-OWNER-26). |

#### 7.2.4 Objects, scene and hierarchy

| ID | Scope |
|---|---|
| `Stage 018A` | Row commands: rename, visibility on/off (hidden objects are not viewport-pickable), lock/unlock, delete/duplicate. |
| `Stage 018B` | Multi-select plus group transform. One clearly defined group pivot/space contract must exist before coding (`OQ-04`). |
| `Stage 018C` | Parent/child hierarchy, drag/drop/reparent, descendant selection and transform propagation. Hierarchy is scene truth (UI-OWNER-12, UI-OWNER-40). |

#### 7.2.5 Persistence, bridge and the end-to-end slice

| ID | Scope |
|---|---|
| `E2E-R1` | **New milestone in v4.0.0**, not a single stage. It contains three agent stages — `E2E-R1A`, `E2E-R1B`, `E2E-R1C` — each within the one-day / two-round budget of 9.2 R-6, sharing one acceptance criterion (7.3 `GATE-E2E`). The milestone is what `GATE-E2E` and `BRIDGE-R1` depend on; the sub-stages are what get prompts. |
| `E2E-R1A` | `Stage 021` `.forge` save/load v1 plus permanent golden corpus, corruption/version tests and process-death reopen proof. There is no fake historical v0 migration. |
| `E2E-R1B` | `Stage 022` autosave, crash recovery, Scoped Storage, load/save error messages, offline diagnostics. |
| `E2E-R1C` | `Stage 023` early GLB export, the units and up-axis decision, two sample projects in the APK, export error messages. |
| `Stage 021` (inside `E2E-R1A`) | Portable `.forge` v1 governed by 5.4: `FORGESH1` chunked binary; `SCNE` + conditional `CONS`/`SCUL`; semantic Construction feature/source truth with all remembered primitive parameters and exact transforms; current Sculpt local mesh/topology truth; fail-closed temporary decode → atomic apply; deterministic corpus; minimal app-private Save/Open only. Session undo/render/GPU caches are not serialized. |
| `Stage 022` (inside `E2E-R1B`) | Autosave and crash recovery for both project kinds. Extended in v4.0.0 with: `VK_ERROR_DEVICE_LOST` and GPU resource loss/rebuild, background/split-screen/PiP/foldable behaviour, and offline diagnostics — a local log ring buffer, a crash report written next to the project, and a manual "send report" path. |
| `Stage 023` (inside `E2E-R1C`) | Early GLB export vertical slice using current project truth, not coupled to the legacy Freeze/Resume model. |
| `BRIDGE-R1` | Construction Project → Sculpt Copy: replace user-facing `Freeze` wording with `Sculpt from CAD` / `Create Sculpt Copy`; require a durable Construction snapshot before derivation; create a separate Sculpt project copy; no `Back to Construction` mode inside it; regenerating creates a new copy rather than silently destroying an existing sculpt project. Initial semantics preserve eligible CAD bodies as separate derived sculpt objects; automatic union/remesh waits for topology infrastructure. UI-OWNER-24, UI-OWNER-27. |

#### 7.2.6 Sculpt

| ID | Scope |
|---|---|
| `SCULPT-H1` | Separate Sculpt history owner: initial 5-action capacity, Undo/Redo plus a compact scrollable history navigator, branch invalidation on a new action after undo, and stylus-hover preview of a history entry where hover hardware exists. Construction Stage 019 history is never reused as mutable Sculpt mesh history. UI-OWNER-41. **Implementation must report from day one:** bytes per history state, peak memory, snapshot/restore time, and behaviour on a heavier mesh. |
| `Stage 025` | Flatten, Crease, Mask. |
| `Stage 026` | Sculpt Stroke X Symmetry; stylus pressure where available. |
| `Stage 027` | Isolate/hide, mesh preview, and remaining non-history Sculpt workflow completion. |
| `Stage 028` | Subdivide / multires-style density increase **and** simple remesh / voxel-style topology rebuild, kept as distinct operations. Dynamic topology stays later and owner-gated. Includes a higher-density performance re-check and the decision on partial/async GPU update work. |
| MVP brush set | Grab, Clay, Smooth, Flatten, Inflate, Crease, Mask — exactly seven. |

#### 7.2.7 CAD

| ID | Scope |
|---|---|
| `CAD-R0-DATA` | Data-model decision scope: semantic feature graph; project-neutral scene vs Construction/Sculpt representation truth; binary64 Construction / float32 Sculpt mesh boundary; stable feature-lineage topology references; deterministic chunked `.forge`; and `GATE-KERNEL` criteria/candidate families. It never selects the boolean kernel or answers owner `OQ-A`. Exact record: `FORGESHAPE_CAD_R0_DATA_DECISION_v1.md`. |
| `CAD-R0-UX` | The interaction half, deliberately run **when CAD implementation is about to begin**, not months earlier as a frozen specification. Re-check the then-current Shapr3D tablet modelling space, sketch UI, Items Manager, History, adaptive selection UI, gizmo behaviour, snapping/guides, construction planes and exact numeric interaction; compare against ForgeShape's then-current workspace; use it as an interaction reference only, never pixel-copying branding, iconography, layout or terminology; separate useful ideas from heavy CAD scope; explicitly record **adopt / adapt / defer / reject** before coding. Then run the owner questionnaire (below) before writing the `CAD-A1` prompt. |
| `CAD-A1` | Sketch foundation: standard XY/XZ/YZ planes; Line/Polyline, Rectangle, Circle; exact supported dimensions; grid/snap; select/delete; closed-profile validation; stylus/finger input. |
| `CAD-A2` | Extrude New Body: closed profile → exact Extrude, first operation supports `New Body`. |
| `CAD-A3` | Iterative Sketch/Extrude: repeated sketches, standard planes then supported planar faces/work planes, repeated Extrude preserving Construction feature truth. |
| `CAD-A4` | Extrude Add, Extrude Cut, Revolve around an explicit sketch axis/edge, preserving exact parameters and editable Construction feature ownership. Does not authorize Fillet/Chamfer/Shell/Loft/Patterns. |
| `Stage 024` | Booleans and operation tree: Union, Subtract, Intersect, non-destructive Construction operation history. Any third-party geometry library requires a dedicated dependency/build/license/ABI/16 KB/performance audit first — no silent dependency. |

**`CAD-R0-UX` owner questionnaire — must ask, in practical workflow terms, at least:**
1. how a sketch is started: world XY/XZ/YZ, planar body face, custom construction plane;
2. which first sketch entities are actually MVP: Line/Polyline, Rectangle, Circle, Arc/Spline, etc.;
3. which constraints are MVP, which are automatic, and whether full definition is required or only supported;
4. exact dimensions and edit-in-place labels versus a separate numeric inspector;
5. snapping/guides: grid, endpoints, midpoints, axes, projected edges, 3D guide points;
6. whether selection should expose an adaptive contextual action set (profile → Extrude, body → Transform) rather than menu traversal;
7. construction geometry needed in the first useful vertical slice;
8. feature order and scope: Extrude New Body, Add/Cut after booleans, Revolve, and whether Loft/Sweep/Offset/Fillet are later candidates or explicitly out;
9. Items/Hierarchy and feature-history presentation on phone versus tablet;
10. stylus/finger workflow, hover feedback, and direct manipulation versus exact numbers;
11. what Shapr3D behaviour is attractive but too complex for ForgeShape MVP;
12. a final owner ranking of the workflows that must feel excellent before more CAD features are added.

*Research snapshot recorded 2026-08-26, non-binding inspiration only:* Shapr3D documentation then showed a viewport-first modelling space with floating side menus, selection-adaptive tool suggestions, an Items sidebar, a History sidebar, orientation/snapping controls, sketching on world planes / planar faces / construction planes, exact dimensions, constraints and auto-constraints, and direct gizmo manipulation with precise numeric values. Candidate patterns to study later; not approved features. Do not turn reconnaissance into hidden scope.

#### 7.2.8 UV and export

| ID | Scope |
|---|---|
| `Stage 029` | Seam mark/remove data ownership. |
| `Stage 030` | Automatic charts; packing. |
| `Stage 031` | UV preview; overlap detection. |
| `Stage 032` | Margin; target resolution; approximate texel density; regenerate. |
| `Stage 033` | Final GLB/glTF: scale/units, pivot, normals/smoothing, UV, basic materials, names, hierarchy where supported, optional merge, triangulation, counts/statistics. |
| `Stage 034` | OBJ: geometry, normals, UV, material reference where appropriate, documented limitations. |
| `Stage 035` | Engine validation: automated validation and real import tests in Unreal, Unity and Godot. FBX remains later. |

Any third-party UV library requires a dependency/build/license/ABI/16 KB audit first.

#### 7.2.9 Platform, performance and input

| ID | Scope |
|---|---|
| `PERF-BASELINE` | **New in v4.0.0.** A repeatable performance benchmark with regression thresholds, extending the P1-E ladder. The plan already forbids optimizing without evidence; this is the mechanism that collects the evidence. Includes a GPU memory budget policy, `onTrimMemory`/OOM behaviour, and sub-allocation (one `VkDeviceMemory` per object exhausts `maxMemoryAllocationCount`, which is 4096 on some drivers). |
| `SPATIAL-R1` | **New in v4.0.0.** A spatial acceleration structure (BVH/octree) plus a threading and long-operation cancellation model. Linear CPU picking is tolerable in itself, but the absence of a spatial structure blocks `020S1`, `020S2`, `Stage 024`, `Stage 028` and isolate/hide simultaneously. This is a missing foundation, not evidence-driven debt. |
| `DEVICE-M1` | **New in v4.0.0.** A small Vulkan driver matrix across three different GPU families (Adreno / Mali / Xclipse) — not three phones of one brand. |
| `STYLUS-G1` | **New in v4.0.0.** A small physical stylus gate before Stage 026 pressure work: prove Android tool type is stylus, capture pressure samples and range, verify palm rejection and stroke cancellation semantics, and confirm the stylus stroke reaches the Sculpt path without touch-routing regression. Closes the `P1-D` UNVERIFIED gap so that pressure/palm/cancel behaviour is not discovered during final beta hardening. |
| `Stage 036` | Physical device matrix: weaker ARM64 phone, stronger phone where available, tablet, stylus-capable device. |
| `Stage 037` | Stylus/touch interaction hardening: pressure, palm and contact edge cases, latency observations. |
| `Stage 038` | Performance/memory: agreed mesh-density tiers, long sculpt sessions, memory pressure, thermal behaviour. Also declares the supported ceiling ("supported up to N triangles on a class-X device"), which today exists only as an import gate. |
| `Stage 039` | Long operations: progress, cancel, failure recovery. |
| `Stage 040` | Full MVP regression and beta hardening. |

#### 7.2.10 Cloud, hub and portability

| ID | Scope |
|---|---|
| `CLOUD-R0` | Account/cloud architecture research: choose the identity model and providers; re-check Apple/Google platform policies; choose backend/storage; define the local-first cache; define project revision/conflict semantics; define encryption/security/privacy expectations; estimate storage and bandwidth cost; run an owner questionnaire on guest/local mode, account requirement, version history and sync UX. |
| `CLOUD-H1` | Cross-device project library and sync: account-backed library, offline local cache, upload/download revisions, conflict-safe duplicate/fork behaviour, the same library on Android/iOS/iPadOS/macOS/Windows clients as they become available. |
| `PORT-R0` | Cross-platform architecture spike. Compare: UI strategy (native Apple/Windows shells vs a shared cross-platform UI framework); renderer backend strategy; the C++ core bridge/API; mouse/keyboard/trackpad/stylus mapping; responsive phone/tablet/desktop layout; packaging/update/store constraints. Do not migrate the current Android UI speculatively before evidence justifies it. |

### 7.3 Existential gates

The plan has always had quality gates. It has never had a gate that can **stop or redirect** the project. These can.

| Gate | When | Criterion |
|---|---|---|
| `GATE-E2E` | after `E2E-R1` + diagnostic `GLB-IMPORT-R0/R1`, before any further UI stage | Create/move/save/kill/reopen/export; internal Source→GLB→ForgeShape equivalence is proven; OWNER must also verify the real external low-poly preview and an external-engine import with correct scale/orientation. PASS/FAIL. |
| `GATE-KERNEL` | before `Stage 024` and before the Add/Cut half of `CAD-A4` — **not** before `CAD-A1`/`CAD-A2` | Dedicated in-house/candidate spike must pass the CAD-R0-DATA robustness corpus, deterministic topology/geometry, generated/modified/deleted lineage sufficient for fail-closed `TopoRef`, binary64/tolerance behaviour, Android arm64+x86_64 and 16 KB integration, portability, license/store fit, size/perf/maintenance gates; canonical coplanar `box − cylinder` <200 ms on reference physical ARM64. FAIL triggers owner `OQ-A`: adopt a qualifying dependency or cut booleans/Add-Cut from MVP. |
| `GATE-PERF-SCULPT` | before `Stage 028` | A numeric target: target vertex count and maximum stroke latency on the reference device. Without a number, "feels like a mobile sculptor" (3.2, condition 3) is untestable. |
| `GATE-USER-01` | after `GATE-E2E` | Three people from outside the project perform "make and export a simple shape" without help. Measured: completion, time, points where they got stuck. **This replaces the fifth UI iteration.** |
| `GATE-SCOPE` | **a calendar date, not a condition — the date is `OQ-E` and is not yet set. Proposed default: 2026-11-30.** Until the owner sets it, this gate is inert and must not be counted as a working control. | Review how much of 3.4.1 actually exists, and formally decide whether to trim the MVP. Without this, scope can only grow — and it did: UI-OWNER-33 through 42 were all added in a single August. |
| `GATE-VIABILITY` | owner-owned, no stage | One falsifiable sentence: who the user is and why they choose ForgeShape over Nomad, Shapr3D or Blender. Written down, with a verification date. See 3.3 / `OQ-C`. |

**Stop criteria.** Nowhere in v3.69.0 was there a condition under which the project changes course or ends. A plan without such a condition cannot lose — which means it also cannot win. `GATE-KERNEL`, `GATE-USER-01` and `GATE-SCOPE` are the three places where a genuine "no" is permitted, and a "no" there must produce a scope cut, not another correction round.

### 7.4 Superseded roadmap — v3.69.0, kept for traceability

```
UI-LAYOUT-R2 → owner visual acceptance → optional motion/morph closeout
→ 020D Directional Scale → 020M Body Dimensions + Relative Scale
→ 020S1 Contact Magnet Snap → 020S2 Alignment Guides → UI-PREF
→ 018A/B/C → SCULPT-H1 → 021 .forge → BRIDGE-R1 → cloud/project hub
→ CAD-R0 → CAD implementation → remaining Sculpt/topology
→ UV/export/hardening.
```

Defects of that sequence, recorded so the same mistake is recognisable if it recurs: `Stage 022` (autosave) and `Stage 023` (early GLB export) do not appear in it at all, although §9 of the same document defined `Stage 023` as an **early** stage placed right after autosave; `SCULPT-H1` appeared in it without existing in the roadmap section; and `UI-PREF` appeared without its `1`/`2` split. Superseded by 7.1 on 2026-08-29.

---

### 7.5 Dependency graph

Relations as explicit fields, not prose (0.4 rule 3). **This table is the only place a dependency is *recorded*.** Section 7.2 may restate a constraint in words where it helps a reader understand the scope, but only for a relation that already appears here as a field; it may never introduce one that is missing here. If 7.2 names a dependency this table lacks, this table is the defect. `Blocked-by` means it cannot start; `Needs` means it cannot be *accepted* without it.

| ID | Blocked-by | Needs | Note |
|---|---|---|---|
| `UI-SPEC-R0` | — | — | `OQ-05` is answered *inside* it, as a mirrored-zone frame. It does not block the start. |
| `UI-LAYOUT-R2` | `UI-SPEC-R0` | accepted mock numbers | **COMPLETE.** Revision 1 accepted; correction round 1 closed the L/E Exact/Details translation; all six cells within tolerance; coordinator visual PASS. Structural UI-OWNER-44 remains binding as frozen baseline. |
| `CAD-R0-DATA` | `UI-LAYOUT-R2` acceptance | — | decision v1 establishes 5.4 and `GATE-KERNEL` criteria; does not answer `OQ-A` |
| `E2E-R1A` | `CAD-R0-DATA` | document-model decision | dependency satisfied by CAD-R0-DATA decision v2; implementation must follow 5.4 and ARCH-OWNER-06 without implementing interchange formats |
| `E2E-R1B` | `E2E-R1A` | — | dependency satisfied; E2E-R1B COMPLETE |
| `E2E-R1C` | `E2E-R1A` | units/up-axis decision | |
| `GLB-IMPORT-R0` | `E2E-R1C` | ARCH-OWNER-08 diagnostic exception | COMPLETE; does not satisfy production IMPORT-01 gate |
| `GLB-IMPORT-R1` | `GLB-IMPORT-R0` | ARCH-OWNER-09 external-static preview extension | COMPLETE; OWNER real low-poly GLB opens normally |
| `IMPORT-01A` | `GLB-IMPORT-R1`, ARCH-OWNER-10 | owner early durable-import override | COMPLETE / OWNER accepted on real file |
| `IMPORT-01B` | `IMPORT-01A`, ARCH-OWNER-11, UI-OWNER-45 | technical PASS at `04f28df49...` | OWNER real-file Sculpt/Delete verdict pending |
| `GATE-E2E` | `E2E-R1A`, `E2E-R1B`, `E2E-R1C`, `GLB-IMPORT-R0`, `GLB-IMPORT-R1` | — | OPEN; OWNER allowed 01A before final verdict |
| `DEVICE-M1` | `GATE-E2E` | three GPU families | |
| `GATE-USER-01` | `GATE-E2E` | 3 external people | |
| `Stage 025` | `SCULPT-H1` | — | no new brushes before undo |
| `SPATIAL-R1` | `PERF-BASELINE` | — | needs the benchmark to prove the win |
| `BRIDGE-R1` | `GATE-E2E` | durable persistence | UI-OWNER-24 data-safety rule |
| `Stage 020S1` | `SPATIAL-R1` | — | semantics settled by UI-OWNER-34 |
| `Stage 020S2` | `SPATIAL-R1`, `OQ-03` | — | |
| `Stage 020D` | `OQ-01` | — | |
| `SCULPT-DIM-01` | `OQ-02` | — | split out of `Stage 020M` in v4.0.2; the Construction Body-dimensions core is not blocked |
| `Stage 018B` | `OQ-04` | — | group pivot contract |
| `GRID-SNAP-01` | `OQ-06` | — | increment/unit contract |
| `Stage 026` | `STYLUS-G1` | real stylus evidence | closes the P1-D gap |
| `Stage 028` | `GATE-PERF-SCULPT`, `SPATIAL-R1` | numeric latency/memory target | |
| `CAD-R0-UX` | `CAD-R0-DATA` | — | |
| `CAD-A1`, `CAD-A2` | `CAD-R0-UX` | the owner questionnaire answered | **not** `GATE-KERNEL` |
| `CAD-A3` | `CAD-A2` | topological selection from `CAD-R0-DATA` | |
| `Stage 024` | `GATE-KERNEL` | scene ownership, transaction model, persistence round trip, early export proof | |
| `CAD-A4` (Add/Cut half) | `Stage 024` | boolean infrastructure | Revolve is not blocked by booleans |
| `Stages 029–032` (UV) | — | `OQ-A` answered | may need `xatlas` |
| `Stage 035` | `Stage 033`, `Stage 034` | — | |
| `CLOUD-R0` | stable local `.forge` | — | ARCH-OWNER-04 dependency order |
| `CLOUD-H1` | `CLOUD-R0` | — | |
| `PORT-R0` | — | — | required before the first Apple/desktop client, whenever that is scheduled |
| `IMPORT-01-SCALE` | `Stage 040` | broad/high-density readiness gate in 3.6 | still required for mesh-ceiling/performance claims and broad format expansion |

---

## 8. RISKS, DEBT, OPEN QUESTIONS AND IDEA INBOX

### 8.1 Risk register

| # | Risk | Likelihood | Consequence | Early signal | Mitigation |
|---|---|---|---|---|---|
| **R1** | Single point of failure: code exists only locally (6.2), and the plan travels as `.md` files between AI conversations | ~10–15% over 24 months | total loss | **already materialised on the plan**: v3.57.0 lost its entire middle section | **Today, 30 minutes:** private remote repository + push + plan set under Git (1.6, 10.3) |
| **R2** | MVP scope far larger than the available execution capacity | >90% | the project never reaches MVP | **already visible** — zero features since `Stage 020R3` | separate "technical MVP" (primitive → save → export → imports into an engine) from "market MVP"; booleans and UV behind `GATE-KERNEL`; `OQ-B` |
| **R3** | No end-to-end proof; persistence and export were at positions 13 and ~32 | high | redesigning the domain after ~50 stages of work built on it | `Stage 023`, defined as "early", had been pushed behind everything | **mitigated by 7.1**: `CAD-R0-DATA` then `E2E-R1` then `GATE-E2E` |
| **R4** | UI acceptance loop with no definition of "done" | high | unbounded iteration | four UI audits versus zero shipped features in the same period | measurable layout contract (4.2) + two-round limit (9.2 R-4) + UI moratorium after R2 |
| **R5** | No second human: the only reviewer and user is the author | certain | cannot distinguish "unusable" from "not to my taste today" | five consecutive UI rejections | `GATE-USER-01` — three people, 20–30 minutes, the current build |

**Cross-cutting process risk:** the plan grows faster than the application. Three sections of v3.69.0 were stale relative to its own header; §23 existed to prevent exactly that and did not. The symptom is that project energy moved from producing to administering production. A hard resource limit has also already been hit once (v3.10.0: "`015C-R` INTERRUPTED by agent quota") and agent cost is nowhere budgeted.

### 8.2 Technical debt

Confirmed and remaining:

- **whole-mesh publication and GPU upload remain coarse-grained**, with Sculpt publication potentially `O(vertices)` per moved stroke sample. Invisible at 10k vertices. At 200k, one sample is roughly 6–10 MB of traffic; at 60 Hz with a dozen samples per frame that is hundreds of MB/s across unified memory, blocking CPU and GPU alike. **This is an architectural ceiling, not an optimization backlog item** — it will surface at `Stage 028` and at `BRIDGE-R1`, and the "multiple millions of triangles" target in 3.6 is unreachable with it. Reaching that target requires dirty-range upload, persistent-mapped buffers, mesh chunking and normal recomputation limited to the brush radius;
- **no spatial acceleration structure.** Linear CPU picking is survivable to roughly 50–100k triangles, but the absence of the structure simultaneously blocks `020S1`, `020S2`, `Stage 024`, `Stage 028` and isolate/hide. Now owned by `SPATIAL-R1`;
- **history is an in-memory session-scoped undo stack, not a document model.** CAD needs a feature tree/DAG with regeneration. Do not bolt a separate ad-hoc Sketch undo system onto the product, and do not assume the transaction stack becomes the feature tree. Owned by `CAD-R0-DATA`;
- **numeric precision in the core is undecided** (5.4);
- normal recompute remains coarse where the current renderer/domain path requires it;
- release-artifact retention of debug self-test code remains UNVERIFIED unless a release binary is inspected — inspect only when release-size/hardening work needs the answer;
- startup/debug self-test lifecycle remains synchronous and should not be expanded casually; do not suppress Android lifecycle/configuration semantics merely to accommodate tests;
- the renderer failure/reporting path may still permit poor black-viewport diagnostics; re-evaluate if it becomes observable rather than refactoring pre-emptively;
- Selection Outline remains non-trivial renderer work — now scheduled as `SEL-OUT-R1`;
- Objects/list update scalability should be re-evaluated only if scene sizes make current behaviour measurable;
- the current pointer transport supports stylus semantics, but pressure-sensitive deformation is intentionally deferred.
- **Sculpt Radius/Strength remains a visual P2 debt** reported unchanged by UI-LAYOUT-R2. It was deliberately not redesigned inside R2; revisit only in later product-linked Sculpt UI work, not as a reason to reopen static right-host scope.

Do not optimize coarse mesh publication or picking pre-emptively without current device evidence — but do not classify the two items marked above as "debt" either: they are missing foundations with owning stages.

Documentation debt: orphaned `Stage 015A-R` SVG wireframes; a dated `Stage 015B` framing around a current status decision; an unreviewed `Stage 015C` shading comparison artifact.

### 8.3 Open questions register

`PROVISIONAL` is no longer a stage status (9.1 W-5). Everything that used to be provisional is a question with an owner.

| ID | Question | Answered by | Blocks |
|---|---|---|---|
| `OQ-A` | Does the zero-third-party-dependency rule (5.7) also govern the boolean kernel and UV unwrap? | owner, informed by `GATE-KERNEL` | `Stage 024`, `Stages 029–032` |
| `OQ-B` | Does 3.4.1 remain the v1 scope, or do we adopt the tiering in 3.4.3? | owner, at `GATE-SCOPE` | overall horizon; nothing immediately |
| `OQ-C` | Who is the user, and why do they choose ForgeShape over Nomad / Shapr3D / Blender? One falsifiable sentence. | owner | `GATE-VIABILITY`; strongly influences whether CAD or Sculpt leads |
| `OQ-D` | Does early GLB export (`Stage 023`) belong in the near-term roadmap? | **provisionally answered YES in 7.1** by folding it into `E2E-R1`; owner may reverse | `E2E-R1` |
| `OQ-01` | Directional Scale: exact anchor/contact rule for primitives versus arbitrary future bodies; planar and uniform behaviour under one-sided mode; multi-selection/group semantics; whether the mode is sticky per session/tool or resets when leaving Scale. | owner | `Stage 020D` |
| `OQ-02` | History ownership for a Body-level dimension/transform edit performed while Sculpt is active, so it is not mislabelled as a brush stroke. | coordinator + owner | `Stage 020M` (Sculpt dimensions part) |
| `OQ-03` | Alignment guides: which candidates exist first (body centers, pivots, bounding-box faces/edges, construction planes, exact geometry features); whether the threshold is screen-space, world-space or hybrid; what Rotate alignment means in v1; priority when Contact Snap and Alignment Guide are both candidates. | owner | `Stage 020S2` |
| `OQ-04` | Group pivot and World/Local orientation semantics for multi-selection. | owner | `Stage 018B` |
| `OQ-05` | Handedness and thumb reach: the entire layout assumes a right-side rail. Mirrored zones? A preference? | owner | **nothing.** It is answered *inside* `UI-SPEC-R0` as a mirrored-zone frame in the mock, and implemented later in `UI-PREF1`. It does not block `UI-SPEC-R0` from starting. |
| `OQ-06` | Grid Snap increment/unit contract. | owner | `GRID-SNAP-01` |
| `OQ-E` | What calendar date triggers `GATE-SCOPE`? Proposed default 2026-11-30. | owner | `GATE-SCOPE` is inert until answered |
| `OQ-07` | Plane orientation/normal/extent semantics were flagged as requiring an explicit decision at `Stage 016`; the stage completed. Confirm the implemented contract is the intended one, or record it as settled. | coordinator, against repo truth | nothing; hygiene |

### 8.4 Load-bearing assumptions that have never been tested

| Assumption | Cost if wrong | Cheapest experiment |
|---|---|---|
| An in-house boolean kernel is feasible in reasonable time | catastrophic — no CAD without it | `GATE-KERNEL`: a two-week desktop spike, `box − cylinder` with coplanar faces, plus a Manifold/libigl license audit |
| An in-house UV unwrap + packing is feasible | high — blocks UV and textured export | LSCM on a sphere, C++ on desktop, one weekend, no Android |
| GLB export will work in UE/Unity/Godot (scale, axis, pivot, normals) | medium-high, and rising with every domain stage | a hand-written exporter for one box, 2–3 days, imported in Godot and Blender |
| `.forge` can be serialized from the current domain | high, if the truth turns out to live in runtime state | dump six primitives plus transforms to a file and read it back |
| Whole-mesh `O(V)` publication will carry the density sculpting needs | very high — renderer and domain rewrite after the fact | extend the P1-E ladder to 500k on the reference device and measure stroke latency |
| Coding agents can handle research-grade geometry, not only UI and tests | critical — the whole schedule rests on it | the same boolean spike |
| Anyone besides the author wants this application and can use it | existential | `GATE-USER-01` |
| The author sustains this pace for 2–4 years without external reward | existential | none — which is why a showable result must be **built**, and that is what `E2E-R1` is for |

### 8.5 Idea inbox

Ideas the owner has raised but that are not yet classified into a decision, a stage or a question. Nothing here is approved, and nothing here authorises work. Coordinator: follow 2.3 before moving anything out of this list.

| ID | Idea | Raised | Status | Proposed destination |
|---|---|---|---|---|
| `IDEA-01` | Hierarchical decimal numbering (`1.2` / `1.3.1`) for the plan | 2026-08-28 | **RESOLVED** — adopted for section headings only, max 3 levels; explicitly rejected as a work/decision identifier (0.4) | this file, 0.4 |

*(Append new entries here. One line each: what was said, when, and where it would land.)*

---

## 9. PROCESS — WIP, CLOSURE AND WORK TYPES

Derived from the 2026-08-28 delivery analysis: in the ten days covered by the version notes there were 11 accepted product-changing stages, 9 rejected/partial/held ones, 8 read-only audits, 6 documentation/infrastructure stages and 2 prompts abandoned before launch. Rejection rate on product work: **45%**. In the last 2.5 days of that window: 17 plan versions, 13 stage activities, **zero** new features, two of them with a documented `PRODUCT_DIFF_COUNT=0`.

### 9.1 WIP and closure policy

**W-1 — WIP limit.** At most 1 stage `RUNNING` and at most 1 stage `OPEN` (implemented, awaiting a verdict). Total ≤ 2. No new prompt is written until the counter drops below the limit.

**W-2 — Definition of closure.** A stage is closed only when: (a) the verdict is `COMPLETE` or `ACCEPTED-AS-IS`; (b) it appears in no "hold" or "preserve" list; (c) its roadmap entry carries that status with no adjectives; (d) its debts have been moved into 8.2 as numbered individual items.

**W-3 — No open-ended intermediate states.** `TECHNICAL PASS`, `VISUAL HOLD`, `PARTIAL PASS` and `CLOSEOUT HOLD` are valid for 48 hours. After that the stage must become `COMPLETE`, `ACCEPTED-AS-IS` or `CANCELLED`. There is no "suspended" status.

**W-4 — no double bookkeeping of one defect.** A stage whose remaining objections are already fully owned by a later stage is closed, not held open. Applying this rule closed `Stage 020R2`; its status lives in 1.3 and nowhere else.

**W-5 — `PROVISIONAL` is not a stage status.** It is removed. Each former provisional item was resolved into exactly one of two states, and neither is left vague:
- **it had a genuine unanswered question** → the question is an `OQ-*` entry in 8.3 and the stage is `BLOCKED-BY` that question. Applies to `Stage 020D` (`OQ-01`), the Sculpt-dimensions part of `Stage 020M` (`OQ-02`), `Stage 020S2` (`OQ-03`), `GRID-SNAP-01` (`OQ-06`);
- **it had no unanswered question and was provisional only because its position was unsettled** → it is an ordinary `PLANNED` stage. Applies to `UI-PREF1`, `UI-PREF2`, `SCULPT-H1`, and the core of `Stage 020M`. `Stage 020S1` is in this group too: its semantics are settled by UI-OWNER-34, and what it waits for is `SPATIAL-R1`, which is a dependency, not a question.

No item may be left in a third state. A stage with an open question never appears as `PLANNED`.

**W-6 — Do not start a new stage** while an unclosed stage exists under W-2. Exception: an explicitly labelled `SPIKE` (R-1).

**W-7 — Audit limit.** At most 1 audit per 5 product stages. An audit that has not produced a corrective stage within 2 stages is cancelled and its findings move to 8.2.

### 9.2 Work types, readiness and budgets

**R-1 — SPIKE ≠ stage.** A spike has one goal: one decision. No full test package, no `.md` updates, no evidence report. Output: a screenshot or video plus a recommendation. The code may be discarded entirely; a `spike/*` branch need not be merged. Budget: one agent round.

**R-1a — RESEARCH GATE ≠ spike ≠ stage.** A research gate produces **one decision document** and no product code. It has no test package and no `.md` repository updates, but unlike a spike it does produce a durable written artifact that later prompts quote. Executor is `COORDINATOR`. Budget is declared in advance, in agent rounds or in owner review passes, not in calendar weeks. Applies to `CAD-R0-DATA`, `CAD-R0-UX`, `CLOUD-R0`, `PORT-R0`.

**R-2 — Definition of Ready.** A production prompt requires: one sentence of goal; an explicit visual acceptance criterion (a drawing or a geometric description); ≤30 test IDs; a declared number of rounds.

**R-3 — Mock first.** Any layout change that has already been visually rejected once goes through a spike with a single static screen, accepted from one screenshot, before a production stage.

**R-4 — Round limit.** At most 2 correction rounds per defect. A third round is forbidden: either change the method (R-3) or record `ACCEPTED-AS-IS` and move the residue to 8.2. **"Good enough" is a valid verdict.**

**R-5 — Proportional test gate.** As tabulated in 6.4.

**R-6 — Stage budget.** A production stage: ≤1 calendar day and ≤2 agent rounds. Exceeding it forces a scope cut, not an extension.

**R-7 — Direction review every 10 stages.** Five questions: what did we deliver for the user? how many stages did not touch the product? is the order still right? what do we close as `ACCEPTED-AS-IS`? what do we remove from the plan?

**R-8 — Retrospective after every rejection.** Three sentences: what we accepted, what the prompt failed to say, which rule prevents a repeat. Recorded in the version note.

**R-9 — Version-note hygiene.** One plan version per verdict, not per prompt edit. Full notes live in `HISTORY`; nothing is compressed without owner approval (2.5).

---

## 10. APPENDICES

### 10.1 Legacy identifier map

Old IDs are never deleted — they live in commits, reports, screenshots, prompts, tests and version notes. This table exists so that a mnemonic in an old artifact can still be resolved.

| Identifier | Meaning | Note |
|---|---|---|
| `Stage 020S` | early Move-only surface-snap concept | **RETIRED** — split into `Stage 020S1` (contact) and `Stage 020S2` (alignment). Any reference to "the Stage 020S design space" means those two. |
| `Stage 020R1` | narrow world-rotation correction | **SUPERSEDED before launch** by `Stage 020R2`; the prompt `AGENT_STAGE_020R1_WORLD_SPACE_ROTATION_CORRECTION.md` was never run. |
| `UI-PREF` (no digit) | appearance/settings work | means `UI-PREF1` + `UI-PREF2`. |
| `Stage 021` / `022` / `023` | persistence, autosave, early export | grouped under the milestone `E2E-R1` and executed as three stages `E2E-R1A` / `E2E-R1B` / `E2E-R1C`, one per old ID. `E2E-R1` is a milestone, never a stage; it takes no prompt. The old IDs remain valid for tests and commits. |
| `CAD-R0` | one CAD research gate | now split into `CAD-R0-DATA` (early) and `CAD-R0-UX` (at CAD implementation time). |
| `Forge Shell` | old name for the Editor Workspace UI | superseded by `Editor Workspace` (UI-OWNER-01). |
| `Freeze` / `Frozen Sculpt Mesh` | legacy reversible Construction↔Sculpt data path | internal implementation term only; forbidden in user-facing copy (UI-OWNER-27); disappears at `BRIDGE-R1`. |
| `Stage 015A / 015A-R` D1–D6 | old decision table | historical only. Active decisions use `*-OWNER-*` IDs. |
| `FORGESHAPE_PROMPT_AUTHORING_RULES.md` | standalone prompt policy | deprecated since v3.46.0; canonical home is 2.6. |
| `FORGESHAPE_PROJECT_STATE*.md` | old continuity file | deprecated since v3.33.0; if it exists for harness compatibility it is a generated non-canonical mirror. |
| `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md` | old UX pack | deleted in DOC-R2; two durable invariants were rescued into `ARCHITECTURE.md`. |

### 10.2 Roadmap nodes backfilled in v4.0.0

These were inside the approved MVP scope or explicitly required by an owner decision, but had no owning stage in v3.69.0 and could therefore have been lost:

| Node | Why it existed without a home |
|---|---|
| `GRID-SNAP-01` | in MVP scope; the roadmap said only "Grid Snap remains separate" |
| `MIRROR-01` | Construction Mirror/Symmetry is in MVP scope with no stage anywhere |
| `SEL-OUT-R1` | required by UI-OWNER-10/11; the plan itself said the renderer stage was unscheduled |
| `GATE-E2E` | the plan had no early end-to-end product proof at all |
| `CAD-R0-DATA` | the data-model decisions were buried inside a research gate scheduled after they were needed |
| `GATE-PERF-SCULPT` | Subdivide/Remesh had no memory or latency budget |
| `STYLUS-G1` | `P1-D` stylus evidence was UNVERIFIED and the next stylus work was in final hardening |
| `PERF-BASELINE` | the plan forbade optimizing without evidence but had no mechanism to collect it |
| `SPATIAL-R1` | classified as debt; it is a prerequisite for five stages |
| `DEVICE-M1` | GPU driver diversity was only covered at `Stage 036` |
| `Stage 022` autosave | defined in the plan but absent from the protected roadmap |
| `Stage 023` early GLB | defined as "early" but absent from the protected roadmap |

### 10.3 Plan integrity commands

Run before publishing any new plan version; append the result line to the version note (2.5).

```bash
# 0. one-time: the plan set in its own git repository
git init && git add *.md && git commit -m "v4.0.3" && git tag v4.0.3

# 1. heading-set diff — heading TEXT only, no line numbers.
#    `grep -n` was wrong: adding a paragraph shifts every later number and the diff
#    reports changes that did not happen.
grep -o '^#\+ .*' FORGESHAPE_MASTER_PLAN_v4.0.14.md | sort > /tmp/old.h
grep -o '^#\+ .*' FORGESHAPE_MASTER_PLAN_v4.0.58.md | sort > /tmp/new.h
diff /tmp/old.h /tmp/new.h

# 2. owner-decision counter — count HEADINGS, not lines mentioning an ID.
#    `grep -c 'OWNER-[0-9]'` returns 143 on this file, because every ID also appears
#    in its own metadata row and in cross-references. It must return 57.
grep -c '^### .*OWNER-[0-9]' FORGESHAPE_OWNER_DECISIONS_v4.0.54.md

# 3. change size matches intent — otherwise treat it as a loss alarm
git diff --stat

# 4. status-claim detector — the known shapes of the thing 0.1 forbids.
#    It is a heuristic, not a proof: no regular expression can decide whether a
#    sentence is a status claim. It catches every shape that has actually occurred
#    in this plan's history, including the on-hold and awaiting forms. A clean run
#    means "no known-bad shape", not "rule satisfied" — the coordinator still reads.
grep -nEi 'is currently running|is current\b|— CURRENT\b|RUNNING work|Current next|currently active|the running [A-Z0-9]|is (on hold|in progress|awaiting|pending)|wording exists outside section 1' \
  FORGESHAPE_MASTER_PLAN_v4.0.58.md

# 5. status vocabulary — every decision Status must be one closed value
grep -h '^| Status | \*\*' FORGESHAPE_OWNER_DECISIONS_v4.0.54.md | sort -u
```

Exempt from check 4: sections 0.1, 10.1, 10.4 and 10.5 of this file, the whole `HISTORY` file, and decision bodies in `OWNER_DECISIONS`. They quote historical wording on purpose. Any hit outside those places is a defect to fix in the same edit. Check 5 must return only values from the closed vocabulary in 0.3.

Version-note evidence line:

```
heading-set diff vs <prev>: +N / −N | OWNER-IDs 57→57 | competing-now hits outside §1: 0
```

### 10.4 v4.0.0 migration record

**Nothing was deleted.** Every owner decision, every stage definition, every roadmap item, every palette value, every gate and every version note from v3.69.0 exists in this file set. What changed is location, order and status hygiene.

**Moved:**

| From v3.69.0 | To |
|---|---|
| §20 Active owner decisions (67 decisions) | `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md`, with a status header per decision |
| §25 Version notes (741 lines, 74 notes) | `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58.md`, verbatim |
| §21 Latest accepted report delta | `HISTORY`, renamed **Report archive**, newest first, with the four missing reports backfilled |
| §0 Governance, §22 Prompt policy, §22A, §23, §24 | section 2 (single coordinator section) |
| §19A Naming policy | 2.12 (DOC-OWNER-01 now approves it rather than duplicating it) |
| §24A Premium UI governance | 4.3 |
| §1, §16, §17, §15 | section 3 |
| §5, §7–§14, §9, §10 stage definitions | 7.2, with all scope preserved and all status removed |
| §6 Gate P1, §4 Gate P0 | 6.6, 6.7 |
| §18 Dependencies | 5.7 |
| §19 Debt/risks | 8.1, 8.2 |
| `PORT-R0`, `CLOUD-R0` (defined inside §20, though they are stages) | 7.2.10; `CLOUD-R0`'s duplicate second definition removed, one canonical copy kept |

**Corrected — twenty factual defects verified against the source file:**

| # | Defect in v3.69.0 | Correction |
|---|---|---|
| A1 | §20 "Current next: `UI-AUDIT2B` is currently running" — nine versions stale, sitting inside the owner decisions a fresh coordinator is told to read | removed; replaced by 0.1's single-present-tense rule |
| A2 | §19 "UI-R3 is current" while §5 marked UI-R3 visually superseded | removed |
| A3 | §8 "Stage 020R2 — CURRENT" while §5 said reported complete / closeout hold | status now `ACCEPTED-AS-IS`, in one place |
| A4 | §2 baseline `f58c7b8` "before the running R2 stage" versus header HEAD `54c3dfa` | hashes removed from the product-state section; 1.1 is the only holder |
| A5 | §2 "Scale/planar handles are RUNNING work" | rewritten as completed capability (1.2) |
| A6 | §2 "the Stage 020R2 report when it arrives" — it had arrived | removed |
| A7 | §21A "Do not start Stage 019 before that review" — Stage 019 complete at `d8536ab` | removed |
| A8 | §21A dispute about a 2000-line documentation cap — settled by DOC-R2 | removed; 2.11 is canonical |
| A9 | §7 Stage 016 "add exact Plane" as a future goal, while Plane is implemented | 7.2.1, scope only, no future tense |
| A10 | UI-OWNER-31 "Surface Snap is the next capability" contradicting the roadmap | decision text preserved; ordering authority stated to be 7.1 only; `Extended-by: UI-OWNER-34` recorded |
| A11 | §21 called "Latest" while missing `UI-LAYOUT-R1`, `UI-ARCH-AUDIT1`, `UI-ARCH-R1`, `TEST-HARNESS-R1` | renamed Report archive; the four backfilled |
| A12 | header dated 2026-08-27 while the newest note was 2026-08-28 | dates reconciled |
| A13 | live references to the retired `Stage 020S` | mapped in 10.1 |
| A14 | `UI-PREF` without a digit in the protected roadmap | unified, mapped in 10.1 |
| A15 | §8 listing `020D / 020S1 / 020S2` and losing `020M` | one catalogue in 7.2.3, complete |
| A16 | prompt size given as "~1,800–2,500 words" in one place and "150–350 lines" in another | one metric, 2.6 rule 14 |
| A17 | §19A canonizing `Frozen Sculpt Mesh` "created by Freeze" while UI-OWNER-27 retires `Freeze` | 2.12 marks it explicitly internal-only |
| A18 | protected roadmap missing `Stage 022` and `Stage 023` | both in 7.1 inside `E2E-R1`; recorded as `OQ-D` |
| A19 | unexplained version gaps (`v3.9.0`, `v3.57.0`–`v3.59.0`) | explained in `HISTORY` |
| A20 | `CLOUD-R0` fully defined twice | one canonical definition, 7.2.10 |

**Added:**

- 0.1 single-present-tense rule; 0.3 closed status vocabulary; 0.4 identifier layering; 0.5 ID index; a table of contents;
- 2.1 coordinator identity and behaviour; **2.3 idea-intake protocol**; 2.13 owner communication and delta-based rejection;
- 4.2 `UI-SPEC-R0` and the measurable layout definition of done; 4.5 load variant; 4.6 the UX items that had no home;
- 5.4 the document-model question; 6.4 proportional test gates;
- 7.1 the new binding order; the backfilled nodes in 10.2; 7.3 existential gates including stop criteria;
- 8.1 risk register; 8.3 open-questions register; 8.4 untested assumptions; **8.5 idea inbox**;
- section 9 WIP and work-type policy;
- 10.3 mechanical integrity commands.

**Not done, deliberately, and needing owner input:** `OQ-A`, `OQ-B` and `OQ-C` in 8.3 are left open rather than answered on the owner's behalf. `OQ-D` was answered provisionally because 7.1 could not be written without a position on it.

### 10.5 v4.0.1 correction record

v4.0.0 was handed to a fresh coordinator with an instruction to report contradictions before doing anything else. It found twenty, all verified mechanically against the files. All twenty are fixed here. The pattern in them is worth recording, because it will recur: **v4.0.0 declared rules it did not itself obey, and shipped integrity commands that did not implement the rules they claimed to check.** A rule that cannot be checked is a preference.

| # | Defect in v4.0.0 | Fix in v4.0.1 |
|---|---|---|
| 1 | 0.1 banned the *words* `current`/`running`/`now`/`next` outside section 1. Unenforceable and false on its face — the plan legitimately says "current repo truth", "the current renderer" 75 times. | 0.1 restated: what is forbidden is a **status claim**, not vocabulary. Forbidden shapes listed explicitly. |
| 2 | 0.1 claimed to govern all three files; 10.3 exempted `HISTORY`. Two scopes for one rule. | Scope stated once in 0.1: this file plus decision metadata rows. `HISTORY` and verbatim decision bodies are exempt, and both files now say so themselves. |
| 3 | The detector searched four literal phrases, not the declared rule. | Detector rewritten to match the restated rule, plus a fifth check on the decision status vocabulary. |
| 4 | `grep -c 'OWNER-[0-9]'` returns **143**, not 57 — every ID appears in its own metadata row and in cross-references. The plan instructed the coordinator to run a command that could never pass. | Counts headings: `grep -c '^### .*OWNER-[0-9]'` → 57. |
| 5 | `grep -n '^#'` embeds line numbers, so adding one paragraph produces a heading diff full of changes that did not happen. | `grep -o '^#\+ .*' \| sort` — heading text only. |
| 6 | 1.5 used `HELD`, which is not in the closed vocabulary, while 7.1 used `BLOCKED-BY:UI-SPEC-R0` for the same stage. | 1.5 uses `BLOCKED-BY:UI-SPEC-R0`. 0.3 states that `HELD` is not a status. |
| 7 | `UI-SPEC-R0` was `READY`, which 0.3 defines as "a prompt may be written now" — but it is an owner artifact, and the handoff prompt had to say so in prose because the model could not express it. | New **executor** axis in 0.3 (`OWNER` / `COORDINATOR` / `AGENT`), and an executor column in 7.1. Ownership is now in the data, not in a warning. |
| 8 | 8.3 listed `OQ-05` as blocking `UI-SPEC-R0` while 1.6 said only `OQ-D` blocked anything. Literally read, the single next step was not startable. | `OQ-05` is answered *inside* `UI-SPEC-R0`, as a mirrored-zone frame in the mock. Blocks nothing. |
| 9 | Decision metadata used composite statuses (`APPROVED (values binding)`, `APPROVED-FUTURE / OPEN-QUESTIONS`, `APPROVED / SATISFIED`) and off-schema implementation values (`DEFERRED by…`, `ONGOING QUALITY BAR`). The closed vocabulary was violated in the same document that declared it. | All 57 normalized to one legal value each; the nuance moved into a renamed `Notes / open questions` row; both vocabularies tightened and made greppable (10.3 check 5). |
| 10 | 7.1 marked `020D`/`020S2`/`018B`/`GRID-SNAP-01` as `PLANNED`, which 0.3 defines as "no blocking open question", while 8.3 recorded a blocking question for each. | Those stages are `BLOCKED-BY` their specific `OQ-*` or `SPATIAL-R1`, itemised in the new 7.5. |
| 11 | W-5 said every former `PROVISIONAL` item "returns to the roadmap only with an answer", then listed items that are in the roadmap with no question attached. | W-5 rewritten with two explicit outcomes — question → `BLOCKED-BY`, no question → plain `PLANNED` — and each of the seven items assigned to one of them. |
| 12 | `CAD-R0-DATA` was told to decide "own boolean kernel or library", while `OQ-A` said that decision is informed by `GATE-KERNEL`, which sits many positions later. Circular. | `CAD-R0-DATA` no longer decides the kernel. It writes `GATE-KERNEL`'s pass criteria and candidate list; the owner answers `OQ-A` at the gate, on evidence. |
| 13 | `GATE-KERNEL` blocked `CAD-A1`/`CAD-A2`, although UI-OWNER-04 separates Extrude New Body from Add/Cut, and Add/Cut is the part that needs booleans. This would have delayed the project's only differentiator behind its largest technical risk for no reason. | `GATE-KERNEL` gates `Stage 024` and the Add/Cut half of `CAD-A4` only. `CAD-A1`/`CAD-A2` proceed after `CAD-R0-UX`. |
| 14 | `CAD-R0-DATA` was labelled a `SPIKE` (R-1: one agent round, no documents, output a screenshot) but required to produce a decision document. | New work type **R-1a RESEARCH GATE**: one decision document, no product code, no test package, budget declared in rounds. Applies to `CAD-R0-DATA`, `CAD-R0-UX`, `CLOUD-R0`, `PORT-R0`. |
| 15 | `E2E-R1` was one production stage spanning `021`+`022`+`023` — format, migration, autosave, recovery, Scoped Storage, error handling, sample projects and GLB export — against an R-6 budget of one day and two rounds. | Decomposed into `E2E-R1A` / `E2E-R1B` / `E2E-R1C`, each within R-6, sharing one `GATE-E2E`. `E2E-R1` is now a milestone, not a stage. |
| 16 | `DEVICE-M1` was backfilled as important, carried the ordering instruction "run right after `E2E-R1`" inside 7.2 — which is supposed to hold no order — and appeared nowhere in 7.1. | `DEVICE-M1` is position 5 in 7.1. Ordering prose stripped from 7.2 catalogue entries. |
| 17 | `GATE-SCOPE` was defined as "a calendar date" with no date, making one of three stop criteria inert. | Recorded as `OQ-E` with a proposed default of 2026-11-30, added to the owner actions in 1.6, and explicitly marked inert until answered. |
| 18 | Import sat in a bundle with `Stages 036–040` with no internal order, so the readiness gate in 3.6 was not guaranteed to precede it. | Import is its own row 22, `BLOCKED-BY:Stage 040` **and** the 3.6 readiness gate, with the gate stated to outrank its roadmap position. |
| 19 | 0.4 required relations as explicit named fields, but 7.2 recorded them in prose — "depends on", "preceded by", "only after", "run right after" — so no dependency check was possible. | New **7.5 dependency graph**: one table, `Blocked-by` and `Needs` as fields. Prose dependencies removed from the catalogue. |
| 20 | `Stage 020R2`'s `ACCEPTED-AS-IS` status was stated in 1.3 and restated in W-4 — exactly the duplication v4.0.0 exists to prevent. | W-4 is now a rule ("no double bookkeeping of one defect"), not a status. Status lives in 1.3 only. |

What this review did **not** find, and it is worth stating: no missing owner decision, no lost version note, no altered decision body, no incorrect roadmap dependency beyond items 12–13, and no factual error in section 1. The migration was sound; its rule layer was not.

### 10.6 v4.0.2 correction record

The coordinator re-reviewed v4.0.1 against its own twenty findings. Thirteen were closed. Seven were fixed in one place but not in every place, and four new inconsistencies came from the fixes themselves. All eleven are closed here, and every one was verified against the files before being accepted.

The pattern this time is narrower than last time and worth naming exactly: **v4.0.1 introduced three new models — `executor`, the closed `Status` vocabulary in the roadmap, and the dependency graph — and applied each of them to the case that motivated it, but not to every row of every table.** A model applied to eighty percent of its rows is not a model; it is a convention with exceptions, and exceptions are what the reader stops trusting.

| # | What was still open in v4.0.1 | Fix in v4.0.2 |
|---|---|---|
| 1 | 0.1 was corrected, but 2.8's pre-prompt checklist still carried the old vocabulary form of the rule — "no `current` / `running` / `next` wording exists outside section 1". The plan therefore stated the rule twice, in two incompatible versions. | 2.8 restated as a status-claim check pointing at 0.1. |
| 3 | The detector missed the `on hold` and `awaiting` shapes that 0.1 explicitly forbids, and could not have caught the stale rule in 2.8. | Patterns widened to every shape that has actually occurred in this plan's history, and the command now says plainly that it is a heuristic: a clean run means "no known-bad shape", not "rule satisfied". |
| 4 | The MASTER PLAN's counter command was fixed; `OWNER_DECISIONS` still instructed the reader to run the broken one. | Fixed there too, with the failure mode written next to it so it cannot be reintroduced. |
| 9 | `Implementation` still contained free prose inside the parentheses — `IMPLEMENTED(Stage 020R2, ACCEPTED-AS-IS at a90e7df)` — against its own schema. Check 5 also matched the schema definition row, so it could never return only real values. | Schema tightened to stage IDs, `legacy`, and at most one trailing `accepted YYYY-MM-DD`; the commit hash and verdict moved to UI-OWNER-30's notes; check 5 now matches `^\| Status \| \*\*`. |
| 10 | 7.1 carried `BLOCKED-BY: per item — see 7.5`, which is not a legal status, and listed `Stage 020M` as `PLANNED` while part of its scope was blocked by `OQ-02` — one ID with two statuses. | Grouped rows 17, 18, 19 and 21 split into one row per work item, each with one legal status. The Sculpt-dimensions half of `Stage 020M` became its own ID, `SCULPT-DIM-01`, `BLOCKED-BY:OQ-02`. |
| 15 | The decomposition was right, but 10.1 still said `Stage 021/022/023` are "now executed as one stage `E2E-R1`", which had become false. | 10.1 states the milestone/stage distinction and maps one old ID to each new sub-stage. |
| 19 | 7.5 claimed to be "the only place dependencies are recorded" while 7.2 still described several in prose. | The claim is now precise and true: 7.5 is the only place a dependency is *recorded*; 7.2 may restate one in words only if it already exists here as a field, and never introduce one that is missing. If it does, this table is the defect. |
| new 1 | Live pointers in 0.5 and in the section 4 intro still named the `v4.0.0` filenames while the file set at the top named `v4.0.1`. | All live filename pointers updated together with the version. |
| new 2 | The `executor` axis, introduced in v4.0.1 with a three-value closed vocabulary, immediately violated it: row 21 read `mixed — see 7.2`. | Row 21 split into five rows, each with one executor. |
| new 3 | The roadmap `Status` column, declared to use only 0.3's vocabulary, contained `READY — today`, `gate`, `gate over …`, `PLANNED; …` and a compound `BLOCKED-BY:… and …`. | `GATE` added to 0.3 as a legitimate kind; every cell reduced to one value; the qualifications moved into the `What` and `Why here` columns where they belong. |
| new 4 | `E2E-R1a/b/c` used lowercase suffixes, while 0.4 defines decomposition as uppercase `A/B/C`. A rule broken by the first IDs created under it. | Renamed `E2E-R1A/B/C`; 0.4 now states that the decomposition letter attaches to the whole ID, exactly as in `Stage 018A/B/C`. |

**This is the last correction round on the plan.** 9.2 R-4 caps corrections at two rounds per defect, and 9.1 W-7 cancels an audit that has not produced a corrective stage within two stages. Both now apply to this document. The plan is a means; the product is the thing that cannot yet save a file. If a further defect is found in these files, it is recorded in 8.5 and fixed in passing during the next version bump — it does not open a third review round, and it does not delay `UI-SPEC-R0` or the remote backup by a single day.

---

*End of MASTER PLAN v4.0.58.******************************* Owner decisions: `FORGESHAPE_OWNER_DECISIONS_v4.0.54.md`. History and report archive: `FORGESHAPE_MASTER_PLAN_HISTORY_v4.0.58.md`.*
