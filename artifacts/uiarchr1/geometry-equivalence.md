# Geometry and visibility equivalence

The capture harness resolves controls by stable semantic resource id, records
presence, bounds, enabled/selected/checked state, and saves the framebuffer.
Each CSV contains 175 rows. Excluding the newly introduced host id leaves 164
comparable rows; the corrected field-by-field comparison reports zero deltas.

| State | Before right-cluster union | After right-cluster union |
| --- | --- | --- |
| Construction resting | `880,296–1059,817` | `880,296–1059,817` |
| Move | `880,296–1059,1557` | `880,296–1059,1557` |
| Rotate | `880,296–1059,1557` | `880,296–1059,1557` |
| Scale | `880,296–1059,1261` | `880,296–1059,1261` |
| Exact Transform | `880,296–1059,1261` | `880,296–1059,1261` |
| Exact + IME | `637,296–1059,836` | `637,296–1059,836` |
| Display | suppressed | suppressed |
| Short landscape | `2200,231–2379,668` | `2200,231–2379,668` |
| Expanded/tablet | `1409,296–1588,817` | `1409,296–1588,817` |
| Sculpt resting | `880,296–1059,1163` | `880,296–1059,1163` |
| Sculpt Details | `880,296–1059,1163` | `880,296–1059,1163` |

The first baseline Exact+IME capture was rejected because Gboard exposed a
floating accessory instead of the full numeric IME. The baseline APK was rebuilt
from exact SHA `717cf70`, the capture was repeated after stabilizing the IME, and
only the corrected matched set is retained here. Representative Exact+IME and
expanded/tablet pairs were also inspected visually.
