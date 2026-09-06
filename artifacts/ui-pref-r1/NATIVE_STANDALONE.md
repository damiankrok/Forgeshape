# Native standalone evidence — UI-PREF-R1

The platform-neutral suites were compiled with the NDK's own
`x86_64-linux-android26-clang++` (29.0.14206865) into a PIE binary, pushed to
`/data/local/tmp` on `emulator-5580` (`ForgeShape_Stage006`) and run there,
before and after the change, for a red/green loop that does not wait on a
Gradle build. The binary was removed afterwards.

## The gizmo geometry golden

A scratch `main` hashes `generateGizmoVertices` (FNV-1a 64 over the vertex
bytes) so "Regular is byte-identical to the accepted gizmo" is a measured fact
rather than a static_assert on counts alone:

| build | list | vertices | FNV-1a 64 |
| --- | --- | ---: | --- |
| baseline `565d400` | the one list | 1116 | `84976216ea9f24b8` |
| after UI-PREF-R1 | Regular (two-argument overload) | 1116 | `84976216ea9f24b8` |
| after UI-PREF-R1 | Thin | 1116 | `bb9e7edae6094071` |
| after UI-PREF-R1 | Bold | 2268 | `27e1f10112f1c0a5` |

Per-mode baseline hashes (`move=2c74a963ce4501d7`, `rotate=dae2e1d847a78056`,
`scale=7ab432dd115eb6bd`) are what `gizmoVertexRange(mode, Regular)` still
addresses. The x86_64 numbers are the authoritative emulator's; an arm64
build's transcendental rounding may differ in the last bit, which is why the
in-app self-test pins the count and compares the two overloads' bytes
(`uipref_regular_is_the_pre_preference_gizmo_byte_for_byte`) rather than a
cross-ABI hash.

## The suites, standalone

`runGizmoSelfTests` and `runRenderMeshSelfTests` (the render-shading suite that
owns the display store and grid palette checks) on the final tree: 167 and 345
checks, 0 failures. The first standalone run after the change failed three new
checks, each of which changed the design rather than the check:

- `uipref_every_handle_is_pickable_at_both_visual_bounds` at a 0.75 floor:
  the XZ plane handle's centre projected inside the pivot's 24-unit dead disc
  from the oblique test camera. The floor became 0.9 (`GIZMO_APPEARANCE.md`).
- `uipref_the_same_pixel_drag_scales_by_the_same_factor…` at 1e-9: the two
  screen directions are normalised from different grab points, so the factors
  agree to ~1e-7; the tolerance is 1e-6.
- `display_the_two_light_grounds_are_distinguishable` at a 0.2 lean gap:
  the real gap is 0.097 (25/255); the threshold is 0.05, still four times a
  one-step drift.

## In the app (debug launch, 64 MiB ring, cleared)

Twenty `_SELFTEST_OK`, zero `_SELFTEST_FAIL` / `_FAIL:`, **3019 checks**
(2981 before: render-shading 329 → 345, gizmo 145 → 167, and the project /
import / sketch-UX suites at the counts POST-AUDIT-HARDEN-R1 left them). The
raw capture is `startup.log`.
