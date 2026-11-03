# OWNER feedback — CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1

Physical review of the `CAD-V6-S2-OWNER-CORRECTION-E2E-R1` APK
(product `8ca3e6b`, APK SHA-256 `431e233b…d948b1`).

## Verbatim (Polish)

> Zaznaczanie fajnie działa, ale nie mogę zaznaczyć więcej obiektów niż teraz
> mam zaznaczone na screenie i Extrude wtedy nie działa, a jak objąłem parę to
> nadal coś nie działa Extrude. Jest błąd na górze: “Several profiles are
> closed — choose one in Sketch values.” Czy jest jakiś limit ilości wybranych
> pól?

## What it says

1. **Selection now feels good** ("Zaznaczanie fajnie działa").
2. **Further cells stop selecting on a larger sketch**: no more areas can be
   chosen than the ones already chosen on the screenshot.
3. **Extrude cannot be completed as expected** — not with the many cells
   chosen, and still not after choosing only a couple.
4. **The screenshot's status line reads**:
   `Several profiles are closed — choose one in Sketch values.`
5. The owner asks whether there is a limit on how many areas can be chosen.

## Screenshot

Supplied by the OWNER with this feedback to the coordinator; it is not
reproduced in this repository. It shows a mixed sketch (rectangle, crossing
circles, spline-derived cells) in Ready with a large fill selection and the
status line above.

## Answer, from source (see `BEFORE.md`)

Yes — two defects, independent of each other:

- **A 16-area limit.** A PlanarFaces selection inherited the loop-region cap
  (`kMaxPlanarFaceSelection = kMaxProfileRegions = 16`); the 17th tap is
  refused `TooManyRegions`.
- **Extrude refused for EVERY fill selection in a new CAD project.** The first
  project's commit (`commitFirstCadProject`) decided "is anything chosen" from
  the loop-region field `selectedProfileId()`, which a PlanarFaces selection
  never sets. With several closed loops in the sketch the refusal is named
  `AmbiguousProfile` — exactly the screenshot's message — even with one, two
  or sixteen faces chosen and the preview `Ok`.
