# SKETCH-NAVIGATOR-OUTCOME-R1 — SUMMARY

**Result: `PASS-SKETCH-NAVIGATOR-OUTCOME-R1`.**

| Gate | Status |
| --- | --- |
| Technical | TECH PASS (emulator) |
| OWNER physical review | REQUIRED |
| Integration | NOT MERGED |

NO CURRENT FULL MASTER PLAN COMPLIANCE CLAIM.

## Outcome

The sketch orientation navigator now turns the real view. +90° / −90° turn the
drawing on screen, Flip shows the plane from behind, and the plane chips
re-frame the camera. All of this holds:
- on every world plane and every combination of flip and roll;
- with a correct, never mirrored, orthographic camera;
- with drawing and picking landing exactly where the user sees.

A face sketch rolls but cannot be flipped: the chip is disabled, and the
refusal is now also enforced below JNI. Back to Sketch shows the aligned view
with a navigator that says so. Cancel and Commit return the camera the user
had before the sketch. View acts change no authored, project or history byte.

This is the fix for the already-accepted `UI-OWNER-48` / `SKETCH-UX-R1`
behaviour (CR-06-S3), not a new navigator.

**Root cause.** JNI `beginSketchView` framed the camera on the authoring frame,
so `SketchSession::viewFrame()` had no production caller.

**Fix.** `frameCameraOnSketchView` (domain) is the one framing statement, and
JNI calls it. `backToEditing` resets flip and roll. `setViewFlipped` refuses
outside a session and on a face sketch. The options considered are in
`DESIGN_DECISIONS.md`.

## Files changed

| Area | Files |
| --- | --- |
| Product | `forgeshape_sketch_session.{h,cpp}`, `forgeshape_jni.cpp` |
| Host tests | `forgeshape_sketch_ux_selftest.cpp` (+18) |
| Device test | `SketchNavigatorCameraTest.java` (new, 9 cases) |
| Docs | `ARCHITECTURE.md`, `CLAUDE.md`, `PRODUCT.md`, `PROJECT_STATUS.md` |

Unchanged:
- Java;
- renderer, kernel, picking;
- formats, fixtures, `DATA_PACKAGE_SPEC.md`;
- `.gitattributes`, CI, build.

## Evidence

| Gate | Result |
| --- | --- |
| RED | CR-06 host re-run; `CI DEVICE` `41507903710` 7/9 FAIL on the unchanged product; mutations 6 / 1 / 2 |
| Host | `HOST_SELFTESTS_OK (4472 checks, 0 failed)`; 25 suites; 17 oracles; SKETCH_UX 136/256 |
| JVM | 199/0 |
| Corpus | 72 |
| Release guard | PASS |
| `CI DEVICE` `41517008402` | **69/69** |
| Final `CI FAST` `41524092732` | success; `app-debug.apk` 17755951 bytes, SHA-256 `f8cbb67de8e1b0541ebf4d5cbd79d203eb82689d6456c195fc93022c0aaf4194` |

Every attempt, failures included, is in `TESTS.md`.

## Invariants measured

Project bytes and history did not move with the view acts:

| Measure | Value |
| --- | --- |
| Project bytes | 294 |
| Fingerprint | `22a5519654955ab0` |
| Undo / Redo | 0 / 0 |
| Bodies | 1 |
| Drawn line | the same before and after |

Commit adds exactly one Undo step.

## Held / OWNER

- **OD-09 option (b):** flip and roll surviving Back to Sketch / Edit Sketch.
  Not implemented; reset-on-re-entry is current.
- **OD-01, OD-03, OD-27:** open.
- **`.gitattributes`:** OWNER-REVIEW-PENDING.
- **CR-03 Q2 / Q3:** import with the chooser alone, and with no project.
- **Flip chip hit area:** 36.19 dp (historical CR-06-S6). Reported, not
  changed.
- **Physical device, ARM64, stylus, hover, TalkBack:** UNVERIFIED.
