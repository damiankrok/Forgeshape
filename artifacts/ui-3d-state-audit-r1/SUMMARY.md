# UI-3D-STATE-AUDIT-R1 — Summary

**Result:** `PASS-UI-3D-STATE-AUDIT-R1-WITH-FINDINGS`

The audit completed its whole required matrix with reproducible evidence and
made **no product fix**. Seven findings stand, in three root-cause families;
none is P0, and none touches project truth.

| | |
| --- | --- |
| **Start HEAD** | `dfcab1afd1361d37b6dfe4607772a5597f31d043` (unique match for the reported prefix `dfcab1afd1361d37b6dfe460`, clean worktree, no remote) |
| **Device** | `ForgeShape_Stage006` / `emulator-5580`, 1080 × 2400, density 2.625. Identity confirmed by `emu avd name`; `emulator-5554` never contacted and not attached |
| **Runner** | `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass com.forgeshape.app.Ui3dStateAuditTest`, `MODE=FOCUSED_SUBSET`. No `-FullSharded`, no aggregate marker claimed |
| **Attempts** | 2 of the 2 §7 allows; **3 min 20 s** of automated runtime against a 20-minute target and a 30-minute hard stop |
| **Inventory** | 43 dynamic surfaces classified, 5 exclusions with reason |
| **Matrix** | 117 assertions (38 MUST_SHOW, 71 MUST_HIDE, 8 MAY_SHOW), 117 executed, **111 PASS / 6 FAIL**, 0 not run |
| **Spatial** | 5 anchored surfaces measured, 72 rows (60 measured, 12 not measurable, 10 clamped); **median error 48.90 dp**, max 282.53 dp; **7 stale-anchor rows** |

## The answer to the OWNER's reported concern

> *"dimensions/3D UI can remain stale or detach after moving a body"*

**Confirmed, and the mechanism is now named.** The concern is real, and it is
two separate defects wearing one appearance:

1. **The labels never sit on the geometry at all.** Every world- and
   feature-anchored surface in the product stands exactly **one system-bar inset
   (128 px = 48.8 dp on this device) below** the anchor native reports, because
   the placing views are children of `overlayRoot`, which carries the chrome's
   window-inset padding, while the anchors are in full-window viewport pixels.
   30 of the 60 measured rows carry that offset and nothing else.
   (**UI3D-F-001**, P1, `ANDROID_LAYOUT`.)
2. **They then go stale, in four different ways**, because world-anchored chrome
   has no refresh driver tied to the frame: it is re-read only when a chrome
   control is pressed, while the domain state it mirrors is decided on the
   render thread every frame. So a camera orbit, pan or zoom leaves the numbers
   behind (up to 119 dp); a body switch leaves them on the **previous body**
   (282 dp — the worst measurement in the run); opening the mode leaves them
   **absent** until an unrelated act; and entering Sculpt or hiding the body
   leaves them standing as **ghost UI** over work they no longer describe.
   (**UI3D-F-002 / F-007 / F-003 / F-004**, P1/P1/P2/P1.)

Moving a body through the exact placement editor is a chrome act, so it does
refresh — which is why the concern reads as intermittent. Moving the camera is
not, and that is the case that always fails.

## CAD dimensions and Extrude controls under camera and body motion

**The CAD canvas surfaces are correct**, and they are correct for a reason the
rest of the product does not share: `onViewportGestureMoved` refreshes the
extrude canvas — and only the extrude canvas — on every pointer sample.

Through a real arrow drag, an orbit, a zoom out to the scale floor and back, a
body Move, Rotate, Scale and a real gizmo drag, the retained `Edit Sketch` chip
(7/7 rows), the Side B value (4/4) and the sketch line dimension label (3/3) all
carry the constant UI3D-F-001 offset and **nothing else** — 48.6 to 49.0 dp,
never more. Side A and Side B stay on their own sides under independent drags,
every extent-mode transition withdraws what it should, and the camera-attached
scale saturates at exactly the authored **0.800** floor.

The one CAD blemish is UI3D-F-006 (P3): the cluster's edge clamp is computed
against the padded container rather than the viewport, so it clamps earlier than
the window requires — a consequence of F-001 rather than a separate cause.

## Renderer visibility of dynamic overlay styles

`SketchOverlayStyle` has five values. Four — `GridMinor`, `GridMajor`, `Axes`,
`Entities` — have a case in the renderer's overlay switch. **`Dimension` has
none, and there is no `default:`**, so `push.highlight[3]` stays `0.0f` and the
shader's `vec4(rgb, pc.highlight.w * weight)` makes every vertex in such a range
invisible.

Both producers were checked and both emit it: the sketch line annotation is
styled `Dimension` **entirely**, and Stage 020M's dimension overlay styles the
**active-axis** range `Dimension` while the other two axes stay `Entities`.

**Adjudicated at runtime rather than assumed.** `ui3d07_01_line_selected.png`
shows a selected straight Line whose dimension native reports (1.6 m), the
numeric chip drawn — and no extension lines, no dimension line and no ticks
anywhere in the frame. `ui3d05_05_after_zoom_out.png` shows the Stage 020M
leaders clearly, because no axis is active there; the invisibility bites only on
the axis the user is editing.

This is **existing recorded debt**, found beside `CAD-UX-S1` and already in
`PROJECT_STATUS.md` → *Known Issues*. Its provenance is preserved; this audit
adds the runtime frame and nothing else. It is **not fixed here**
(**UI3D-F-005**, P2, `OVERLAY_RENDER_STYLE`).

## UI3D-01..17 acceptance

| case | result | note |
| --- | --- | --- |
| UI3D-01 baseline / provenance | **PASS** | the reported prefix resolves to one clean HEAD, and the audit starts from it |
| UI3D-02 inventory completeness | **PASS** | 43 surfaces classified from source plus runtime, 5 exclusions each with a reason |
| UI3D-03 show/hide matrix | **PASS (with findings)** | all required transitions executed; 6 of 117 assertions fail, all on S19 |
| UI3D-04 dimensions under body transform | **PASS (with findings)** | Move, Rotate, Scale, a typed resize on a one-sided anchor and an Undo, each measured |
| UI3D-05 dimensions under camera motion | **PASS (with findings)** | orbit, pan, zoom near and far; UI3D-F-002 |
| UI3D-06 selection / context invalidation | **PASS (with findings)** | A↔B, hide, lock, Delete → Undo → Redo; UI3D-F-004, UI3D-F-007 |
| UI3D-07 CAD selected-line dimension | **PASS (with findings)** | label present and tracking; the overlay geometry is invisible — UI3D-F-005 |
| UI3D-08 staged Extrude attachment | **PASS** | real arrow drag (1.0 → 1.514 m), orbit, zoom; anchor error is the F-001 constant only |
| UI3D-09 Symmetric / Two Sides | **PASS** | correct value counts, sides independent under drag, no stale second value |
| UI3D-10 retained Edit Sketch lifecycle | **PASS** | tracks through Move/Rotate/Scale, a real gizmo drag, orbit and zoom; Edit Sketch withdraws committed-body context; Finish restores the preview for the same body |
| UI3D-11 Sculpt leakage | **PASS (with findings)** | one leak only — UI3D-F-004; nothing else crosses in or out |
| UI3D-12 objects / visibility / lock | **PASS (with findings)** | folded into UI3D-06; locked body exposes no gizmo, hidden body keeps a ghost label |
| UI3D-13 global dismissal | **PASS** | one primary surface at a time, System Back closes the topmost, Relative Scale closes Dimensions, Shape withdraws the Transform members |
| UI3D-14 projection / update freshness | **PASS (with findings)** | every expected anchor re-read AFTER the action; the audit never sleeps and re-observes the same stale point |
| UI3D-15 visual evidence pack | **PASS** | 59 frames, with mechanical expected/actual overlays on every measurable spatial case |
| UI3D-16 release / product neutrality | **PASS** | no production file changed; no observability seam added — see below |
| UI3D-17 final clean tree | **PASS** | harness, evidence and status committed; `git status --short` empty |

## Release neutrality (UI3D-16)

**No production translation unit was touched.** The audit needed no new
observability seam: every anchor it compares against comes from a read-only
debug seam the repository already ships — `bodyDimensionLabelPoint`,
`cadExtrudeToolState`, `cadBodySketchAnchor`, `sketchLineDimension`,
`sketchScreenPoint`, `debugProjectWorld`, `debugCameraPose`, `gizmoHandlePoint`,
`gizmoState`, `bodyDimensionsState`. Where a placed container had no semantic id
of its own, the harness walks the view tree to the child the container moves
rather than adding an id to the product.

Everything added lives under `app/src/androidTest/`, which is not compiled into
the release APK at all, so release behaviour and the export surface are
unchanged by construction. No schema, section, version, fixture or corpus byte
moved; no `.forge` file was written by the audit.

## Deviations and harness limitations

1. **Two matrix errors of this audit's own, corrected between attempts.**
   Attempt 1 marked `sketch_editor` and `clear_mask` `MUST_SHOW` and found them
   hidden. They are **bodies of the one precision surface**, not surfaces of
   their own, so the question was wrong rather than the product. Attempt 2 opens
   the panel from its own toggle first; all such rows pass. Recorded in
   `FINDINGS.md` rather than quietly dropped.
2. **The clamp predictor is approximate.** The harness predicts an edge clamp
   from the **viewport** bounds while the product clamps into the **padded**
   container. Ten rows are therefore marked `CLAMPED` and excluded from the
   error statistic; the direction of the difference is itself UI3D-F-006, so no
   row is silently counted either way.
3. **UI3D-F-003 is a race**, so it is reported as one: it failed in 4 of 5
   places in attempt 2 and 3 of 5 in attempt 1. The race is real in both.
4. **Activity recreation was not exercised.** The `LIFECYCLE_RECREATE` family is
   therefore **not audited**; the existing `EditorWorkspaceLifecycleTest` covers
   rebuild-from-native for the resting chrome, but no case in this audit rotates
   or recreates the Activity with an anchored surface standing. Recorded as a
   coverage gap, not as a pass.
5. **One window size only** (compact portrait). Landscape and expanded windows
   would very likely make UI3D-F-001's horizontal half visible, since a
   landscape navigation bar gives `overlayRoot` a non-zero left or right
   padding. Not measured; stated as an expectation, not a result.
6. **No `OVERLAY_GEOMETRY` attachment error was measured.** Renderer-drawn
   surfaces are rebuilt from the domain every frame and cannot hold a stale
   screen coordinate, so they were audited for visibility and for the
   renderer-style question instead.

## Known debt versus newly evidenced defects

| id | classification |
| --- | --- |
| UI3D-F-001 | **newly evidenced** |
| UI3D-F-002 | **newly evidenced** (the OWNER reported the symptom; the mechanism is new) |
| UI3D-F-003 | **newly evidenced** |
| UI3D-F-004 | **newly evidenced** |
| UI3D-F-005 | **existing recorded debt**, provenance preserved, adjudicated at runtime |
| UI3D-F-006 | **newly evidenced** (a consequence of F-001) |
| UI3D-F-007 | **newly evidenced** |

## Artifacts

```
artifacts/ui-3d-state-audit-r1/
  SUMMARY.md                      this file
  DYNAMIC_SURFACE_INVENTORY.md    43 surfaces, 5 exclusions, the three refresh drivers
  CONTRACT_MATRIX.md              the expected-state matrix and where each expectation comes from
  CONTRACT_MATRIX.tsv             the same, machine-readable, with the measured result
  VISIBILITY.tsv                  117 show/hide rows
  SPATIAL_ATTACHMENTS.tsv         72 attachment rows
  NOTES.tsv                       28 diagnostic reads
  FINDINGS.md                     UI3D-F-001..007, by root cause
  EVIDENCE_INDEX.md               every artifact, every frame, what it shows
  OWNER_LATER_TEST_PACK.md        the three questions automation cannot settle
  screenshots/                    59 frames, with mechanical overlays
  logs/run-01.log, run-02.log     both attempts
app/src/androidTest/java/com/forgeshape/app/
  Ui3dStateAuditTest.java         the nine audit cases
  Ui3dAuditRecorder.java          the ledgers, the geometry and the annotated capture
```

**No product fix or next feature stage was started.**
