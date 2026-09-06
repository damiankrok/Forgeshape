# Stage 018A — focused results

Every case `OBJ018A-01..16`, where it lives, and what it actually asserts.
Cases 01–10 and 12–14 are in the **scene** self-test, 11 is in the **CAD-A3**
self-test (it needs that suite's face fixtures), and 15–16 are in the
**project** self-test beside the other golden digests. All of them build their
own scene and their own history, so none depends on what a previous test or a
live session left behind.

---

## A. Fast domain / history / codec cases

| Case | Suite | What it asserts |
| --- | --- | --- |
| `OBJ018A-01` | scene | Rename succeeds and stores the name; it is **exactly one** history step; Undo restores the previous name and Redo reapplies the new one; the selection stays on the same `ObjectId`; renaming to the name it already has records **nothing**; an empty/whitespace name is **refused** and the old name stands with the depth unmoved; an unknown body is refused |
| `OBJ018A-02` | scene | A name of accented Latin + CJK + a **supplementary character** (U+1F9F2) is accepted and stored exactly; the UTF-16↔UTF-8 boundary round-trips it byte for byte; the whole `.forge` round trip returns the identical byte string |
| `OBJ018A-03` | scene | Two bodies are in `snapshot()`; after Hide the hidden one is **absent** and the count drops; it is still **in the scene** and still findable; it **kept its published revision**; showing it again restores the snapshot with no republication |
| `OBJ018A-04` | scene | Hide is one step; hiding the **active** body does not move the selection; Undo shows it again and the snapshot grows back; Redo hides it and the snapshot shrinks; hiding an already-hidden body records nothing |
| `OBJ018A-05` | scene | Lock stores; a locked body is **still drawn and picked** (still in `snapshot()`); it keeps its placement; the lock is readable where the JNI guard asks it; **Rename and Hide still work** on a locked body; Unlock restores the movable state |
| `OBJ018A-06` | scene | Lock is one step; Undo unlocks; Redo locks again; locking an already-locked body records nothing |
| `OBJ018A-07` | scene | Duplicate succeeds and adds one body; the copy has a **fresh `ObjectId`** at or above the allocator's prior mark; the allocator only moved **forward**; the copy is the active body |
| `OBJ018A-08` | scene | The copy has the same representation, the same shape state, the same placement, and carries the **lock**; its name is the deterministic `bracket copy`; it **published its own geometry**; the source is untouched; a second copy becomes `bracket copy 2` |
| `OBJ018A-09` | scene | Duplicate is **exactly one** step; Undo removes **only the copy** and leaves the source; Redo restores the **same identity**; a later creation does **not reuse** the copy's id |
| `OBJ018A-10` | scene | A source with a real deformation and one sculpt-history entry duplicates; the copy owns a frozen mesh carrying the **deformed geometry**; the mesh wears the **copy's** identity; the copy reports the `hasEdits` fact; the copy's `SculptHistory` is **empty both ways**; the source's is **untouched** |
| `OBJ018A-11` | cad_a3 | A world-plane CAD body **duplicates**, carries the authored sketch and regenerates its own mesh; a **face-supported** dependent is refused `RefusedFaceSupportedCad`; the refusal creates **no body, no id and no step**; the dependency graph is intact afterwards; a **producer that has dependents still duplicates**, and the copy has none of its own |
| `OBJ018A-12` | scene | A plain project still encodes and **stays at `SCNE` v1**, read out of the bytes; it decodes with `visible`, `!locked` and no SCNE name on every body; applying it to a live scene leaves both defaults |
| `OBJ018A-13` | scene | A project with a name, a hidden body and a locked body **is promoted to v2**; it decodes; `sameProjectDocument` holds; the state reaches the live scene; a load starts a **fresh history**; re-encoding what came back reproduces the **same bytes**; and each of the three commands **moves the fingerprint** |
| `OBJ018A-14` | scene | A hidden **and** locked body still deletes; Delete is still exactly one step; Undo restores the same object **with its visibility and lock**; Redo removes it again; the **last body is still refused**; the selection fallback is unchanged |
| `OBJ018A-15` | project | `object_state_v2.forge` matches the committed digest `3c9bcd63…`; it decodes to the same document; it carries the name, the lock and the hide exactly, with the third body unmarked |
| `OBJ018A-16` | project | `object_state_bad_flags_v2.forge` matches the committed digest `b2cc2bf2…`; a **reserved** SCNE flag bit is refused `BadPayload`, never masked off |

### Totals

```
scene           155 checks, 0 failed      (was 130 before this stage)
history         147 checks, 0 failed
project         256 checks, 0 failed      (was 251)
cad             122 checks, 0 failed
cad_a3           66 checks, 0 failed
sketch_ux        52 checks, 0 failed
gltf_export      93 checks, 0 failed
sculpt          494 checks, 0 failed
TOTAL          1385 checks, 0 failed
```

On the debug launch: **twenty** `*_SELFTEST_OK` tokens, zero `*_SELFTEST_FAIL`,
zero `_FAIL:`, then `FORGESHAPE_NATIVE_VIEWPORT_OK`.

---

## B. The one device journey

`ObjectCommandsTest.e2eObj018a01_theRowCommandsRunThroughTheRealControls` —
**OK (1 test)**.

A two-body project, driven through the real controls, found by semantic id and
`ObjectId` tag and never by screen position:

1. the row's **⋯** opens the strip (48 dp hit area asserted);
2. **Rename** opens the inline field; the name is typed and committed; the
   domain has it, it is one history step, and the row label reads it back;
3. **Hide** — the body is hidden, one step, and the row is still there;
4. **Show** — it comes back;
5. the gizmo is confirmed **offered**, then **Lock** — the gizmo is gone, and a
   transform write reached directly below JNI returns `APPLY_REJECTED_LOCKED`;
6. **Unlock** — the gizmo comes back;
7. **Duplicate** — exactly one body added, one history step, a **new**
   `ObjectId`, the copy active and named `carrier plate copy`, and the list has
   grown by one row that can be found by the copy's tag.

Every control asserts its 48 dp hit area and a content description naming the
act and the body.

**One flow, one representation, deliberately.** Repeating it per representation
would re-prove the representation-neutrality the domain suites already
establish, and `TEST-OWNER-03` asks for the smallest useful device evidence.

---

## C. Build sanity and regression

| Check | Result |
| --- | --- |
| `gradlew :app:assembleDebug` | BUILD SUCCESSFUL |
| Focused device regression on the final tree | **OK (118 tests)** |
| `.forge` corpus `-VerifyOnly` | 28 pre-existing fixtures byte-identical; 30 total |
| C++ ↔ PowerShell fixture parity | both new digests match exactly |
| `verify-device-guards.ps1` | all `DEV2-*` / `DEV3-*` PASS |

The regression set was chosen as the code actually at risk: the Objects list and
Delete (the row changed), the gizmo (lock withdraws it), and the persistence
suites (the codec changed). Unrelated suites were **not** run, per policy.

### One test was updated rather than left to pass by accident

`ObjectsDeleteTest.imp01b24` asserted that no rename/duplicate/hide/lock control
was drawn. Those controls now exist, and the case would have kept passing only
because it scans visible TEXT and the new controls are icons with content
descriptions. It was rewritten to assert what Stage 018A actually still forbids
— grouping, nesting, reorder and multi-select — rather than left as a test that
passes for the wrong reason.

`FSR1A_09_a_newer_required_section_version_is_refused` probed with SCNE version
**2**, which is now a version this build understands. It was moved to version 99
so it still tests what its name says.

---

## Timing

Automated work counted against the `TEST-OWNER-03` budget: **≈21 minutes**
(native runner builds and runs, corpus build/verify, two `assembleDebug` runs,
two launch captures, four device runs, device guards). Inside the 30-minute hard
stop.

Not counted, and reported separately for honesty: **≈8 minutes** recovering the
isolated AVD, which dropped off adb mid-session. That was an infrastructure
fault, not a product failure; the stuck ForgeShape emulator process was stopped
by pid (the reserved `emulator-5554` process was left alone), the AVD was
restarted through `scripts\start-forgeshape-emulator.ps1`, and its identity was
re-confirmed by `emu avd name` before any further use.
