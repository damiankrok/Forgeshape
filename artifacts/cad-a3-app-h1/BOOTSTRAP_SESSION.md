# The CAD bootstrap and the no-project scene (native half of `APP-H1`)

## Why not an optional-body rewrite

`ConstructionScene::activeBody()` returns a reference and is read, through four
process-scoped accessors, from roughly seventy JNI sites. The brief forbade
weakening every one of them to show a plane chooser, and it was not necessary:
every read of "the active body" from the shell goes through one of four
funnels, and the four can answer for "no project" themselves.

| Funnel | Answer with no project |
| --- | --- |
| `activeConstructionOrNull()` | `nullptr` — every caller already handles a body with no Construction Source (an Imported Mesh) |
| `meshStore()` | a process-static unbound `MeshStore(kNoObject)` nothing draws from |
| `constructionTransform()` | a process-static unbound identity placement; the edit entry point refuses before writing to it |
| `sculptSession()` | `bindTarget(nullptr)` — the session's own `unbound_` target already reports no mesh, no edits, no history |

The direct `activeBody()` readers left in JNI — `freezeToSculpt`,
`sceneActiveBodyIsImported/IsCad/IsFaceSupportedCad`, `cadState`, the CAD
Apply, the state log lines and the primitive-refusal log — ask `hasProject()`
first and refuse or answer "no" by name (`NoProject`).

`activeBody()` itself, on an empty scene, no longer dereferences an empty
vector: it answers a process-static null object (`SceneObject(kNoObject)`, in
no scene, never published, saved, picked or edited) and increments
`activeBodyMisuseCount()`. That counter is exposed as
`debugActiveBodyMisuseCount()` and `HomeFlowTest` asserts on every journey that
it did not move. It moved zero times on the device. The native suite proves
the counter itself works (`CADA3_BOOT_02`) and that the whole bootstrap never
touches it (`CADA3_BOOT_11`).

Three self-tests had read the process-global store or Source as "the startup
Box" — the picking, mesh and construction suites, and one sculpt sidedness case
— and would have crashed or lied against an empty process scene. Each now
builds its own `MeshStore` / `ConstructionScene`, which is the rule `CLAUDE.md`
already stated for suites; the process-global 8-argument `pickScene` overload
they used lost its last caller and was removed.

## The scene

```
ConstructionScene()              a project scene: one default Box, selected   (tests, loads)
ConstructionScene(NoProjectTag)  the no-project scene: empty                  (the process scene)
hasProject()                     bodies_.size() > 0  — THE answer, no second flag
closeProject()                   destroy every body, select nothing; allocator NOT rolled back
activeBodyMisuseCount()          reads of activeBody() with no project, process-wide
```

The allocator is not rolled back on close: ids stay unique for the life of the
process, so a renderer resource keyed by an old project's `ObjectId` can never
be mistaken for a new project's body. The first body of the next project wears
whatever the allocator hands out next; a loaded document's ids are its own.

## `commitFirstCadProject` (`forgeshape_project_bootstrap.{h,cpp}`)

```
preconditions   no project open           → NotSketching (the ordinary commit is the path)
                session Ready             → NotSketching
                no Construction edit open → RefusedEditInProgress
                a profile chosen          → AmbiguousProfile / ProfileNotFound
candidate       validateCadBodyState(candidateState())  → the state's own CadStatus
document        kind Construction; SCNE {nextObjectId = id+1, active = id, [id @ identity]};
                CADB {id, state}   — one world-plane CAD body
commit          loadProjectDocument(document, scene, session, history)
                  → validates again, builds and regenerates off the scene, swaps in one step,
                    binds the session, enters Construction, history.clear()
on success      sketch.cancel(); report {bodyId, revision}
on any refusal  NOTHING changes: the scene stays empty, the sketch stays in Ready,
                the reason is in sketch.lastStatus()
```

The document is never encoded: it exists to travel through the one validated
replace-the-scene path Open and Recover already take. That is what makes the
first commit atomic and fail-closed without a second implementation of either
property.

The new project's history is empty — the same postcondition every loaded
document has. `ARCHITECTURE.md` records why: undoing the creation of the only
body would give an empty project, which does not exist. The Sculpt bootstrap
keeps its empty history the way it always did, through the
session-initialization bracket.

## JNI

| Native | Behaviour |
| --- | --- |
| `projectOpen()` | `hasProject()` |
| `closeProject()` | cancels the chooser, the sketch and its borrowed view, drops any stroke, leaves Sculpt, withdraws the gizmo, clears the history, `scene.closeProject()`; writes nothing; logs `FORGESHAPE_PROJECT_CLOSED bodies=N` |
| `sketchCommit()` | no project → `commitFirstCadProject`, then `FORGESHAPE_FIRST_PROJECT_CREATED objectId=… bodies=1 undo=0`; otherwise the ordinary commit |
| `sketchBegin(plane)` | on success also cancels a support chooser left open, so a by-name plane chosen over the spatial chooser hands the gesture to the sketch |
| `encodeProject()` | null with `FORGESHAPE_PROJECT_ENCODE_SKIPPED:no_project` |
| `projectFingerprint()` | 0 |
| `debugActiveBodyMisuseCount()` | the counter above |
| `start()` | logs `FORGESHAPE_STARTUP_NO_PROJECT bodies=0`; publishes nothing; the post-self-test republish is a no-op |

## Native evidence

`CADA3_BOOT_01..13` in the CAD-A3 suite (`NATIVE_SELFTEST_CADA3.txt`): the
empty scene; the counted null object; no document to write; a real touch-driven
bootstrap sketch over an empty scene reaching Ready; the first commit making
exactly one durable CAD body with an empty history and no sketch; the first
project being an ordinary world-only CAD document; a second first-commit refused
once a project is open; cancel leaving no project and no minted id; an invalid
profile creating nothing; the whole bootstrap never reading an active body;
`closeProject` keeping the allocator monotonic; and the seeded first body
inside the bracket recording nothing.
