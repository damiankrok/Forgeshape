# CAD-R2-C1 — PERF PROTOCOL

## Trees, toolchain, machine

| Item | Value |
| --- | --- |
| A (baseline) | `636797e518898c31c4cd691d453db2833ef00896`, isolated worktree `/tmp/claude-0/baseline` (clean) |
| B (R2) | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb`, isolated worktree `/tmp/claude-0/wt-r2` (clean); its `app/src/main` is R2's last product tree |
| Compiler | `g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`, same binary for both |
| Flags | exactly `scripts/host-native-selftests.sh`'s: `-std=c++17 -O1 -g0 -DMANIFOLD_PAR=-1 -DMANIFOLD_NO_IOSTREAM -DMANIFOLD_NO_FILESYSTEM` (kernel `-O2 -w`); product objects `-Wall -Wextra -Wno-unused-parameter` |
| CPU | `Intel(R) Xeon(R) Processor @ 2.80GHz`, family 6 model 85 (Skylake-SP / Cascade Lake class), 4 logical cores; governor not exposed in the container; no thermal sensor exposed |
| Load | `/proc/loadavg` 1-min value recorded before every run (`PERF_RAW_SUITE.tsv` column `load1`; harness runs in `perf/…/*.load`); mean load equal on both sides (1.28 / 1.28 in the suite run) |
| Release | none of this ships: the harness lives under `artifacts/`, links host objects only, and the release guard is unchanged |

Binary hashes (SHA-256):

| Binary | SHA-256 |
| --- | --- |
| A `host-selftests` | `a3122ba530ceba321c45a61f01d9bdb2871e8307eca10f8b3cebc61095f87eba` |
| B `host-selftests` | `4e15b529cb8c689a57cab95f505f177c8fec1be806a43fd524442e8bb2f72ee8` |
| A `bench_c1` (all objects) | `a413f8186adb7d26a2f9dc3026a6e64a0e89d67d50ebae00ab4d13010c1d8555` |
| B `bench_c1` (all objects) | `96f1a9af490fceefd1bbfaf07fa37ee3d0fd58751b62f37ab0250bf7b9ebd411` |
| B `bench_c1_judge` | `d7b07705eedad34e6d463e7ebfdc63d89abdbdce0e92d0ede829a5001636995e` |
| ablation `bench_c1` | `0dcc920618d13221dff5ab9fe5ae2ea2504a70db257f23eea15bfef5a39128aa` |
| ablation `bench_prod` | `a9699151d4a9a839aef9727ad17fd4ae2555247231e9cf20e64d5bbef4da45c9` |
| A with JCC mitigation | `af9087a8cdfb239f16f6d8a8f9bc42483c4ae81b23efd073781ce08551725d66` |
| B with JCC mitigation | `d3c581f68245aa79c3334b6b52396dc1bab1cdd0d61cba984428c2844e180647` |

## Metric definitions (identical in A and B)

The three product metrics are defined by self-test code that is byte-for-byte
the same in both trees (`forgeshape_cad_multiface_selftest.cpp`,
`forgeshape_sketch_drafting_selftest.cpp`, `forgeshape_cad_revolve_selftest.cpp`
unchanged by R2). Same input fixtures, loop counts and units (µs, steady_clock).
`eval_all` and `trim100` are ONE sample per suite run; `revolve_square_max` is
the MAX of 20. **Comparable as definitions**, but the suite binaries are not
the same program (B links R2's ~65 extra self-test checks and three new
objects), which is why the isolated harness below was added.

## Experiments

1. **Suite level** (`perf/run_suite_abba.sh`): `host-selftests CAD_FEATURE`,
   one untimed warm-up each, then 12 ABBA/BAAB blocks → **24 runs per
   binary**. Raw: `PERF_RAW_SUITE.tsv`.
2. **Isolated harness** (`perf/bench_c1.cpp`, `perf/run_bench_abba.sh`): one
   source compiled unchanged against each tree's own objects. Per process: the
   63 × 63 grid finished once, every face selected, then 5 warm-up and 30
   recorded iterations of { `evaluateCandidate` (forced re-evaluation by
   toggling face 0 off and on), `candidateState()`, `regenerateCadBody`,
   `planSketchTrim`, `regenerateCadBody(revolved square)` }. Processes
   alternated ABBA; 6 blocks (360 samples per side) for the main run, 4 blocks
   (240 per side) for each ablation. Raw: `PERF_RAW.tsv` (4140 rows).
3. **Cause isolation** (all scratch, nothing committed):
   - `bench_prod`: the same harness linked against PRODUCT objects only (no
     `*_selftest.o`) — changes only the link layout;
   - `bench_rev`: product objects in reverse link order;
   - same-tree controls: A vs A and B vs B with only the link layout changed;
   - one-factor ablation: B with `validateCadStateBeyondRegeneration` removed
     from `SketchSession::evaluateCandidate` (scratch copy `/tmp/claude-0/ablate`);
   - direct cost of the judge (`bench_c1_judge`, 60 samples);
   - JCC-erratum check: both trees' objects rebuilt with
     `-Wa,-mbranches-within-32B-boundaries`, original full link, ABBA.
4. Object identity: `.text` of each object compared between trees
   (`objcopy --only-section=.text`).

Statistics (`perf/analyse.py`): n, median, p10..p90, IQR, absolute and relative
median delta, Mann-Whitney U two-sided p (normal approximation).

## Reproduce

```
HOST_SELFTEST_OUT=<dir>/base bash scripts/host-native-selftests.sh CAD_FEATURE   # in each worktree
bash artifacts/cad-r2-c1-quality-gate/perf/run_suite_abba.sh <A> <B> 12 out.tsv
g++ <flags> -c artifacts/cad-r2-c1-quality-gate/perf/bench_c1.cpp && g++ -o bench_c1 bench.o <tree>/obj/*.o libmanifold.a
bash artifacts/cad-r2-c1-quality-gate/perf/run_bench_abba.sh <A> <B> 6 30 5 outdir
python3 artifacts/cad-r2-c1-quality-gate/perf/analyse.py bench A_*.tsv -- B_*.tsv eval_all_us trim100_us revolve_square_us
```
