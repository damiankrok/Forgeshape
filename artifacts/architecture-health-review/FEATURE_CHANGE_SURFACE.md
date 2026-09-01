# Feature change surface — four probes (2026-09-01)

Each probe lists the files a hypothetical stage would have to touch, judged
from the tree at baseline `4c296f5`. Nothing here was implemented.

## Probe 1 — Imported Mesh Sculpt (`IMPORT-01B`)

- `forgeshape_sculpt.{h,cpp}`: `SculptMesh::freezeFrom` takes a
  `ConstructionMesh`; an Imported Mesh has positions/normals/indices in its
  own arrays (`forgeshape_imported_mesh.h`), so a second `freezeFrom`
  overload or a shared "source triangle mesh" view is needed.
- `forgeshape_scene.{h,cpp}`: `FrozenSculpt` already lives on every body;
  `staleSource` semantics for a body with no Construction Source must be
  defined (there is no shape to be stale against; the reset source is the
  Imported Mesh itself, so the "Reset Sculpt from Shape…" wording has no
  meaning there).
- `forgeshape_jni.cpp`: the guards that refuse `Start Sculpting` for an
  imported body, and the Java withdrawals in `EditorWorkspaceView`,
  `GlobalToolbarView` and `ToolRailView`.
- `forgeshape_project_state.cpp` + `DATA_PACKAGE_SPEC.md`: SCLP today is keyed
  to CONS bodies; a Frozen Sculpt Mesh on an IMPT body needs the writer and
  the validator to allow it. That is a `.forge` decision and the version
  question is the coordinator's.
- Tests: `forgeshape_sculpt_selftest.cpp`, `forgeshape_project_selftest.cpp`,
  `ImportedMeshDurableTest`, `EditorWorkspaceSculptRetentionTest`.
- Verdict: about 8 files, no architectural obstacle. The body-level
  `FrozenSculpt` and the pointer-returning Construction accessors were the
  enabling moves and are in place.

## Probe 2 — Another static importer (e.g. OBJ) — prohibited today

- `forgeshape_gltf_import.{h,cpp}` produce `ImportedMesh` geometry; the commit
  path (`forgeshape_import_commit.cpp`) is format-agnostic: it takes built
  meshes. A second reader would add one file and one JNI entry, reuse the
  commit path unchanged, and add one Project-surface row.
- Verdict: the reader/commit split is right; the change surface is about 4
  files. Not to be built (CLAUDE.md: OBJ/FBX absent in both directions).

## Probe 3 — Per-object visibility

- `SceneObject` (a `visible` flag), `SceneSnapshot` (skip or flag the item),
  `Renderer` (skip draw), `forgeshape_picking.cpp` (skip hit), history
  (`BodyConstructionState` gains the flag so Undo restores it), project_state
  + `DATA_PACKAGE_SPEC.md` (SCNE per-body field, a `.forge` change), export
  (decide whether hidden bodies export; CLAUDE.md says EVERY body exports),
  Objects list UI + `ids.xml`.
- Verdict: about 10 files. The cost is that visibility is scene truth, so it
  lands in history and `.forge`. No shortcut keeps it out of the document
  without making it session-only.

## Probe 4 — A third body representation

- Files with representation dispatch: `forgeshape_scene.{h,cpp}`
  (`BodyRepresentation`, `SceneObject` storage, `publishSceneObject`),
  `history.cpp:58`, `project_state.cpp:113/124/403/405`,
  `gltf_export.cpp:213/236`, `glb_roundtrip.cpp:45/58`, `jni.cpp` (6 sites),
  `project_document.{h,cpp}` (a new section plus the both/neither refusal),
  `DATA_PACKAGE_SPEC.md`, and the Java withdrawals keyed on
  `sceneActiveBodyIsImported()` (a boolean that would have to become a
  representation query).
- Verdict: about 12 files. The first payment is a shared "interchange
  geometry of a body" function used by export and roundtrip (HOTSPOTS §B),
  and turning the Java boolean into a representation enum. Scored 3 in the
  scorecard.
