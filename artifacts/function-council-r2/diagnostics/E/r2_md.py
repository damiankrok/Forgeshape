p='/home/user/Forgeshape/artifacts/function-council-r2/SEAT_E.md'
s=open(p).read()
import re
def R(old,new):
    global s
    pat=r'\s+'.join(re.escape(t) for t in old.split())
    m=list(re.finditer(pat,s))
    assert len(m)==1, (len(m), old[:70])
    a,b=m[0].span()
    lead=old[:len(old)-len(old.lstrip())]
    s=s[:a]+new.lstrip() if False else s[:a]+new[len(lead):]+s[b+ (0)] if False else s[:a]+new[len(lead):].rstrip('\n')+s[b:]
R("| `PROJECT` | 256 / 0 | Encode/decode/validate; committed digests `FSR1A_12_*` and `IMP01A_19_*` (`forgeshape_project_selftest.cpp:2330-2360`) |",
  "| `PROJECT` | 256 / 0 [^cap] | Encode/decode/validate; committed digests `FSR1A_12_*` and `IMP01A_19_*` (`forgeshape_project_selftest.cpp:2330-2360`) |")
R("No host check covers GLB export of a CAD body,",
  "[^cap]: R2 (seat H obj 16): 256 is exactly the device result cap `kMaxProjectChecks = 256` (`forgeshape_jni.cpp:821`), and `Recorder::check` silently drops records past `max` (`forgeshape_project_selftest.cpp:21-32`). The host harness allocates 8192 (`scripts/host-native-selftests.sh:104`), so the next PROJECT check added will run on the host and vanish on the device. The device PROJECT token cannot grow without a cap change. E2.\n\nNo host check covers GLB export of a CAD body,")
R("- `InvalidSemanticValue` for every other `bad_*`, `noncanonical`,\n  `unresolved`, `overlap`, `conflict` and `surface_bad_ref` fixture\n",
  "- `InvalidSemanticValue` for every other `bad_*`, `noncanonical`,\n  `unresolved`, `overlap`, `conflict` and `surface_bad_ref` fixture (22 of 28)\n\nR2 (seat H obj 15): the script only prints names; no expected-file was passed. The 22 `InvalidSemanticValue` refusals do not prove each file is refused for its PLANTED defect. E1 for the refusal, E2/E5 for the cause.\n")
R("  Independent bodies match exactly.\n- **Impact.** A face-supported body is exported",
  "  Independent bodies match exactly.\n- **Second divergence (R2, seat H obj 1).** `ConstructionScene::snapshot`\n  leaves out a face-supported body whose support does not resolve\n  (`forgeshape_scene.cpp:359-366`); export still writes it, at its stored\n  identity. The scene comment says a well-formed scene never reaches that\n  branch (load validates the graph; edits keep the lineage), so reachability is\n  E5; the fix must refuse such a body by name rather than export it.\n- **Impact.** A face-supported body is exported")
R("  If it does not resolve, fail by name. Add one host check per face\n  fixture.",
  "  If it does not resolve, fail by name (not export at the origin). Add one\n  host check per face fixture.")
R("  - Recover reloads the checkpoint with no dirty question and clears the live\n    Construction Undo history. If the checkpoint was written before the\n    newest edits, those edits are lost: on a foreground recreate, `onStop`'s\n    immediate checkpoint runs on the old worker while the new constructor\n    reads the file.",
  "  - Recover reloads the checkpoint with no dirty question and clears the live\n    Construction Undo history (E2).\n  - R2 regrade (seat H obj 2): \"those edits are lost\" is E5. It needs\n    `onStop`'s immediate checkpoint on the old worker to race the new\n    constructor's read; the race itself is HYP-E7.")
R("- **Impact.** A recovery candidate can be offered and then fail to recover:\n  it is quarantined and reported. Open File reports \"invalid\". Both stay\n  fail-closed. Only a crafted file, or a corrupt one with a valid CRC, can\n  carry such a body.",
  "- **Impact.** A recovery candidate can be offered and then fail to recover:\n  it is quarantined and reported. Open File reports \"invalid\". The project\n  scene is unchanged by the refusal, but (R2, seat H obj 8) the JNI wrapper\n  has already cancelled any open sketch and ended its view before\n  `loadProjectDocument` refuses: see E-12. Only a crafted file, or a corrupt\n  one with a valid CRC, can carry such a body.")
R("- **Fix.** Refuse `faceEdgeLocalIndex > 0xFFFF` in every face-token reader\n  (v2 TopoRef, v5 support, v6 token), and state the bound in\n  `DATA_PACKAGE_SPEC.md` §7c.",
  "- **Fix.** Refuse `faceEdgeLocalIndex > 0xFFFF` in every face-token reader\n  (v2 TopoRef, v5 support, v6 token), and state the bound in\n  `DATA_PACKAGE_SPEC.md` §7c. R2 (seat H obj 5): the `& 0xFFFF` mask is part\n  of the FORMAT lineage formula (`DATA_PACKAGE_SPEC.md:555`); the fix is a\n  decode-side bound only and must not change the code formula, or every\n  fixture's lineage token moves.")
R("- **Grade and verdict.** E2. DEFECT-CONFIRMED.\n- **Fix.** Retire the checkpoint only after the replacing project is live, or\n  reset the autosave baseline.",
  "- **Grade and verdict.** E2. R2 (seat H obj 7): PARTIAL, owner decision.\n  CLAUDE.md says Discard retires the checkpoint, and the user did say\n  Discard; whether a cancelled picker or refused file should restore\n  protection is a policy choice. P3.\n- **Fix (if the owner chooses protection).** Retire the checkpoint only after\n  the replacing project is live, or reset the autosave baseline.")
R("- **Impact.** No shell path reaches it mid-drag today.\n- **Grade and verdict.** E2. DEFECT-CONFIRMED.",
  "- **Impact.** No shell path reaches it mid-drag today: `closeProject` is\n  called only from shell paths after the dirty guard\n  (`EditorWorkspaceView.java:2420, 2446, 2580, 2641, 2721`).\n- **Grade and verdict.** E2. R2 (seat H obj 13): PARTIAL (missing defensive\n  guard), P3/INFO. A guard asymmetry with no reachable wrong outcome is not a\n  confirmed defect.")
R("### E-11 — INFO — Documentation divergences",
  """### E-12 — P3 — The JNI load wrapper mutates sketch, view and gesture state before a post-decode refusal (R2, from seat H obj 8)

- **Location.** `forgeshape_jni.cpp:8463-8478`. Under `g_stateMutex`, before
  `loadProjectDocument`, it clears `g_grabbing` and `g_strokePending`, calls
  `sketchSession().cancel()` and `endSketchView()`.
- **What is wrong.** `loadProjectDocument` can still refuse after a decode Ok:
  `RefusedEditInProgress` (`forgeshape_project_state.cpp:250`), a validate
  failure, `MissingRequiredSection` from `runtimeCanEvaluateProject`, or the
  E-05 class (`InvalidSemanticValue`, `pf_gap.out`). The staged sketch or Edit
  Sketch is then discarded and the camera moved by a load that fails.
- **Reachability.** Open Saved Project / Open File are drawer rows, and the
  mark is shown while sketching (`ProjectDrawerPolicy.java:109-111`). A clean
  project skips the unsaved question. On a refusal the Java callers skip the
  refresh (`EditorWorkspaceView.java:5490-5496`, `6095-6103`), so the chrome
  can still describe a sketch native has cancelled (E5 for the visible effect).
- **Grade and verdict.** E2. DEFECT-CONFIRMED. P3. Violates CLAUDE.md "a
  failed load changes nothing at all" at the entry point the product calls.
- **Fix.** Run `editInProgress`, validate and `runtimeCanEvaluateProject`
  before the cancels, or move the cancels into the commit branch.

### E-13 — P3 — A checkpoint can record a fingerprint for a state it did not encode (R2, from seat H obj 11 / SEAT_H H-02)

- **Location.** `AutosaveController.performCheckpoint` reads
  `projectFingerprint()` (`AutosaveController.java:157`) and `encodeProject()`
  (`:173`) in two separate `g_stateMutex` acquisitions (`forgeshape_jni.cpp:8936-8947`,
  `8385-8410`), then records the EARLIER fingerprint (`:189`).
- **Outcome.** An edit during the encode followed by an Undo inside the 900 ms
  debounce (`:60`) leaves `recovery.forge` holding the undone edit; the next
  run sees fingerprint == recorded and never rewrites it.
- **Grade and verdict.** E2 mechanism, timing E5. DEFECT-CONFIRMED (same grade
  as SEAT_H H-02). P3.
- **Fix.** Return fingerprint and bytes from one JNI call under one lock.

### E-14 — P3 — A checkpoint write in flight can land after Save cleared it (was H-E1; R2, seat H obj 12)

- **Location.** `saveProjectToSlot` (`EditorWorkspaceView.java:5458-5468`)
  runs `ProjectCheckpoint.clear` on the UI thread; `performCheckpoint` writes on
  the worker with no shared lock; `noteProjectPersisted` is posted behind any
  in-flight write (`AutosaveController.java:202-218`), so it cannot prevent one.
- **Outcome.** A `recovery.forge` survives Save, is never rewritten while the
  fingerprint stays equal, and is offered at the next cold launch. Its state may
  be OLDER than the slot, because its fingerprint and encode preceded Save.
- **Grade and verdict.** E2 mechanism, timing E5. DEFECT-CONFIRMED, P3
  (graded identically to E-13 / H-02).
- **Fix.** Serialize `clear` through the autosave worker (post it, and have
  `performCheckpoint` re-check a "cleared at generation" marker).

### E-11 — INFO — Documentation divergences""")
R("- **H-E1. An autosave write can land after Save clears it.**",
  "- **H-E1 → promoted to E-14 in round 2.** An autosave write can land after Save clears it.")
R("- **H-E4. Save can stall the UI thread.** Save encodes and fsyncs on the UI\n  thread (`saveProjectToSlot`), which could jank or ANR on large sculpt or\n  imported projects. E5.",
  "- **H-E4. Save can stall the UI thread and the viewport.** Save encodes and\n  fsyncs on the UI thread (`saveProjectToSlot`), which could jank or ANR on\n  large sculpt or imported projects. R2 (seat H obj 22): the encode holds\n  `g_stateMutex` (`forgeshape_jni.cpp:8391`), which the render loop takes every\n  frame to snapshot the scene (`forgeshape_jni.cpp:1870-1872`; seat H cited\n  1822-1826, which is the `g_viewport.mutex` wait), so the viewport freezes for\n  the whole encode too; the same applies to the autosave worker's encode.\n  Mechanism E2; reachability at large sizes E5, OWNER-PHYSICAL-UNVERIFIED.\n- **HYP-E6. Two autosave writers can destroy the valid checkpoint.** Across a\n  recreation the old controller's `quitSafely()` still runs onStop's queued\n  immediate checkpoint (`AutosaveController.java:283-289`) while the new\n  constructor creates a second controller (`EditorWorkspaceView.java:1019`).\n  `ProjectCheckpoint.write` (`ProjectCheckpoint.java:85-125`) is static, has no\n  lock, and every writer uses `recovery.forge.pending`. If two overlap, the\n  first rename succeeds, the second's rename fails (its pending is gone), and\n  the fallback `checkpoint.delete() && pending.renameTo(checkpoint)` (`:112`)\n  deletes the valid checkpoint and leaves none; both streams may also have\n  written the same inode. Mechanism E2, reachability E5 (needs the new\n  controller to write while the old one is still writing).\n- **HYP-E7. A checkpoint read can race a rename and quarantine a valid file.**\n  `read()` takes `checkpoint.length()` (`:133`) and then opens and `readFully`s\n  (`:141`). A rename in between gives EOF (null → `PROJECT_NO_DATA`) or a\n  prefix (`Truncated`); `offerRecoveryIfPresent`\n  (`EditorWorkspaceView.java:5583-5595`) then quarantines the session's own\n  valid checkpoint and reports \"recovery failed\". E-02's foreground recreation\n  reaches this read. Mechanism E2, reachability E5.")
R("- **Every refusal fixture is refused by its expected name** (28 of 28), and\n  every valid CAD fixture regenerates (E1, `host_corpus_verdicts.txt`).",
  "- **28/28 refusal fixtures are refused, with the status the self-tests and\n  builder expect; the specific planted cause is not asserted** (22 are the\n  coarse `InvalidSemanticValue`). Every valid CAD fixture regenerates. E1 for\n  the refusal, E2/E5 for the cause (R2, seat H obj 15).")
R("- **Load fails closed, and a successful load starts a fresh history.**",
  "- **Load fails closed at `loadProjectDocument` (not at the JNI entry point,\n  see E-12), and a successful load starts a fresh history.**")
R("| J-E5 | negative, regression for E-03 |",
  "| J-E6 | negative, regression for HYP-E6/E7 | A large project open, edited, no candidate at launch | Trigger a recreation (palette change) while a checkpoint of the large project is being written | Exactly one valid `recovery.forge`; no `.pending` left; no quarantine; no recovery prompt |\n| J-E7 | negative, regression for E-12 | A clean project with an Edit Sketch open | Open File → pick a file that decodes Ok but fails load (E-05 class) | Refused; the staged sketch, the camera and the chrome are exactly as before |\n| J-E5 | negative, regression for E-03 |")
open(p,'w').write(s)
