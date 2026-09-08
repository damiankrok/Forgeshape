# UI-3D-STATE-C2 — Finding closure

One finding. The other six were closed by `UI-3D-STATE-C1` and are unchanged by
this correction.

---

## UI3D-F-005 — `SketchOverlayStyle::Dimension` rendered fully transparent

| | |
| --- | --- |
| **Severity / family** | P2, `OVERLAY_RENDER_STYLE` |
| **Affected surfaces** | S32 (the sketch line annotation, entirely) and S21 (the ACTIVE-axis dimension leader only) |
| **Layer** | renderer |
| **Status** | **CLOSED** |

### Before

`artifacts/ui-3d-state-audit-r1/screenshots/ui3d07_01_line_selected.png`: a
straight Line is selected, `sketchLineDimension` reports it (length 1.6 m), the
numeric chip `1.6 m` is drawn — and there are **no extension lines, no dimension
line and no end ticks anywhere in the frame**. `ui3d05_05_after_zoom_out.png`
shows the Stage 020M leaders clearly, because no axis is active there and all
three ranges take `Entities`; the invisibility bit exactly on the axis being
edited.

This correction reproduces the same "before" as a NUMBER on the final tree, from
the state that is equivalent to it: with nothing selected, the strip along where
the dimension line stands carries **0** pixels of the annotation's colour
(`FACTS_ui3dc204_theSelectedLineDimensionIsDrawn.txt`), and with no active axis
the whole viewport carries **0** (`FACTS_ui3dc205_...txt`).

### Root cause, all four links re-verified on the baseline

1. **Producers emit it, and still do.**
   `forgeshape_sketch_session.cpp` styles the whole line annotation `Dimension`
   (`dimension.style = SketchOverlayStyle::Dimension;` in `buildOverlay`);
   `forgeshape_body_dimension_overlay.cpp` styles the **active-axis** range
   `Dimension` (`activeRange.style = SketchOverlayStyle::Dimension;`) while the
   other two take `neutralRange.style = SketchOverlayStyle::Entities;`. Neither
   file is touched by this correction — `git diff --stat` shows it.
2. **The renderer had no mapping.** `Renderer::recordSketchOverlayDraw` switched
   on `range.style` with four cases and no `default:`.
3. **The alpha was therefore zero.** `GizmoPush push{}` is zero-initialised
   inside the per-range loop, and `gizmoHighlightColor` writes rgb only, so
   `push.highlight[3]` stayed `0.0f`.
4. **The shader multiplies by it.** `shaders/gizmo.vert`:
   `fragColor = vec4(rgb, pc.highlight.w * weight);`, and the gizmo pipeline
   blends with `SRC_ALPHA`.

### Fix

The per-style weights left the renderer for a pure function,
`sketchOverlayStyleWeights` (`forgeshape_sketch_overlay.{h,cpp}`), which now
owns all five styles including `Dimension`:

| style | `neutralLevel` (`axisX.w`) | `alpha` (`highlight.w`) |
| --- | --- | --- |
| `GridMinor` | `neutral` | `kGizmoAxisAlpha × 0.22` |
| `GridMajor` | `neutral` | `kGizmoAxisAlpha × 0.45` |
| `Axes` | `neutral` | `kGizmoAxisAlpha × 0.90` |
| `Entities` | `neutral × 0.35 + highlight.r × 0.65` | `kGizmoAxisAlpha` |
| **`Dimension`** | **`neutral × 0.35 + highlight.r × 0.65`** | **`kGizmoAxisAlpha × 0.85`** |

The four older rows are the previous expressions unchanged, and a self-test
asserts them by exact float equality on every one of the five grounds. The new
row stands at the entity LEVEL so the annotation is never mistaken for the grid,
and just under the entity WEIGHT so it never out-reads the stroke it measures.
Every vertex of both producers carries the emphasis tag, so the colour drawn is
the gizmo's existing held-handle amber and no new hue was introduced.

The renderer now refuses a range whose style has no mapping instead of recording
a draw at alpha 0.

### Guard against a repeat

Two, deliberately in different layers, both proved by temporarily adding a sixth
enum value and then removing it:

- **Compile time.** The mapping's switch has no `default:`, so the NDK clang
  emits `warning: enumeration value 'ProbeStyleTemporary' not handled in switch
  [-Wswitch]`.
- **Test time.** `overlay_every_style_has_a_mapping_on_every_ground` and
  `overlay_the_style_count_states_the_whole_enum` both failed with the sixth
  value present (`gizmo checks=176 failed=2` on the standalone NDK runner), and
  both pass without it (`gizmo checks=176 failed=0`).

### After

| | before | after |
| --- | --- | --- |
| **S32** the sketch line annotation, along the dimension line native reports, in a strip 78.75 px clear of the stroke | 0 px | **270 px** |
| **S21** the Stage 020M leaders, whole viewport, geometrically identical frames (183 979 ink in both) | 0 px | **86** sampled px |

Frames: `screenshots/ui3dc2_04_before_nothing_selected.png` →
`ui3dc2_04_after_line_selected.png`, and
`ui3dc2_05_before_no_active_axis.png` → `ui3dc2_05_after_x_axis_active.png`,
with `ui3dc2_05_product_path_x_editor_open.png` showing the state reached by
tapping the product's own label.

**The "after" is the `Dimension` style and not an Android imitation.** The
counted pixels are inside the Vulkan viewport, in a region computed from
`sketchLineDimension` and `sketchScreenPoint`; the numeric chips are excluded by
their own view bounds; and no Java or XML file draws a line for either surface —
the whole product diff outside the two test files is four files, one of which is
a `CMakeLists.txt` line.

### What stays open

Nothing about `UI3D-F-005`. The exact colour, alpha, weight and contrast are an
owner judgement and are listed in `OWNER_LATER.md`; the technical visibility this
finding was about is closed.
