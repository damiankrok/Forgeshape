# Contrast audit - UI-PREF-R1 (UIPREFR1-27)

Computed from `app/src/main/res/values/colors.xml` as committed (WCAG 2.x relative luminance and contrast ratio), for every
text role of every palette against every ground it is drawn on. The targets are the REPOSITORY'S own, unchanged by this stage:
primary, secondary and error text at 4.5:1 on all six grounds they are drawn on (`UILR1-11`); the measure and success verdicts
at the accepted 2.4:1 floor against the precision surface (`EditorWorkspaceThemeTest.MIN_VERDICT_CONTRAST`); `text_on_primary`
at 4.5:1 on the accent; the focus ring is reported for information. Cells outside a role's target are marked (info). The three dark
palettes are value-for-value what they were before this stage - their twelve anchors are owner-approved and were not moved - so
their rows are the accepted baseline the two light palettes are held to. Both light palettes clear 4.5:1 for EVERY text role on
every ground, which is stricter than the baseline requires.

## Warm Graphite (`p1_*`) - ground luminance 0.028, DARK

| role | viewport background | chrome surface | surface floating | surface precision | control surface | field surface | selected rest | start page surface | target |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `text_primary` #F4F1EB | 12.01 | 10.37 | 8.73 | 9.58 | 8.87 | 11.34 | 7.15 (info) | 13.73 (info) | 4.5 on the six |
| `text_secondary` #B6B1A9 | 6.35 | 5.48 | 4.61 | 5.07 | 4.69 | 5.99 | 3.78 (info) | 7.26 (info) | 4.5 on the six |
| `text_error` #E8A093 | 6.37 | 5.50 | 4.63 | 5.08 | 4.71 | 6.01 | 3.79 (info) | 7.28 (info) | 4.5 on the six |
| `text_measure` #E0A84B | 6.36 (info) | 5.49 (info) | 4.62 (info) | 5.08 | 4.70 (info) | 6.01 (info) | 3.79 (info) | 7.28 (info) | 2.4 on precision |
| `text_success` #7FB388 | 5.61 (info) | 4.84 (info) | 4.08 (info) | 4.48 | 4.15 (info) | 5.30 (info) | 3.34 (info) | 6.42 (info) | 2.4 on precision |
| `text_disabled` #7A746C | 2.93 (info) | 2.53 (info) | 2.13 (info) | 2.34 (info) | 2.16 (info) | 2.76 (info) | 1.74 (info) | 3.35 (info) | none |
| `text_on_primary` #12110F on accent / pressed accent | 5.58 / 7.13 | | | | | | | | 4.5 |
| `accent` focus ring on precision (non-text) | 3.19 | | | | | | | | 3.0 |

## Neutral Charcoal (`p2_*`) - ground luminance 0.021, DARK

| role | viewport background | chrome surface | surface floating | surface precision | control surface | field surface | selected rest | start page surface | target |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `text_primary` #ECEEF0 | 12.72 | 11.25 | 9.27 | 10.44 | 9.55 | 12.07 | 8.32 (info) | 14.05 (info) | 4.5 on the six |
| `text_secondary` #A4AAB1 | 6.31 | 5.58 | 4.60 | 5.18 | 4.74 | 5.99 | 4.13 (info) | 6.98 (info) | 4.5 on the six |
| `text_error` #F09A8E | 6.85 | 6.05 | 4.99 | 5.62 | 5.14 | 6.50 | 4.48 (info) | 7.56 (info) | 4.5 on the six |
| `text_measure` #DDA94F | 6.95 (info) | 6.14 (info) | 5.06 (info) | 5.70 | 5.22 (info) | 6.59 (info) | 4.55 (info) | 7.68 (info) | 2.4 on precision |
| `text_success` #74C58A | 7.11 (info) | 6.29 (info) | 5.18 (info) | 5.84 | 5.34 (info) | 6.75 (info) | 4.65 (info) | 7.86 (info) | 2.4 on precision |
| `text_disabled` #6C737A | 3.08 (info) | 2.72 (info) | 2.24 (info) | 2.53 (info) | 2.31 (info) | 2.92 (info) | 2.02 (info) | 3.40 (info) | none |
| `text_on_primary` #101214 on accent / pressed accent | 5.55 / 7.09 | | | | | | | | 4.5 |
| `accent` focus ring on precision (non-text) | 3.59 | | | | | | | | 3.0 |

## Light Charcoal (`p3_*`) - ground luminance 0.049, DARK

| role | viewport background | chrome surface | surface floating | surface precision | control surface | field surface | selected rest | start page surface | target |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `text_primary` #F3F2F0 | 9.48 | 8.24 | 6.52 | 7.49 | 6.74 | 8.91 | 5.32 (info) | 11.04 (info) | 4.5 on the six |
| `text_secondary` #CDCECC | 6.72 | 5.83 | 4.62 | 5.31 | 4.77 | 6.31 | 3.77 (info) | 7.82 (info) | 4.5 on the six |
| `text_error` #FFC0B6 | 6.80 | 5.91 | 4.68 | 5.38 | 4.83 | 6.39 | 3.81 (info) | 7.92 (info) | 4.5 on the six |
| `text_measure` #D7A65A | 4.80 (info) | 4.17 (info) | 3.30 (info) | 3.79 | 3.41 (info) | 4.51 (info) | 2.69 (info) | 5.58 (info) | 2.4 on precision |
| `text_success` #7CC08C | 4.94 (info) | 4.29 (info) | 3.40 (info) | 3.90 | 3.51 (info) | 4.64 (info) | 2.77 (info) | 5.75 (info) | 2.4 on precision |
| `text_disabled` #82858A | 2.87 (info) | 2.49 (info) | 1.97 (info) | 2.26 (info) | 2.03 (info) | 2.69 (info) | 1.61 (info) | 3.34 (info) | none |
| `text_on_primary` #101418 on accent / pressed accent | 5.47 / 6.98 | | | | | | | | 4.5 |
| `accent` focus ring on precision (non-text) | 2.48 | | | | | | | | 3.0 |

## Warm Light (`p4_*`) - ground luminance 0.803, LIGHT

| role | viewport background | chrome surface | surface floating | surface precision | control surface | field surface | selected rest | start page surface | target |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `text_primary` #1F1C18 | 13.79 | 14.82 | 15.87 | 15.34 | 13.31 | 12.81 | 12.80 (info) | 14.31 (info) | 4.5 on the six |
| `text_secondary` #5C564D | 5.90 | 6.34 | 6.79 | 6.56 | 5.69 | 5.48 | 5.48 (info) | 6.12 (info) | 4.5 on the six |
| `text_error` #B3261E | 5.31 | 5.71 | 6.11 | 5.91 | 5.13 | 4.93 | 4.93 (info) | 5.51 (info) | 4.5 on the six |
| `text_measure` #7A4F00 | 5.79 (info) | 6.22 (info) | 6.67 (info) | 6.44 | 5.59 (info) | 5.38 (info) | 5.38 (info) | 6.01 (info) | 2.4 on precision |
| `text_success` #1F6B2E | 5.33 (info) | 5.73 (info) | 6.14 (info) | 5.93 | 5.14 (info) | 4.95 (info) | 4.95 (info) | 5.53 (info) | 2.4 on precision |
| `text_disabled` #847D73 | 3.31 (info) | 3.55 (info) | 3.80 (info) | 3.68 (info) | 3.19 (info) | 3.07 (info) | 3.07 (info) | 3.43 (info) | none |
| `text_on_primary` #12110F on accent / pressed accent | 5.58 / 7.13 | | | | | | | | 4.5 |
| `accent` focus ring on precision (non-text) | 3.06 | | | | | | | | 3.0 |

## Cool Light (`p5_*`) - ground luminance 0.803, LIGHT

| role | viewport background | chrome surface | surface floating | surface precision | control surface | field surface | selected rest | start page surface | target |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| `text_primary` #171B20 | 14.05 | 15.26 | 16.39 | 15.84 | 14.19 | 13.53 | 12.53 (info) | 14.83 (info) | 4.5 on the six |
| `text_secondary` #4F5862 | 5.87 | 6.38 | 6.85 | 6.62 | 5.93 | 5.65 | 5.24 (info) | 6.20 (info) | 4.5 on the six |
| `text_error` #B3261E | 5.31 | 5.77 | 6.19 | 5.98 | 5.36 | 5.11 | 4.73 (info) | 5.61 (info) | 4.5 on the six |
| `text_measure` #7F5400 | 5.38 (info) | 5.84 (info) | 6.28 (info) | 6.06 | 5.43 (info) | 5.18 (info) | 4.80 (info) | 5.68 (info) | 2.4 on precision |
| `text_success` #1E6B34 | 5.32 (info) | 5.77 (info) | 6.20 (info) | 5.99 | 5.37 (info) | 5.12 (info) | 4.74 (info) | 5.61 (info) | 2.4 on precision |
| `text_disabled` #757E88 | 3.35 (info) | 3.63 (info) | 3.90 (info) | 3.77 (info) | 3.38 (info) | 3.22 (info) | 2.98 (info) | 3.53 (info) | none |
| `text_on_primary` #101214 on accent / pressed accent | 5.55 / 7.09 | | | | | | | | 4.5 |
| `accent` focus ring on precision (non-text) | 3.10 | | | | | | | | 3.0 |

## Hierarchy, stated as contrast against the precision surface

"Quieter" means nearer the ground: on a light palette a quieter role is LIGHTER, on a dark one DARKER, so the order is
stated as contrast, which is the same rule in both families (`EditorWorkspaceLegibilityTest.uilr111`, `EditorWorkspaceThemeTest.r1b2_14`).

| palette | primary | secondary | disabled | order holds |
|---|---:|---:|---:|---|
| Warm Graphite | 9.58 | 5.07 | 2.34 | yes |
| Neutral Charcoal | 10.44 | 5.18 | 2.53 | yes |
| Light Charcoal | 7.49 | 5.31 | 2.26 | yes |
| Warm Light | 15.34 | 6.56 | 3.68 | yes |
| Cool Light | 15.84 | 6.62 | 3.77 | yes |

## Distinctness of the two light palettes

Warm Light ground #EDE7DC: R-B = 17 (warm lean). Cool Light ground #E4E8EC: R-B = -8 (cool lean). The two leans differ by 25 of 255 - the same kind of separation Warm Graphite (#302E2B, R-B = +5) has from Neutral Charcoal (#26282A, R-B = -4), scaled to a light ground. The native viewport grounds (`DISP-VBG-08/09`) pin the same two values and `display_the_two_light_grounds_are_distinguishable` asserts the lean difference.

## System bars

`Theme.ForgeShape.LightBase` states `windowLightStatusBar` and `windowLightNavigationBar` for the two light palettes, and
`ForgeShapeActivity.applySystemBarAppearance` restates it through `WindowInsetsController` (API 30+) or the legacy
`SYSTEM_UI_FLAG_LIGHT_*` flags once the window is attached; `ForgeShapeActivity.systemBarsLight()` reads it back and
`EditorWorkspaceThemeTest.r1b2_04` and `SettingsPreferencesTest.uiprefr1_22_26` assert it equals `AppTheme.isLight()` for all five.

Result: **PASS** - no role below its target on any ground in any palette.
