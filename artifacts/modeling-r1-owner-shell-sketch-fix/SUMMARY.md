# SUMMARY — MODELING-R1-OWNER-CORRECTION-PROJECT-SHELL-SKETCH-SUPPORT-PLANAR-R1

**Result: `PASS-MODELING-R1-OWNER-SHELL-SKETCH-CORRECTION`** — TECH PASS / OWNER
PHYSICAL REVIEW REQUIRED / NOT MERGED.

Task branch `feature/modeling-r1-owner-shell-sketch-fix-r1`, created at exactly
`7e0224056c18bcff2446487e526cd5a703b1d36b`. Last product-code commit `4a393fc`;
the DEVICE union and the final CI FAST ran on `01c3942` (test-only commits after
`4a393fc`, so the product is identical). `main` stays `6c9f156`,
`feature/cad-v6-sketch-face-r1` stays `bbae765`; no FullSharded. Read first:
`OWNER_FEEDBACK.md`, `BEFORE.md`, `REPRO_BEFORE.md`.

## What the OWNER reported, and what each turned out to be

| OWNER report | Root cause (proved) | Correction |
| --- | --- | --- |
| "New Sketch and choosing a face disappeared once I have a 3D object" | The act existed (Add Primitive → New Sketch → spatial chooser) but was reachable only from the Objects capsule's **+**, and every face a **Cut** made was ineligible (`featureEligible = operation != Cut`), so a pocket floor or wall could not be chosen at all | New Sketch in the new project drawer (same handler); per-face eligibility; Cut faces framed material-outward and valid only where they survived |
| "Error when I select the shapes inside" (`Those areas meet only at a point…`) | NOT a stale status: an edge-connected selection whose two unchosen inner cells meet at one node passes that node twice; `mergeEdgeConnectedFaces` refused on the first revisit (`PinchedSelection`). Reproduced on the baseline: 45 of 253 "all but two" selections of a 23-cell OWNER-like sketch | The union loop is SPLIT at the node into one outer and its holes, each with its own vertex ring — valid, one solid, exact area |
| "No ForgeShape logo top-left that opens a slide-out panel with save/load; no Create New during work" | The project opener was a generic `ic_project` icon in the TRAILING group | The ForgeShape mark leads the toolbar and opens a left project drawer (Create / Project / Transfer / Import / Application) |

A fourth defect was found by the device class: during the support chooser the
retained-sketch **Edit Sketch** chip still stood on the body's sketch anchor and
took the tap meant for the face under it (DEV-OSS-03 attempt 2). It is now
withdrawn while the chooser is up.

## Changes

**Native (`app/src/main/cpp`)**

- `forgeshape_sketch_arrangement.cpp` — `mergeEdgeConnectedFaces` completes every
  walk and splits it at revisited nodes (stack split, deterministic, canonical
  rotation per piece); only a group that is not one outer plus holes stays
  `PinchedSelection`. Selections that merged before are bit-identical.
- `forgeshape_cad_feature.{h,cpp}` — `CadFeatureFace::eligible` is per face (flat
  Extrude caps and straight sides, any operation; never a curved side or a
  Revolve face); `lineageEligible` carries the frozen R1 bit the §7c/§7f
  signature mixes; `materialOutward` reverses a Cut face's `n` and `v`.
- `forgeshape_scene.cpp` — `validateCadFaceSupport` accepts a Cut face only when
  the producer's published regeneration carries it (`SupportFaceLost`).
- `forgeshape_owner_shell_selftest.{h,cpp}` — the `OSS_*` host checks, run in the
  CAD-feature suite (no new startup token; 25 tokens unchanged).
- Partial Revolve caps stay ineligible: their tokens are stable but R1 gives them
  no exact frame (`cap.frame = g->placement`, a placeholder), and a revolved body
  takes no later feature — so OSS-10 pins that nothing pretends otherwise.

**Android (`app/src/main/java`, `res`)**

- `GlobalToolbarView` — the ForgeShape mark (`ic_forgeshape_mark`, id
  `project_actions_button`, 48 dp, own capsule `toolbar_mark_group`) leads the
  row; the trailing project icon is removed; `ToolbarRowBudget` reserves the
  mark before fitting the transition.
- `ProjectActionsPopoverView` — now the project drawer: leading-edge anchor,
  leading-top pivot, header (mark + product name), scrolling rows built from
  `ProjectDrawerPolicy.ROWS`; `showNewSketchAvailable`.
- `ProjectDrawerPolicy` (new, pure Java) — groups, rows, `perform` (close first,
  then the ONE existing handler), `markShown`, `newSketchShown`.
- `EditorWorkspaceView` — wires the closer and `onNewSketchRequested` (→ the
  existing `onNewSketchSpatial`), and New Sketch availability per refresh.
- `CadExtrudeCanvasView` — the Edit Sketch chip is absent while the support
  chooser is active.

**Format**: none. No layout, section, version, fixture or lineage token moved;
`DATA_PACKAGE_SPEC.md` §7f/§7g now state the frozen lineage bit beside the
per-face support rule, and the self-touching split.

## The point-touch reproduction and its resolution

Baseline (`REPRO_BEFORE.md`): 23 faces summing to 24.000000 m²; selecting all but
{0, 4} (21 faces) → one edge-connected group, node 14 at (−0.6, −0.8) passed
twice → `PinchedSelection` → candidate `PlanarFacesTouchAtPoint`, selection
intact (21). After the fix, the same three selections → `Ok`, one component,
volumes 16.477 / 16.841 / 16.361 m³ (polygon areas × 1 m), `ALL_BUT_TWO_PINCHES 0`.
Device (DEV-OSS-04): an OWNER-like sketch of 9 entities (rectangle, six circles,
a spline, a rectangle) → 22 cells; four real probe taps found the four cells
around the circles' crossing (a-only, b-only, lens, outside); 20 cells chosen by
real taps (all but the two inner cells meeting at the node), every tap
`FORGESHAPE_SKETCH_TAP:resolved`, no cell dropped, candidate `Ok`, status
"Regions in the extrusion: 20.", committed 1 component, volume 21.7923 m³
against an analytic area of 21.7953 m² (tessellation).

## Tests

**Host** (`scripts/host-native-selftests.sh`): `HOST_SELFTESTS_OK (4304 checks,
0 failed)`, 25 suites. New: `OSS_03`..`OSS_18` (+ `OSS_06a`). Restated to the
corrected contract: `MF_15`, `MF_15B`, `MF_15C`, `CADV6S2_REG_04b`,
`CADVS_OPS_04`, `CADVS_OPS_15`.

**JVM**: 179/179 (170 before + `ProjectDrawerPolicyTest` 9: OSS-01/02, groups,
availability, leading anchor, 48 dp, old opener absent, narrow row).

**Builds**: debug, androidTest, release green locally (NDK 29.0.14206865);
release self-test guard PASS (0 `SelfTests` symbols / 0 strings in release).
**Corpus**: 72 fixtures, verdicts byte-identical before and after
(`corpus_verdicts_before.txt` / `_after.txt`); golden digests unchanged (PROJECT
suite).

**Focused DEVICE** (`OwnerProjectShellSketchSupportTest`, 5 cases), attempts:

| run | head | result | cause | fix |
| --- | --- | --- | --- | --- |
| 37490459826 | `94a21dd` | 0/5 | harness: a message helper re-entered `onWorkspace` on the UI thread | build names on the UI thread |
| 37492151914 | `2694606` | 3/5 | **product**: the Edit Sketch chip took the pocket-floor tap during the chooser; harness: undo tap after the camera moved | chip absent during the chooser (`4a393fc`) |
| 37494000556 | `4a393fc` | 3/5 | harness: a 0.2 m drag snapped to one grid point; stale probe pixel | drag ≥ 1 m then type; re-project |
| 37495733628 | `682fbce` | 3/5 | harness: a big cell's interior off-screen; pocket centre assumed (0,0) | tap at probe points; read the drawn centre |
| 37497383540 | `beae62d` | 4/5 | harness: feature view moved a cell off-screen | real two-finger pan back |
| **37499163269** | `fcc1a10` | **5/5 PASS** | | |

**Final DEVICE union** on `01c3942` (the workflow caps instrumentation at
2700 s and cancels a second run on the same ref, so the union ran as two
sequential dispatches): attempt 37500587693 (10 classes in one) timed out at 102
tests with 2 failures, both stale assertions already false at the baseline —
HomeFlowTest expected 3 New Project entries (4 kinds + Cancel = 5 since
`MODELING-FOUNDATIONS-R1`) and UIR4B-08 expected 6 anchored surfaces (8 since
History and Surface Finish) — restated in `01c3942`. Then:

- **37507824851 — PASS 67/67**: `OwnerProjectShellSketchSupportTest` 5,
  `CadMultiFaceOwnerTest`, `CadFillPickR2Test`, `CadHud3dOwnerTest`,
  `CadRevolveOwnerTest`, `CadSketchDraftingOwnerTest`,
  `CadParametricHistoryOwnerTest`, `HomeFlowTest`,
  `EditorWorkspaceProjectActionsTest`.
- **37513387645 — PASS 37/37**: `EditorWorkspaceCorrectionTest` (includes
  UIR4B-09, the drawer's leading pivot).

Every run: 25/25 startup tokens in order, `NATIVE_VIEWPORT_OK`, 0 failure
lines. Emulator evidence closes no physical-device gate.

**Final CI FAST** — run **37516245911** on `01c3942`, after the DEVICE union
was green: success. Build + JVM unit tests + debug/androidTest/release APKs;
release self-test guard PASS (release x86_64 and arm64-v8a: 0 symbols, 0
strings); `.forge` corpus parity **72/72 byte-identical**; device-free runner
and device-guard checks PASS; whitespace check PASS.

## Exact APK

| field | value |
| --- | --- |
| product SHA | `4a393fc` (last product-code change; `01c3942` is product-identical) |
| FAST run / head | `CI FAST` **37516245911** on `01c3942493e1557cf572d8414f72b4f3bc3e5877` |
| artifact | `ci-fast-evidence`, id **11437665909** |
| artifact ZIP digest | `sha256:29842e9863768bd4b77968cf9c78c3374c6a6c8d5fe54384c7578899b7e4882f` |
| artifact expiry | 2026-10-20T19:10:54Z |
| path in the artifact | `app/build/outputs/apk/debug/app-debug.apk` |
| APK bytes | **17,169,462** |
| APK SHA-256 | `b557827d56c6979cf46c3451894b38717d6c882faa7610cfbc74f088113411cb` |

Taken from that FAST artifact; no substitute rebuild.

## OWNER re-review checklist

1. Open any project: the ForgeShape mark is in the top-left corner; tap it —
   the drawer unfolds from it down the left edge; tap it again to close.
2. In the drawer: New Sketch, New Project…, Save Project, Open Saved Project,
   Save Copy…, Open File…, Share Diagnostics…, Import GLB…, Settings….
3. There is no other project icon at the top right.
4. With a CAD body: mark → New Sketch → tap the body's top, tap again → sketch
   on it; finish, Extrude. Also try a side.
5. Cut a pocket, then mark → New Sketch → tap the pocket's FLOOR (and a straight
   wall) → sketch there; Add rises into the pocket, Cut sinks below it.
6. A round pocket's curved wall is not offered.
7. Re-create the screenshot sketch; choose every cell except two inner cells
   that meet at a point: no "meet only at a point" message, the count shows,
   Extrude makes one body with those two left open.
8. Choose two cells that touch only at a corner: two separate pieces, one body.
9. On a narrow phone: the mark, the transition (Finish Sketch / Extrude /
   Revolve / Back to Construction) and Display / Hide UI all stay reachable.
10. A brand-new CAD sketch (before the first Extrude) shows Back to Home and no
    mark — there is no project yet.
