# Source complexity delta

Measured against baseline `717cf70` and the final UI-ARCH-R1 working source:

| Measure | Before | After | Delta |
| --- | ---: | ---: | ---: |
| `EditorWorkspaceView.java` LOC | 2952 | 2447 | -505 |
| Constructor/composition block lines | 469 | 275 | -194 |
| Root private field declarations | 54 | 40 | -14 |
| Right-cluster root fields | 15 | 1 (`trailingHost`) | -14 |
| Right-cluster composition/layout methods in root | 5 | 1 (`renderTrailingHost`) | -4 |
| Right-cluster root verification accessors | 14 | 0 | -14 |
| New host LOC | 0 | 339 | +339 |

The root reduction is structural, not a line move alone: construction, child
order, orientation, visibility/suppression, entry rebuilding, leaf rendering and
parent placement moved behind the host boundary. Native calls and authoritative
state decisions stayed in `EditorWorkspaceView`.
