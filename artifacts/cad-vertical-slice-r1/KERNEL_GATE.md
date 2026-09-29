# GATE-KERNEL: the boolean kernel for same-body Add and Cut

**Verdict: PASS.** Manifold 3.5.4 is vendored behind a ForgeShape-owned adapter.
It passed the capability corpus below before anything in `CadBody` used it.
`BLOCKED-CAD-VS-KERNEL` does not apply.

## 1. What had to be decided

Same-body Add and Cut need a robust boolean union and difference on closed
solids. The requirements:

- regenerated from the authored chain every time;
- deterministic;
- bounded;
- fail-closed;
- running on Android x86_64 and arm64-v8a today and on a future desktop build
  tomorrow, through one portable seam.

**What was not in the repo.** Nothing could do this:

- the renderer's meshes are float and derived;
- the sketch layer only triangulates simple polygons;
- the prohibited shortcuts were excluded outright: triangle soup over renderer
  indices, voxels as truth, overlapping or hidden bodies, baking to an Imported
  Mesh, a network service, and a platform-specific dynamic library.

**Why not our own.** A robust mesh boolean is a research-grade problem.
Coplanar faces, touching faces and exact degeneracies are where hand-written
booleans fail, and a hand-written one would be the least reviewed part of the
product.

## 2. Why Manifold

- **Guaranteed manifold output by design.** Manifold's documented goal is
  topologically robust booleans whose output is always a valid 2-manifold. That
  is exactly the property a watertight CAD body needs, and it is what the
  corpus below probes.
- **Plain portable C++17 static library.** It builds with the pinned NDK r29
  under our own CMake. There is no platform code, and threading is optional:
  we build it single-threaded.
- **Semantic face identity survives the boolean.** Every input triangle can
  carry a face ID, and Manifold carries it into the result
  (`MeshGL::faceID`). That is what lets an Add or Cut result still say which
  `(feature, face)` each triangle came from, with no triangle-index identity.
- **Permissive licence.** It is Apache-2.0.
- **Small subset.** The core library is 46 files once the optional Clipper2
  cross-section module, the bindings, the build scripts and the tests are left
  out.

**Alternatives considered:**

- **OpenCascade (OCCT).** A full B-rep kernel: an order of magnitude larger,
  LGPL with an exception, and far beyond this slice.
- **CGAL's Nef polyhedra.** GPL and heavyweight.
- **Carve and Cork.** Unmaintained, or with a known robustness history.
- **A hand-written BSP boolean.** Fragile on the coplanar cases this product
  hits constantly: a boss on a cap is flush by construction.

## 3. Acceptance evidence (prompt §9.2)

| Requirement | Evidence |
| --- | --- |
| Permissive licence, exact licence recorded | **Apache License 2.0**. The unmodified text is in `app/src/main/cpp/third_party/manifold/LICENSE`; contributors are in `AUTHORS`. Upstream ships no `NOTICE` file. |
| Licence text shipped with the object form (Apache-2.0 §4) | `app/src/main/assets/licenses/manifold-3.5.4-LICENSE.txt` is packaged in the APK. `vendor-manifold.sh --verify` `cmp`s it against upstream. |
| Exact version pinned | **3.5.4**, from the upstream source distribution on PyPI, `manifold3d-3.5.4.tar.gz`, SHA-256 `5bd88c482c6fd7fc01aa7b715b3e31c2585ee98343d5559d286ba30db0d0b6f0`. |
| Acquisition reproducible and documented | `scripts/vendor-manifold.sh` downloads that archive, checks the SHA-256 and copies the subset. `--verify` regenerated it in this session and printed `VENDOR_MANIFOLD_VERIFY_OK 3.5.4`: the committed tree is byte-identical to upstream. Provenance is in `third_party/manifold/FORGESHAPE_VENDOR.md`. |
| No runtime network access | Nothing is fetched at build or run time. ForgeShape still holds no network permission. |
| Builds x86_64 and arm64-v8a | `:app:assembleDebug` and `:app:assembleRelease` package `libforgeshape_native.so` for both ABIs, with NDK `29.0.14206865` (pinned, not bumped). |
| Portable API behind a ForgeShape-owned seam | `forgeshape_cad_kernel.h` has only ForgeShape types: `CadSolid`, `CadKernelStatus`, `CadBooleanOp`, `CadSolidMeasure`. `forgeshape_cad_kernel.cpp` is the ONLY file that includes a Manifold header. |
| Release build carries no test/debug kernel entry points | `MANIFOLD_DEBUG` is not defined, so the debug/throw paths are absent. The `CADVS_*` corpus lives in the debug-only self-test sources. `scripts/ci-release-selftest-guard.sh`, now also matching `CADVS_`, reads **0** `SelfTests` symbols and **0** self-test strings in both release ABIs. |
| Deterministic | Built single-threaded (`MANIFOLD_PAR=-1`). The adapter canonicalises the output: triangles grouped by ascending face tag, vertices renumbered in first-use order. `CADVS_K09` runs the same boolean twice and gets bit-identical positions, indices and tags. `CADVS_OPS_21` regenerates a chain twice with the same result. |
| Bounded failure: no crash, no NaN, no half-write | Nothing throws. Every status is a value. `cadKernelBoolean` writes nothing on refusal. Inputs are validated first: non-finite → `NonFinite`, open or non-manifold → `NotManifold`, inward-wound → `InvertedInput`. Results over `kMaxCadKernelTriangles` (262 144) are refused (`ResultTooLarge`). `CADVS_K08`, `K10`. |
| Source size | 46 vendored source files (headers + `.cpp`), **22 380 lines, 795 250 bytes**. 896 KiB on disk with `LICENSE` and `AUTHORS`. |
| Release APK size delta | See §5. |

## 4. The capability corpus (prompt §9.3)

These run in the `CAD_FEATURE` self-test suite, as checks 1–15. They passed
before the chain used the kernel, on the host (`bash
scripts/host-native-selftests.sh CAD_FEATURE`) and on the device at every debug
launch.

| Check | Case | Result |
| --- | --- | --- |
| `CADVS_K00` | a 2 × 2 × 1 box is valid | volume 4 |
| `CADVS_K01` | box ∪ an overlapping box | volume 4.75, one shell |
| `CADVS_K02` | box − a through prism | volume 3.75; the hole walls carry the tool's tags |
| `CADVS_K03` | blind cut, flush with the top | pocket floor present, mouth open (no cap left over the pocket) |
| `CADVS_K04` | union and cut with a tool that has a HOLE | exact volumes both ways |
| `CADVS_K05` | disjoint union | two shells: the chain maps this to `AddDisjoint` |
| `CADVS_K06` | disjoint cut | volume unchanged: the chain maps this to `CutNoIntersection` |
| `CADVS_K07` | touching union and touching cut | the union merges into one shell; the cut removes nothing. Named and deterministic. |
| `CADVS_K07b` | flush boss on a cap | one shell, no internal face |
| `CADVS_K08` | reversed-winding input | **refused** `InvertedInput`, output untouched. Manifold alone would have read it as a hole and silently subtracted it. |
| `CADVS_K09` | the same boolean twice | bit-identical canonical output |
| `CADVS_K10` | open, malformed and NaN inputs | refused by name |
| `CADVS_K11` | a cut that removes everything | an EMPTY result: the chain maps this to `CutRemovesBody` |
| `CADVS_K12` | region triangulation with a hole, both orientations | exact area |
| `CADVS_K13` | kernel identity | `manifold-3.5.4` |

**Mapping to the chain.** Each kernel outcome becomes a named product refusal,
proven again at the chain level:

- a disjoint Add → `AddDisjoint` (`CADVS_OPS_07`);
- no-effect Add → `AddNoEffect` (`_06`);
- a missing Cut → `CutNoIntersection` (`_08`);
- a total Cut → `CutRemovesBody` (`_09`).

## 5. Size cost (measured, release, unsigned)

| | Baseline `2380623` | Candidate | Delta |
| --- | ---: | ---: | ---: |
| `lib/arm64-v8a/libforgeshape_native.so` | 1 282 240 B | 2 010 088 B | **+727 848 B (+56.8 %)** |
| `lib/x86_64/libforgeshape_native.so` | 1 363 264 B | 2 215 824 B | **+852 560 B (+62.5 %)** |
| `app-release-unsigned.apk` | 2 904 907 B | 4 512 095 B | **+1 607 188 B (+55.3 %)** |

- **Method.** The baseline was built locally from `2380623` in a detached
  worktree. The candidate is the tested code (`db8ff6b`, identical to
  `759ed91`). Both used `:app:assembleRelease` on the
  same machine and toolchain.
- **What the delta includes.** Almost all of it is the kernel's core. It also
  includes the chain, region and HUD code and the 11 357-byte licence asset
  (`assets/licenses/manifold-3.5.4-LICENSE.txt`, in the APK only).
- **Why it was not trimmed.** It was not traded against speed in this slice:
  Manifold is built at `-O2`. Building it with `-Os`, or stripping unused
  Manifold entry points, is a recorded follow-up (`POST_AUDIT.md`), not done
  here.

## 6. What the kernel is NOT allowed to do

- **Never truth.** It never sees authored truth. It is handed DERIVED closed
  solids in body-local binary64 metres. Its output is never stored, serialized,
  fingerprinted or read back as authored data.
- **Never identity.** A kernel vertex index or triangle order is never identity.
  The face tag it carries is minted by ForgeShape and indexes ForgeShape's own
  face table.
- **Never a kernel-specific file.** A kernel change cannot change a `.forge`
  file: nothing kernel-specific is stored.
