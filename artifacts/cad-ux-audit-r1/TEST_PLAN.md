# TEST_PLAN — what the future stages must prove

**Task:** `CAD-UX-AUDIT-R1` · **Baseline:** `42314cbfece5a27f163045f090394603ab80246b`
Plan only. **No test was written, changed or run for this audit.**

Where a suite already exists it is named, because the repo's convention is to
widen an existing suite rather than open a parallel one — the way
`SCULPT-FCM-R1` widened the sculpt suite from 533 to 655 checks.

Existing homes: `forgeshape_cad_selftest.cpp`, `forgeshape_cad_a3_selftest.cpp`,
`forgeshape_sketch_ux_selftest.cpp`, `forgeshape_project_selftest.cpp`,
`forgeshape_history_selftest.cpp`; on the device, `SpatialSketchTest`,
`EditorWorkspaceControlsTest`. Native suites run from `NativeViewport.start()`
once, debug-only, each building its own scene, history and camera.

---

## 1. Domain geometry and feature lineage

| Id | Claim to prove | Where |
| --- | --- | --- |
| T-D1 | A committed CAD Body's `sketch()` is bit-identical to the sketch that was drawn — Extrude **consumes nothing** | CAD self-test |
| T-D2 | `ExtrudeFeature::profileEntityId` still names the same profile after unrelated entities are added and deleted (ids are never reused) | CAD self-test |
| T-D3 | Symmetric places the caps at `-d/2` and `+d/2`; TwoSides at `-B` and `+A`; both meshes are watertight, every edge on exactly two triangles, canonically wound, on **all three** workplanes | CAD self-test, parameterized over planes as the existing suite already is |
| T-D4 | `cadTopologySignature` is **unchanged** across a OneSide → Symmetric → TwoSides edit — a dependent stays attached | CAD-A3 self-test |
| T-D5 | Face **frames** move with the extent (the caps moved) while **tokens** do not, and a dependent's derived world model follows | CAD-A3 self-test |
| T-D6 | Zero, negative, non-finite and over-range distance A and B, and an unknown extent code, are each refused **by name**, changing nothing | CAD self-test |
| T-D7 | A refused extent edit leaves the previous valid state exactly — no half-regenerated body | CAD self-test |
| T-D8 *(Stage024)* | An Add/Cut feature list evaluates in order; reordering changes the result; the same list always yields the same triangles (determinism, because nothing is stored) | CAD self-test |
| T-D9 *(Stage024)* | A Cut that removes a face a dependent sketch stands on is refused `DependentFaceLost`; nothing is retargeted or cascaded | CAD-A3 self-test |

## 2. Codec backward compatibility

| Id | Claim | Where |
| --- | --- | --- |
| T-C1 | Every one of the **30 existing fixtures** decodes byte-identically after the v4 work — the full corpus hash list unchanged | project self-test, `FORGESHAPE_PROJECT_GOLDEN_SHA256` |
| T-C2 | A OneSide-only project still writes **v1** (or v2/v3 as applicable) and is byte-identical to the pre-stage encoding | project self-test |
| T-C3 | v4 is written **only** when some body is Symmetric or TwoSides, and always carries the v2 and v3 blocks (superset rule) | project self-test + spec |
| T-C4 | `scripts/build-forge-corpus.ps1` reproduces every new fixture byte-for-byte from `DATA_PACKAGE_SPEC.md` alone | corpus parity run |
| T-C5 | The two constructed-corrupt v4 fixtures (`distanceB = 0` on TwoSides; `extentCode = 9`) are refused with the right named status and change nothing | project self-test |
| T-C6 | A v4 `CADB` presented to a reader that knows only v3 is refused as a **required section at an unknown version** — the whole file, not a degraded body | project self-test |
| T-C7 | Round trip: encode → decode → encode is byte-identical for a Symmetric and a TwoSides project | project self-test |
| T-C8 *(Stage024)* | A one-feature list encodes **byte-identically to v3**; v5 appears only with a second feature | project self-test |

## 3. ProjectHistory one-step semantics

| Id | Claim | Where |
| --- | --- | --- |
| T-H1 | One canvas length drag (many pointer samples) is **exactly one** history step | history self-test |
| T-H2 | A drag that returns to its starting value records **nothing** and leaves the redo stack untouched | history self-test |
| T-H3 | A second pointer or a Cancel mid-drag restores the pre-drag state and records nothing | history self-test |
| T-H4 | Undo of an extent edit restores the previous extent, direction **and** both distances; Redo restores the edited ones | history self-test |
| T-H5 | A typed in-canvas length is one step, and is identical in every stored byte to the same value typed in the precision panel | history + project self-test |
| T-H6 | Nothing in the tool state — operation, preview, anchors, screen scale — appears in any step, checkpoint or fingerprint | project self-test, byte comparison before/after activating the tool |
| T-H7 *(Stage024)* | One Add is one Undo; Undo restores the target's previous feature list and mints no id; Redo restores the same result | history self-test |

## 4. Same-body vs New Body identity

| Id | Claim | Where |
| --- | --- | --- |
| T-I1 | New Body mints a fresh `ObjectId`; a refused commit mints none (the existing `addCadBody` rule) | scene/CAD self-test |
| T-I2 *(Stage024)* | Add/Cut mints **no** `ObjectId`, adds **no** row, and `bodyCount()` is unchanged across the act | scene self-test |
| T-I3 *(Stage024)* | After an Add, the `.forge` document contains exactly **one** `CADB` record for the target — no orphan record, no second body | project self-test |
| T-I4 | A face-supported sketch still creates a **second** body and is never reported as Add (guards against the workaround the contract forbids) | CAD-A3 self-test |
| T-I5 *(Stage024)* | Target selection refuses by name: a Construction Body, an Imported Mesh, a body with a Frozen Sculpt Mesh, a locked body, a hidden body, a face-supported body's producer where the graph would cycle | CAD self-test |
| T-I6 | Before the kernel exists, `setOperation(Add)` and `setOperation(Cut)` refuse by name and the control is absent | CAD self-test + device |

## 5. One-side / symmetric / two-side exact distances

| Id | Claim | Where |
| --- | --- | --- |
| T-E1 | A typed distance is stored **exactly** and never snapped to the grid — the existing `applyLineLength` promise, applied to depth | CAD self-test |
| T-E2 | Flip changes `directionCode` only; the mesh is the mirror through the sketch plane and the vertex **count** is unchanged | CAD self-test |
| T-E3 | Symmetric with distance `d` and TwoSides with `A = B = d/2` produce **identical** meshes | CAD self-test |
| T-E4 | The canvas drag and the typed field reach exactly the same stored state for the same value | sketch-UX self-test |

## 6. World anchor and camera stability

| Id | Claim | Where |
| --- | --- | --- |
| T-A1 | Anchors are derived from the sketch frame and the intent alone — **no camera, no viewport, no zoom** enters. Assert the same anchors under two different cameras. | new tool self-test |
| T-A2 | The screen scale is clamped: at extreme near and far the projected cluster stays within `[S_min, S_max]` px, monotonic in between | tool self-test over a swept camera distance |
| T-A3 | Drawing and hit testing consume **one** scale value — a hit at the drawn edge succeeds and one just outside fails, at three distances | tool self-test |
| T-A4 | The drag basis frozen at pointer-down does not move while the preview grows: the same finger delta produces the same length delta at the start and the end of a long drag | tool self-test |
| T-A5 | A degenerate viewpoint (looking straight down the extrusion normal) holds the last good value — no NaN, no jump, and a named unresolvable status | tool self-test, mirroring `AxisSolveStatus` |
| T-A6 | The badge cluster anchor does not move during a drag and re-derives on pointer-up | tool self-test |
| T-A7 | Rotating the device and resuming leaves the anchors and the intent unchanged (native-owned, not view-owned) | device: `EditorWorkspaceControlsTest` |

## 7. Touch / stylus / mouse-neutral action model

| Id | Claim | Where |
| --- | --- | --- |
| T-P1 | The manipulator responds identically to a `Finger`, a `Stylus` and a `Mouse` `TouchPointer` with the same coordinates — tool type is carried, never consumed | tool self-test |
| T-P2 | A second pointer hands the gesture to the camera and mutates no intent | tool self-test |
| T-P3 | Every act is reachable by a **semantic id** (`ids.xml`), never a screen coordinate | device tests |
| T-P4 | A stylus hover highlights and never commits — the `supportChooserHover` rule, applied here | tool self-test (device hover is not claimed on the emulator) |
| T-P5 | No Android type reaches the tool: it compiles and passes in the standalone NDK runner with no JNI | standalone runner file list |

## 8. Viewport screenshots and OWNER acceptance

Captured on the isolated AVD via `scripts\start-forgeshape-emulator.ps1`, with
the log ring buffer at 64M, in **all five palettes** and both handedness
settings, portrait and landscape:

1. the extrude manipulator at near, mid and far zoom (the scaling question);
2. One Side, One Side flipped, Symmetric, Two Sides — each mid-drag and settled;
3. the numeric field open at the arrow tip, with the IME up, not covering the
   geometry it measures;
4. the operation badges over a light and a dark ground;
5. the manipulator against a body it would cut into (the occlusion question);
6. the retained sketch reached from a committed body;
7. the whole cluster with the Tool Rail and the trailing host visible, proving
   nothing covers a live control.

## 9. Test debt to clear first

`PROJECT_STATUS.md` records two items the coordinator must factor in before any
of the above is gated on an aggregate:

* the **`SpatialSketchTest` suite-isolation defect** is unfixed and *"will block
  the next `-FullSharded` run"* — and `SpatialSketchTest` is the closest existing
  device suite to this work;
* **no aggregate has been run since `SEL-OUT-R1`**, so an exhaustive gate needs a
  fresh full run on a stable tree rather than a resume.

Both belong to the next test-hardening batch, not to a CAD UX stage.
