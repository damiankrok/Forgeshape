# FUNCTION-COUNCIL-R2-CLOSEOUT-R1 — BEFORE (preflight, before any audit work)

Type: audit only. No product, test, script, fixture, workflow, Gradle or
format change. Nothing is fixed in this stage.

## Git snapshot (verified 2026-10-30 by `git fetch`)

| Item | Value |
| --- | --- |
| Council R2 head (base) | `origin/audit/function-council-r2-full-product-ux-platform-r1` = `2e5d8fc4a41094fbb720c18bd43926a94c0eb7e0` (matches the prompt) |
| Product parent | `origin/feature/cad-r2-c2-picking-seam-r1` = `70a6c14f703bfda2ab689b855126c1d12a541e41` (matches) |
| `origin/main` | `6c9f156c1df91fe1a55445aed6186105a075ab31` (matches) |
| New branch | `audit/function-council-r2-closeout-r1`, created from exactly `2e5d8fc` |
| `git status --short` before branching | empty |
| Ancestry | `70a6c14` ⊂ HEAD (yes); `origin/main` ⊂ HEAD (yes); no merge commits after `70a6c14` |
| Product diff `70a6c14..2e5d8fc` | only `artifacts/function-council-r2/**` and `PROJECT_STATUS.md`; 0 files under `app/`, `scripts/`, `testdata/`, `.github/`, Gradle, `DATA_PACKAGE_SPEC.md` |

## Source package

`FORGESHAPE_COUNCIL_R2_SOURCE_ARCHIVE.zip` (197,231 bytes, SHA-256
`90f5d5818a9294fc53a3b4784cfafb477e3af03762a344f61988018d65ef20c4`),
delivered by the OWNER in this session. **16/16 files match `MANIFEST.md`
byte count and SHA-256** (`sources/SHA256SUMS.txt`). The manifest itself
states that the full canonical `FORGESHAPE_MASTER_PLAN_v4.1.61.md` and the
matching full history / OWNER decisions are **absent** and must not be
reconstructed by concatenation. Copies are kept unchanged under `sources/`.

## Inputs re-read (not rewritten)

`artifacts/function-council-r2/**` at `2e5d8fc` (681-row matrix, 10 seats,
`work/VERIFICATION.md`, evidence of `CI FAST 40796992679` and `CI DEVICE
40797031704 / 40800569636 / 40805804385`), `artifacts/function-council-r1/**`,
CAD-R2 / C1 / C2 dossiers, `PROJECT_STATUS.md` v0.103.0.

## Environment

Linux container; NDK `29.0.14206865`; JDK; no `pwsh`; no emulator/KVM (device
evidence only via `CI DEVICE` dispatch, 2700 s instrumentation cap per run);
no MSVC; no physical device. FullSharded is not authorised and will not run.
