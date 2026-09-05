# Persistence and the `.forge` codec — DEEP-AUDIT-R1 (Part F)

## What was read

`forgeshape_project_document.{h,cpp}` (1669 lines), `forgeshape_project_bytes.h` (`ByteWriter`/`ByteReader`), `forgeshape_project_state.{h,cpp}` (capture, `loadProjectDocument`, `projectSemanticFingerprint`), `forgeshape_project_selftest.cpp` (251 checks), `DATA_PACKAGE_SPEC.md`, `scripts/build-forge-corpus.ps1`, the Java storage adapters (`ProjectSlot`, `ProjectCheckpoint`, `ProjectTransfer`, `AutosaveController`).

## Format facts confirmed

Header 28 bytes (`FORGESH1`, major 1 LE, minor 0, header-bytes 28, kind, header flags, section count, file bytes). Section header 24 bytes (4-char tag, u16 version, u16 flags, u32 reserved=0, u64 payload bytes, u32 CRC-32/ISO-HDLC over the payload). Sections `SCNE`/`CONS`/`SCUL`/`IMPT`/`CADB` (v1/v2/v3). Every field is a file-owned fixed-width little-endian encoding through `ByteWriter`; no struct image, enum ABI value, pointer, `size_t`, path or Android type reaches a byte (verified by reading the writer). `projectSemanticFingerprint` uses the deliberately short FNV basis `1469598103934665603` (a change DETECTOR, never an identity).

## Corpus parity

All 28 golden digests match three ways: the device launch print (`FORGESHAPE_PROJECT_GOLDEN_SHA256`, `…_IMPORTED`, `…_IMPORTED_SCULPT`, `…_CAD`, `…_CAD_V2`), the C++ self-test assertions (`FSR1A-12`, `IMP01A-19`, `IMP01B-11/12`, `CADR0-33/34/36`, `CADA3-46..51`, `CADUXR1-38`), and the independent PowerShell `build-forge-corpus.ps1 -VerifyOnly` run (captured `corpus_verify.txt`). The 22 pre-`SKETCH-UX-R1` fixtures are byte-for-byte unchanged; the 6 v3 fixtures agree between the codec and the second implementation. **No digest moved** — the fingerprint fix (F-01) touches `projectSemanticFingerprint`, which is not serialized, so no file changed.

## Load is all-or-nothing (confirmed)

`decodeProject` → validate whole graph → `loadProjectDocument` commit. Decode/validate run into a temporary `ProjectDocument`; the commit section is arithmetic on already-built `SceneObject`s ("cannot fail"). A refused load changes nothing (`FSR1A_07`, `IMP01A_20` prove the live scene, mode, active body and history survive; `FSR1A_13` proves the session history survives a refusal). The four valid source/sculpt combinations (`SCNE+CONS`, `+SCUL`, `SCNE+IMPT`, `+SCUL`) plus `CADB` are enforced; a body named by two geometry sections or none is refused.

## Negative-fixture coverage

The self-tests construct genuinely-uncovered negatives by patching one field + its CRC: truncation, inconsistent file length, section length past EOF, impossible body count (before allocation), sculpt vertex-count overflow, flipped CRC bit, reserved batch-flag bit, unknown required section, unsupported major, `cad_bad_plane`/`cad_bad_face_ref`/`cad_dependency_cycle`/`cad_bad_arc`/`cad_bad_spline`. The four corrupt v2/v3 corpus fixtures are CONSTRUCTED with the bad value in place by the PowerShell builder (not mutated), and the two routes agreeing is what proves they describe one file. **No fuzzing framework was added** (brief).

## Java storage adapters (verified)

`ProjectSlot` / `ProjectCheckpoint`: temp-file + `fsync` + atomic rename; a failed write keeps the previous valid file; a complete fsync'd pending file that could not be renamed is KEPT (it is the only copy) rather than tidied away. `ProjectCheckpoint.quarantine` moves an undecodable candidate aside once (do-not-loop). `ProjectTransfer` opens with `"wt"` (truncating) and bounds every read at 256 MiB before allocation; no `Uri`/`ContentResolver`/path reaches JNI. `AutosaveController`: fingerprint-gated, coalesced, off the UI thread, immediate flush on `onStop`, `awaitIdle` barrier (no test sleeps for the debounce).

## Findings

| ID | Sev | Finding | Status |
| --- | --- | --- | --- |
| F-01 | P1 | `mixCad` omitted Arc/Spline payloads, so an in-place curve edit could not move the fingerprint autosave/dirty-guard rely on. | FIXED + `DAR1_01` |
| — | open (ARCH-HEALTH) | `validateProjectDocument` does not bound `nextObjectId` below the preview key range `2^60`; a crafted file could make a body and a preview share a renderer key. Reachable only from a crafted file with a test driving the preview; reported for the coordinator, not changed (a `.forge` validation-rule decision). | reported |
