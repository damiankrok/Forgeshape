# UI-PREF-R1 — Settings hub, persistent preferences, handedness, five palettes, gizmo appearance

Baseline `565d4002dc1306161a72bf86bb90cba7144c7316` (clean). Result:
**`PASS-UI-PREF-R1-OWNER-WAIVER-CLOSEOUT`** — the implementation is complete
and every focused, device, native, JVM, build and corpus gate is green, and
the final `-FullSharded` aggregate was **NOT achieved and was explicitly
waived by the OWNER for this stage** (`OWNER_WAIVER_CLOSEOUT.md`). No
`FULL_SHARDED_SUITE_PASS` exists for UI-PREF-R1 and none is claimed. See also
*Deviations*. Device: the isolated `ForgeShape_Stage006` / `emulator-5580`
AVD (identity confirmed with `emu avd name` before every run; `emulator-5554`
never contacted).

| document | what it holds |
| --- | --- |
| `PREFERENCES.md` | the `AppPreferences` model, the store, the per-field fallback rules, the project-truth isolation proof (Parts B, H) |
| `HANDEDNESS.md` | what mirrors and what does not, the runtime switch, the measured right/left frames (Part C) |
| `GIZMO_APPEARANCE.md` | the visual-size bounds and why 0.9, the three stroke recipes, the byte-identical Regular, the handle-style deferral (Parts E, F, G) |
| `CONTRAST.md` | the WCAG audit of all five palettes on the repository's own targets, generated from `colors.xml` (Part D4, UIPREFR1-27) |
| `VISUAL_EVIDENCE.md`, `OWNER_CONTACT_SHEET_UI_PREF_R1.png`, `frames/` | the twenty composed-display captures with measured facts (Part N) |
| `DEVICE_E2E.md` | `E2E-UIPREFR1-01..18` mapped to the case or capture that proves each (Part M) |
| `TEST_RESULTS.md` | every suite run on the final tree: JVM, focused instrumented, the aggregate |
| `FULL_SHARDED_RUN1_FAIL.txt`, `FULL_SHARDED_RUN2_ABORT.txt`, `FULL_SHARDED_RUN3_FAIL.txt` | the three attempted aggregates, kept as the record of what each found. There is no passing aggregate for this stage |
| `OWNER_WAIVER_CLOSEOUT.md` | the OWNER's waiver of the aggregate gate, the focused verification run in its place, and the final HEAD |
| `PERFORMANCE.md` | what each preference change does and does not do, from the code paths and the logged tokens (Part O) |
| `DELETE_HOLD.md` | the Delete → Undo → Redo hold untouched, by `git diff --stat` (Part I) |
| `NATIVE_STANDALONE.md`, `startup.log` | the standalone-runner golden hashes and the debug launch capture (20/20, 3019 checks) |
| `RELAUNCH.md` | the process-death relaunch transcript (E2E-UIPREFR1-03) |
| `focused/` | the focused runner logs |

## What was built

- **Settings hub** (Part A): `SettingsPageView`, a start page on `StartPageView`'s
  grammar, reached from Home's quiet `Settings` row (`home_settings`) and the
  Project surface's `Settings…` (`project_settings`, under a new *Application*
  group). One implementation, one store. Groups: Appearance (five rows),
  Workspace (Handedness: two rows), Gizmo (Visual size: four rows; Thickness:
  three rows). Back at the foot; System Back is one step; the chrome is
  withdrawn under the opaque page; the viewport is not reachable through it.
- **Preference model and store** (Part B): `AppPreferences` (immutable,
  versioned, no Android type) + `AppPreferencesStore` (`SharedPreferences`,
  names not ordinals, synchronous atomic writes, one cached copy per process,
  guarded reads). Read before `setTheme`, so no first-frame flicker.
  `EditorUiState` no longer holds the palette; it holds `settingsOpen`, carried
  across the recreation a palette change performs.
- **Handedness** (Part C): `Handedness` enum; `EditorWorkspaceView.applyHandedness`
  mirrors the middle row's order and edge margins;
  `WorkspaceTrailingHostView.setMirrored`; `leadingLimitFor` beside
  `trailingLimitFor`; `placeInspector` seats a side-placed Exact inboard of the
  host on either edge. Right = UI-LAYOUT-R2 exactly.
- **Five palettes** (Part D): `AppTheme` gains `WARM_LIGHT`, `COOL_LIGHT`
  (`isLight()`, `labelRes()`, `fromStoredName`); `themes.xml` gains
  `Theme.ForgeShape.LightBase` (light system-bar icons) and the two light
  themes; `colors.xml` gains the `p4_*` / `p5_*` palettes; the three dark
  palettes are untouched value for value. Native: `ViewportBackground` gains
  `WarmLight`, `CoolLight` (indices 3, 4; the dark three unchanged),
  `viewportBackgroundIsLight`, two grid palettes that sink into the paper, and
  the gizmo's saturated palette on light grounds. The Appearance group left the
  Display popover for Settings (UI-OWNER-37).
- **Gizmo visual size** (Part E): `GizmoSnapshot.visualScale`,
  `gizmoPlacementScale`, `GizmoSession::setVisualScale` (bounds `[0.9, 1.5]`,
  refused outside), JNI `setGizmoVisualScale` / `gizmoVisualScale`; the hit
  corridors and every drag amount unchanged (scale ruler on a canonical
  snapshot).
- **Gizmo thickness** (Part F): `GizmoStrokeWeight` in the display store,
  `GizmoStrokeStyle` recipes, `generateGizmoVertices(out, cap, weight)`,
  `gizmoVertexRange(mode, weight, …)`, `kGizmoVertexCountMax`; the renderer's
  one max-sized buffer and `syncGizmoGeometry`; JNI `setGizmoStrokeWeight` /
  `gizmoStrokeWeight`. Regular is byte-identical to the accepted gizmo.
- **Tests**: JVM `AppPreferencesTest` (new, 10), `AppThemeTest` (rewritten, 10),
  `DisplaySettingsContractTest` (+2); native +16 render-shading and +22 gizmo
  checks; instrumented `SettingsPreferencesTest` (new, 15),
  `UiPrefVisualEvidenceTest` (new, 1), `EditorWorkspaceThemeTest` (palettes
  chosen on the Settings page, five palettes, light/dark split, hierarchy as
  contrast, waits for the recreated Activity), `EditorWorkspaceCorrectionTest`
  UIR4B-18, `EditorWorkspaceLegibilityTest` UILR1-11, `HomeFlowTest` E2E-APPH1-01,
  `EditorWorkspaceChromeCompositionTest` UIR4C-11 (five palettes).
- **Scripts / docs**: `scripts/collect-ui-pref-evidence.ps1`; `CLAUDE.md`
  (the preferences rule, the vocabulary), `PRODUCT.md` (five appearances,
  a Settings chapter, what survives, not implemented), `ARCHITECTURE.md`
  (ownership rows, the appearance chapter, an *Application preferences*
  section, the gizmo chapter), `README.md`, `PROJECT_STATUS.md`.

## Deviations

- **`GIZMO_STYLE_DEFERRED_BY_RENDERER_CONTRACT`** (Part G): no Handle Style
  row is drawn and the model carries no field for it. See `GIZMO_APPEARANCE.md`.
- The visual-size floor is **0.9**, not the 0.75 first tried: below 0.9 an
  obliquely viewed plane handle's centre falls inside the pivot's fixed 24-unit
  dead disc and the handle is unreachable. The corridor was not changed,
  because that would change the accepted hit semantics.
- Opening the Settings page CLOSES an open Exact/Details (it is an opaque page
  over the workspace), so a handedness switch made through the page never
  operates on a live panel; the live re-seat path is exercised through the
  store (`uiprefr1_15_17`).
- Phone viewport only for the captures; the expanded layout is proven by the
  unchanged synthetic `WorkspaceLayoutMode` arithmetic and
  `EditorWorkspaceRightHostPlacementTest` UILR2C-05/06, as before. No physical
  tablet was tested.
- The process-death half of E2E-UIPREFR1-03 uses a preference file placed
  under the app's own directory (`run-as`) before a cold start, because an
  instrumentation case cannot outlive its process; the in-process half
  (`uiprefr1_06`) proves the product writes exactly that file.

## Runs, in order

See `TEST_RESULTS.md`. The first focused `SettingsPreferencesTest` run crashed
every case at Activity creation: `Window.getInsetsController()` was called
from `onCreate`, before the decor is attached, and threw — the same defect had
made the manual debug launch print all twenty self-test tokens and then no
`FORGESHAPE_NATIVE_VIEWPORT_OK`. Moved to `onAttachedToWindow` / `onResume`.
The second run failed two cases (the Exact-open expectation above; the
system-bar readback on the OLD Activity, because the store answers a new
palette before the recreation begins) — both test-side, fixed by waiting for a
new Activity instance. The fourth run found a product defect the earlier races
had hidden — the Settings page the recreation brings back had no row marked
chosen — fixed by repainting the page from the store whenever it comes on
screen. The final green runs are in `TEST_RESULTS.md`.
