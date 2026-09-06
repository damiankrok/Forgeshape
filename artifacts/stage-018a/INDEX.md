# Stage 018A — evidence index

**Rename, Show/Hide, Lock/Unlock and Duplicate** (`UI-OWNER-40`), plus the
SEL-OUT-R1 rollforward commit that preceded them.

Baseline `538623c26e0122d97303150ebbb52eb3ea0d32b3`. All device work on the
isolated AVD `ForgeShape_Stage006` (`emulator-5580`, confirmed by `emu avd name`
before every run). `emulator-5554` was never contacted.

Run under **`TEST-OWNER-03`**, the reduced-testing policy: focused correctness
only, one device journey, no `-FullSharded`. See `FOCUSED_RESULTS.md` for what
was measured and `OWNER_LATER_TEST_PACK.md` for what was deliberately deferred.

---

## What was built

Four object commands, all of them **project truth** rather than row decoration,
all of them **one history transaction**, and all of them owned by one
platform-neutral module (`forgeshape_body_commands.{h,cpp}`) built on
`forgeshape_body_delete`'s pattern.

| Command | Where it is enforced | What it costs |
| --- | --- | --- |
| **Rename** | the domain's existing name rule, reached a new way | no publish, no revision |
| **Show/Hide** | `ConstructionScene::snapshot`, the ONE list the renderer and CPU picking share | one boolean per body per frame |
| **Lock/Unlock** | two named guards: `setGizmoActive` and the transform entry point | nothing |
| **Duplicate** | dispatches on `BodyRepresentation`; refuses a face-supported CAD body by name | one publish, for the body it created |

The three facts (`name`, `visible`, `locked`) sit on `SceneObject` beside
`transform_`, are captured/compared/restored by the Construction history's
existing SNAPSHOT, reach the `.forge` file through **`SCNE` v2**, and move the
project fingerprint. Undo and Redo needed no per-command inverse.

---

## Gates

| gate | result |
| --- | --- |
| Native self-tests, debug launch | **twenty** `*_SELFTEST_OK` tokens then `FORGESHAPE_NATIVE_VIEWPORT_OK`; **zero** failures. Scene **155** checks (was 130), Project **256** (was 251) |
| Native standalone runner (x86_64, NDK clang) | **1385 checks, 0 failed** across scene, history, project, cad, cad_a3, sketch_ux, gltf_export and sculpt |
| `ObjectCommandsTest` (the one device journey) | **OK (1 test)** |
| Focused device regression, final tree | **OK (118 tests)** over `ObjectCommandsTest`, `ObjectsDeleteTest`, `EditorWorkspaceObjectsTest`, `EditorWorkspaceGizmoTest`, `ProjectAutosaveRecoveryTest`, `ImportedMeshDurableTest`, `SettingsPreferencesTest`, `HomeFlowTest` |
| `assembleDebug` | BUILD SUCCESSFUL |
| `.forge` corpus, `-VerifyOnly` | all **28 pre-existing fixtures byte-identical**; two new `SCNE` v2 fixtures added, corpus now **30** |
| C++ / PowerShell fixture parity | the encoder reproduces both new fixtures byte for byte — `object_state=3c9bcd63…`, `object_state_bad_flags=b2cc2bf2…`, matching the builder's output exactly |
| `scripts\verify-device-guards.ps1` | all `DEV2-01..07` and `DEV3-01..06` PASS over 19 executable surfaces |
| `-FullSharded` | **NOT RUN**, by policy (`TEST-OWNER-03`). No `FULL_SHARDED_SUITE_PASS` is claimed |

---

## The measurements that matter

**Backward compatibility is a value, not a hope.** `OBJ018A-12` reads the SCNE
section version straight out of the encoded bytes and asserts a plain project
stays at **v1**; the corpus verifier then confirms all 28 older fixtures are
byte-identical on disk. A v1 file decodes with `visible = true`,
`locked = false` and no SCNE name, and applying it to a live scene leaves both
defaults — by the model's own member initializers, not by a migration branch.

**The version bump is fail-closed.** `SCNE` is a required section, so a build
that does not know v2 refuses the file rather than opening a project with a lock
silently dropped. A reserved flag bit is refused (`BadPayload`), never masked —
`object_state_bad_flags_v2.forge` pins exactly that.

**Hidden is one fact.** `OBJ018A-03` asserts the hidden body is absent from
`snapshot()`, which is simultaneously what the renderer draws and what picking
casts against — so there is no second predicate to drift, and no renderer branch
was added. The body keeps its published revision throughout, so showing it again
republishes nothing.

**Lock is two named guards, not a missing control.** The device journey turns
the gizmo on, locks the body, and asserts the gizmo is gone AND that a transform
write reached directly below JNI returns `APPLY_REJECTED_LOCKED` — its own code,
because a lock is not a statement about a value.

**A duplicate does not inherit the original's strokes.** `OBJ018A-10` gives the
source a real deformation and one sculpt-history entry, duplicates it, and
asserts the copy carries the deformed geometry and the `hasEdits` fact while its
own `SculptHistory` is empty in both directions — and the source's is untouched.

**Unicode survives exactly.** `OBJ018A-02` renames a body to a string carrying
accented Latin, CJK and a supplementary character (U+1F9F2), proves the
UTF-16 boundary round-trips it byte for byte, and then proves the `.forge` round
trip does too. This is the half `GetStringUTFChars` would have silently broken.

---

## What is NOT claimed

* **No `-FullSharded` aggregate.** It was not run, by explicit policy. This is
  focused evidence and cannot stand in for the exhaustive gate.
* **No owner approval of the row UI.** Whether the overflow, the inline strip
  and the two toggle glyphs read correctly under a real thumb is the OWNER's,
  and nothing here decides it. See `OWNER_LATER_TEST_PACK.md`.
* **Not every representation was driven on device.** The journey is one flow
  over one representation; representation-neutrality is proved by the domain
  suites, which build their own scenes.
* **The Delete → Undo → Redo owner verdict** of `IMPORT-01B` / `UI-OWNER-45` is
  still pending and is not answered here. Stage 018A's Delete cases are
  automated **non-collision** evidence only.
* **The `SpatialSketchTest` isolation defect** found during SEL-OUT-R1 is
  retained as known test debt and was deliberately not fixed here.
