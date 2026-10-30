# Triage of the one failure in CI DEVICE 40983965648 (72/73)

`EditorWorkspaceMobileTest#uir4a04_addPrimitiveOffersTheSixRealPrimitivesAndNothingElse`
failed at `EditorWorkspaceMobileTest.java:274`: `expected:<8> but was:<12>`.

- The assertion counts every visible clickable tile of the Add palette and
  expects 6 primitives + New Sketch + the by-name fallback = 8.
- The palette now also draws three Freeform tiles (`add_freeform_box/plane/
  cylinder`, commit `d1a0671`, 2026-10-06) and one Surface tile (`add_surface`,
  commit `7b4c7a4`, 2026-10-06) — `AddPrimitivePaletteView.java:102, 206` —
  which `MODELING-FOUNDATIONS-R1` authorised (CLAUDE.md: Freeform and Surface
  rules; New Project offers CAD, Sculpt, Freeform and Surface). 8 + 3 + 1 = 12.
- The test file was last changed 2026-09-27 (`fd88db8`), before those commits.
  The last exhaustive aggregate (`FULL_SHARDED` on `fa0b6b4`, 2026-09-29)
  predates them too (`git merge-base --is-ancestor d1a0671 fa0b6b4` → false),
  and no run of this class on a later tree is recorded — so the stale
  assertion went undetected for ~3.5 weeks.

Classification: **TEST_HARNESS_DEFECT (stale count assertion), not a product
regression**, confidence E1 (emulator run) + E2 (source/commit dates). The
other 72 tests of the three classes passed, incl. all 46 `EditorWorkspaceGizmoTest`
and 12 `Ui3dStateCorrectionTest`. Not fixed here (audit only).
