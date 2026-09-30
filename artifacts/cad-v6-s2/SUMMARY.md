# CAD-V6-S2-PLANAR-RUNTIME-R1 — a sketch whose curves cross extrudes its faces

This is the intermediate branch `feature/cad-v6-sketch-face-r1`, continued from
`f91c6dd`, where C1 closed. **It is not merged to `main`, by design, and S3 has
not started.** `BEFORE.md` (commit `873afc5`) was written before any product
edit.

## What changed

1. **Fragment side tokens.**
   - `CadFaceToken` gained `fragment`, `fragmentStart` and `fragmentEnd`, which
     are `ArrangementCut`s moved into `forgeshape_sketch.h`.
   - A union-boundary fragment that is its whole source edge
     (SourceStart→SourceEnd) keeps the legacy token and the legacy code. A
     proper piece gets code `0x03 << 56 | (FNV-1a 64 over entity, local,
     start, end) & (2^56 − 1)`.
   - `sameCadFaceToken` is now field equality. It used to be code equality.
   - This fixes the S1 defect in which two pieces of one rectangle side wore
     one token (`CADV6S2_TOK_01..03`).
2. **Format.**
   - `CADB` v6 FACE code 4 is a fragment side plus two cuts. It is v6-only:
     v1..v5 readers refuse it, and a fragment token anywhere makes a state not
     legacy-representable.
   - Codes 1..3 are unchanged. There is no v7.
   - Spec: `DATA_PACKAGE_SPEC.md` §7g. The PowerShell encoder reimplements it
     in `Add-CadFaceV6`, `Get-CadFragmentTokenCode` and `Get-CadLensLineage`.
   - One new fixture, `cad_fragment_support_v6.forge` (531 bytes, SHA-256
     `e1726cb5…acb6`), brings the corpus to **57/57**. **All 56 earlier
     fixtures are byte-identical, including the 44 legacy ones.**
3. **Regeneration.**
   - `mergePlanarFaces` unions the chosen faces on the arrangement's own
     half-edges:
     - a shared fragment cancels;
     - holes are owned by the smallest containing outer loop;
     - a reused node is `PinchedSelection`, which surfaces as
       `OverlappingRegions`.
   - `derivePlanarFeature` emits CapPlane, CapFar, then one Side per boundary
     fragment. Curved sides are ineligible, and so is every face of a Cut.
   - `appendPrism` builds the solid for both selection kinds.
   - New Body, Add and Cut all regenerate. `PlanarFaceRegenerationUnavailable`
     is retired; its number is kept.
4. **Load.** `runtimeCanEvaluateProject` no longer refuses planar faces, so a
   PlanarFaces project opens.
5. **Session.**
   - `sketchRequiresPlanarFaces` is the one predicate that chooses the mode at
     Finish.
   - In PlanarFaces mode:
     - a tap toggles the face under the finger;
     - JNI lists faces by transient handle;
     - an exact loop region maps to its identical face or is refused
       (`PlanarFaceUnresolved`).
   - Preview and commit are the same evaluation.
   - Reopen and Edit Sketch re-resolve the stored refs exactly. A topology edit
     that loses a ref is refused by name and changes nothing.
   - The overlay hatches the actual chosen cells.
   - New natives: `sketchSelectionKind()` and `cadFeatureSelectionKind()`.
     Tool-state slot 29 is the selected-area count. No Java-side id cache was
     added.
6. **Stale support chooser (was debt).** `refreshChosenSupport` re-validates
   the aimed support at confirm and re-frames it. A stale support is refused
   (`FORGESHAPE_SUPPORT_CHOOSER_STALE`), and a scene-changing Undo/Redo cancels
   the chooser (`CADV6S2_CHOOSER_01`).
7. **Shared sketch.** An edit that loses one feature's face refuses the whole
   edit (`CADV6S2_SHR_01`).

## Tests

**Host.** `HOST_SELFTESTS_OK (3934 checks, 0 failed)`, of which CAD_FEATURE has
305. The `CADV6S2_*` checks and what they cover:

| Area | Checks |
| --- | --- |
| Tokens | `TOK_01..03` |
| Faces and lineage | `FACE_02..06` |
| Regeneration | `REG_01..05` |
| Session | `SES_00..03`, `SES_06..11` |
| Shared sketch | `SHR_01` |
| Chooser | `CHOOSER_01` |
| Codec and fixture | `P16`, `P17` |

Earlier checks whose expectation S2 deliberately changed were flipped **by
name**: `CADR0_21_S2`, `CADFC1_SES_07_S2`, `CADFC2_PF_01f_S2`,
`CADFC2_PF_02e_S2`, `CADV6_F03_S2`, `CADV6_F04_S2` and `CADV6_P13_S2`.

**Device.** `CadPlanarFaceRuntimeTest` runs six journeys. Each is asserted on
the stored selection kind and the regenerated solid's volume and shell count:

- **J1:** circle crossing a rectangle, lens → New Body.
- **J2:** every face → the protrusion.
- **J3:** two circles, lens.
- **J4:** union across a fragment → the disk.
- **J5:** save/reopen, byte-identical re-encode, Edit reopens the same face.
- **J6:** a face selection on a cap → same-body Add, then Cut.

## Gates

- **Local:**
  - host 3934/0;
  - NDK debug + release built;
  - release guard `RELEASE_SELFTEST_GUARD=PASS` (0/0 in release);
  - JVM unit tests pass;
  - androidTest compiles;
  - corpus 57/57.
- **CI DEVICE** (focused: CadPlanarFaceRuntimeTest, CadVerticalSliceTest,
  SketchExtrudeTest, JniBoundaryHardeningTest): _pending_.
- **CI FAST** (the one final run): _pending_.
- **OWNER APK:** _pending_.
- **FullSharded: NOT RUN** (out of S2 scope).

## Not this stage

- Retained-sketch mobile UX (S3): creating, sharing, deleting or browsing
  sketches from the UI.
- Physical-device review, which is OWNER REVIEW REQUIRED.
