# Process-death relaunch — E2E-UIPREFR1-03 / UIPREFR1-06

An instrumentation case cannot outlive its own process, so the process-death
half of "preferences survive a relaunch" is a device transcript
(`relaunch_transcript.txt`, produced by scoped `adb -s emulator-5580` calls
only; the AVD's identity is the first line).

What the product WRITES is proven in-process by
`SettingsPreferencesTest.uiprefr1_06` (`existsOnDisk`, the cache dropped, a
fresh read equal to what was written) and `uiprefr1_07_08` (the same keys read
back through the same reader). This transcript proves what the product READS
on a cold start: with the app force-stopped (no PID), a preference file with
`COOL_LIGHT / LEFT / 1.5 / BOLD` in the app's own `shared_prefs/` directory,
and a cold `am start`, the very first tokens native code logs are

```
FORGESHAPE_VIEWPORT_BACKGROUND:CoolLight requested=4 known=1 changed=1
FORGESHAPE_GIZMO_VISUAL_SCALE:1.500 requested=1.500 accepted=1
FORGESHAPE_GIZMO_STROKE_WEIGHT:Bold requested=2 known=1 changed=1
FORGESHAPE_GIZMO_UPLOAD_OK weight=Regular vertices=1116 ...   (device creation)
FORGESHAPE_GIZMO_UPLOAD_OK weight=Bold vertices=2268 ...      (before the first frame)
FORGESHAPE_NATIVE_VIEWPORT_OK
```

followed by 20/20 `_SELFTEST_OK` and no failure. The viewport background was
set BEFORE `NativeViewport.start()` ran the self-tests — that is
`ForgeShapeActivity.applyTheme` reading the store ahead of `setTheme` — so the
first frame is Cool Light with no default-to-stored flicker.

One observation, recorded rather than hidden: the gizmo's canonical list is
uploaded twice on a cold start whose stored weight is not Regular — once at
device creation (the renderer's display snapshot is still the default at that
moment) and once more by `syncGizmoGeometry` before the first frame. It is a
few kilobytes through the existing staging path, it happens once per process,
and it is exactly the mechanism a later weight change uses; a warm process
uploads once per change and never per frame.

The planted file was removed and the app force-stopped afterwards, so the
device is left with no preferences on disk.
