# The start pages: Home and New Project (`SKETCH-UX-R1` A)

## One class, two pages

`StartPageView` owns the shape both take. It is not a variant of
`ChooserSurfaceView`; it is the opposite of one.

| | `StartPageView` | `ChooserSurfaceView` |
| --- | --- | --- |
| Ground | opaque, full window | partial scrim over the live viewport |
| Content | full-width rows, capped and centred on a wide window | one raised centred panel |
| Behind it | nothing — no project exists | a project the user can see |
| Used by | Home, New Project | unsaved changes, recovery |

## Where they sit in the hierarchy

Both are added to **`EditorWorkspaceView` itself**, not to `overlayRoot`.
`overlayRoot` carries the chrome's window-inset padding, and a page is the
window's own ground: the system bars draw over it, exactly as they draw over
the Vulkan viewport. Each page pads its **content** by the same inset rect
instead (`applyContentInsets`), so the text and the actions clear the bars
while the page's ground still reaches every edge.

## Composition

1. **The mark and the name.** `ic_forgeshape_mark` is the product's own
   geometry — a closed profile and the depth it was extruded to — drawn on the
   same 24-unit grid as every other icon and tinted at use, so it belongs to
   the icon set rather than sitting outside it as a logo. The mark and the name
   are ONE node to a screen reader.
2. **The headline** (`Start`, `New Project`) and one line of subtitle.
3. **The actions**, full-width rows recessed into the page — the same tonal
   idea the chooser's cards used, because a raised card on an opaque page would
   bring back the floating-dialog reading.
4. **The page's own status line**, GONE until it has something to say. This is
   the one moment the toolbar's status capsule is not on screen, so a refused
   Open has to say so here.
5. **A quiet secondary action** at the foot, on a subpage: `Back`.

## The motif

`bg_start_page_motif` is a static vector: a drafting grid with one extruded
corner, at half alpha, anchored to the bottom trailing corner and sized to half
the window's smaller dimension. It is **structurally incapable of being an
editor viewport**: it is not clickable, not focusable, not in the accessibility
tree, and contains nothing that can be selected, moved or picked. It exists so
a full-screen opaque page does not read as a blank error state.

## New Project replaces Home

`refreshShellPhase` hides Home while the New Project page stands. As two opaque
full-window pages they would otherwise be **stacked** — invisibly, but really,
with two focusable action sets in the view tree at once. `CADUXR1-04` caught
exactly this and is what the rule is now asserted by.

Over an **open** project the New Project page still stands in front, because
there it is one step away from a workspace the user is about to leave.

## What did not change

Every lifecycle semantic of `APP-H1`: `hasProject()` is still the one answer to
whether a project is open; nothing is drawn, picked, saved, checkpointed or
fingerprinted behind Home; no default primitive is fabricated; Open File takes
the same SAF contract and a refused file leaves Home standing with the verdict
on it; the dirty guard still asks Save / Discard / Cancel by fingerprint; and
`HomeFlowTest` still asserts that no active body is ever read while none exists.
