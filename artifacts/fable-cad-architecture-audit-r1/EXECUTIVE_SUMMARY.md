# Executive summary — FABLE-CAD-ARCHITECTURE-AUDIT-R1

**Result: `PASS-FABLE-CAD-ARCHITECTURE-AUDIT-R1`.** Audit only: no product,
test, script, workflow, build or format byte changed; no emulator suite ran.

## 1. Baseline and scope

| Item | Value |
| --- | --- |
| Audited tree | `origin/main` = `509de02331ca1a8dfc20528b42d83a18392cef9f` (expected SHA; verified) |
| Work branch | `claude/new-session-g87rge`, brought to `509de02` by a normal merge (its one conflict, `PROJECT_STATUS.md`, resolved by taking `main`'s file); the tree was byte-identical to `main` before any audit file was written |
| Method | six read-only source passes (extrude HUD, region model, feature retention, coordinators and module graph, Autodesk docs, Shapr3D/Onshape/FreeCAD docs), each claim re-verified by the coordinator against source where it is load-bearing |
| Evidence tags | `SOURCE_CONFIRMED`, `TEST_CONFIRMED` (tests READ, never run), `DEVICE_EVIDENCE` (recorded CI frames), `DOC_ONLY`, `INFERRED`, `UNVERIFIED`, `RUNTIME_VERIFICATION_NEEDED` |
| Files changed | `artifacts/fable-cad-architecture-audit-r1/` (twelve files) and one status entry in `PROJECT_STATUS.md` |

One correction to the brief: **Shapr3D has no iPhone app** (official
compatible-devices page); its lessons are canvas grammar on an iPad with a
Pencil, and the honest phone comparison is Onshape's iOS/Android app
(`CAD_BENCHMARK.md`).

## 2. The twelve conclusions

**C1 — Is the CAD domain architecture sound?** Yes. One truth owner per
fact: `CadBodyState` (feature chain) is the only CAD truth, `regenerateCadBody`
the only derivation, `applyState` writes nothing on refusal, the renderer
sees only a snapshot and a line list, Java re-reads native per refresh and
holds no second truth (enums cross as validated wire ints; one anchor
conversion). The three sentences that are WRONG are around the geometry, not
in it: "two regions may not share a loop", "the hit box is the drawn box",
"a sketch is a field of a feature". (`CODE_ARCHITECTURE_MAP.md` §2–3, §6.)

**C2 — Is the extrude HUD architecture right for a technical-drawing feel?**
Right split, wrong presentation. Truth native, arrow world, text Android, one
projection — keep. But six floors (band 0.80, glyph 24 dp, 48 dp hit box that
IS the drawn box, fixed type, the row capsule, the head floor) hold the HUD at
screen size, the value sits ON the shaft rather than beside a leader, and the
overlay and the HUD read `metersPerPixel` at two different depths
(`jni:1859-1862` vs `:4854`), contradicting `session.cpp:2004-2010`. The
recommended hybrid draws a `Dimension`-style leader with the machinery
Dimensions mode already has, orients the value along it, decouples drawn from
hit, and needs no renderer text, no new pipeline, no truth change.
(`EXTRUDE_HUD_AUDIT.md`.)

**C3 — Why can the OWNER not select rectangle + one circle?** Because
`validateRegionSelection` refuses a region beside its own hole
(`sketch_region.cpp:329-334`: `loopContains(R, A)` and `insideAHole` skips
`h == A`), and the viewport tap path never shows that refusal — it silently
DROPS the ring (`sketch_session.cpp:1318-1327`). The three atomic regions
(ring with holes {A, B}, disk A, disk B) are correct; the missing sentence is
"a selection is any set of atomic regions, extruded as their union", which is
a parity rule over disjoint regions, needs no 2D boolean library, changes no
`ProfileRegionRef` and no `CADB` byte. (`PROFILE_SELECTION_AUDIT.md` §5.)

**C4 — Are later sketches and features retained and reopenable?** Yes,
natively complete and coherent: every feature keeps its sketch, extrusion,
operation and support by value; `beginEdit` is a forwarder to
`beginEditFeature`; reopening stages all three; Add ↔ Cut switchable; Finish is
one step. The OWNER's feeling is explained by four Android facts: the list is
two surfaces deep and withdrawn for one-feature bodies; the only canvas chip
reopens the BASE sketch; no retained sketch is ever drawn; the typed Shape
fields are base-only and do not say so. (`SKETCH_FEATURE_HISTORY_AUDIT.md`.)

**C5 — Can one sketch drive several features?** No, structurally: a sketch is
an embedded value with no id (`cad_body.h:268`). Missing: a sketch identity,
a sketch table in `CadBodyState`, features referencing it, a dependency rule,
`CADB` v6. Regions are already sketch-relative, so nothing in the selection
model changes. (`SKETCH_FEATURE_HISTORY_AUDIT.md` §5; OWNER decision D3.)

**C6 — Implemented but poorly exposed.** The feature list (precision surface
only, withdrawn for one feature); region re-toggle inside an edit session
(works, untested); the `Dimension` leader overlay (Dimensions mode and the
Line dimension have it; the extrude HUD does not); Add ↔ Cut on reopen; the
extent transitions; `viewFrame()` (the navigator's Flip never reaches the
camera, council D2 still open).

**C7 — Duplicated or obsolete.** The base-only `cadState`/`cadApply*` door
with its process global; `sketchSelectProfile` beside the uncalled
`sketchToggleRegion`; three Undo families; three anchored value editors; the
duplicated profile-listing loop; the five project reset lists; two saved-state
baselines (D3); `orientation`/`onSegment` twice; six dead exports and three
dead helpers; 54 test-only exports (27 % of the JNI surface), 18 unguarded;
the Imported Mesh Preview kept alive with a per-tap check. Plus ~30 stale
comments, of which one contradicts code. (`COMPLEXITY_RED_FLAGS.md` §4.)

**C8 — Simple behaviours overengineered.** The HUD cluster (1228 lines for
four controls, six floors); `CADB` versioning (about seven hand edits per
version); the 715-line `touchEvent`; the test-only JNI surface; the
5504-line coordinator (226 methods, 20 listeners, 97 natives, 22 resync
sites). (`COMPLEXITY_RED_FLAGS.md` §2.)

**C9 — Hard behaviours underengineered or fragile.** The region-selection
sentence (a refusal plus a silent drop instead of a union rule); the
multi-region prism build (concatenation with no merge, safe only while
adjacency is forbidden); retained-sketch identity; the origin-vs-base
`metersPerPixel` split; `nextCadFeatureId` from `laterFeatures.back()` (id
reuse the day delete exists); the first candidate evaluation on the UI thread
(untimed at 16 features on a phone); the render loop that never rests; a
history step with no byte budget. (`COMPLEXITY_RED_FLAGS.md` §3.)

**C10 — Modularise before Through All / Revolve / projected edges.** In this
order: (1) the region union rule (B1) — Through All and Revolve both extrude
a REGION SET, and building them on today's rule bakes the refusal in twice;
(2) sketch identity (D3, `CADB` v6) — projected edges add non-authored
reference geometry to a sketch, which needs a sketch that is an object, and
Revolve needs an axis reference INTO a sketch; (3) the `CADB` version table
(B3) — all three options need v6 and today each version costs seven hand
edits; (4) the JNI family split with the debug TU first (B4) and one session
reset (B5) — every new feature otherwise lands another branch in `touchEvent`
and another reset list; (5) one `metersPerPixel` (S3) before any HUD work.
(`REFACTOR_SEQUENCE.md`.)

**C11 — Already good; do not rewrite.** The loop extraction and its refusal
vocabulary; the nesting region model; the extent model (two positive
distances, canonical forms, pure transitions); the feature chain, lineage
signature and atomic regeneration; the one-file kernel adapter; the staged
edit session with one commit; the candidate-preview substitution; the
anchors/scale/manipulator pure functions; `ViewportAnchorSpace` and the single
native projection; the `.forge` all-or-nothing codec and its independent
encoder; the two separate histories; the object commands as pure functions
with one transaction each; `EditorUiState`; the snapshot as the ONE list.
(`REFACTOR_SEQUENCE.md` "DO NOT DO".)

**C12 — The top five OWNER decisions** (`OWNER_DECISIONS.md`): D1 the HUD's
visual bands and value placement; D2 the union selection semantic; D3 sketch
identity and retained-sketch visibility; D4 what a sketch on a face DOES
(same body vs other body); D5 one door for typed CAD values.

## 3. Facts, interpretation, recommendation — kept apart

- **Facts** are the cited lines in the six audits; every one carries a tag.
- **Interpretation** is confined to the "Interpretation" columns of the
  benchmark, the `INFERRED` rows, and the classifications in
  `COMPLEXITY_RED_FLAGS.md`.
- **Recommendations** are the §4 C hybrid, the §5 union rule, the sketch
  identity sketch, the target modules and the refactor order — none is
  implemented, and each names the OWNER decision it waits on.

## 4. Runtime questions still open

Sixteen items in `RUNTIME_VERIFICATION_GAPS.md`, each with the smallest test
that answers it. Three magnitudes need the OWNER's physical device: the
perspective head/corridor disagreement on a face sketch (G1), the cost of a
16-feature chain on the UI thread (G9), and the render loop's battery cost
(G15). None changes a conclusion above.

## 5. Recommended next action (audit conclusion only)

Put D1–D5 to the OWNER. While they are decided, the SAFE NOW list (S1–S8) can
proceed as ordinary small tasks: it removes dead code, guards the shipped test
seams, fixes the one contradicted comment by fixing the code it describes
(S3), moves the saved-state baseline (S5), shares the value editor (S7) and
corrects the stale documentation (S6). Nothing in that list changes a
user-visible semantic, a byte, or a fixture. Do not start Through All,
Revolve or projected edges before B1, D3 and B3.

## 6. Confirmation

No feature was implemented, no refactor applied, no dependency added, no
`.forge`/`CADB`/test/workflow/build file touched, no long test suite run.
The twelve artifacts and one `PROJECT_STATUS.md` entry are the whole diff.
