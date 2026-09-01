# Hotspots — ForgeShape architecture health review (2026-09-01)

Every required hotspot category, what was looked at, and the verdict. Line
numbers are at baseline `4c296f5` unless marked (post-fix).

## A. Null dereferences and unconditional `construction()`

- `SceneObject::constructionOrNull()` / `activeConstructionOrNull()` are the
  only Construction accessors; both return pointers. No reference-returning
  `construction()` exists (grep: 0 hits).
- Every call site checked: `forgeshape_jni.cpp` (6 sites), `history.cpp:58`,
  `project_state.cpp:113/403`, `gltf_export.cpp:213/236`,
  `glb_roundtrip.cpp:45/58`, `construction.cpp` (1), `scene.cpp:179`. Each
  handles `nullptr` explicitly (refuse with a named status, skip the body, or
  return `kNoMeshRevision`).
- `ConstructionScene::activeBody()` (scene.cpp:63–75) returns a reference with
  a `bodies_.front()` fallback. Bodies are never deleted (only detached by
  history, which immediately re-points `activeBodyId_`), so `bodies_` is never
  empty after construction. **Verdict: no reachable null deref.**

## B. Duplicated representation switches

Dispatch on Construction-vs-Imported exists at: `scene.cpp:publishSceneObject`,
`history.cpp:58`, `project_state.cpp:113/124/403/405`, `gltf_export.cpp:213/236`,
`glb_roundtrip.cpp:45/58`, `jni.cpp` (6), plus document validation. Two of
them do the SAME work twice: `gltf_export.cpp:195–250` and
`glb_roundtrip.cpp:30–75` each re-evaluate a Construction Source or read an
Imported Mesh's arrays into positions/normals/indices. **Verdict: real
duplication, bounded (2 sites), not corrected** — a shared "geometry of a body
for interchange" function would be a refactor that touches the exporter (out
of policy). Recorded in `FEATURE_CHANGE_SURFACE.md` as the first thing a third
representation would pay for.

## C. Duplicate transform / validation logic

- Euler↔matrix: one bridge (`forgeshape_transform.h`), used by gizmo, export
  and history. No second decomposition (grep for `atan2` outside
  transform.cpp/selftests: 0).
- Scale/translation validation: `validateTransformValue`/`validateScaleValue`
  in transform.cpp, reused by `transformValid` in project_document.cpp. One
  rule, two callers.
- Imported translation: `import_commit` checks `isfinite` on the float
  translation; rotation 0 / scale 1 always pass `transformValid`. **No
  fail-closed hole between import and save.**

## D. Active-object assumptions

`meshStore()`, `sculptSession()`, `constructionTransform()`,
`activeConstructionOrNull()` all resolve "the active body" on every call.
`sculptSession()` rebinds the target each access (scene.cpp:225–229), which is
what prevents the stale-body-pointer bug. The gizmo and picking read the scene,
not the accessors. **Verdict: sound; the single-session sculpt design is a
deliberate product contract (shared Radius/Strength).**

## E. ObjectId lifecycle

- Monotonic `nextObjectId_`; `makeBody`/`reserveObjectIdsThrough` push forward
  only; an undone creation's id is restored by name (history keeps the detached
  body). Import mints nothing on refusal (`addImportedBody` validates first).
- `.forge` decoder requires `nextObjectId > highest` and no duplicates
  (document.cpp:325–352). It does NOT bound `nextObjectId` below
  `kFirstPreviewRenderKey` (2^60): a hand-crafted file could push the allocator
  into the preview key range, after which the diagnostic preview and a real
  body could share a renderer key. Reachable only from a crafted file and only
  with the diagnostic preview driven from a test; not a user path. **Not
  changed** (it is a `.forge` validation rule — reported per the mission's
  data-contract instruction).

## F. Persistence skipping a representation

`captureProjectState` (project_state.cpp:113–130) writes CONS for Construction
bodies and IMPT for Imported ones, and `validateProjectDocument` refuses a body
named by both or neither. `projectSemanticFingerprint` hashes the imported
name, counts and batch flags, so an import moves the fingerprint and autosave
sees it. **Verdict: no representation is skipped.** Verified at runtime by
`ImportedMeshDurableTest` (12/12).

## G. Stale preview comments

None stale: preview files describe themselves as diagnostic with no UI
control, matching `PROJECT_STATUS.md` and the Java surface (no
`Show`/`Clear` rows exist). See `COMMENT_AUDIT.md` for the six comments that
WERE stale (none about the preview).

## H. JNI status mismatches

All status enums returned across JNI (`PrimitiveApplyStatus`,
`TransformUpdateStatus`, `ProjectCodecStatus`, import/export statuses) were
compared value-by-value against the `NativeViewport` constants (lines 83–1348).
92 JNIEXPORT vs 91 `static native`: the extra native symbol is `JNI_OnLoad`.
**Verdict: no mismatch.**

## I. UI label branching

`EditorWorkspaceView` withdraws `Shape`/`Start Sculpting` from
`sceneActiveBodyIsImported()` (native truth), not from a label or a Java-side
cache (lines 1595–1640, 1790–1802). Tests locate controls by `R.id` only:
0 `withText`, 0 coordinate taps, 0 `Thread.sleep` across `androidTest`.

## J. Test helpers masking defects

`WorkspaceTestSupport` waits on `awaitIdle` barriers, never sleeps, and its
helpers assert rather than swallow. No `catch (Throwable) {}` in test code.

## K. Shared mutable state across threads — one defect found and fixed

- `applyPrimitive` (jni.cpp:768) mutated Construction parameters, the
  remembered sets and the stale flag with NO lock, while the autosave worker
  reads exactly those under `g_stateMutex` in `projectFingerprint()` /
  `encodeProject()`. `applyBoxTransform` next to it already locked. **Fixed:**
  the lock is now taken before the edit scope (see `SAFE_FIXES.md` #1).
- `applyBoxTransform` reads `updateCount()` outside the lock for the log line
  only (jni.cpp:912). Benign (a counter for a log), left alone.
- `sceneAddBody` publishes outside the lock; analysed and found safe (only the
  new body's own `MeshStore`, which has its own mutex, changes). The comment
  claiming it was the universal rule was wrong and was corrected.

## L. Renderer ownership

`Renderer::bodies_` (renderer.h:351) caches `BodyRenderResources` per
`ObjectId` and is cleared only by `destroyMeshResources` (device loss /
teardown). A body detached by Undo leaves its GPU buffers resident until the
process ends or the device is rebuilt. Bounded (history capacity 64, bodies are
never deleted otherwise, buffers are small) and never drawn (the frame iterates
the snapshot, not the map). **Not changed** — renderer changes are out of
policy; recorded as debt with timing LATER.

## M. Feature magnet

`EditorWorkspaceView.java` is 3427 lines / 113 methods and hosts the
Objects, Transform, Sculpt, Project and Display contexts' wiring. It is the
one file every UI-touching stage edits. Not a correctness defect; noted in the
scorecard (Feature isolation) with timing BEFORE_NEXT_MAJOR_FEATURE.
