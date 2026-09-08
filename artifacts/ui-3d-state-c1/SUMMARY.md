# UI-3D-STATE-C1 — Summary

**Result:** `PASS-UI-3D-STATE-C1-OWNER-LATER`

The six correction findings of `UI-3D-STATE-AUDIT-R1` are closed with runtime
evidence on the authoritative device. `UI3D-F-005` is **not** fixed, was not
touched, and stays open with its original provenance.

| | |
| --- | --- |
| **Start HEAD** | `7aaf77a01a9ec585c6675159e46fb6b6d07cf53d`, clean worktree, no remote |
| **Device** | `ForgeShape_Stage006` / `emulator-5580`, 1080 × 2400, density 2.625. Identity confirmed by `emu avd name`; `emulator-5554` was never contacted and is not attached |
| **Runner** | `scripts\run-instrumented-tests.ps1 -Serial emulator-5580 -TestClass …`, `MODE=FOCUSED_SUBSET`. No `-FullSharded`, no aggregate marker claimed |
| **Lifecycle matrix** | 117 assertions, 117 executed, **117 PASS / 0 FAIL** (was 111/6) |
| **Spatial** | 72 rows, **60 measured, 60 PASS, 0 FAIL, 0 stale anchors**; median **0.16 dp**, max **0.25 dp** (was median 48.90 dp, max 282.53 dp) |
| **Native self-tests** | all 22 `*_SELFTEST_OK` tokens plus `FORGESHAPE_NATIVE_VIEWPORT_OK`; body dimensions 97 → **102 checks** |

## The two root causes, and what replaced them

### R1 — a viewport anchor was written as a translation inside a padded container

Native projects a world point into **viewport-content pixels**: the origin is the
rendered surface, which is the window, because the Vulkan viewport is full-bleed.
The three placing views are children of `overlayRoot`, which carries the chrome's
window-inset padding, so a `setTranslationY` computed straight from that anchor
was drawn one inset too low — a constant **+128 px = 48.8 dp** on this device
(`UI3D-F-001`) — and the CAD cluster additionally clamped into that padded
content box rather than into the viewport (`UI3D-F-006`).

`ViewportAnchorSpace` is now the ONE conversion, and it is pure runtime geometry:

```
translation = anchor + (viewportOriginInWindow
                        - parentOriginInWindow
                        - placedLayoutPositionInParent)
```

No status-bar height, no navigation-bar height, no density constant, and no
assumption that the horizontal inset is zero. The clamp bound is the real
viewport `(0, 0, w, h)` carried through the same offset. All three placing views
call it, so there is one contract rather than three that can drift.

Two placement details fell out of the same rework and are fixed with it: a
surface is measured under the **constraint its parent will impose** rather than
unconstrained (an unconstrained measure of a wide cluster answers a size layout
never produces, and centring it shifts the control sideways), and a surface
placed in the very pass that first lays it out is re-placed once — bounded, and
reset the moment a pass reads a settled layout.

### R2 — world-anchored chrome had no refresh driver, and its anchors were a frame old

Two independent halves, both closed:

**The shell** now has one refresh path for every anchored surface —
`refreshWorldAnchoredUi()` — and `onViewportGestureMoved` drives all four rather
than only the extrude cluster, which is exactly why only that one tracked the
camera before (`UI3D-F-002`). It is deliberately cheaper than `syncFromNative()`
and rewrites no exact-value editor, so it is safe on every pointer sample.

**Native** stopped answering out of the render thread's cache. The label anchors
are derived for the instant the chrome asks (`bodyDimensionLabelAnchors`, a pure
read over values), so they exist before a frame has drawn the leaders
(`UI3D-F-003`) and belong to the body being measured now rather than the previous
one (`UI3D-F-007`) — the same race with its two opposite outcomes. The cached
`labelAnchors_` was **deleted**, so a session is now structurally incapable of
holding a stale anchor. And "is this mode still measurable" is now **one named
predicate** asked by the render thread and every chrome read alike, so a mode
whose body has been hidden, locked, deleted or handed to Sculpt reads as closed
at the instant of the transition instead of one frame later (`UI3D-F-004`).

## Before → after

| | before | after |
| --- | --- | --- |
| lifecycle assertions | 111 PASS / **6 FAIL** | **117 PASS / 0 FAIL** |
| spatial rows measured | 50 (all FAIL) | **60 (all PASS)** |
| spatial rows not measurable | 12 | **0** |
| median measured error | 48.87 dp | **0.16 dp** |
| max measured error | 282.53 dp | **0.25 dp** |
| stale-anchor rows | 7 | **0** |
| clamped-row max residual | 179.63 dp | 65.96 dp |

The six lifecycle rows that failed were all `S19`, and they map exactly onto the
findings: four are `UI3D-F-003` (`dimensions_open` ×3, `after_orbit`) and two are
`UI3D-F-004` (`sculpt_entered`, `active_body_hidden`).

**On the clamped rows.** Twelve rows are `CLAMPED`, and a clamped row's distance
from its anchor is correct rather than stale: the anchor is outside the viewport
and the control is deliberately held inside it. Every CAD clamped row's residual
fell by 60–75 % (99.53 → 26.36 dp is typical) because the bound moved from the
padded box to the viewport, which is `UI3D-F-006` closing. The two new clamped
rows (`UI3D-06 / dimensions_on_body_B`) were `NOT_MEASURED` before **because the
labels were absent**; they are now drawn and correctly held at the edge.
`UI3DC1-04` asserts separately that a clamped surface stays inside the viewport.

## `UI3D-F-005` is untouched and still open

`SketchOverlayStyle::Dimension` still has no case in the renderer's overlay
switch and no `default:`, so the active-axis leader and the sketch line
annotation still render fully transparent. It is a different layer and a
different root cause, `§4` forbids fixing it here, and `git diff` proves
neutrality: `forgeshape_renderer.cpp`, `forgeshape_gizmo.cpp`,
`forgeshape_sketch_overlay.h` and every file under `shaders/` are **byte
unchanged**. `after_ui3d07_01_line_selected.png` still shows the numeric chip
with no leader geometry, exactly as the audit recorded — the chip now stands on
the annotation instead of one inset below it.

## Product and data neutrality

No `.forge` byte, section, version, fixture or corpus file moved; no history
step, revision, checkpoint or fingerprint is involved; `DATA_PACKAGE_SPEC.md`,
`scripts/build-forge-corpus.ps1` and all thirty-six fixtures are untouched. The
CAD extent contract, the dimension anchor solver and every domain semantic are
unchanged: the native edits are a read that derives what a cache used to hold, a
predicate named once instead of copied four times, and the deletion of a cache
that now has no reader.

`assembleDebug` PASS, `assembleRelease` PASS, `verify-device-guards.ps1` all
PASS.

## Deviations and test limitations

1. **Activity recreation is still not exercised.** The audit recorded that gap
   and this correction does not close it; `§4` puts it out of scope.
2. **One window size only** (compact portrait). The conversion is written to
   absorb an arbitrary left/right inset and carries no constant, but a landscape
   or expanded window was not measured.
3. **The bounded post is a real dependency on Android layout timing.** A surface
   made visible in the same pass that first lays it out cannot know its container's
   position until layout runs; the correction places the best answer available and
   repeats once. It is capped at three and reset by any settled pass, so it is
   layout readiness and not a poll — but it is a retry, and it is stated rather
   than hidden.
4. **Clamped rows are classified, not measured**, on the audit's own model and
   with the audit's own predicate. None is silently dropped: all twelve appear in
   `SPATIAL_BEFORE_AFTER.tsv` with both verdicts.

## Artifacts

```
artifacts/ui-3d-state-c1/
  SUMMARY.md                  this file
  FINDING_CLOSURE.md          one row per closed finding: before, cause, fix, after
  SPATIAL_BEFORE_AFTER.tsv    all 72 rows, both verdicts, joined on case+action+surface
  SPATIAL_AFTER.tsv           the re-run's own spatial ledger
  STATE_MATRIX_FINAL.tsv      the 117 lifecycle assertions, all PASS
  NOTES.tsv                   the re-run's diagnostic reads
  EVIDENCE_INDEX.md           every artifact and what it shows
  OWNER_LATER.md              the four questions automation cannot settle
  screenshots/                before_* from the audit, after_* from the re-run
  logs/audit-rerun.log        the strict re-run
```

**No next feature or second correction family was started.**
