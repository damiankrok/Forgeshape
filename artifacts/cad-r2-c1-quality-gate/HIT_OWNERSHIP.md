# CAD-R2-C1 — CHOOSER HIT OWNERSHIP (track B)

## The rule, as the code states it

`SupportChooser::resolve` (`app/src/main/cpp/forgeshape_support_chooser.cpp`)
answers one question per tap: what is the NEAREST support target under this
pixel? The world-plane targets are three ±3 m squares at the origin, drawn as
line overlays with the depth test OFF (`Renderer::createGizmoPipeline`), so
their grid lines show through bodies. Faces come from the ordinary scene pick
(`pickSceneSnapshot`) — what is hittable is what is drawn — and a triangle is
mapped to a SEMANTIC face through the published mesh's face ranges, never by
index. Distances are measured from the camera eye along the pick ray.

| Situation under the pixel | Before `CAD-R2-C1` | After |
| --- | --- | --- |
| Eligible face nearer than every plane | that face (exact `TopoRef`) | unchanged |
| Eligible face coplanar with a plane (within 1 mm) | the face (tie rule) | unchanged |
| Plane nearer than the frontmost surface | the plane | unchanged — **INTENDED-VISIBLE-PLANE-PRIORITY** |
| **Ineligible surface (curved wall, Revolve face, non-CAD body) nearer than a plane behind it** | **the plane behind the surface** | **nothing (None)** |
| Ineligible surface, no plane anywhere on the ray | nothing | unchanged |
| No surface, a plane on the ray | the plane | unchanged |

The defect was the fourth row: `resolve` let only an ELIGIBLE face replace the
plane, so when the frontmost surface could not carry a sketch the loop
`break`'d and kept a plane the ray only reached AFTER passing through the
body. The fix (one block, before the face lookup) treats any body surface
nearer than the best plane hit by more than 1 mm as occluding it: the answer
becomes None, and an eligible face then replaces None exactly as it replaced
the plane before. No eligibility, no lineage, no face token, no retargeting,
no renderer change.

## Reproduction (before)

R2 device run `40689211847` (head `7d2d7b4`), `CadRetainedSketchOwnerTest.d04`:
block 2 × 2 × 1, a round Cut pocket r 0.3 at (−0.4, 0) 0.4 deep from the top
(z = 1). Camera pose `debugSetCameraPose(yaw −1.0, pitch 0.1, distance 6)`
(from −x, above the hole's opening). Target: the wall point 30° round from the
hole's +x axis, `(−0.4 + 0.3 cos 30°, 0.3 sin 30°, 0.8) = (−0.140, 0.150, 0.800)`,
projected by `debugProjectWorld` and tapped twice through the decor window.

```
TEST_FACT d04.curved_kind=2                         (the YZ plane, x = 0)
TEST_FACT d04.curved_opened=[1.0, 2.0, 0.0, 0.0, 0.0, 1.0]
          (active, plane YZ, not face-supported: a SKETCH OPENED ON YZ)
```

The YZ plane at x = 0 lies BEHIND the wall point (x = −0.14) seen from −x:
the wall was the frontmost surface and was correctly ineligible, and the tap
fell through to a plane the user could not have aimed at. The old assertion
(`kind != FACE`) passed.

Host reproduction (`forgeshape_cad_r2_selftest.cpp`, `testChooserOwnership`),
run against the UNFIXED chooser (fix reverted in the working tree, rebuilt):

```
CADR2_CHECK_FAILED C1_CH_02_a_tap_on_a_curved_wall_chooses_nothing_not_the_plane_behind_it
CADR2_CHECK_FAILED C1_CH_05_a_revolved_surface_with_a_plane_behind_chooses_nothing
CADR2_CHECK_FAILED C1_CH_06_hover_over_the_curved_wall_offers_nothing_and_selects_nothing
```

(`C1_CH_04` also failed in that first run — see "Pick crack" below; it was an
aim point on a triangle diagonal, and fails/passes identically before and
after the fix once aimed off the diagonal.)

## Host evidence (after)

`CAD_FEATURE (632 checks, 0 failed)`, all 25 suites `HOST_SELFTESTS_OK (4375
checks, 0 failed)`:

| Check | Scene, camera (orthographic `lookAlong`) | Expected and asserted |
| --- | --- | --- |
| C1_CH_01 | shop body + round Cut (feature 4, c (−0.5, −0.5), r 0.2, 0.3 deep); wall point (−0.327, −0.400, 0.85), view along (1, 0, −1) | the ray crosses the YZ square BEHIND the wall (> 5 cm), computed apart from the chooser |
| C1_CH_02 | same pixel, two taps | `None` both times, chooser still active, fingerprint unchanged |
| C1_CH_03 | YZ point (0, 1.5, 0.5), view along (−0.6, −0.8, 0); the base's y = +1 side lies behind on the same ray | `WorldPlane YZ` exactly |
| C1_CH_04 | rectangular pocket floor (0.62, −0.41, 0.6), view along (−0.2, 0, −1); XY plane behind | `Face`, producer = the body, feature 3, `CapFar`, lineage = `cadFeatureTopologySignature(state, 3)`; second tap the same `TopoRef` |
| C1_CH_05 | full Revolve ring (r 1..2 about Y); outer surface at 45°, view radially inward; YZ/XY cross behind at the axis | `None`, fingerprint unchanged |
| C1_CH_06 | hover over the curved wall | hover `None`, nothing selected |

## Device evidence

`CadRetainedSketchOwnerTest` (real decor-window touches, native truth read
back):

- **D04 (strengthened)** — the same pixel as the reproduction: first tap
  `supportChooserSelectedKind() == SUPPORT_KIND_NONE (-1)`; second tap: no
  sketch (`SKETCH_INACTIVE`), chooser still active, kind still −1,
  `projectFingerprint()` unchanged; screenshot `d04_curved_wall_refused.png`.
- **D15** — plain block; camera `(yaw 1.4, pitch 0.93, distance 8)`; YZ point
  (0, 1.5, 0.5) whose pixel lies inside the block's projected silhouette (a
  body IS under it, behind the plane): first tap kind == 2 (YZ) exactly,
  fingerprint unchanged; second tap opens a sketch with `SKETCH_VIEW_PLANE ==
  2`, `FACE_SUPPORTED == 0`; Cancel writes nothing. `d15_plane_aimed.png`.
- **D16** — block + round Cut: aim the base's top (0.5, 0.5, 1.0) from
  (0.7, 0.9, 8) → FACE; a one-finger orbit drag of 140 px → still FACE, no
  sketch; tap the curved wall → None, no sketch, fingerprint unchanged; aim +
  confirm the top → sketch on the face (origin projects onto z = 1).
- **D10 (unchanged)** — Undo between aim and confirm is refused, never
  retargeted; rotation between aim and confirm re-projects and opens on the
  face it names.
- **D12 (unchanged)** — the Edit Sketch chip is absent while the chooser is up.

Device run results: see `TESTS.md`.

## Classification

| Case | Classification |
| --- | --- |
| Plane in FRONT of a surface takes the tap | **INTENDED-VISIBLE-PLANE-PRIORITY** (asserted exactly: C1_CH_03, D15) |
| Ineligible surface in front of a plane | **BUG FIXED** (chooser arbitration only; C1_CH_02/05/06, D04, D16) |
| Eligible face | unchanged, exact identity (C1_CH_04, D03, D04 floor/wall) |
| Stale support between aim and confirm | unchanged, fails closed (R2_SUP_06, D10) |

**Interpretation recorded:** a refused tap (nothing selectable at the pixel)
leaves the chooser ACTIVE and the sketch and project untouched, and — as a tap
on empty space always has — clears the previous AIM rather than keeping it, so
an intervening miss can never turn the next tap into a confirm of something
aimed earlier. The chooser has no per-tap message surface; the standing status
line ("Tap a plane or a body face, then tap it again to sketch on it") is the
existing UI, so no new string or status code was added.

## Pick crack (pre-existing, recorded, not changed)

A ray through the exact edge two triangles share can miss both at float
precision with a far orthographic eye (found by C1_CH_04's first aim point,
the pocket floor's centre on its diagonal: `pickSceneSnapshot` hit = 0 with
the eye 250 m away). In the chooser that means such a pixel resolves as if no
body were there (a plane behind may answer). It is a property of the CPU
ray/triangle test, independent of this stage; a fix belongs to picking, not to
the chooser, and is left as a known issue.
