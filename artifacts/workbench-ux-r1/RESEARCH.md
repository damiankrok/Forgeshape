# Touch- and pen-first CAD: Shapr3D (iPad/Windows/Mac), Onshape mobile, SolidCraft, Plasticity, Feather, with Apple HIG and Android platform g

## Selection-first adaptive tool menu (Selection-Based Interface) (commands)
Shapr3D: you select geometry first, then the left-hand main menu turns into a short list of the tools that apply, with one default tool already active. Select a face and Offset Face is active. Select a sketch profile and Extrude is active. Select a body and Move/Rotate is active, with its gizmo shown straight away. Select a sketch line and Sketch mode is active. A line plus a face gives Rotate Around Axis; a profile plus an axis gives Revolve. Two faces on different bodies add Align and Replace Face. A 'More' button lists the other tools that are valid for the current selection. Selections made in the Items Manager drive the same menu. Command-first still works: pick the tool, and step tips walk you through ('Select face or closed sketch' → Next → 'Select lines or curves for spine'). Plasticity has a Suggested Commands window (Shift+F) that lists commands for the selection type (Solid, Sheet, Curve, Region or Point). Shapr3D 26.200 also suggests and activates the matching 2D or 3D tool from the selection.

Seen in: Shapr3D, Plasticity. Why: On a tablet, the scarcest resources are hops through menus and screen space. Once the selection has said what the user means, only a few tools make sense, so the menu shrinks to those, and the default tool saves even the one tap. Shapr3D claims this makes workflows 2 to 3 times faster (a vendor claim). Tool-first with step tips stays available as the learning path, and selection-first is the fast path.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873882619548-Adaptive-user-interface https://support.shapr3d.com/hc/en-us/articles/7378907587484-Accessing-tools https://support.shapr3d.com/hc/en-us/articles/13079186051996-Extrude-with-Adaptive-User-Interface https://support.shapr3d.com/hc/en-us/articles/27742013674780-26-200-SVG-Import-Sketch-Fillet-and-Workflow-Improvements https://doc.plasticity.xyz/plasticity-essentials/plasticity-interface/command-bar

## Tap empty space to commit (implicit Done) (commands)
Shapr3D: many tools started from the adaptive menu finish without pressing Done. You just tap an empty area of the grid. Parameter Cards and History edits also finish with 'click or tap anywhere in the modeling space'. On touch, a sketch finishes by tapping an empty area of the grid; with the pen, lifting it finishes a line. With a keyboard, Enter sets the last point and Esc/Backspace removes the last point. SolidCraft takes a desktop route: an option puts a large check and a large cross at the view's top-right while a dialog or sketch is open, and pushes the axis view or cube down below them. Without a dialog they are Finish Sketch and Cancel Sketch. Plasticity confirms with OK in the Command Dialog or a right-click.

Seen in: Shapr3D, SolidCraft, Plasticity. Why: You commit where your hand already is, on the canvas, so you don't travel to a button. The explicit check/cross variant suits a mouse and suits beginners. Both keep Esc and Enter as keyboard equivalents.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873882619548-Adaptive-user-interface https://support.shapr3d.com/hc/en-us/articles/31002224435356-Parameter-Cards https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://docs.solidcraft.io/guide/ui-tour.html https://doc.plasticity.xyz/solid/extrude

## Value label at the geometry, numpad and calculator on touch (input)
Shapr3D shows a dimension label next to the active gizmo arrow and on every sketch element. With a pointer, you hover the label and start typing; the field opens on its own and Enter commits. On touch or pen, you tap the label to open the field, tap a numpad button for an on-screen numpad/calculator, and commit with a checkmark. Fields accept + - * / and evaluate on Enter ('10 + 5' shows 15); comma decimals cause errors. Tapping a gizmo arrow, rather than dragging it, opens precise input along that direction. Sketch length labels carry a Distance Type badge: Absolute, Horizontal or Vertical. SolidCraft: once a shape's first point is down, value boxes follow the cursor (length and angle, width and height, diameter). Typing goes into the first box, Tab moves to the next, Enter places the shape, and every typed value becomes a dimension. Typing 'width = 40' creates the parameter 'width' in the same step. Plasticity: you drag the yellow dot for distance, with exact Distance / Degrees / Thickness fields in the Command Dialog.

Seen in: Shapr3D, SolidCraft, Plasticity. Why: The number sits where the user is looking, so the eye never leaves the model. Drag gives the rough value and typing gives the exact one, through the same control. A numpad with a calculator avoids the full system keyboard covering half a tablet screen.
Sources: https://support.shapr3d.com/hc/en-us/articles/7874038080668-Defining-and-calculating-numerical-values https://support.shapr3d.com/hc/en-us/articles/7383914603548-Move-and-copy-items-using-the-gizmo https://support.shapr3d.com/hc/en-us/articles/7874245890844-Editing-sketch-dimensions https://docs.solidcraft.io/print.html https://doc.plasticity.xyz/solid/extrude

## Parameter Cards: essentials on the canvas, full card on demand (properties)
Shapr3D (rolling out to a limited set of users in 26.180): when a tool starts, its essential parameters appear as on-canvas controls. Selecting the active tool's icon again opens the full Parameter Card; selecting it once more hides the card without leaving the tool. Cards can be dragged anywhere, and on iPad dragged off-screen to hide them and back again. A visibility menu in the card's upper-right corner has two switches, 'Show on Tool Start' and 'Show on History Edit'. By default the card opens only when you re-select the tool. Selecting a History step opens that step's card. On iPadOS the cards dock next to the sidebar to keep the canvas clear (26.200). Pressing a tool's shortcut again cycles its modes; pressing E again toggles Extrude between single-sided and symmetric.

Seen in: Shapr3D. Why: This is progressive disclosure: 90% of edits need one or two values at the geometry, and the full option set costs one extra tap. Because the user can move or dock the card, it never sits permanently on the part being edited.
Sources: https://support.shapr3d.com/hc/en-us/articles/31002224435356-Parameter-Cards https://support.shapr3d.com/hc/en-us/articles/27741964397212-26-180-Parameter-Cards-Rollout-Stability-Improvements https://support.shapr3d.com/hc/en-us/articles/27742013674780-26-200-SVG-Import-Sketch-Fillet-and-Workflow-Improvements

## Boolean badge on the extrude gizmo, with the operation inferred from geometry (commands)
Shapr3D Extrude picks the boolean from the geometry. It is New Body when the new solid touches nothing. It is Union when you extrude a face of a body, or pull a connected sketch away from a body. It is Subtract when you push a profile or face into a solid. Intersect is never automatic; you choose it from the Boolean badge menu at the gizmo. The badge is the manual override. 26.200 added clearer previews of how material joins, cuts or intersects. History holds the rest of the settings: Sides (One-Sided/Symmetric), Extent (Distance / To Object / Through All, with Flip), Draft Angle, and Start (From Profile / Offset / From Plane). 26.200 also lets you set To Object by dragging the arrow onto the target body. Plasticity Extrude uses single-key booleans while dragging: Q Union, W Difference, Shift+E Intersect, B New Body, T Keep Tools.

Seen in: Shapr3D, Plasticity. Why: Push in to cut, pull out to add matches what a pen gesture means physically, so the common case needs no choice. A small badge at the arrow keeps the override where the user is already looking.
Sources: https://support.shapr3d.com/hc/en-us/articles/7874453786908-Extrude https://support.shapr3d.com/hc/en-us/articles/27742013674780-26-200-SVG-Import-Sketch-Fillet-and-Workflow-Improvements https://doc.plasticity.xyz/solid/extrude

## Finger navigates, pen creates (input)
Shapr3D on iPad: drag one finger from the grid to orbit, drag two fingers to pan, and pinch to zoom. A two-finger twist rotates the view only when the 2-Finger Rotation setting is on. Double-tap a face with a finger to zoom to it. The pen sketches the moment it touches the screen ('Use your pen to start drawing a line, just like you would on paper'), and lifting the pen finishes the line. Pen single tap selects; pen double tap selects the whole body. A review summed it up: 'The Pencil is used to create and edit geometry', the finger handles the view. On a Windows pen display without touch, you navigate with modifier keys and pen hover or drag: Shift orbits, Ctrl pans, Alt zooms, and Space focuses the face under the pointer. Feather offers a 'Finger-pen' mode so fingers can draw too. Not every user likes the split: one feature request asks for a toggle so the Pencil can rotate the view.

Seen in: Shapr3D, Feather. Why: No mode switch is needed between looking and making: the tool type decides the role. Apple HIG backs this ('let people make a mark the moment Apple Pencil touches the screen'), but also says controls must respond to the Pencil too, and people should choose when to switch.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://support.shapr3d.com/hc/en-us/articles/7874250627484-Line https://develop3d.com/reviews/review-shapr3d-ipad-parasoild-modelling/ https://support.shapr3d.com/hc/en-us/articles/7873933972380-Peripherals-and-accessories https://developer.apple.com/design/human-interface-guidelines/apple-pencil-and-scribble https://apps.apple.com/us/app/id6737254232

## Precision selector and overlap disambiguation (selection)
Onshape mobile: tap-and-hold brings up the Precision Selector, a crosshair offset from the finger so the finger doesn't cover the target. It highlights faces, edges and lines live as you drag; on lift it selects. With 3D Touch it zooms the area under the crosshair and restores the original zoom afterwards. The team rejected offset-only and second-finger-click designs because neither 'felt natural'. Shapr3D: when items overlap, a pop-up list lets you pick the exact sketch, face or edge, with named items shown by name. Select Through mode reaches internal geometry. Selected items stay visible on top even when hidden by other geometry. SolidCraft: hold still for 0.5 s (long-press on a touch screen), or right-click › Select Other…, to list everything under the pointer, hidden bodies included. Hovering a row highlights that item. Clicking the same spot again cycles through the candidates.

Seen in: Onshape mobile, Shapr3D, SolidCraft. Why: A fingertip covers about 7–10 mm and hides small edges. An offset reticle or a list of candidates turns an ambiguous hit into a deliberate choice, without zooming the whole scene.
Sources: https://www.onshape.com/en/resource-center/tech-tips/tech-tip-mastering-gestures-mobile https://www.onshape.com/en/blog/under-the-hood-cad-for-mobile https://www.onshape.com/en/blog/mobile-cad-onshape-app https://support.shapr3d.com/hc/en-us/articles/7770768736924-Selecting-geometry https://support.shapr3d.com/hc/en-us/articles/19318133804572-5-830-More-precise-selections https://docs.solidcraft.io/guide/ui-tour.html

## Area selection whose meaning depends on drag direction, with in-gesture filters (selection)
Shapr3D: dragging a box left to right selects only elements fully inside it; dragging right to left selects everything it touches. On touch you start it with a long-tap and drag. While the box is still held, filter chips (All Items, Bodies Only, Faces Only, Edges Only) can be tapped with another finger. With a keyboard, Tab cycles the filters and B/F/E pick one. Plasticity uses the same direction rule and colour-codes it: right-to-left is green (touching), left-to-right is purple (enclosed). It also has 1–4 / Tab selection-mode buttons in the upper-left corner. Onshape: double-tap the screen to clear the selection. Shapr3D: tap empty grid with the pen to deselect all.

Seen in: Shapr3D, Plasticity, Onshape mobile. Why: One gesture answers both 'what is inside' and 'what touches'. Filtering inside the gesture saves a trip to a mode bar.
Sources: https://support.shapr3d.com/hc/en-us/articles/7770768736924-Selecting-geometry https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://doc.plasticity.xyz/plasticity-essentials/selecting-objects https://doc.plasticity.xyz/plasticity-essentials/plasticity-interface/selection-mode https://www.onshape.com/en/resource-center/tech-tips/tech-tip-mastering-gestures-mobile

## Orientation cube with face, edge and corner targets and 2D rotate arrows (navigation)
Shapr3D's Orientation Cube has 6 faces, 12 edges and 8 corners, all tappable, with the X/Y/Z axes drawn. Tapping a face goes to that 2D view. In a planar view, arrows above the cube's top edges rotate the view 90° clockwise or counter-clockwise. Tapping an edge or corner gives an orthographic view from there. Dragging the cube orbits. Double-tapping the cube resets the default view. A right-click on the cube offers Default View, Top View and Zoom to Fit. Shapr3D 5.880 added fixed-increment rotation, snap to the nearest orthographic view, and align-to-selected-face. Plasticity: click an axis arrow to look along it; double-clicking a face flips to the opposite side; right-click gives 90° roll and rotate. SolidCraft: an axis view where you click a signed axis tip, and click the aligned tip again for the opposite side. A house icon goes Home. An optional pitch/yaw/roll readout under the axis view accepts typed angles.

Seen in: Shapr3D, Plasticity, SolidCraft. Why: The cube is a live orientation readout and a set of 26 one-tap standard views in one small widget. Edge and corner targets give quick isometric views without menus.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873937480604-Orientation-Cube https://support.shapr3d.com/hc/en-us/articles/7873942151068-Context-menus https://support.shapr3d.com/hc/en-us/articles/20524622488476-5-880-Navigate-faster-manage-items-better-track-edits-clearly https://doc.plasticity.xyz/plasticity-essentials/plasticity-interface/view-cube https://docs.solidcraft.io/guide/ui-tour.html

## Rotation lock while sketching (navigation)
Onshape mobile: panning or dimensioning a sketch could accidentally rotate the model in 3D. One-finger drag orbited in 3D and two-finger drag rotated in 2D. The team added an on-screen lock icon in the top-right corner that blocks rotation. The lock engages automatically during certain operations, and the user can override it.

Seen in: Onshape mobile. Why: An orbit during sketching takes away the aligned view under the finger that is placing a point. A visible lock state explains why the view won't turn, and keeps the override one tap away.
Sources: https://www.onshape.com/en/blog/under-the-hood-cad-for-mobile

## Two-finger tap context menu on touch; secondary click on pointer (commands)
Onshape mobile: a two-finger tap opens a context-sensitive menu, the touch equivalent of right-click. It offers edit sketch/feature, appearance and similar actions. Shapr3D: in the Items Manager and on the canvas, tap-and-hold with the pen (or right-click) opens Rename / Zoom to / Delete. Right-click on the canvas gives Isolate selection, Zoom to Selection, Reveal in Items and Select Through. SolidCraft: right-click opens a marking menu of eight commands around the cursor, plus a menu for whatever is under it. Near the window's bottom or right edge the menu flips above or to the left so it stays on screen.

Seen in: Onshape mobile, Shapr3D, SolidCraft. Why: Actions that belong to an object open at that object, so the toolbars stay small. Long-press and two-finger tap are the two touch gestures that don't collide with orbit, pan or zoom.
Sources: https://www.onshape.com/en/resource-center/tech-tips/tech-tip-mastering-gestures-mobile https://support.shapr3d.com/hc/en-us/articles/7873936188956-Items-Manager https://support.shapr3d.com/hc/en-us/articles/7873942151068-Context-menus https://support.shapr3d.com/hc/en-us/articles/20524622488476-5-880-Navigate-faster-manage-items-better-track-edits-clearly https://docs.solidcraft.io/guide/ui-tour.html

## Items Manager / Browser: typed rows, eye, long-press menu, hover highlight (layout)
Shapr3D lists every sketch, body, plane, axis, image and drawing as an item, each with a type icon. An 'All Items' header filters by type, and the sidebar width is adjustable. Each row has a Visibility eye; a folder's eye toggles everything inside it. Tap-and-hold with the pen, or right-click, gives Rename / Zoom to / Delete. Hovering a row highlights the item in the model (5.880), and 'Reveal in Items' from the canvas scrolls to its row. A three-dot menu at the bottom right offers Show Hidden Items and Invert All Item Visibility. Selecting rows drives the adaptive tool menu. iPad gained a tree view in 5.760. SolidCraft puts the eye at the row's right end. Pressing one eye and dragging over the others shows or hides them all as one undo step. Components start collapsed, and Find in Browser opens the rows down to a picked body.

Seen in: Shapr3D, SolidCraft. Why: The browser is a second way to select, useful when the item is hard to tap in 3D, and it never forks the selection model. Eye at the trailing edge plus a long-press menu keeps each row to one 48 dp line on touch.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873936188956-Items-Manager https://support.shapr3d.com/hc/en-us/articles/20524622488476-5-880-Navigate-faster-manage-items-better-track-edits-clearly https://docs.solidcraft.io/print.html

## History list with per-step menu, filter by selection, and named, fixable errors (history)
Shapr3D's History sidebar is the parametric timeline. Selecting a step opens its Parameter Card in the canvas, and tapping the canvas finishes the edit. Each step's three-dot menu offers Insert Breakpoint (later steps disabled, removed with an X), Suppress/Unsuppress, Zoom To, Rename, Duplicate, Delete and Expand/Collapse. Selecting or isolating geometry filters the list to related steps. Merge collapses the history. Since 5.710, double-tapping a step no longer drops a breakpoint by accident. A failing step shows a yellow warning symbol. Expanding its card reveals 'Fix...'; you pick a valid plane, edge or profile and press Done, and missing references are highlighted yellow. SolidCraft's timeline runs along the bottom. Double-click a feature to edit it, drag the rollback marker (12 pt high, a 2 pt line ending in a 10 pt hollow circle) and drag features to reorder. Transport controls go start, back, play, forward, end, and Play History pauses 0.5 s per step. Sketch icons take their sketch-state colour (red when constraints conflict). An option docks the timeline vertically at the far left as a 56 pt icon strip that snaps to 130 pt with names.

Seen in: Shapr3D, SolidCraft. Why: Edits happen in the model through the same card as creation, so there is one editor per feature. Filtering by selection keeps a long chain usable on a small screen. Naming the failing step and offering Fix turns a regeneration error into a task the user can finish.
Sources: https://support.shapr3d.com/hc/en-us/articles/11567903089180-History https://support.shapr3d.com/hc/en-us/articles/11882948231196-Error-messages-in-History https://support.shapr3d.com/hc/en-us/articles/15829270331292-5-710-History-sidebar-updates https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/print.html

## Undo as platform gesture plus one fixed button pair (history)
Shapr3D iPad: swipe left with three fingers to undo and right to redo, or use the Undo/Redo icons. iPad lists them in the main menu; Windows puts them in the top bar, with a setting to place them in the Title Bar or a Bottom Bar. Shapr3D had two-finger and three-finger tap for undo/redo until iPadOS, then moved to the system's three-finger swipe (staff reply, June 2020). Users still ask for two-finger tap, while others worry about 'accidentally activating it all the time'. A community thread also reports palm contact triggering undo. Apple HIG: don't redefine the standard three-finger undo swipe; name what will be undone ('Undo Name'); show the result of an undo that happens off-screen; in compact layouts show explicit undo/redo buttons in a toolbar. Android: palm rejection arrives as ACTION_CANCEL or FLAG_CANCELED (API 33). The app must discard that pointer's motion set back to its last ACTION_DOWN, so a palm never commits a stroke.

Seen in: Shapr3D, Apple HIG, Android. Why: Undo is the user's safety net, so it must be reachable without hunting and impossible to trigger by accident. A system gesture plus one fixed, labelled button pair covers both needs.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://support.shapr3d.com/hc/en-us/articles/7873880676508-Shapr3D-modeling-space https://support.shapr3d.com/hc/en-us/articles/7873943982620-Settings-or-Preferences https://discourse.shapr3d.com/t/two-finger-tap-to-undo/8091 https://discourse.shapr3d.com/t/undo-redo-gestures-on-ipad/39643 https://developer.apple.com/design/human-interface-guidelines/undo-and-redo https://developer.android.com/develop/ui/views/touch-and-input/stylus-input/advanced-stylus-features

## Gizmo with draggable centre, arrows, planar tiles, value label and copy badge (input)
Shapr3D's gizmo has five parts. Its centre is the rotation pivot and sets the gizmo's orientation; dragging it snaps it to axes, faces, edges, profiles and construction geometry, and an Auto-orientation option can be turned off. Arrows drag for linear or rotational moves, or a tap opens precise value entry. Tiles drag for movement in a plane. The dimension label shows the live value; on touch you tap it for the numpad. A Copy badge leaves copies at each move while it is on, and a Link badge sets whether the copy stays linked in History. Plasticity Move (G): X/Y/Z keys lock to an axis, Shift+X/Y/Z to a plane. V places the pivot. W cycles orientation: Normal / Pivot / Construction Plane / World. F is freestyle, two clicks.

Seen in: Shapr3D, Plasticity. Why: One instrument covers drag, type, constrain and copy, and each sub-handle is a separate large target. The value label on the arrow ties the number to the motion that produced it.
Sources: https://support.shapr3d.com/hc/en-us/articles/7383914603548-Move-and-copy-items-using-the-gizmo https://doc.plasticity.xyz/common/move

## Sketch state by colour, backed by a second cue (visual)
Shapr3D: a fully defined sketch item or point is green and an under-defined one is blue. A connected point takes the state of the freer item. Selecting a sketch element highlights its constraints and dimensions, and selecting a constraint highlights every element it affects. SolidCraft: a sketch's icon in the browser and timeline takes the state colour, red when constraints conflict, with the state named in a tooltip. An optional status bar shows the state with its degrees of freedom. Colour-vision modes (Protan, Deutan, Tritan) recolour meaningful colours and stripe failing faces. Form warnings outline the field and put the message under it 'so colour is never the only cue'.

Seen in: Shapr3D, SolidCraft. Why: The state can be read at a glance from across the model, and the second cue (text, a pattern or a count) keeps it readable for colour-blind users. Apple HIG says to 'convey information with more than color alone'.
Sources: https://support.shapr3d.com/hc/en-us/articles/7409883850908-Sketch-states https://support.shapr3d.com/hc/en-us/articles/7770768736924-Selecting-geometry https://docs.solidcraft.io/print.html https://developer.apple.com/design/human-interface-guidelines/accessibility

## Floating side menus that keep the project front and centre, with handedness and label options (layout)
Shapr3D's menus float over the canvas at the sides 'keeping your project front and center'. The main menu (Search, Sketch, Insert, Construct, Transform, Tools) is on the left, sketch constraints on the right, and the Section View / Isolate / Measure modes sit in the main menu's bottom-left corner. An iPadOS setting moves menus and tools to the right for left-handed users. A labels setting offers Always, On Hover On All Buttons, or (Windows) Only for Hovered Buttons. The 5.440 refresh brought icons 'that pop more', tweaked typography, 'a shift to rectangular buttons, and larger labels'. Theme is System/Light/Dark with a user accent colour. Apple HIG: 'Avoid placing controls in locations that may be obscured by either hand… consider letting people reposition them.' SolidCraft offers two layouts: Floating cards and Compact panels (one top bar, Browser flush underneath, tool sheets docked under the compass). Text under ribbon icons is a separate toggle.

Seen in: Shapr3D, SolidCraft, Apple HIG. Why: The model owns the centre of the screen and chrome hugs the edges. Mirroring for handedness keeps the pen hand from covering the menus. Optional labels serve learners without permanently costing experts the space.
Sources: https://support.shapr3d.com/hc/en-us/articles/7378907587484-Accessing-tools https://support.shapr3d.com/hc/en-us/articles/7873880676508-Shapr3D-modeling-space https://support.shapr3d.com/hc/en-us/articles/9971839217436-5-440-User-Interface-Refresh https://support.shapr3d.com/hc/en-us/articles/7873943982620-Settings-or-Preferences https://developer.apple.com/design/human-interface-guidelines/apple-pencil-and-scribble https://docs.solidcraft.io/print.html

## Docked panels with live preview on large windows, floating on small ones (layout)
SolidCraft docks feature dialogs on the right, with a live preview in the viewport and drag handles and a value box on the model. An option opens dialogs in the Browser's left column instead. The Browser is on the left and the Timeline at the bottom, and Browser and History can share one column with a remembered seam. Shapr3D: the Items and History sidebars are toggled from the sidebar buttons (Ctrl+Alt+S / Ctrl+Alt+H), and on iPad parameter cards dock next to the sidebar (26.200). Onshape mobile lets users 'slide different views' to manage screen space. Android window size classes give the thresholds: compact width < 600 dp (99.96% of phones in portrait), medium 600–840 dp (tablets in portrait), expanded 840–1200 dp (97.22% of tablets in landscape). A compact height < 480 dp makes two-pane layouts impractical even when the width is medium.

Seen in: SolidCraft, Shapr3D, Onshape mobile, Android. Why: From expanded width up there is room to dock a browser and a properties pane without covering the model. Below it, panels must be on-demand and collapsible. Basing the decision on the window size class, not on 'is it a tablet', handles split screen and resizable windows.
Sources: https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/print.html https://support.shapr3d.com/hc/en-us/articles/27742013674780-26-200-SVG-Import-Sketch-Fillet-and-Workflow-Improvements https://www.onshape.com/en/blog/under-the-hood-cad-for-mobile https://developer.android.com/develop/ui/compose/layouts/adaptive/use-window-size-classes

## Fuzzy command search that knows the selection (commands)
Shapr3D Command Search opens with X (when hotkeys are on) or Ctrl/Cmd+F, from the top of the main menu, or on touch from Help > Search Commands with the on-screen keyboard. Matching is fuzzy: 'p3' finds Add Plane - 3 Points and 'snu' finds Scale - Non-uniform. An empty query lists recent commands, and pre-selecting geometry narrows the results to tools valid for it. The right arrow autocompletes, up/down cycles and Enter runs. SolidCraft: S or / opens a popup under the ribbon search field, which expands from 28 to 40 pt tall and 520 pt wide. Results are grouped by ribbon path and include features and parameters. An empty query shows recent, then pinned commands. Shift+Enter runs a command and keeps the popup open. Plasticity: F opens the Command Palette, with favourites shown as buttons under the input and shortcuts assignable by right-click.

Seen in: Shapr3D, SolidCraft, Plasticity. Why: Search finds every command, not just the ones on visible chrome, so a small screen needn't show a long tail of tools. Selection-aware results make search an extension of the adaptive menu.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873942926876-Command-Search https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://docs.solidcraft.io/print.html https://doc.plasticity.xyz/plasticity-essentials/plasticity-interface/command-palette

## Discoverable hotkeys and a one-gesture cheat sheet for keyboard-attached tablets (onboarding)
Shapr3D shows each tool's hotkey next to its name in the side menu. Long-pressing Ctrl (Windows) or Command (iPadOS) opens the full cheat sheet, which is also under Help > Keyboard Shortcuts. Shortcuts are customisable, and a Single Key Action setting picks Hotkeys or type-to-search. The defaults: E Extrude, F Fillet/Chamfer, H Shell, M Move/Rotate, V Revolve, L Line, R Rectangle, C Circle, A Arc, I Spline, T Trim, O Offset Edge, Ctrl/Cmd+1..7 standard views. A per-device Navigation Presets screen mimics other CAD apps and draws a diagram of how pan, orbit and zoom map. SolidCraft: the arrow keys turn the view 15° (Shift for a quarter turn), F6 fits the view, and 'Keep model keys out of sketches' stops a stray key from finishing a heavy sketch.

Seen in: Shapr3D, SolidCraft. Why: An iPad with a Magic Keyboard is a common setup, and users learn shortcuts in passing because each one is printed next to its tool. A press-and-hold cheat sheet costs no screen space.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys https://support.shapr3d.com/hc/en-us/articles/7873881091356-Navigation-Presets https://docs.solidcraft.io/print.html

## Onboarding by doing: step tips in tool-first mode, re-runnable basics, tutorial overlay (onboarding)
In Shapr3D's tool-first path, the tool tells you what to select next ('Select face or closed sketch', then Next, then 'Select lines or curves for spine'). Once that is familiar, the selection-first route is the fast one. The first-run tutorial teaches the pen and modifier gestures, and Dashboard > Tutorials > Redo the Basics replays it. Tutorial Mode shows pencil, touch and key presses on screen, for teaching and recording. SolidCraft shows command messages for a few seconds in a status chip at the bottom-left of the view. Clicking the chip, or Help › Messages…, gives the whole session's messages newest first, errors in red, with Copy all.

Seen in: Shapr3D, SolidCraft. Why: Hints appear only while a tool is waiting for input, so they never become permanent chrome. A message log means a missed toast is never lost.
Sources: https://support.shapr3d.com/hc/en-us/articles/13079186051996-Extrude-with-Adaptive-User-Interface https://support.shapr3d.com/hc/en-us/articles/7873933972380-Peripherals-and-accessories https://support.shapr3d.com/hc/en-us/articles/7873943982620-Settings-or-Preferences https://docs.solidcraft.io/print.html

## Pen hover previews and never commits; squeeze opens a menu at the tip (input)
Apple HIG: 'Use hover to help people predict what will happen when Apple Pencil touches the screen' and 'Avoid using hover to initiate an action'. Don't change the preview with hover height. Show hover previews for the Pencil, not for a pointer. Squeeze is one discrete, nondestructive action, and any menu it opens appears 'close to Apple Pencil Pro'. Double tap must not modify content. Barrel roll only modifies marking. Android: stylus hover distance (AXIS_DISTANCE) varies by hardware, so 'don't rely on precise values for app-critical functionality'. Use hover to preview or to indicate which button will be selected. Detect the pen with TOOL_TYPE_STYLUS (TOOL_TYPE_ERASER for an eraser end). Feather: squeezing the Pencil opens a customisable menu of common tools, and hovering over a guide previews brush size and colour.

Seen in: Apple HIG, Android, Feather. Why: Hover is imprecise and involuntary. Used only for preview it adds confidence at no risk; a menu at the pen tip keeps the hand where the work is.
Sources: https://developer.apple.com/design/human-interface-guidelines/apple-pencil-and-scribble https://developer.android.com/develop/ui/views/touch-and-input/stylus-input/advanced-stylus-features https://apps.apple.com/us/app/id6737254232

## Preview tells source from result before commit (feedback)
Shapr3D 5.840: Mirror, Move/Rotate and similar tools 'display clearer previews that differentiate the original geometry from the resulting output'. Section View got clearer colours and fill patterns. 26.200 added Boolean previews showing how material joins, cuts or intersects. Tool mode changes are 'more visually distinct so you always know which state is active'. SolidCraft docks dialogs with a live preview, and the preview follows each arrow-key or wheel step in a value field when stepping is enabled.

Seen in: Shapr3D, SolidCraft. Why: On touch, where a correction costs a gesture, seeing the outcome before lifting the finger matters more than on desktop. Distinct original-versus-result styling stops the user confusing the ghost with the part.
Sources: https://support.shapr3d.com/hc/en-us/articles/19415232408220-5-840-Tool-and-Section-View-visual-updates https://support.shapr3d.com/hc/en-us/articles/27742013674780-26-200-SVG-Import-Sketch-Fillet-and-Workflow-Improvements https://docs.solidcraft.io/print.html

## Modes as visible toggles that adapt to the selection (commands)
Shapr3D puts Section View, Isolate and Measure in a modes area at the bottom-left of the main menu. With nothing selected, Section View and Measure are available; as you select items or open tools, the modes adapt. Isolate shows only the isolated items and filters History to their steps. Measure mode turns taps into measurements. Each mode's icon has a distinct on/off state (icon-…-false / -true).

Seen in: Shapr3D. Why: Modes that change what a tap does need a persistent, visible state, or users get lost. Grouping them in one corner gives the user one place to look for the answer to 'why does tapping do something different?'.
Sources: https://support.shapr3d.com/hc/en-us/articles/7873880676508-Shapr3D-modeling-space https://support.shapr3d.com/hc/en-us/articles/9971839217436-5-440-User-Interface-Refresh https://support.shapr3d.com/hc/en-us/articles/11567903089180-History

### Numbers
- Apple HIG, iOS/iPadOS: the default control size is 44x44 pt and the minimum is 28x28 pt. Put about 12 pt of padding around bezeled elements and about 24 pt around unbezeled ones. Default text is 17 pt, minimum 11 pt. Contrast should be 4.5:1 for text up to 17 pt and 3:1 for text of 18 pt or larger, or bold. (https://developer.apple.com/design/human-interface-guidelines/accessibility)
- Android: every interactive element should have a touch target of at least 48x48 dp. Text contrast should be at least 4.5:1 for text under 18 sp (or bold text under 14 sp), and 3:1 otherwise. (https://developer.android.com/guide/topics/ui/accessibility/apps)
- Android window size classes: compact width < 600 dp (99.96% of phones in portrait); medium 600–840 dp (93.73% of tablets in portrait); expanded 840–1200 dp (97.22% of tablets in landscape); large 1200–1600 dp; extra-large ≥ 1600 dp; compact height < 480 dp (99.78% of phones in landscape); expanded height ≥ 900 dp. (https://developer.android.com/develop/ui/compose/layouts/adaptive/use-window-size-classes)
- Android stylus input: touch sampling runs at 60–1000 Hz. FLAG_CANCELED for palm rejection arrived in API 33, while ACTION_CANCEL has existed since API 1. Tilt ranges from 0 (perpendicular) to π/2 (flat). Orientation runs 0..π clockwise and 0..−π counter-clockwise. Pressure is nominally 0..1 and should be normalised if it reads above 1. The low-latency front-buffer library needs API 29 or higher and should not be used for full-screen pan or zoom (tearing). (https://developer.android.com/develop/ui/views/touch-and-input/stylus-input/advanced-stylus-features)
- Shapr3D Orientation Cube: 6 faces, 12 edges and 8 corners, all selectable. The rotation arrows in a planar view turn it 90°, and a double-tap resets the default view. (https://support.shapr3d.com/hc/en-us/articles/7873937480604-Orientation-Cube)
- Shapr3D view shortcuts: Ctrl/Cmd+1 resets the view, and 2 through 7 give Front, Back, Top, Bottom, Right and Left. Tool hotkeys: E Extrude, F Fillet/Chamfer, H Shell, M Move/Rotate, V Revolve, W Sweep, P Project, S Scale, L Line, R Rectangle, C Circle, A Arc, I Spline, G Polygon, T Trim, O Offset Edge. X opens command search when hotkeys are on. (https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys)
- Shapr3D gestures on iPad: 1 finger orbits, 2 fingers pan, a pinch zooms, an optional 2-finger twist rotates, a 3-finger swipe left/right undoes/redoes, a finger double-tap zooms to a face, a pen double-tap selects the whole body, and a long-tap and drag starts area selection. (https://support.shapr3d.com/hc/en-us/articles/7873906073884-Keyboard-shortcuts-gestures-and-hotkeys)
- Shapr3D graphics settings: anti-aliasing can be Disabled, 2x MSAA or 4x MSAA. Tessellation can be Very High, High (the default) or Low. Undo/Redo can sit in the Title Bar or a Bottom Bar on Windows. (https://support.shapr3d.com/hc/en-us/articles/7873943982620-Settings-or-Preferences)
- Shapr3D vendor claim: the adaptive interface 'can accelerate your workflow twice or thrice as much'. (https://support.shapr3d.com/hc/en-us/articles/7873882619548-Adaptive-user-interface)
- SolidCraft desktop UI tokens, mouse-first, in pt: controls 28; panel title band 40 (a 14 pt semibold title, 19 pt command icon and 26 pt close/fold targets); footer 48 with 28 pt actions; table rows 36; tree rows 26; compact scene rows 28; menu rows 30 with a 28 pt icon column; body insets 12; hint text 12; popup corners 10 with a 1 pt border; completion lists at least 230 wide; the Browser card is capped at ten 28 pt rows. (https://docs.solidcraft.io/print.html)
- SolidCraft timeline: the rollback marker is 12 pt high, a 2 pt line ending in a 10 pt hollow circle. The vertical timeline is a 56 pt icon strip that snaps to 130 pt, and can widen continuously to 624 pt. Play History pauses 0.5 s after each step. (https://docs.solidcraft.io/print.html)
- SolidCraft interaction numbers: hold still for 0.5 s (long-press on touch) to open Select Other. The mouse-gesture ring opens after a 4 pt move, with a 280 pt inner and 350 pt outer radius. The radial menu has 8 sectors with 4 pt gaps. Each arrow key turns the view 15° (Shift for 90°). Perspective field of view is 15–90°, about 34° by default. The search field grows from 28 to 40 pt tall and 520 pt wide. (https://docs.solidcraft.io/print.html)
- Onshape: one in eight Onshape logins come through the mobile apps. The rotation-lock indicator sits in the top-right corner. (https://www.onshape.com/en/blog/under-the-hood-cad-for-mobile)
- Shapr3D pen display without touch (Windows/Wacom): Shift with pen hover or drag orbits, Ctrl pans, Alt zooms, Space focuses the face under the pointer. Touch navigation is supported on pen displays only, not on screenless pen tablets. (https://support.shapr3d.com/hc/en-us/articles/7873933972380-Peripherals-and-accessories)

### Anti-patterns
- Using a two-finger tap for undo on a pen tablet. Palms and gripping fingers set it off: Shapr3D dropped two- and three-finger tap undo for the iPadOS three-finger swipe, users report palm contact triggering undo, and even requesters worry about 'accidentally activating it all the time' (discourse.shapr3d.com threads 8091 and 39643; Apple HIG undo-and-redo: don't redefine the standard gestures).
- Making the user tap a button or enter a mode before the pen can mark. Apple HIG says 'let people make a mark the moment Apple Pencil touches the screen'.
- Starting any action from pen hover, or changing the preview with hover height. Hover is imprecise and involuntary (Apple HIG; Android says not to rely on exact AXIS_DISTANCE values).
- Putting destructive or content-changing actions on Pencil double-tap or squeeze. Both can fire by accident (Apple HIG).
- Letting a single-finger drag orbit the model while the user is sketching, panning or dimensioning. Onshape had to add a rotation lock (top-right) because accidental 3D rotation took the sketch view away.
- Shipping the desktop UI unchanged to a phone or tablet. 'Icons are too small, measurements are hard to read, and dialog boxes are difficult to traverse' (Onshape mobile blog).
- Selection that depends on hitting a tiny edge under a fingertip with no fallback. Provide a precision reticle, a list of candidates or click-cycling (Onshape, Shapr3D, SolidCraft).
- Showing sketch or feature state by colour alone (green/blue/red). Add text, shape or pattern, and offer colour-vision modes (Apple HIG accessibility; SolidCraft colour vision modes and 'colour is never the only cue').
- Locking the user into a fixed pen/finger split with no setting. Shapr3D users ask for a toggle so the Pencil can rotate the view, and HIG says controls must also respond to the Pencil.
- A history gesture with a hidden side effect. Before 5.710, double-tapping a Shapr3D History step silently created a breakpoint; it was fixed to preserve the later steps.
- Deleting a referenced sketch or plane in history-based CAD. Downstream features lose their references, so the Shapr3D tutorial advises hiding items instead.
- Toasts that auto-dismiss with no way back. Apple HIG says to minimise timed UI, and SolidCraft keeps every status-chip message in Help › Messages with Copy all.
- Status chrome that permanently covers the canvas. Shapr3D keeps menus floating at the sides 'keeping your project front and center' and lets parameter cards be dragged off-screen or docked by the sidebar.

### Notes
How I read the sources: support.shapr3d.com returns a Cloudflare challenge (HTTP 403) to WebFetch and curl. I read the same Shapr3D Help Center articles through Shapr3D's public Zendesk Help Center API (support.shapr3d.com/api/v2/help_center/en-us/articles/<id>.json), so the content is vendor text. I cite each article's normal html_url. The update dates I saw run from 2024 to 3 Nov 2026.

Not verified, or only from secondary sources:
(1) "SolidCraft on tablets/phones": the only SolidCraft I found is the open-source Rust parametric CAD (github.com/bherbruck/solidcraft; docs.solidcraft.io, tangency-labs). It is a desktop and web app (Windows/macOS/Linux/FreeBSD plus an installable web app at app.solidcraft.io). Its documented UI is mouse- and keyboard-first; the only touch behaviour documented is long-press for Select Other, plus trackpad pinch and pan. I found no SolidCraft tablet or phone app. If the owner means a different product, it was not found. SolidCraft's pt sizes are desktop tokens from its contributor UI-kit section, not touch targets.
(2) Onshape mobile: there is no current text help on toolbar or panel placement; the help page only indexes videos. The gesture facts (long-press precision selector, two-finger tap context menu, double-tap deselect, rotation lock top-right, 3D-Touch force zoom) come from Onshape tech tips and blogs dating from 2015 to about 2021, so current behaviour may differ.
(3) Plasticity: I found no pen or Wacom documentation, and could not confirm 'Gizmo Distance Input' (it appeared only in a search snippet). What is documented: the yellow-dot drag plus Command Dialog fields, the F Command Palette, the bottom Command Bar, and Suggested Commands (Shift+F).
(4) uMake: no primary documentation found, so it is not used.
(5) Feather: details come only from App Store listing snippets (Finger-pen mode, squeeze menu, hover preview).
(6) Shapr3D forum claims (finger support is limited, palm triggers undo, a navigation-toggle request) come partly from search snippets. I fetched only threads 8091 and 39643.
(7) Shapr3D Parameter Cards are a limited rollout (26.180), not available to every user.
(8) Exact pixel sizes of Shapr3D's controls, cube and badges are not published.
(9) The Material 3 site did not render, so the 48 dp figure is from developer.android.com.

Relevance to ForgeShape, as my own reading and not sourced: several patterns are already ForgeShape rules — stylus hover highlights and never commits, a control that cannot succeed is not drawn, the sketch owns the single finger, handedness, and push-through Cut. These are strong candidates still missing: selection-first adaptive tool lists with a default tool, a value label at the geometry with a calculator numpad, tap-empty-to-commit, precision or overlap selection, History step menus with named Fix, and Shapr3D-style Parameter Cards. Shapr3D's automatic boolean inference and its fuller History step menu (suppress, breakpoint, reorder) would conflict with current ForgeShape rules — no suppression or rollback, Intersect never automatic — and would need owner approval.

# Desktop parametric CAD UI: Autodesk Fusion, Onshape, SolidWorks and SolidCraft. Covers command organisation and discovery, feature parameter

## Workspace, then tab, then captioned panel ribbon, with a contextual Sketch tab (commands)
There are three levels. Fusion: a workspace menu (Design, Manufacture...) sits at the far left of the toolbar. Its tabs follow (Design has Solid, Surface, Mesh, Sheet Metal), and each tab holds named panels (Create, Modify, Inspect...). A contextual tab (Sketch) appears beside the others only while its command is active. A contextual environment (Form) REPLACES the default tabs until you exit it. SolidCraft copies this: a DESIGN switcher on the left, then MODEL, SURFACE, ASSEMBLE, INSERT, INSPECT, MANAGE, VIEW, SHEET METAL, with SKETCH added only while a sketch is edited. Tabs can be hidden from a + button, and commands can be dragged between panels. SolidWorks' CommandManager is the same idea: tabs below a context-sensitive toolbar, Ctrl+PageUp/PageDown to cycle tabs, and 'Use Large Buttons with Text' to toggle captions. Turning off SolidCraft's 'Show text under ribbon icons' makes the ribbon 24 pt shorter, and tooltips stay on. SolidCraft (0.18) gives each ribbon tab its own accent colour, and (0.29) can tint panels and the 3D background per tab, with a fade between tabs.

Seen in: Autodesk Fusion, SolidCraft, SolidWorks. Why: The tab names the intent (model, sketch, inspect), so the visible tool count stays small. The contextual tab makes the mode visible without a modal dialog. For ForgeShape: a desktop Sketch tab that appears only while a sketch is open, and disappears after Finish, matches Fusion and SolidCraft exactly. Captions should be a user toggle, not removed.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-THE-FUSION-INTERFACE.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-WORKSPACES.htm https://docs.solidcraft.io/guide/ui-tour.html https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_commandmanager.htm https://github.com/tangency-labs/solidcraft/releases/tag/v0.18.0 https://github.com/tangency-labs/solidcraft/releases/tag/v0.19.0 https://github.com/tangency-labs/solidcraft/releases

## Command search on one key, with results that point back to the ribbon (commands)
Every vendor has a typed way to find a command. SolidCraft: press S or click the ribbon's search field, and the field grows into a popup. Results are grouped by ribbon path (Model > Create > Extrude) and include the design's own features and parameters. Shift+Enter runs the command and keeps the popup open, and right-clicking a result offers pin and toolbar actions. Onshape: Alt/Opt+C opens 'Search tools' at the right end of the toolbar. Hovering a result shows its description on the right. A 'Highlight tool in toolbar' checkbox lights where the tool lives, and tools the user removed still appear under 'Removed from toolbar'. Fusion: S opens a Toolbox at the cursor, named after the workspace (for example 'Model Toolbox'), with a type-to-find field and pinned shortcuts (Extrude and Fillet by default). SolidWorks: S opens a shortcut bar per mode (Part, Assembly, Drawing, Sketch), with 'Search All Commands' in it. Arrow keys navigate, Alt+Down opens a tool's menu, Enter runs, Esc dismisses.

Seen in: SolidCraft, Onshape, Autodesk Fusion, SolidWorks. Why: A large feature set, which ForgeShape now has (extrude extents, revolve, hole, pattern, finish, freeform, surface), cannot all stay visible. Search lets users find a command without hunting, and 'highlight in toolbar' teaches where it lives, so search does not replace learning the ribbon. Grouping results by ribbon path does the same job.
Sources: https://docs.solidcraft.io/guide/ui-tour.html https://cad.onshape.com/help/Content/Home/search_tools.htm https://cad.onshape.com/help/Content/Home/keyboard_shortcuts_and_hotkeys.htm https://www.nobledesktop.com/learn/cad/efficient-tool-access-in-fusion-360-marking-menu-toolbox-and-customization https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_shortcut_bars.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-E8541F92-A2DA-4CBF-A708-5374679B3F35.htm

## Radial marking menu on right-click, with fixed directions and blind gestures (commands)
Fusion: right-clicking the canvas opens a radial menu of 8 commands around the pointer. In Design they are Repeat (top), Undo (left), Redo (right), Sketch (bottom), and PressPull, Hole, Move/Copy and Delete in the diagonals. Inside a command, left is Cancel and right is OK. Hovering Sketch opens a second ring of 8 sketch tools (Finish Sketch, 2-Point Rectangle, Spline, Project, Line, Offset, Dimension, Circle). A plain context menu (Pan, Zoom, Orbit, Isolate, workspaces, saved shortcuts) hangs below the ring. You dismiss it by clicking the centre, clicking outside, or pressing Esc. Once learned, it works blind: right-press and drag straight down gives Line, and right-press and flick right gives OK. The contents change with workspace, tab and active command. SolidWorks has mouse gestures on right-drag with 2, 3, 4, 8 or 12 slots (default 4), and releasing inside the guide cancels. SolidCraft has an 8-command ring plus a menu for whatever is under the pointer, and menus flip to stay on screen near edges.

Seen in: Autodesk Fusion, SolidWorks, SolidCraft. Why: Direction is remembered by the hand (Fitts and the muscle-memory effect), so frequent acts (OK, Cancel, Undo, Repeat) cost a flick with no travel to a toolbar. On ForgeShape desktop and the mouse-equipped tablet, a right-drag ring with stable positions would add speed without adding permanent chrome. Repeat at a fixed top slot is the cheapest high-value command.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-6514ABC1-CB75-4F0B-AB0E-316FAD36BA93.htm https://www.nobledesktop.com/learn/cad/efficient-tool-access-in-fusion-360-marking-menu-toolbox-and-customization https://help.solidworks.com/2023/english/SolidWorks/sldworks/c_mouse_sestures.htm https://docs.solidcraft.io/guide/ui-tour.html

## A feature dialog docked at the side, with a live preview, OK/Cancel and Enter/Esc (properties)
Creating or editing a feature opens one dialog, docked on the right in Fusion and SolidCraft (Fusion remembers where the user last dragged it), or on the PropertyManager tab of the left pane in SolidWorks. The model shows a live preview while the dialog is open. SolidWorks' PropertyManager anatomy is a read-only title (feature icon plus name, for example Boss_Extrude1), then OK, Cancel, Detailed Preview, Help and Keep Visible (a pushpin that keeps it open for repeated use), then a Message box saying what to pick next, then collapsible Group Boxes (Direction 1, Direction 2). Enter = OK and Esc = Cancel everywhere. Onshape adds Shift+Enter (accept, close and reopen the same feature empty). SolidCraft: OK waits for the operation check to finish, Cancel 'restores the complete timeline', and a successful edit is one undo step. SolidCraft also has options to move dialogs to the left column or put OK/Cancel/Apply in the title row. Fusion has no Apply button, and users ask for one in forums.

Seen in: Autodesk Fusion, SolidWorks, Onshape, SolidCraft. Why: There is one place to look for parameters, and the model stays visible beside it, never under it. Enter/Esc gives keyboard users a commit path with no pointer travel. The 'next step' message box answers what to do next without a tutorial. For ForgeShape this maps to the Properties panel / precision surface. It should carry the feature name and icon, a one-line next-step message, and Enter/Esc, with OK always standing in the same place.
Sources: https://help.solidworks.com/2022/english/SolidWorks/sldworks/r_pm_overview.htm https://cad.onshape.com/help/Content/dialogs.htm https://cad.onshape.com/help/Content/feature-basics.htm https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/guide/features.html https://forums.autodesk.com/t5/fusion-api-and-scripts-forum/the-way-to-control-command-dialog-positioning/m-p/12956032 https://forums.autodesk.com/t5/fusion-api-and-scripts-forum/is-there-a-way-to-add-an-apply-button-in-command-dialog/m-p/6415415

## Invalid input blocks commit visibly: red title, disabled checkmark, red field outline (feedback)
Onshape: the dialog TITLE turns red when required input is missing or a value errors, which 'prevents you from committing a broken feature'. A black title means it can commit. An unsolvable expression puts a red outline around its field and greys the checkmark. A deleted reference shows in red inside the selection field, with an X to remove it or right-click > 'Clear missing references'. SolidCraft: a missing reference 'warns instead of silently picking another face'. A refusal gives its reason (for example 'a taper too steep for the profile'), and a Join that leaves separate pieces warns '`Body1` is now in 2 separate pieces'. The SolidCraft UI kit requires invalid fields to show error TEXT and a tinted surface, never colour alone.

Seen in: Onshape, SolidCraft. Why: The verdict sits where the eye already is, on the dialog and the field at fault. Refusing a commit is better than producing a broken feature. ForgeShape already refuses by name (for example AddDisjoint, ToFaceBehind), so it should show that name as readable text on the failing field or the panel title, not only in a status line.
Sources: https://cad.onshape.com/help/Content/feature-basics.htm https://cad.onshape.com/help/Content/dialogs.htm https://docs.solidcraft.io/guide/features.html https://docs.solidcraft.io/dev/ui-kit.html

## Selection fields typed by appearance, and the single profile picked automatically (properties)
Onshape dialogs show two field kinds. A solid BLUE-HIGHLIGHTED field needs a pick in the graphics area or the Feature list. A blue-OUTLINED field needs keyboard input. Tab moves focus between fields, and clicking a feature name in the list supplies it to the active field. SolidWorks: the active selection box is PINK, it grows as items are added, a picked item highlights in the graphics area, and right-click offers Delete or Clear Selections. Fusion Extrude: 'when there is only one profile visible in your design, it is automatically selected'. In Fusion, selecting a Sketch in the Browser or Timeline selects all its profiles at once.

Seen in: Onshape, SolidWorks, Autodesk Fusion. Why: The user can tell 'pick something' fields from 'type something' fields without reading. Auto-selecting the only candidate removes a click from the most common path. ForgeShape already auto-selects when exactly one region exists, which is the same rule. Showing the selection count and a clear-X on the region field would complete it.
Sources: https://cad.onshape.com/help/Content/dialogs.htm https://cad.onshape.com/help/Content/feature-basics.htm https://cad.onshape.com/help/Content/PartStudio/features_and_parts_lists.htm https://help.solidworks.com/2022/english/SolidWorks/sldworks/r_pm_overview.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-REF-EXTRUDE.htm

## OK/Cancel in the viewport corner, movable to the pointer (commands)
SolidWorks' Confirmation Corner: while a command or sketch is active, OK and Cancel icons (Exit Sketch / Cancel Sketch inside a sketch) appear in the UPPER RIGHT corner of the graphics area. Pressing D moves them to the pointer, and pressing D again sends them back. The same D key moves selection breadcrumbs to the pointer. It can be turned off in System Options > General. SolidCraft has an opt-in 'OK and Cancel in the view's corner': the check and cross act as the open dialog's OK/Cancel, or as Finish/Cancel Sketch when no dialog is open. In Fusion, right-press and flick right is OK.

Seen in: SolidWorks, SolidCraft, Autodesk Fusion. Why: Committing should not need a trip to a side panel when attention is on the model. A fixed corner gives one learned place for finishing any act, and the move-to-pointer key shortens travel on large screens. ForgeShape's Finish Sketch / Cancel transitions could keep one fixed check/cross pair in the viewport corner on the workbench and desktop.
Sources: https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_accepting_features.htm https://www.javelin-tech.com/blog/2015/11/solidworks-confirmation-corner/ https://docs.solidcraft.io/guide/ui-tour.html https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-6514ABC1-CB75-4F0B-AB0E-316FAD36BA93.htm

## Editing an earlier feature rolls the model back to it, with a 'Final' peek and an opacity slider (history)
Onshape: while a feature is edited, 'Onshape displays the model rolled back to its state when that feature was created', so later features are hidden. A FINAL button shows the finished result without leaving the dialog, and it is absent when editing the last feature. The Preview slider is an opacity control from 0% (before the feature) to 100% (after). Fusion's API says the timeline is rolled to the editing feature when the command activates and returns to its prior position when it deactivates. SolidCraft rebuilds from the first changed feature. Cancel restores the whole timeline, and Undo, Redo or a History click with an editor open first cancels that editor (0.30.1).

Seen in: Onshape, Autodesk Fusion, SolidCraft. Why: The user edits a feature in the context it was authored in, without later geometry hiding its faces, and can still check the downstream effect with one toggle. ForgeShape's staged History edit (whole chain regenerated, first failure named) gains a lot from a 'Final / At this step' toggle and a before/after fade. Having Undo cancel an open editor first prevents two histories fighting.
Sources: https://cad.onshape.com/help/Content/feature-basics.htm https://cad.onshape.com/help/Content/dialogs.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-360-API/files/core_Command.htm https://docs.solidcraft.io/guide/features.html https://github.com/tangency-labs/solidcraft/releases

## A bottom timeline of feature icons, with a marker and step controls (history)
Fusion's Timeline lists every feature as an icon in creation order. Its controls are Move to Beginning, Previous Step, Play (steps through every feature), Next Step and End of Timeline, plus a Timeline Marker and a Settings gear. Right-click a feature for Edit or Suppress, and dragging reorders it, refused before anything it depends on. Suppressing also suppresses downstream dependents. Timeline Settings > Component Color Swatch colour-codes features by component, and activating a component filters the timeline to it. SolidCraft uses the same model: double-click a feature to edit, drag the marker to roll back, drag features to reorder (a feature's sketch moves with it), and Play/Stop (Esc halts). The bar can dock at the bottom, along the full left side or under the Browser. Its seam switches between icons and names, and sketch icons take their constraint-state colour. Note that in Fusion and SolidCraft the step arrows MOVE THE MARKER (non-destructive rollback); they are not Undo.

Seen in: Autodesk Fusion, SolidCraft. Why: The whole build order is readable at a glance as icons, any step opens with a double-click, and the marker lets the user inspect an earlier state without losing later work. For ForgeShape's History strip, icon-per-feature chips with the same glyphs as the ribbon, double-click to edit, and an icons/names toggle all fit. ForgeShape's owner chose arrows that are Undo/Redo and no marker, so the strip should label them as Undo/Redo. Users from Fusion will otherwise expect them to step a marker.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Designs/files/ASM-DESIGN-MODELING-MODES.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-THE-FUSION-INTERFACE.htm https://docs.solidcraft.io/guide/ui-tour.html https://github.com/tangency-labs/solidcraft/releases/tag/v0.19.0

## A rollback bar inside the feature tree, with arrow-key stepping and Roll to here / Roll to end (history)
Onshape and SolidWorks put the history in a left tree rather than a bottom strip. A thick rollback bar can be dragged up or down. When the bar is selected, Up/Down arrows step one feature, and features below it are temporarily suppressed: SolidWorks shows their icons grey, and Onshape displays the model only up to the bar. Context menu: Roll to here (on a feature) and Roll to end (on the bar), plus SolidWorks' Roll Forward and Roll to Previous. New features can be inserted while rolled back, and a SolidWorks model can be saved with the bar anywhere. On touch, SolidWorks adds handles to the bar.

Seen in: Onshape, SolidWorks. Why: Inserting a feature mid-chain is a first-class act, and the keyboard gives precise one-step control. This is shown for comparison. The ForgeShape owner explicitly declined a slider or rollback marker (WORKBENCH-RIBBON-R1), so this informs what users may ask for later rather than proposing it now.
Sources: https://cad.onshape.com/help/Content/PartStudio/features_and_parts_lists.htm https://www.onshape.com/en/resource-center/tech-tips/tech-tip-roll-to-here-roll-to-end https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_rollback_bar.htm

## Failure status in the tree: an icon on the row, a count at the top, and an 'explain' surface (feedback)
SolidWorks FeatureManager: a red X before a feature icon marks an error with that feature, an exclamation mark a warning, a down-arrow a warning underneath the node, and a combined icon both. Older releases also colour the feature text red for errors and green for warnings. Hovering shows the explanation in a tooltip. Right-click > What's Wrong? opens a box with a description, suggestions and an Edit Feature link, and clicking the top node lists all errors. Onshape: a top-level error icon in the Feature list's upper-left shows how many features failed, and clicking it filters to errors (the filter also takes ':errors'). A yellow triangle marks a missing selection, a blue indicator an under-defined sketch, and strikethrough a suppressed feature. Fusion's API exposes Warning and Error health states with an errorOrWarningMessage.

Seen in: SolidWorks, Onshape, Autodesk Fusion. Why: Errors are findable from the top (a count), locatable (an icon on the row) and explained (a tooltip or dialog with a link to fix). Icon shape carries the meaning, not colour alone. ForgeShape's History already names the first failing feature with a cross and marks later ones not rebuilt. Adding a failure count badge on the History control, a hover/long-press reason, and an 'Edit' link would complete the SolidWorks/Onshape loop.
Sources: https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_FeatureManager_Design_Tree_Conventions.htm https://help.solidworks.com/2019/english/SolidWorks/sldworks/hidd_new_whats_wrong.htm https://cad.onshape.com/help/Content/PartStudio/features_and_parts_lists.htm https://cad.onshape.com/help/Content/PartStudio/part_studios.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-360-API/files/TimelineGroup_errorOrWarningMessage.htm

## A floating sketch palette with options for the active tool and a Finish Sketch that is always visible (sketch)
Fusion's Sketch Palette floats in the canvas whenever a sketch is open. Its top 'Contextual Options' section changes with the active tool (Line types while Line is active, all slot types for Slot) or the selection (Normal/Construction and Curvature Comb for a spline). Below are Options: Linetype, Construction, Centerline, Look At, Sketch Grid, Snap, Slice (temporarily cut bodies at the sketch plane), Show Profile (blue shading of closed profiles), Show Points/Dimensions/Constraints/Construction/Projected, and 3D Sketch. Finish Sketch is on the contextual Sketch tab and in the marking menu (Sketch > Finish Sketch). SolidCraft: on short windows the palette's options scroll while the constraint status and Finish Sketch stay visible. Auto look-at on sketch open can be turned off: in Fusion via Preferences > Design 'Auto Look At Sketch', in SolidCraft via 'Turn to face a sketch when it opens'.

Seen in: Autodesk Fusion, SolidCraft. Why: Tool options stand next to the work and change with the tool, so there is no separate settings hunt. The exit is never scrolled away. For ForgeShape this matches the sketch actions palette and the orientation navigator. Slice and Show Profile shading are cheap high-value toggles for face sketches.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Sketch/files/GUID-4183A4B7-E002-4396-AD5A-7FF3C8B2F33A.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-Sketch/files/SKT-FINISH-SKETCH.htm https://docs.solidcraft.io/guide/sketching.html

## Sketch inference cues and value boxes that follow the cursor (sketch)
SolidCraft: the pointer snaps to points, midpoints and curve crossings, and a crossing shows the hint 'X'. Lining up with other points shows 'H', 'V' or 'H+V' with dotted guides from each point. Shift prefers midpoints, and Ctrl/Cmd or Alt/Option suppresses snapping for that gesture. Once a shape's first point is placed, value boxes follow the cursor: Tab moves between them and Enter places the shape. A new dimension's value box takes the keyboard with the measured value pre-selected, so typing replaces it. Line draws a chain, and right-click or Esc ends it. Constraint state colours: under-constrained blue, fully constrained dark, conflicts red on the geometry, glyph or dimension involved. Onshape's sketch palette: hover #f3bf8e, selected #ffc107, inactive #949ba0, closed-sketch fill #e3e6e9, over-defined/error #bd3039. SolidWorks prefixes sketch names (+) over-defined, (-) under-defined, (?) unsolvable, and no prefix when fully defined.

Seen in: SolidCraft, Onshape, SolidWorks, Autodesk Fusion. Why: The user sees why a point will land where it lands before clicking, and exact values are typed without leaving the drawing flow. ForgeShape has snapping and a Line dimension. Visible snap glyphs (X at crossings, H/V guides) and cursor-following length/angle boxes during drawing on desktop would close most of the precision gap. ForgeShape has no constraint solver, so the constraint colours apply only as Driving/Reference dimension cues.
Sources: https://docs.solidcraft.io/guide/sketching.html https://cad.onshape.com/help/Content/Home/colors.htm https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_FeatureManager_Design_Tree_Conventions.htm

## View cube: faces, edges and corners as views, drag to orbit, a home house and a context menu (navigation)
Fusion's ViewCube sits in a canvas corner. Dragging it orbits. A named face gives that orthographic view, a corner an isometric view, and triangles near a face's edges go to the adjacent face while a face view is current. A Home icon returns to the home view. Right-click offers Go Home; Camera: Orthographic, Perspective, Perspective with Ortho Faces; Set current view as Home (Fit to View or Fixed Distance); Reset Home; Set current view as [Front] / Reset; and ViewCube Size: Automatic, Large, Normal, Small, Tiny. Onshape's cube is in the UPPER RIGHT of the graphics area, and its curved arrows rotate 15° per click (Shift 90°, Ctrl 5°). Its menu has Top/Bottom/Front/Back/Right/Left, Isometric/Dimetric/Trimetric, Zoom to fit, Perspective, six display styles and Section View. SolidWorks' View Selector (Ctrl+Space) explodes the hidden back faces outward so they can be picked without rotating. SolidCraft's compass is 110 pt. Clicking an axis tip looks along it, an editable View angles readout shows pitch, yaw and roll, the cube follows Perspective so it matches the view, and an option makes a cube click also centre the model.

Seen in: Autodesk Fusion, Onshape, SolidWorks, SolidCraft. Why: The current orientation and every standard view are in one small, always-visible widget, and the context menu holds the projection and home settings, so the view bar can stay minimal. For ForgeShape's view cube: add edge targets, curved 15°/90° step arrows, a home house, and right-click/long-press for Ortho/Perspective and 'set current as Home'. Keep the zoom unchanged on face clicks; an Onshape forum complaint is that cube re-alignment zooms all the way out.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-VIEWCUBE.htm https://cad.onshape.com/help/Content/View/view_navigation_and_the_view_cube.htm https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_view_selector.htm https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/dev/ui-kit.html https://github.com/tangency-labs/solidcraft/releases https://forum.onshape.com/discussion/18725/align-views-via-navigation-cube-without-zooming-out

## A floating navigation bar at the bottom centre: orbit, look at, pan, zoom, fit, display, grid, viewports (navigation)
Fusion's Navigation bar is 'the floating toolbar at the bottom of the canvas'. Orbit (Free / Constrained, and the button shows the mode used last), Look At (makes a selected face, edge or line parallel to the screen), Pan, Zoom (usable during other navigation), Fit / Zoom Window (shows whichever was used last), Display Settings, Grid and Snaps, and Viewports (Single, or up to four with Synchronize Views). Display Settings holds Visual Style (Shaded; Shaded with Hidden Edges; Shaded with Visible Edges Only; Wireframe variants), Environment, Effects (Ambient Occlusion, Anti-Aliasing, shadows, Selection Effect), Object Visibility, Camera (Orthographic, Perspective, Perspective with Ortho Faces) and Full Screen. SolidWorks puts a transparent 'Heads-up View toolbar' at the TOP of each viewport, holding the view tools and the View Orientation flyout. SolidCraft's navigation bar has 32 pt buttons and 18 pt icons in a 40 pt outer height (compact: 26 / 16 / 34 pt).

Seen in: Autodesk Fusion, SolidWorks, SolidCraft. Why: The view tools stand at the model and away from the authoring commands. Split buttons that remember the last mode keep the bar short. ForgeShape's view bar (Fit, Home, Front, Top, Right, projection) matches. Folding the standard views into the cube and giving the bar a remembered Fit/Zoom-Window split and a Visual Style flyout (shaded / shaded+edges / wireframe) would follow Fusion.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-NAVIGATION-BAR.htm https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_heads_up_view_toolbar.htm https://docs.solidcraft.io/dev/ui-kit.html

## Keyboard views and navigation, with mouse presets matching other CAD tools (input)
Onshape: Shift+1..7 = Front, Back, Left, Right, Top, Bottom, Isometric. F = fit. N = normal to (parallel to the screen). Z / Shift+Z = zoom out / in. W = zoom box. Arrows rotate 15°, Shift+arrows 90°, Ctrl+arrows 5°, and Ctrl+Shift+arrows pan. Mouse: right-drag rotate, Ctrl+right or middle drag pan, wheel zoom. Touch: 1-finger rotate, 2-finger pan/pinch/rotate, 2-finger tap for the context menu. SolidCraft: Ctrl+1..7 standard views, F6 fit, Ctrl+8 look at the selected face/plane/sketch, arrows 15° / Shift a quarter turn / Ctrl pan, a double middle-click goes Home, and the wheel zooms at the cursor. Fusion: middle-drag pan, wheel zoom, Shift+middle orbit, and Shift+click+middle to orbit around a point. Its Preferences > Mouse setup offers Fusion, Alias, Inventor and SolidWorks presets, and SolidCraft adds NX and Creo presets.

Seen in: Onshape, SolidCraft, Autodesk Fusion. Why: Users switching from another CAD tool keep their hands' habits through presets, and the keyboard gives exact 90° and 15° steps that drag cannot. For ForgeShape desktop and tablet-with-keyboard: number-row standard views, F to fit, arrow-key steps, zoom at the cursor, and a SolidWorks/Fusion mouse preset are low cost and high familiarity.
Sources: https://cad.onshape.com/help/Content/Home/keyboard_shortcuts_and_hotkeys.htm https://cad.onshape.com/help/Content/View/view_navigation_and_the_view_cube.htm https://docs.solidcraft.io/guide/shortcuts.html https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-F0491540-0324-470A-B651-2238D0EFAC30.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-878489CD-3A23-4303-8450-C2F4F8E410B1.htm https://github.com/tangency-labs/solidcraft/releases

## Single-letter command keys that the vendors share (input)
Fusion and SolidCraft bind the same letters. Model: E Extrude, H Hole, F Fillet, Q Press Pull, M Move/Copy, J Joint, A Appearance, V toggle visibility, S toolbox/search, I Measure, Delete. Sketch: L Line, R 2-point Rectangle, C Center-Diameter Circle, T Trim, O Offset, P Project, X Normal/Construction toggle, D Sketch Dimension. Also Ctrl+Z / Ctrl+Y and F2 Rename (SolidCraft). Onshape uses Shift-modified keys in the Part Studio (Shift+E Extrude, Shift+W Revolve, Shift+S Sketch, Shift+F Fillet) and letters in Sketch (L line, A 3-point arc, C circle, R centre rectangle, G corner rectangle, D dimension, M trim, X extend, O offset, Q construction). Shift+/ opens a searchable shortcut map, and every shortcut can be edited, disabled or reset. Fusion: 'Keyboard shortcuts will not work while a text field is active'.

Seen in: Autodesk Fusion, SolidCraft, Onshape. Why: These letters are close to an industry convention. Adopting them lets a Fusion or SolidCraft user drive ForgeShape at once. ForgeShape's sketch rail (Line, Polyline, Rectangle, Circle, Arc, Spline, Trim) and Extrude, Hole and Fillet map one-to-one onto L / R / C / A / T / E / H / F. A Shift+/ cheat sheet makes them discoverable, and showing the key in every tooltip and menu row (as SolidCraft does with F2) teaches them.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-F0491540-0324-470A-B651-2238D0EFAC30.htm https://docs.solidcraft.io/guide/shortcuts.html https://cad.onshape.com/help/Content/Home/keyboard_shortcuts_and_hotkeys.htm https://github.com/tangency-labs/solidcraft/releases

## Selection grammar: click, modifiers, window versus crossing box, and a count at the cursor (selection)
Fusion: selection is always active. Ctrl/Cmd toggles add/remove, and Shift adds without removing. Modes are Window (1, default), Freeform (2) and Paint (3). Dragging left to right selects only what is fully enclosed, and right to left also selects what is crossed. Freeform uses clockwise and counter-clockwise for the same distinction. 'Select Through' decides whether hidden objects are picked. Onshape: a click TOGGLES an entity. A left-to-right box draws a solid blue outline (#789ad1) with a blue fill (#d0dcef). A right-to-left box draws a DOTTED yellow outline (#ffdc74) with a yellow fill (#fff3cf). The cursor shows the selection count up to 5, then '5+'. Space or a click on empty space clears, and on mobile a two-finger press-and-hold opens the box. SolidCraft: a click on the same spot again selects the next item under the cursor, double-click selects a chain or tangent loop, and Lasso draws a freehand loop.

Seen in: Autodesk Fusion, Onshape, SolidCraft. Why: The box direction is encoded by colour AND line style, so window versus crossing reads at a glance. The cursor count confirms multi-select without looking at a panel. ForgeShape's Freeform cage multi-select and sketch multi-select would benefit from this exact window/crossing grammar on desktop, and from a count chip on touch.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECTION.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECT-OBJECTS.htm https://cad.onshape.com/help/Content/Home/selection.htm https://cad.onshape.com/help/Content/Home/colors.htm https://docs.solidcraft.io/guide/shortcuts.html https://github.com/tangency-labs/solidcraft/releases

## Select Other: a pick list for what is under or behind the pointer (selection)
Fusion: click and HOLD to select objects hidden behind others. A dialog lists the candidates, with a Depth tab (canvas order) and a Parents tab (design hierarchy). SolidWorks: right-click > Select Other hides the face you right-clicked and lists the faces behind it. SolidCraft: a long press (on touch too) or right-click > Select Other… lists everything under the cursor. Clicking the same spot again cycles through the candidates, hovering a list row highlights it, and picking a hidden object leaves its visibility unchanged. A preference includes or excludes hidden bodies and sketches.

Seen in: Autodesk Fusion, SolidWorks, SolidCraft. Why: Dense parts (a hole wall behind a fillet, a cap behind a pattern instance) cannot always be reached by rotating. A depth list makes every surface reachable from any view. A long-press list maps directly to ForgeShape's touch model, and it is a gap: ForgeShape's NEAREST chooser target always owns the tap.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECT-OBJECTS.htm https://help.solidworks.com/2021/english/SolidWorks/sldworks/HIDD_SelectOther_dlg.htm https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/guide/shortcuts.html https://github.com/tangency-labs/solidcraft/releases

## Selection priority and filters: limit picks to one type, with one active at a time (selection)
Fusion: Select > Selection Priority has Body, Face, Edge and Component priority. Only one can be active. Turning one on checks its filter and unchecks the rest, and clicking it again restores the previous filters. Select > Selection Filters is a checklist (Bodies, Body Faces, Body Edges, Body Vertices, Sketch Curves, Sketch Profiles, Sketch Points, Dimension, Features, Work Geometry...). SolidWorks: the Selection Filter toolbar toggles with F5, with single keys for some filters (E edges, X faces, V vertices). Forum users report that filters 'change on their own' inside commands; Fusion changes the filter while a command runs.

Seen in: Autodesk Fusion, SolidWorks. Why: When faces, edges and vertices crowd one pixel, a type filter makes the pick predictable. Changing the filter automatically per command (for example, a fillet accepts edges only) removes the need to set it at all. But the active filter must stay VISIBLE, because silent filter changes are a documented source of confusion. ForgeShape's Freeform Vertex/Edge/Face modes are such a priority; showing the mode in the selection chip keeps it honest.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECTION-PRIORITY-FILTERS.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECTION-FILTERS.htm https://help.solidworks.com/2020/english/SolidWorks/sldworks/r_Selection_Filter.htm https://forums.autodesk.com/t5/fusion-support-forum/filters/td-p/9524478

## Breadcrumbs, context toolbar and cross-highlighting for the current selection (selection)
SolidWorks selection breadcrumbs appear in the UPPER LEFT of the graphics area when an entity or a tree node is selected. They show its chain (face > feature > its sketch > body / part) and its suppression state, and clicking a crumb shows that item's tools. They are hidden while a PropertyManager is open or several entities are selected. Esc or a click on empty space removes them, D shows them at the pointer, and an option keeps them semitransparent at the pointer. A context toolbar of the most frequent actions also appears beside the selection, a subset of the right-click menu, and right-clicking shows the rest. Selecting in the graphics area expands and scrolls the FeatureManager tree to the item. Onshape cross-highlights the features that created or use a selected part, face or edge, and hovering a hidden entity in the Parts list outlines it in the view. SolidCraft: in a Hole dialog, hovering or typing in a field lights its dimension in the model.

Seen in: SolidWorks, Onshape, SolidCraft. Why: Selection is linked to its history and its actions in both directions: the user learns which feature made a face and reaches 'edit the sketch under this face' in one click. ForgeShape's face offer (Sketch here, Round/Bevel edges, Hole, Pattern, Mirror) is already a context toolbar. Adding a breadcrumb row (Face > Feature #n > Sketch #k) and highlighting the producing History chip when a face is tapped would turn it into the SolidWorks loop.
Sources: https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_selection_breadcrumbs.htm https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_context_toolbars.htm https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_featuremanager_design_tree.htm https://cad.onshape.com/help/Content/PartStudio/features_and_parts_lists.htm https://docs.solidcraft.io/guide/features.html

## Drag handles on the model, bound to the same values as the dialog (properties)
SolidWorks Instant3D: drag handles and dimension manipulators resize features on the model, and an on-screen ruler appears during the drag with snapping to its ticks. Dragging an extruded boss one way lengthens it, and dragging the opposite way creates a cut extrude. Single-clicking a dimension opens Quick Edit, and double-clicking opens the full Modify box or PropertyManager. SolidCraft: dialogs dock right while 'drag handles and value boxes also appear in the viewport'. A Hole's centre has a drag handle, and a Cylinder's edge distances can be typed in the window or in a box on the canvas. Fusion: the extrude has distance and taper manipulators on the canvas next to the dialog fields.

Seen in: SolidWorks, SolidCraft, Autodesk Fusion. Why: There is one truth with two doors: a drag and a typed value land in the same field, so they cannot disagree. ForgeShape already does this (the extrude arrow, the revolve ring, and push-through that turns a drag past the sketch plane into a Cut, which is SolidWorks' own Instant3D behaviour). The remaining step is ruler tick snapping during a drag (1/2/5 steps by zoom), so a drag can land on round numbers.
Sources: https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_instant3d.htm https://help.solidworks.com/2017/English/SolidWorks/sldworks/c_instant3d_functionality.htm https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/guide/features.html

## The canonical extrude dialog: Start, Direction, Extent, Operation, and an operation chosen from the material (properties)
Fusion's Extrude fields, in order: Type (Extrude / Thin Extrude); Profiles; Start (Profile Plane / Offset / Object); Direction (One Side / Two Sides / Symmetric, where Symmetric has Measurement: Half Length or Whole Length); Extent Type (Distance / To Object with Extend: To Selected Face, To Adjacent Faces, To Body, Through Body plus Offset / All); Taper Angle; Operation (Join / Cut / Intersect / New Body / New Component). When the preview has to pass several bodies with All, the preview matches the final result. SolidCraft picks the operation automatically from the material: Join when the result touches a body, New Body when separate, Cut when going into a body. 'An operation you pick stays fixed', and a preference turns the automatic choice off. Picking a face out of the profile's plane switches Distance to To Object.

Seen in: Autodesk Fusion, SolidCraft. Why: A fixed field order and fixed names build transferable knowledge across tools, and an automatic operation removes the most common wrong-operation error. ForgeShape's extent set (One Side, Symmetric, Two Sides, Through All, To Face) and operations (New Body, Add, Cut, Intersect) map field-for-field. ForgeShape already flips the direction when Cut is chosen and does push-through. Offering Fusion's 'Half / Whole length' as presentation of Symmetric fits ForgeShape's stored per-side value without a format change.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-EXTRUDE-SOLID.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-REF-EXTRUDE.htm https://docs.solidcraft.io/guide/features.html

## The browser tree: eye at the row end, a split features/parts list, folders, F2 rename (layout)
Fusion's Browser on the LEFT lists components, bodies, sketches, origin, joints and construction geometry, with an eye icon per row for visibility (V toggles visibility of the selection). Onshape's left Feature list sits above a collapsible Parts list, with a drag bar between them. Each feature shows the icon of the tool that made it. A filter field takes text or ':part' / ':errors', and folders can be created and nested. SolidCraft: tree rows are 26 pt, the eye stands at the RIGHT end of each row (0.28, configurable), one drag can show or hide many rows, F2 renames, components start collapsed, and there is Expand All / Collapse All / Find in Browser. Fusion users complain that the on and off eye icons are too similar in colour.

Seen in: Autodesk Fusion, Onshape, SolidCraft. Why: Visibility, identity and order are scannable in one column, and a right-aligned eye is a stable target that does not jump with name length. ForgeShape's Browser (Objects plus History) should give each row the producing tool's icon. The hidden state should be told apart by SHAPE (an eye versus a crossed eye) plus dimmed text, not by colour.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-THE-FUSION-INTERFACE.htm https://www.autodesk.com/products/fusion-360/blog/how-to-navigate-autodesk-fusion-user-interface-ui/ https://cad.onshape.com/help/Content/PartStudio/features_and_parts_lists.htm https://cad.onshape.com/help/Content/PartStudio/part_studios.htm https://docs.solidcraft.io/guide/ui-tour.html https://docs.solidcraft.io/dev/ui-kit.html https://github.com/tangency-labs/solidcraft/releases https://forums.autodesk.com/t5/fusion-design-validate-document/hide-and-show-toggle-visibility/m-p/9022673

## Dark themes built from roles, with one accent split into fill, ink and soft (visual)
Fusion (2024-25 preview, API enum since January 2025) offers Light Gray, Fusion Classic, Dark Blue and match-device themes. The canvas follows only when Display Settings > Environment is 'Theme (default)'. Autodesk's Weave design system ships the Light gray, Dark gray and Dark blue schemes, each in HIGH and LOW density, and in its high-density theme the body font is smaller. Onshape has a dark mode in Part Studios, Assemblies and Render Studios via Preferences > Theming. SolidWorks (since 2016) has four interface background shades and two icon colour sets. SolidCraft: themes are JSON that override named colours and corner radii. Colour-vision modes (Protan, Deutan, Tritan) apply on top of any theme. Each tab accent has three roles: FILL (pills, active-tool border, history marker), INK (small text, selected rows) and SOFT (tinted selection and tool backgrounds). Pill text uses on_accent only when it reaches 4.5:1, otherwise black, and tests require 4.5:1 for ink on card, ink on soft and pill text on fill in both themes. One Fusion forum user called Dark Blue 'dated'.

Seen in: Autodesk Fusion, Onshape, SolidWorks, SolidCraft. Why: One accent used in three tested strengths keeps the UI calm while active state, selection and the history marker stay unmistakable. Density as a theme axis lets one design serve a 27-inch desktop and a tablet. ForgeShape's five palettes and role-based attrs already follow this. Adding the fill/ink/soft triple per accent and a 4.5:1 test for each pair is the concrete next step. A high-density variant for the desktop shell and a comfortable one for touch mirrors Weave.
Sources: https://help.autodesk.com/cloudhelp/ENU/Fusion-360-API/files/UserInterfaceThemes.htm https://all3dp.com/6/fusion-gets-a-dark-mode/ https://forums.autodesk.com/t5/fusion-support-forum/ui-theme-preview-dark-mode-feedback-amp-discussion/td-p/13284919 https://github.com/Autodesk/hig https://cad.onshape.com/help/Content/Plans/account_menu.htm https://store.trimech.com/?p=28876 https://docs.solidcraft.io/dev/ui-kit.html https://docs.solidcraft.io/guide/ui-tour.html

## Commands that adapt: last-used first, a second click ends the tool, Repeat, panels that collapse (commands)
Onshape: 'the last-used tool moves to the top of its group' (it resets when the document is reopened), and at lower resolution or higher browser zoom, icons collapse into more dropdown groups. SolidWorks flyout buttons carry a small down arrow for related commands. Fusion's Orbit and Fit buttons show the mode used most recently, and Repeat sits at the top of the marking menu. SolidCraft (0.29): 'a second click on the lit ribbon button of the sketch tool in use ends it'. Each ribbon drop-down row waits the full tooltip delay, so tooltips do not cover the rows below (0.30). Ribbon tooltips give two-sentence help for the main tools (0.18.2). Drop-downs can optionally show as a grid of large icons.

Seen in: Onshape, SolidWorks, Autodesk Fusion, SolidCraft. Why: Recency cuts repeated travel without the user customising anything, and a lit button that also ends the tool gives one obvious way out of a mode. Tooltips that explain in two sentences teach without a manual. For ForgeShape's ribbon and Tool Rail: the active tool's button lit in accent FILL, a second tap ending it, and split buttons remembering the last variant (rectangle and circle types, extent).
Sources: https://cad.onshape.com/help/Content/ui-basics.htm https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_commandmanager.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-NAVIGATION-BAR.htm https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-6514ABC1-CB75-4F0B-AB0E-316FAD36BA93.htm https://github.com/tangency-labs/solidcraft/releases https://github.com/tangency-labs/solidcraft/releases/tag/v0.18.0

### Numbers
- SolidCraft UI kit: standard control height 28 pt; table rows 36 pt (control_height + space_8); Browser tree rows 26 pt; compact scene rows 28 pt; menu rows 30 pt with a 28 pt icon column (https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft panel/window: 40 pt title band (14 pt semibold title, 19 pt command icon, 26 pt close/fold targets), 12 pt body insets, 48 pt footer with 28 pt action buttons; Confirm uses a 36 pt icon disc and a 52 pt footer (https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft default dock widths: Browser 266 pt, Inspector 305 pt; compass (view cube) 110 pt; Shots strip 48 pt (https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft navigation bar: regular 32 pt buttons, 18 pt icons, 40 pt outer height; compact 26 pt buttons, 16 pt icons, 34 pt outer height (https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft corner radius defaults: radius_sm 4, radius_md 6, radius_lg 8, radius_xl 10, control 6, sketch-mode 7 pt; theme override range 0-32 pt; border widths 0-4 pt (https://docs.solidcraft.io/guide/ui-tour.html, https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft tooltip delay: 800 ms; turning off 'Show text under ribbon icons' makes the ribbon 24 pt shorter (https://docs.solidcraft.io/dev/ui-kit.html, https://github.com/tangency-labs/solidcraft/releases/tag/v0.19.0)
- SolidCraft contrast rule: at least 4.5:1 for accent ink on card/panel, ink on soft and pill text on fill, in both light and dark themes (https://docs.solidcraft.io/dev/ui-kit.html)
- SolidCraft UI scale: Ctrl/Cmd+Plus enlarges, Ctrl/Cmd+Minus reduces, Ctrl/Cmd+0 restores 100% (https://docs.solidcraft.io/guide/ui-tour.html)
- Fusion marking menu: 8 first-level commands + 8 second-level sketch commands + a context menu below the ring (https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-6514ABC1-CB75-4F0B-AB0E-316FAD36BA93.htm)
- SolidWorks mouse gestures: 2, 3, 4, 8 or 12 per guide, default 4 (https://help.solidworks.com/2023/english/SolidWorks/sldworks/c_mouse_sestures.htm)
- Onshape view-cube arrows and keyboard arrows: 15° per step, Shift 90°, Ctrl 5° (https://cad.onshape.com/help/Content/View/view_navigation_and_the_view_cube.htm)
- Onshape standard-view keys Shift+1..7 (Front, Back, Left, Right, Top, Bottom, Isometric); SolidCraft Ctrl+1..7 for the same seven views and Ctrl+8 Look At (https://cad.onshape.com/help/Content/Home/keyboard_shortcuts_and_hotkeys.htm, https://docs.solidcraft.io/guide/shortcuts.html)
- Fusion ViewCube sizes: Automatic, Large, Normal, Small, Tiny; camera options: Orthographic, Perspective, Perspective with Ortho Faces (https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-VIEWCUBE.htm)
- Fusion viewports: up to 4, with Synchronize Views (https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GS-NAVIGATION-BAR.htm)
- Fusion selection-mode keys: Window 1 (default), Freeform 2, Paint 3 (https://help.autodesk.com/cloudhelp/ENU/Fusion-Model/files/SLD-SELECTION.htm)
- Onshape cursor selection count: exact up to 5, then '5+' (https://cad.onshape.com/help/Content/Home/selection.htm)
- Onshape box selection colours: left-to-right solid outline #789ad1 with fill #d0dcef; right-to-left dotted outline #ffdc74 with fill #fff3cf (https://cad.onshape.com/help/Content/Home/colors.htm)
- Onshape selection/sketch colours: selected part/edge/vertex #ffc107; error #bd3039; sketch edge hover #f3bf8e; sketch dimension hover #d9bfa0, selected #d9a566; under-defined sketch entity #0000ff; fully defined #000000; inactive sketch curve #949ba0; closed sketch fill #e3e6e9; sketch plane line/text #90cef1 (https://cad.onshape.com/help/Content/Home/colors.htm)
- Onshape alert palette (outline/fill/text): info #b2ddf6/#def1fb/#094174; warning #fcda7f/#fff8e6/#333333; error #dd979b/#f9ebec/#333333; success #a2db7d/#ddf6cc/#333333 (https://cad.onshape.com/help/Content/Home/colors.htm)
- Onshape feature preview slider: opacity from 0% (before the feature) to 100% (after) (https://cad.onshape.com/help/Content/dialogs.htm)
- Fusion single-letter keys: E Extrude, H Hole, F Fillet, Q Press Pull, M Move, V Visibility, S Toolbox, A Appearance, J Joint; sketch: L Line, R 2-point Rectangle, C Circle, T Trim, O Offset, I Measure, P Project, X Construction, D Dimension (https://help.autodesk.com/cloudhelp/ENU/Fusion-GetStarted/files/GUID-F0491540-0324-470A-B651-2238D0EFAC30.htm)
- Fusion timeline controls: 5 (Move to Beginning, Previous Step, Play, Next Step, End of Timeline) plus a marker and a Settings gear (https://help.autodesk.com/cloudhelp/ENU/Fusion-Designs/files/ASM-DESIGN-MODELING-MODES.htm)
- SolidWorks CommandManager tab cycling: Ctrl+PageUp / Ctrl+PageDown; Selection Filter toolbar F5; filter keys E edges, X faces, V vertices; Confirmation Corner / breadcrumbs to pointer: D (https://help.solidworks.com/2022/english/SolidWorks/sldworks/c_commandmanager.htm, https://help.solidworks.com/2020/english/SolidWorks/sldworks/r_Selection_Filter.htm, https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_accepting_features.htm)
- SolidWorks View Selector: Ctrl+Space; Alt selects back-face views (https://help.solidworks.com/2024/English/SolidWorks/Sldworks/c_view_selector.htm)
- Autodesk Weave design system: Light gray, Dark gray and Dark blue colour schemes, each in high and low density (https://github.com/Autodesk/hig)

### Anti-patterns
- Colour as the only carrier of state. Fusion users complain that the visibility-eye on and off icons are too similar in colour. Use shape (eye versus crossed eye, X versus !) plus text, as SolidWorks' tree prefixes (+)/(-)/(?) and SolidCraft's 'error text and tinted surface, not colour alone' rule do (https://forums.autodesk.com/t5/fusion-design-validate-document/hide-and-show-toggle-visibility/m-p/9022673, https://docs.solidcraft.io/dev/ui-kit.html).
- Silently retargeting a lost reference to another face. SolidCraft explicitly 'warns instead of silently picking another face'. Onshape shows the missing reference in red inside the field (https://docs.solidcraft.io/guide/features.html, https://cad.onshape.com/help/Content/feature-basics.htm).
- A history marker that can disappear. Fusion's marker vanishing on long timelines and large screens is a long-standing bug (FUS-26303) that users can only clear by reopening the file. The current position must always be drawn and scrolled into view (https://forums.autodesk.com/t5/fusion-design-validate-document/history-marker-keeps-going-missing-on-me/td-p/6453150).
- Drag-to-reorder over long distances without a dependency check. Fusion users report a feature dropped at the very start of the timeline, ahead of the sketch it needs. A reorder must validate dependencies and refuse, not land somewhere unexpected (https://forums.autodesk.com/t5/fusion-support-forum/timeline-reordering-incorrect-and-impossible-moves-features-to/td-p/13917763).
- Selection filters that change silently inside a command. Fusion users perceive filters 'changing on their own', and a stuck Body priority blocks face and edge picks with no visible cause. The active filter or priority must be shown (https://forums.autodesk.com/t5/fusion-support-forum/filters/td-p/9524478, https://forums.autodesk.com/t5/fusion-support/select-body-priority-is-stuck-on-cannot-select-faces-edges/td-p/9203674).
- View-cube clicks that throw away the user's zoom. An Onshape forum complaint is that re-aligning to a sketch with the cube always zooms all the way out (https://forum.onshape.com/discussion/18725/align-views-via-navigation-cube-without-zooming-out).
- Forced camera jumps on mode entry with no opt-out. Both Fusion ('Auto Look At Sketch') and SolidCraft ('Turn to face a sketch when it opens', 'When a sketch ends') had to add preferences because some users do not want the view changed for them (https://docs.solidcraft.io/guide/sketching.html).
- Hiding commands into ever-deeper dropdowns as width shrinks, with no search fallback. Onshape collapses icons into dropdown groups at low resolution, which only works because Search tools (Alt+C) and 'Removed from toolbar' results exist (https://cad.onshape.com/help/Content/ui-basics.htm, https://cad.onshape.com/help/Content/Home/search_tools.htm).
- Tooltips that cover the next menu row, or appear instantly. SolidCraft 0.30 fixed drop-down tooltips covering the rows below, and its kit uses an 800 ms delay (https://github.com/tangency-labs/solidcraft/releases, https://docs.solidcraft.io/dev/ui-kit.html).
- A pinned dialog that stays open when the user expects it to close. SolidWorks' Keep Visible pushpin is useful for repeated mates, but users report it is annoying when left on for one-shot commands. Pinning should be explicit and visibly ON (https://www.cati.com/?p=118133).
- A dark palette that is merely a recoloured light theme. Fusion's first Dark Blue preview drew 'dated, 20+ years old' feedback, and the canvas did not follow the theme unless the Environment was set to 'Theme (default)'. Chrome and viewport ground must be designed together (https://forums.autodesk.com/t5/fusion-support-forum/ui-theme-preview-dark-mode-feedback-amp-discussion/td-p/13284919, https://all3dp.com/6/fusion-gets-a-dark-mode/).
- Undo or Redo racing an open feature editor. SolidCraft 0.30.1 changed Undo, Redo and History clicks to cancel the open editor first, because both acting at once corrupted the user's expectation of which change was undone (https://github.com/tangency-labs/solidcraft/releases).

### Notes
Method: Autodesk Fusion help (help.autodesk.com cloudhelp pages) and Onshape help (cad.onshape.com/help) were read directly. SolidWorks help pages (help.solidworks.com) render through JavaScript, so their text was taken from the 'helpText' embedded in each page's raw HTML, saved under /tmp/claude-0/-home-user-Forgeshape/2f5454c6-63b1-52b8-b92c-5f8b69c420c6/scratchpad/web. Pages were mainly the 2022 and 2024 versions, and menu wording may differ in other releases.

Not verified online, so each item is general knowledge or a gap:

1. Fusion Timeline. The help page ASM-TIMELINE.htm returns 404. The controls come from ASM-DESIGN-MODELING-MODES.htm. That the Timeline sits along the BOTTOM of the window, and its icon colours for warnings and errors (yellow and red highlights), are general knowledge; only the API's Warning and Error health states are sourced.
2. Fusion pre-selection and selection highlight colours (light blue hover, blue selection) are general knowledge; Fusion's help names only a 'Selection Effect' setting. Fusion's ViewCube is in the upper right, the browser on the left and command dialogs on the right by default: the help says only 'corner of the canvas', and dialog placement comes from forum posts.
3. Fusion S-key Toolbox. The Autodesk blog returned 403. Details come from a help-page search snippet and a Noble Desktop tutorial (a secondary source).
4. Onshape. Where feature dialogs float (over the graphics area, next to the Feature list) is not stated in the help. Neither is a view-cube home button. Onshape's toolbar icon sizes are not published.
5. SolidWorks. The Heads-up View toolbar's exact button list, the CommandManager's default Part tabs (Features, Sketch, Evaluate...) and current theme names were not confirmed. The four background shades and two icon sets come from a TriMech reseller blog.
6. SolidCraft. 'app.solidcraft.io' could not be confirmed. The project is the open-source, Rust, Fusion-style clean-room CAD at github.com/tangency-labs/solidcraft (earlier bherbruck/solidcraft), with docs at docs.solidcraft.io and a browser app reported at solidcraft.io/app. Its docs list the ribbon tabs as MODEL, SURFACE, ASSEMBLE, INSERT, INSPECT, MANAGE, VIEW and SHEET METAL, with SKETCH contextual. 'Weldments' and 'Assembly', as named in the task, do not appear in its docs. SolidCraft deliberately mirrors Fusion (shortcuts, marking menu, timeline), so agreement between those two is one design, not two independent sources.
7. Shapr3D and touch-first CAD were outside this domain and were not researched here.

One fact for the parent agent: in Fusion and SolidCraft, the History or Timeline back and forward arrows MOVE THE ROLLBACK MARKER (non-destructive); they are not Undo/Redo. ForgeShape's owner decision (WORKBENCH-RIBBON-R1) makes them Undo and Redo with no marker. That decision stands. The strip's labels and tooltips should say 'Undo' and 'Redo' so users coming from Fusion are not misled.

Pixel sizes come from SolidCraft's published UI kit, in points. Fusion, Onshape and SolidWorks publish no pixel metrics for toolbars or panels.

# Platform and accessibility design guidance for ForgeShape's two shells: Material 3 large screens and adaptive layouts, Android large-screen 

## Lay out by window size class, never by device type (layout)
Width classes: Compact < 600 dp, Medium 600–839 dp, Expanded 840–1199 dp, Large 1200–1599 dp, Extra-large >= 1600 dp. Height classes: Compact < 480 dp, Medium 480–899 dp, Expanded >= 900 dp. Width and height are classified separately, and the class can change at any moment (rotation, split screen, fold, DeX or ChromeOS window resize), so the layout must re-derive on every configuration change. Phone landscape is medium or expanded WIDTH but compact HEIGHT, so a two-pane layout is not practical there. The Compose adaptive defaults (PaneScaffoldDirective) are: Compact and Medium get 1 horizontal pane; Expanded gets 2 panes with a 24 dp spacer; Large and above get 3 panes with a 24 dp spacer. Default pane width is 360 dp (412 dp at Large and above), and a second VERTICAL pane (24 dp spacer) is used only in tabletop posture or with one column at Expanded height. For ForgeShape: the workbench gate (EXPANDED plus Browser affordance) matches the 2-pane rule. Browser + viewport + Properties as THREE docked columns is the Large/XL (>= 1200 dp) shape. Phone landscape (compact height) should keep panels transient.

Seen in: Android / Jetpack Compose Material 3 adaptive, ChromeOS, Samsung DeX. Why: The breakpoints are opinionated majority cases: 99.96% of phones in portrait are Compact; 93.73% of tablets in portrait are Medium; 97.22% of tablets in landscape are Expanded; desktop displays are >= 1600 dp. Deciding from the window's available space rather than the screen keeps split screen and desktop windowing correct.
Sources: https://developer.android.com/develop/ui/compose/layouts/adaptive/use-window-size-classes https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/adaptive/adaptive-layout/src/commonMain/kotlin/androidx/compose/material3/adaptive/layout/PaneScaffoldDirective.kt

## Supporting-pane canonical layout for viewport plus Properties (layout)
The supporting-pane layout gives the primary content most of the window and puts content that only makes sense beside it in a secondary pane. The guidance names 'a media editing tool with palettes, effects and other settings' as its use case. Expanded width: 70% main / 30% supporting. Medium width: an equal split, OR the supporting content hidden in a bottom or side sheet opened by a control. Compact width: the supporting content goes below the main content or into a bottom sheet. List-detail is different: its detail is meaningful on its own. When list-detail narrows from Expanded, the DETAIL stays visible and the list hides. For ForgeShape the Properties panel (precision surface) is a supporting pane. It is irrelevant without the model, which argues for the 70/30 docked shape at Expanded and a sheet or anchored surface below that. The Browser (Objects + History) behaves like a list pane.

Seen in: Android canonical layouts, Material 3. Why: A tool palette is meaningless by itself, so it should never displace the canvas. Keeping the canvas at roughly 2/3 to 70% of the width at Expanded protects the work area while keeping settings one glance away.
Sources: https://developer.android.com/develop/ui/compose/layouts/adaptive/canonical-layouts

## Leading navigation, trailing panels open on desktop, resizable dividers (layout)
Adaptive Tier 2 (Responsive_adaptive_layouts) lists four shapes. Leading-edge navigation rails expand into full navigation panels on larger windows. Trailing-edge panels are open by default on desktop screen sizes and closed on smaller screens. Grids add columns. Multi-pane layouts are used where appropriate. The Desktop experience tier (UI_Config) adds that panels in multi-panel layouts are collapsible and reconfigurable with a mouse or trackpad: the user resizes them by dragging dividers or rearranges them. Toolbars may be dockable or movable. This does not apply to navigation bars, rails and drawers. Tier 2 (UI_Secondary_Elements) adds: bottom sheets, buttons and text fields are NOT full width on large screens (apply a max width), small edit menus do not cover the screen, and context menus appear next to the selected item. For ForgeShape: the Browser on the leading edge and Properties on the trailing edge, open at rest on the workbench and desktop, are the documented shape. What is missing is a drag-resizable divider on each docked panel (desktop and DeX), plus a max width on any sheet or editor.

Seen in: Android adaptive app quality guidelines, ChromeOS / Googlebook desktop, Samsung DeX. Why: Navigation stays where the eye starts reading, and contextual detail sits on the trailing side next to the content. A pointer user expects to size panels to their task, as in every desktop CAD tool.
Sources: https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/tier-2 https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop

## Tool Rail sized as a Material navigation rail (navigation)
Material 3 navigation rail tokens: collapsed container 96 dp wide (narrow variant 80 dp), item vertical spacing 4 dp, top space 44 dp. The expanded rail is 220–360 dp wide (default 220). Icon 24 dp; collapsed active indicator 56 x 32 dp, fully rounded, with 4 dp to the label. In the expanded rail the indicator is 56 dp high with 16 dp padding and 20 dp horizontal margin. 3–7 destinations collapsed, unlimited expanded. The rail replaces a bottom navigation bar on Medium and larger windows. For ForgeShape's Tool Rail (8 sketch tools, 7 sculpt brushes): 8 entries exceeds the 7-destination guidance for a navigation rail. The Tool Rail is a TOOL selector rather than navigation, so the cap is advisory, but it argues for grouping, for example the draw tools versus Trim. An 80–96 dp column with a 24 dp glyph and an optional label under it is the platform-standard footprint.

Seen in: Material 3 / Jetpack Compose, Material Components for Android. Why: A fixed-width leading column keeps tool targets at a constant thumb or pointer distance. The 56 x 32 dp pill marks the active tool by shape as well as colour.
Sources: https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/NavigationRailCollapsedTokens.kt https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/NavigationRailExpandedTokens.kt https://github.com/material-components/material-components-android/blob/master/docs/components/NavigationRail.md

## Two target-size regimes: 48 dp touch, visual-sized pointer targets (input)
Android: every touch target is at least 48 x 48 dp (Tier 2 Touch_Targets). The Material small icon button draws a 40 dp container around a 24 dp icon and reaches 48 dp with invisible padding. With a mouse or trackpad, targets may be smaller than 48 dp. The Desktop tier (Cursor_Target_Size) requires the cursor hit area to MATCH the visual bounds, and the pointer guide says do not let buttons keep intrinsic touch targets beyond their visual boundaries. Windows: the touch target is 7.5 mm square (40 x 40 epx at 135 PPI, 1.0x), and standard density aligns everything to a 40 x 40 epx grid. A high-density Compact option exists for pointer-heavy UIs, with body text at 14 px. WCAG 2.2 SC 2.5.8 (AA): at least 24 x 24 CSS px, OR a 24 px-diameter circle centred on each undersized target that intersects no other target or circle. For ForgeShape: keep the 48 dp floor for Android touch chrome. On the Windows desktop shell and for mouse use on DeX, ribbon and panel controls can be pointer-dense (40 epx standard, smaller in a compact density) as long as no target falls below 24 x 24 px or breaks the spacing circle, and the hit box equals the drawn box.

Seen in: Android / Material 3, Windows 11 / WinUI, WCAG 2.2. Why: A finger contact patch needs about 7.5–9 mm. A cursor is a precise point, so oversized invisible hit areas only cause mis-hits between neighbours. Matching hit and visual bounds lets pointer users aim at what they see.
Sources: https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/tier-2 https://developer.android.com/guide/topics/ui/accessibility/apps https://developer.android.com/design/ui/desktop/guides/interaction/pointer-interactions https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/SmallIconButtonTokens.kt https://learn.microsoft.com/en-us/windows/apps/develop/input/guidelines-for-targeting https://learn.microsoft.com/en-gb/windows/apps/design/style/spacing https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html

## Mouse, trackpad and keyboard parity on Android large screens (input)
Tier 2 requirements:
- Context_Menus: right-click (secondary button or two-finger tap) opens the same context menu a long-press does.
- Content_Zoom: Ctrl + scroll wheel and trackpad pinch zoom the content.
- Hover_States: actionable elements show a hover state.
- Keyboard_Navigation: Tab and arrow keys move focus.
- Keyboard_Shortcuts: select, cut, copy, paste, undo, redo.
- Keyboard_Exit: Esc closes menus and dialogs, cancels focus, deselects all, aborts renaming without saving, and 'exits cropping/transform tools and discards changes'.
The Desktop tier adds:
- Input_Combinations: Ctrl+click and Shift+click for multi-select and ranges.
- Scrollbar_Display: a scrollbar shows while scrolling.
- Hover_Parity: tooltips, previews and fly-outs on hover, with accessible contrast.
- Custom_Cursors: I-beam, resize handles at panel edges, and tool cursors.
- Keyboard_Parity: shortcut parity with the desktop version, including Ctrl+Z and Ctrl+Shift+Z.
- Desktop_Menus: desktop-style menus and small modals.
Primary click alone must complete every main journey (Switch Access depends on it). For ForgeShape: in a sketch or transform, Esc should cancel the active tool or drag, discarding it. Ctrl+click should add to selection (multi-select of sketch entities and Freeform cage elements already exists in the domain). The Windows and Android shortcut sets should be identical.

Seen in: Android adaptive quality Tier 2, Android desktop experience tier, ChromeOS. Why: Desktop and DeX users carry decades of desktop muscle memory. Meeting it costs little and is what makes an Android app feel native on a big screen rather than a phone app in a window.
Sources: https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/tier-2 https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop https://developer.android.com/design/ui/desktop/guides/interaction/pointer-interactions https://developer.android.com/develop/ui/compose/touch-input/input-compatibility-on-large-screens

## Publish shortcuts to the Keyboard Shortcuts Helper; show them in tooltips and keytips (commands)
Android: Meta+/ (Windows key on PC keyboards, Search on ChromeOS, Command on Mac) opens the system Keyboard Shortcuts Helper on API 24+. The app lists its own shortcuts by overriding onProvideKeyboardShortcuts() and adding KeyboardShortcutGroup objects, each a titled list of KeyboardShortcutInfo(label, keycode, meta). Groups appear as separate categories, so a CAD app can publish 'Sketch', 'Model' and 'View' groups. Activity.requestShowKeyboardShortcuts() opens the screen from a button. Windows: every ribbon command has an enhanced tooltip giving the command name, the shortcut key and a description, never a restatement of the label. Keytips: press Alt, then a letter for a tab, then letters for a command. Assign F to the application button and H to Home; give tabs their first letter; use numeric keys for the Quick Access Toolbar. Do not start an access key with J, Y or Z, which are reserved for contextual tabs, unassigned keytips and pop-up groups.

Seen in: Android 7.0+ Keyboard Shortcuts Helper, Windows ribbon (Office, Paint, WordPad). Why: Shortcuts are invisible until surfaced. A system-wide helper and shortcut-bearing tooltips teach them at the moment of use, without a cheat sheet.
Sources: https://developer.android.com/develop/ui/compose/touch-input/keyboard-input/keyboard-shortcuts-helper https://developer.android.com/design/ui/desktop/guides/interaction/keyboard https://learn.microsoft.com/en-us/windows/win32/uxguide/cmd-ribbons

## Ribbon structure by the Windows ribbon guidelines (commands)
Tab and group structure:
- At most 7 core tabs; most programs should aim for 4 or fewer. Do not add a Help tab.
- The first tab is Home, holding the most frequent commands. Order the remaining tabs by frequency while keeping a logical flow.
- At most 7 groups per tab, 3–5 commands per group; avoid groups of 1–2 commands. Group names are specific and task-based, never Edit, Tools or Advanced.
- Groups in the CENTRE of a tab draw the most attention, so place the most-used groups there.
- Within a group, 32 x 32 px icons for the most frequent and important labelled commands, placed before the 16 x 16 px ones. Mixed sizes aid recognition; uniform sizes only when scaled down. In-ribbon gallery height is 48 px.
- Each command lives on ONE tab only.

Contextual and modal tabs:
- A contextual tab appears only for a selected object type. It is auto-selected on insert or double-click, and when it disappears the Home or first tab becomes active.
- A modal tab serves a temporary mode where no core tab applies. ForgeShape's Sketch tab is exactly this.

Scaling and state:
- Specify up to 3 layouts per group; the ribbon shows the largest combination that fits, and there is no default width.
- Show the ribbon expanded on first run. Persist the minimized state, but NOT the last-selected tab.
- Context menus and mini-toolbars carry in-place commands.
- Disable, don't hide, commands that do not apply, because hiding changes the ribbon layout and makes it unstable. This is in tension with ForgeShape's 'a control that cannot succeed is not drawn' rule; reserving the slot, or keeping group composition stable per tab, resolves it without drawing an inert control.

WinUI CommandBar:
- Primary commands in order of importance overflow dynamically into '...'; at most 4 primary commands fit at 320 epx.
- On large windows put labels to the RIGHT of icons so they are visible while the bar is closed.
- Labels are one word; icons are 20 x 20 px in the bar and 16 x 16 px in overflow.
- Avoid sticky bars, because they break light-dismiss.

Seen in: Microsoft Office / Windows ribbon, WinUI CommandBar, Autodesk Fusion (tabbed toolbar). Why: Labelled groups make commands self-explanatory and findable by elimination. A small, stable tab set means users rarely switch tabs during a common task, and stable positions build spatial memory.
Sources: https://learn.microsoft.com/en-us/windows/win32/uxguide/cmd-ribbons https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/command-bar

## Tooltips on every icon-only control (feedback)
Android: View.setTooltipText() or android:tooltipText (API 26+) shows a small description near the view on mouse hover OR long-press, independent of contentDescription. The Desktop tier Hover_Parity asks for informative tooltips and fly-outs on hover. Windows: enhanced tooltips give the name, the shortcut and a short description, and may explain WHY a command is disabled. WinUI tooltip corners are 4 px. For ForgeShape: Tool Labels defaults to OFF, so hover and long-press tooltips are the missing discoverability layer for the rail, the ribbon, the view bar, the dock and the face bar on DeX, the tablet with a mouse, and Windows. They also let the HUD stay caption-free.

Seen in: Android 8.0+, Windows ribbon / WinUI. Why: Icon-only chrome stays compact for experts while novices get the name and the shortcut on demand. Long-press gives touch users the same information.
Sources: https://developer.android.com/guide/topics/ui/tooltips https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop https://learn.microsoft.com/en-us/windows/win32/uxguide/cmd-ribbons https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry

## Windows title bar geometry and behaviour (layout)
Geometry:
- Height 32 px, or 48 px when it holds a search box or a person picture.
- App icon 16 x 16 px, 16 px from the left edge (or 16 px right of a back button), vertically centred (8 px above and below at 32 px).
- Window title in Caption style (Segoe UI Variable 12/16) placed 16 px from the icon. The title truncates with an ellipsis; the icon and caption buttons never do.
- The back button is 16 x 16 px with a full-bleed backplate, 16 px from the edge.
- Caption buttons use Segoe Fluent Icons E921 minimize, E922 maximize, E923 restore and E8BB close, with full-bleed backplates and rest, hover, pressed, active and inactive states.

Behaviour:
- All empty space and the title are draggable.
- Right-click shows the system window menu; double-click toggles maximize.
- Clicking the icon opens the system menu; double-clicking it closes the window.
- All title-bar elements are semi-transparent while the window is inactive.
- The bar grows with text scaling and switches with light/dark and contrast themes.

The default background is Mica, and guidance says the title bar should blend with the rest of the window. Tabs as the main element may occupy the title bar, with caption controls staying anchored right. For ForgeShape's own Vulkan-drawn title row: each of these behaviours must be implemented by hand, through the non-client hit test, because no framework provides them.

Seen in: Windows 11 apps (File Explorer, Settings, WinUI Gallery). Why: Users move, snap and close windows without thinking because every Windows title bar behaves identically. A custom-drawn bar that misses one behaviour feels broken immediately.
Sources: https://learn.microsoft.com/en-us/windows/apps/design/basics/titlebar-design

## Type ramps: Segoe UI Variable on Windows, Material 3 scale on Android (visual)
Windows 11 ramp (size/line height in epx): Caption 12/16 Regular; Body 14/20; Body Strong 14/20 Semibold; Body Large 18/24; Body Large Strong 18/24 Semibold; Subtitle 20/28 Semibold; Title 28/36 Semibold; Title Large 40/52 Semibold; Display 68/92 Semibold.
Windows rules:
- Minimum legible size is 14 px Semibold or 12 px Regular.
- Sentence case everywhere; left aligned (centre only for text under icons).
- Semibold for emphasis; no Bold and no Italic, which reduces readability, especially for people with dyslexia.
- Clip or ellipsize to fit. Caption size suits very confined spaces such as command buttons; Body Strong titles a confined panel.
- The optical-size axis adjusts automatically from 8 to 36 pt.
- 50–60 characters per line for prose.
Material 3 scale (sp, size/line height, weight): Label Small 11/16 Medium; Label Medium 12/16 Medium; Label Large 14/20 Medium; Body Small 12/16; Body Medium 14/20; Body Large 16/24; Title Small 14/20 Medium; Title Medium 16/24 Medium; Title Large 22/28; Headline Small 24/32; Headline Medium 28/36; Headline Large 32/40.
For ForgeShape: ribbon captions and panel field labels = Caption or Label Medium (12); values and list rows = Body (14); panel headers = Body Strong or Title Small (14 semibold/medium); page titles = Title (28) or Headline Medium (28).

Seen in: Windows 11 / Fluent 2, Material 3 / Android. Why: Each ramp gives a small number of steps, each with a fixed job, so hierarchy reads at a glance. The minimum-size rules keep dense CAD panels legible across languages.
Sources: https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography https://learn.microsoft.com/en-us/windows/apps/develop/platform/xaml/xaml-theme-resources https://learn.microsoft.com/en-us/windows/apps/design/basics/content-basics https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/TypeScaleTokens.kt

## A 4 px spacing ramp with named gaps (visual)
Fluent 2 base unit is 4 px. The global size tokens are 0, 2, 4, 6, 8, 10, 12, 16, 20, 24, 28, 32, 36, 40, 48, 52, 56 px, where 2, 6 and 10 exist only to align icon padding. Windows named gaps:
- 8 epx between buttons, between a button and its flyout, and between a control and its header.
- 12 epx between a control and its label, and between content areas.
- 16 epx from a surface edge to its text.
- 16 epx between a control and an expander button; controls inside an expander are indented 48 epx.
- Title, Subtitle and Body are stacked with 12 epx spacing.
Fluent web breakpoints: small 320–479, medium 480–639, large 640–1023, x-large 1024–1365, xx-large 1366–1919, xxx-large >= 1920 px, on a 12-column grid. Android adaptive panes: 24 dp spacer between panes. For ForgeShape's desktop UI kit, adopt this exact ramp as tokens. Panel padding 16; row gap 8; label-to-field 12; section gap 12–16.

Seen in: Fluent 2 (web), Windows 11 / WinUI, Material 3 adaptive. Why: A short multiplicative ramp makes every distance one of a few recognisable steps. Equal gaps then read as 'related' and larger gaps as 'separate', and the corner logic maps onto the same numbers.
Sources: https://fluent2.microsoft.design/layout https://learn.microsoft.com/en-us/windows/apps/design/basics/content-basics https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/adaptive/adaptive-layout/src/commonMain/kotlin/androidx/compose/material3/adaptive/layout/PaneScaffoldDirective.kt

## Corner radius by surface level, square where edges meet (visual)
Windows 11:
- 8 px on top-level containers: windows, flyouts, dialogs, menus, teaching tips.
- 4 px on in-page controls (Button, CheckBox, ComboBox, TextBox, list backplates) and on bars (ProgressBar, ScrollBar, Slider) and tooltips.
- 0 px where straight edges meet other straight edges (the halves of a SplitButton), on the side of a flyout attached to its invoker, and on window corners when snapped or maximized.
- Global resources: ControlCornerRadius 4 and OverlayCornerRadius 8.
Fluent 2: None 0 (navigation and tab bars), Small 2 (badges), Medium 4 (buttons, dropdowns), Large 8 (large buttons), X-Large 12 (sheets, popovers), Circle 50%. Stroke Thin 1 px, Thick 2 px.
Material 3 shape scale: ExtraSmall 4, Small 8, Medium 12, Large 16, LargeIncreased 20, ExtraLarge 28, ExtraLargeIncreased 32, ExtraExtraLarge 48 dp, Full.
For ForgeShape: on desktop use 4 px controls and 8 px floating surfaces, with the ribbon bar and docked panels square at the window edge. On Android, Material 12–16 dp panels and 28 dp sheets stay consistent with the existing concentric rule (inner = outer − gap).

Seen in: Windows 11, Fluent 2, Material 3. Why: Rounding steps with elevation, so transient surfaces read as floating while docked or touching edges stay square. That avoids crescents and slivers at joins.
Sources: https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry https://fluent2.microsoft.design/shapes https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/ShapeTokens.kt

## Dark neutral surface ladder, never pure black; viewport one step lighter than chrome (visual)
Material: the baseline dark surface is #121212, not black. Elevation is shown by a LIGHTER surface through a white or primary overlay rather than by shadow: the M2 codelab uses 2% for cards and 12% for a bottom app bar. Material 3 dark tonal ladder: Surface #141218; SurfaceContainerLowest #0F0D13; Low #1D1B20; Container #211F26; High #2B2930; Highest #36343B; SurfaceBright #3B383E.
Windows dark: SolidBackgroundFillColorBase #202020, Secondary #1C1C1C, Tertiary #282828, Quarternary #2C2C2C. Control fills are translucent white over these (ControlFillColorDefault #0FFFFFFF, CardBackground #0DFFFFFF). DividerStroke is #15FFFFFF; ControlStrongStroke is #8BFFFFFF.
Blender (a 3D-viewport reference), default theme: 3D viewport background #3D3D3D graded to #303030; area headers #303030 at 70% alpha; text fields and regions #1D1D1D; widget fill #545454; text #E6E6E6; wire #000000. The 3D canvas is deliberately LIGHTER than the panels, so black edges, dark-shaded faces and shadows stay readable against it.
Fusion's dark theme is 'Dark Blue', and its canvas follows the UI theme only when Display Settings > Environment is set to Theme (default).
NN/g: positive polarity (light) beats dark for normal-vision acuity and proofreading, especially at small font sizes. Dark helps some low-vision users (cloudy ocular media). Offer both, keep the choice consistent, and honour the OS setting.

Seen in: Material 2/3 dark theme, Windows 11 dark theme, Blender, Autodesk Fusion. Why: Dark grey rather than black leaves room to express elevation by lightening, reduces halation of light text, and lets shadows remain visible. A mid-dark neutral viewport keeps every shading value distinguishable without tinting the model.
Sources: https://codelabs.developers.google.com/codelabs/design-material-darktheme/ https://m2.material.io/design/color/dark-theme https://github.com/material-components/material-components-android/blob/master/docs/theming/Dark.md https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/ColorDarkTokens.kt https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/PaletteTokens.kt https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml https://projects.blender.org/blender/blender/raw/branch/main/release/datafiles/userdef/userdef_default_theme.c https://www.nngroup.com/articles/dark-mode/

## Desaturated light-tone accents and status colours on dark (visual)
Material dark themes use the LIGHT tone of the brand colour: M3 dark Primary is tone 80 (#D0BCFF, 10.91:1 on #141218), with a dark OnPrimary (tone 20). The M2 codelab replaces saturated colours with lighter tonal variants, because pure #FFFFFF and saturated colours 'vibrate' against dark grounds. Windows flips its status colours per theme. Dark: Critical #FF99A4 (8.03:1 on #202020), Success #6CCB5F (8.03:1), Caution #FCE100 (12.34:1). Light: #C42B1C, #0F7B0F, #9D5D00.
A counter-example from Blender's defaults: the selected-widget fill #4772B3 on #303030 is only 2.72:1, and #E6E6E6 text on it is 3.89:1. A mid-saturation blue selection fill therefore fails both 1.4.11 (3:1) and 1.4.3 (4.5:1) unless a second carrier (outline, check mark, weight) states the selection.
In the viewport itself, Blender reserves high-chroma orange for selection: select #ED5700, 3.08:1 on #3D3D3D; active #FFA028, 5.33:1. The chrome stays neutral grey.

Seen in: Material 3, Windows 11, Blender. Why: Lighter, less saturated tones keep accents readable at every elevation without glare. Reserving the one saturated colour for the selection makes the selected geometry pop against neutral chrome.
Sources: https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/ColorDarkTokens.kt https://codelabs.developers.google.com/codelabs/design-material-darktheme/ https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml https://projects.blender.org/blender/blender/raw/branch/main/release/datafiles/userdef/userdef_default_theme.c

## WCAG 2.2 AA contrast applied to text, control boundaries and state indicators (accessibility)
SC 1.4.3 (AA): text needs 4.5:1. Large text needs 3:1; large means at least 18 pt (about 24 px) or 14 pt bold (about 18.67 px). Android restates this as < 18 sp, or bold < 14 sp, needs 4.5:1. Exempt: inactive (disabled) components, logotypes and decoration. AAA is 7:1. Ratios are not rounded, so 4.499:1 fails.
SC 1.4.11 (AA): 3:1 against adjacent colours for:
- the visual information needed to identify a control and its operation;
- state cues (selected, focused, checked, open);
- the parts of graphics needed to understand content.
Focus indicators are NOT exempt. Hover styling need not reach 3:1 but must not reduce existing contrast. Disabled controls are exempt. A control with visible text or a contrasting icon needs no visible boundary. 2.999:1 fails.
Computed on the platform tokens:
- M3 Outline #938F99 on Surface #141218 = 5.87:1, fine for control borders. OutlineVariant #49454F = 1.99:1, decorative dividers only.
- Windows dark text: secondary #C5FFFFFF = 10.15:1; tertiary #87FFFFFF = 5.51:1; disabled #5DFFFFFF = 3.34:1 on #202020.
- Windows LIGHT tertiary text #72000000 on #F3F3F3 = 3.28:1, which FAILS for small text. Light disabled text is 2.50:1.

Seen in: WCAG 2.2, Android accessibility guidance, Material 3, Windows 11. Why: 3:1 for non-text and 4.5:1 for text are the thresholds at which people with moderately low vision can still find the control and read its state. Measuring the actual token pairs catches failures, such as light-palette tertiary text, before they ship.
Sources: https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast.html https://developer.android.com/guide/topics/ui/accessibility/apps https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml

## Every drag has a single-pointer, non-drag alternative (accessibility)
SC 2.5.7 Dragging Movements (AA): all functionality operated by dragging must also work with a single pointer WITHOUT dragging, unless dragging is essential. Accepted alternatives:
- buttons or arrows that move an item;
- click-to-set (clicking a point on a slider track);
- menus that choose a destination;
- text inputs for exact values.
A swipe or flick does not count. The SC's own examples: map panning backed by up, down, left and right buttons; a selection rectangle made by clicking one corner and then the opposite corner; colour wheels backed by typed values. For CAD:
- extrude arrow drag = typed distance (already present);
- revolve ring drag = typed angle (present);
- gizmo drag = placement editor (present);
- rectangle and line drawn by tap-tap as well as press-drag;
- orbit and pan = view cube faces and view bar buttons (Front, Top, Right, Home, Fit);
- divider resizing = a collapse toggle or a menu size choice.

Seen in: WCAG 2.2. Why: People with tremor, head pointers or switch access cannot hold a button while moving. A typed or tapped path also gives everyone a precise route that a drag cannot.
Sources: https://www.w3.org/WAI/WCAG22/Understanding/dragging-movements.html

## Visible, high-contrast keyboard focus on custom-drawn UI (accessibility)
Windows high-visibility focus visual:
- a 2 px PRIMARY border outside a 1 px SECONDARY border, with a 1 px margin from the control bounds;
- dark theme FocusStrokeColorOuter #FFFFFF and Inner #B3000000; light #E4000000 and #B3FFFFFF;
- thickness and margin are adjustable per control.
WCAG 2.4.13 Focus Appearance (AAA): the indicator's area is at least that of a 2 CSS px perimeter of the component, with at least 3:1 change between the focused and unfocused pixels. 1.4.11 also requires focus indicators to contrast 3:1 with adjacent colours.
Android Tier 2 Drawable_Focus: interactive custom drawables must be focusable when not in touch mode, with an apparent focus state.
Android keyboard guide:
- Tab cycles all interactive elements in reading order;
- arrows move directionally; a slider takes Left/Right for its value;
- focus stays trapped inside a modal dialog;
- Esc is strictly a local cancel;
- initial focus goes to the most important element, such as the first text field.
For ForgeShape's Vulkan-drawn desktop kit and its custom Android views, no framework supplies any of this. The two-tone 2 px + 1 px ring is a proven recipe that stays visible on both light and dark grounds.

Seen in: Windows 11 / WinUI, Android, WCAG 2.2. Why: A two-tone ring (light outside, dark inside) contrasts with any background, so the focused control is findable over panels and over the 3D canvas alike.
Sources: https://learn.microsoft.com/en-us/windows/apps/develop/input/guidelines-for-visualfeedback https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml https://www.w3.org/WAI/WCAG22/Understanding/focus-appearance.html https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/tier-2 https://developer.android.com/design/ui/desktop/guides/interaction/keyboard

## Honour Windows contrast themes with system colour pairs (accessibility)
Contrast themes (Aquatic, Desert, Dusk, Night sky; toggle Left Alt + Left Shift + PrtScn) use palettes typically >= 7:1. Map every role to SystemColor PAIRS and never hard-code colours:
- WindowColor with WindowTextColor: pages, panes, headings, body text and non-interactive UI.
- ButtonFaceColor with ButtonTextColor: anything interactive.
- HighlightColor with HighlightTextColor: selected, hovered, pressed or in-progress UI.
- GrayTextColor ONLY for disabled UI, never for secondary text.
- HotlightColor ONLY for hyperlinks.
Add contrast-theme-only borders where two panes share a background, and use 2 px borders on flyouts and dialogs. Detect the theme through ThemeSettings.HighContrast. List items must invert their nested text on highlight. For ForgeShape: the desktop theme already flows through role tokens plus a user theme file, so a HighContrast role table that reads the system colours is a contained addition. The 3D viewport should still draw the selection outline in HighlightColor.

Seen in: Windows 11. Why: Users pick these colours for medical reasons. Respecting their pairs, rather than the app's aesthetics, is the only way the UI stays legible for them.
Sources: https://learn.microsoft.com/en-us/windows/apps/design/accessibility/high-contrast-themes https://learn.microsoft.com/en-us/windows/apps/develop/platform/xaml/xaml-theme-resources

## Mica only on the base layer and title area, never behind the viewport (visual)
Mica is an opaque material that samples the wallpaper ONCE (cheap). Apply it as the app's base layer and let it show in the title bar by extending into the non-client area. A content layer sits on top with LayerFillColorDefault (#4C3A3A3A dark, #80FFFFFF light). Mica Alt (stronger tint) suits tabbed title bars, with a commanding layer of LayerOnMicaBaseAltFillColorDefault. Mica falls back to solid SolidBackgroundFillColorBase (#202020 dark, #F3F3F3 light) when:
- transparency is off;
- Battery Saver is on;
- the hardware is low-end;
- the window is inactive;
- the build is below 22000.
Apply a backdrop only once per app, and set every layer above it transparent for it to show. For ForgeShape: Mica is optional polish for the title row and ribbon base. The viewport, panels and HUD should stay opaque, and the documented fallback is the plain #202020 ground.

Seen in: Windows 11 (File Explorer, Settings, Terminal). Why: The material signals which window is active and gives depth to chrome without touching content. Behind a 3D canvas any tint would bias the user's reading of shading and colour.
Sources: https://learn.microsoft.com/en-us/windows/apps/design/style/mica https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml

## Fitts' law placement: frequent at edges for the mouse, contextual next to the pointer (commands)
Movement time T = a + b·log2(2D/W): D is the distance; W is the target width along the motion, the SMALLER side for a rectangle. Bigger and closer targets are faster, and time grows logarithmically with distance.
With a mouse, screen edges and corners are effectively infinite targets (the macOS menu bar, the Windows taskbar). On TOUCH, edge targets are slower because fingers overshoot (NN/g citing Avrahami).
Consequences:
- Contextual menus appear next to their trigger, not in a distant sheet.
- Order linear menus by frequency.
- Make the whole icon-plus-label area clickable.
- Do not crowd small targets.
- Put controls used in sequence close together.
- Invisible padding helps only if users perceive it.
The Windows ribbon guide adds: users who work in one area of a large window for long periods benefit from mini-toolbars, palettes and direct commands, because the round trip to the ribbon is inefficient.
For ForgeShape: the canvas extrude cluster, the docked face actions and the action dock follow this. On a maximized desktop window, ribbon buttons and the History strip can extend their hit areas to the screen edge. On tablets, nothing important should rely on edge infinity.

Seen in: Microsoft Office ribbon (mini toolbar), macOS menu bar, Windows taskbar. Why: Pointing time is dominated by distance over size. Putting the next action where the pointer already is, or where it cannot overshoot, removes most of the travel cost of a long modelling session.
Sources: https://www.nngroup.com/articles/fitts-law/ https://smashingmagazine.com/2022/02/fitts-law-touch-era/ https://learn.microsoft.com/en-us/windows/win32/uxguide/cmd-ribbons

## State layers and motion timing as fixed numbers (feedback)
Material 3 state-layer opacities over the content colour: hover 8%, focus 10%, pressed 10%, dragged 16%. Disabled content is drawn at 38% opacity. Motion durations: Short1–4 = 50/100/150/200 ms; Medium1–4 = 250/300/350/400 ms. Windows caption and back buttons respond to rest, hover, pressed, active and inactive states, and hover uses subtle translucent fills (SubtleFillColorSecondary #0FFFFFFF dark, #09000000 light; Tertiary #0AFFFFFF / #06000000). The Android desktop tier asks for distinct, consistent hover states whose contrast supports accessibility. Fusion users have publicly complained that pre-selection highlights 'flash' annoyingly in dark mode. So viewport pre-highlight should be a calm, non-blinking change, consistent with ForgeShape's existing 220 ms selection pulse.

Seen in: Material 3, Windows 11, Autodesk Fusion (user reports). Why: Small fixed opacity steps make every control's states look like one family. Short durations (100–250 ms) acknowledge input without making an expert wait.
Sources: https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/StateTokens.kt https://raw.githubusercontent.com/androidx/androidx/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/MotionTokens.kt https://learn.microsoft.com/en-us/windows/apps/design/basics/titlebar-design https://raw.githubusercontent.com/microsoft/microsoft-ui-xaml/main/controls/dev/CommonStyles/Common_themeresources_any.xaml https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop https://forums.autodesk.com/t5/fusion-support/dark-mode-and-customizing-highlight-colors-to-prevent-flashing/td-p/13398636

## Stylus as a first-class precise pointer (input)
Identifying the tool and reading the axes:
- Identify the tool per pointer with getToolType(): TOOL_TYPE_STYLUS, TOOL_TYPE_ERASER or TOOL_TYPE_FINGER.
- AXIS_PRESSURE is 0..1; normalise values above 1.
- AXIS_TILT runs from 0 (perpendicular) to π/2 (flat).
- AXIS_ORIENTATION is 0..π clockwise and 0..−π counter-clockwise.
- AXIS_DISTANCE is hover height, which varies by vendor, so never rely on its exact value for critical function.
- Use getHistorical*() to read the samples batched between frames, and requestUnbufferedDispatch() to receive events as they arrive.

Palm rejection:
- Cancel the gesture on ACTION_CANCEL.
- On API 33+, also check FLAG_CANCELED on ACTION_POINTER_UP. Do not check only FLAG_CANCELED, because older APIs do not set it.
- On API <= 32, detection works only for single-pointer events.
- A stylus-only drawing mode is suggested as a way to ignore stray touches.

Rendering:
- The motion prediction library (API 19+) supplies temporary predicted points; never use them for the final stroke.
- Front-buffer rendering is for small areas only, never for full-screen pan or zoom, which tears.

Tier 1 stylus asks for draw and erase, low latency, pressure, tilt, palm rejection, and stylus drag and drop.

For CAD: a pen sketches with precision while fingers pan and pinch. Stylus hover can preview a snap or highlight a chooser target, which ForgeShape already does without committing.

Seen in: Android 10+ / ChromeOS, Samsung Galaxy Tab S-Pen. Why: A pen is as precise as a mouse yet stays on the touch screen. Separating the pen from the palm and from finger navigation is what makes direct sketching dependable.
Sources: https://developer.android.com/develop/ui/compose/touch-input/stylus-input/advanced-stylus-features https://developer.android.com/develop/ui/compose/touch-input/input-compatibility-on-large-screens https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/stylus

## DeX and desktop windowing readiness (input)
Samsung DeX requirements:
- Set android:resizeableActivity="true"; without it the app opens in a fixed-size window.
- Do NOT declare a required touchscreen (reqTouchScreen=finger or android.hardware.touchscreen required). That disables mouse and keyboard interaction, and the app will not launch in Desktop mode at all.
- Switching between phone and DeX triggers configuration changes: density, resolution, orientation, screen layout, screen size, smallest screen size, uiMode and colour mode. Use keepalive meta-data (com.samsung.android.multidisplay.keep_process_alive on DeX 3.0+) to avoid a restart.
- Fonts should scale as the window resizes.

Android desktop tier:
- multi-instance windows;
- requestFullscreenMode() for a full-screen toggle;
- registering as a file handler (for .forge);
- the OS file picker for import and export;
- printing or export to a printable format.

Recommended test sizes: Foldable 841 x 701 dp, 8-inch tablet 1024 x 640 dp, 10.5-inch tablet 1280 x 800 dp, 13-inch Chromebook 1600 x 900 dp, and a 160 ppi Googlebook.

Seen in: Samsung DeX, ChromeOS, Android desktop windowing. Why: DeX users get a resizable desktop window with mouse and keyboard. A fixed-size or touch-required manifest silently forfeits the desktop experience that the Windows shell offers.
Sources: https://developer.samsung.com/samsung-dex/modify-optimizing https://developer.android.com/docs/quality-guidelines/adaptive-app-quality/experiences/desktop https://developer.android.com/docs/quality-guidelines/adaptive-app-quality

### Numbers
- Android width size classes: Compact <600 dp, Medium 600–839, Expanded 840–1199, Large 1200–1599, Extra-large >=1600 dp; height Compact <480, Medium 480–899, Expanded >=900 dp (developer.android.com/develop/ui/compose/layouts/adaptive/use-window-size-classes)
- Compose adaptive pane directive: Compact and Medium 1 pane; Expanded 2 panes with a 24 dp spacer; Large+ 3 panes with a 24 dp spacer; default pane width 360 dp (412 dp at Large+); vertical split only in tabletop or Expanded-height single column, 24 dp spacer (androidx PaneScaffoldDirective.kt)
- Supporting pane split: Expanded 70% main / 30% supporting; Medium 50/50 or sheet; Compact below or bottom sheet; primary 'typically about two thirds' (developer.android.com canonical layouts)
- Material 3 navigation rail: collapsed 96 dp (narrow 80 dp), expanded 220–360 dp, item vertical space 4 dp, top space 44 dp, icon 24 dp, active indicator 56x32 dp, 3–7 destinations (androidx NavigationRail*Tokens.kt; material-components-android NavigationRail.md)
- Touch target >=48x48 dp on Android (adaptive Tier 2 Touch_Targets); M3 small icon button container 40 dp, icon 24 dp (SmallIconButtonTokens.kt)
- Windows touch target 7.5 mm = 40x40 epx at 135 PPI; standard density 40x40 epx grid; body text 14 px in both densities (learn.microsoft.com guidelines-for-targeting; Control size and density)
- WCAG 2.5.8 AA: pointer targets >=24x24 CSS px, or a 24 px-diameter spacing circle that intersects no other target (w3.org Understanding target-size-minimum)
- WCAG 1.4.3 AA: 4.5:1 text; 3:1 for large text >=18 pt (~24 px) or >=14 pt bold (~18.67 px); AAA 7:1; not rounded (w3.org contrast-minimum)
- WCAG 1.4.11 AA: 3:1 for control boundaries, state indicators, focus indicators and required graphics; disabled controls exempt; 2.999:1 fails (w3.org non-text-contrast)
- WCAG 2.4.13 AAA focus: area >= 2 CSS px perimeter, >=3:1 change between focused and unfocused (w3.org focus-appearance)
- Windows focus visual: primary border 2 px outside, secondary 1 px inside, 1 px margin; dark outer #FFFFFF / inner #B3000000 (learn.microsoft.com guidelines-for-visualfeedback; WinUI Common_themeresources_any.xaml)
- Windows title bar: 32 px tall (48 px with search or person picture); icon 16x16 at 16 px from the edge, 8 px top/bottom; title 16 px from icon, Caption style; caption glyphs E921/E922/E923/E8BB (learn.microsoft.com titlebar-design)
- Windows type ramp (epx size/line): Caption 12/16, Body 14/20, Body Strong 14/20 SB, Body Large 18/24, Subtitle 20/28 SB, Title 28/36 SB, Title Large 40/52 SB, Display 68/92 SB; minimum 12 px Regular / 14 px Semibold; no Bold or Italic (learn.microsoft.com typography)
- Material 3 type scale (sp): Label S 11/16, Label M 12/16, Label L 14/20 (Medium); Body S 12/16, Body M 14/20, Body L 16/24; Title S 14/20 M, Title M 16/24 M, Title L 22/28; Headline S 24/32 (androidx TypeScaleTokens.kt)
- Windows spacing: 8 epx between buttons, button↔flyout, control↔header; 12 epx control↔label and between content areas; 16 epx surface edge↔text; expander content indent 48 epx (learn.microsoft.com content-basics)
- Fluent 2 size ramp px: 0,2,4,6,8,10,12,16,20,24,28,32,36,40,48,52,56; 4 px base; breakpoints 320/480/640/1024/1366/1920 (fluent2.microsoft.design/layout)
- Windows corner radius: 8 px top-level (windows, flyouts, dialogs), 4 px controls, bars and tooltips, 0 px where edges touch or the window is snapped/maximized; ControlCornerRadius 4, OverlayCornerRadius 8 (learn.microsoft.com geometry)
- Fluent 2 radius: 0 bars, 2 badges, 4 buttons, 8 large buttons, 12 popovers/sheets; strokes 1/2/3/4 px web (fluent2.microsoft.design/shapes)
- Material 3 shape scale dp: 4, 8, 12, 16, 20, 28, 32, 48, full (androidx ShapeTokens.kt)
- Windows ribbon: <=7 core tabs (aim <=4); <=7 groups per tab; 3–5 commands per group; 32x32 icons for most-used, 16x16 others; in-ribbon gallery 48 px; up to 3 layouts per group for scaling (learn.microsoft.com win32 uxguide cmd-ribbons)
- WinUI CommandBar: icons 20x20 in the bar, 16x16 in overflow; max 4 primary commands at 320 epx width; labels right of icons on large windows (learn.microsoft.com command-bar)
- Material dark surface #121212 (M2); M3 dark Surface #141218, containers #0F0D13/#1D1B20/#211F26/#2B2930/#36343B, Primary tone-80 #D0BCFF (codelab; ColorDarkTokens.kt + PaletteTokens.kt)
- Dark text emphasis (M2 codelab): high 87%, medium 60%, disabled 38% white; M3 disabled opacity 0.38 (SmallIconButtonTokens/StandardIconButtonTokens)
- Windows dark fills: base #202020, secondary #1C1C1C, tertiary #282828, quaternary #2C2C2C; text secondary #C5FFFFFF, tertiary #87FFFFFF, disabled #5DFFFFFF; control strong stroke #8BFFFFFF; divider #15FFFFFF (WinUI Common_themeresources_any.xaml)
- Computed contrast (WCAG formula, alpha-blended): Windows dark text secondary 10.15:1, tertiary 5.51:1, disabled 3.34:1 on #202020; strong stroke 5.79:1; Windows LIGHT tertiary #72000000 on #F3F3F3 = 3.28:1 (fails 4.5:1); M3 Outline #938F99 on #141218 = 5.87:1; OutlineVariant #49454F = 1.99:1
- Blender default 3D viewport: background #3D3D3D graded to #303030; widget fill #545454; text #E6E6E6 (8.70:1 on #3D3D3D); selected widget #4772B3 = 2.72:1 on #303030 (fails 3:1), text on it 3.89:1 (fails 4.5:1); viewport select #ED5700 3.08:1, active #FFA028 5.33:1 (Blender userdef_default_theme.c, ratios computed)
- Windows dark status colours: critical #FF99A4 8.03:1, success #6CCB5F 8.03:1, caution #FCE100 12.34:1 on #202020; light-theme equivalents #C42B1C, #0F7B0F, #9D5D00 (WinUI theme resources, ratios computed)
- Material 3 state layers: hover 8%, focus 10%, pressed 10%, dragged 16%; motion Short1–4 50/100/150/200 ms, Medium1–4 250/300/350/400 ms (androidx StateTokens.kt, MotionTokens.kt)
- Fitts' law T = a + b·log2(2D/W), W = smaller side of a rectangular target; screen edges infinite for the mouse, slower on touch (nngroup.com fitts-law)
- Android Keyboard Shortcuts Helper: Meta+/ on API 24+, via onProvideKeyboardShortcuts(); tooltips via setTooltipText on API 26+ (developer.android.com)
- Stylus: AXIS_PRESSURE 0..1 (normalise >1), AXIS_TILT 0..π/2, AXIS_ORIENTATION ±π; FLAG_CANCELED palm rejection on API 33+; low-latency library API 29+; touch sampling 60–1000 Hz (developer.android.com advanced-stylus-features)
- Android adaptive test sizes: foldable 841x701 dp, 8-inch tablet 1024x640 dp, 10.5-inch 1280x800 dp, 13-inch Chromebook 1600x900 dp, Googlebook 160 ppi (developer.android.com adaptive-app-quality)
- Windows contrast themes typically >=7:1; 2 px borders recommended on flyouts and dialogs in contrast themes (learn.microsoft.com high-contrast-themes)
- Mica fallback: SolidBackgroundFillColorBase #202020 dark / #F3F3F3 light when transparency is off, Battery Saver is on, hardware is low-end, the window is inactive, or the build is <22000; content layer LayerFillColorDefault #4C3A3A3A dark (learn.microsoft.com mica; WinUI theme resources)

### Anti-patterns
- Choosing layouts by device type (isTablet) instead of the window size class. It breaks split screen, DeX windows and folding, and phone landscape (compact height) gets a two-pane layout it cannot hold.
- A pure-black (#000000) viewport or chrome, or large areas of 100% white text and saturated accents on dark. Material says these 'vibrate' and leave no room to show elevation. A mid-dark neutral, one step lighter than the panels, keeps black edges and dark shading legible.
- A mid-saturation accent fill as the ONLY selected-state signal. Blender's #4772B3 on #303030 is 2.72:1, below the 3:1 of WCAG 1.4.11, and white text on it is 3.89:1, below 4.5:1. Add an outline, check mark or weight change as a second carrier.
- Using a hairline divider token (M3 OutlineVariant ≈ 2:1) as the only visible boundary of an interactive field or button.
- Light-palette tertiary text (Windows #72000000 on #F3F3F3 = 3.28:1) for labels smaller than 18.67 px bold or 24 px regular. Disabled text is exempt, secondary information is not.
- Drag-only operations, such as divider resize, orbit, ring and arrow drags, or press-drag rectangle drawing, with no typed, tap-tap or button alternative (WCAG 2.5.7).
- Context menus reachable only by long-press, with no secondary-click equivalent, and no Esc to cancel a tool or transform (Android Tier 2 Context_Menus / Keyboard_Exit).
- Pointer hit areas larger than the drawn control on desktop, so neighbouring targets steal clicks. Conversely, Android touch targets smaller than 48 dp.
- Full-width bottom sheets, buttons and text fields on large screens (Tier 2 UI_Secondary_Elements). Editors need a maximum width.
- In a ribbon, hiding commands that do not apply so the groups reflow. Microsoft says to disable them instead, because hiding destabilises the layout. In ForgeShape, where unusable controls are not drawn, reserve the slot rather than reflowing.
- More than 7 ribbon tabs or groups, groups of only 1–2 commands, generic group names (Tools, Advanced), tooltips that just restate the label, or persisting the last-selected tab across launches.
- A custom title bar that loses the system behaviours: drag on empty space, double-click to maximise, right-click system menu, inactive-state dimming, text-scaling growth.
- Bold or italic UI text on Windows, or UI text below 12 px Regular / 14 px Semibold.
- Hard-coded colours that ignore Windows contrast themes. Also using GrayText for secondary text, or Hotlight for anything but links.
- Mica or acrylic behind the 3D viewport, or a backdrop applied more than once. Any tint biases how shading is read, and Mica falls back to solid #202020 anyway.
- Relying on Fitts edge 'infinity' for touch-primary controls: on touch, edge targets are slower because fingers overshoot.
- Front-buffer rendering for full-screen pan or zoom, which tears, and treating predicted stylus points as final geometry.
- Declaring a required touchscreen in the manifest, or a non-resizeable activity. On DeX either one disables mouse and keyboard or forces a fixed window.
- Shipping dark-only. NN/g finds light polarity better for normal-vision acuity, especially with small text. Offer both and follow the OS setting.
- Blinking or flashing pre-selection highlights in a dark viewport. Fusion users report it as annoying, so pre-highlight should be a calm, steady change.

### Notes
What was verified versus taken from general knowledge:

1. m3.material.io and m2.material.io pages are rendered client-side and returned only a title. Material numbers were verified from primary token source instead:
   - the androidx Compose Material 3 token files (TypeScaleTokens, ColorDarkTokens/PaletteTokens, NavigationRail*Tokens, ShapeTokens, StateTokens, MotionTokens, SmallIconButtonTokens);
   - the Compose adaptive PaneScaffoldDirective.kt, for pane counts, the 24 dp spacer and 360/412 dp pane widths;
   - material-components-android docs on GitHub.
   NOT verified: M3 window margin values (often quoted as 16 dp compact / 24 dp medium+) and the general 4/8 dp layout grid wording.

2. The #121212 dark surface comes from a search-result snippet of the M2 dark-theme page plus the Google codelab. The codelab also gave 87/60/38% text emphasis and the 2%/12% overlay examples. The full elevation-overlay table (dp to %) and the often-quoted 15.8:1 figure could not be verified.

3. Windows control-size facts come only from a search snippet of 'Control size and density' (the 40x40 epx standard grid, 14 px body, compact density for pointer input); the page itself redirects. Exact compact control heights (often 24 vs 32 epx) and Compact.xaml values could not be fetched. GitHub API access to microsoft-ui-xaml was not enabled; raw file access worked for Common_themeresources_any.xaml only.

4. Every contrast ratio marked 'computed' was calculated by me with the WCAG relative-luminance formula, alpha-blending translucent tokens over their base colour. They are arithmetic on published colour values, not published ratios.

5. Blender values come from its default theme source (userdef_default_theme.c). The claim that the viewport is deliberately lighter than the panels so dark shading reads is my inference from those values.

6. Autodesk forum pages returned 403. The Fusion 'Dark Blue' theme, canvas-follows-theme via Environment > Theme (default), and the complaint that pre-highlight 'flashes' in dark mode come from search-result snippets only.

7. The Samsung DeX optimisation page dates from the DeX 2.x/3.0 era and may be outdated; right-click, hover and minimum window size are not covered there.

8. The Fitts edge-on-touch claim is NN/g citing Avrahami. 'Magic corners' terminology is unsourced. The Smashing Magazine article was seen only in search results.

9. From general knowledge, not fetched:
   - Win32 high-contrast detection through SPI_GETHIGHCONTRAST.
   - Mica in a custom Vulkan/Win32 window through the DWMWA_SYSTEMBACKDROP_TYPE attribute (the Mica page links to 'Apply Mica in Win32 desktop apps', which I did not open).
   - Resolving right-click as a context menu versus right-drag as navigation by a travel threshold.
   - The specific ForgeShape mappings in each pattern ('For ForgeShape: ...'). These are my applications of the guidance, not vendor statements.

10. Tension the owner should decide: Microsoft's ribbon guidance says to disable rather than hide inapplicable commands, for layout stability. That conflicts with ForgeShape's rule that 'a control that cannot succeed is not drawn'. Material uses a 38% disabled opacity.

11. Navigation-rail guidance (3–7 destinations) is written for navigation, not tool selection, so applying it to the 8-entry sketch Tool Rail is advisory.
