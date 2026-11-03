# FORGESHAPE MASTER PLAN — ADDENDUM v4.1.63 (CAD-R2 closeout / C1 gate)

**2026-10-28**. DOŁĄCZ do kanonicznego `FORGESHAPE_MASTER_PLAN_v4.1.61.md` oraz poprzedniego addendum v4.1.62. **Nie zastępuj** nimi pełnego Master; pełna kopia v4.1.61 nie jest dostępna w tym kontenerze. Zasada: tylko sekcja bieżącego wykonania Master może ogłaszać aktualny stan po scaleniu.

## CURRENT EXECUTION — delta do sekcji 1 kanonicznego Master

**CAD-R2-RETAINED-SKETCH-PARAMETRIC-UX-AND-GEOMETRY-HARDENING**: delivered scope technically tested; no merge and no final OWNER physical verdict. GitHub `feature/cad-r2-retained-sketch-parametric-ux-r1@ea24d20805d9487d26221d9dd45e47dc16fb6ecb`, 11 commits ahead/0 behind baseline `636797e`, `main 6c9f156` unchanged. Code after `f706e92` preserved across DEVICE B/FAST/final docs (test/docs-only commits).

Technical evidence: host **4369/0 (report/documented; not re-run)**, 25 suites; 17 new independent geometry oracles (reported), JVM **196/0** independently parsed from FAST artifact, device 67/0 + 51/0 from two artifacts (same product, two heads), startup 25/25 each, corpus **72/72 byte-identical**, release guard PASS both ABIs, CI FAST 40694343402 success. APK 17 545 911 B, SHA-256 `043203e18733d10e28985bf86a4cb41ed87fc3c3fd743b391fbc20a6ad5ac272`, original CI bytes.

**Product outcome:** retained sketch browser, Keep Sketch on self-feature faces, shared sketch reuse for Add/Cut, safe delete unused, feature parameter edit and dependency diagnostics, Select All/Clear, robust validation and oracles. No format change, no new CAD module from SolidCraft. Docs: `artifacts/cad-r2-retained-sketch-parametric-ux/`.

**Status vocabulary:** `TECH PASS (DELIVERED SCOPE) / A2-WORLD-PLANE DESIGN BLOCK / PERF INVESTIGATION OPEN / OWNER R2 PHYSICAL REVIEW OPEN / NOT MERGED`. Claude's PASS token is *not* an unconditional full-scope acceptance.

## Next bounded work recommendation: CAD-R2-C1

A focused *quality-gate* after major CAD-R2 implementation, on a new task branch from exact `ea24d208`:

1. Reproduce/isolate +31.5% `eval_all_us` and +35.1% `drafting_trim100_us` median; +27.4% `revolve_square_max_us` as watch. A/B fresh same-toolchain builds, warm-up, interleaved/order-balanced samples, raw traces, variance and causal attribution. Fix only confirmed product regression; never sweep away a real issue as 'container noise' without evidence.
2. Check curved-wall tap vs world-plane chooser priority/occlusion with actual touch and screenshots/selectedKind; fix only unexpected/unsafe hit ownership; preserve explicit user world-plane selection and fail-closed no curved-face support.
3. FontScale >1.0 and landscape History action 48 dp checks on emulator; test stale chooser, Undo, Cancel, no accidental UI commit; standard host/JVM/DEVICE/FAST/corpus/guard, exact APK if product files change.
4. Clearly separate OWNER physical acceptance from emulator tests. Report 1:1 feature matrix and residual risks.

**Out of scope:** world-plane second root sketch; feature reorder, solver, Fillet, Chamfer, Shell, STEP/IGES, Revolve Add/Cut, public MCP, Rust or SolidCore kernel, `.forge` migrations, independent code copied from SolidCraft. No silent widening of MVP, no main or v6 merge and no FullSharded without a separately authorised integration gate.

## Open OWNER design decision (leave unresolved)

`OQ-CAD-R2-WORLD-PLANE-RETAINED-SKETCH`: Should independent sketches authored on XY/XZ/YZ belong to an existing CAD body without becoming the base root? Current v6 model refuses a second root and New Sketch on the world plane follows its existing New Body path. OWNER has not approved a broader body-local workplane anchor, validation change or codec migration. Keep explicit refusal until separately approved; do not relabel A2 as full PASS.

## Evidence/provenance and file hygiene

Use canonical repo `PROJECT_STATUS.md` v0.100.0, R2 `SUMMARY.md`, `TESTS.md`, `ACCEPTANCE_MATRIX.md`, `DESIGN.md`, `perf_same_machine.md`; artifacts GitHub run IDs 40683528533, 40689211847, 40694343402. Full canonical Master and Owner Decisions must be preserved. Only append this section; never reconstruct missing history from conversation memory or overwrite complete files with shortened text.
