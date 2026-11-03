# CAD-R2-C1 — BEFORE (post-implementation quality gate)

Written before any product edit, on branch `feature/cad-r2-c1-quality-gate-r1`
created from exactly `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` (the CAD-R2
head). Remote refs re-verified at start:

| Ref | SHA |
| --- | --- |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` |
| `origin/feature/modeling-r1-owner-shell-sketch-fix-r1` (R2 predecessor) | `636797e518898c31c4cd691d453db2833ef00896` |
| `origin/feature/cad-r2-retained-sketch-parametric-ux-r1` (R2) | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` (unchanged) |

Worktree clean at branch creation. R2's last native/Java product commit is
`5127cfc` (+ the Java constant in `5dd7d97`); everything after it is device
tests and docs, so the R2 product tree equals `ea24d20`'s `app/src/main`.

Coordinator verdict carried in: `TECH PASS (DELIVERED SCOPE) / A2-WORLD-PLANE
DESIGN BLOCK / PERF INVESTIGATION OPEN / OWNER R2 PHYSICAL REVIEW OPEN / NOT
MERGED`. Nothing here is an OWNER physical acceptance.

## Risk A — performance (open)

R2 reported, from 11 runs of the full host suite per side (one sample per
metric per run): `eval_all_us` +31.5 %, `drafting_trim100_us` +35.1 %,
`revolve_square_max_us` +27.4 % (watch).

What each metric is, before any measurement:

| Metric | Defined at | Work | Touched by R2? |
| --- | --- | --- | --- |
| `eval_all_us` | `forgeshape_cad_multiface_selftest.cpp:821-829` | ONE `SketchSession::evaluateCandidate()` over all 3969 faces of a 63 × 63 grid — a single sample | **yes**: `evaluateCandidate` (`forgeshape_sketch_session.cpp:2987`) now calls `candidateState()` (R2 branches for reuse / sketch-only first) and `validateCadStateBeyondRegeneration` (`forgeshape_sketch_session.cpp:3016`, `forgeshape_cad_feature.cpp:645`) |
| `drafting_trim100_us` | `forgeshape_sketch_drafting_selftest.cpp:1619-1622` | ONE `planSketchTrim` on a 100-line grid — a single sample | **no** source change in the path (`forgeshape_sketch_drafting*` untouched; `forgeshape_sketch.cpp` only gained two status-name cases) |
| `revolve_square_max_us` | `forgeshape_cad_revolve_selftest.cpp:888-899` | MAX of 20 `regenerateCadBody` calls on the canonical revolved square | **no** source change in the path |

Measurement weaknesses already visible: two of the three are single-shot
samples, the third is a maximum, and R2's binary also runs ~65 extra checks
in the same process (after these workloads — the multiface, revolve and
drafting sub-suites run at `forgeshape_cad_feature_selftest.cpp:5822-5847`,
before R2's insertion at 5855). The prior record was therefore not a
controlled comparison. Status: **OPEN — to be measured, not assumed**.

## Risk B — chooser tap on a curved wall (suspected defect)

`SupportChooser::resolve` (`forgeshape_support_chooser.cpp:146`) first takes
the NEAREST world-plane square hit by the ray, then casts the ordinary scene
pick. A face nearer than the plane replaces it (`:216`). But when the
frontmost scene hit is an INELIGIBLE face (a curved Cut wall, `:201`), or a
face that does not resolve, or a non-CAD body, the code `break`s and keeps
the world plane — **even when that plane hit is BEHIND the surface the user
touched**. The chooser's plane targets are line overlays drawn with the depth
test off (`forgeshape_renderer.cpp` gizmo pipeline, `createGizmoPipeline`),
so the plane's grid lines show through the body, but the plane's surface at
that pixel is hidden behind it.

R2 D04 (`CadRetainedSketchOwnerTest.java:226-249`) taps the round pocket's
curved wall twice and recorded `d04.curved_kind=2` (the YZ plane, x = 0,
behind the wall from the −x camera) and `d04.curved_opened=[1,2,0,0,0,1]` —
**a sketch opened on the YZ plane** after two taps on a body wall. The
assertion was only `kind != FACE`, which this passes. Second tap commits
because `forgeshape_jni.cpp:9709-9724` confirms on a re-selection of the
same target.

Expected classification after evidence: plane-behind-an-occluding-surface is
an invisible fall-through (fix at chooser level, no eligibility change, no
nearest-face retarget); a plane IN FRONT of a surface is intended visible
priority (keep).

## Risk C — font scale and lifecycle (not exercised)

R2 exercised the History surface (Sketches / Features segments, act strip,
detail text) at font scale 1.0 only, portrait and landscape (D12). Font
scale 1.3 / 1.5 never ran on a device. Lifecycle invariants (Cancel,
refusal, rotation, Save→Load) are covered by D02, D06, D08, D10 at 1.0.

## Scope table

| Track | Product change allowed only if | Out of scope |
| --- | --- | --- |
| A perf | a repeatable slowdown is tied to an R2 path by trace or ablation, and correctness is identical | removing validation for speed |
| B chooser | the fall-through is reproduced and the fix is chooser arbitration only | eligibility change, nearest-face retarget, renderer change |
| C font / lifecycle | a device run shows mislayout or a hit mismatch | new UI design |
| A2 world-plane retained sketch | never (OWNER design hold) | any implementation or workaround |
