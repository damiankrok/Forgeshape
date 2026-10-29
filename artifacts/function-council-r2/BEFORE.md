# FUNCTION-COUNCIL-R2 — BEFORE (preflight, written before the audit run)

Work ID: `FUNCTION-COUNCIL-R2-FULL-CAPABILITY-UX-PLATFORM-AUDIT`. Type: research,
source audit and automated evidence. **No product, test, schema, fixture,
script, Gradle or workflow change is made by this audit.**

## Snapshot (verified by `git fetch` on 2026-10-29, not trusted from the prompt)

| Ref | Expected (prompt) | Actual `origin/…` | Match |
| --- | --- | --- | --- |
| `main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` | `6c9f156c1df91fe1a55445aed6186105a075ab31` | yes |
| `feature/cad-r2-retained-sketch-parametric-ux-r1` | `ea24d20805d9487d26221d9dd45e47dc16fb6ecb` | same | yes |
| `feature/cad-r2-c1-quality-gate-r1` | `08406cfbbff471f6e53c576c24b45791f903301c` | same | yes |
| `feature/cad-r2-c2-picking-seam-r1` | `70a6c14f703bfda2ab689b855126c1d12a541e41` | same | yes |
| `feature/cad-v6-sketch-face-r1` (integration line, not in prompt) | — | `bbae765645a318f83451e9a61b6a45cf2cb17e33` | recorded |

- Audited tree: **`70a6c14f703bfda2ab689b855126c1d12a541e41`** (committed
  2026-10-28 23:35:15 UTC), the C2 head. Its product tree (`app/src/main`) is
  byte-identical to `f3db953`; the last production C++ change is `0fa12c9`
  (`forgeshape_picking.cpp`). C2 CI FAST `40787465884`; C2 DEVICE `40776968165`
  (59/59) and `40782861799` (67/67).
- Audit branch: `audit/function-council-r2-full-product-ux-platform-r1`, created
  from exactly `70a6c14`. Working tree clean before branching (`git status
  --short` → 0 lines). `local.properties` is git-ignored.
- Ancestry: `merge-base(origin/main, HEAD) = 6c9f156` (main is an ancestor);
  HEAD is 126 commits ahead of main (`306 files changed, 75136 insertions,
  2546 deletions`). `main` does NOT contain the CAD-V6 … CAD-R2-C2 task chain,
  so it is not the audit base.
- Other remote refs (20 total) recorded, none modified.

## Source inventory at `70a6c14`

| Area | Count |
| --- | --- |
| Native C++ (`app/src/main/cpp`, excl. `third_party`) | 100 `.cpp`, 101 `.h`, 123,905 lines (35 `*selftest*.cpp`) |
| JNI exports (`Java_com_forgeshape_*` in `forgeshape_jni.cpp`) | 276 |
| Java product (`app/src/main/java`) | 85 files, 33,208 lines (programmatic Views; no XML layouts) |
| JVM unit test files (`app/src/test`) | 20 |
| Instrumented test files (`app/src/androidTest`) | 76 |
| `.forge` fixture corpus (`testdata/forge`) | 72 files |
| Shaders | `app/src/main/cpp/shaders` (AOT `glslc`) |
| Vendored kernel | Manifold 3.5.4 under `cpp/third_party/manifold` |
| CI workflows | `ci-fast.yml`, `ci-device.yml`, `ci-full-sharded.yml` |

## Existing test coverage (historical, as recorded by C2 — to be re-verified, not assumed)

Host `HOST_SELFTESTS_OK (4392 checks, 0 failed)`, 25 suites, 17 geometry
oracles; JVM 196; corpus 72/72; DEVICE 126/126 over two bounded runs; FAST
`40787465884` success. These are C2's records; this audit re-executes what the
environment allows and labels everything else historical.

## Existing historical audits

`artifacts/function-council-r1/*` (against the older `a1b50f2` tree, four of
six seats partial), `artifacts/function-council-c1-d1/*` (D1 fix),
`artifacts/architecture-health-review`, `deep-audit-r1`,
`fable-cad-architecture-audit-r1` (contains an `OWNER_DECISIONS.md`),
`cad-ux-audit-r1`, `uiaudit1`, `uiaudit2a`, `post-audit-harden-r1`, and the
per-stage dossiers. None is promoted to a current fact without re-check.

## Authoritative sources — availability

| Source | Status |
| --- | --- |
| `FORGESHAPE_MASTER_PLAN_v4.1.61.md` (canonical) | **NOT AVAILABLE** — not in the repo, not attached, not on disk (filesystem search) |
| `FORGESHAPE_OWNER_DECISIONS_*` | **NOT AVAILABLE** (only the audit-local `artifacts/fable-cad-architecture-audit-r1/OWNER_DECISIONS.md`) |
| `FORGESHAPE_MASTER_PLAN_HISTORY_*` | **NOT AVAILABLE** |
| `FORGESHAPE_PROJECT_STATE.md` / addenda (v4.1.62 mentioned in `cad-r2-retained-sketch-parametric-ux/BEFORE.md`) | **NOT AVAILABLE** |
| Prior full Master v4.1.0 | **NOT AVAILABLE** |
| Repo `CLAUDE.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `PROJECT_STATUS.md`, `DATA_PACKAGE_SPEC.md`, `docs/CI_CLOUD.md`, `docs/ui/wireframes` | available |

Consequence, stated before any finding: the result can be at best
**`PARTIAL-PLAN-SOURCE-GAP`**. Horizons H0/H1/H2 are reconstructed from the
repo's own OWNER-decision records (`PROJECT_STATUS.md` *Owner Decision
Baseline*, `CLAUDE.md` rule IDs, stage dossiers) and are labelled as such.

## Environment

Linux container; Android SDK/NDK at `/root/android-sdk` (NDK `29.0.14206865`
as pinned); JDK available; **no `pwsh`** (the corpus builder is PowerShell →
local parity is NOT RUN; CI FAST runs it on Linux `pwsh`); **no local
emulator/KVM device** → device evidence only through `CI DEVICE` dispatch;
GitHub access through the MCP server (dispatch, logs, artifacts).

## Plan

1. Fresh local execution: host self-tests (25 suites), JVM + debug /
   androidTest / release builds, release guard, host corpus verdicts.
2. Fresh CI FAST on the audit branch (corpus parity via `pwsh`); bounded
   CI DEVICE runs on existing suites for journey and screenshot evidence.
3. Ten independent seats A–J, a two-round deliberation, adversarial
   verification of the strongest positive claims and highest-risk defects,
   then synthesis into the required files.
