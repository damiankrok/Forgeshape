# Future-contract matrix — DEEP-AUDIT-R1

Every forward-looking claim the documents make, classified: IMPLEMENTED · OWNER_APPROVED_NOT_IMPLEMENTED · PLANNED_NOT_OWNER_APPROVED · DEFERRED/OUT_OF_SCOPE · SUPERSEDED · STALE/CONTRADICTORY. STALE/CONTRADICTORY items were corrected in the docs commit; nothing here obliges future code.

| Claim | Where | Class | Note |
| --- | --- | --- | --- |
| Arc, Spline, face-based sketch, Edit Sketch, orientation navigator, line dimension | CLAUDE/PRODUCT/STATUS | IMPLEMENTED | `SKETCH-UX-R1`; verified at `d783d4b` |
| CADB v3 for curves, superset of v2/v1 | DATA_SPEC §7d | IMPLEMENTED | digests match device + PowerShell |
| Sculpt Undo/Redo, per-body bounded | CLAUDE/STATUS | IMPLEMENTED | `ARCH-OWNER-12` |
| Imported body sculpting | CLAUDE/PRODUCT | IMPLEMENTED | `IMPORT-01B` |
| Home / project lifecycle, dirty guard, recovery | CLAUDE/PRODUCT | IMPLEMENTED | `APP-H1` |
| CAD → Sculpt | CLAUDE ("Not this stage"), STATUS Technical Debt | DEFERRED/OUT_OF_SCOPE | refused by name; needs three named decisions |
| Custom construction planes, curved-face / imported-surface sketches, projected edges | CLAUDE, PRODUCT "Not yet", STATUS | DEFERRED/OUT_OF_SCOPE | no code/control/string exists (verified: grep) |
| Booleans, fillet, chamfer, shell, revolve, sweep, loft, pattern, mirror, offset, trim, constraint solver | PRODUCT, STATUS | DEFERRED/OUT_OF_SCOPE | none present |
| Editing a polygon profile's / a spline's points in place | PRODUCT, STATUS | DEFERRED/OUT_OF_SCOPE | spline edited by redraw; polygon count-only |
| Selection Outline | STATUS ("candidate awaiting owner decision") | PLANNED_NOT_OWNER_APPROVED | needs a post-processing pass |
| `BRIDGE-R1` one-way Construction→Sculpt derivation | STATUS | PLANNED_NOT_OWNER_APPROVED | "future" |
| Stage 018A object commands (rename, visibility, lock, duplicate, group, reorder, multi-select) | STATUS, ARCHITECTURE | DEFERRED/OUT_OF_SCOPE | only Delete exists |
| Stage 033 full exporter, OBJ, FBX, materials/textures/UV/animation both ways | STATUS, PRODUCT, ARCHITECTURE | DEFERRED/OUT_OF_SCOPE | none present |
| Save As, naming, recents, thumbnails, project library, cloud, accounts | PRODUCT, STATUS, ARCHITECTURE | DEFERRED/OUT_OF_SCOPE | one slot + one checkpoint |
| 16 KB-page ARM64 combined target | STATUS Known Issues | DEFERRED/OUT_OF_SCOPE | device-availability; both `.so` carry 0x4000 LOAD alignment (verified this audit) |
| Sanitizer / static-analysis build type | (none) | PLANNED_NOT_OWNER_APPROVED | audit recommends; not present |
| Sculpt Radius/Strength visual reduction | STATUS ("deferred P2") | DEFERRED/OUT_OF_SCOPE | carried UI item |
| "The sketch grid is fixed at 0.25 m/1 m" (debt) | STATUS Technical Debt | SUPERSEDED | adaptive since CAD-A3 — corrected |
| "The top view sits at the pitch clamp" (debt) | STATUS Technical Debt | SUPERSEDED | exact `frameSketchView` since CAD-A3 — corrected |
| "ARCH-HEALTH-01: unify geometry extraction before the next major feature" | STATUS Technical Debt | STALE/CONTRADICTORY | the third representation arrived unmerged; re-timed LATER, F-13 |
| "opens with a single body", "Undo drawn only in Construction", "no Sketch grid", "no arc/spline", "two representations", "no camera read-back", "five sketch tools" | PRODUCT/ARCHITECTURE/STATUS | STALE/CONTRADICTORY | corrected in the docs commit (see `MARKDOWN_TRUTH_MATRIX.md`) |
| No engine, no third-party runtime lib, NDK pinned, no OBJ/FBX drawn control | CLAUDE hard rules | IMPLEMENTED (enforced) | verified: no runtime dep, NDK unchanged, no reserved control (`setChipReserved` now dead — F-18/DEAD_CODE) |
