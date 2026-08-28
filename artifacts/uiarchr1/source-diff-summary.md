# Source diff responsibility summary

Moved from `EditorWorkspaceView` into `WorkspaceTrailingHostView`:

- construction and child order for Tool Rail, bounded scroll, precision trigger
  and transform selector row;
- Construction/Sculpt rail entry composition and active presentation;
- selector group orientation for short-window/IME compact presentation;
- visibility of transform mode/space and Display suppression/restoration;
- fixed top/right parent layout params, density re-resolution and host-level
  occlusion surfaces;
- leaf lookup through stable semantic ids instead of root verification accessors.

Removed from root layout plumbing:

- `selectorFollowerParams()`;
- `applyTransformSelectorOrientation()`;
- `applyGroupOrientation()`;
- `applyRailDock()` and its `appliedRailDocked` cache;
- `buildRailFor()`;
- fourteen package-private right-cluster accessors.

Retained in `EditorWorkspaceView`:

- every JNI/native read and command;
- Construction/Sculpt transitions and tool command routing;
- primary-surface exclusivity and inspector/body content;
- system inset interpretation and derived compact/available-window input;
- toolbar, bottom controls, viewport, navigation and global chrome orchestration.

The root's one new right-cluster responsibility method, `renderTrailingHost()`,
reads authoritative state and builds the host's immutable presentation snapshot.
No generic panel/plugin/framework abstraction and no UI-LAYOUT-R2 behavior was added.
