# Performance (`CAD-A3` O)

All measured on `emulator-5580`, printed at startup as
`FORGESHAPE_CAD_A3_PERFORMANCE`, and by the standalone runner.

## Dependency resolve / regenerate

| Chain length (dependents) | Resolve time |
| --- | --- |
| 1 | ~13-42 us |
| 8 | ~24-117 us |
| 32 | ~68-490 us |

Bounded and roughly linear in chain length. The 32-deep case -- far beyond any
plausible hand-built model -- resolves in well under a millisecond warm; the
higher cold figure is the first in-app call. Resolution is depth-bounded by the
body count, so a corrupt or cyclic graph cannot recurse without end.

## Semantic face pick

A face pick is one ordinary scene ray-pick (bounded, low-poly CAD meshes) plus a
range lookup over `2 + n` face ranges. No project encode/decode and no
per-triangle-times-all-faces-times-all-objects path. World-plane picks are three
ray-plane tests plus a bounds check. Both run per tap, not per frame.

## Bounds honoured

- Existing project object/entity caps unchanged.
- Dependency traversal bounded by body count; no unbounded stack on a cycle.
- Picking does no full-project serialisation.
- The sketch overlay and the support-chooser overlay draw a bounded line count
  regardless of zoom.
