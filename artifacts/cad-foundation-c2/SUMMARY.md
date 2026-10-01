# CAD-FOUNDATION-C2 — one action panel at the arrow tip; planar-face preflight

Result: **`PASS-CAD-FOUNDATION-C2-HUD-PLANAR-BLOCKED`.** The HUD is corrected;
planar faces stop at `BLOCKED-CAD-C2-PLANAR-FACE-IDENTITY` with the next model
fully specified (`PLANAR_FACE_BEFORE.md`, `PLANAR_FACE_MODEL_PROPOSAL.md`).

Baseline `origin/main = 3872120`. Branch
`feature/cad-foundation-c2-owner-correction-r1`. Tested product candidate
`cc99400` (product `f69f24e` + the correction `cc99400`; `937ee6d` and the
closeout commits are docs only).

## OWNER findings → code

| OWNER finding (physical phone, C1) | Cause in C1 source | C2 change |
| --- | --- | --- |
| Value `2.3593521118164062 m` | `LengthUnit.formatWithUnit` printed `BigDecimal.valueOf(double)`: the shortest round-trip decimal of a dragged binary64 | Labels take the ONE bounded display rule the area label already had: 3 decimals in the display unit, half-up, zeros stripped (`2.359 m`). `format` (editor seed) keeps every digit; native keeps the double. Also fixes the Line and body-dimension labels. |
| Value dominates while the model is tiny | Floor 11 sp independent of the glyph band, plus the 19-character raw string | `valueTextSp = clamp(14 sp × visualScale, 9, 18)` — the panel's own multiplier; the whole annotation collapses at the scale floor when the value is wider than its leader (`annotationCollapsed`). |
| Icons shrink/disappear independently | `layoutLeader` placed each glyph at its own point and hid each one whose point left the screen | ONE plate, shown whole or not at all (`layoutPanel`). |
| Icons move apart; spread around the body at close zoom | Glyph spacing was `max(glyph + gap, 48 dp / |dir|)` along the leader, from the leader's visible midpoint — the 48 dp floor spread the drawing | The plate is laid out once at its reference size and `setScale`d as one unit; its internal spacing is a fixed multiple of the glyph at every zoom. |

## HUD architecture

- **Value**: above the leader, rotated and upright — unchanged from C1.
- **Action panel**: `cad_extrude_panel_plate` (extent, operation, Flip in One
  Side) at reference size, pivot (0, 0), scaled by `visualScale` (0.40..1.60),
  NOT clickable. `cad_extrude_panel` is ONE invisible, unscaled group proxy,
  `max(48 dp, plate)` rounded up to whole pixels, covering the plate; it opens
  ONE action palette (`cad_extrude_actions_palette`: extent row, operation row
  of what native offers, Flip) of full-size screen chrome.
- **Tip anchor**: native slots 42..44 = the primary arrow's DRAWN POINT from
  `cadExtrudeArrowPoint`, the one function the drawing and the hit test also
  end the head at. The proxy stands one corridor (24 dp + 4 dp) past it along
  the arrow; else beside it (away from the leader, then toward it), allowed to
  slide back along the shaft by at most its own length; else hidden whole. It
  never enters the arrow's grab corridor and is never clamped away.
- **Hit targets**: one group proxy was chosen over three overlapping 48 dp
  proxies because the glyph pitch is 30 dp × scale (< 48 dp below scale 1.6);
  deterministic "nearest glyph" ownership would give the middle icon a
  12..30 dp target. Cost: Flip is two taps (panel, Flip) instead of one.
- **Ownership**: pure policy in `CadHudPresentation`; placement only in
  `CadExtrudeCanvasView`; `EditorWorkspaceView` unchanged (0 lines). No
  renderer text, no Java CAD truth, no format change.

## Zoom (CI DEVICE `36744737415`, perspective, 2 × 1 m rectangle, depth 1 m)

| Level | Distance | Scale (clamp) | Model (profile) | Panel plate | Proxy | Placement | Tip→panel centre | Value | Leader | Collapsed |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| close | 4 | 1.600 (high) | 320 dp | 154.8 × 58.5 dp | 155.0 × 58.7 dp | beside (slid) | 98.3 dp | "1 m" 18 sp, 30.5 dp | 187.2 dp | no |
| normal | 9 | 0.866 | 141 dp | 83.8 × 31.7 dp | 83.8 × 48 dp | past the tip | 76.2 dp | 12.1 sp, 20.2 dp | 75.8 dp | no |
| far | 18 | 0.433 | 71 dp | 41.9 × 15.8 dp | 48 × 48 dp | past the tip | 60.3 dp | 9 sp, 14.9 dp | 36.6 dp | no |
| very far | 60 | 0.400 (low) | 21 dp | — | — | hidden whole | — | hidden | 10.7 dp | **yes** |

Screenshots: `HUD_ZOOM_CLOSE_NORMAL_FAR_VERYFAR.png` (from the run's
`zoom_*.png`). Plate/proxy ratio constant, panel shrinks monotonically, value
never wider than its leader at the floor, no partial panel at any level.

## Evidence

| Gate | Where | Result |
| --- | --- | --- |
| JVM | `:app:testDebugUnitTest` | 116/116 (`CadHudPresentationTest` rigid group, tip anchoring, spacing invariant, whole-panel visibility, edge slide, collapse, one-proxy ownership; `LengthUnitTest` display precision) |
| Host native | `scripts/host-native-selftests.sh` | `HOST_SELFTESTS_OK (3813 checks, 0 failed)`; CAD_FEATURE 168 → 184 (PF-01..04) |
| `CI FAST` | `36742635591` on `f69f24e`; `36744733518` on `cc99400` | success, success; `FORGE_CORPUS_PARITY=PASS (44/44 byte-identical)` |
| `CI DEVICE` attempt 1 | `36742639778` on `f69f24e` | `FAIL-CI-CLOUD-DEVICE-PRODUCT` 3/46, one real defect: the proxy was sized from the dp sum while the plate rounds per child (254 vs 252 px) — fixed in `cc99400` (layout takes the MEASURED plate, proxy rounded up) |
| `CI DEVICE` attempt 2 | `36744737415` on `cc99400` | **PASS**: 23/23 tokens in order, `OK (46 tests)`, 503 s; union list `CadCanvasExtrudeTest`, `CadVerticalSliceTest`, `CadExtrudeExtentTest`, `SketchExtrudeTest`, `Ui3dStateCorrectionTest` |

New device cases: `cadFoundationC2_theActionPanelStaysOneUnitAtTheTipAtEveryZoom`
(the four levels) and `cadFoundationC2_aRealTapOnThePanelOpensThePaletteAndItsFlipActs`
(real `MotionEvent`s through the window: a tap on the drawn extent glyph opens
the palette; a tap on its Flip reverses the side, depth unchanged).

**FullSharded NOT RUN** (TEST-OWNER-04). Emulator evidence closes no
physical-device gate.

## Persistence

No `CADB`/`SCNE` byte, section, version, codec or fixture changed; corpus
44/44 byte-identical in CI; the HUD writes nothing to a project.

## OWNER review APK

`CI FAST` `36744733518` (artifact `11112660566`, expires 2026-10-14), built from
`cc99400`: `app/build/outputs/apk/debug/app-debug.apk`, **10,910,129 bytes**,
SHA-256 **`81f5cd73195e4c59af11d89161a3881b30d9916ba8844fb693a148dcb4882b9e`**.

OWNER checklist: (1) the panel reads as one unit at the tip at every zoom;
(2) its size band feels right (plate 36 dp tall at scale 1, 0.40..1.60);
(3) the value `2.359 m`-style text is legible and never dominates; (4) tapping
the small panel at far zoom is reliable; (5) two taps for Flip is acceptable;
(6) the close-zoom "beside the tip" placement reads as attached; (7) the
very-far collapse point feels right.

## Not started

Planar-face implementation, `CAD-SKETCH-IDENTITY-R1`, `CADB` v6, Through All,
Revolve, projected edges.
