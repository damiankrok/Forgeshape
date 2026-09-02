# UI-OWNER-45 — Delete removes a real project object

## Where it lives

`forgeshape_body_delete.{h,cpp}`, a small module of its own for the reason
`forgeshape_import_commit` is one: deleting a body is a decision ABOUT the
project that needs both the scene and the history, and neither of those owns the
other. The scene knows how to detach a body and nothing about transactions; the
history knows how to record one and nothing about which body should be selected
afterwards.

```
DeleteBodyStatus deleteSceneBody(ObjectId, ConstructionScene&,
                                 ConstructionHistory&, DeleteBodyReport* = nullptr);
```

## Representation-neutral, and that is the design

Nothing in it asks what a body is. A Construction Body, an Imported Mesh, and
either of them carrying a retained Frozen Sculpt Mesh are removed by the same
three lines, because a body is removed as a WHOLE OBJECT: its identity, its
representation, its placement, its published mesh and its sculpt state leave
together and come back together. A per-representation delete path is exactly how
one of them would eventually come back missing something.

## The body is held, not destroyed

`ConstructionHistory::holdDetachedBody` takes ownership. This is not an
optimization: an Imported Mesh's geometry and any Frozen Sculpt Mesh are not
derived from anything a history step holds, so a step could not rebuild them and
an undo that had to would come back with an empty object wearing the right id.

`pruneDetachedBodies` was widened to match. It now asks **both stacks** and
**both sides** of every step:

| act | what restores the body | which side names it |
| --- | --- | --- |
| undo a creation, then redo | `redo()` | the step's AFTER state |
| delete, then undo | `undo()` | the step's BEFORE state |

Asking only the forward side was correct while an undone creation was the only
way a body could leave the scene. A body no step in either stack names on either
side can never come back and is released — so the retention is still bounded by
`kConstructionHistoryCapacity`.

## One transaction

`ScopedConstructionEdit`, exactly like every other act — and always as the scope
that OWNS the edit. An edit already in progress is refused outright
(`RefusedEditInProgress`), which is the rule `loadProjectDocument` and
`commitImportedGlbScene` already apply; nothing in the product wraps a delete in
a larger act, and refusing rather than joining keeps this operation's transaction
boundary unambiguous.

`commitEdit` compares the scene's Construction state on either side, so the step
is recorded because the body list genuinely differs — not because a call was
made.

## Selection

Resolved from the list as it is BEFORE the detach, because the rule is about the
row that takes the deleted row's place:

- deleted body was **not** active → the active body does not change;
- deleted body **was** active → the NEXT body in scene order;
- deleted body was active and **last** → the one before it.

`detachBody`'s own fallback puts the selection on the first body; the product rule
is stated in `deleteSceneBody`, inside the transaction, so the step carries it and
an Undo puts the original selection back.

## The last body is refused, by name

`DeleteBodyStatus::RefusedLastBody`. This product has **no empty project**, and
that is not a UI opinion — it is four independent structural facts:

- `ConstructionScene()` creates a body eagerly;
- `activeBody()` returns a REFERENCE, and `meshStore()`, `sculptSession()` and
  `activeConstructionOrNull()` are all defined in terms of it;
- `validateProjectDocument` refuses a document with zero bodies
  (`ImpossibleCount`);
- every `.forge` load requires an active body the scene carries.

So the refusal is the honest answer, and `UI-OWNER-45` authorizes exactly it. No
replacement primitive is ever invented — that would be creating an object the
user did not ask for in order to satisfy a delete they did.

The row for the last body draws no Delete at all, and the guard remains below JNI
regardless.

## Refused while sculpting

`kDeleteRefusedInSculpt`, in `forgeshape_jni.cpp`, on the same terms body
switching (`sceneSelectBody`) and Undo/Redo already follow: the Sculpt target is
fixed for the duration of Sculpt mode, and Undo is refused there — so a delete
made in Sculpt could not be taken back until the user left the mode. Every row
withdraws the control while sculpting, and it returns the moment Sculpt is left.

**This is a deviation from a literal reading of Part E case 5** ("Delete
currently active/sculpting body"), and it is recorded as one. What is tested
instead is deleting the currently ACTIVE body, including one that has retained
sculpt work and was being sculpted a moment earlier — which is well-defined,
undoable, and consistent with every neighbouring rule in the product.

## The ObjectId allocator is never rolled back

A deleted body's id comes back BY NAME through its Undo, and the next creation
mints a fresh one. Reuse is what would let a stale `ObjectId` held in a selection
or a render snapshot silently resolve to a different body. Asserted by
`IMP01B_15_a_delete_does_not_roll_the_allocator_back` and
`IMP01B_15_and_the_next_creation_does_not_reuse_the_deleted_id`.

## Renderer resources

`Renderer::releaseBodiesAbsentFromScene`, at the top of `syncScene`.

ARCH-HEALTH-01 recorded the retention as **bounded** debt with timing LATER, and
that verdict was correct at the time: a body could only leave the scene by having
its creation undone, the history holds at most 64 of those, and each entry is a
handful of kilobytes. **Delete removes the bound.** Add and delete in a loop and
every cycle mints a fresh `ObjectId`, because the allocator is deliberately
monotonic, so `bodies_` would grow one entry per cycle for the life of the
process — and each entry holds two `VkDeviceMemory` allocations against
`maxMemoryAllocationCount`, a hard device limit commonly around 4096.

The prune is the smallest thing that fixes it:

- it runs only over a map with a handful of entries, and does nothing at all when
  every entry is still in the snapshot;
- it waits on every in-flight frame (`waitForMeshBuffersIdle`, the same wait a
  capacity grow already performs) ONCE, and only when something is actually going
  to be destroyed;
- if the wait fails it leaves everything resident rather than freeing unsafely.

**Undo republishes.** The body comes back with the same `ObjectId` and the same
published `MeshRevision`; a fresh `BodyRenderResources` starts at
`kNoMeshRevision`, so `syncBody`'s revision gate misses and uploads it again —
the same path a body already takes the first time it is drawn. No renderer
lifecycle was redesigned and no other renderer behaviour was touched.

One consequence, documented rather than hidden: the diagnostic imported preview
REPLACES the scene snapshot rather than joining it, so while a preview is on
screen the project's own bodies are released here and re-uploaded when it is
cleared. That is correct by the same route Undo takes, and the preview is
debug-only and reachable from nothing in the product.

## The UI

A row is a **pair**: the label carries `object_row`, the ObjectId tag and the
activated state, and selects; the Delete beside it carries `object_row_delete`,
the same tag, the product's existing `fsTextError` colour and the ordinary 48 dp
icon-button hit area, and removes. Two targets, never one gesture with two
meanings. The pair is a plain container with no id, so every existing caller that
reaches a row still gets the label.

**Unconfirmed, deliberately.** The product confirms exactly one act — *Reset
Sculpt from Shape* — and confirms it because it genuinely cannot be undone. A
Delete is one Undo away, the history capsule is on the same screen, and the status
line says `Deleted <name> — Undo restores it.` A dialog in front of a reversible
act is what trains a user to dismiss the dialog in front of the irreversible one.

`deleteControlFor` reports the control a row OFFERS, so the two withdrawals — the
last body's row, which never builds one, and every row while sculpting, which
hides it — are one answer, because they mean the same thing to a user.

### The bounded wait, and the defect that produced it

The first authoritative aggregate **FAILED**, and the failure was mine. It is
recorded here rather than quietly fixed, because the mechanism is the useful
part.

`ProjectAutosaveRecoveryTest.fsr1b08_aCorruptCandidateIsQuarantinedAndNeverOfferedAgain`
failed in `ActivityScenarioRule.after()`, not on a product assertion:

```
java.lang.AssertionError: Activity never becomes requested state "[DESTROYED]"
        (last lifecycle transition = "PAUSED")
```

The class had passed 14/14 standalone twice on the same code, so it was not
deterministic — but there was a real mechanism behind it, and it was one this
stage introduced:

1. `releaseBodiesAbsentFromScene` called `waitForMeshBuffersIdle()`, which waits
   on **every** in-flight fence with `UINT64_MAX`.
2. `drawFrame` resets the current frame's fence immediately before
   `vkQueueSubmit`. If that submit fails, the fence is left **reset with nothing
   left to signal it** — `handleFrameResult` may then return true and the loop
   continues.
3. An unbounded wait on that fence never returns. The render thread hangs;
   `surfaceDestroyed` blocks until the render thread releases the
   `ANativeWindow`; the Activity never leaves PAUSED; `ActivityScenario.close()`
   times out. Exactly the observed symptom.

The latent condition pre-existed. What this stage changed is **how often the wait
is reached**: before, only from a capacity grow, which is rare; after, from every
frame on which a body leaves the snapshot — a delete, an undo, and **every
`.forge` load that changes ObjectIds**, which is what this suite does
constantly.

**The fix**: `waitForMeshBuffersIdle` gained an optional
`timeoutNanoseconds`, defaulting to `UINT64_MAX`. The capacity grow — which MUST
complete before it reallocates — keeps the default and is bit-for-bit
unaffected, because with no deadline `VK_TIMEOUT` cannot occur. The prune passes
`kMeshReleaseWaitNanoseconds` (100 ms) and, on anything but success, leaves every
entry resident and asks again next frame. It is opportunistic and has infinitely
many retries, so it never needs to stake the render thread on a fence.

100 ms is a ceiling and not a budget: it is far above a ~16 ms frame, so in the
steady state the release still lands on the frame the body left, and pruning
only happens on a user-initiated event in the first place.

Verified after the fix: 17/17 native suites clean, `ProjectAutosaveRecoveryTest`
14/14, and the aggregate rerun from shard 1 on the corrected tree.

### The second aggregate failure — a test that assumed process-scoped state

The second authoritative aggregate also FAILED, and this time the product was
right and **the new test was wrong**:

```
1) imp01b16and17_anImportedBodyWithSculptWorkDeletesAndReturnsWhole
java.lang.AssertionError: IMP01B-17: and no orphan SCUL record either
```

The case asserted that after deleting an imported body with retained sculpt
work, the document carries **no `SCUL` section at all**. That is an assertion
about every OTHER body in the scene, and the scene is process-scoped:
`ObjectsDeleteTest` shares a process with six other classes in that shard, and a
body one of them sculpted legitimately still carries a Frozen Sculpt Mesh. The
case passed standalone — where nothing else had sculpted anything — and failed
beside its shard-mates. `PROJECT_STATUS.md` names this trap in as many words: no
instrumented test may assume a body count, which body sits at the origin, or
which mode is current.

**The fix asks the rule instead of the section.** `validateProjectDocument`
refuses a `CONS`, `IMPT` or `SCUL` entry naming a body `SCNE` does not carry
(`UnresolvedReference`), so a document that still passes `validateProject`
provably carries no orphan record **for any body** — which is what IMP01B-17 is
about, and is a stronger statement than a section check could make. Beside it the
case now asserts that the deleted id is gone from the scene, that the document
really changed, and that it got smaller.

Two further weaknesses of the same family were found by inspection while fixing
it, and fixed:

- `importedVertexCountFromProject()` read the FIRST `IMPT` entry, which assumes
  the body under test is the only imported one. It now walks the entries and
  matches the `ObjectId`.
- one assertion had degenerated into a tautology (`x == x`) through an earlier
  rename, and was passing vacuously. It now asserts what that step is actually
  about: that leaving Sculpt KEPT the retained mesh.

Before rerunning the aggregate, shard 3's exact seven-class composition was
replayed through the runner as a focused subset — **92/92 OK** — so the fix was
proved against the condition that caught it rather than only in isolation.
