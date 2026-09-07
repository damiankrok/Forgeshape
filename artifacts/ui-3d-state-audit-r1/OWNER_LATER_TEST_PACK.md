# UI-3D-STATE-AUDIT-R1 — Owner Later Test Pack

Three questions this audit deliberately does **not** answer, because they are
about how something reads to a person and no emulator settles that. Everything
mechanical is already answered in `FINDINGS.md` and `SUMMARY.md`.

These are **not** approvals being sought. They are the questions a correction
stage will need answered before it can choose between two correct fixes.

---

## OL-1 — Where should a world-anchored number stand once it stands correctly?

**Why it is a question.** UI3D-F-001 puts every anchored surface 48.8 dp below
its anchor today. Removing that offset centres each number **exactly on** its
anchor — the midpoint of the dimension line for Stage 020M, the profile
centroid for the CAD cluster. A number sitting exactly on the line it measures
is the technical-drawing convention, but it also covers the line.

**What to look at.** `screenshots/ui3d04_02_after_move.png` and
`ui3d10_01_committed.png`: the green ring is where the number will be once the
offset is gone.

**The question.** Does the number belong **on** the anchor, or does this product
want a small authored offset — and if so, in which direction relative to the
camera? Note that any answer becomes an authored offset policy the audit's 4 dp
tolerance would then be measured against, so it needs stating once rather than
per surface.

---

## OL-2 — Should the dimension leaders emphasise the active axis, or is a highlight the wrong instrument?

**Why it is a question.** UI3D-F-005 means the active-axis leader is currently
invisible rather than emphasised. The four-line renderer repair restores it —
but restores it as the **highlight colour at full weight**, which is a level
nothing else in the dimension overlay uses, and which was never seen because it
never drew.

**What to look at.** `screenshots/ui3d05_05_after_zoom_out.png` shows the three
leaders as they read today with no axis active — all `Entities`, all one
weight. The repair changes only what happens when a user taps one number.

**The question.** Should the axis being edited be drawn in the highlight, or
should all three stay one weight and the emphasis live in the chrome chip
alone? The same question decides whether the sketch line annotation (which is
`Dimension` in its entirety, so today it is wholly absent) should come back at
the highlight level or at the neutral one.

---

## OL-3 — Is a per-frame refresh of the world-anchored chrome acceptable, or should it be event-driven?

**Why it is a question.** One fix closes UI3D-F-002, F-003, F-004 and F-007: a
refresh driver for world-anchored chrome that runs when the frame does. But the
product has a hard rule that the Android layer holds no domain truth and does
not poll, and `PROJECT_STATUS.md` records that opening and closing panels
produces **zero** publications — a per-frame Android read is a new kind of cost
in a shell that currently has none.

**The two shapes, both correct:**

- a **per-frame** reprojection of exactly the anchored surfaces that are on
  screen (bounded: at most five views, each one JNI read and one translation);
- an **event** from native when the anchor set changes — camera moved, overlay
  rebuilt, session closed — which keeps the shell reactive but adds a callback
  direction across the JNI boundary the product does not currently have.

**The question.** Which cost does this product want to carry? This is an
architecture decision, not a UI one, and it is the coordinator's rather than a
bounded correction stage's.

---

**Nothing in this pack is a defect report, and none of it blocks the findings in
`FINDINGS.md` from being grouped and scheduled.**
