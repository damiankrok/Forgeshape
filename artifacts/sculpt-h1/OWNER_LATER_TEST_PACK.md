# SCULPT-H1 — OWNER LATER test pack

The Sculpt History navigator is closed on the technical side. What no emulator
settles is below. **No aesthetic approval is claimed for anything in this list**,
including the navigator glyph, whose exact visual design is explicitly OWNER
LATER.

Append these to the existing combined OWNER pack rather than reviewing them in
isolation — they sit beside the still-pending MIRROR-01, Stage 020M, Stage 018A,
UI-PREF-R1 and SEL-OUT-R1 items.

## Placement and readability

1. The navigator's trigger is the **third icon in the bottom-trailing history
   capsule**, after Undo and Redo. Does a three-icon capsule still read as one
   capsule, or does the third control make it a bar?
2. The surface grows **upward** out of that capsule, 184 dp wide, standing off
   the row by one control height plus the shared anchor gap. Does it clear the
   capsule cleanly, and does it stand on the model rather than on anything live?
3. Row wording: **`Start`**, then **`Stroke 1`**, `Stroke 2`, … Does `Start` read
   as "before you sculpted anything"? Is `Stroke N` the right noun?
4. The **current row** is marked by a filled `●`, a tinted fill, and a medium
   weight; a past row is `○` and a future (undone) row is `◌` at 60 % alpha. Is
   the current state unmistakable at a glance, and are past and future
   distinguishable from each other without reading the glyph closely?

## Five visible rows

5. `VISIBLE_ROWS = 5` is a **viewport target, not a history bound** — the branch
   keeps whatever `SculptHistory` retained (up to 32 entries) and the list
   scrolls over all of it. **Does five feel right?** Too few on a tall phone? Too
   many on a short landscape window?
6. Scrolling with a long history: take 15–30 strokes, open the navigator, and
   scroll. Does the list open showing the part you are working in (it scrolls the
   cursor into view), and is the scroll comfortable inside a 184 dp panel?
7. When strokes have been evicted, a caption reads **"Older strokes were dropped
   to stay in budget."** Is that the right thing to say, in the right place?

## Tap-to-jump feel

8. Tap an older row, then a newer one, then the one you are on. Does the model
   change quickly enough to feel direct? Is the absence of any confirmation
   message right (it is deliberate — the model changing is the feedback)?
9. The navigator **stays open** after a jump, so comparing several states is one
   gesture each. Is that the right choice, or should it dismiss?
10. Jump backward, then make a new stroke: the abandoned future disappears from
    the list. Is that legible enough, or does it need to say something?

## Context

11. **Imported Mesh sculpt**: the same navigator over a body imported from a
    `.glb`. Does anything read differently?
12. **Back to Construction → Resume Sculpt**: the list and the cursor come back
    where you left them. Does that feel right?
13. **Body switching**: sculpt two bodies, switch between them. Does the
    navigator's rebinding to each body's own branch feel obvious, or does it need
    to name the body?
14. **Handedness** (Left and Right) and all **five palettes**: the navigator is
    anchored to the bottom-trailing corner in both handednesses, because the
    bottom row is not mirrored. Confirm that is right, and that the surface reads
    correctly on the two light palettes as well as the three dark ones.
15. The **navigator glyph** (`ic_history_navigator.xml`): a vertical spine of
    three nodes with the middle one filled, beside three rules. It is
    deliberately not a clock and not a third curved arrow. **Exact visual design
    is OWNER LATER and is not approved here.**

## Real-stylus hover preview — DEFERRED, and why

16. Hover preview was **not implemented**, and nothing fake was substituted. The
    blocker is concrete and is not a UI problem:
    `AutosaveController.performCheckpoint()` runs on its own `HandlerThread` and
    reads `NativeViewport.projectFingerprint()` **at the moment the task runs**,
    by deliberate design ("newest wins"). A preview that moved sculpt vertices
    and moved them back would therefore be readable by a checkpoint that fired
    mid-preview, and the recovery candidate would then hold a state the user
    never committed. Making it safe needs either an **autosave-suspend concept**
    threaded through the controller and the workspace, or a **shadow render
    path** so the preview never touches the live mesh — both broad changes with
    their own risk, and neither is this stage's scope.
    A second, independent obstacle: the authoritative emulator does **not deliver
    real stylus hover**, so the "preview restores the exact committed state"
    claim could not be verified on this hardware, and the project's existing rule
    is that hardware hover is not claimed on the emulator.
    **Owner decision wanted:** is a non-destructive hover preview worth an
    autosave-suspend concept, or should the tap-to-jump interaction stand alone?
