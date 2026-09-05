# Markdown truth matrix — DEEP-AUDIT-R1

Every tracked `.md` classified. Classes: BINDING_CURRENT_TRUTH · PRODUCT_DOC · ARCHITECTURE_SPEC · DATA_SPEC · STATUS_ROADMAP · AUDIT_EVIDENCE · HISTORICAL_ARCHIVE · GENERATED/THIRD_PARTY · UNKNOWN.

Rule applied (CLAUDE.md "Documentation ownership"): the six root docs are current-truth and were corrected where stale (see the contradictions rows and `MARKDOWN_TRUTH_MATRIX`'s companion `ARCHITECTURE.md` audit table). Everything under `artifacts/` is dated per-stage evidence and is HISTORICAL_ARCHIVE — **not edited**, per the brief ("Do not edit historical reports merely because their old statements are no longer current"), except this stage's own `artifacts/deep-audit-r1/` (AUDIT_EVIDENCE). The repository uses no banner convention, so superseded artifact statements are noted here rather than stamped in the files.

## The six current-truth documents

| Path | Class | Claims current? | Claims future? | Authoritative for | Contradictions found | Action |
| --- | --- | --- | --- | --- | --- | --- |
| `CLAUDE.md` | BINDING_CURRENT_TRUTH | yes | no | repository rules, device safety, UI vocabulary | none (twenty-suite list, twenty-eight-fixture corpus, three representations all current) | none |
| `README.md` | BINDING_CURRENT_TRUTH | yes | no | tooling, build/run/verify | none (twenty suites, twenty-eight fixtures, seven sketch tools) | none |
| `PROJECT_STATUS.md` | STATUS_ROADMAP | yes | yes (Next Stage, Technical Debt, still-out list) | current status, verified capability, environment, next stage | "one of two representations"; sculpt "no save/load/undo"; sketch grid fixed / top view at clamp (debt); "no camera read-back" ×2; still-out list named arcs/splines/face planes; "five sketch tools" | CORRECTED in the docs commit |
| `ARCHITECTURE.md` | ARCHITECTURE_SPEC | yes | boundaries list only | production architecture, module ownership | "A body has two possible representations"; "No snapping, and no Sketch grid"; "two verbs" for the Objects list | CORRECTED |
| `PRODUCT.md` | PRODUCT_DOC | yes | "Not yet implemented" list only | runtime-verified user-visible behaviour | opens on a single body; Undo Construction-only; grid quarter-metre fixed; "no arc/spline/face plane"; "no sketching and no extruding"; imported "no sculpting yet"; start choice remembered | CORRECTED |
| `DATA_PACKAGE_SPEC.md` | DATA_SPEC | yes | §12 extension rules only | `.forge` layout, codes, validation, determinism, fixtures | none (v1/v2/v3, twenty-eight fixtures, digests all match the device print and the PowerShell VerifyOnly) | none |

## All tracked markdown, by class

| Class | Count | Current? | Action |
| --- | ---: | --- | --- |
| BINDING_CURRENT_TRUTH | 2 (`CLAUDE.md`, `README.md`) | yes | none |
| STATUS_ROADMAP | 1 (`PROJECT_STATUS.md`) | yes | corrected |
| ARCHITECTURE_SPEC | 1 | yes | corrected |
| PRODUCT_DOC | 1 | yes | corrected |
| DATA_SPEC | 1 | yes | none |
| AUDIT_EVIDENCE | this stage's `artifacts/deep-audit-r1/*` | yes (dated 2026-09-05) | authored here |
| HISTORICAL_ARCHIVE | the remaining 106 under `artifacts/` | no — dated per-stage evidence | **not edited** (brief) |
| GENERATED/THIRD_PARTY | 0 | — | — |
| UNKNOWN | 0 | — | — |

Notable HISTORICAL_ARCHIVE files whose old statements are now superseded but were deliberately left as-is: `artifacts/cad-r0-a1a2/*` (says "R0 had no arc and no spline"), `artifacts/glb-import-r*/` (preview described before IMPORT-01A removed its controls), `artifacts/architecture-health-review/*` (its deferred items include the representation-unification and the release-symbol question this audit re-examined). `artifacts/cad-a3-c2-sketch-ux-r1/` is the latest closed stage and remains accurate.

## Full per-file listing

| Path | Class | Claims current? | Claims future? | Authoritative for | Contradictions | Action |
| --- | --- | --- | --- | --- | --- | --- |
| `ARCHITECTURE.md` | ARCHITECTURE_SPEC | yes | boundaries list only | production architecture and module ownership | — | none |
| `CLAUDE.md` | BINDING_CURRENT_TRUTH | yes | no | repository rules, device safety, vocabulary | — | none |
| `DATA_PACKAGE_SPEC.md` | DATA_SPEC | yes | §12 extension rules only | .forge binary layout, codes, validation, fixtures | — | none |
| `PRODUCT.md` | PRODUCT_DOC | yes | not-yet list only | runtime-verified user-visible behaviour | — | none |
| `PROJECT_STATUS.md` | STATUS_ROADMAP | yes | yes (Next Stage, deferred lists) | current status, verified capability, environment, next stage | — | none |
| `README.md` | BINDING_CURRENT_TRUTH | yes | no | tooling, build/run/verify instructions | — | none |
| `artifacts/architecture-health-review/COMMENT_AUDIT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/FEATURE_CHANGE_SURFACE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/HOTSPOTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/MODULARITY_SCORECARD.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/MODULE_MAP.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/SAFE_FIXES.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/architecture-health-review/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/BOOTSTRAP_SESSION.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/CADB_V2_DATA_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/CORPUS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/DEPENDENCY_GRAPH.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/FACE_TOPOLOGY.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/GRID_CAMERA_UX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/HOME_FLOW.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/PERFORMANCE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/SPATIAL_PLANE_PICKING.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/TOPOREF_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/TRANSFORM_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-app-h1/VISUAL_EVIDENCE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/CORPUS.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/CURVE_DOMAIN.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/DATA_CONTRACT.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/DEVICE_E2E.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/EDIT_SKETCH.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/HOME_PAGE.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/INDEX.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/LINE_DIMENSION_CONTRACT.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/ORIENTATION_NAVIGATOR.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/OWNER_FAILURE.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/TEST_RESULTS.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-a3-c2-sketch-ux-r1/VISUAL_EVIDENCE_C2.md` | AUDIT_EVIDENCE | no (dated evidence) | deferred items of its stage | CAD-A3-C2 / SKETCH-UX-R1 evidence (latest closed stage) | — | none |
| `artifacts/cad-r0-a1a2/CORPUS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/DATA_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/DOMAIN_MODEL.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/EXTRUDE_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/PARAMETRIC_REGENERATION.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/PERFORMANCE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/PROFILE_EXTRACTION.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/SKETCH_ENTITY_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/TRIANGULATION.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/cad-r0-a1a2/WORKPLANE_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1a/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1b/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1b/saf-transfer-results.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/BAKED_TRANSFORM_C1.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/COORDINATE_AUTHORITY.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/GATE_E2E_GODOT_CHECK.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/e2er1c/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r0/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r0/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r0/SUBSET.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r0/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r1/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r1/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r1/OWNER_SAMPLE_TARGET.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r1/SUPPORTED_SUBSET.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/glb-import-r1/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01a/CORPUS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01a/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/CORPUS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/DATA_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/DELETE_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/SCULPT_SOURCE_CONTRACT.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/import-01b/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/ARCHITECTURE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/DEVICE_E2E.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/MEMORY_BOUNDS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/STROKE_TRANSACTION.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/sculpt-undo-r0/TEST_RESULTS.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/stage015c_shading_comparison.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/stage020r3/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/geometry-equivalence.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/source-delta.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/source-diff-summary.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/source-ownership.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiarchr1/test-summary.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit1/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2a/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2a/SKILL-PROOF.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2b/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2b/SKILL-PROOF.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2c/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2c/SKILL-PROOF.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2d/MOBBIN-INVOCATION-LOG.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2d/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uiaudit2d/REFERENCE-CATALOGUE.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr1/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2-correction/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2-correction/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2-owner-review/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2-spec-closeout/INDEX.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2-spec-closeout/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uilayoutr2/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
| `artifacts/uir5a/README.md` | HISTORICAL_ARCHIVE | no (dated evidence) | its stage's deferred items | that stage's evidence at its date | — | none |
