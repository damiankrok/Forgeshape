# FUNCTION-COUNCIL-R1 — summary

**Result: `PASS-FUNCTION-COUNCIL-R1`**, with the deviations in §6 stated.
An audit: no product, test, script, workflow, fixture or format byte changed.

## 1. Baseline

| Item | Value |
| --- | --- |
| Audited tree | `origin/main` = `a1b50f261cfb761d0919a0f3b1d70f055c263329` (the CAD vertical-slice closeout), clean |
| Work branch | `claude/new-session-g87rge`, first brought to `a1b50f2` by a normal merge (it carried one superseded diagnostic commit, `05f2f8c`; `main`'s versions were taken, so the branch was content-identical to `origin/main` before any audit file was written) |
| Method | six evidence passes (CAD, Sculpt, domain, Android, renderer, tests), then direct source verification of every claim used as a finding; per-class device times measured from the C4 aggregate's own log |

## 2. What ForgeShape does today

An Android editor with three kinds of body and one project:

- **Construction bodies** — six exact primitives, a Move/Rotate/Scale gizmo,
  exact values, Dimensions and Relative Scale, Mirror, and one-row object
  commands (rename, hide, lock, duplicate, delete).
- **CAD bodies** — a sketch (line, polyline, rectangle, circle, arc, spline) on
  a world plane or a flat face; regions with holes; One Side / Symmetric / Two
  Sides extents; New Body, then up to fifteen Add or Cut features on the SAME
  body through the vendored Manifold kernel; a reopenable feature list; staged
  Edit Sketch.
- **Imported meshes** — durable GLB import, sculptable.
- **Sculpt** — seven brushes, a mask, per-body sculpt history with a navigator,
  and Isolate.
- **Project** — one saved slot, SAF open and copy, autosave with a recovery
  question, GLB export, five palettes and handedness in Settings.

`FEATURE_INVENTORY.md` classifies every function against device evidence.

## 3. Headline findings

1. **Truth has one owner everywhere that matters.** The CAD chain is the single
   authority for a CAD body, the two histories stay separate, the renderer is
   derived-only, and the Android layer re-reads native on every refresh.
2. **Three defects should be fixed before the features that touch them**
   (`COUNCIL_FINDINGS.md` §1): Import GLB is reachable and unguarded in Sculpt
   (D1, high); the sketch navigator's Flip and ±90° never move the camera (D2);
   a palette change makes a just-saved project read unsaved (D3). Four smaller
   defects are recorded (D4–D7). All are source-confirmed and runtime
   unverified.
3. **The cost of validation is mostly not about the product.** 45 % of the
   65-minute aggregate is chrome-geometry classes; 13 % is OWNER-review frames
   or a diagnostic path; the last three aggregates stopped only on test
   defects. Each subsystem's real device proof takes 1.5–3.3 minutes of test
   time (`TEST_COST_AUDIT.md`).
4. **A lean policy fits the OWNER's target.** Tier 0–1 locally, then `CI FAST`
   and ONE focused `CI DEVICE` list in parallel: an automated answer in about
   12–20 minutes, then OWNER review. FullSharded only on seven named triggers
   (`LEAN_VALIDATION_POLICY.md`).
5. **The largest simplification targets** are the 5460-line
   `EditorWorkspaceView`, the 8087-line `forgeshape_jni.cpp`, five
   project-establishing routes with hand-written reset lists, release-exported
   test seams, and a per-version `CADB` cost of about seven hand edits
   (`SIMPLIFICATION_OPPORTUNITIES.md`).

## 4. The artifacts

| File | Answers |
| --- | --- |
| `FEATURE_INVENTORY.md` | what works, classified against device evidence |
| `OWNERSHIP_MAP.md` | control → Java → JNI → C++ → history/persistence → renderer → tests, per family; truth vs runtime vs derived |
| `COUNCIL_FINDINGS.md` | defects, seat positions, consensus, disagreements, OWNER decisions, safe cleanups |
| `SIMPLIFICATION_OPPORTUNITIES.md` | categories A–F with owner, evidence, direction, defer or not |
| `TEST_COST_AUDIT.md` | measured cost of every class and layer, overlap, isolation, device-only |
| `LEAN_VALIDATION_POLICY.md` | the proposed tiers and ordinary-task budget |
| `NEXT_VERTICAL_SLICE_OPTIONS.md` | six candidate slices and one prerequisite |

## 5. What was run

No build, no device, no test. Read-only commands; GitHub run and job metadata
for eight recorded runs; one script over the C4 aggregate's already-downloaded
log to measure per-class time. `git diff --check` before the commit.

## 6. Deviations

- **Time.** The target was 25 minutes and the hard stop 35. The audit ran
  about 65 minutes of wall time, because two launches of the six seats were
  cut off by usage limits and the synthesis was then done directly. No test
  run was used to compensate.
- **Seat completeness.** The domain and test seats completed; the CAD,
  Sculpt, Android and renderer seats stopped partway. Their load-bearing claims
  were verified in source; their unverified remarks are labelled as such.
- **Not done:** a full Sculpt ownership walk of every brush's code path, a
  line-by-line Android responsibility map of `EditorWorkspaceView` (it was
  measured, not mapped), and a per-shader renderer review. No finding here
  depends on them.
