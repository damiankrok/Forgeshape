# CAD-V6-S1-C1-ID-LIFETIME-R1 — BEFORE

Written before any product or documentation edit, on
`feature/cad-v6-sketch-face-r1` at `dfb5091` (`origin/main = 6c9f156`,
merge-base `6c9f156`, clean tree, `git diff --check` clean). Line numbers are
at that commit. The only change beside this file is the five BEFORE checks
(`CADV6C1_IDL_B01..B05`, `forgeshape_cad_feature_selftest.cpp`), which drive
the product's own session, commit and history paths and pass on the unchanged
source.

## The question

`forgeshape_cad_body.h:310` says a `CadSketchId` is "never reused within one
state lineage"; the S1 contract asked for "never reused after allocation
inside a v6 body's lifetime". `PROJECT_STATUS.md` (S1 debt) says "after Undo
of an append the next append mints the same feature and sketch id again.
Nothing references an undone feature". This file measures which is true.

## 1. From an Add to a minted id

1. `SketchSession::commit` (`sketch_session.cpp:1643`) routes a non-New-Body
   operation to `commitIntoTarget` (`:1692`).
2. `commitIntoTarget` refuses if the body moved under the staged copy
   (`sameCadBodyState(body->state(), targetBaseState_)`, `:1702`), evaluates
   the candidate, then builds it with `candidateState()` (`:1463`).
3. `candidateState()` copies `targetBaseState_` and calls
   `appendCadLaterFeatureWithSketch` (`:1510`), which on a COPY of the state
   (`cad_body.cpp:454`) calls `addCadSketchRecord` — `record.sketchId =
   state->nextSketchId++` (`:424`) — and `appendCadLaterFeature` —
   `feature.featureId = state->nextFeatureId++` (`:446`).
4. The minted state is applied by `body->applyState(candidate)` inside ONE
   `ScopedConstructionEdit` (`:1723-1724`).

So an id is minted from the two high-water marks, which are ordinary fields of
`CadBodyState` (`cad_body.h:383`, `:393`). Nothing else mints one: the only
other writers of those fields are the codec (read) and `cadBaseSketch`'s
repair door, which never runs on a validated state.

## 2. Into the history

`ScopedConstructionEdit` → `ConstructionHistory::beginEdit` (`history.cpp:92`)
captures `captureSceneConstructionState` (`:65`), which copies each CAD body's
WHOLE `CadBodyState` (`captured.cad = cad->captureState()`, `:80`). The edit's
close, `commitEdit` (`:101`), captures again and records `{before, after}` only
when `sameSceneConstructionState` says they differ — and that comparison is
`sameCadBodyState` (`:58`), which compares `nextSketchId` and `nextFeatureId`
among everything else (`cad_body.cpp:569-572`). The high-water marks are part
of every snapshot, on both sides.

## 3. Undo

`ConstructionHistory::undo` (`history.cpp:172`) moves the entry to the redo
stack (`:181`) and `applyState(redoStack_.back().before)` (`:182`), which
restores the body's `CadBodyState` wholesale (`restoreState(wanted.cad)`,
`:290`). The marks come back with it: after Undo of an Add the body says
`nextFeatureId = 2, nextSketchId = 2` again (`CADV6C1_IDL_B01`).

## 4. Redo

`ConstructionHistory::redo` (`:186`) moves the entry back and
`applyState(undoStack_.back().after)` (`:193`): the whole forward snapshot —
feature 2, sketch 2, marks 3/3 — exactly. Redo never merges a snapshot with
the live state; it replaces the scene's history-domain state with one that
existed (`CADV6C1_IDL_B02`).

## 5. Where a new edit clears redo

`commitEdit` — and only a commit that records a step — runs
`redoStack_.clear()` (`history.cpp:132`) in the same call that pushes the new
step. A refused or no-op commit leaves redo alone (`:115-120`). So the commit
that re-mints id 2 on a new branch is the SAME call that drops the redo step
holding the abandoned id 2 (`CADV6C1_IDL_B03`).

## 6. Volatile session, UI and JNI state that holds an id across Undo

| Holder | What it holds | Can it survive an Undo? | Can it see a RE-minted id? |
| --- | --- | --- | --- |
| `SketchSession` (`editingFeatureId_`, `targetBaseState_`, `sketch_.faceSupport`) | a feature id, a staged body state, a `TopoRef` | **No.** `runHistoryStep` refuses Undo/Redo while `sketchSession().active()` (`jni.cpp:5945`), and a stale staged state is refused at commit (`sketch_session.cpp:1702`) | No |
| `SupportChooser::selected_` / `hovered_` | a `TopoRef` (producer, feature id, face, lineage) and a world frame | **Yes** — the chooser does not withdraw the history capsule (`EditorWorkspaceView.refreshHistoryControls` hides it only while sketching) and `runHistoryStep` does not ask the chooser | **No.** Only a sketch-session commit mints, every session begin cancels the chooser first (`jni.cpp:4020, 4064, 5478, 5528`), and `SupportChooser::cancel` clears both (`support_chooser.cpp:87`). A stale selection after Undo alone names a feature that no longer exists, and an Add/Cut/New Body on it is refused at commit (`FeatureSupportInvalid` / TopoRef resolution) — identical under either id model |
| Java feature list (`CadFeatureEditorView.refreshFeatures`) | a feature id per row tag | No: every Construction refresh rebuilds the rows (`syncFromNative` → `cadEditor.refreshFromNative`, `EditorWorkspaceView:2606`), and Undo/Redo end in `onNativeStateChanged` (`:2946`). A row only asks `sketchBeginEditFeature`, which resolves the id against the CURRENT state and stages an edit the user then sees — it never writes | No |
| JNI `cadFeatureInfo`, `sketchEditingFeatureId`, `cadExtrudeToolState` | read on demand | No state kept | No |
| `CadSketchId` anywhere above native | — | Not exposed through JNI at all | — |

## 7. Renderer and picking

Nothing in the renderer or `forgeshape_picking` names a feature or sketch id.
A CAD mesh's per-triangle face tags `(feature, face token)` are DERIVED, live
in the published mesh, and a restore republishes a body whose `CadBodyState`
differs (`history.cpp:286`, `:324-333`). The session's candidate evaluation is keyed by
its own revision and dies with the session.

## 8. Persisted references outside `CadBodyState`

Another body's `TopoRef` (`producerObjectId`, `producerLocalFeatureId`,
lineage) is the only one. It lives in THAT body's `CadBodyState`, which is in
the same whole-scene snapshot as its producer, so every state a history step
can land on is a scene that existed as a whole. Detached bodies held by the
history (`detached_`) come back with the step's own `CadBodyState`
(`history.cpp:290`), never with the one they left with.

## 9. Fingerprint, dirty state and saved bytes

`projectSemanticFingerprint` mixes the high-water marks ONLY outside
`cadBodyStateLegacyRepresentable` (`project_state.cpp:601-604`), and the codec
writes `CADB` v6 only there (`cadDocumentNeedsV6`, `project_document.cpp:392`).
The Java dirty test is `projectFingerprint() != persistedFingerprint`
(`EditorWorkspaceView:2303`). Measured (`CADV6C1_IDL_B05`): a saved `CADB` v1
block, Add, Undo — the fingerprint AND the encoded bytes are equal to the saved
ones again, and the file stays v1. Undo back to the saved state is clean.

## Current behaviour, measured

| Check | Result on `dfb5091` |
| --- | --- |
| `CADV6C1_IDL_B01` Undo rewinds both marks; the next feature re-mints feature 2 on sketch 2 | PASS — ids reused |
| `CADV6C1_IDL_B02` after Undo, `canRedo()`, and Redo restores exactly feature 2 / sketch 2 | PASS — the redo step holds the undone identity |
| `CADV6C1_IDL_B03` the new-branch commit clears redo; no Undo/Redo walk reaches the abandoned Add | PASS |
| `CADV6C1_IDL_B04` a cancelled session and a cancelled open edit burn no id and record no step (the edit cancel rewinds the marks and keeps the redo step) | PASS |
| `CADV6C1_IDL_B05` Undo to the saved state is byte- and fingerprint-equal, still `CADB` v1 | PASS |

## Corrections this forces

* The source comment "never reused within one state lineage" is accurate in
  substance but undefined; the S1 contract's "body lifetime" is NOT what the
  code does. One of them must become the single definition.
* `PROJECT_STATUS.md`'s "Nothing references an undone feature" is false as
  written: the redo step references it, and Redo restores it. What is true is
  that nothing references it once the next committed step has cleared redo —
  which is exactly the step that can re-mint its id.
* Observation, not an id-lifetime defect: a support-chooser selection survives
  an Undo and is not re-resolved at confirm (`confirmChosenSupportLocked` →
  `beginOnFace` trusts it). Its worst case is a staged sketch framed on a face
  that has moved or gone; every commit re-validates the support, so it cannot
  land on the wrong face, and it cannot observe a re-minted id (section 6).
  Out of scope here; recorded for the owner.
