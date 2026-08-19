# CLAUDE.md — durable rules for this repository

Short and durable. Not a stage diary.

## Workspace

`D:\TRAVELAPPS\ForgeShape` — standalone Android app, Java shell + native C++17
Vulkan renderer.

## Commands

```
gradlew.bat :app:assembleDebug
adb -s <serial> install -r app\build\outputs\apk\debug\app-debug.apk
adb -s <serial> shell am start -n com.forgeshape.app/.ForgeShapeActivity
adb -s <serial> logcat -s ForgeShape:V
```

APK: `app/build/outputs/apk/debug/app-debug.apk`

Success tokens in logcat: `FORGESHAPE_NATIVE_VIEWPORT_OK`,
`FORGESHAPE_CAMERA_SELFTEST_OK`, `FORGESHAPE_PICKING_SELFTEST_OK`,
`FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK`, `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK`,
`FORGESHAPE_CONE_CAPSULE_SELFTEST_OK`,
`FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK`.
Failures: `FORGESHAPE_NATIVE_VIEWPORT_FAIL:*` and the matching `*_SELFTEST_FAIL`.

Enlarge the log ring buffer (`adb -s <serial> logcat -G 16M`) before capturing
startup evidence: the default buffer drops part of the self-test output and it
looks like a suite that stopped partway.

Camera, picking, dynamic-mesh, Construction-box and sculpt self-tests are
debug-only and run once from `NativeViewport.start()`. They must never run per
frame.

## Hard rules

- **No engine.** Godot, GDExtension, Unity, Unreal or any general-purpose engine
  that owns the viewport or render loop is prohibited in the product.
- **No third-party runtime, rendering, math or input library.** Math lives in
  `app/src/main/cpp/forgeshape_math.h`. No GLM.
- **Do not initialize Git at the repository root.** No commits, branches,
  remotes, pushes or global Git config changes. Non-mutating checks are fine.
- **Read `PROJECT_STATUS.md` first, and update it yourself** before reporting a
  stage complete. It records only verified current facts and names exactly one
  next stage.
- **No scope expansion.** Implement what the stage asks for and stop. Do not
  start the next stage.
- **Preserve existing owner changes.** Modify only what the task requires; do not
  opportunistically refactor the renderer or bundle unrelated debt fixes.
- **One brush kernel, no brush framework.** A sculpt tool is a deformation rule
  inside `SculptStroke`, selected by a closed enum and a switch. Do not add a
  brush base class, registry, plugin surface or reflection: brushes are code, not
  data, until a stage pays for making them data.
- **A gesture that becomes multi-touch navigation must never mutate the sculpt
  mesh.** No vertex written, no `SculptRevision` minted, no stroke committed.
- **The Construction Source is never written by sculpting.** The object has two
  representations — the exact primitive plus its parameters and placement, and
  the Frozen Sculpt Mesh. A sculpt edit may never change a primitive parameter,
  a `PrimitiveKind` or a transform, and no Construction parameter may ever be
  reconstructed from sculpt vertices. Adopting a changed Construction shape into
  the sculpt mesh is always an explicit user act, never automatic.

## Documentation ownership

One durable fact has exactly one primary owner.

| File | Owns |
| --- | --- |
| `PROJECT_STATUS.md` | current status, verified capability, environment facts, next stage |
| `ARCHITECTURE.md` | current production architecture and module ownership |
| `PRODUCT.md` | runtime-verified user-visible behaviour only |
| `README.md` | required tooling, build/run/verify instructions |
| `CLAUDE.md` | these rules |

Never document a feature as implemented unless it is runtime-verified. Unverified
paths are reported as UNVERIFIED, not as behaviour.

## Android / Vulkan safety

- The emulator dies if launched as a child of a tool shell. Spawn it detached
  (e.g. WMI `Win32_Process.Create`).
- When more than one Android target is connected, every `adb` command must use
  an explicit `adb -s <serial> ...`. Never rely on the default target.
- `AVD Medium_Phone_API_36.1` / `emulator-5554` is **reserved by another program**
  and must not be used, started, stopped, modified, installed to, logged,
  screenshotted or sent input by ForgeShape work, until the owner lifts this.
- `AVD ForgeShape_Stage004` / `emulator-5556` is **contended**: another program
  runs on it, steals the foreground and injects input. Do not use it for
  authoritative runtime evidence, and do not stop, wipe or reconfigure it.
- Runtime evidence is taken on a ForgeShape-owned isolated AVD (currently
  `ForgeShape_Stage006` / `emulator-5558`). Before any evidence-sensitive input
  or screenshot, confirm ForgeShape is the resumed activity, and invalidate any
  run contaminated by foreign input. Creating a new AVD from already-installed
  tooling is allowed; installing or updating SDK/NDK/JDK/system images is not.
- `surfaceDestroyed` must block until native code has released the
  `ANativeWindow`. Never let the render thread touch a destroyed window.
- Shaders are compiled ahead of time by the NDK `glslc` in CMake. No shader
  compiler ships at runtime.
