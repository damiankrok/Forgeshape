# DATA_CONTRACT_IMPACT — `.forge`, TopoRef lineage and history

**Task:** `CAD-UX-AUDIT-R1` · **Baseline:** `42314cbfece5a27f163045f090394603ab80246b`
Audit only. **No codec, spec, fixture or corpus-script byte was changed.**

---

## 1. Current `CADB` state

| Version | Written when | Adds | Fixtures |
| --- | --- | --- | --- |
| v1 | any CAD body exists | objectId, workplaneCode, nextEntityId, **profileEntityId**, **directionCode**, **depth**, entities (Line/Polyline/Rectangle/Circle) | `cad_rectangle`, `cad_circle`, `mixed_cad`, `cad_bad_plane` |
| v2 | any body is face-supported | a support block after the workplane code: supportKind, producerObjectId, producerFeatureId, faceKind, faceEdgeEntityId, faceEdgeLocalIndex, **lineageToken** | 6, incl. 2 the decoder must refuse |
| v3 | any sketch carries an Arc or a Spline | entity kindCodes 5 and 6; **always writes the v2 block** | 6, incl. 2 the decoder must refuse |

Section: **required** whenever announced by header bit3 `hasCADB`
(`DATA_PACKAGE_SPEC.md:101, 433-441`). Total corpus: **30 fixtures**, every older
one byte-for-byte unchanged across every bump.

The whole extrusion, as stored today, is **three fields**:
`profileEntityId (u32)`, `directionCode (u8)`, `depth (f64)`.

There is **no** operation code, target body id, extent mode, second distance or
feature ordinal anywhere in the format.

## 2. The governing rules any change must obey

Read off `DATA_PACKAGE_SPEC.md` and `CLAUDE.md`, not invented here:

1. **A section version is a superset of the one below it.** v3 always writes the
   v2 block *"because a version is a superset of the one below it and a reader
   that has to guess which optional blocks a version carries is not reading a
   format"* (`DATA_PACKAGE_SPEC.md:582-584`).
2. **Write the lowest version that can express the project.** A project using
   none of the new semantics must stay byte-identical.
3. **A required section at an unknown version is refused**, whole file, rather
   than opened with a body silently degraded.
4. **The spec and `scripts/build-forge-corpus.ps1` are two implementations** of
   the same layout and their bytes must stay identical.
5. **Corrupt fixtures are constructed with the bad value in place**, never
   generated and then mutated.
6. **Nothing derived reaches the file.** No polygon, no triangulation, no
   vertex, no face frame, no render index.

## 3. Impact by requirement

### 3a. One Side / Flip — **zero schema impact**

`ExtrudeDirection` is `directionCode` 1/2 today and both values already round-
trip. An in-viewport Flip writes exactly what the chip writes. No version, no
field, no fixture.

### 3b. Symmetric and Two Sides A/B — **`CADB` v4, and it does not need the kernel**

`generateCadMesh` already spans two independent offsets
(`forgeshape_cad_body.cpp:108-111`), so the geometry is free. What must be
stored is which offsets.

Minimal shape (a proposal, not a decision):

```
v4 tail, replacing v1's  directionCode(u8) + depth(f64):
  u8   extentCode        1 OneSide, 2 Symmetric, 3 TwoSides
  u8   directionCode     1 along normal, 2 against   (OneSide only; else 1)
  f64  distanceA         strictly positive, a usable Construction length
  f64  distanceB         TwoSides only; strictly positive           <- v4 only
```

* **Byte cost:** +1 byte for every CAD body in a v4 file, +8 for a two-sided one.
* **When written:** only when some body is Symmetric or TwoSides. A OneSide
  project keeps v1/v2/v3 **byte-identical** — the existing 30 fixtures are
  untouched, which rule 2 requires and rule 4 must be shown to preserve.
* **New fixtures:** one Symmetric, one TwoSides, one mixed with a v2 support
  (to prove v4 ⊃ v2 ⊃ v1), plus **two the decoder must refuse** — a
  `distanceB` of zero on a TwoSides body, and an `extentCode` of 9 — constructed
  with the bad value in place per rule 5.
* **New `CadStatus`:** one value, e.g. `InvalidExtrudeExtent`, mirrored as a
  `CAD_*` int in `NativeViewport.java` and added to the spec's refusal table.
  `InvalidExtrudeDepth` already covers a bad distance.
* **Absent by design:** nothing about how the extrusion was *looked at* while it
  was authored. The view state rule (`SKETCH-UX-R1` C) already forbids that.

### 3c. Add / Cut on the same body — **`CADB` v5, and the model change is the hard half**

`CadBodyState` is *one* sketch and *one* extrusion, and the header says so on
purpose:

> *"The state is a struct rather than a list so nothing pretends a feature tree
> exists; when a second feature kind arrives it takes a new `CADB` section
> version rather than a discriminator inside this one."*
> — `forgeshape_cad_body.h:39-42`

So Add/Cut on the same body requires the body's truth to become an **ordered
feature list**, each entry carrying its own sketch (or a reference to a shared
one), its profile, its extent and its operation. That touches:

| Place | What changes |
| --- | --- |
| `forgeshape_cad_body.h/.cpp` | `CadBodyState` → base + `std::vector<CadFeature>`; `sameCadBodyState` compares the list; `validateCadBodyState` validates every feature *and* their composition; `generateCadMesh` evaluates the list in order |
| `forgeshape_history.h:86` | nothing, **if** the list stays bounded. It already copies `CadBodyState` whole. The bound must be explicit (a `kMaxCadFeatures`), or a step stops being bounded — which is the one property the snapshot design rests on. |
| `forgeshape_cad_face.{h,cpp}` | `CadFaceToken` currently identifies a face by (kind, sketch-entity, edge index) of **the** extrusion. Over a feature list a token must also name **which feature** — `TopoRef` already carries `producerLocalFeatureId`, fixed at `kCadFeatureId = 1` (`sketch.h:249`), and that field exists precisely for this. |
| `forgeshape_project_document.cpp` + `DATA_PACKAGE_SPEC.md` + `scripts/build-forge-corpus.ps1` | v5 record, refusal table, and a matching second implementation |
| `CadStatus` + `NativeViewport.java` mirror | operation/target refusals |

**A one-element feature list can be made byte-identical to v3** — write v3 when
the list has exactly one entry with operation NewBody. That is the migration
plan: no fixture moves, and v5 appears only for a project that actually has a
second feature. Recommended.

### 3d. Sketch reuse across features — **inside v5, not a new axis**

If a feature names a sketch by an index into a per-body sketch list, reuse costs
one `u32` per feature and needs no sketch identity outside the body. Sharing a
sketch **across** bodies would need a genuinely new global identity and is the
larger, later question; it is not required by the owner contract as written
("a downstream feature has a reference to Sketch/profile, not a copy of renderer
triangles" — a per-body reference satisfies this literally).

## 4. TopoRef and lineage implications

The lineage token is a **format field** with its rule restated in the spec so a
second implementation can produce it (`DATA_PACKAGE_SPEC.md:539-566`):

```
faces = [CapPlane, CapFar, Side(0..n-1)]
code  = (kind << 56) | (edgeEntityId << 16) | (edgeLocalIndex & 0xFFFF)
h     = FNV-1a 64 over profileEntityId, face count, then each code + eligibility
```

| Change | Effect on lineage |
| --- | --- |
| **Flip** | none — direction takes no part in the signature |
| **Symmetric / Two Sides** | **none to the token or the signature** — offsets take no part either. Dependents stay attached across an extent edit, which is the correct and desirable answer. |
| **Symmetric / Two Sides, semantic** | `CapPlane` is documented as *"the cap lying ON the producing sketch's support plane"* (`sketch.h:220`). With neither cap on the plane that sentence stops being true while the identity stays stable. **Decision needed:** rename to a neutral pair (near/far along the normal) and update the spec text, or keep the code and correct the prose. Either is a doc change; neither is a byte change. |
| **Add / Cut** | **Substantial.** A boolean result's faces are not the enumerable "two caps and n sides" of one extrusion. Either the token grows a feature ordinal (the `producerLocalFeatureId` slot exists and is reserved for exactly this) and faces are enumerated per feature, or CAD-A4 defines a durable naming scheme for boolean-derived faces. Face-supported dependents of a body that later gains a Cut **must fail closed** rather than retarget — that is the existing, non-negotiable rule. |
| **A cut that removes a supported face** | Must be refused by name on exactly the terms `DependentFaceLost` already refuses a sketch edit that strips a face (`sketch_session.cpp:255-268`). The mechanism exists; only the call site is new. |

## 5. ProjectHistory implications

* **The Construction history already stores the whole CAD truth.**
  `BodyConstructionState::cad` is a full `CadBodyState` (`history.h:80-86`), so
  every field added to that struct gets Undo/Redo *for free* — provided
  `sameCadBodyState` learns to compare it, or the no-op detection silently stops
  working and an edit that changed nothing would record a step.
* **One user act stays one transaction.** A canvas drag on an extrusion arrow is
  the gizmo's pattern exactly: open one `ScopedConstructionEdit` at pointer-down,
  write the authoritative state throughout the drag, commit once at pointer-up,
  `cancelEdit` on a second pointer or a cancel. `commitEdit` records nothing if
  nothing differs and leaves redo alone (`history.h:158-172`).
* **Pre-commit tool state is legitimately volatile.** The sketch session is the
  precedent: nothing before its commit is project truth. An active-extrude tool
  state (operation, extent, in-progress distances, preview validity) belongs
  there — session-scoped, native-owned, never serialized, never in a step, never
  in the fingerprint.
* **What must NOT be renderer-only temporary state:** the committed operation,
  extent, distances and target. Once committed they are authored truth and every
  one of them reaches a `.forge` byte and the semantic fingerprint. A preview
  mesh, a manipulator's world anchor, the arrow's screen scale and the badge
  positions are the opposite and must never be stored.
* **Fingerprint:** `projectSemanticFingerprint` hashes semantic values, so any
  new stored field moves it — correctly, since it is a real project change. A
  preview must not; the safest expression is that the preview never writes the
  body, exactly as the staged sketch edit never writes it before `commitEdit`.

## 6. Failure-closed behaviour required of any new field

Following the domain's existing conventions verbatim:

| Input | Answer |
| --- | --- |
| non-finite distance | `NonFinite`, refuse |
| zero or negative distance | `InvalidExtrudeDepth`, refuse — **never clamp** |
| distance beyond `kMaxSketchCoordinateMeters` | `InvalidExtrudeDepth`, refuse |
| unknown extent code | new named refusal — **never masked, never defaulted** |
| unknown operation code | new named refusal |
| target body that is not a CAD body | `NotCadBody` |
| target body that does not exist | `UnresolvedReference` at load; a named session refusal live |
| a boolean that produces no solid | a named refusal; **the last valid state stands** — `applyState` is all-or-nothing and there is no half-regenerated body (`cad_body.h:31-38`) |
| a reserved bit set | refuse, never mask (the `SCNE` v2 flags-byte precedent) |

## 7. Summary of schema impact by stage

| Stage (working name) | `.forge` impact | New fixtures | Older fixtures |
| --- | --- | --- | --- |
| `CAD-UX-S1` — canvas UI for what already exists | **none** | none | unchanged |
| `CAD-EXT-R1` — Symmetric / Two Sides A/B | **`CADB` v4** | 3 valid + 2 refused | unchanged, byte-for-byte |
| `Stage024` — feature list + Add/Cut | **`CADB` v5** | to be specified with the design | unchanged **if** a one-feature list encodes as v3 |
| `CAD-A4` — boolean-result topology | possibly a token widening inside v5 | to be specified | unchanged |
