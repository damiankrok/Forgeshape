# CAD-R2-C2 — SUMMARY (CPU picking seam)

Read `BEFORE.md`, `BEFORE_REPRO.md` and `DESIGN.md` first; `TESTS.md` holds
every gate and attempt, `PERF.md` the cost.

## Refs

| Ref | SHA |
| --- | --- |
| Base (exact, the C1 head) | `08406cfbbff471f6e53c576c24b45791f903301c` |
| Branch | `feature/cad-r2-c2-picking-seam-r1` |
| The one product change | `0fa12c9` — `app/src/main/cpp/forgeshape_picking.cpp` |
| `app/src/main` frozen at | `f3db953` (the fix + PICK_C2 self-tests) |
| Device union, CI FAST, APK | `TESTS.md` T4 / T5 |

`main` `6c9f156`, R2 `ea24d20` and C1 `08406cf` untouched. No merge, rebase,
cherry-pick, force push, FullSharded run, format or fixture change.

## Outcome

- **Reproduced red on the pinned baseline** (`BEFORE_REPRO.md`): the C1 pocket
  floor's centre, on the diagonal its two triangles share, from the product's
  orthographic eye 250 m out → `pickSceneSnapshot hit = 0`, the chooser answered
  `WorldPlane XY` (behind the floor). float32 u+v = 1.00000429 on triangle 34,
  where double over the same float inputs gives 0.999998093 (inside).
- **Root cause, earliest failing layer:** `intersectRayTriangle`'s float32
  containment arithmetic — cancellation in `s = origin − v0` grows linearly with
  the origin's distance (8e-8 at 2 m … 3.5e-6 at 250 m) past the fixed 1e-6
  overlap. Ray construction, scene arbitration and the chooser were correct.
- **Fix:** the same Möller–Trumbore, same thresholds, same culling, same float
  interface, evaluated in double. `kBarycentricEpsilon` untouched.
- **Green:** host `HOST_SELFTESTS_OK (4392 checks, 0 failed)`; 8 of the 17
  PICK_C2 checks red before, all 17 green after; C1_CH_01..06 and the 17 R2
  oracles kept.
- **Device:** D19 (the seam opens a sketch kept on the Cut's floor, `CapFar`, in
  the orthographic projection chosen through the Display control), D20 (outside
  the body / down a through hole → exactly the XY plane that is there; just
  inside the edge → a face), D21 (the aim survives an orbit and a rotation, the
  confirm opens the face sketch, Cancel writes and records nothing; perspective
  seam a face too), D22 (a simulated stylus hover resolves the face and selects
  nothing).
- **Cost:** +2.4 % to +3.0 % per triangle test (20,000-triangle mesh, 360
  samples per side, product-only link); the CAD scene pick unchanged.

## Behaviour invariants (evidence)

Face identity through `CadFaceRange` → `TopoRef` (PICK_C2_13, 14, 17, D19);
nearest hit (PICK_C2_10); backface culling (PICK_C2_07); silhouette not
widened (PICK_C2_06, D20 outside); holes empty (PICK_C2_15, D20 hole); an edge
between two semantic faces → one of exactly those two, deterministically
(PICK_C2_16); fail-closed input (PICK_C2_08); rigid transform (PICK_C2_09); C1
world-plane policy (C1_CH_01..06, D04, D15, D16); no retarget, no new state, no
JNI / renderer / kernel / format change.

## A2 — design hold (unchanged)

`BLOCKED-DESIGN-DECISION-R2-WORLD-PLANE-RETAINED-SKETCH` stands; option 0 (the
refusal) is the current behaviour, not a decision. Options 1–3 as in
`artifacts/cad-r2-c1-quality-gate/SUMMARY.md`. Nothing here touched it
(`R2_SK_06`, D18 still pass).

## Known issues / not tested

- Arm64 device performance and hardware stylus hover are not measured (emulator
  only; the hover in D22 is a simulated STYLUS tool type).
- Two DIFFERENT faces meeting at an edge: the winner is the nearer computed t,
  then the lower triangle index — deterministic for a build; across ABIs a ray
  exactly on such an edge may resolve to either face, both genuinely under the
  pixel.
- `pickSceneSnapshot` between BODIES still compares float t exactly (earlier
  body wins exact ties) — unchanged by design.

## OWNER physical test pack

1. Display → Orthographic. Block with a rectangular Cut pocket. New Sketch: tap
   the exact centre of the pocket floor (and of the top) twice — a sketch opens
   on that face, never on a plane behind.
2. The same after an orbit and after a rotation; Cancel; Undo shows no new step.
3. A through hole: tapping down it chooses the plane below, never the body.
4. Sculpt and Freeform: strokes and cage picks behave as before (same picking
   function).
5. On the phone: picking on a dense sculpt mesh feels unchanged.
