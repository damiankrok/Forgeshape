# Vendored: Manifold 3.5.4 (the CAD boolean kernel)

- **Upstream:** https://github.com/elalish/manifold — "Manifold", a geometry
  library for topologically robust mesh booleans.
- **License:** Apache License 2.0 (`LICENSE` in this directory, unmodified).
  Contributors are listed in `AUTHORS`.
- **Version:** 3.5.4, from the upstream source distribution published to PyPI
  as `manifold3d-3.5.4.tar.gz`, SHA-256
  `5bd88c482c6fd7fc01aa7b715b3e31c2585ee98343d5559d286ba30db0d0b6f0`.
- **Reproduce / audit:** `scripts/vendor-manifold.sh` downloads that exact
  archive, checks the SHA-256 and copies the subset below;
  `scripts/vendor-manifold.sh --verify` proves this directory is byte-identical
  to it. Nothing fetches anything at build or run time.
- **Subset:** `include/manifold/*.h` (without the optional Clipper2-backed
  `cross_section.h`) and `src/*.{h,cpp}`. The bindings, the test suite, the
  build scripts and the optional cross-section module are not taken.
- **Distribution:** Apache-2.0 §4 asks that recipients of the object form get
  a copy of the License. The APK carries it as
  `assets/licenses/manifold-3.5.4-LICENSE.txt` (a byte copy of `LICENSE`
  here); upstream ships no `NOTICE` file.
- **Modifications:** none. ForgeShape's flags are set in
  `app/src/main/cpp/CMakeLists.txt`: single-threaded (`MANIFOLD_PAR=-1`, no
  TBB), no iostream or filesystem API, no `MANIFOLD_DEBUG` (so the library
  throws nothing), always optimised.
- **Seam:** nothing in ForgeShape includes a Manifold header except
  `forgeshape_cad_kernel.cpp`. The rest of the product sees only the
  ForgeShape-owned types in `forgeshape_cad_kernel.h`, so replacing the kernel
  is a change to one file. Why this kernel was chosen, and the evidence it had
  to pass first, is in `artifacts/cad-vertical-slice-r1/KERNEL_GATE.md`.
