# OWNER LATER — questions to ask once the CAD UX exists

**Task:** `CAD-UX-AUDIT-R1` · recorded now, asked later.

Nothing is implemented, so **no owner review is requested by this task** and no
screenshot is offered. These are the questions the implementing stages must come
back with, written down now so they are not re-derived later.

## Legibility

1. Are the operation badges readable without hiding the model — in all five
   palettes, over a light and a dark ground, and against a busy silhouette?
2. Does the manipulator read as part of the work, or as chrome floating over it?
3. Do the extrude annotations read as the **same drawing language** as the sketch
   dimension and the body dimension leaders, or as a third convention?

## Direction and extent

4. Is One Side / Flip / Symmetric / Two Sides intuitive **without** the panel —
   can the user tell which one is active at a glance?
5. Does Two Sides make it obvious which distance is A and which is B?
6. Is Flip in the right place, and does it read as "the other way" rather than
   "undo"?

## Direct manipulation

7. Is dragging length directly in 3D actually better than the bottom panel, or
   only different?
8. Does the numeric field at the arrow tip get in the way of the geometry it is
   measuring, with the IME up?
9. Does the manipulator ever fight the camera — does a drag that was meant to
   orbit extrude instead, or the reverse?

## Camera-distance scale

10. Is the scaling natural at very close and very far zoom, or does the clamp
    read as the control "sticking"?
11. Should the attenuation be the hard-clamped form or the soft exponent
    (`CANVAS_UI_ARCH.md` §4b vs §4c), and if the exponent — what value?
12. What are the final `S_min` / `S_max` and the world reference size? These are
    deliberately **not** chosen in the audit; the implementing stage brings
    candidates with screenshots at three distances.

## The retained sketch

13. Is the sketch easy to find and reopen from a committed body?
14. Should a body advertise that it has a sketch, and where?
15. Is "Edit Sketch" the right words, given the vocabulary rule that the user
    never reads "Freeze"?

## Add vs Cut

16. Is Add vs Cut obvious **visually, before confirming** — not only from a label?
17. Should the Cut preview be occluded by the material it cuts into, or stay on
    top like every other overlay? (`GAP_MATRIX.md` S7)
18. When Add/Cut cannot yet succeed, should the controls be absent or visibly
    unavailable? The repo's standing rule says absent.

## Platform mapping

19. Phone one-handed, tablet two-handed, and a desktop mouse: does the same
    cluster work, or does the desktop want a different placement?
20. What should a mouse wheel, a right-click and a modifier key mean here? None
    of the three has a neutral vocabulary in `forgeshape_input.h` today.
