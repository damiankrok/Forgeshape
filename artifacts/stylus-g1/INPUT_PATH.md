# STYLUS-G1 §3 — The real input path, from source

Source-backed only. Nothing here was measured on hardware; it is the map a real
run would instrument, recorded at baseline `6c09c7c`.

## 1. The path

```
android.view.MotionEvent
  -> ForgeShapeSurfaceView.onTouchEvent            (Java, the ONE Android touch entry)
  -> PointerSemantics.neutralToolType              (the ONE Android->neutral tool map)
  -> NativeViewport.touchEvent                     (JNI, plain int/float arrays)
  -> Java_..._NativeViewport_touchEvent            (forgeshape_jni.cpp:6314)
  -> forgeshape::TouchPointer / TouchAction        (forgeshape_input.h, platform-neutral)
  -> gizmo / sketch / sculpt / camera arbitration  (forgeshape_jni.cpp)
  -> SculptSession::beginStroke / updateStroke / endStroke / cancelStroke
```

A separate, non-touch path exists for hover:

```
MotionEvent (HOVER_*)
  -> ForgeShapeSurfaceView.onHoverEvent            (ForgeShapeSurfaceView.java:135)
  -> NativeViewport.supportChooserHover            (NativeViewport.java:2275)
```

## 2. Where each field is read

| Question | Answer | Location |
| --- | --- | --- |
| Where is `toolType` read? | `event.getToolType(i)` mapped by `PointerSemantics.neutralToolType` onto ForgeShape's own wire codes (`Unknown 0, Finger 1, Stylus 2, Eraser 3, Mouse 4`). No `MotionEvent.TOOL_TYPE_*` constant crosses JNI. | `ForgeShapeSurfaceView.java:83`, `PointerSemantics.java` |
| Where is `pressure` read? | `event.getPressure(i)`, passed across raw and sanitized on the native side by `sanitizePointerPressure` — non-finite becomes the default, finite is clamped to `[0, 1]`. | `ForgeShapeSurfaceView.java:84`, `forgeshape_jni.cpp:6388` |
| Where is pressure **consumed**? | **Nowhere.** It is carried and stored only. No brush, camera, selection or sketch rule reads it — the invariant `forgeshape_input.h` states as "Carrying them is ALL that happens today". A pressure-driven brush is Stage 026 and does not exist. | `forgeshape_input.h` header comment |
| Default when hardware reports nothing? | **Full pressure (`1.0`), not zero** — a device with no force sensor is still in full contact, and defaulting to 0 would make a future pressure brush do nothing on most hardware. | `kPointerPressureDefault`, `forgeshape_input.h` |
| Where is tilt read? | `AXIS_TILT` (radians from perpendicular) and `getOrientation(i)`, sanitized together so a zero tilt always reports a zero orientation. | `ForgeShapeSurfaceView.java:89`, `forgeshape_jni.cpp:6392` |
| Where are hover events received? | `onHoverEvent`, for `HOVER_ENTER` and `HOVER_MOVE` only. It consumes the event **only** when `supportChooserHover` reports a lit target; otherwise it defers to `super`. `HOVER_EXIT` is not handled. | `ForgeShapeSurfaceView.java:135-145` |
| Can hover start a stroke? | **No, structurally**: hover never reaches `NativeViewport.touchEvent`, so it cannot produce a `TouchAction` at all. The one thing it may do is highlight a spatial support chooser target. | same |
| How are stylus/touch/palm distinguished? | They are **not** distinguished by tool type. Arbitration is by pointer **count** and motion, not by what is touching: a one-finger Down on the mesh is held `g_strokePending` and promoted to a stroke only after `kStrokeArmPixels` (8 px) of travel; a second pointer ends the stroke cleanly and the gesture becomes camera navigation. | `forgeshape_jni.cpp:255-263, 6678-6693` |
| Pointer cap | 6 (`MAX_POINTERS` / `kMaxTrackedPointers`); extra pointers are truncated, never dropped as an event. | `ForgeShapeSurfaceView.java:25`, `forgeshape_input.h` |
| How does `ACTION_CANCEL` reach Sculpt? | `getActionMasked()==3` → `kActionCancel` → `TouchAction::Cancel` → `sculpt.cancelStroke()`. | `forgeshape_jni.cpp:328, 6801` |

## 3. The binding cancel contract (quoted, not assumed)

`SG1-05` requires the live contract rather than an assumption that cancel means
rollback. It does **not**. From `forgeshape_sculpt.h:649-658`:

> Abandons the stroke. Positions already written stay written — a cancelled
> stroke is a stroke that stopped, not one that is rolled back.
>
> Sculpt Undo does not change that rule, it completes it: because the
> deformation stands, it must be UNDOABLE, so `SculptSession::cancelStroke`
> records the same single entry an ordinary end would. A cancel that moved
> nothing still records nothing. What must never happen — and cannot, because
> the entry is built in one go from the affected set — is a PARTIAL entry
> describing half a stroke.

`endStroke()` and `cancelStroke()` share one implementation of the recording
step, `recordActiveStroke()` (`forgeshape_sculpt.h:1035`), which is what makes
"one completed stroke is one entry" structural rather than a convention two
call sites happen to share.

**Therefore the SG1-05 pass condition on real hardware is:** after a real
system cancel, the deformation **stands**, exactly one history entry exists for
that stroke, Undo removes it, no pointer or stroke state is left open, and the
next stylus stroke behaves normally. A run that asserted rollback would be
asserting against the product's own contract.

## 4. Observability — no new hook is needed

The gate's §4 preference is to prove `action`/`toolType`/`pointerId`/`pressure`
with existing seams. One already exists and is sufficient for identity:

`NativeViewport.debugLastPointerEvent(float[])` (`NativeViewport.java:133`,
`forgeshape_jni.cpp:1824`) reads back the platform-neutral snapshot of the last
touch event — count, then 7 floats per pointer: `id, x, y, toolTypeCode,
pressure, tilt, tiltOrientation`. It takes the state lock, changes nothing (no
mesh, revision, camera or selection), and is compiled out entirely under
`NDEBUG`, so it has no release footprint. `EditorWorkspacePointerTest` already
uses it for synthetic stylus transport.

**One honest limitation for a future real run:** it is a *snapshot of the last
event*, not a trace. It proves SG1-01 (a real `TOOL_TYPE_STYLUS` pointer with
its pressure arrived intact and stayed attached to the right pointer id), but
polling it cannot reliably reconstruct the per-`MOVE` sample series SG1-02 asks
for (count, min, max, median across light and firm strokes) — samples between
polls are simply overwritten.

Closing SG1-02 rigorously would therefore need either a bounded debug-only ring
buffer of contact samples behind the same `NDEBUG` boundary, or a capture taken
above JNI in the Java adapter. Both are debug/test-only in principle and neither
was written here, because §4 forbids adding a hook that is not needed to close a
case that hardware could not open in the first place. That decision is left to
the coordinator with the rerun.
