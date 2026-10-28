# CAD-R2-C1 — PERF ANALYSIS

All numbers in µs. Method, binaries and hashes: `PERF_PROTOCOL.md`. Raw:
`PERF_RAW_SUITE.tsv`, `PERF_RAW.tsv`.

## 1. Suite level reproduces the reported gap (24 runs per binary, ABBA)

| metric | median A | p10..p90 A | IQR A | median B | p10..p90 B | IQR B | Δ | Δ % | MWU p |
|---|---|---|---|---|---|---|---|---|---|
| eval_all_us | 35504 | 33878..62816 | 16438 | 47054 | 44646..57506 | 5416 | +11550 | +32.5% | 0.0133 |
| drafting_trim100_us | 10767 | 10454..13794 | 623 | 14884 | 14413..22867 | 1594 | +4118 | +38.2% | 3.2e-07 |
| revolve_square_max_us | 559 | 474..906 | 250 | 597 | 474..816 | 184 | +38 | +6.8% | 0.89 |
| parametric_chain16_regen_us | 11636 | 11129..16055 | 1090 | 11650 | 11186..14960 | 1196 | +14 | +0.1% | 0.95 |
| region_base_us | 134 | 127..175 | 11 | 130 | 124..228 | 8 | −5 | −3.7% | 0.085 |
| add_cut_us | 2987 | 2913..3546 | 190 | 2988 | 2905..4445 | 175 | +1 | 0.0% | 0.81 |

`eval_all` and `trim100` reproduce at about +33 % / +38 %; `revolve_square_max`
does not reproduce (p = 0.89).

## 2. The isolated harness shows the same gap — in code R2 did not change

Main run, 360 samples per side (12 processes × 30):

| metric | median A | median B | Δ % | MWU p |
|---|---|---|---|---|
| eval_all_us (`evaluateCandidate`) | 35036 | 46512 | +32.8% | <1e-15 |
| eval_state_us (`candidateState()`, R2-changed) | 284 | 282 | −0.8% | 0.23 |
| eval_regen_us (`regenerateCadBody` called directly, NOT through R2's path) | 34149 | 45801 | +34.1% | <1e-15 |
| trim100_us (`planSketchTrim`) | 12280 | 16709 | +36.1% | <1e-15 |
| revolve_square_us | 490 | 494 | +0.8% | 0.26 |

`.text` of `forgeshape_sketch_drafting.o`, `forgeshape_sketch_arrangement.o`,
`forgeshape_sketch_region.o`, `forgeshape_sketch_snap.o`, `forgeshape_cad_revolve.o`
and `forgeshape_cad_kernel.o` is **byte-identical** between A and B. The trim
path runs only that code and is still +36 % slower; the gap is concentrated in
work that derives the planar arrangement (trim, the 3969-face regeneration)
and absent where it is not (the revolve).

## 3. Cause isolation

| Experiment (240 samples per side) | eval_all Δ | trim100 Δ | revolve Δ | reading |
|---|---|---|---|---|
| A vs B, **product objects only** (`bench_prod`) | +1.3% (p 0.008) | +1.0% (p 0.11) | +1.4% | R2's product code at baseline speed |
| A vs B, product objects in **reverse link order** | +27.7% | +31.9% | +3.2% | the gap follows the link layout |
| **same tree** A: full link vs product-only | +2.4% | +3.7% | +1.8% | layout alone moves A by a few % |
| **same tree** B: full link vs product-only | **−25.1%** | **−26.3%** | −2.6% | layout alone moves B's IDENTICAL code by ~25 % |
| B vs B **without the R2 judge** (ablation), full link | −0.9% (p 0.058) | −1.1% (p 0.20) | −1.9% | removing the new validation buys nothing |
| B vs B without the judge, product-only | +0.3% (p 0.26) | +0.4% (p 0.17) | +0.8% | same |
| A vs B, both built with `-Wa,-mbranches-within-32B-boundaries` | **−1.1% (p 0.76)** | **−1.6% (p 0.63)** | +0.4% (p 0.36) | the gap vanishes when jumps are kept off 32-byte boundaries |

Direct cost of `validateCadStateBeyondRegeneration` on the 3969-face candidate
(60 samples): **median 1.1 µs, max 1.9 µs — 0.003 % of `eval_all`**.

**Conclusion.** The reported regression is real in the host self-test binary
and is NOT caused by R2's code. It is a code-placement effect: R2's extra
self-test objects shift where the unchanged arrangement code lands in the
host executable, and on this CPU (family 6 model 85, which carries the
Intel JCC erratum, SKX102) a hot jump that ends up crossing a 32-byte boundary
runs through the slow path. Three independent proofs: (1) the same R2 code is
25 % faster in a different link of itself; (2) linking product objects only
removes the A/B gap; (3) the assembler's 32-byte branch mitigation removes it
in the original full link. The one-factor ablation of R2's only new work in
the path changes nothing measurable.

## 4. Disposition

| Metric | Disposition | Evidence |
|---|---|---|
| `eval_all_us` | **NO-REPRO-STABLE** for the product code (suite-level gap = host-binary code placement, proven above) | §2, §3 |
| `drafting_trim100_us` | **NO-REPRO-STABLE** for the product code (byte-identical path; same placement cause) | §2, §3 |
| `revolve_square_max_us` (watch) | **NO-REPRO-STABLE** (not even at suite level, p = 0.89; harness +0.8 %, p = 0.26) | §1, §2 |

No product change was made for Track A. Nothing was removed for speed.

## 5. Remaining risks

- The arrangement hot path is **layout-sensitive by ~25-35 % on JCC-affected
  x86 hosts**. That is pre-existing (it moves with unrelated code) and is
  independent of R2, but it means single-shot host-suite timings of
  `eval_all` / `trim100` cannot be compared across commits that change the
  link. A product build flag (`-mbranches-within-32B-boundaries`) is a build
  change outside this stage and would not apply to the ARM64 device anyway.
- These are host measurements. The APK's native library is built by the NDK
  clang with different flags and runs on ARM64 or the x86_64 emulator; device
  timings were not measured here (emulator timings are not performance
  evidence) and remain an OWNER physical-device item.
