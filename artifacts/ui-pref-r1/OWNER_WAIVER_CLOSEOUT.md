# OWNER waiver and closeout — UI-PREF-R1

## The waiver

The OWNER explicitly **waived the final `-FullSharded` aggregate gate** for
UI-PREF-R1, on these grounds:

- the feature works in manual use;
- the focused, device, native, JVM, build and corpus gates are green;
- repeated aggregate attempts consumed excessive time;
- the last unresolved failure was diagnosed as a test-harness Activity
  recreation wait race rather than a demonstrated product defect.

The fourth ground is the OWNER's, recorded as they gave it. This closeout
refined it: the failure was not the recreation race that had been suspected
but the guest itself, and the section below shows how that was established.

**The waiver applies to this stage alone.** It does not excuse any other
stage's evidence, and it does not make the aggregate optional in future.

Alongside it the OWNER recorded **TEST-OWNER-02**, a bounded automated test
budget for a closeout: 30 minutes target, 45 minutes hard stop, no single
focused command past 20 minutes without being stopped and classified, no
repeated emulator reboots, no broad unrelated suites.

## There is no aggregate PASS

**No `FULL_SHARDED_SUITE_PASS` exists for UI-PREF-R1 and none is claimed.**
Three aggregates were attempted and all three are kept, unedited:

| transcript | what it found |
| --- | --- |
| `FULL_SHARDED_RUN1_FAIL.txt` | discovery PASS (42 classes / 540 tests); shard 1 PASS; shard 2 ASSERTION_FAILURE — `EditorWorkspaceChromeCompositionTest.uir4c11` still asserted three palettes. Test-side, fixed. |
| `FULL_SHARDED_RUN2_ABORT.txt` | shards 1–2 PASS; shard 3 `INSTRUMENTATION_ABORTED: System has crashed.` inside the unchanged `SculptUndoTest` — the guest's `system_server` died. Infrastructure. |
| `FULL_SHARDED_RUN3_FAIL.txt` | shards 1–4 PASS; shard 5 ASSERTION_FAILURE — `EditorWorkspaceLegibilityTest.uilr106`, a Back key that did not close the Add Primitive palette. |

## The focused verification run in this closeout

Ordered as the closeout brief requires, on `ForgeShape_Stage006` /
`emulator-5580` (identity confirmed each time; `emulator-5554` never
contacted), against the dirty UI-PREF-R1 candidate at baseline
`565d4002dc1306161a72bf86bb90cba7144c7316`:

| step | command | result |
| --- | --- | --- |
| JVM preference model | `gradlew :app:testDebugUnitTest --tests …AppPreferencesTest` | **PASS** (5 s) |
| single method | `-TestClass …EditorWorkspaceLegibilityTest#uilr106_backDismissesTheTopmostSurfaceBeforeItLeaves` | failed twice, then **PASS** (1 test, 1 m 54 s) after one clean guest restart |
| full class | `-TestClass …EditorWorkspaceLegibilityTest` | **PASS** — OK (20 tests), 8 m 59 s |
| settings suite | `-TestClass …SettingsPreferencesTest` | **PASS** — OK (15 tests), 4 m 23 s |

Total automated closeout time is about 32 minutes: over TEST-OWNER-02's
30-minute target, inside its 45-minute hard stop. The overrun is the two failed
attempts and the one guest restart that resolved them.

## `uilr106`: an environment failure, proven so

Run in isolation, with the assertion message widened to report the state at
the moment of failure, the workspace reported:

```
one Back closed the palette (finishing=false armed=true dismissible=true
                             settings=false home=false chooser=false bootstrap=false)
```

Read together that says the Back key **never reached the application**:

- `finishing=false` — the app did not leave, so Back was not handled as the
  platform's default either;
- `armed=true` — the app's `OnBackInvokedCallback` was still registered, so
  `dismissTopmostSurface()` was never invoked (a dismissal unregisters it);
- `settings=false`, `home=false`, `chooser=false`, `bootstrap=false` — none of
  the branches this stage added to `dismissTopmostSurface()` intercepted, so
  the new code is not diverting the press.

The matching platform line in logcat is
`WindowManager: setOnBackInvokedCallback(): No window state for
package:com.forgeshape.app`, i.e. the guest's WindowManager had no window
state to route Back to. The guest's `system_server` had crashed during
aggregate run 2 and crashed again during this closeout (a clean-install retry
ended in `INSTRUMENTATION_ABORTED: System has crashed.`).

**One clean guest restart resolved it.** The identical APK, unchanged, then
passed the method alone (OK, 1 test) and the whole class (OK, 20 tests). That
is what makes this an environment failure rather than a product one: no
product byte differs between the failing runs and the passing ones. The path
the case exercises — `hasDismissibleSurface()`, `dismissTopmostSurface()` and
the palette's own close — is unchanged by this stage except for the Settings
branch, which the diagnostic shows was not taken.

The diagnostic message stays in the test. A Back-dismissal failure that
reports `finishing`, `armed` and which shell phase is up separates "the app
ignored Back" from "Back never arrived" in one line, and that distinction cost
three runs to make here.

The wait added to that suite's `switchTo` in the previous pass also stays:
with five palettes its contrast loop performs five Activity recreations, and
waiting for a new Activity instance rather than for a settle is correct
whatever caused this particular failure.

## Final HEAD

The candidate was committed in two commits on the local `master` branch, on
top of the baseline `565d4002dc1306161a72bf86bb90cba7144c7316`:

1. `23bc6521070ed2578a37788500cadba018242cb5` —
   `feat(ui-pref): settings handedness themes and gizmo preferences`: the
   product, runtime, resource and test implementation.
2. this documentation-and-evidence commit,
   `docs(test): record owner aggregate waiver and ui-pref closeout`, which is
   the final HEAD. Its hash is reported to the coordinator with this closeout,
   because a commit cannot contain its own hash.

The repository has no remote and none was added.
