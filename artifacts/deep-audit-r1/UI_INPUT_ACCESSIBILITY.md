# UI, input and accessibility — DEEP-AUDIT-R1 (Part N)

## What was read

`EditorWorkspaceView.java` (4441 lines), `ForgeShapeSurfaceView` (touch/hover forwarding), the Global Toolbar, Tool Rail, trailing host, Objects capsule/section/popover, Add Primitive palette, the two start pages, the sketch navigator and dimension label, `EditorControlStyles`, `res/values/{ids,dimens,strings}.xml`, and the JVM `WorkspaceLayoutModeTest`/`EditorUiStateTest`/`ChromeMotionTest`.

## Confirmed

* **Semantic ids only.** 218 ids in `ids.xml`; every verification path resolves an id, never a coordinate (the one allowed coordinate is the touch harness injecting into the `SurfaceView`, where nothing is inferred from the pixel). Rows share one `object_row` id and carry the `ObjectId` as a tag.
* **Input arbitration** (single gesture, in order): support chooser → sketch → gizmo → sculpt pending/promote → camera+selection. A multi-touch gesture whose first finger lands on the sculpt mesh is structurally unable to deform (PENDING-then-promote, second finger abandons). Chrome consumes its own gestures (every surface returns true from `onTouchEvent`).
* **Full-bleed viewport**: no layout decision insets the `SurfaceView`; the IME is consumed as chrome padding only. `applyChromeInsets` follows the animated IME inset frame-by-frame.
* **48 dp interactive floor** reached with padding, not by growing a drawn box (`iconButton` insets glyph inside `icon_button_size`; `setMinimumHeight(control_height)` on chips, rows, fields, nav chips, start-page actions — 9 call sites verified). A control that cannot succeed is absent, not disabled (creation/Delete while sculpting, Shape on an Imported Mesh, the `+` while sketching).
* **Accessibility.** `setContentDescription` on 41 controls across 20 view classes; the way-out-of-Sculpt short forms keep the full wording as the content description. Reduced motion: `ChromeMotion` obeys a zero animator-duration-scale exactly (lands on the final state, posts no animator); pushed on every `syncFromNative`.

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| existing | P3 | Reduced motion is pushed on refresh, not observed: changing the setting in the foreground and immediately selecting a body can give one pulse decided by the previous value. Needs a `ContentObserver`; PROJECT_STATUS records it. | reported |
| existing | P3 | A horizontal drag along the Property Inspector header toggles it (default click fires on `ACTION_UP` inside bounds). Harmless (never reaches the viewport); PROJECT_STATUS records it. | reported |
| existing | P3 | The primitive chooser's 3-column chips clip labels ("Cylinder"→"Cyli") in a narrow docked inspector. Pre-existing; PROJECT_STATUS records it. | reported |
| — | OK | No new input/accessibility defect. Stylus is carried, consumed by nothing (a stylus and a finger produce identical geometry); hardware hover is not claimed on the emulator; the support-chooser hover highlight is a synthesized-hover test path only. |
| — | OK | WCAG: one verdict colour (Light Charcoal error red 2.4:1) is below AA and owner-approved/fixed (theme suite asserts 2.4:1); typed values held to 4.5:1 in all three appearances. PROJECT_STATUS records it. |
