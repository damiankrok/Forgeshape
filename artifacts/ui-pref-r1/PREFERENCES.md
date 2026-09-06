# The preference model and store — UI-PREF-R1 Part B

## One versioned value (`AppPreferences`)

| field | type | default | storage rule on read |
| --- | --- | --- | --- |
| `schemaVersion` | int | 1 | accepted at any value; a newer schema is read with this build's rules |
| `palette` | `AppTheme` (five members) | `WARM_GRAPHITE` | stored by NAME; an unknown or missing name is the default |
| `handedness` | `Handedness` (`RIGHT`, `LEFT`) | `RIGHT` | stored by NAME; an unknown or missing name is the default |
| `gizmoVisualScale` | float in `[0.9, 1.5]` | `1.0` | NaN / ±Inf → default; finite out-of-range → CLAMPED to the nearer bound; missing → default |
| `gizmoStrokeWeight` | `GizmoStrokeWeight` (`THIN`, `REGULAR`, `BOLD`) | `REGULAR` | stored by NAME; an unknown or missing name is the default |

There is deliberately no `gizmoHandleStyle` field: see `INDEX.md` § Handle style.

The value is immutable (`withPalette`, `withHandedness`, `withGizmoVisualScale`,
`withGizmoStrokeWeight` return new values), holds no Android type, and every
rule above is proven on the JVM by `AppPreferencesTest` (10 cases) and on the
device by `SettingsPreferencesTest.uiprefr1_07_08`, which plants
`"NEON_PINK" / "AMBIDEXTROUS" / NaN / "HAIRLINE" / schema 42`, then `9.0`, then
`-3.0` under the real keys and reads the documented fallbacks back.

## Storage (`AppPreferencesStore`)

- Mechanism: the platform's `SharedPreferences`, file `forgeshape_preferences`
  under the app's private preferences directory. No new runtime dependency;
  the product APK still has none.
- Keys: `schema_version`, `palette`, `handedness`, `gizmo_visual_scale`,
  `gizmo_stroke_weight`.
- Writes: `commit()` — synchronous, and atomic at the platform's level (a
  `.bak`, then a rename), which is enough for a record of four fields and is
  what lets a case assert "on disk" the moment a row was pressed
  (`uiprefr1_06`: `existsOnDisk`, then `dropCacheForVerification` and a
  fresh read equal to what was written).
- Reads: once per process into one cached copy; every typed read is guarded
  against a `ClassCastException` (a wrong-type value under a known key is
  that field's default, never a crash on launch).
- Load order: `ForgeShapeActivity.onCreate` reads the store BEFORE
  `super.onCreate` and `setTheme`, so the first frame is already the stored
  palette — no default→stored flicker (`applyTheme`).
- Survives: Activity recreation (`uiprefr1_05`, through
  `ActivityScenario.recreate()`), HOME/resume (the cache is process-scoped),
  process death (a fresh read from the file; the device relaunch is in
  `DEVICE_E2E.md`).
- No project file, `Uri`, `ContentResolver` or path is involved; the store
  does not know what a project is.

## Not project truth (Part B3 / Part H)

`SettingsPreferencesTest.uiprefr1_09_10_11_37` builds a turned cone with a
history, records `encodeProject()`, `projectFingerprint()`, both history
depths, the dirty flag, the active `ObjectId`, the mesh revision and the whole
native snapshot; changes handedness (Left, Right), visual size (largest,
smallest), stroke weight (Bold, Thin) and the palette twice (Warm Light, Cool
Light — each a full Activity recreation); and asserts every one of them
identical afterwards — the `.forge` bytes byte for byte.

No preference is written by `saveProject`, `encodeProject`, the autosave
checkpoint or the recovery document: none of `forgeshape_project_*.cpp`
changed in this stage (`git diff --stat 565d400` on those files is empty), and
`DATA_PACKAGE_SPEC.md` is untouched — the twenty-eight-fixture corpus is
byte-identical.
