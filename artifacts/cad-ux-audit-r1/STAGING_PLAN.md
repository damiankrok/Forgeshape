# STAGING_PLAN — the smallest safe order

**Task:** `CAD-UX-AUDIT-R1` · **Baseline:** `42314cbfece5a27f163045f090394603ab80246b`
Proposal for the coordinator. Working names only; **no stage is authorized here
and none was started.**

---

## 0. The sequencing insight

The owner's target splits cleanly along a line that is **not** where it first
appears to be:

* **Direction, in-canvas manipulation, exact in-canvas length, badges, camera
  scaling and retained-sketch access need no new geometry and no new bytes.**
  Everything they express is already storable (`ExtrudeDirection`) or already
  presentational.
* **Symmetric and Two Sides A/B need bytes but no boolean kernel.**
  `generateCadMesh` already spans two independent offsets
  (`forgeshape_cad_body.cpp:108-111`); only the storage of *which* offsets is
  missing. This is the largest single CAD capability available with zero kernel
  dependency, and sequencing it early also proves the v4 superset rule before
  the much larger v5 has to.
* **Add and Cut need two things, and the kernel is the smaller one.** The body's
  truth must become an ordered feature list (`CadBodyState` is a struct
  *deliberately*, `cad_body.h:39-42`), which touches the domain, the history's
  bound, the codec, the spec and the PowerShell corpus builder. The boolean
  itself touches `generateCadMesh`.

So: **do the canvas first, extents second, and let the kernel gate only what
genuinely needs it.**

---

## 1. Pre-kernel, no schema change — `CAD-UX-S1` (working name)

*"The canvas control for the extrusion that already exists."*

**Scope**

1. `forgeshape_cad_feature_tool.{h,cpp}` — the neutral active-feature tool state
   (`CANVAS_UI_ARCH.md` §1), holding profile, extent (OneSide only for now),
   direction, distance A, operation (NewBody only) and validity. Volatile,
   native-owned, session-scoped. `setOperation(Add|Cut)` and any extent but
   OneSide **refuse by name**.
2. World anchors derived in C++, and the screen-scale rule of
   `CANVAS_UI_ARCH.md` §4 as a small pure function beside the gizmo's, never
   inside it.
3. A third `SketchOverlayPtr` producer for the arrow and its ticks. **No
   renderer change.**
4. Manipulator hit test and one-pointer drag in C++, on the gizmo's contract:
   one captured pointer, basis frozen at pointer-down, second pointer cancels,
   degenerate viewpoint holds the last good value.
5. Badges and an editable numeric length as Android chrome positioned from a
   projected native anchor — the `bodyDimensionLabelPoint` pattern a third time.
6. **Flip in the viewport**, writing exactly what the existing chip writes.
7. **Retained-sketch access**: make it visible from the body that a sketch
   exists and reachable in fewer steps than "select → precision surface →
   scroll". Pure UI; no new truth.
8. Retire `draftDirection` from the two Java views into the tool.

**Explicitly out:** any operation but New Body, any extent but One Side, any
`.forge` byte, any fixture, any boolean, any preview that merges geometry.

**Why it is safe:** every value it writes is one the codec already stores and the
history already snapshots. If the whole stage were reverted, no project file
would differ.

**Risk to name in the stage prompt:** gesture arbitration. While a sketch is
open the single finger belongs to the sketch. The manipulator should own it only
in `Ready` (and later over a committed body), never in `Editing`.

## 2. Pre-kernel, one schema bump — `CAD-EXT-R1` (working name)

*"Symmetric and Two Sides, with Distance A and B."*

**Scope**

1. `ExtrudeFeature` grows `CadExtrudeExtent` and `distanceB`;
   `sameCadBodyState` compares them (or no-op detection silently breaks);
   `validateCadBodyState` refuses a bad extent and a bad B **by name, never
   clamped**.
2. `generateCadMesh` computes `nearOffset`/`farOffset` from the extent — a small
   change to two lines that already have the right shape.
3. `CADB` **v4**, written only when a body is Symmetric or TwoSides; every
   existing project stays v1/v2/v3 byte-identical.
4. `DATA_PACKAGE_SPEC.md` §7e and the matching `scripts/build-forge-corpus.ps1`
   implementation; 3 valid + 2 constructed-corrupt fixtures.
5. One new `CadStatus` value plus its `CAD_*` mirror in `NativeViewport.java`.
6. The extent control in the canvas cluster from `CAD-UX-S1`, and in the panels.
7. Decide and record the `CapPlane` naming consequence
   (`DATA_CONTRACT_IMPACT.md` §4) — a doc change, not a byte change.

**Why it is independent of the kernel:** a symmetric or two-sided extrusion of
one closed profile is still exactly one closed solid, produced by the same cap +
side generator. No boolean is involved at any point.

## 3. Requires GATE-KERNEL — the boolean itself

The only thing that genuinely needs a kernel decision is **evaluating a union or
a difference of two solids**. Everything the owner listed under Add/Cut that is
*not* that — the operation control, the target selection, the refusals, the
preview framing, the history shape — can be specified without it, and most of it
is already specified by this audit.

Gate content the coordinator should require before authorizing:

* which kernel (or an in-house mesh boolean) and under which licence — the repo
  bans third-party runtime, rendering, math and input libraries; a geometry
  kernel is a new category and needs its own explicit owner decision;
* robustness contract: what happens to coplanar faces, to a subtraction that
  removes everything, to a union of disjoint solids;
* determinism: the same input must produce the same triangles, because
  `.forge` stores no vertex and every mesh is regenerated on load — a
  non-deterministic kernel would make a reopened project a *different* project.

## 4. Requires Stage024 — the same logical body

*"Add and Cut modify the body, not the scene."*

**Scope**

1. `CadBodyState` → base + a **bounded** ordered `std::vector<CadFeature>`.
   The bound is not optional: it is what keeps a history step bounded, which the
   whole snapshot design rests on.
2. `CadFeature` = sketch (or a per-body sketch reference), profile, extent,
   direction, distances, operation. Evaluation in order through the one path.
3. `CADB` **v5**, with a one-feature list encoding **byte-identically to v3** so
   no fixture moves and no existing project changes.
4. Target-body semantics below JNI: an Add/Cut commit writes into the target's
   `CadBodyState` as **one** `ScopedConstructionEdit`, mints **no** `ObjectId`,
   and refuses by name for every ineligible target.
5. Sketch reuse across features inside a body (`DATA_CONTRACT_IMPACT.md` §3d).
6. Resolve the two existing refusals a merge would touch:
   `RefusedHasDependents` and `RefusedFaceSupportedCad` (`GAP_MATRIX.md` S2).

**Hard rule for this stage, restated:** no fake Add/Cut. No hidden `SceneObject`,
no renderer-side merge, no triangle-index identity, no "two bodies that look like
one". A face-supported dependent is **not** Add and must never be presented as
one (`GAP_MATRIX.md` S1).

## 5. Belongs to CAD-A4 — topology over a feature graph

* A durable face identity over a boolean result. `TopoRef` already reserves
  `producerLocalFeatureId` (fixed at `kCadFeatureId = 1` today, `sketch.h:249`)
  precisely so a face can name which feature produced it.
* The lineage-signature rule extended to a feature list, restated in
  `DATA_PACKAGE_SPEC.md` so the PowerShell builder can reimplement it — the
  existing FNV-1a rule is a **format field** and this constraint carries over.
* A cut that removes a face another body's sketch stands on: refused by name on
  exactly `DependentFaceLost`'s terms. The mechanism exists
  (`sketch_session.cpp:255-268`); only the call site is new.
* Whether a boolean-derived face may support a sketch at all, or only faces
  traceable to an authored profile edge.

## 6. UI/presentation only — must never become domain truth

These may ship in any of the stages above and must not create or move truth:

* badge glyphs, colours per palette, cluster layout, handedness mirroring;
* the arrow's proportions and the screen-scale constants;
* which control is drawn where, and the accessibility labels;
* the sketch's discoverability from a body.

Each is a presentation decision. None mints a revision, records a step, moves the
fingerprint or reaches a `.forge` byte.

## 7. Recommended order, and why not fewer or more stages

```
CAD-UX-S1   ->   CAD-EXT-R1   ->   GATE-KERNEL   ->   Stage024   ->   CAD-A4
(no bytes)       (CADB v4)         (a decision)       (CADB v5)       (topology)
```

**Why `CAD-UX-S1` and `CAD-EXT-R1` are two stages and not one:** the first
changes no persisted byte at all and can be reverted with zero project
consequence; the second bumps a required section version and adds five fixtures
to a corpus with a byte-identity promise. Merging them would put a schema bump
inside a UX stage, and the corpus-parity work would be judged alongside a
subjective canvas review. That is the only place this plan splits, and it splits
for a reason rather than to make more tasks.

**Why `CAD-UX-S1` is not split further:** the tool state, the anchors, the
overlay, the hit test and the chrome are one mechanism. Shipping the manipulator
without the numeric field, or the badges without the tool state that backs them,
produces a control that cannot succeed — which the repo forbids drawing.

**If only one stage can be authorized now:** `CAD-UX-S1`. It converts the
existing, already-correct domain into the canvas-first interaction the owner
asked for, it is the prerequisite surface every later stage plugs into, and it
risks nothing that is stored.
