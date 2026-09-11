# STAGE027-R0 — Sculpt workflow contract audit

**Task:** `STAGE027-R0` · **Type:** audit / contract · **Date:** 2026-09-11
**Baseline:** `6808fae17b1ad06530cdcd38d8962b1298f112a8`, worktree clean
**Product code changed:** none. **Tests changed:** none. **Schema changed:** none.
**Built:** nothing. **Ran on a device:** nothing.

This run resolves the historical Stage027 phrase — *"Isolate/hide, mesh preview,
and remaining non-history Sculpt workflow completion"* — against live source so
the coordinator can put **one** bounded contract to the OWNER.

---

## The three words, resolved

| Word | Verdict |
| --- | --- |
| **hide** | **`ALREADY_IMPLEMENTED`.** Durable, representation-neutral, one owner (`SceneObject::visible_`), one enforcement point (`ConstructionScene::snapshot()`), one Undo, persisted as `SCNE` v2 bit0, fail-closed on a reserved bit. Stage027 must **not** build a second Hide. |
| **isolate** | **`STAGE027_CANDIDATE`, and the only genuinely new thing here.** No product source exists; the only `isolate*` hits in the repo are test scaffolding that moves bodies apart. |
| **mesh preview** | **Undefined by any source, and not resolvable by this audit.** One reading is already delivered (MatCap + Flat shading). The other two need renderer work the OWNER should choose deliberately. `OWNER_DECISION_REQUIRED`. |

## The finding that gives Isolate its reason

`SculptSession::hitsSculptMesh` (`forgeshape_sculpt.cpp:1471-1492`) casts the
brush ray against **the active body's own triangles alone**. It never consults
the scene. Meanwhile Sculpt draws the whole scene — there is no sculpt branch at
the renderer seam (`forgeshape_jni.cpp:1649-1652`), and `PRODUCT.md:1287-1288`
states that as deliberate.

**So a body standing between the camera and the sculpt target hides the target
from the eye without blocking the brush. The user sculpts what they cannot
see.** That is mechanical, not aesthetic, and it is what Isolate answers.

## The gap the phrase was actually pointing at

Visibility is delivered *everywhere except where Stage027 wants it*.
`objectCommandsBlockedByMode()` (`forgeshape_jni.cpp:3017-3019`) refuses
Show/Hide — and Rename, Lock, Duplicate, Mirror, Delete and `+` — for the whole
duration of Sculpt Mode, and `EditorWorkspaceView.java:2551` withdraws the
controls to match. The Objects **list** stays; every command on it goes.

## Two defects found while reading — neither named by the phrase

Both are **SOURCE-CONFIRMED, RUNTIME-UNVERIFIED** in this audit. Both sit
directly in Isolate's path, which is why they are reported here rather than
filed away.

- **`FINDING-A` — the Sculpt target is not actually fixed.**
  `PRODUCT.md:1286-1290` and `forgeshape_jni.cpp:2800-2806` both say it is, and
  `sceneSelectBody` enforces it for the **Objects row**. The **viewport tap**
  path does not go through that guard: a Down that misses the sculpt mesh is
  deliberately not swallowed, and the tap resolves into
  `constructionScene().setActiveBody(hit.objectId)` directly
  (`forgeshape_jni.cpp:7283`). Because `sculptSession()` re-binds to the active
  body on every access (`forgeshape_scene.cpp:516`), the session would follow.
  **Isolate would incidentally mask this. That must not be mistaken for a fix.**
- **`FINDING-B` — Sculpt is reachable on a hidden body.** Neither
  `freezeToSculpt` nor `enterSculptMode` consults `visible()`, and above JNI
  visibility is read in exactly two places, both in the Objects row. Strokes
  land, the revision advances, the fingerprint moves — and nothing is drawn.

A third, unrelated: **`FINDING-C`** — `PRODUCT.md:1946-1948` and `:1955-1956`
still say selection is a tint and there is no outline. `SEL-OUT-R1` is
delivered. **Not corrected here**, because it does not prevent truthful
reporting about Stage027 and this audit's docs mandate is narrow. Flagged for
the coordinator.

## The recommendation, in one line

Build **Sculpt Isolate** as a transient, session-only, native-owned restriction
filtered **inside `ConstructionScene::snapshot()`** — so drawn and picked stay
one fact — plus the two named guards above. Build **no** new Hide. Build **no**
Mesh Preview until the OWNER says which of two concrete things it means.

Cost of the Isolate core: no new pipeline, no new shader, no new buffer, no
device feature, no upload, no `MeshRevision`, no `.forge` byte, no history step,
no fingerprint movement. A shorter snapshot is strictly cheaper than a longer
one.

The durable-visibility alternative (isolate by hiding the other bodies) is
**rejected on source grounds**: it writes project truth and the fingerprint for a
viewing choice, it needs transient state anyway in order to restore correctly,
it leaves a project isolated across a crash, and it must weaken an existing
guard to run at all.

## Six forks left to the OWNER

| Id | Question |
| --- | --- |
| `UI-1` | Isolate control in the Display popover, or in the Sculpt Property Inspector? |
| `HIDE-1` | Make durable Show/Hide reachable from Sculpt, or leave the guard standing? |
| `PREVIEW-1` | Mesh Preview = nothing (already delivered) / polygon-mode wireframe / CPU edge list? |
| `LIFE-1` | Does Isolate survive Back → Resume Sculpt? |
| `GUARD-1` | In Sculpt, does a tap that misses the mesh clear the selection or change nothing? |
| `GUARD-2` | Start/Resume Sculpt on a hidden body: refuse by name, or show the body on entry? |

## Files

| File | What it holds |
| --- | --- |
| `CURRENT_TRUTH.md` | source-by-source ownership map; the three findings |
| `VARIANTS.md` | Isolate A/B/C, Hide reconciliation, Mesh Preview M1–M4, the workflow classification table |
| `RECOMMENDED_CONTRACT.md` | one bounded contract; every fork marked `OWNER_DECISION_REQUIRED` |
| `STAGING_PLAN.md` | five slices, dependency graph, predicted files, zero-diff list |
| `TEST_PLAN.md` | evidence per behaviour, native-first, inside `TEST-OWNER-03` |

## What this run did not do

Did not implement Isolate, Hide or Mesh Preview. Did not change production
C++/JNI/Java/XML/shaders/tests. Did not add a `.forge` or `CADB` chunk or
version. Did not change `SceneObject` visibility, `ProjectHistory` or
`SculptHistory` semantics. Did not start Stage026, Stage028, `SPATIAL-R1`,
`PERF-BASELINE`, CAD, UV or booleans. Did not build, install or contact any
device. Did not claim OWNER acceptance.
