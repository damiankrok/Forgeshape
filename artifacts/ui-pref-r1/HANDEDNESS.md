# Handedness — UI-PREF-R1 Part C (UI-SPEC-R0 Rev1 mirrored-zone reservation)

## What mirrors

`EditorWorkspaceView.applyHandedness(Handedness)` re-seats the members of the
middle row in mirrored ORDER and swaps their edge margins. Nothing else.

| member | right-handed (default, UI-LAYOUT-R2) | left-handed |
| --- | --- | --- |
| trailing host (Tool Rail + selectors + precision toggle) | last in the row, `rightMargin = brush_gap` (8 dp), `leftMargin = 0` | first in the row, `leftMargin = brush_gap` (8 dp), `rightMargin = 0` (`WorkspaceTrailingHostView.setMirrored`) |
| side-placed precision surface (short landscape / expanded) | seated BEFORE the host, `leftMargin = row_gap_small`, `rightMargin = overlay_anchor_gap` (toward the host) | seated AFTER the host, `leftMargin = overlay_anchor_gap` (toward the host), `rightMargin = row_gap_small` |
| Sculpt brush controls | second in the row, `leftMargin = brush_gap` | second from last, `rightMargin = brush_gap` |
| expanded-window Objects column | first in the row, `leftMargin = row_gap`, `rightMargin = row_gap_small` | last in the row, mirrored margins |
| the weighted gap (the model) | between | between |

Width, top, the 48 dp floors, the internal vertical scrolling under IME
pressure and every callback are unchanged: the host's `parentLayoutParams`
still carry `trailing_host_width` and `row_gap − toolbarExpansion` on top,
and only which margin holds the 8 dp inset moves.

Anchored surfaces that grow out of the Objects capsule (the Objects panel,
Add Primitive) are kept clear of a LEFT rail by `leadingLimitFor`, the mirror
of the existing `trailingLimitFor`, which becomes the window edge while the
rail is on the left. They are also closed by a switch, because they were
anchored against the old edge's limits.

## What does not mirror

- The Global Toolbar and the bottom row (Objects capsule, history capsule) —
  not part of the rail zone the reservation names.
- The sketch orientation navigator (upper trailing corner) — the rail on the
  left does not overlap it, so its own contract stands untouched.
- Every CAD coordinate, world axis, workplane, the camera, the gizmo's
  solvers and handle placement, every object transform, every exported byte,
  every gesture meaning. `uiprefr1_16_18` records the X handle's pixel, the
  camera pose and the whole native snapshot, switches to Left, and asserts all
  three identical, with the tool (Transform), the gizmo mode (Rotate) and the
  selection kept and the history depth still 0.

## Runtime switch

- Applied immediately from the Settings row (`onHandednessChosen` → store →
  `applyHandedness`), with no recreation.
- With Exact open: opening the Settings page is an opaque page over the
  workspace and CLOSES the precision surface (and its keyboard) first, so the
  switch never operates on a live panel through the page. A handedness change
  applied while Exact IS open (the store changing behind the page —
  `reapplyPreferencesForTest`) re-seats the open panel on the new edge with
  its standoff toward the host; `uiprefr1_15_17` proves both paths and that
  the panel never intersects the rail.
- Survives rotation (the placement is re-derived in `applyLayoutForWindow` →
  `placeInspector` with the same handedness), recreation (`uiprefr1_05`) and
  relaunch (the store).

## Measured invariants

- `uiprefr1_03_04` / `assertRightHandedFrame`: with defaults, the host is the
  last member of the row, its width is `trailing_host_width`, and
  `window.right − chromePadding.right − host.right == 8 dp` — the accepted
  UI-LAYOUT-R2 frame. `EditorWorkspaceRightHostPlacementTest` (UILR2C-01..12)
  is unchanged and still passes on the default.
- `uiprefr1_13_14` / `assertLeftHandedFrame`: after Left,
  `host.left − chromePadding.left == 8 dp`, the width and the top equal the
  right-handed frame's, the host is the FIRST member of the row, and no second
  rail stands on the right; switching back restores the right frame `Rect`
  exactly.
- The device frames `14_right_handed_editor`, `15_left_handed_editor` and
  `16_left_handed_exact_open` carry `rail_left_inset_dp`,
  `rail_right_inset_dp`, `rail_width_dp`, `rail_top_dp` and
  `exact_overlaps_rail=false` (`VISUAL_EVIDENCE.md`).
