# OWNER LATER test pack

What was **deliberately not automated** under `TEST-OWNER-03`, and what only a
person with the device in their hands can settle. Nothing on this list is a
known defect; it is the judgement the emulator cannot make.

---

## Stage 018A — the object commands

### Rename
- Rename across all four kinds: a Construction Body, an Imported Mesh, a CAD
  Body, and a body carrying a sculpt mesh. (The domain suites prove the
  operation is representation-neutral; what is unverified is that the row, the
  capsule, the panel title and the status line all read right in each case.)
- The inline field under a real thumb: is it wide enough at 220 dp, does the
  keyboard cover the row it belongs to, does **Done** feel like the commit.
- A long name, a right-to-left name and an emoji name **as drawn** — the bytes
  are proved exact; the ellipsis and the row height are not.
- Rename, then Undo, then Redo, watching the label follow.

### Show / Hide
- Whether the open-eye and struck-eye glyphs read at a glance, and whether a
  hidden row is distinguishable enough from a shown one without relying on
  colour alone.
- Hiding the object you are currently working on: does keeping the selection
  where it is feel right, or does it feel like the app lost the object.
- Hiding several bodies in a busy scene and finding them again.

### Lock / Unlock
- Whether a locked object that still highlights and still selects, but refuses
  to move, reads as *locked* rather than as *broken*.
- The moment of locking while the gizmo is on screen: the handles disappear
  under your hand. Is that clear or is it startling.
- The refusal message when a value is typed for a locked body.
- Whether Lock is discoverable at all where it now lives, behind the row
  overflow.

### Duplicate
- The copy naming: `Bracket copy`, `Bracket copy 2` — is that the right
  convention, and is the copy landing at the **end of the list** (rather than
  next to its source) the right place.
- The copy appearing exactly on top of the original, with no offset. This is
  deliberate — an automatic offset would be a placement the user did not choose
  — but it is worth an opinion.
- Duplicate across representations: a Construction Body, an Imported Mesh, a
  world-plane CAD Body, and a sculpted body.
- The **refusal** for a CAD body standing on another body's face: is the wording
  clear enough that the user understands why, and does refusing feel better than
  a copy that cannot be moved.

### The row and the strip
- Whether one overflow plus an inline strip is the right shape, or whether the
  four commands want a different surface entirely.
- Whether the strip pushing rows down (rather than floating over them) is right
  in a long list.
- Left-handed layout: the strip and its four controls under a left thumb.
- All five palettes: the strip's icons against each ground.

### Undo / Redo of each
- Rename → Undo → Redo, Hide → Undo → Redo, Lock → Undo → Redo,
  Duplicate → Undo → Redo, each watched in the chrome rather than asserted.

### Interaction with the outline and the gizmo
- How a **hidden** selected body feels: the row is still selected, the viewport
  shows nothing, no outline. Is that coherent?
- How a **locked** selected body feels: the outline is there, the handles are
  not.

---

## Still pending from earlier stages

- **Delete → Undo → Redo** — the `IMPORT-01B` / `UI-OWNER-45` owner verdict,
  still outstanding. Stage 018A did not change Delete and its cases here are
  non-collision evidence only.
- **Settings, the five palettes, handedness and the gizmo preferences** —
  UI-PREF-R1's combined retest, closed under an owner waiver of the aggregate.
- **Selection Outline** — its visual weight and its toggle. SEL-OUT-R1 is closed
  on the technical side; whether the band reads as the right weight under a real
  thumb, on a dark studio ground and on the two light papers, is the OWNER's.
  Further SEL-OUT testing was waived.
- **The CAD sketch workflow** end to end.

---

## Known test debt carried forward

**`SpatialSketchTest` suite isolation.** Two cases fail when the class runs
after `GlbImportPreviewTest`, because `setUp` resets the active body but does
not clear bodies an earlier class left in the scene. Bisected during SEL-OUT-R1
and proved to reproduce at the clean baseline with that stage's work stashed
(`artifacts/sel-out-r1/PREEXISTING_SHARD5_FAILURE.md`). **Not fixed in Stage
018A**, by instruction. Because no `-FullSharded` aggregate is run under the
reduced-testing policy, it blocks nothing here — but it will block the next
aggregate, and it belongs in the next test-hardening batch.
