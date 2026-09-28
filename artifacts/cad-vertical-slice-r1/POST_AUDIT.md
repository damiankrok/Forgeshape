# Post-implementation audit — CAD Vertical Slice R1

All evidence is from the cloud CI emulator: API 36 x86_64, SwiftShader,
1080 × 2400 px at density 2.625.

- **Frames:** `before/` (unfixed product, run `36329102110`) and `after/`
  (the final candidate's run; `TEST_EVIDENCE.md` names it).
- **Numbers:** the `after/facts-*.txt` file of that run.
- **What emulator evidence cannot close:** any physical-device gate
  (hardware GPU, touch feel, performance).

## 1. The twelve required evidence items

| # | Required | Where | Verdict |
| ---: | --- | --- | --- |
| 1 | Before/after at comparable camera and size | `before/01_ready_one_side` ↔ `after/04_compact_hud_one_side` (same 2 × 1 m One Side, same emulator) | shown |
| 2 | Measured overlay area | cluster 6.78 % → **2.70 %**; precision panel 28.79 % auto-opened → collapsed; navigator 5.71 % → absent; drawing rail 9.29 % → absent (the host is 3.02 % with three actions) | measured |
| 3 | Labels OFF | `after/04`, `05`, `06` | shown |
| 4 | Labels ON | `after/07_compact_hud_labels_on`, cluster 177 dp wide | shown |
| 5 | Rectangle-with-hole region selection | `after/01` (2 regions, none chosen), `after/02` (ring hatched, hole empty), `after/02b` (disk) | shown and measured (preview volume = ring area × depth) |
| 6 | New Body preview | `after/04`, `after/15` | shown |
| 7 | Add preview and same-body result | `after/08`, `after/09` (same id, volume 4 → 4.32) | shown and measured |
| 8 | Cut preview and committed void | `after/10` (the body tinted Cut with the through-pocket, the arrow into the body), `after/11` (volume 4 → 3.68, bounds unchanged) | shown and measured (finding A2 was the capture, and is fixed) |
| 9 | Feature edit after commit | `after/13` (Add reopened), `after/13b` (upstream depth carries the Add: 6.16 m³, top 1.75 m) | shown and measured |
| 10 | No hidden or extra bodies | body count unchanged across Add, Cut and the edit; the Objects capsule names the same body; `operation_refusal` compares project bytes | asserted |
| 11 | Save/reopen | `after/14`: same volume, same triangles, byte-identical re-encode | asserted |
| 12 | §16 audit | §4 below | written |

## 2. The questions the prompt asks

- **Does the workflow feel like one coherent sketch → feature operation?**
  Mostly yes.
  - Finish Sketch leaves the drawing tools behind and puts one row of controls
    on the arrow.
  - A face sketch offers Add and Cut, and they land in the same body.
  - The body's features are listed in its Shape panel and reopen in place.
  - Where it is not yet coherent:
    - the region choice and the operation choice are two separate places (the
      viewport and the badge palette), with no prompt ordering them;
    - after Add or Cut the tool returns to Shape, not to the next sketch.
- **Is the model visible while the controls are active?** Yes.
  - The HUD covers 2.70 % of the viewport, down from 6.78 %.
  - The trailing host is down to three actions.
  - The precision panel no longer opens over 28.8 % of the screen.
- **Can a first-time user infer One Side, Symmetric and Two Sides from the
  icons?** Probably, UNVERIFIED with people.
  - One outward arrow, two equal arrows, and two unequal arrows are distinct
    shapes.
  - Only the current mode is shown at rest; the others need a tap on the
    extent icon.
  - Descriptions carry the words.
- **Can a user tell Add, Cut and New Body apart without labels?** By shape and
  colour, yes: a separate box, a solid with a plus (green), a solid with a
  notch (red). Whether "notch" reads as Cut at 27 dp is an OWNER-LATER
  judgement.
- **Can a user intentionally choose a nested region with a hole?** Yes.
  - Nothing is chosen for them.
  - A tap in the ring selects the ring, and the hole stays empty in the
    hatch.
  - A tap on the disk switches, and a second tap deselects.
  - Measured by volume on the device.
- **Does same-body editing behave as retained CAD truth?** Yes.
  - Add and Cut keep the same `ObjectId`, and the Objects list does not grow.
  - Each is one Undo; Undo and Redo restore exactly.
  - The chain reopens from `.forge` byte-identically.
  - An upstream edit regenerates the Add on the moved face.
- **What are the next three highest-value missing fundamentals?** See §4:
  Through All, Revolve on the same model, and projected edges plus inference
  snapping.

## 3. Findings of this audit

| # | Finding | Status |
| --- | --- | --- |
| A1 | With several regions, Finish could not tilt the view; after the first tap the arrow pointed at the eye and could not be dragged (seen in `after/02`, straight-on) | **Fixed in `5fe9841`.** The first chosen region installs the tilt. `owner_rectangle_circle_region` asserts the arrow's screen extent. |
| A2 | The `e191dad` frame `10_cut_preview` showed an outward, New-Body-coloured prism and no Cut tint, although native held Cut and AgainstNormal (and `02b` still showed the ring after the disk was chosen) | **Evidence defect, not a product defect; fixed in the test.** The renderer's own log shows the Cut candidate uploaded into the target body at frame 10, 1.5 s before the screenshot. The CI emulator presents about 3 frames a second through a four-image FIFO swapchain, and the capture waited a fixed 400 ms, so it caught a frame recorded before the change. Captures now wait for six presented frames. See `TEST_EVIDENCE.md` §3. |
| A3 | A dragged distance is shown to full double precision ("1.2311801314353943 m") | Pre-existing (the old cluster formatted the same way). OWNER-LATER: display rounding of a dragged value |
| A4 | The Add preview tints the WHOLE target body, not just the added material | By design for R1 (one tint per draw item). OWNER-LATER: a per-triangle tool highlight |
| A5 | A Cut leaves its pocket faces ineligible as sketch supports | Deliberate. §4 row "faces carved by a Cut" |
| A6 | The kernel adds ~0.73–0.85 MB per ABI | Measured (`KERNEL_GATE.md` §5). Follow-up: `-Os` for the vendored kernel |

**OWNER-LATER** (aesthetics, not defects):
- the 27 dp glyph size;
- the Cut notch icon's legibility;
- the preview tint strengths (Add 0.30, Cut 0.34, New Body 0.22);
- the hatch spacing;
- Tool Labels caption size;
- where the region count is reported.

This audit does not call any of them perfect.

## 4. Professional-CAD fundamentals, audited (§16; not implemented)
The patterns referred to are Fusion 360, Inventor, FreeCAD Part Design and
Shapr3D, as the workflows each exposes. Nothing below was built in this
milestone. Classes: `ALREADY_IMPLEMENTED`, `NEXT_HIGH_VALUE`,
`DEPENDENCY_BLOCKED`, `LATER` and `OUT_OF_MVP`.

| # | Fundamental | Class | Why, and what it depends on |
| ---: | --- | --- | --- |
| 1 | Through All extent | **NEXT_HIGH_VALUE** | Every reference tool offers it for Cut. See the next-task list below. |
| 2 | To Object / Up To Face | LATER | Needs a face or vertex picker in Ready and a distance derived from the target's geometry at every regeneration. It also needs a rule for a target that disappears (fail closed, like a support). Up To Face over a boolean result needs the face table this slice added, so it is not blocked, but it is larger than Through All. |
| 3 | Revolve on the same region/operation model | **NEXT_HIGH_VALUE** | See the next-task list below. |
| 4 | Fillet | DEPENDENCY_BLOCKED | Manifold is a mesh kernel. A true constant-radius fillet on a sharp B-rep edge needs an exact edge model, or a fillet approximated in the sketch profile. A mesh-smoothing "fillet" would be a fake. It needs either a B-rep kernel gate (OCCT-class, a much larger decision) or a sketch-level fillet tool (2D arcs at profile corners, which the Arc entity could carry). |
| 5 | Chamfer | DEPENDENCY_BLOCKED | Same blocker as Fillet for edges of the SOLID. A 2D chamfer at a sketch corner is possible now and is part of the sketch-tools item under #13. |
| 6 | Shell | DEPENDENCY_BLOCKED | Needs an offset surface of an arbitrary solid, which Manifold does not provide exactly. |
| 7 | Dedicated Hole feature | LATER | A Hole is a positioned circular Cut with standard sizes (plus counterbore and countersink). The Cut and Through All pieces make it cheap once #1 exists. Its value is the standard-size catalogue and the placement UX. |
| 8 | Linear pattern | LATER | Manifold can union N transformed copies of a feature's tool cheaply. The chain needs a pattern feature referring to an earlier feature by id. The retained feature ids this slice added are the prerequisite. |
| 9 | Circular pattern | LATER | Same as #8, with an axis reference (for which a Revolve axis is the natural source). |
| 10 | Feature mirror | LATER | Unlike body Mirror (`MIRROR-01`), this mirrors a feature's tool across a plane inside one body. Same prerequisites as the patterns. |
| 11 | Draft angle | LATER | A tapered extrude is a lofted prism. The prism generator could produce it, but the side-face tokens and lineage rules for tapered faces need a design first. |
| 12 | Offset sketch / projected geometry | LATER | Projecting a support face's edges into the sketch as reference geometry is how every reference tool lets a user align a boss to an edge. It needs a reference-entity kind that is not authored truth. |
| 13 | Stronger sketch snapping and constraints | **NEXT_HIGH_VALUE**, snapping part only | See the next-task list below. A full constraint solver stays `LATER`. |
| 14 | Richer feature-history navigation | LATER | This slice delivers the minimum: an ordered list, reopen, one-step Undo. Suppress, roll-back bar, delete and reorder are real value, but each needs dependency rules first. For example, deleting a feature another feature stands on must be refused as `RefusedHasDependents` already is. |
| 15 | Measure / inspect | LATER | Read-only and cheap. Volume and bounds are already computed for the self-tests (`cadBodyMeasure`). The UX is the work. |
| — | Faces carved by a Cut as a sketch support | `DEPENDENCY_BLOCKED` by design | The inside of a pocket is ineligible today, because its token and frame are not yet defined for the reversed faces. |

`ALREADY_IMPLEMENTED` items this slice adds to the baseline:

- New Body, Add and Cut on the same body;
- regions with holes;
- retained, editable features;
- One Side, Symmetric and Two Sides extents on every feature.

### The next three tasks

1. **Through All, and Cut-by-default direction polish** (small, highest
   everyday value).
   - Add a fourth extent mode whose distance is DERIVED at regeneration from
     the target body's extent along the feature normal. It is stored as a mode
     with no distance, which needs a `CADB` v6 extent code.
   - The rules already exist: a Cut that misses, or that removes everything, is
     refused by name.
   - Depends on: nothing new. The kernel, chain and face tags are in place.
   - User value: the most common pocket and through-hole operation, with no
     number to guess.
2. **Revolve on the same region and operation model** (medium).
   - A second feature kind, `RevolveFeature {regions, axis reference, angle,
     operation}`.
   - It uses the same regions, the same New Body / Add / Cut rules, the same
     kernel union and difference, and the same face-tag table: a revolved side
     is a curved face and is ineligible as a support.
   - Depends on: an axis reference. A sketch line as the axis is enough for R1.
     It also needs a `CADB` feature-kind code, which the v5 tail was laid out
     to accept by version.
   - User value: turned parts such as shafts, knobs and bottles, which are
     unreachable today.
3. **Sketch precision: projected edges plus inference snapping** (medium).
   - Project the support face's boundary into a face sketch as non-authored
     reference geometry that the endpoint snap can use.
   - Add horizontal, vertical, parallel and midpoint inference snaps.
   - Depends on: a reference-entity kind excluded from regions and from
     `.forge` truth.
   - User value: bosses and pockets placed exactly relative to the body they
     change, which is the precision gap the owner will hit first after Add and
     Cut.

Fillet, chamfer and shell are not in the top three on purpose. Each needs a
second kernel gate (B-rep), and that decision should be made on its own
evidence rather than slipped in behind the vertical slice.
