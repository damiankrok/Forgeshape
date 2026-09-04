# Home, New Project and the CAD bootstrap (`APP-H1`, delivered by `CAD-A3-C1`)

The previous pass deferred this because New CAD needs a scene with no bodies
and `activeBody()` dereferenced `bodies_.front()`. This pass took the brief's
preferred architecture — **Home is not a project; New CAD is a transient
bootstrap; the first Sketch→Extrude commit creates the first durable project**
— and made an empty scene safe at the four funnels instead of rewriting every
call site. `BOOTSTRAP_SESSION.md` has the native half; this is the shell.

## Surfaces

Three new surfaces share one scaffold, `ChooserSurfaceView` (a partial scrim,
one raised panel, an eyebrow, a headline, one line of prose, option cards, an
optional secondary chip and an own status line). The recovery question was
already this shape.

| Surface | id | Options | When |
| --- | --- | --- | --- |
| `HomeView` | `home_surface` / `home_panel` | `home_new_project`, `home_open_file` | no project open and no bootstrap in progress; never remembered, derived from `projectOpen()` |
| `NewProjectChooserView` | `new_project_chooser` / `_panel` | `new_project_cad`, `new_project_sculpt`, secondary `new_project_cancel` | from Home, or from an open project's Project surface (after the dirty guard) |
| `UnsavedChangesPromptView` | `unsaved_prompt` / `_panel` | `unsaved_save`, `unsaved_discard`, secondary `unsaved_cancel` | New Project…, Open Saved Project or Open File… over a dirty project |

The toolbar draws `back_to_home` ("Back to Home", never abbreviated) while the
CAD bootstrap is open and withdraws Export and the project control; the bottom
edge (Objects capsule, history capsule) and creation are withdrawn with no
project. The Project surface gains `project_new` ("New Project…") first.

## The phase is derived, not remembered

`EditorWorkspaceView.refreshShellPhase()` runs at the end of every
`syncFromNative()`:

```
projectOpen   = NativeViewport.projectOpen()            (scene has ≥1 body)
bootstrapping = !projectOpen && (supportChooserActive || sketch active)
atHome        = !projectOpen && !bootstrapping
Home          visible  ⇔ atHome && !recovery question
chooser       visible  ⇔ uiState.newProjectChooserOpen && !recovery && !unsaved
chromeRoot    visible  ⇔ !atHome
```

Only `newProjectChooserOpen` is Java state (an open-panel fact, carried across
the theme recreation like the precision surface). A rotation, a recreation and
a resume therefore land where native truth says: `HomeFlowTest`
`e2eAppH1_12` recreates the Activity at Home and inside the bootstrap chooser
and finds both where they were.

## Journeys

**Cold launch.** Native starts with an empty scene
(`FORGESHAPE_STARTUP_NO_PROJECT bodies=0`), the recovery question is offered
first if a validated checkpoint exists, otherwise Home. No default primitive,
no history, `encodeProject()` null, `projectFingerprint()` 0, autosave skips
(`skippedNoProjectCount`).

**New Project → CAD.** `supportChooserBegin(false)` over the empty scene: the
three world planes as spatial targets, no list-first step, no faces (nothing to
sketch on). Tap to aim, tap the same target to begin the sketch. Draw, Finish
Sketch, type a depth, Extrude → `sketchCommit` sees no project and dispatches
to `commitFirstCadProject` → one body, active, published; empty history;
status "Project created — Body 1 is its first body." Cancel Sketch goes back
to the planes; Back to Home (toolbar or System Back) from the planes goes Home
with nothing to lose (`closeProject`, which has no body to remove).

**New Project → Sculpt.** Inside `beginSessionInitialization` /
`endSessionInitialization`: `sceneAddBody`, `applyConstructionSphere` with the
diameter read back, `freezeToSculpt`. One body, Sculpt mode, 482-vertex sphere,
Construction history empty, Sculpt history empty. A refusal closes the project
again and says so.

**Open File from Home.** The same SAF contract (`requestOpenProjectDocument` →
`onOpenProjectDocumentChosen`). Cancel: Home stays, Home's own status line
says "Nothing opened…". Damaged / not-a-project / newer-version: the fail-closed
load changes nothing, Home stays, the verdict is on Home. Valid: the editor,
`noteProjectPersisted()` (an opened file is what the user has: not dirty).

**Leaving a dirty project.** `projectDirty()` = project open AND (never
persisted OR fingerprint ≠ fingerprint at last Save / Open / Open File /
Recover). `leaveProjectFor(intent)` asks when dirty:

| Answer | Effect |
| --- | --- |
| Save and continue | `saveProjectToSlot()`; on failure the project stays open, the question stays with the failure written on it; on success `noteProjectPersisted()` then continue |
| Discard changes | `ProjectCheckpoint.clear()` (the checkpoint protected exactly the discarded changes), then continue |
| Cancel — stay in this project | nothing; System Back means this too |

"Continue" for New Project closes the project immediately and opens the chooser
over Home (Cancel from the chooser lands at Home, not in a project already
decided against); for Open File and Open Saved Project the live project stays
until a file actually loads.

**System Back**, innermost outward: unsaved question → Cancel; New Project
chooser → Cancel; bootstrap sketch → the plane chooser; bootstrap chooser →
Home; a support chooser inside a project → cancelled; then the anchored
surfaces; at Home with nothing open → the platform's own Back.
`hasDismissibleSurface()` reports every one of these to the Activity's
predictive-back registration.

## What is deliberately unchanged

The Objects capsule, the palette, the sketch tools, the history capsules,
Delete, Save, Save Copy, Import GLB and Export are the controls they were, on
the project they were. `New Sketch` inside a project now lands directly in the
spatial chooser (`UI-OWNER-46`), with `sketch_plane_by_name` as the
accessibility fallback to the named planes, which are otherwise untouched.
