# ARCHITECTURE — SCULPT-UNDO-R0 (`ARCH-OWNER-12`)

## The two histories

ForgeShape now has two, and they never merge.

| | `ConstructionHistory` | `SculptHistory` |
| --- | --- | --- |
| Owns | project/object truth: parameters, placements, creation, import, delete | completed sculpt strokes |
| Scope | one per process | one per BODY |
| Lives in | `forgeshape_history.{h,cpp}` | `forgeshape_sculpt_history.{h,cpp}`, held by `FrozenSculpt` |
| A step holds | bounded Construction-domain state, never a mesh byte | a position delta, never a parameter or a document |
| Serialized | no (session history) | **no** (and no schema change was made for it) |
| Bounded by | ~60 steps | 32 entries AND 4 MiB per body, 1 MiB per entry |
| Reachable from | Construction mode only | Sculpt mode only |

There is no combined timeline, and neither history is ever consulted for the
other.

## Why a second history rather than a wider first one

A `ConstructionHistory` step holds bounded Construction-domain state and never
one mesh byte — the rule that makes undo cheap and makes a step something the
domain can rebuild. A sculpt stroke's entire effect IS mesh bytes. Putting one in
a step would have inverted that rule; the owner's boundary keeps it and adds a
second mechanism beside it.

## Ownership is the mechanism

`SculptHistory` lives inside `FrozenSculpt`, beside the `SculptMesh` it
describes. That single decision supplies most of the required behaviour with no
code:

- **Per body, with no key.** No registry, no map, no "current sculpt stack".
- **Body switching switches history.** `sculptSession()` rebinds to the active
  body's `FrozenSculpt` on every call, so body A's Undo is *structurally
  incapable* of reaching body B.
- **Back / Resume keep it.** Leaving Sculpt is navigation; it takes nothing off
  the body.
- **Delete disposes of it correctly.** The body leaves whole and is HELD by
  `ConstructionHistory::holdDetachedBody`; undoing the Delete restores the SAME
  object, so its stacks come back with it. When no step names the body any more
  it is released and the history dies with it. Nothing was serialized and
  nothing was added to a history step to achieve that.

## The stroke transaction

See `STROKE_TRANSACTION.md`. One completed stroke is one entry, recorded once in
`SculptSession::recordActiveStroke`, reached from both `endStroke` and
`cancelStroke`.

## The entry

```
SculptStrokeDelta
    vector<uint32_t> vertexIndices     sorted, unique, strictly increasing
    vector<Vec3>     beforePositions   parallel
    vector<Vec3>     afterPositions    parallel
    bool             beforeHasEdits
    bool             afterHasEdits
```

Deliberately absent, and why:

| Absent | Why |
| --- | --- |
| normals | derived from positions by one producer (`SculptMesh`'s cache); a stored copy could only disagree |
| a `ProjectDocument` | an undo is not a load |
| Construction parameters, `PrimitiveKind` | a stroke never changed one |
| the `ImportedMesh` | immutable source truth; a stroke never touched it |
| the body's `ConstructionTransform` | a stroke never moved a placement |
| the `ObjectId` allocator | no identity is created or destroyed |
| GPU buffers, `.forge` bytes, camera/UI state | not what a stroke changed |

A malformed delta — unsorted, repeated index, ragged vectors, non-finite
position — is REFUSED by `SculptHistory::record` rather than stored and applied
later.

## Deltas, not snapshots

The affected set is captured at pointer-down and fixed for the stroke's life, so
an entry is bounded by the BRUSH, not by the mesh. A small brush on a
million-vertex import costs a few hundred vertices; a snapshot would cost twelve
megabytes for the same stroke. No snapshot fallback was needed and none was
implemented.

## `hasEdits` stopped deriving from the revision

It was `revision > kFrozenSculptRevision`. Sculpt Undo forced the two apart,
because a revision must stay monotonic and "has edits" must be able to go back.
It is now an explicit flag on `SculptMesh`:

- cleared by every `freezeFrom`
- set by `advanceRevision()` — which runs only when a stroke moved a vertex, so
  every pre-existing behaviour is preserved exactly
- restored by `restoreEditedFlag()`, whose ONE caller is
  `SculptSession::applyHistorySide`

The case a depth counter cannot answer: a project loaded with an already-edited
sculpt mesh starts with an EMPTY history and must still report edits, so undoing
the one new stroke taken since must land on `true` — while undoing the first
stroke after a fresh Freeze must land on `false`. Only the value each entry
captured at its own stroke's start answers both.
(`SCUNDO_11_*`, `SCUNDO_12_*`.)

`.forge` already stored `hasEdits` as a boolean, so nothing about the format
changed; what changed is that `loadProjectDocument`'s existing single
`advanceRevision()` now sets a flag rather than being the fact.

## Revisions stay monotonic

`applyHistorySide` advances the revision and THEN restores the edited flag. The
renderer, CPU picking and `projectSemanticFingerprint` all notice a sculpt change
by that number, so an Undo that rewound it would be invisible to precisely the
caches that must see it — including autosave, which is why a Sculpt Undo is
correctly checkpointed. Geometry goes backwards; the counter only goes forwards.

## The apply lives in the session, not the history

`forgeshape_sculpt_history.h` includes `forgeshape_math.h` and nothing else from
the domain. `SculptHistory` is a bounded stack: it decides what is retained and
never writes a vertex. `SculptSession::undoStroke`/`redoStroke` read the top
entry, apply it, then commit the move — so there is exactly one place history can
write a sculpt vertex, and it is the same class that owns the stroke that wrote
it first. It also makes the history testable with no mesh at all, which is how
the bounds checks are driven.

## Threading

Every history mutation and every step runs under `g_stateMutex` — the same lock a
stroke's own position writes take (the whole touch arbitration is inside it), the
same lock the render thread's whole-scene snapshot takes, and the same lock the
autosave worker takes for the fingerprint and the encode. So a step cannot race
autosave capture, renderer publication, input stroke updates, body deletion or a
Back/Resume transition.

Publication goes through the ordinary `publishSculptRepresentation`, so a history
step reaches the renderer exactly the way a stroke does. There is no second path
for sculpt geometry to become a frame.

## Source immutability

A history step never touches a Construction Source, an Imported Mesh's arrays, a
body's placement, an `ObjectId`, or the Construction history. Asserted in the
domain (`SCUNDO_09_*`, `SCUNDO_10_*`) and on the device against the `.forge`
document's own `IMPT` bytes (`E2E-SCUNDO-02`).

## JNI surface

| Family | Meaning | In Sculpt |
| --- | --- | --- |
| `constructionUndo` / `constructionRedo` / `...Available` / `...Depth` | ALWAYS the Construction history | **refused** (`HISTORY_REFUSED_IN_SCULPT`) — the guard did not move |
| `sculptUndo` / `sculptRedo` / `...Available` / `...Depth` / `sculptHistoryBytes` / `...NotRetainedCount` / `...EvictedCount` | ALWAYS the active body's Sculpt history | the subject |
| `historyUndo` / `historyRedo` / `...Available` | what the CHROME calls; dispatches on product mode | means the Sculpt history |

The dispatch is native because the mode is native. Letting Java choose which
history a button means would put the one decision that must never be wrong in the
layer that holds no state to decide it with. The mode is re-read under the step's
own lock rather than passed in, so a mode change between the availability read
and the tap cannot route a step to the history the user has left.

Status codes: `HISTORY_OK`, `HISTORY_NOTHING_TO_DO`,
`HISTORY_REFUSED_IN_SCULPT`, `HISTORY_STROKE_ACTIVE`, `HISTORY_UNAVAILABLE`,
`HISTORY_ENTRY_NOT_RETAINED`. Nothing is overloaded onto a generic or
"not finite" status.

## Files

| File | Change |
| --- | --- |
| `forgeshape_sculpt_history.h/.cpp` | **new.** The delta, the bounds, the bounded stack. |
| `forgeshape_sculpt.h/.cpp` | `SculptHistory` on `FrozenSculpt`; explicit `hasEdits_`; `SculptStroke::buildDelta` and `beganWithEdits_`; `SculptSession::undoStroke`/`redoStroke`/`canUndoSculpt`/`canRedoSculpt`/`recordActiveStroke`/`applyHistorySide`; `freezeToSculpt` clears the history; the three mode transitions route their cancel through `cancelStroke` so a live stroke is recorded. |
| `forgeshape_jni.cpp` | three new status codes, `runSculptHistoryStep`, and the `sculpt*` and `history*` entry points. |
| `forgeshape_sculpt_selftest.cpp` | `runSculptUndoChecks` — 84 checks. |
| `CMakeLists.txt` | the new translation unit. |
| `NativeViewport.java` | the new constants and native declarations. |
| `EditorWorkspaceView.java` | the pair is drawn in both modes; `historyUndo`/`historyRedo`; the settled-gesture refresh; the unretained-stroke report. |
| `strings.xml` | two new status strings. |
| `SculptUndoTest.java` | **new.** 10 device cases. |
| `WorkspaceTestSupport.java` | `sculptTheViewport` dispatches through the real `SurfaceView` (see `TEST_RESULTS.md`). |
| `EditorWorkspaceHistoryTest`, `ImportedMeshSculptTest` | one assertion each: the pair is VISIBLE in Sculpt. Both still assert the Construction refusal. |
