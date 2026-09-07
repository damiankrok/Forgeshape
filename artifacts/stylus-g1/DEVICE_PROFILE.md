# STYLUS-G1 — Device profile as found

No physical device was attached. Both targets visible to `adb` were emulators,
and neither is a qualifying device for this gate under §2.1.

## Attached targets

| Serial | AVD (`adb -s … emu avd name`) | `ro.kernel.qemu` | `ro.build.characteristics` | Model | Android | Qualifies? |
| --- | --- | --- | --- | --- | --- | --- |
| `emulator-5580` | `ForgeShape_Stage006` | `1` | `emulator` | `sdk_gphone64_x86_64` | 16 | **No — emulator** |
| `emulator-5560` | `CarPassport_API_36` | `1` | `emulator` | `sdk_gphone64_x86_64` | 16 | **No — emulator, and foreign to ForgeShape** |

`emulator-5554` was never contacted. `emulator-5560` belongs to another
program; it was identified read-only and otherwise left alone.

`ForgeShape_Stage006` is the project's authoritative emulator target and is
explicitly disqualified for real-stylus PASS by the gate itself. It was not
used to manufacture stylus evidence.

## No stylus hardware

No physical stylus was available, so no digitizer was enumerated and no
`TOOL_TYPE_STYLUS` pointer could reach ForgeShape. An emulator reports
`TOOL_TYPE_FINGER` for injected input, and a finger, `adb shell input`, an
instrumentation-built `MotionEvent` and a debug flag are each ruled out by
§2.1 as substitutes.

## Qualifying hardware that exists but was not attached

`PROJECT_STATUS.md` records an owner-supplied physical ARM64 target used for
Gate P1: **Samsung Galaxy S25 Ultra (`SM-S938B`)**, Snapdragon 8 Elite
(`SM8750`), Android 16 / API 36, 1440×3120, `arm64-v8a`, attached over Wi-Fi
adb with the serial supplied per session and deliberately not recorded in the
repo. That device ships with an S Pen and would satisfy the hardware half of
this gate. The owner confirmed in this session that it could not be attached
and operated now.

Per §4, no unique physical serial is recorded here. Device family, model and
Android version are sufficient to identify the class of hardware, and the
per-session serial stays out of the repo exactly as the existing status entry
already requires.
