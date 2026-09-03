# Sketch entity contract

`forgeshape_sketch.h`. Each entity is an id plus a `std::variant` payload, so an
entity that IS a circle physically carries no rectangle width for anything to
misread (the same shape `PrimitiveSpec` has).

| Entity | Truth | Refused by name |
| --- | --- | --- |
| Line | `start (u,v)`, `end (u,v)` | `NonFinite`, `OutOfRange`, `ZeroLengthLine` (endpoints within 1 um) |
| Polyline | ordered vertices, `closed` flag | `TooFewVertices` (< 2; < 3 when closed), `DuplicateEdge` (consecutive coincident vertices, or a closed polyline repeating its first vertex), `TooManyEntities` (> 256 vertices), `NonFinite`, `OutOfRange` |
| Rectangle | `center (u,v)`, `width`, `height` (axis-aligned in sketch space) | `ZeroSizeRectangle` (a size that is not a usable Construction length: `validateDimensionMeters` != Ok), `NonFinite`, `OutOfRange` |
| Circle | `center (u,v)`, `radius` | `InvalidCircleRadius`, `NonFinite`, `OutOfRange` |

Sketch-level rules (`validateCadSketch`): the workplane is one of the three; no
id is zero, none repeats, every id is below `nextEntityId`; at most 256
entities.

Operations: `addSketchEntity` validates with a provisional id BEFORE minting, so
a refusal costs no id; `replaceSketchEntity` keeps the id and refuses invalid
geometry, changing nothing; `removeSketchEntity` never touches the allocator, so
a deleted id is never reused (`CADR0_11_remove_never_reuses_an_id`).

A rectangle is parametric on purpose: "make it 40 mm wide" is an edit to a
RECTANGLE, and four unrelated lines have no width to edit. Its four edges are
generated (`rectangleProfilePolygon`). Rotated rectangles are not R0.

Numeric editing (session and CAD Body): rectangle width/height, circle radius,
line endpoints, extrude depth. Typed values are exact - never snapped - and a
refused value leaves the entity as it was. A polyline's points are not
numerically editable in R0 and the panel says so.
