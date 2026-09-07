# Stage 025 (`SCULPT-FCM-R1`) — OWNER LATER test pack

Flatten, Crease and the Sculpt Mask are closed on the technical side. What no
emulator settles is below. **No aesthetic approval is claimed for anything in
this list**, including the three new Tool Rail glyphs, the mask overlay's colour
and strength, and Clear Mask's placement.

Append these to the existing combined OWNER pack rather than reviewing them in
isolation — they sit beside the still-pending SCULPT-H1, MIRROR-01, Stage 020M,
Stage 018A, UI-PREF-R1 and SEL-OUT-R1 items.

## Flatten

1. **Does it plane?** Work a Flatten stroke over a curved area of a sphere at a
   few Radius/Strength combinations. Does it read as *pressing the surface onto
   a flat plate*, or as a generic smoothing? Smooth and Flatten must not feel
   like the same tool with a different name.
2. **Does it stop where it should?** Keep going over the same place. The
   arithmetic says the surface converges on the plane and never crosses it —
   does that also look right, or does the facet develop a rim or a step at the
   brush edge?
3. **Is the plane the one you expected?** The plane is fitted once, from the
   surface under the brush at the moment the finger lands. Land on a saddle, a
   ridge and a corner in turn. Where the fit is ambiguous the tool does nothing
   at all. Is silence the right answer there, or should it say something?
4. **Facets on real shapes.** Try to put a flat spot on a sphere, a flat on a
   cylinder's side, and a bevel on a box's corner. Is Flatten the tool you would
   reach for, and does the result survive a couple of Smooth passes without
   melting?

## Crease

5. **Width and depth.** Cut a groove at four or five Radius/Strength
   combinations. Is there a usable range, or is it either invisible or a gash?
   The inward/pinch split is 0.75/0.45 — does the channel read as *narrower than
   the brush*, which is what that ratio is for?
6. **Repeated passes.** Go over the same line several times. Does the groove
   deepen along one channel, or does it wander?
7. **On a low-density mesh.** Try it on a Plane and on a box, where there are
   very few vertices under the brush. Is the result honest (little or nothing)
   or does it produce visible faceting artefacts?
8. **Against Clay.** Clay outward and Crease inward with the same brush and the
   same stroke. Do they read as opposites of one family, or as unrelated tools?

## The Mask

9. **Is the overlay readable but not opaque?** The masked area is mixed 72 %
   toward a cool low value. Can you still read the FORM through it, and can you
   still tell where the mask ends? Check the **shadow side** of a sphere
   particularly — a multiply would have vanished there, which is why it is a mix.
10. **All five palettes.** Warm Graphite, Neutral Charcoal, Light Charcoal, Warm
    Light and Cool Light. The mix is relative to the object's own shading rather
    than to the ground, so it should read the same in all five. Does it? Is any
    palette noticeably worse?
11. **Both shading models.** Studio Solid and MatCap. The mask goes on after
    shading, so it should look the same relative amount in both.
12. **Selection over a mask.** Select a masked body. The acknowledgement pulse
    is drawn AFTER the mask, so it should still be obvious. Is it?
13. **Does painting feel right?** Painting is travel-driven like Clay: a short
    pass paints a little and working over an area protects it fully. Is that the
    right feel, or should one pass protect more? (`kMaskGain` is 1.0 and one
    brush-radius of travel at full Strength fully masks the centre.)
14. **Working against a mask.** Mask a finished area, then sculpt right up to
    its edge. Does the partial falloff at the mask's own edge give you a usable
    soft boundary, or does it need to be sharper?

## Clear Mask

15. **Is it discoverable?** It lives in the Sculpt Property Inspector, above the
    stale-source block and Reset Sculpt, and is **absent** unless a mask exists.
    Is the inspector the right home, or does it belong beside the brush controls
    or on the Tool Rail?
16. **Is the caption useful?** It reads *"Mask: N vertices protected from the
    other brushes."* Is a vertex count the right thing to say to a user, or
    should it say nothing at all?
17. **No confirmation.** It asks nothing, because one Undo puts the whole mask
    back. Is that right, or does losing a carefully painted mask deserve a
    prompt?

## The seven-tool rail

18. **Does the rail still work at seven entries?** Check portrait and landscape,
    compact and expanded. Seven 48 dp targets is the same count the sketch rail
    already carries; does the sculpt rail scroll, and if so is the scroll
    comfortable and are all seven reachable one-handed?
19. **Is the ORDER right?** The rail reads Grab, Clay, Smooth, Flatten, Inflate,
    Crease, Mask — the two that add material either side of the ones that take
    away or even out, with Mask last because it is the one entry that is not a
    deformation. Is that the order you would want?
20. **Are the three glyphs legible at 24 dp?** Flatten is a bumpy profile
    pressed onto a plane, Crease is a V driven into a surface, Mask is a shield.
    Is Mask's shield right, or does it read as "security"? Does Flatten read as
    distinct from Smooth?

## Across the product

21. **Construction Sculpt and Imported Mesh Sculpt.** Every new tool and the
    mask on both. Is there any difference in feel that should not be there?
22. **Non-uniform Scale.** Stretch a body 3:1 and use all three new tools. The
    footprint should stay round on screen and the mask should darken a round
    patch. Does it?
23. **Handedness.** Left and Right. The rail moves and the mask overlay does not
    care; is anything about the seven-entry rail worse on the left?
24. **Undo/Redo and the History navigator over a MIXED sequence.** Alternate
    geometry strokes and mask acts, then walk back and forward with Undo/Redo
    and with the navigator. Do the mask rows read sensibly in a list whose rows
    are labelled `Stroke N`? A mask act is a "stroke" by the navigator's
    wording, which may or may not be right.
25. **A long session.** Twenty or thirty mixed acts. Does anything about the
    mask make eviction confusing — for instance, undoing past an evicted mask
    paint?

## Not in this stage, and deliberately

Mask Invert, Grow, Shrink and Blur; mask by topology; mask persistence across a
reopen; vertex-colour or texture painting; symmetry; stylus pressure; the
real-stylus hover preview (still deferred on the autosave-suspend blocker from
`SCULPT-H1`); isolate/hide; subdivide, remesh and dynamic topology.
