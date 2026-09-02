# IMPORT-01B — evidence index (2026-09-02)

Result: **PASS-IMPORT-01B-OWNER-RETEST-READY**, with
`FULL_SHARDED_SUITE_PASS` on the final tree (472/472).
Baseline `f4d6cf73648cf6de1259b0a7d1cc79c7959d5802` (clean). The final HEAD is the
commit that adds this directory; `PROJECT_STATUS.md` records it.

Two bounded capabilities, under `ARCH-OWNER-11` and `UI-OWNER-45`:

1. an **Imported Mesh** enters the existing reversible Sculpt workflow, seeded
   from its own geometry, with the imported arrays immutable throughout;
2. the Objects list **deletes a real `SceneObject`**, as exactly one Undo.

| File | Contents |
| --- | --- |
| `SCULPT_SOURCE_CONTRACT.md` | the seed contract: one dispatch point, the raw-index and sidedness decisions and why, the placement split, immutability, state transitions, export |
| `DELETE_CONTRACT.md` | the delete operation: representation neutrality, the held body and the widened prune, one transaction, selection, the two refusals, the renderer prune, the UI |
| `DATA_CONTRACT.md` | the `.forge` decision: `SCUL` generalized, no version bump, why that is safe, the four valid combinations |
| `CORPUS.md` | every fixture with bytes and SHA-256 — ten unchanged, two new — and where each is asserted |
| `TEST_RESULTS.md` | native suites, JVM, device guards, corpus, focused and regression instrumented classes |
| `DEVICE_E2E.md` | `E2E-IMP01B-01..12` with what proves each, and the honest gap at 12 |
| `FULL_SHARDED.txt` | the authoritative exhaustive-sharded aggregate: `FULL_SHARDED_SUITE_PASS`, 33 classes / 472 tests / 5 shards, missing=duplicates=unexpected=0. It took six attempts — two real defects it caught (an unbounded GPU fence wait that hung the render thread, and a test that assumed process-scoped state) and four device failures. Both defects are written up in `DELETE_CONTRACT.md`; the device story is in `TEST_RESULTS.md` |
| `selftest_startup.txt` | raw `logcat -s ForgeShape:V` of the launch: 17/17 suites, 2628 checks, both golden digest lines |
| `instrumented-*.txt` | raw runner output for every instrumented class run |

## What is deliberately NOT here

**No screenshots.** Every claim in this stage is a value a test asserts — a
`SculptRevision`, a byte comparison of an `IMPT` section, a history depth, an
ObjectId a pick returned — and a screenshot could not carry any of them. The
UI change is two new labels and one new control per row, and their geometry is
asserted numerically in `ObjectsDeleteTest.imp01b15and23_*`.

**No owner-file evidence.** `E2E-IMP01B-12` is
`OWNER_REAL_FILE_01B_RETEST_PENDING`: the owner's `1 lowpoly.glb` is not in this
environment and no user folder was searched for it. See `DEVICE_E2E.md`.

**No `BRIDGE-R1`.** The one-way Construction-to-Sculpt project derivation was
excluded by the stage and nothing here approaches it.

## Device discipline

`emulator-5580`, confirmed `ForgeShape_Stage006` by `adb -s emulator-5580 emu avd
name` before anything was installed. **`emulator-5554` was never contacted.**
Every instrumented run went through `scripts\run-instrumented-tests.ps1 -Serial
emulator-5580`; no bare `connected*AndroidTest` and no unscoped `adb` call was
issued.
