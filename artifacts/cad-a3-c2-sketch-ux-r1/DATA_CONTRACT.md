# `CADB` v3: the smallest correct evolution (`SKETCH-UX-R1` G)

## The decision

An Arc and a Spline are authored truth that no rule could recreate from
anything else in the file, so both must be stored. The question was where.

**Chosen: a new `CADB` section version, 3.** Not a new section, not a header
flag, and not a `.forge` major/minor bump.

| Option | Why not |
| --- | --- |
| A second section for curves | A curve is an entity of a sketch, not a parallel branch of a body. Splitting one sketch across two sections would let a reader open half of it. |
| A new header flag | Header flags announce whole BRANCHES a reader must be able to rebuild (`CONS`, `SCUL`, `IMPT`, `CADB`). A curve is inside a branch that already has one. |
| A `.forge` minor bump | Section versions evolve independently of the file's major/minor — that is what the version dispatch is for, and `CADB` v2 already used it. |
| A discriminator inside v1 | The version says what the payload may contain. A payload contradicting its own version is malformed, which is precisely the check v3 adds. |

## The layout

v3 is v2's record with two entity kinds added:

```
kindCode 5 — Arc:     f64 startU, startV, midU, midV, endU, endV     (53 bytes)
kindCode 6 — Spline:  u32 pointCount (2 .. 32)
                      f64 u, v  × pointCount                    (9 + 16n bytes)
```

Nothing else about the record changes.

## v3 always writes the v2 support block

Whether or not any body is face-supported. **A version is a superset of the one
below it**, and a reader that has to guess which optional blocks a version
carries is not reading a format. The cost is one zero byte per body in a
curve-carrying project with no face supports.

## When each version is written

```cpp
cadV3 = any body's sketch carries an Arc or a Spline
cadV2 = cadV3 || any body is face-supported
```

The section is written at the **lowest version that can carry it**, so:

- a project of lines, polylines, rectangles and circles on world planes → **v1**;
- the same plus a face support → **v2**;
- anything with a curve → **v3**.

**Every one of the twenty-two fixtures that predate this stage is byte-for-byte
unchanged.** A feature costs a project that does not use it exactly nothing —
the same property `IMPT`, the generalized `SCUL` and `CADB` v2 each established
in turn.

## Compatibility, both directions

| Situation | Answer |
| --- | --- |
| An older build meets v3 | `UnsupportedSectionVersion` — a required section at an unknown version. It refuses the file rather than opening a body with a curve missing, or (far worse) replaced by the straight edge between its two ends. |
| This build meets v1 or v2 | Read exactly as before. |
| A curve code inside a section declaring v1 or v2 | `InvalidSemanticValue`. The version says what the payload may contain; a payload contradicting its own version is malformed, not newer. `CADUXR1-37` patches a real v3 file's version byte down to 2 and asserts the refusal. |
| `pointCount` outside 2..32 | `ImpossibleCount`, refused **before a byte is allocated** for it. |
| An arc with collinear or coincident points | `InvalidSemanticValue`, through the domain's own `validateSketchEntity`. |
| A spline whose ends meet | `InvalidSemanticValue`, same route. |

## Validation is the domain's, not the codec's

Nothing about a curve is restated in the codec. `validateProjectDocument` calls
`validateCadBodyState`, which calls `validateCadSketch`, which calls
`validateSketchEntity` — the same functions the live editor runs. A file cannot
carry a curve the editor would have refused, and a change to a domain rule
cannot leave the codec behind.

A file whose sketch closes no profile is refused whether the sketch contains a
curve or not, exactly as before.

## The topology signature

Unchanged as an algorithm. What changed is an input: a curve's polygon edges are
**not eligible** side faces, and eligibility is already one of the values mixed
in. Since no fixture that predates this stage contains a curve, no existing
lineage token moves — which the twenty-two unchanged digests prove.
