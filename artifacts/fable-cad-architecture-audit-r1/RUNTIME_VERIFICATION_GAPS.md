# Runtime verification gaps (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

No test was run for this audit (FAUD-17). Every claim marked
`RUNTIME_VERIFICATION_NEEDED` below is source-confirmed as CODE but not
observed on a device by this audit; each names the SMALLEST future test that
would answer it. `INFERRED` and `UNVERIFIED` items from the audits are
collected here so nothing unverified is silently promoted.

| # | Claim | Status | Smallest test that answers it |
| --- | --- | --- | --- |
| G1 | The extrude arrow head is drawn at the world-origin `metersPerPixel` while the hit test and glyph scale use the anchor's depth, so on a perspective FACE sketch the head and the corridor disagree (`jni:1859-1862` vs `tool.cpp:302`, `jni:4854`) | `RUNTIME_VERIFICATION_NEEDED` (magnitude) | one `Ui3dStateCorrectionTest` case: sketch on the far cap of a body placed 3 m from the origin, perspective on, read `CAD_EXTRUDE_SCALE` and the drawn head length from `sketchOverlay` vertices, assert they agree within 1 % — or record the ratio |
| G2 | Tapping disk A while the ring is selected silently REPLACES the ring (not refuses) | `TEST_CONFIRMED` natively (`CADVS_SES_05`) and on device (`owner_rectangle_circle_region`) — no gap; listed because it is the O2 symptom | — |
| G3 | Region toggling works inside an EDIT session of a later feature (reopen → Ready → tap a region → Finish) | `INFERRED` from code, no test | one `CadVerticalSliceTest` case: reopen feature 2, toggle a region, Finish, assert the body volume changed and one Undo restores it |
| G4 | Reopening a CUT and reopening feature 3 of a three-feature chain behave as the Add case | `INFERRED` (no operation- or index-specific branch) | extend `feature_edit_roundtrip` with a Cut row and a third feature |
| G5 | The auto-select edge: one Ok region beside one `OverlappingHoles` region selects NOTHING (code) rather than the Ok one (docs) | `SOURCE_CONFIRMED` code, `DOC_ONLY` drift, untested | one native check in `CADVS_REG_*`: a rectangle with two crossing inner circles → `finish()` → `extrude().profileEntityId == kNoSketchEntity` |
| G6 | A + B selected with R present and unselected extrudes two disks | `INFERRED` | one native check: `validateRegionSelection(x, {ref(2), ref(3)}) == Ok` and `components == 2` |
| G7 | A chain anchor of permuted Line/Arc/Spline storage is still the smallest member id (`sketch.cpp:987` takes first-in-storage) | `UNVERIFIED` | one native check: build a chain with ids {5, 3, 4} stored in that order, assert `anchorEntityId == 3` |
| G8 | Manifold's behaviour on a two-shell solid with coincident opposite-facing walls (what allowing rect + A WITHOUT a 2D merge would feed it) | `UNVERIFIED`; moot if B1 merges in 2D first | not needed if the parity merge lands; otherwise one kernel capability check |
| G9 | The first candidate evaluation after a drag sample runs on the UI thread under the state lock (`jni:4840`); cost of a 16-feature chain on a phone | `SOURCE_CONFIRMED` code; cost `UNVERIFIED` | `FORGESHAPE_CAD_FEATURE_PERFORMANCE` already prints chain timings on the emulator; the physical-device number is a Tier 4 OWNER capture of the same token on the S25 Ultra |
| G10 | `CAD_EXTRUDE_ON_SCREEN` means "in front of the eye", and the Java clamp keeps an off-viewport cluster at the edge | `SOURCE_CONFIRMED`; `TEST_CONFIRMED` "clamped but inside" | — |
| G11 | Council D2: the navigator's Flip and ±90° never move the camera (`jni:311`) | still in source; runtime `UNVERIFIED` | one `SketchUxTest` case asserting the camera pose (`debugCameraPose`) after Flip |
| G12 | Council D3: a palette change makes a saved project read unsaved | still in source; runtime `UNVERIFIED` | one `SettingsPreferencesTest` case: Save, change palette, leave → no unsaved question |
| G13 | Council D5: the overlay stays stale after a pinch zoom | still in source; runtime `UNVERIFIED` | one `SketchUxTest` case: pinch, read `sketchGridStep`, assert the overlay revision moved |
| G14 | The 18 unguarded test-only exports are present in a release `.so` | `INFERRED` (no release build made; `app/build.gradle` declares only `debug`) | extend `scripts/ci-release-selftest-guard.sh`'s symbol list with the 18 names |
| G15 | Render-loop battery/thermal cost of never resting | `UNVERIFIED` on a phone | a Tier 4 OWNER measurement (battery stats over 10 min still scene) before T8 |
| G16 | The HUD numbers in `OWNER_FINDINGS.md` (2.70 %, 0.18 dp) | `DEVICE_EVIDENCE` on the CI emulator, not re-measured here | — |

None of these gaps changes a conclusion in `EXECUTIVE_SUMMARY.md`: C1–C12 rest
on `SOURCE_CONFIRMED` structure (struct definitions, predicates, call graphs),
not on runtime magnitudes. The result token is therefore `PASS`, with G1, G9
and G15 called out as the three magnitudes the OWNER's physical device must
supply before B2 and T8 are sized.
