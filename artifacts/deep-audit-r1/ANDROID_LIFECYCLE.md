# Android lifecycle — DEEP-AUDIT-R1 (Part E)

## What was read

`ForgeShapeActivity.java` (511 lines), `ForgeShapeSurfaceView.java`, `EditorWorkspaceView` lifecycle paths, `AutosaveController.java`, `NativeViewport` surface/renderer natives, the JNI surface handshake and `renderThreadMain`, `AndroidManifest.xml`.

## Configuration and process model

* `configChanges="orientation|screenSize|smallestScreenSize|keyboardHidden|screenLayout|density"` + `adjustResize`; a rotation the config absorbs does not recreate the Activity. A theme change and a density change DO recreate it, and `onDestroy` keeps native alive across a configuration change (`isChangingConfigurations()` gates `NativeViewport.stop()`), so the render thread, the Vulkan device and every GPU buffer survive — no re-upload, no self-test re-run. Verified in code.
* `NativeViewport.start()` is idempotent per process (a second call while the render thread lives returns immediately), so a recreation costs nothing.
* Process-scoped native state (scene, mode, active id, sculpt mesh, camera, display settings, both histories) outlives the Activity; the Java layer re-reads it in `onResume → syncFromNative`.

## Callback discipline (verified correct)

| Callback | Does | Verdict |
| --- | --- | --- |
| `onCreate` | `applyTheme` before `super`; `NativeViewport.start`; edge-to-edge; install diagnostics + Back dismissal; build workspace | correct order (theme before any resolved resource) |
| `onResume` | `syncFromNative`; `checkRendererLifecycle` (polls `RESTART_REQUIRED` once) | display refresh, not restore |
| `onStop` | `requestImmediateCheckpoint` (immediate, not debounced) — the last guaranteed callback before a kill | correct edge |
| `onDestroy` | `releaseAutosave` always; `NativeViewport.stop` only when not a config change | prevents an autosave-thread leak per theme change; keeps GPU across recreation |
| `surfaceDestroyed` | blocks until native releases the `ANativeWindow` (≤ 5 s ack) | render thread never touches a dead window |

## Findings

| ID | Sev | Finding |
| --- | --- | --- |
| — | OK | No lifecycle leak: the autosave `HandlerThread` is `quitSafely`'d in `onDestroy` and the rebuilt workspace makes its own; `quitSafely` runs the already-queued `onStop` checkpoint first (deliberate — the single most important write). |
| — | OK | System Back is registered only while a dismissible surface is open (API 33+ `OnBackInvokedCallback`, pre-33 `onBackPressed`), so predictive back and app-exit are untouched. `dismissTopmostSurface` routes through each surface's own control path (focus/keyboard/active-styling cannot drift). |
| — | OK | `onKeyDown` debug drivers (keyevents 1–9, A–G) are gated by native returning false in release; not a product path. |
| F-16 (ref) | P2 | The renderer self-tests run once from `start()` on the UI thread in a debug build — startup measured ~6 s from `ACTIVITY_CREATE` to `FORGESHAPE_NATIVE_VIEWPORT_OK` on the emulator (20 suites, 2981 checks). Debug-only; a release build skips the call. No ANR observed. Cross-reference: the suites still link into release (F-16). |
| — | OK | `onStop` reason is logged as `config`/`background` via `isChangingConfigurations`; the immediate checkpoint is correct for both. |

## Renderer lifecycle / device loss

`RenderRecoveryPolicy` (platform-neutral, self-tested without a GPU): a bounded 2 rebuild attempts, then `RestartRequired`, which checkpoints the project and tells the user a restart is needed rather than drawing through a corrupt device. The debug injection seam (`debugInjectDeviceLoss`) enters the real recovery path at the `VK_ERROR_DEVICE_LOST` point; a real loss has never been provoked on the authoritative emulator (correct — the repo forbids it). `rendererLifecycle` is an atomic never read on the render thread; `checkRendererLifecycle` reports `RESTART_REQUIRED` once per process. Verified: `FORGESHAPE_RENDER_RECOVERY_SELFTEST_OK (24 checks)` on the device launch.
