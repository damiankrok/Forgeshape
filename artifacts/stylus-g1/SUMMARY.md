# STYLUS-G1 — Real Stylus Evidence R1

**Result: `BLOCKED-ENV-STYLUS-G1`.**

No qualifying physical Android device with a compatible physical stylus was
attached or operable in this session, and the gate requires real user-made
stylus strokes. Nothing was measured, nothing was implemented, and no synthetic
input was substituted for any real-evidence point.

**Baseline:** `6c09c7c9d26da1b67b867f89bfb13b2d1317968a`, tree clean at start.
No product code, test code or shader was changed by this gate.

## SG1 matrix

Every case below is BLOCKED for the same single reason: the gate's own
§2.1 rule that PASS may rest only on a physical device, a physical stylus and
real events through the ForgeShape runtime. None of these is a product failure,
and none was attempted with a substitute.

| ID | Case | Result | Why |
| --- | --- | --- | --- |
| SG1-01 | Physical device / stylus identity | **BLOCKED-ENV** | Only emulator targets attached; `ro.kernel.qemu=1` on both. |
| SG1-02 | Pressure capability profile | **BLOCKED-ENV** | Needs real contact samples from a real stylus tip. Unmeasured — deliberately NOT classified. |
| SG1-03 | Hover capability profile | **BLOCKED-ENV** | Needs a real hovering digitizer. Not recorded as `HOVER_NOT_SUPPORTED_BY_TEST_HARDWARE`, because no qualifying hardware was tested at all. |
| SG1-04 | Stylus + finger/palm coexistence | **BLOCKED-ENV** | Needs simultaneous real stylus and real skin contact. |
| SG1-05 | Real system `ACTION_CANCEL` | **BLOCKED-ENV** | Zero of the permitted three real-device attempts were possible. Synthetic cancel is explicitly not a substitute. |
| SG1-06 | Real stylus reaches Sculpt | **BLOCKED-ENV** | Needs a real stylus DOWN/MOVE/UP through the live input path. |

## What is NOT claimed

- Pressure is **not** classified. `PRESSURE_SUPPORTED`,
  `PRESSURE_CONSTANT_OR_UNUSABLE` and `PRESSURE_NOT_EXPOSED` all require real
  samples; none exists.
- Hover is **not** classified in either direction.
- The `SCULPT-H1` real-stylus hover preview remains **unauthorized and
  deferred**, on its own unchanged autosave-safety blocker
  (`AutosaveController.performCheckpoint` reads the project fingerprint on its
  own worker thread at run time). This gate neither re-authorizes it nor
  weakens that blocker.
- No statement here widens what `PRODUCT.md` already says: nothing in the
  product reads `toolType`, `pressure` or tilt, so a stylus and a finger
  tracing the same pixels still produce identical geometry.

## What was done instead

Only the two pieces of gate work that need no hardware:

1. `INPUT_PATH.md` — the source-backed map of the real input path
   (§3 of the gate), including the existing debug-only observation seam and the
   binding Sculpt cancel contract quoted from the code that owns it.
2. `DEVICE_PROFILE.md` — the environment as actually found, and the exact
   conditions that would unblock a rerun.

## What would unblock this gate

1. A physical Android device attached by explicit serial — the owner-supplied
   Samsung Galaxy S25 Ultra (`SM-S938B`, Android 16 / API 36) already recorded
   as a runtime target in `PROJECT_STATUS.md` qualifies, and its S Pen is a
   compatible physical stylus.
2. The S Pen physically present, and an operator able to make real strokes:
   two light, two firm, a hover pass, a deliberate palm contact during a
   stylus stroke, a real focus/gesture steal to provoke `ACTION_CANCEL`, and
   one Clay/Inflate stroke with Undo/Redo.
3. A debug build of the exact baseline installed on that device, since the
   observation seam is compiled out under `NDEBUG`.

No repo change is needed to run the gate — see `INPUT_PATH.md` §4.
