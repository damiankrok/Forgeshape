# IMPORT-GLB-SKETCH-GUARD-R1 — SUMMARY

**Result: `PASS-IMPORT-GLB-SKETCH-GUARD-R1`.**

| Gate | Status |
| --- | --- |
| Technical | TECH PASS (emulator) |
| OWNER physical review | REQUIRED |
| Integration | NOT MERGED |

NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM.

## Outcome

Import GLB can no longer change the project while a sketch is open:
- Editing or Ready;
- a staged Edit Sketch or Edit Feature;
- the CAD bootstrap's first sketch.

Every door now asks:

| Door | What happens |
| --- | --- |
| Drawer row | absent while sketching; refreshed when the drawer opens |
| Row tap and late picker answer | re-read native at that moment; their own sentence |
| Bytes seam | 1006 mapped to that sentence, never "corrupt file" |
| Native `importGlbDurable` | `RefusedInSketch` (appended ordinal 6, wire 1006) under `g_stateMutex`, immediately before the commit |

Sculpt keeps `RefusedInSculpt` (1005) and its precedence. A refusal leaves no
trace, and the draft can still be finished or cancelled:
- no body, active-body, allocator, Undo/Redo, fingerprint or `.forge` byte
  changes;
- no sketch change.

The same file imports normally once the sketch is gone. This closes the sketch
half of Council `CR-03`.

## Changed files

**Native:**
- `forgeshape_import_commit.{h,cpp}`: the status, `ImportModeFacts`,
  `importModeRefusal` and `commitImportedGlbSceneInMode`;
- `forgeshape_jni.cpp`: facts under the lock, the token bound and a comment.

**Java and resources:**
- `NativeViewport.java`: `IMPORT_REFUSED_IN_SKETCH`;
- `ProjectDrawerPolicy.java`: `importShown`;
- `EditorWorkspaceView.java`: row, drawer refresh, both callbacks, the 1006
  branch and its reporter;
- `strings.xml`: one sentence.

**Tests:**
- host: `forgeshape_gltf_import_selftest.cpp`, +20 checks;
- JVM: `ProjectDrawerPolicyTest`, +3;
- device: `ImportGlbSketchGuardTest`, new, 5 cases.

**Docs:** `ARCHITECTURE.md`, `PRODUCT.md`, `PROJECT_STATUS.md`. `CLAUDE.md`
is unchanged: its rule that creation is refused while sketching now holds for
import too.

## Stability

Unchanged:
- every earlier `ImportCommitStatus` ordinal and the 1000 base;
- `.forge`, `CADB`, the codec, the 72 fixtures (decoder verdicts and FAST
  byte parity) and `DATA_PACKAGE_SPEC.md`;
- renderer, kernel, export and face placement;
- build dependencies;
- `.gitattributes`;
- the 25 startup suites and their order.

GLTF_IMPORT has 209 checks against 256 recorder slots; PROJECT stays at
256/256 with no check added. The release guard passes.

## Performance

The guard costs two boolean reads under a lock the commit already took. It
was not measured separately; the host import suite runs in 4–5 ms either way.

## Evidence

| What | Result |
| --- | --- |
| RED, host | CR-03 scenario A on `8eb94ce`: accepted, bodies 1→2, undo 0→1 |
| RED, device | `CI DEVICE` `41490895211`: 5/5 FAIL on the unchanged product |
| RED, mutation | 8 host checks |
| Host | `HOST_SELFTESTS_OK (4454 checks, 0 failed)`, 17 oracles |
| JVM | 199/0 |
| Corpus | 72 |
| Release guard | PASS |
| `CI DEVICE` `41492901369` | **65/65** |
| Final `CI FAST` `41497992257` | success; `app-debug.apk` 17727519 bytes, SHA-256 `841a9ce50431c0085fd9f0e31beb516667dc1faff146f7105412892299ab2ba7` |

Every attempt, the failed ones included, is in `TESTS.md`.

## Known issues and OWNER gates

**CR-03 Q2 — the support chooser without a sketch.** Import is still
accepted there. Whether the chooser is a creation-in-progress state is the
OWNER's decision.

**CR-03 Q3 — no project / Home.** A direct-call import is unchanged.
- The bootstrap with its first sketch open IS refused, as a consequence of the
  sketch rule.
- A chooser-only bootstrap and Home are unchanged; both are unreachable from
  the UI.

**`.gitattributes`: OWNER-REVIEW-PENDING.** `artifacts/**/diagnostics/**` also
exempts authored diagnostic sources. A narrower rule is recommended
(`BEFORE.md`); it was not applied.

**Physical device, stylus and TalkBack:** UNVERIFIED.

**Open OWNER decisions:** OD-01 (A2), OD-03 (FullSharded), OD-05/06, OD-16,
OD-22, OD-27 (physical review), OD-29.
