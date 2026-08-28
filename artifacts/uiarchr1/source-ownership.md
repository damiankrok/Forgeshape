# Source ownership comparison

## Before

```text
EditorWorkspaceView
├── JNI/native reads and commands
├── workspace mode and surface orchestration
├── ToolRailView + BoundedScrollView construction
├── precision group/toggle construction
├── transform row, groups and actions construction
├── compact selector orientation and Display suppression
├── right-cluster placement and density re-resolution
└── 14 right-cluster verification accessors
```

`TrailingClusterColumn` only decided which child absorbed a height squeeze.
The window model also exposed `railDocked`, although UI-LAYOUT-R1 had already
made both branches use the same top/right placement.

## After

```text
EditorWorkspaceView
├── JNI/native reads and commands
├── mode transitions and primary-surface exclusivity
├── workspace insets, toolbar, inspector, bottom controls and viewport
├── derives WorkspaceTrailingHostView.PresentationState
└── receives semantic callbacks from one trailingHost field

WorkspaceTrailingHostView : TrailingClusterColumn
├── ToolRailView + BoundedScrollView
├── precision group/toggle
├── transform selector row, mode group and space group
├── Construction/Sculpt entry composition and active presentation
├── compact selector orientation and Display suppression
├── fixed top/right placement plus density re-resolution
└── stable semantic children and occlusion surfaces
```

The host owns composition and presentation only. It does not call JNI, choose a
product mode, own the selected native tool, own transform values, or own whether
Exact/Details was requested. Its immutable `PresentationState` is a derived
snapshot. Callbacks express user intent; the root performs native commands and
then re-renders from authoritative readback.

`WorkspaceLayoutMode.railDocked()` and the corresponding root boolean/accessor
were deleted. The incidental density-refresh side effect of the former apply
method is preserved explicitly as `WorkspaceTrailingHostView.refreshParentPlacement()`.
