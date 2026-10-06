# ForgeShape — Product Behaviour

Only behaviour that has been verified at runtime, through the real Android touch
path, is documented here. This file describes what ForgeShape does today and
nothing else — accepted-but-unbuilt direction lives in `PROJECT_STATUS.md`.

## What the viewport shows

The viewport shows the **Construction Bodies** in the scene. A Construction Body
is an exactly dimensioned shape — a **box**, a **cylinder**, a **sphere**, a
**cone**, a **capsule** or a **plane**. Its dimensions are real lengths in
**meters**, and they are what the shape is made from: what is on screen is
generated from those numbers. Where it sits is a separate matter of its position
and rotation.

There can be several bodies at once, and every one of them is drawn. Exactly one
is **selected**, and only the selected body is outlined; the rest are drawn
normally. Changing a body from one of those shapes to another changes what *that
body* is — it does not create a second one, and it does not touch any other
body.

ForgeShape opens on **Home**, not on a body — see *Starting a project*. A new
Construction Body, whichever way it is created, begins as a box **2.0 m wide ×
1.0 m high × 0.5 m deep**, centred at the world origin and unrotated, so it is
visibly not a cube; choosing a shape from Add Primitive replaces that box with
the shape you chose, and it is selected at once.

**Behind the bodies there is a grid** — a reference floor on the ground plane
through the world origin, with a line every **1 metre** and a stronger line every
**5**. It is what tells you how big something is, which way the world faces and
where the origin is, and it fades out with distance rather than stopping at a
visible edge. The two lines through the origin are tinted very slightly warm and
cool so the two ground directions can be told apart. It reaches about 20 m from
the origin in each direction.

The grid is drawn, and nothing more. **Nothing snaps to it**, nothing is measured
from it, it cannot be selected or sculpted, and it is never part of what you are
building — a body that sits exactly on the ground plane covers the grid rather
than fighting with it. It is on when ForgeShape starts and can be switched off
under **Display → View → Grid**; the choice lasts as long as the app is running.

## How the surface is shaded

The object is drawn as neutral grey clay, lit from the upper left with a soft
fill and a restrained sheen. The light travels with the camera, so orbiting never
makes a face appear to change: if the shading changes, the *shape* changed.

The point of it is that form is readable. Every shape reads as a **solid seen
from outside** — the surface facing you is the one you see, and a standard
three-quarter view of a box looks like a box sitting in front of you, never like
the inside of an open corner. A box shows six flat faces with genuine hard
corners and no rounded edges, and its left and right faces are clearly different
values rather than the same grey. A cylinder's side is smooth while its
end caps stay flat, with a crisp rim between them. A sphere is continuously
smooth all over, including at the poles. A cone has a smooth side, a flat base, a
sharp rim, and a clean point at the top. A capsule is smooth from end to end, with
no visible join where the rounded ends meet the middle — and a capsule as tall as
it is wide is simply a smooth sphere.

**Display** in the Global Toolbar opens a small panel. Every control in it works;
nothing there is drawn as a promise of something that does not exist yet.

*Shading* picks how the surface is lit:

- **Studio** — the default. Matte and even, best for judging flat faces and exact
  outlines.
- **MatCap** — glossier and more contrasty, best for reading curvature and for
  seeing what a sculpting stroke actually did.

*Surface* picks how the surface is smoothed:

- **Smooth** — the default. Curved surfaces look curved; only real edges look
  sharp.
- **Faceted** — every triangle is shaded flat, which deliberately shows the
  underlying triangles.

*Projection* picks how the camera flattens the scene onto the screen:

- **Perspective** — the default. The natural view: parts of the object that are
  nearer are drawn larger, and edges that are parallel in the world converge
  toward the distance. It reads like a photograph and is the better view for
  judging overall form.
- **Orthographic** — a parallel view. Two edges of the same length are drawn the
  same size no matter which is nearer, and edges that are parallel in the world
  stay parallel on screen. This is the view for judging exact geometry, because
  nothing is enlarged merely for being closer.

An edge pointing away from you still looks shortened in Orthographic. That is
foreshortening from *orientation*, which both projections have and which is
correct: what Orthographic removes is foreshortening from *distance*.

*View* picks what the viewport draws besides the model. Today that is two
things:

- **Grid — On** (the default) or **Off**. The reference floor described above.
  Turning it off leaves the bare model; turning it back on costs nothing and
  changes nothing about what you are building. The grid is correct in both
  projections.
- **Selection Outline — On** (the default) or **Off**. The thin outline drawn
  around the selected body's silhouette — see *Selection*. Turning it off leaves
  the model unmarked, and the Objects capsule still names the selected body;
  turning it back on costs nothing and changes nothing about what you are
  building. It is correct in both projections and for every kind of body.

Both choices last as long as the app is running and survive rotating the phone
and leaving the app, and neither is written into your project.

Switching between Studio and MatCap changes nothing but the picture: the object,
its dimensions, its placement and any sculpting are untouched, and so is what you
can tap. Smooth and Faceted are the same — the same triangles are drawn either
way. Perspective and Orthographic are the same again: they move the camera's
description of the scene, never the scene. None of them discards a dimension you
are part-way through typing.

Switching projection does not jump the frame. The object keeps its place on
screen and stays roughly the size it was, because the two views are matched at
the depth of the point the camera is looking at.

The panel stays open while you try several options, and the choices survive
leaving the app and coming back.

A selected object is marked by an OUTLINE around its silhouette rather than by a
tint, in both shading modes and in every appearance, so its own light and shade
carry its form unmodified while it is selected. Only the brief acknowledgement
at the moment of selection tints it at all. See *Selection*.

While sculpting, the lighting follows the surface as it moves: a stroke that
pulls a lobe out of a sphere lights that lobe immediately, with no stale shading
left behind.

## Starting a project

ForgeShape opens on **Home** whenever no project is open — on a fresh launch,
and after you leave a project. Home is a **full screen of its own**, with the
ForgeShape mark and name at the top, and it offers exactly two things: **New
Project** and **Open File…**. Nothing stands behind it: no editor, no default
body, no project, nothing to save and nothing to recover until you choose. Home
stays Home if you cancel the file picker, or pick a file that is not a
ForgeShape project, damaged, or from a newer ForgeShape — it says so on Home and
nothing is created.

**New Project** is the next page rather than a box over Home, and it asks which
representation the project begins in. Exactly two answers, and **Back**:

**CAD** opens a sketch straight away, flat on the XY plane and seen square-on,
with the grid filling the viewport — there is nothing to pick first. Draw a
closed profile, **Finish Sketch**, type a depth, and **Extrude** creates the
project with that extrusion as its first body. Until that Extrude there is no
project: **Back to Home** in the toolbar leaves with nothing to lose, and a
sketch that cannot be extruded creates nothing and says why. The new project
starts with an empty Undo history, as an opened file does.

Which plane you are drawing on is not fixed by that: the **orientation
navigator** in the top corner of the sketch names it (*XY · Along +Z · 0°*) and
changes it. Three chips choose XY, XZ or YZ; one control flips to look at the
plane from the other side; and **−90°** and **+90°** turn the drawing on screen
without moving anything you have drawn. The plane can be changed while the
sketch is still empty — once there is geometry on it the numbers mean that
plane, and ForgeShape will not quietly reinterpret them somewhere else, so it
says the plane is fixed instead.

**Sculpt** begins on a sphere already prepared for sculpting, so the first thing
you can do is make a stroke. Nothing is skipped to get there — the sphere is an
ordinary exact Construction sphere and it was prepared the ordinary way, so
**Back to Construction** shows that sphere with its diameter, and **Resume
Sculpt** returns to the same sculpt mesh with the strokes already on it.
Preparing it is not your first Undo.

**Open File…** opens a `.forge` from your files straight into the editor.

Once a project is open, the Project surface offers **New Project…** beside Save,
Open Saved Project, Save Copy…, Open File…, Share Diagnostics… and Import GLB….
Starting a new project or opening one while the current project has changes
you have not saved asks first — **Save and continue**, **Discard changes**, or
**Cancel — stay in this project**. Save writes the project to this device and
only then continues; if it cannot, the project stays open and the question
stays. Discard continues without writing anything. Cancel, and System Back,
leave the project exactly as it was. A project you just saved, opened or
recovered is not asked about.

System Back always goes one step: it cancels the question you are reading,
takes the New Project page back to Home, takes a New-CAD sketch back to Home
(nothing was created, so there is nowhere else to go), and from Home leaves the
app.

Neither choice is a document, a template or a saved project, and neither closes
anything off: both representations stay one control apart for the life of the
body.

## The Editor Workspace

The editor is one workspace, and **the model is the workspace**. Everything else
stands at an edge and is small, and nothing at all sits across the bottom of the
screen waiting to be used.

There is no bar across the top: the **Global Toolbar**'s controls sit in two small
floating groups with the model visible between and behind them — an editing group
on the leading edge carrying what is being edited and the control that crosses
between constructing and sculpting, and a utility group on the trailing edge
carrying the viewport's display settings and the control that hides the chrome.

**The workspace at rest says nothing, and draws nothing to say it.** A status or
error message appears in a quiet capsule of its own just below the toolbar, sized
to its own text, and then it goes: an acknowledgement after a few seconds, a
rejection after longer, because a rejection explains a constraint and has to be
readable to the end. A newer message replaces an older one outright rather than
waiting behind it. When there is nothing to report the capsule is not on screen
at all. The one thing that stays is a standing fault — the warning that the
constructed shape has changed under a sculpt — which is still true after any
message that covers it and comes back when it does.

Nothing there ever tells you how to use ForgeShape. That line reports what just
happened and nothing else: it does not caption the mode you are in, and it does
not describe the panel you have open, because the rail, the control that opened
the panel and the panel's own title already say those things. Choosing
Construction and switching between shape and placement are silent.

The control that crosses between constructing and sculpting is a single control —
one pill, not a button set into a second one — and it is never abbreviated where
the window can carry its wording. **Back to Construction** is the way out of
Sculpt Mode, so on a window too narrow for the sentence it reads
**← Construction** rather than trailing off mid-word; it is announced as *Back to
Construction* either way.

Over an object that came from a `.glb` the same control reads **Back to Imported
Mesh**, and **← Imported Mesh** where the row is too narrow, because that is
where it actually goes — an imported object has no constructed shape behind it.
It is announced as *Back to Imported Mesh* in both forms. The label beside it
names what you are looking at the same way: *Construction* for a body you built,
*Imported Mesh* for one that came from a file, *Sculpt* while you are sculpting
either.

Low on the leading edge, where a thumb reaches, is the **Objects capsule**: it
names the body you are working on and, while you are constructing, carries a
**+** beside it. It is in the same place whether you are constructing or
sculpting. The **+** is not there while you are sculpting, because ForgeShape
does not add bodies in Sculpt Mode — a control that could only ever refuse is
worse than one that is absent.

Opposite it, at the trailing end of the same bottom edge, is a second small
capsule holding **Undo** and **Redo** — see *Undo and Redo* below. The two
capsules do not join up: the model shows between them, so the bottom edge carries
what the project *is* on one side and what just happened to it on the other,
rather than a bar. That pair is drawn only while you are constructing.

At the trailing edge is one **right contextual surface**. Its top, right edge and
width do not move. In Construction it begins with **Shape** and **Transform**;
when Transform is held, Move/Rotate/Scale, World/Local where applicable, and the
exact-values control continue downward inside that same surface. In Sculpt the
four brushes and Details use the same external surface. Context can make its
bottom edge extend, but cannot split it into detached capsules or move its top.
When a window is too short — a large system font, a phone on its side, or the
keyboard up — the same vertical content scrolls inside the surface and no visible
target is squeezed below 48 dp. Exact/Details appears only when asked for and
goes away again when dismissed. Where the window opens it beside the model rather
than along the bottom — a phone on its side, a tablet — it takes the room between
the model and the right surface, and the right surface does not move over for it:
what you were reaching for is still under your finger when the panel arrives. A
utility control hides the whole right context at once, leaving the bare model and
one chip to bring it back.

Every panel in the workspace comes out of the control that opened it. The list of
bodies and the shape palette rise out of the Objects capsule; the exact values
unfold from the control beside the rail. Nothing arrives from an edge that has
nothing to do with what you just pressed. A panel stands over the model, which is
what makes it a panel in a viewport — but never over another control: the shape
palette is placed clear of the Tool Rail and the control beneath it rather than
half on top of them.

Those four task panels do not compete. Opening **Objects**, **Add Primitive**,
the exact values/details, or **Display** closes whichever of those was already
open. Display temporarily clears the trailing controls from its region; they
return to the same anchor when it closes. Moving between Construction and Sculpt
also closes the previous mode's task panel. On a compact window an Exact or
Sculpt Details sheet owns the lower region for as long as it is entering, open
or exiting. The Objects and Undo/Redo row hides instead of moving upward, then
returns to exactly the same resting place after the sheet is gone. The keyboard
uses the same rule and never creates a second horizontal right-control layout.

Touching the model between the two toolbar groups navigates: they are surfaces,
not a bar, and only the surfaces themselves take a touch.

**The system Back gesture closes what you opened before it leaves.** With the
shape palette, the list of bodies, the exact values or the display settings
open, Back closes that panel — with the same short animation its own control
gives it — and ForgeShape stays where it was. Back leaves the app only when there
is nothing open to close, which is exactly what it did before you opened
anything.

The same regions are there in both modes. What changes is what they carry, never
where they are. Each answers one question and only one: what mode this is, which
tool is held, what the scene holds, and what the selected body's numbers are.

**A bigger window buys more of the model in view at once, not just more space
around it — and it is the same workspace, not a different one.** On a phone the
list of bodies opens as a small panel over the model from the Objects capsule and
is dismissed again. On a tablet-sized window there is room to stop stacking
things, so while you are constructing the **list of bodies gets a panel of its
own** on the leading edge, carrying its own **+**. Everything keeps the look it
has on a phone: each panel stands clear of the window edge, is rounded on every
corner and is raised off the model, and the Tool Rail is the same floating card at
the same edge in every window. There are no flat panels welded to the sides of
the screen. The exact values still open only when you ask for them, in the same
way, from the same control.

**That extra panel appears only where it is worth having.** While you are
sculpting there is no permanent list of bodies even on a large window: you cannot
switch body or add one while sculpting, so a permanent list of them would be
furniture — and it would push **Radius** and **Strength** in off the edge and onto
the model, away from the hand that reaches for them. Sculpting on a tablet gets
the same Objects capsule a phone has, in the same place, and the brush controls
stay at the edge.

**None of those side panels is a full-height column.** Each ends where its own
content ends — a scene of two bodies is a short card, not an arm's length of
empty panel — and the viewport keeps the rest. A panel with more in it than the
window can show scrolls rather than stretching, and it ends on a whole row rather
than through the middle of one, so the boundary always reads as "there is more
below" rather than as something broken.

That only happens where it genuinely fits. A window that is merely wide — a large
phone in landscape — keeps the phone arrangement, because three permanent columns
there would take more from the model than they give back. Whichever window you are
in, the list of bodies behaves identically: the same rows, the same order, the
same way to add a body, and a body picked in the viewport highlights in the list
either way. A long list scrolls.

Touching any of that chrome never moves the camera and never sculpts: a surface
owns its own gesture completely. Touching the viewport anywhere else navigates
exactly as it always has. Every control answers the moment it is touched — it
lifts under the finger before anything else happens — and the control that is
currently active is a **filled** shape with a brightened label, so which tool is
held can be read without relying on colour. There is no outline around it: the
fill is the answer, and the blue accent is spent on the two things it is for —
the button that commits a change, and the ring around the field the keyboard is
typing into. A lit control takes the same shape as the group it sits in, so it
reads as that group's own state rather than as a box dropped inside it. Resting
controls draw no box at all: what separates them from the surface under them is
one step of tone and the space around them, and the only resting outline left in
the product is the one around a value you can type into.

**Everything you can touch is at least 48 dp across.** That is the hit area and
not the drawing: the icons are the size they look, and the extra is reach. It
holds in portrait and in landscape, with a fingertip and with a stylus.

**ForgeShape comes in five appearances, and you choose which.** *Settings*
carries an **Appearance** group listing **Warm Graphite**, **Neutral
Charcoal**, **Light Charcoal**, **Warm Light** and **Cool Light**. The first
three are dark workspaces, and the viewport's own ground changes with each:
Warm Graphite — what a fresh install gives you — is a warm dark studio ground,
Neutral Charcoal is a cooler steel grey, and Light Charcoal is the lightest of
the dark three. Warm Light is a cream paper and Cool Light a steel paper: the
same controls, the same panels and the same grid, drawn as ink on a light
canvas, with the grid's lines sinking into the paper instead of lifting off
the floor and the status bar's icons turning dark to stay readable.

Switching changes how everything is drawn and nothing about what you are
drawing. The model, its exact dimensions, its placement, which body is selected,
whether you are in Construction or Sculpt, and any sculpting already done all
come through untouched. So does what you were doing: the display unit you had
chosen, the panel you had open and the editor you were in are all still there.
The whole workspace is rebuilt in the new appearance, so it blinks once, and
comes back on the Settings page you chose it from.

The choice is saved on the device: turning the phone, leaving and coming back,
and starting ForgeShape fresh all keep it. It is not part of any project — a
project file saved in Cool Light opens in whatever appearance the device it is
opened on has chosen.

Tools, modes and panels are named by drawn icons rather than by characters, and
a control that is reserved for a feature the product does not have yet is drawn
dimmed, says so when read aloud, and does nothing.

A tap on a Tool Rail entry selects that tool even if the finger drifts a little,
and dragging the rail scrolls it without selecting whatever it started on.

**Panels move, briefly, and never make you wait.** Every panel that opens from a
control grows out of that control — the list of bodies and the shape palette out
of the Objects capsule, the exact values out of the control beside the rail, the
display settings out of the button in the toolbar — and all four move the same
way, at the same speed, on the same curve. They leave immediately and settle at
the end, rather than starting slowly, because you have already decided to open
them. Arriving takes about a fifth of a second and leaving is quicker. Hiding and
restoring the controls fades them rather than blinking them away.

Every one of those can be interrupted: tapping again immediately reverses what is
happening rather than queueing behind it, so a panel can never be left half
visible or showing a control that disagrees with what it is doing. **The model
itself never animates**: nothing moves the viewport, nothing resizes it, and none
of this happens while your finger is on the model. If you are sculpting and reach
for a panel with a second finger, the panel simply appears — the stroke comes
first.

**If you have asked Android to remove animations, ForgeShape removes them.** Every
panel lands on its final state immediately instead of running a shortened
version, and a newly selected body is simply outlined at once, with no flash —
so selection is still just as easy to see, it simply takes no time. That works
because the outline is not an animation: it appears on the frame the selection
changes and stays.

The workspace re-arranges itself for the window it is in — a phone in portrait, a
phone in landscape, a split-screen half, a tablet — and the arrangement is
decided by the window's size, not by the device or the orientation. The model
stays visible in all of them. On a tablet the exact values open beside the model
instead of over it, and the controls do not grow: only the viewport does. Which
tool you are holding survives all of it — turning the device or resizing the
window never leaves the rail without a held tool.

Turning the phone changes how much of the model you can see, never what shape it
is. A sphere is a circle on screen in portrait and a circle in landscape, and a
box keeps its proportions in both; rotating the device only makes the picture
larger or smaller, and never stretches it one way. What you can tap follows what
you see, so a tap that hits the model in portrait hits it in landscape too.

## Several bodies, and which one you are editing

The **Objects capsule**, low on the leading edge, names the body you are working
on. Tapping that name opens the scene list, which rises out of the capsule: one
row per body, labelled `Body #1`, `Body #2` and so on, with the selected one
visibly active, and a **+** to add another. It closes again on a second tap, so
while it is shut it costs the picture nothing. On a window wide enough for it, and
while you are constructing, the same list is a permanent panel beside the model
instead, and the capsule goes away — there is never a second copy of it on
screen. The capsule is in the same place, and works the same way, in both modes.

The exact-value surface names the body it is editing — *Exact Shape — Body #1* —
so when you do open it, the numbers on screen always say whose they are.

**The + does not add a body by itself.** It opens **Add Primitive**, which grows
out of it: six shapes — Box, Cylinder, Sphere, Cone, Capsule and Plane — each
drawn as its own outline with its name under it. Choosing one creates a body that
*is* that shape, selects it, and puts the palette away. There is nothing else in
that palette: every shape it offers is a shape ForgeShape actually builds.

The **+** is there while you are constructing and not while you are sculpting, in
either presentation — the capsule's and the panel's. ForgeShape refuses to add a
body in Sculpt Mode, so there is nothing to be gained by offering the choice and
then declining it.

A new body starts at the shape's own default size, unrotated, at the world
origin. It therefore sits exactly on top of whatever is already at the origin;
move it with the ordinary Position fields. Nothing is offset for you, because
guessing where you wanted it would be worse than putting it somewhere you can see
and type over.

Selecting a body is two equivalent things: tap its row in the Objects list, or
tap the body itself in the viewport. Either way the shape fields, the placement
fields and the highlight all move to that body together — they cannot disagree,
because they are all reading the same one answer. Selecting a body changes
nothing about the model: nothing is rebuilt, nothing is republished, and no
body's identity changes.

**Every edit applies only to the selected body.** Changing a shape, typing a
dimension, applying a placement, Freezing and sculpting all reach that body and
no other. Switching to another body and back brings the first one's exact
numbers back unchanged.

What the scene deliberately does **not** offer: no groups, no nesting, no
reordering the list, no multi-select and no drag and drop.
Adding, deleting, renaming, hiding, locking and duplicating are the verbs the
list has, and every one of them is undoable — see
*Undo and Redo*.

### Deleting a body

Each row in the Objects list carries a **Delete** beside its name. It removes the
object from the project — not just its row: the body stops being drawn, stops
being selectable in the viewport, and is gone from the file the next time you
save, from the automatic checkpoint, and from anything you export.

**It is one Undo away, and that is why it does not stop and ask.** Undo brings
the object back exactly as it was — the same shape or the same imported geometry,
the same position, the same sculpting on it if it had any — in its own place in
the list. Redo removes it again. The status line says so when you press it.
ForgeShape asks before exactly one act, *Reset Sculpt from Shape*, because that
one genuinely cannot be undone; putting a dialog in front of a reversible act is
how people learn to dismiss the one that matters.

**Selecting a body and deleting it are two different taps.** The name selects,
the control beside it deletes, and neither is ever reached by the other.

**The last object cannot be deleted.** A project always has at least one, so the
last row simply does not offer the control, and ForgeShape will not replace a
deleted object with a new box you did not ask for.

**Delete is not offered while sculpting**, for the same reason the **+** is not:
the body being sculpted has to stay the one being sculpted, and Undo is not
available in Sculpt Mode either. Leave Sculpt and the control is there again.

Deleting works the same way on every kind of object — a constructed body, an
imported mesh, and either of them with sculpting on it. What comes back on Undo
is the whole object, sculpting included.

### Renaming, hiding, locking and duplicating

Beside each row's name is a **⋯**. It opens that row's own small strip of
commands, just underneath the row, pushing the rows below it down rather than
covering them. One row is open at a time, a second press closes it, and System
Back closes it before it closes anything else.

The strip holds five things, on two lines so every one of them stays a full-size
target:

**Rename.** The row turns into a field holding the object's current name. Type,
and either press the keyboard's **Done** or the control beside the field. The
new name appears wherever the object is named — the row, the Objects capsule,
the panel title, the status line — because there is one name and every surface
reads it. Accented letters, other scripts and emoji all survive exactly. An
empty name is refused and the old one stands: a nameless object falls back to
*Body #3*, and that is what an object with no name gets, not something you can
type your way into. Back, or closing the strip, cancels and changes nothing.

**Show / Hide.** A hidden object stops being drawn and stops being selectable in
the viewport — you cannot tap it by accident, and it grows no selection outline.
It is **not** deleted: its row stays where it was, you can still select it from
that row, it is still in the file you save and in anything you export, and
showing it again brings it straight back. Hiding the object you are working on
does not move your selection, so Show is one tap away. The glyph is an open eye
or a struck-through one, so the state reads without depending on colour.

**Lock / Unlock.** A locked object stays visible and stays tappable — what it
refuses is being **moved**. The Move/Rotate/Scale handles do not appear over it,
and typing a position, rotation or scale for it is refused with a message saying
so. Everything else still works: you can select it, rename it, hide it,
duplicate it, unlock it, and delete it. Locking is not a permission or a
password; it is a switch you set and clear.

**Duplicate.** You get a second object with the same shape (or the same imported
geometry, or the same sketch and extrusion), in the same place, with the same
visibility and lock, named *Bracket copy* — then *Bracket copy 2*, and so on.
If the original had sculpting on it, the copy carries that sculpted shape too,
but starts with its own clean sculpt Undo history: the strokes you took on the
original belong to the original. The copy becomes the selected object.

One object that cannot be duplicated is **a CAD body whose sketch sits on
another body's face**. Where such a body sits is decided by the face it stands
on, so a copy would sit in exactly the same place forever with no way to move it
off — ForgeShape says so and creates nothing rather than leaving you a copy you
cannot separate. A CAD body on a world plane duplicates normally, and so does a
body that other sketches are standing on.

**Mirror.** You get a second object that is the **reflection** of the first
across one of the three world planes. Choosing **Mirror** replaces the strip
with three buttons — **XY**, **XZ** and **YZ** — and picking one is the whole
act. XY reflects up-down front-back (the Z direction), XZ reflects up and down
(Y), and YZ reflects left and right (X); each button says which in full to a
screen reader. Nothing happens until you pick one, so Back closes the chooser
and costs you nothing.

The reflection is a real, separate object: same shape, same size, mirrored
position and orientation, the same visibility and lock, named *Bracket Mirror* —
then *Bracket Mirror 2*, and so on. It becomes the selected object, and you can
move it, resize it or delete it independently from then on. Mirroring is a
one-off act and not a live link: changing the original afterwards does not
change the reflection.

**Only a shape object can be mirrored, and only one without sculpting on it.**
An imported mesh, a CAD body and an object you have sculpted have no Mirror in
their strip at all, because reflecting them exactly is not something this
version does — and an object turned inside out would be worse than no answer.

**Each of the five is one Undo.** Undo puts the old name back, shows a hidden
object again, unlocks a locked one, or removes a duplicate or a reflection; Redo
does it again. Names, visibility and lock are saved with the project and come
back when you reopen it, and so is a reflection — it is an ordinary object from
the moment it exists. A project saved by an older version of ForgeShape opens
with everything visible and unlocked, which is what it was.

**None of the five is offered while sculpting or while a sketch is open**, for
the same reason the **+** and Delete are not: the scene holds still there. Leave
Sculpt, or finish or cancel the sketch, and the **⋯** is back.

There is no multi-select, no grouping, no nesting, no reordering and no drag and
drop. The list is a flat list of objects.

## Sketching a CAD Body

The **+** offers one more thing beside the six shapes: **New Sketch**. Choosing
it puts the three principal planes — and, in a CAD project, the flat faces of
your CAD bodies — into the viewport as targets: tap one to highlight it, tap it
again to sketch on it. **Choose a plane by name** under the tile still lists
**XY** (the front view), **XZ** (the top view) and **YZ** (the side view) for
anyone who prefers the words. Either way the viewport becomes that plane: the
camera looks straight at it, the view goes orthographic so equal lengths are
equal on screen wherever they fall, a grid appears on it, and the toolbar reads
*Sketch XY*. Your own viewpoint is kept and comes back the moment the sketch
ends, whether you extrude it or cancel it.

**One finger draws; two fingers pan and zoom.** While a sketch is open a single
finger never orbits — the plane would slide away under the point you are
placing — but pinching and two-finger panning work exactly as they always have.
The Tool Rail carries the seven sketch tools:

- **Rectangle** — drag from one corner to the opposite corner.
- **Circle** — drag from the centre out to the radius.
- **Line** — drag from one point to another.
- **Polyline** — tap point by point. Tap the first point again to close the
  loop; tap the last point again to end it open.
- **Arc** — drag the two ends to set the chord, then tap the point the arc
  passes through. Three points on the curve, so there is nothing ambiguous
  about which arc you meant. Three points in a straight line describe no arc,
  and ForgeShape says so rather than drawing a line you did not ask for.
- **Spline** — tap point by point; tap the last point again to finish. The
  curve passes exactly through every point you place, so moving one moves the
  curve there.
- **Select** — tap an entity to select it. Its exact values appear in *Sketch
  values*, and *Delete entity* removes it.

An Arc and a Spline are ordinary sketch entities: selectable, deletable, and
able to close a profile with lines or with each other, as long as their ends
meet. A closed profile containing a curve extrudes exactly like any other.

Points **snap**, always: to an existing endpoint, corner or centre first, and
otherwise to the grid, whose spacing adapts to how far you are zoomed in (1, 2
or 5 times a power of ten metres) so it stays a useful size. A snapped point is
exact — the other entity's own value, or an exact multiple of the grid spacing
in force when the drag began — never a rounded pixel. A small cross shows where the
point will land. What you type is never snapped: select a rectangle, type
`1.234567` for its width in *Sketch values*, press *Apply*, and that is its
width.

**A selected line shows its length, and you can type one.** Select a straight
line and the length is written beside it in your chosen unit. It is meant to be
dimensioned the way a technical drawing dimensions it — extension lines out from
each end, a dimension line between them with a tick at each end — and **that
annotation is drawn**, in the same warm colour a held gizmo handle takes, since
`UI-3D-STATE-C2` (2026-09-08). It was invisible until then, because the renderer
had no case for its drawing style; that was `UI3D-F-005`, and it is closed. The
number itself is drawn, is correct, and stands on the annotation it belongs to
since `UI-3D-STATE-C1` corrected where every
viewport-anchored label in the product sits. Tap the number
and a small field opens right there: type an exact length, press *Apply*, and
the line takes it. **The first end stays where it is and the direction does not
change** — only the far end moves, along the line it was already on. Nothing
else in the sketch moves with it: there is no constraint solver here, and if
the line was part of a loop, shortening it opens that loop. ForgeShape says so
when you try to extrude rather than quietly repairing the sketch. Zero, a
negative number and anything that is not a number are refused, the field stays
open so you can correct it, and nothing you drew moves.

**Finish Sketch** is the one way forward. ForgeShape checks the sketch and
finds every closed loop — a rectangle, a circle, a closed polyline, or a loop
of lines and curves whose ends meet — and refuses, saying why, when there is
none: an open polyline, a chain with a free end or a fork, a loop that crosses
itself, a loop with no area. Nothing is repaired for you, and the sketch stays
editable.

**What you extrude is a region, and a loop inside another is a hole.** Each
closed loop encloses one region: the area inside it, minus any loop drawn
cleanly inside it. A rectangle with a circle in the middle therefore offers
two regions — the disk, and the rectangle with a round hole — and extruding the
second makes a block with a real hole through it. When the sketch has exactly
one region it is chosen for you; when it has more, **none is chosen**: the
status line says how many were found and Extrude is not offered until you tap
the region you want. A tap chooses it, a second tap lets it go, and **a tap
never changes any region but the one under your finger**. Choosing several
regions extrudes them **together, as one shape**: with the ring and its disk
both chosen the hole is filled, so a rectangle holding circles A and B, with
the rectangle and A chosen, becomes a block with only B's hole, and all three
chosen make a solid block. When curves cross, each enclosed cell is its own
choice, and **any cell can be added or removed in any order** — no tap is ever
refused because of what else is chosen. Cells that share an edge merge into
one shape; cells that only meet at a point, or not at all, stay separate pieces
of the same solid. Whether the chosen set can be extruded (for example an Add
that would not touch the body) is said on the operation badge and by the
missing Extrude, and your choice stays exactly as you made it. A finger that
wobbles a little while tapping neither turns the view nor misses its cell, and
a cell is tappable from either side of the sketch plane. A still tap on the
arrow's shaft chooses the cell under it; a drag from the shaft changes the
distance; a tap on the arrow's head belongs to the arrow.
There is no limit of sixteen: every cell a sketch's curves make can be chosen
(a 24-cell grid takes all 24, one tap at a time), and the status line counts
them — *Regions in the extrusion: N.* The first Extrude of a brand-new CAD
project extrudes the chosen cells like any later one. "Several profiles are
closed — choose one" is said only when nothing is chosen; an Extrude that is
refused names its own reason (for example cells that meet only at a point),
and changing the choice replaces that message with the new count.
What is chosen is hatched as one shape, and a hole it leaves open stays
empty. The first
region you choose also turns the view to one the arrow can be dragged in.
Loops that touch or cross each other are never holes of each other; each stays
its own region.

Once a region is chosen the toolbar reads **Extrude**, *Back to Sketch* under
the rail returns to drawing, and the drawing tools, the plane navigator and a
selected line's dimension step out of the way — the question now is the
extrusion. The exact-value panel no longer opens by itself: its toggle opens it
on the regions (each with its hole count), the operation and the depth.

**The extrusion is controlled at the geometry itself.** An **arrow** is drawn
along the direction the solid will grow, standing on the chosen region, and its
length is the distance it grows. Beside it runs a **dimension line**, the way a
technical drawing measures a length: two short extension lines out of the
arrow's base and tip, the line between them and a tick at each end. The **exact
distance** stands just above that line, turned to run along it and never upside
down; tap it to type one (the field opens level, where it is easy to type). It
is written to three decimals in the display unit — `2.359 m`, never a long
string of binary digits — while the field it opens still holds every digit.
Just past the arrow's point stands one small **badge** showing the operation
(a separate box for New Body, a solid with a plus for Add, a solid with a notch
for Cut). It is a little plate standing in the model on the line of the arrow,
so it tilts and foreshortens with the arrow as you orbit, follows it smoothly,
and turns the right way up to read; it never slides along the screen edge or
jumps to the other side of the arrow. When it would be cut off by the screen
edge, or you look almost straight down the arrow, it is simply not shown — it
fades out as you approach that view. Tap the badge and one readable panel
opens with every choice at full size — One Side, Symmetric or Two Sides; the
operations this sketch can make; Flip. The place you tap is always at least a
full fingertip wide, but only the badge and that fingertip-sized square take a
tap: a cell just beside the badge can still be chosen. Zooming out makes the
arrow head, the line, the badge and the number smaller with the model, and
zooming in makes them larger, both within limits. Far enough out that the
number would be wider than the line it measures, the number steps aside, and
the exact fields stay one tap away on the panel toggle. With **Tool labels**
turned on in *Settings → Interface*, the choices in the open panel carry a
one-word caption. A distance typed at the arrow and the same number typed in
the panel are the same thing; there is one extrusion, and both
places show it. These sizes are provisional and are being tuned on a physical
phone.

**New Body, Add or Cut.** A sketch on one of the three planes makes a new body —
New Body is the only operation there, and the badge simply says so. A sketch on
a flat face of a CAD body offers all three: **Add** grows material onto *that
same body* and **Cut** takes material out of it, and neither creates another
object — the Objects list does not change, the body keeps its name and its
place, and one Undo takes the feature back. Choosing Cut points the extrusion
into the body; Flip still turns it round. The viewport previews exactly what
Extrude will make — tinted green for Add, red for Cut, blue for a new body, and
each also recognisable by its icon — and when the result would not make sense
(an Add that does not touch the body, a Cut that misses it or would remove all
of it) the operation badge says so by name and Extrude is not offered.

The dimension line, its number and the badge **follow** the arrow through a
drag, an orbit and a zoom, and so does the **Edit Sketch** control below. When
the line runs off the screen, the number stands on the part of it that is still
visible.

**Three ways for a solid to reach out of its sketch.** The extent selector
offers exactly three:

* **One Side** grows the solid out of one side of the sketch. This is what an
  extrusion has always done. A **Flip** beside the value moves it to the other
  side without changing the distance by a hair.
* **Symmetric** grows it the same distance out of *both* sides. There are two
  arrows and **one** number, because there is one distance — and the number is
  the distance on **each side**, not the total thickness, so a symmetric
  extrusion of 0.75 m is 1.5 m thick. Dragging either arrow moves both. Flip is
  not offered here: both sides are reached already, so there is no side left to
  choose.
* **Two Sides** grows it a different distance out of each side. There are two
  arrows and two numbers, **Side A** along the sketch's normal and **Side B**
  against it, each above its own side's dimension line, and each moves only its
  own side — dragging one never disturbs the
  other. Flip is not offered here either; both sides are stated outright.

Switching between the three never loses a number and never invents one. Going
to Symmetric or Two Sides carries the distance you had onto both sides; coming
back to One Side keeps the side you last chose and its distance, and the
distance is always a positive length on a real side rather than a negative
number. One side of a Two Sides extrusion may be zero — the other one carries
it — and the value stays there to be typed back up. Zero on both sides, a
negative number and anything that is not a number are refused, the field stays
open so you can correct it, and nothing moves.

**Finish Sketch also turns the view.** While you are drawing, the view looks
straight down onto the plane you are drawing on, so a square you draw is a
square on screen. That is exactly the direction the solid grows, though, so from
it the arrow would point straight at you with no direction to pull it along. The
moment you finish the sketch, ForgeShape moves to a view that can see the
extrusion: your own earlier view of the model if it already looks at the solid
from an angle, and otherwise a fixed three-quarter view of the profile you just
drew. From there you can **drag the arrow** to set the depth directly, and orbit,
pan and zoom behave exactly as they do anywhere else in the viewport. A dragged
depth and a typed one are the same depth. Going *Back to Sketch* — or reopening
a sketch with **Edit Sketch** — puts the straight-on drawing view back, and
finishing again returns to a view you can drag in.

The distance and, for a One Side extrusion, the side are also still in the
panel: type the **depth**, choose **Along normal** or **Against normal**, and
press **Extrude** — from the toolbar or from the panel; they are the same act.
The side chips are absent for a Symmetric or Two Sides extrusion, because there
is no side to choose there; the extent itself is set at the geometry, where both
arrows can be seen. With New Body the
sketch becomes one new **CAD Body**: an ordinary object in the Objects list,
selected, with the ordinary Move, Rotate and Scale handles, and one Undo takes
the whole of it back. **Cancel Sketch**, under the rail, discards the sketch and
changes nothing at all; nothing you drew is saved or autosaved until you
extrude it.

**A CAD Body stays editable.** Its *Shape* panel shows the plane it was
sketched on, the profile's sizes — a rectangle's width and height, or a
circle's radius — the distance and, where there is a side to choose, the
direction; *Apply* regenerates the body
from the new values as one Undo step and leaves its placement exactly where it
was. A polygon profile shows its point count and is not numerically editable
yet. Saving, reopening and recovery keep all of this: a reopened CAD Body is
still its sketch and its extrusion, not a frozen mesh. Exporting a GLB writes
the body as it currently is.

**The sketch is still there after you extrude it, and the body says so.** A
single **Edit Sketch** chip stands on the body's own sketch in the viewport
whenever that body is selected — one tap, rather than opening a panel and
scrolling past the fields. It scales with the camera the same way the extrude
cluster does, and it is absent for anything that is not a CAD Body.

**Edit Sketch** is also beside those fields in the panel, and either route
reopens the sketch the body was made from. Everything the sketch tools do
applies — move a point, add a curve,
select a line and type an exact length — on the plane or face the body was
sketched on, with the navigator in the corner as always. **Nothing changes
until you finish**: the body on screen is still the body you had, and *Cancel
Sketch* leaves it exactly that way with nothing recorded. *Extrude* applies the
whole edit as **one Undo step**, and Undo puts the entire previous sketch back.
If another body is sketched on one of this body's faces, an edit that would
take that face away is refused and says so — the dependent is never quietly
broken.

**Every feature stays editable.** A CAD body built from several features lists
them in its *Shape* panel in order — the first extrusion, then each Add and Cut.
Tap one to reopen it: an Add or a Cut opens on its own extrusion, with its
operation and distance in the same compact row, and *Back to Sketch* reaches its
drawing. Finishing applies the edit as one Undo step, and everything after it
is rebuilt in order — deepen the first extrusion and an Add standing on its top
face rises with it. Saving and reopening keep the whole chain.

**Revolving a region instead of extruding it** (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`).
A new CAD body can also be made by turning the chosen region about a straight
edge of its own sketch. Draw the region and, if you want an axis apart from it,
a Line; after *Finish Sketch* open the exact fields and tap **Revolve…**. The
status line asks you to choose an axis: tap any straight edge of the sketch — a
Line, one segment of a Polyline, or one side of a Rectangle; a circle, arc or
spline is not an axis and says so. The view leans so the turn can be seen, and
the preview shows a **full turn (360°)**: the axis is drawn across the region,
with a ring, a handle on it and the angle written beside the ring. Drag the
handle round the ring to change the angle in whole degrees — dragging it never
turns the camera — or tap the angle and type an exact one (37.5° stays
37.5°). **Flip direction** turns the other way with the same angle, **Change axis** picks
another edge, and **Extrude instead** goes back to the extrusion with the same
region chosen. A region that crosses the axis line cannot be revolved and the
status line says so; the *Revolve* button is not offered until it can succeed.
A region that merely touches the axis makes a solid with no hole. *Revolve* in
the toolbar makes the body as one Undo step — in a new project it creates the
project. The revolved body's *Shape* panel lists it as a Revolve; tap the row
to reopen it with its axis, angle and direction, change them, and finish as one
Undo step. Saving and reopening keep the axis, the angle and the direction.

Not yet: a revolve that adds to or cuts from an existing body, a later feature
on a revolved body, sketching on a revolved body's faces, and an axis that is
not an edge of the sketch.

What a CAD Body does **not** offer yet: *Start Sculpting*. It is absent for one,
because the way back from Sculpt over a sketch needs its own design; every other
body sculpts as before.

**Sketching on a body's face** (`CAD-A3`). In a CAD project the flat faces of
your CAD bodies are targets too — both ends of an extrusion and every flat
side; the curved side of an extruded circle is not. Tap a face to highlight it,
tap it again to start sketching on it, looking straight at it. A stylus
hovering over a target highlights it without choosing it. Two fingers still
orbit and pan while you choose; System Back leaves without changing anything.
A sketch drawn on a body's face and extruded becomes a new CAD body that
**stays attached to that face**: move, turn or resize the body it sits on and
it comes along. You cannot delete a body while another still sits on its face —
remove the one on top first. The whole arrangement is saved and reopens with the
attachment intact. The sketch grid adapts to how far you are zoomed in, so it
stays a useful size, and a value you type is never nudged to it.

Not yet: sketching on an imported or sculpted surface; a curved face as a
sketch support; and moving a face-attached body on its own.

## Drafting inside a sketch

While a sketch is being drawn, a **Modify** control stands under the
orientation navigator in the upper corner. It opens the **sketch actions
palette**: *Select multiple*, *Dimension*, *Make Construction* (or *Make
Regular*), *Trim*, *Extend*, *Offset*, *Mirror* and *Delete*, plus the
*Dimensions* choice — *Selected*, *All* or *Off*. The palette offers only what
can work for what is selected: Trim and Extend are always there, Dimension and
Offset appear for one entity whose kind has them, and Mirror and Delete need a
selection. While a mode is active the same corner shows its name, what it needs
and *Done*; choosing a drawing tool ends it.

**Construction geometry.** *Make Construction* turns the selected entities into
construction geometry: drawn dashed and quieter, still selectable, snapped to
and dimensioned, but never part of a filled area — a rectangle made
Construction leaves nothing to extrude, and Finish says so. A construction line
can be the axis of a Revolve. *Make Regular* turns it back.

**Dimensions.** *Dimension* with an entity selected offers the measurements
that entity has — a line's Length and Angle (and Horizontal / Vertical), a
rectangle's Width and Height, a circle's Radius and Diameter, an arc's Radius
and Sweep. Tapping another entity while the mode is on makes it the next
target. A new dimension is **Driving** unless you switch the chip to
**Reference**. Each dimension is a number on the drawing: `R` for a radius, `Ø`
for a diameter, `°` for an angle, and a Reference in parentheses and paler.
Tap a Driving number to type a new value — plain numbers or simple arithmetic
such as `12+3` — and the geometry changes exactly: a line keeps its start point
and direction, an angle keeps the length, a rectangle and a circle keep their
centre. A Reference number only reads; tapping it offers *Delete*. *Selected*
shows the numbers of what is selected, *All* shows every one, *Off* none; the
numbers follow pinching and panning. Where two numbers would overlap, one is
hidden — the selected entity's own first, then Driving — rather than moved, and
a number that would not fit on screen is hidden rather than pushed in. **A
number never covers the line or curve it measures**, and a tap beside a number
reaches the drawing.

**Snapping while drawing.** A new point lands exactly on a nearby endpoint,
midpoint, centre, crossing of two lines, or the origin, or lines up
horizontally or vertically with one.

**Trim, Extend, Offset, Mirror.** *Trim*: tap the piece of a line, circle or
rectangle edge between two crossings and it is removed (a circle becomes an
arc, a rectangle becomes lines); the filled areas follow. *Extend*: tap near
the end of a line and it reaches exactly to the next line or circle in its way.
*Offset*: a parallel copy of a line, circle, rectangle or polyline, previewed
first; drag to set the distance or type it exactly, then *Confirm* — *Done*
without confirming creates nothing. *Mirror*: with *Select multiple* on, select
the entities, choose *Mirror*, tap a straight line as the mirror line, see the
preview, *Confirm*. An entity a dimension measures cannot be trimmed, and a line
whose length is Driving cannot be extended — the status line says so; delete
the dimension first. Offset and Mirror copy no dimension.

**Saving and editing later.** Construction geometry and dimensions are saved
with the project and come back on reopen, with nothing selected. Editing the
sketch of an extruded body — the *Edit Sketch* control on the body — or of a
revolved body — *Edit Sketch* in the body's precision panel — and changing a
Driving dimension or trimming regenerates the same body as one Undo step.

Not yet: a constraint solver that keeps relations, a "fully defined" state,
projecting edges of other bodies into a sketch, drawing sheets and title
blocks, and dimensioning, trimming, extending or offsetting a spline.

## Choosing and sizing the shape

The Tool Rail in Construction carries exactly two entries, **Shape** and
**Transform**, and both of them work. Nothing is drawn there that ForgeShape
cannot do.

Which one is held decides what the final control inside the right context opens. With Shape
held it opens **Exact Shape**: a **primitive chooser** — Box, Cylinder, Sphere,
Cone, Capsule and Plane — the chosen shape's dimensions, a display unit (**mm**,
**cm** or **m**) shared by every length, and an **Apply Shape** button. Only the
chosen shape's fields are on screen, so nothing there can be mistaken for another
shape's dimension. With Transform held it opens **Exact Transform**: the object's
position, rotation and scale, described further down, with its own Apply.

*Transform* is what the entry is called because that is what it is about — where
the body sits and how large it is drawn. Holding it also puts **handles on the
model** and the two controls that decide what they do; the surface it opens is
called *Exact Transform* because it is the typed half of the same thing, not
because it is the only half.

Either surface is one tap away and goes away again on the next one. While it is
shut the model has the whole screen; while it is open the rest of the viewport
stays visible and usable.

The numbers in the fields are always the object's real dimensions, read from the
model itself.

| Shape | Authored by | Defaults |
| --- | --- | --- |
| Box | width, height, depth — its extents, centred on its position | 2.0 × 1.0 × 0.5 m |
| Cylinder | diameter and height, standing on its own vertical axis | 1.0 m across, 2.0 m tall |
| Sphere | one diameter, reaching exactly half of it in every direction | 1.0 m across |
| Cone | bottom diameter and height | 1.0 m across, 1.0 m tall |
| Capsule | diameter and **total** height, ends included | 1.0 m across, 2.0 m tall |
| Plane | width and depth, a flat sheet centred on its position | 1.0 × 1.0 m |

Curved shapes are authored by **diameter**, not radius, because that is what a
drawing and a caliper give you. How finely a curved surface is divided is **not a
setting**: it is a detail of how the exact shape is drawn, not part of what the
shape is, so there is nothing to choose and nothing to get wrong.

### A cone comes to a point

A cone stands on a **flat, closed circular base** and comes to a **point** at the
top — looked at from underneath it is a filled disc, not a hollow shell. There is
no top diameter: the tip is a point, not a small circle, and a shape with a flat
top of its own size is a different shape, not a cone with an extra number.

### A capsule is a middle plus two rounded ends

A capsule is a straight cylindrical middle with a **rounded hemispherical end**
on each end, like a pill. The middle is not entered separately: it is whatever is
left after the two rounded ends, so the total height is enough to say what the
capsule is. The ends are true half-spheres of the capsule's own radius, so the
shape is exactly its diameter across at the waist and exactly its total height
from tip to tip.

Because the two rounded ends alone are already as tall as the capsule is wide,
**the total height cannot be less than the diameter**. Entering one that is says
so in as many words and changes nothing:

> Rejected: Total Height cannot be less than Diameter — the two rounded ends
> alone are that tall. Object unchanged.

A total height *equal* to the diameter is perfectly valid — it is a capsule with
no middle at all, which is a sphere, and it is drawn as a clean one.

### A plane is a flat sheet, visible and usable from both sides

A plane is a flat, **zero-thickness** rectangular sheet — width across, depth
front-to-back, centred on its position. It is not a thin box: there is no third
dimension to type, and looked at edge-on in Orthographic it draws as a true
line, not a sliver. It has one canonical front, but the viewport shows and
shades a plane correctly from either side, and tapping either side selects it,
because a flat sheet has no "inside" the way a box or a sphere does. Rotating
it — with the same Position/Rotation fields every shape uses — is how its
facing is changed; there is no separate orientation control.

**Sculpting a plane keeps it two-sided.** Once it is being sculpted, the sheet
still draws, selects and *sculpts* from either side: a brush works on the
underside exactly as it does on top. Being usable from both sides belongs to the
sheet itself, so it survives Start Sculpting and it stays true no matter what the
Construction shape is later changed to. The converse holds too: a box or sphere
being sculpted stays one-sided — it keeps its inside — even if the Construction
shape is afterwards changed to a plane. What decides is the mesh being worked on,
never whichever shape the Shape selector happens to be showing.

### Choosing a shape changes nothing by itself

Moving the Shape selector only swaps which fields are on screen. The object stays
exactly what it was — same shape, same size, same place — until **Apply Shape**
is pressed.

Each shape remembers its own dimensions, so switching Box → Sphere → Cylinder →
Box brings each one's numbers back rather than resetting them, and the sets never
mix — including a cone's `(diameter, height)` and a capsule's `(diameter, total
height)`, which are the same two numbers in the same order and mean different
things.

### Typing never changes the object

Editing the text does nothing on its own. The object changes only when **Apply
Shape** is pressed. Nothing is applied field by field: the shape and all of its
dimensions are submitted together, and the result is one of three things.

- **Applied** — the object takes the new shape and size, the viewport shows it
  immediately, and what can be tapped is the new surface. Entering 125, 250 and
  75 in cm gives a box of exactly 1.25 × 2.5 × 0.75 m.
- **Unchanged** — the values are valid but describe the object it already is, so
  nothing happens at all. This is judged by the actual shape and size, not by the
  text: 125 × 250 × 75 cm and the same box written as 1.25 × 2.5 × 0.75 m both
  report Unchanged.
- **Rejected** — the status line shows what is wrong and the object does not change.
  Not the dimensions that were fine, not the shape, and not where it sits.

### Choosing a display unit

Switching between mm, cm and m rewrites every length into the chosen unit and
does nothing else. The object is not touched, not redrawn and not re-uploaded;
only the way the same size is written down changes. 2 m, 200 cm and 2000 mm are
the same length. The conversion is exact, so switching m → cm → mm → m returns
the original numbers digit for digit and no amount of unit switching can drift
the object's real size. Values are written without a thousands separator and
without trailing zeros.

### Typing dimensions

The fields take a numeric keypad. Either `.` or `,` may be used as the decimal
separator — typing `333,3` in cm produces exactly 3.333 m, and the field is
rewritten as `333.3` once applied. Letters cannot be typed into a dimension field
at all.

Dimensions are not silently corrected. An empty field, a value of zero or less,
and text that is not a number are each refused with a message naming the field,
and the offending field is focused ready to be fixed — a zero diameter is
reported as "Diameter must be greater than 0" and `1.2.3` as "Diameter is not a
number". The object keeps the shape, size and place it already had.

## Moving, rotating and sizing the object

Holding **Transform** in the Tool Rail gives you two ways to place the object,
and they are two front ends to the same thing. **Handles in the viewport** are
the direct one: grab one and drag. **Exact values** are the precise one: type a
number and Apply. Whichever you use, the object's **placement** is one set of
numbers — a Position X/Y/Z in the shared display unit, a Rotation X/Y/Z in
degrees and a Scale X/Y/Z as plain multipliers — so a drag shows up in the
fields the moment you open them, and a typed Apply moves the handles at once.

### Dragging the handles

Holding Transform draws a small instrument on the active body, and two controls
beside the Tool Rail decide what it does. The first chooses **what kind** of
change the handles make:

- **Move handles** — three arrowed shafts, one per axis, meeting at a marked
  point, plus three small squares between each pair of axes. Drag a shaft and
  the object slides along that axis only; drag a square and it slides in that
  plane, never out of it.
- **Rotate handles** — three rings, one per axis. Drag one and the object turns
  about that ring.
- **Scale handles** — three shafts ending in cubes, the same three plane
  squares, and a cube at the centre. Drag an axis cube and the object stretches
  along that axis; drag a plane square and it stretches in two at once; drag the
  centre cube and it grows or shrinks as a whole, keeping its proportions.

The second control chooses **which axes** the handles point along:

- **World axes** — they keep pointing the way the world does, however the object
  is turned.
- **Local axes** — they turn with the object, so a Move handle slides it along
  its own length rather than along the world's.

Scale is always about the object's own axes, so the World/Local control is not
drawn while Scale is held. When you leave Scale, the choice you had before comes
back.

The three axes are coloured to match the world grid's own axis lines, and a
plane square takes the colour of the axis it faces. Whichever handle you are
holding is drawn in a warm highlight while the others fade back, so there is
never any doubt about what a drag is about to change.

**Each kind of handle is drawn as its own kind of mark**, so which is which does
not rest on position or colour alone. An axis is a band ending in a closed
arrowhead for Move or a cube for Scale; a plane is a square with its diagonals
crossed through it, which reads as a surface rather than as more wireframe; the
point everything happens about is a small neutral cross belonging to no axis; and
in Scale the centre cube — the one handle that acts on all three axes at once —
is the largest mark on the instrument.

The point everything happens about is the object's own position, so a rotation
turns it in place rather than swinging it around the origin, and a scale grows it
about itself rather than sliding it away. A touch right at that point grabs
nothing in Move and Rotate — every handle meets there, so it would be a coin toss
— and in Scale it is the centre cube.

The handles stay about the same size on screen however far the camera is, and
**however large the object is**: stretching a body does not stretch the
instrument you stretched it with. They never shrink to nothing when you pull back
or fill the viewport when you push in, and they are drawn over the model on
purpose — a handle you can see the effect of but cannot reach would be worse than
one that overlaps.

A handle pointing almost straight at you cannot be grabbed, and a plane seen
edge-on cannot either. That is deliberate: there is no reliable direction to drag
it in, so orbit a little and it comes back.

**One drag is one change.** However long you drag, a single **Undo** puts the
object back exactly as it was before you touched it. Pressing a handle and
releasing without moving changes nothing at all.

**Only the handles belong to the gizmo.** A drag that starts anywhere else in
the viewport still orbits, pans and zooms exactly as before, and a tap still
selects. While a handle is held, the camera does not move — and if a second
finger lands, the drag is abandoned and the object jumps straight back to where
it started rather than trying to be moved and orbited at once. A stylus grabs a
handle exactly as a finger does.

Handles are a Construction tool. There are none while sculpting, and neither
control is there either.

Switching what the handles do, or which axes they use, is not a change to the
model: it records nothing, and Undo is unaffected by it.

### Exact values

Opening the exact values from the control below the Tool Rail gives the same
placement as numbers: Position X/Y/Z with the unit they are written in, Rotation
X/Y/Z, Scale X/Y/Z, and a single **Apply Transform** button.

**Apply is always on screen.** It sits below the values rather than after them,
so it does not scroll away and there is never a swipe between deciding and
committing. The values themselves scroll if the window is too short for all nine,
and while there is more below them the panel fades at its bottom edge to say so.

**The unit chips belong to Position and sit with it**, directly under the three
position fields and headed *Position unit*. Rotation is in degrees and Scale is a
bare multiplier, so neither offers a unit and nothing about a unit is drawn near
them.

**A long number keeps its sign and its leading digits.** A field you are not
editing shows as much of its value as fits and shortens it at the END, with an
ellipsis — so −98765.4321098 reads as a large negative number at a glance rather
than as a plausible positive one. Touching the field brings the complete value
back and selects it, so the first key you press replaces it rather than being
appended to it, and nothing is ever stored or applied except the complete value
you typed.

The object starts at position 0, 0, 0 m, rotation 0, 0, 0° and scale 1, 1, 1.
Placement belongs to the object, not to the shape, so it survives any change of
shape untouched — Box, Cylinder, Sphere, Cone and Capsule alike.

### Scaling is not redimensioning

Shape and placement are two independent things, and each has its own Apply.

- **Apply Shape** changes what the object *is*. It is rebuilt at the new shape
  and dimensions and keeps exactly where it was — turning a moved, rotated box
  into a cylinder leaves the cylinder moved and rotated the same way.
- **Apply Transform** changes where the object *sits* and how large it is drawn.
  Its shape and dimensions are untouched, and so is the geometry itself: the
  object is moved, turned and stretched, never rebuilt.

That is the difference between **Scale** and a **dimension**, and it is worth
being exact about. A 2 m box at scale 2 is drawn 4 m across, and its Width is
still 2 m: Scale is a multiplier on the drawing, not a new size for the shape.
If you want the box to *be* 4 m wide, retype its Width; if you want this copy of
it drawn twice as large, set its Scale.

**Dimensions** is the third way, and it asks the question most people mean:
*how big is this object on screen, overall?* That is the Width times the Scale —
4 m for the box above — and typing 6 there sets the Scale that makes it 6 m
across while leaving the Width at 2 m. Nothing is rebuilt. See *Exact
dimensions, and relative scale*, below.

### Position, rotation and scale

Position is measured in the display unit, from the world origin, and is applied
to the centre of the object. Zero and negative values are ordinary — a coordinate
is a place, not a size — so −1.25 m is as valid as 1.25 m.

Scale has **no unit at all**, in any display unit: it is a plain multiplier, so
1 is true size, 2 is twice as large and 0.5 is half. Switching between mm, cm and
m leaves the scale fields alone, exactly as it leaves the rotation fields alone.
Unlike a position, a scale **must be greater than zero**: 0 would collapse the
object to nothing, and a negative value would turn it inside out, which is a
mirror and not something ForgeShape does. Either is refused with the field named,
and nothing else in the transform is applied when it is.

Each axis scales along the object's **own** direction, so a turned object
stretches along its own length rather than along the world's.

Rotation is always in **degrees**, never in the display unit, and switching
between mm, cm and m leaves the rotation fields alone. Each axis turns the object
by the right-hand rule: looking down that axis toward the origin, a positive
angle turns counter-clockwise. The three are applied in the order X, then Y, then
Z, and the position is applied last — so rotating the object does not swing it
around the origin, it turns in place wherever it has been put. Negative angles
are ordinary, and an angle is kept exactly as typed: 370° stays 370° in the
fields rather than being rewritten as 10°, even though the two point the same
way.

Moving, rotating or scaling the object moves, rotates and scales what can be
tapped with it: a stretched body is tappable exactly where it is drawn, not
where it would have been at its true size. A
sphere is the honest exception: it is the same shape from every direction, so
rotating one changes nothing about its outline or about where it can be tapped.
The rotation is still real and is kept exactly as typed — it simply has nothing
to show.

### Apply Transform

Like the shape, all nine values are submitted together and the result is
**applied**, **unchanged** or **rejected**. Rejected leaves all nine exactly as
they were — a scale of zero cannot leave a new position half applied — and a
value that is not a number at all, or a scale that is not greater than zero, is
reported with the field named. Editing the text does nothing until Apply
Transform is pressed.

### Exact dimensions, and relative scale

Under **Transform**, two more entries stand below the Move / Rotate / Scale
selector in the right-hand surface: **Dimensions** and **Relative scale**. Both
are offered only for a Construction Body that is visible and unlocked, and are
absent otherwise.

**Dimensions** is a mode. Entering it puts the transform handles away — the
handles and the leaders are two ways to change the same placement, and this mode
is about typing an exact number — and draws a technical-drawing annotation
around the body on each of its own three axes, with the measurement beside it in
the display unit. Tapping a measurement opens a small field over the viewport
with that value already in it, selected whole; typing a new one and pressing
Apply resizes the body along that axis to exactly that size.

The size shown is the body's own size along its own axes: the object's local
width, height and depth multiplied by the scale it carries. **Turning the body
does not change the numbers**, and neither does moving it or looking at it from
somewhere else.

Each number stands **on the dimension line it belongs to**, and it stays there:
it is present on the frame the mode opens rather than after the next thing you
touch, it follows an orbit, a pan and a zoom, it follows the body through a move,
a turn and a resize, it moves to the body you select instead of staying on the
one you left, and it goes away when you Start Sculpting or hide the body. A
number whose anchor falls outside the viewport is held at the nearest edge so it
stays readable, rather than being drawn off screen. Verified at runtime by
`UI-3D-STATE-C1` (2026-09-08), which corrected the six placement and staleness
defects `UI-3D-STATE-AUDIT-R1` had measured; the same conversion carries the
sketch line's dimension label and the extrude cluster, so those stand on their
geometry too.

**The axis you are editing is drawn in a warmer colour than the other two**, so
which measurement the field belongs to is stated by the drawing as well as by
the field. That leader used to be invisible while it was the active one — the
renderer had no case for its drawing style — which was `UI3D-F-005`; it is
closed by `UI-3D-STATE-C2` (2026-09-08) and all three leaders now draw in every
state.

A **plane** truthfully reports zero thickness, and asking to
resize that axis is refused by name rather than answered by inventing a
thickness for it.

An **anchor** decides which side of the body stays where it is while the other
side moves. Three choices stand under Dimensions, each with its own pictogram
and each naming what it holds:

| Anchor | What stays still |
| --- | --- |
| Hold the negative side | the body's own negative face on that axis; it grows and shrinks the other way |
| Hold the centre | the object's position; it grows and shrinks both ways equally |
| Hold the positive side | the body's own positive face on that axis |

The two side anchors hold a real point in the world still, correctly for a body
that has been turned in all three axes. Dimensions opens on **Hold the centre**
every time.

**Relative scale** is a different question and has its own panel: three
multipliers and one Apply. It always opens at **1, 1, 1** — it asks how much to
change the size *by*, not what the size *is* — and applying multiplies the scale
the body already carries. If X was 6.64 and you type 2, X becomes 13.28; open
the panel again and it reads 1 once more. The multiplier itself is never saved.
It is centred: applying it moves nothing.

Each exact dimension and each relative-scale Apply is **one Undo**. Undo puts
back both the position and the scale exactly, and Redo reapplies both. A value
that is not a number, is zero, is negative, or is applied to a locked or hidden
body is reported with the reason and changes nothing at all.

These are the size of the **body**. They are not the size of a CAD sketch: a
sketch line's length, a circle's radius and an extrusion depth stay in the CAD
shape editor, and neither control touches them.

## Undo and Redo

Two controls sit in a small capsule at the trailing end of the bottom edge,
opposite the Objects capsule, in Construction and in Sculpt alike; only while a
sketch is open are they withdrawn, because a sketch in progress is not in the
history yet. **Undo** takes back the last change to the model; **Redo** puts it
back. Each is live
exactly when there is something to take back or put back, and greyed otherwise —
there is no message and no animation, because the model changing is the answer.

**What one press takes back is one thing you did**, not one number that moved:

- **Apply Shape** — however many fields you retyped first, and whether or not you
  also changed which primitive it is. Undo brings back the previous shape, every
  one of its exact dimensions and the same object.
- **Apply Transform** — all nine values together. Undo cannot put Position X back
  without Y, Z, the rotation and the scale.
- **Adding a body** — choosing Sphere from *Add Primitive* is one act. Undo makes
  the whole body disappear; it never leaves a leftover box behind. Redo brings
  back *the same body*, with the same name, the same shape, the same placement
  and the same position in the Objects list, selected again.
- **Deleting a body** — one press of a row's Delete. Undo brings back *the same
  object*: the same shape or the same imported geometry, any sculpting that was
  on it, the same placement, and the same position in the Objects list, with the
  selection where it was before. Redo removes it again.
- **Renaming a body** — one commit of the inline field. Undo puts the previous
  name back, Redo reapplies the new one, and the object never moves or changes
  shape. Renaming to the name it already has records nothing.
- **Showing or hiding a body** — one press. Undo shows a hidden object again or
  hides a shown one, and nothing about its geometry or its place in the list
  moves.
- **Locking or unlocking a body** — one press, on the same terms.
- **Duplicating a body** — one press. Undo removes *only the copy*, leaving the
  original untouched; Redo brings back the same copy, with the same identity it
  had. The copy's number is never handed to anything else afterwards.
- **One handle drag** — however long you held it and however many times the
  object moved while you dragged, Undo puts all nine values back to what they
  were before you touched the handle. A press that never moved records nothing.

Anything that changed nothing writes no history. A refused Apply, an Apply of
exactly what is already there, choosing a different body, switching between
*Shape* and *Transform*, opening or closing a panel, changing the display unit,
the appearance, the grid, the shading or the projection, hiding the chrome and
rotating the device are all invisible to Undo — which also means none of them
throws away a Redo you still had.

Undo after Undo walks back through the session in the order the changes were
made, across bodies as well as within one, and each step affects only the body it
was about. Making a **new** change after undoing ends the branch: the Redo you
had is gone, as it is in every editor.

The history is the session's. It survives rotating the device and sending
ForgeShape to the home screen and back, and it is **not saved**: closing the app
loses it along with everything else, and there is a limit — after about sixty
changes the oldest ones stop being reachable.

**Sculpting has its own Undo, and it is a separate history.** The same two
controls stay on screen while you sculpt, and there they take back **strokes**:
one press undoes the last stroke you made, however many times your finger moved
during it, and Redo puts it back. They walk only the body you are sculpting —
another sculpted body keeps its own strokes, untouched, and coming back to it
finds them where you left them.

**And while you sculpt there is a third control beside them: Sculpt History.**
It opens a compact list of the states your strokes have taken the body through —
*Start*, then one row per stroke — with the state you are looking at marked, and
tapping any row stands the body on it. Tapping an older row does exactly what
pressing Undo that many times does, and tapping a newer one exactly what Redo
does; tapping the row you are already on does nothing at all. The list shows
five rows at a time and scrolls over the rest, so a long session is all still
reachable — five is how many fit at once, not how many are kept.

Jumping back does **not** throw away the strokes ahead of you: they stay in the
list and you can walk forward to them again. What ends that branch is the same
thing that always ended it — making a **new** stroke from where you are standing,
after which the ones you had walked away from are gone.

The control is Sculpt's alone: it is not on screen in Construction, because the
Construction history has no such branch to show. It closes with System Back, and
it moves nothing but the mesh — jumping saves nothing, changes no name, position
or shape parameter, and puts nothing into the Construction Undo history.

The stroke history is separate from the Construction history in both directions.
Undoing strokes never takes back a Construction change, and Construction Undo
never touches a single sculpted vertex. Leaving to *Back to Construction* or
*Back to Imported Mesh* and returning with *Resume Sculpt* keeps your strokes and
your ability to take them back. Starting a sculpt again from the shape — the
question that warns you first — is the one thing Undo cannot walk back across:
once you have said the previous sculpt is gone, it is.

It is **not saved**. Reopening a project gives you back exactly the mesh you
left, with an empty stroke history over it: what a `.forge` file keeps is the
model, not the path you took to it. There is a limit here too — a long session
stops being able to reach its oldest strokes, and a single stroke big enough to
be worth more than the whole budget applies normally but says, once, that it
cannot be taken back.

If a Construction change was the shape your
sculpt mesh was built from, the usual out-of-date warning appears, exactly as it
does when you retype the shape by hand — the sculpt work itself is untouched, and
*Resume Sculpt* brings it back as it was.

## Sculpting

The object can be **sculpted** — pulled around by hand — as well as constructed
from exact numbers. Those are two different ways of describing the same object,
and ForgeShape keeps both.

### Start Sculpting, and the Sculpt workspace

**Start Sculpting**, in the Global Toolbar, takes the **selected** body's
geometry as it currently stands and makes a sculptable mesh from it, then
switches to Sculpt Mode. Nothing about the body's own description changes: a
constructed body's shape, dimensions and placement are exactly what they were,
and they are still there when Sculpt Mode is left. Starting changes nothing
visible — the picture on screen before and after pressing the button is
identical, pixel for pixel.

It works on **any** body, including one imported from a `.glb`. See *Sculpting
an imported object* below for what is different there, which is only the wording.

**Sculpting is per body, and each body keeps its own.** Sculpt one body, go back
to Construction, select another body and sculpt that one too — then come back to
the first and Resume, and its own sculpting is exactly as it was left. The two do
not share a mesh, a history or a stale-source warning, and sculpting one can
never move a vertex of the other. The other bodies stay visible while you sculpt,
so you can see what you are working against — unless you **Isolate** the body you
are sculpting (below).

Which body is being sculpted is fixed for as long as Sculpt Mode lasts: to work
on a different one, go **Back to Construction** — **Back to Imported Mesh** over
an imported object — select it there, and Start Sculpting or Resume. A tap in the
viewport does not change that either: tapping another body while sculpting
neither selects it nor moves the sculpting to it, and a tap on empty space
leaves the selection exactly as it was. What is *not* per body is the brush — the tool you are
holding and its Radius and Strength stay exactly as you set them when you move
between bodies, for the same reason they stay put when you switch tools.

That button says **Start Sculpting** only while there is no sculpt mesh yet.
Once there is one it says **Resume Sculpt** instead, because those are genuinely
different acts and the wording is the only thing that tells them apart before
they happen: starting builds the mesh from the constructed shape, resuming goes
back to the sculpting exactly as it was left.

**Creating a body is not offered while sculpting.** ForgeShape refuses to add a
body in Sculpt Mode — the mesh being worked on has to stay the one that is being
worked on — so the `+` is simply not there. The scene itself stays one tap away:
the Objects control still names the body being sculpted and still opens the list.

In Sculpt Mode the Tool Rail carries the seven tools — **Grab**, **Clay**,
**Smooth**, **Flatten**, **Inflate**, **Crease** and **Mask** — and a **Radius**
and a **Strength** sit directly at
the opposite edge, always on screen, adjustable without opening anything. Each of
those two reads as one thing: its name, the value it currently has directly under
the name, and the slider under both. The value follows the finger exactly, with
no smoothing and no delay, and it is written in that one place — it is not
repeated anywhere else on screen. The shape and placement fields are not merely
greyed out; they are not there, because in Sculpt Mode there is nothing on screen
that edits the constructed shape.

Radius/Strength and the Sculpt Tool Rail share stable top anchors. Opening
Sculpt details and then opening Objects replaces the first panel with the
second; neither panel stacks over the other, and the direct Radius/Strength
targets remain at least 48 dp wide.

One tool is filled, and it is always the one that is actually active: tapping a
tool asks for it and the rail then shows what it got. It keeps saying so —
rotating the device, resizing the window or moving between compact and expanded
layouts never leaves the rail without a held tool. Choosing a tool changes
nothing about the model: nothing is redrawn or rebuilt, no sculpting is lost, and
the Radius and Strength stay where they were, because they belong to the brush
rather than to the tool.

The Property Inspector in Sculpt Mode carries what the sculpt mesh currently is,
the stale-source warning when there is one, and **Reset Sculpt from Shape…** —
the one act in ForgeShape that cannot be undone. Pressing it when sculpting has
actually been done **to the mesh that exists right now** asks first, says the
sculpting will be discarded, and labels the button with what it does rather than
*OK*. Cancelling changes nothing at all. Pressing it on a mesh nothing has been
done to just rebuilds, because there is nothing to lose and a warning that cries
wolf is worse than no warning.

The question asked is only ever about the current sculpt mesh. Sculpting that was
done, and then deliberately discarded by an earlier confirmed reset, is already
gone; it is not something a later reset can take away. So rebuilding a fresh mesh
is silent no matter how much sculpting happened earlier in the session. For the
same reason the message names no number: what is at stake is this mesh's
sculpting, and a count carried over from meshes that no longer exist would be a
false claim about what is being lost.

### Sculpting an imported object

An object that came from a `.glb` can be sculpted too, and it works exactly the
way sculpting a constructed body works: **Start Sculpting** takes the object's
geometry as it stands, makes a sculptable mesh from it, and switches to Sculpt
Mode. The same seven tools, the same Radius and Strength, the same one-finger
rule.

**The imported mesh itself is never changed.** It is what the file gave you, and
sculpting produces a second version of the object beside it rather than writing
over it. The way out of Sculpt Mode says so: it reads **Back to Imported Mesh**,
and pressing it shows the file's own geometry again, exactly as it arrived, no
matter how much sculpting you have done. **Resume Sculpt** takes you back to your
sculpting, exactly as you left it. You can go back and forth as often as you
like.

**Nothing moves when you start.** The object stays where it is, at the size and
angle you put it. Starting to sculpt changes what you can *do* to the object,
never where it is or how big it is.

**Reset Sculpt from Imported Mesh…** is the imported object's version of the one
act that cannot be undone: it throws the sculpting away and starts again from the
imported geometry. It asks first, in the same words and on the same terms —
whenever there is sculpting on the current mesh to lose.

There is no *stale source* warning for an imported object, because there is
nothing that can make one stale: a constructed body's shape can be re-dimensioned
after you started sculpting it, and an imported mesh cannot be edited at all.

**Your sculpting is saved with the project.** Save, reopen, autosave and recovery
all keep both halves — the imported mesh the file gave you and the sculpting you
did on top of it — so a reopened project comes back with Resume Sculpt waiting
and Back to Imported Mesh still showing the original.

**Sending it to another program** follows the same rule every sculpted body
already follows: exporting while you are sculpting writes the sculpted shape, and
exporting from the imported view writes the imported shape.

### Isolate

**Isolate** in the Sculpt Property Inspector shows the body you are sculpting and
nothing else, so a body standing in front of it no longer hides it — the brush
never touched that body anyway, only your view of the one underneath did. It is
one control with two states: **Isolate**, then **Exit Isolate**. It is only there
while sculpting.

It is a way of looking, not an edit. It hides nothing in the project: the other
bodies keep their Show/Hide state, nothing is saved or checkpointed because of
it, and Undo never steps through it. It stays on while you sculpt, orbit, pan,
zoom and turn the device. **Back to Construction turns it off**, and Resume Sculpt
always starts with every body in view.

A **hidden** body cannot be sculpted: while the selected body is hidden, Start
Sculpting and Resume Sculpt are not offered. Show it again from its Objects row
in Construction and they come back. Hide itself stays a Construction act — the
Objects rows keep their Show/Hide control out of Sculpt, as before.

### One finger on the model sculpts; anywhere else navigates

One finger placed **on the model** sculpts with the active tool. One finger
placed anywhere else on the viewport orbits, exactly as it always has, and two
fingers pan and zoom as they always have. A finger that starts on empty space
cannot sculpt, and a finger that starts on the model cannot orbit — which of the
two it is is decided the moment the finger lands and does not change while it
moves.

**Reaching for a two-finger pan or pinch never sculpts, even if the first finger
lands on the model.** Placing one finger down does not begin anything; sculpting
starts only once that finger actually starts moving on its own. If a second
finger arrives first, the whole gesture is navigation and the model is not
touched at all — not moved a fraction, and nothing to undo. A tap on the model
changes nothing either.

Every tool holds the surface where it was touched and falls off smoothly outwards
to nothing at the edge of the brush, so a stroke leaves a smooth result rather
than a crease. What a stroke has hold of is fixed when the finger lands, so the
deformation stays one coherent shape however far the finger travels.

### The seven tools

**Grab** drags the surface with the finger. The result depends only on where the
finger ends up, not on how fast it got there or how far it wandered on the way.

The other six work as you **drag over** the surface, and how much they do
depends on how far the finger has travelled — not on how long you held it there.
A finger that stops moving does nothing at all, and the same path always does the
same amount whether you draw it quickly or slowly. Keep dragging over the same
place and the effect keeps building.

- **Clay** deposits material outward, building a raised shape where you work.
  Repeated passes make it taller.
- **Smooth** evens out roughness by pulling each point toward the middle of its
  neighbours. It can only ever move part of the way there, so repeated smoothing
  settles down instead of overshooting. A sharp spike pulled out with Grab is
  flattened back into the surface by a couple of Smooth strokes.
- **Flatten** presses the area under the brush toward a single flat plane —
  the one the surface was already closest to when the finger landed. It can only
  ever move part of the way there, so repeated passes settle onto the plane
  instead of pushing through it and raising a ridge on the far side. It is the
  tool for a facet, a flat spot or a clean bearing surface.
- **Inflate** expands the surface outward, following the shape as the shape
  changes — so a bulge it makes widens and rounds rather than stretching further
  in the direction it started.
- **Crease** cuts a narrow groove. It pushes inward and gathers the surface in
  toward the line you are drawing at the same time, which is what makes the
  channel narrower than the brush that cut it — a wrinkle, a seam, a panel line.
- **Mask** paints nothing into the shape at all. See *Protecting an area* below.

Clay and Inflate are not the same brush with different names. Clay pushes along
the directions the surface faced when the stroke began, so it lays down one
coherent slab; Inflate re-reads the surface as it goes. Side by side on the same
sphere, with the same brush and the same stroke, Clay leaves an angular
protrusion and Inflate a broader, rounder swell.

Because these six work by travel rather than by time, a **short** drag with a
**large** brush does very little. If a stroke seems to do nothing, either drag
further or make the brush smaller.

### Protecting an area — the Mask

**Mask** is the one tool that changes nothing about the shape. It paints over
the surface, and everywhere it has painted the other six tools are held off. The
painted area darkens so you can see exactly what is protected.

It builds up as you drag, like the tools that deposit: one short pass protects a
little, working over the same area protects it completely. Where the mask is
full, a brush stroke does **nothing at all** — not a small amount, nothing — and
there is nothing to undo afterwards because nothing happened. Where it is
partial, a brush does proportionally less. That is what lets you work right up
against an edge you have already finished without spoiling it.

**Clear Mask** removes it. It appears in the Sculpt panel only while there is a
mask to clear, it does not ask first, and one **Undo** puts the whole mask back —
which is why it does not need to ask.

**The mask is not part of your project.** Save, reopen, autosave and recovery all
keep your sculpting exactly; none of them keeps the mask, and a reopened project
starts with a clean surface. It is a working aid for the session you are in, like
where the camera is pointing. It does survive going **Back to Construction** and
coming back with **Resume Sculpt**, and each body keeps its own — masking one
body never touches another.

Painting a mask and clearing it are both ordinary steps in the sculpt history:
each one is a single Undo, they sit in the same list as your strokes, and the
history list you can scroll through shows them alongside everything else.

### Radius and Strength

Radius and Strength are shared by all seven tools, so switching tool never changes
how big or how strong the brush is.

**Radius** is measured on screen, in pixels, so the brush covers the same amount
of what you can see regardless of how far you have zoomed in. It has a large and
obvious effect on every tool: on a 2 m sphere, halving the radius took
Clay from 51 affected points to 11 and Inflate from 29 to 8.

**Strength** is how much each tool does. For Grab it is how much of the finger's
travel the grabbed surface follows: at the maximum the surface keeps up with the
finger, at the minimum it barely moves. For the other six it scales how much is
deposited, relaxed, flattened, cut or painted, roughly linearly across the
slider. Neither
setting can be given a useless value: both stop at the ends of their range.

### Switching back and forth

**Back to Construction** shows the constructed object again, exactly as it was
before any sculpting — the original shape, at its original dimensions, in its
original place. The sculpting is not lost and nothing is copied back into the
constructed shape. **Resume Sculpt** returns to the sculpted version with every
deformation still there, down to the pixel; it does not start again from the
constructed shape. Only **Reset Sculpt from Shape** does that, and it is the only
thing that discards sculpting.

### Changing the shape after you have started sculpting

Changing the constructed shape while a sculpt mesh exists never touches the
sculpting. ForgeShape says so — it reports that the shape changed after you
started sculpting and that the sculpt has been kept as it is — and leaves the
decision alone: reset from the new shape, or carry on sculpting what is already
there. That warning is a standing one: it is on the workspace without opening
anything, and it comes back after any passing message that covers it, because it
stays true until you act on it. ForgeShape does not transfer sculpting onto a new
shape by itself, and does not throw sculpting away without being asked.

### What sculpting can and cannot do

Sculpting moves the surface and never adds or removes any of it: the sculpt mesh
always has exactly as many points and faces as the shape it was built from,
through every stroke of every tool. It changes only the sculpted copy — after
working over an object with all seven tools, going back to Construction shows the
constructed shape at exactly its original dimensions in exactly its original
place, the picture identical and the numbers in the inspector unchanged.

What can be tapped follows the sculpting, not the constructed shape: on a
sculpted sphere, a point out on a raised protrusion is part of the model and can
be touched there, while the same distance from the centre in an untouched
direction is empty space.

A stroke that is interrupted stops where it is rather than snapping back — but
what it did is still a stroke, so Undo takes it back like any other.

## Settings

**Settings** is one full-window page holding every preference ForgeShape keeps
between runs, and nothing else. It opens from the quiet **Settings** row at the
foot of Home and from **Settings…** at the bottom of the Project surface, and
**Back** returns you exactly where you were — Home, or the workspace with the
same body selected and the same tool in hand. While it stands the viewport is
not reachable, and nothing behind it can be touched.

Four groups. **Appearance** is the five palettes above. **Workspace** is
**Handedness**: *Right-handed (default)* keeps the tool rail on the right edge;
*Left-handed* moves the rail — and the exact-value panel, the sculpt sliders
and, on a wide window, the Objects column — to the left edge, the same 8 dp off
it, the same width, the same height down the window, with the exact-value panel
opening inward beside it. That is all it moves. The model, its axes, the
sketch's plane, the camera, every handle's meaning and every gesture are
exactly what they were; a left-handed user is looking at the same model from
the same place with the controls under the other thumb. **Gizmo** is how the
transform handles are drawn: **Visual size** in four steps — *Smaller (90%)*,
*Default (100%)*, *Larger (125%)*, *Largest (150%)* — and **Thickness** in
three — *Thin*, *Regular (default)*, *Bold*. Both are presentation only: the
handle you can grab is always at least 48 dp across whatever the size, and the
same drag moves, turns or stretches the body by the same amount at every size
and weight. There is no handle-style choice: the gizmo has one style.
**Interface** is **Tool labels**: *Off (default)* shows the extrusion controls
as icons alone; *On* adds a short one-word caption under each.

Every row shows which option is chosen with a filled row, a check mark and,
to a screen reader, the word "selected" — never colour alone. Choosing an
option applies it at once and saves it; there is nothing to confirm. Changing a
preference changes no project: nothing is added to Undo, nothing is saved into
a project file, and a project that was not dirty stays not dirty.

## Viewport navigation

Touches that land on any chrome surface — the Global Toolbar, the Tool Rail, the
brush controls, the Property Inspector — stay there: dragging across one never
orbits, pans, zooms, selects or sculpts. Everything else is the viewport, which
runs edge to edge under all of it. Touching the viewport also ends any text edit
— focus and the keyboard come back to the model.

Navigation never moves the object. Unless its transform is changed, it stays
exactly where it is and everything the user sees move is the camera, which orbits
a **target** point and is described by target, yaw, pitch and distance.

- **One finger — Orbit.** Dragging orbits the camera around the target; the
  target does not move and the distance does not change. Drag right and the model
  spins to the right (yaw decreases); drag down and the camera rises above the
  model (pitch increases). Sensitivity is `0.005` rad/px, so a full 1080 px sweep
  rotates about 310°. Yaw wraps freely; pitch is clamped to ±1.52 rad (±87.1°) so
  the view never flips over the pole.
- **Two fingers — Pan.** Moving two fingers together slides the target in the
  camera plane, so the scene follows the fingers. One pixel of finger travel
  moves the scene by one pixel measured at the target plane, so panning feels
  identical at every zoom level and on every screen size.
- **Pinch — Zoom.** Spreading zooms in, pinching zooms out, scaled by
  `exp(-Δspan × 0.0035)`, so the same finger travel always produces the same zoom
  ratio and the value can never reach zero or go negative.

All three work the same way in both projections, on the same target. Only what
pinch *changes* differs, and it has to: in Perspective it moves the camera closer
or further away, clamped to `[0.35, 400] m`. In Orthographic moving the camera
along its own axis would change nothing on screen — that is what a parallel view
means — so pinch instead changes how much of the world the viewport spans,
clamped to a visible height of `[0.02, 250] m`, which reaches from a 4 cm detail
to a 500 m object. Panning is scaled to match whichever is active, so one pixel
of finger travel is still one pixel of scene travel at the target plane in both.

Pan and pinch are interpreted from the same two-finger gesture and can be
performed simultaneously. Adding or removing a finger re-anchors the gesture and
produces no camera jump; lifting all fingers, or an interrupted gesture, clears
it without moving the camera and leaves no stale movement behind.

## Selection

The Construction Body can be selected. Selection is presentation only: it
changes nothing about the model, and there is nothing yet that acts on the
selected object.

A short single-finger tap on the object selects it. **Selection announces itself
and then gets out of the way.** The moment a body becomes selected it flushes to
a strong warm orange, and over about a fifth of a second that flush fades away
completely. What is left behind is a thin warm **outline** drawn around the
body's silhouette, and that is what tells you which body you are editing for as
long as it stays selected.

The object itself is never tinted while it rests. That is the point of the
outline: the surface you are judging is shown in its own light and shade,
unmodified, with only its edge marked — so a face that was brighter than its
neighbour still is, exactly as much as it was, in every appearance.

The outline is a true silhouette of the body as it is actually drawn. It follows
the shape, whatever the body is — a shape you set by numbers, a mesh you
imported, something you have sculpted, or a solid you extruded from a sketch —
and it follows the body's position, rotation and scale. It stays the same
thickness however far you zoom in or out, a few pixels wide, thin enough not to
swallow small details. And it obeys what is in front of it: where another object
covers part of the selected one, the outline traces only the part you can
actually see and stops at the edge of whatever is covering it. It never shows
you a ghost of the hidden part.

If you would rather see the bare model, **Selection Outline** can be turned off
in the Display control beside Grid, and the Objects capsule still names the
selected body. Like Grid, that choice lasts for the session and survives
rotating the phone and leaving the app.

Tapping a body that is already selected does not flash
again: the acknowledgement marks a change of selection, not a touch. A short
single-finger tap that misses clears the selection, and the object returns to
exactly its unselected appearance. In Sculpt Mode
one finger on the model sculpts instead of selecting — that is what one finger on
the model means there — so tap-to-select is Construction Mode behaviour, while a
tap on empty space still clears the selection in either mode.

Navigation never selects. Dragging with one finger orbits and never selects or
clears, no matter where the finger lifts: a gesture stops being a tap once the
finger has travelled more than about 24 pixels from where it went down, measured
from the original touch point, so a slow drag cannot sneak past it. Any gesture
that uses two or more fingers is navigation only, including when the second
finger lifts first and the gesture ends on one finger, and a gesture interrupted
by the system leaves the selection untouched.

The tap selects whatever visible surface is under the finger, so it follows the
camera: after orbiting or zooming, tapping the object where it now appears
selects the same object. Only faces that are actually drawn can be hit — tapping
never reaches through the object to a surface on the far side.

What can be tapped is the object's real surface at its real dimensions, and it
follows an edit immediately: after a 1.5 m sphere is changed to 0.6 m, the place
that used to be its surface is empty and taps now land 0.3 m from its centre.

## Saving and reopening a project

**Work now survives the app closing.** The Global Toolbar carries a project
control beside Export; pressing it opens a small surface with the two things
that work — **Save Project** and **Open Saved Project**.

Save writes the whole project: every body, in scene order, with its identity, the
shape it is now, **all six** primitive sizes it remembers, and exactly where it
sits — position, rotation and scale. A body you have sculpted is saved with its
sculpted mesh, vertex for vertex. Which body was active is saved, and so is the
representation you were working in: save while sculpting and the project reopens
sculpting that body, save in Construction and it reopens in Construction. The
status line reports what was written, for example *Project saved — 3 bodies in
Construction.*

There is exactly **one** saved project. Saving replaces it. Open reads it back,
rebuilds every body's geometry and shows it. Undo history starts fresh over the
reopened project, and the status line says so: history is a record of what you
did during a session, not part of the project.

**An Open that cannot succeed changes nothing.** If there is no saved project,
if the file is damaged, if it was written by a newer ForgeShape, or if it is not
a ForgeShape project at all, the status line says which of those it is and your
current work — every body, every sculpted vertex, the mode you are in and your
Undo history — is left exactly as it was. Nothing is partially loaded, and
nothing crashes.

Until you save one, *Open Saved Project* reads *No saved project yet* and does
nothing.

## When ForgeShape closes before you saved

**Your work is checkpointed as you go.** ForgeShape keeps a separate recovery
copy of the project, written automatically shortly after you stop editing and
again whenever the app leaves the foreground. It is not your saved project and
never replaces it: **Save Project still writes only when you press it**, and the
recovery copy exists purely so that work you never got round to saving is not
lost if the app closes, is killed, or stops unexpectedly.

If ForgeShape finds such work the next time it starts, it asks — once — before
anything is replaced:

- **Recover** opens the work as it was when ForgeShape closed. Undo history
  starts fresh, exactly as it does after opening a saved project.
- **Discard** forgets it and starts normally.

**Whichever you choose, a project you saved yourself is untouched.** The
recovery copy is the only thing Discard removes.

If that recovery copy turns out to be unreadable — damaged storage, or written
by a newer ForgeShape — it is set aside once, the status line says so, and you
are never asked about it again. It cannot loop, and it cannot replace anything.

## Moving a project on and off the device

The project surface carries two more actions, under **Transfer**:

- **Save Copy…** asks where to put a copy and writes the project there, using
  the system's own file UI. Your saved project is unchanged.
- **Open File…** asks which project file to read and opens it. The project
  becomes live; **your saved project stays what you saved** until you press Save
  again.

A file written this way is a ForgeShape project — the same format the app uses
internally — and another ForgeShape installation can open it. It is not a model
file for another program; **Export**, below, is the one that produces one.

Anything that goes wrong here costs you nothing: cancelling the file picker does
nothing at all, a file that cannot be read or written is reported, and a file
that is not a ForgeShape project — or is damaged, or was written by a newer
version — is refused with the reason, leaving your work exactly as it was.

## Sending the model to another program

**Export**, in the top bar, writes what you have made as a **`.glb` model file**
— the standard binary form of glTF, which Blender, Godot, Unreal, Windows 3D
Viewer, macOS Quick Look and most other 3D programs open directly. You choose
where it goes, using the system's own file UI, and the suggested name ends
`.glb`.

What lands in the file:

- **Every body in the scene**, each in the place, at the angle and at the size
  you put it. The **shape you see is the shape in the file**: the turning and
  the sizing you did are part of the mesh itself, not instructions attached to
  it, so the object opens the right way round and the right size with nothing
  left to apply. Where it stands is still recorded as a position, so it lands
  where you put it and its pivot is the one you have been rotating about.
- **The real surfaces.** Hard edges stay hard and round shapes stay smooth, the
  way they look in ForgeShape.
- **Metres.** One metre in ForgeShape is one metre in the file, and up is up.
  Nothing needs rotating or rescaling after it opens.
- **Sculpted shapes as you sculpted them.** A body you have been sculpting
  exports its sculpted surface, not the shape it started as. A body you have
  not sculpted exports its Construction shape.
- **One file.** No folder of extra pieces beside it, and nothing is sent
  anywhere: ForgeShape has no network permission at all.

**Export is one-way and it does not change anything.** ForgeShape cannot read a
`.glb` back **as a project** — a `.glb` is a picture of the geometry, not your
project, so keep saving `.forge` files for the work itself. It can *import* a
`.glb`, which adds the objects it describes to the project you already have
open: see **Importing a GLB file** below. Exporting does not alter your model,
your Undo history or your saved project; cancelling does nothing at all; and if
a file cannot be written, ForgeShape says so and your work is untouched.

Not there yet: materials, colours and textures of your choosing, UVs, object
grouping and export options are not implemented — every body is exported with
one plain default surface. Nothing else can be exported: there is no OBJ and no
FBX.

## Importing a GLB file

The project surface has a third group: **Import a mesh**. **Import GLB…** opens
a `.glb` you pick and adds what it describes to the project you already have
open, as objects you keep.

It reads two kinds of file. One is a `.glb` **ForgeShape itself exported**. The
other is an ordinary **static mesh from another program** — a low-poly model out
of a sculpting app, say.

**What you get is real objects.** Each mesh in the file becomes one object with
its own row in the Objects list, named after whatever the file called it. You
can select it, move, rotate and scale it with the handles or the exact values,
Undo and Redo those edits, save it, and open the project again later — the
geometry lives in the `.forge` file from then on, so you never need the original
`.glb` again. If a mesh was made of several parts with different materials, it
stays **one** object rather than becoming several rows.

**The whole import is one Undo.** However many objects arrive together, one Undo
removes all of them and one Redo brings all of them back exactly as they were.

**Importing is not offered while you sculpt.** While you sculpt, the Project
surface has no *Import a mesh* group, because an import would put a different
object under your brush. It is back as soon as you go Back to Construction. If
you had already picked a file before you started sculpting, it is not imported
when the picker returns: the status line says *Import is not available while
sculpting. Your project is unchanged.*, and nothing about your sculpt or your
project moves.

**An imported object is not a shape you can re-dimension.** It came in as a
mesh, not as a box or a sphere with numbers behind it, so *Shape* is not offered
while one is selected — there are no dimensions to type. Move, Rotate and Scale
work on it exactly as they work on anything else, and selecting one of your own
bodies again brings Shape straight back.

**But you can sculpt it.** *Start Sculpting* is offered for an imported object
just as it is for anything else, and it starts from the object's own geometry.
See *Sculpting an imported object*.

**Where it lands is where the file put it.** ForgeShape keeps the position the
file gave the object and bakes any rotation, scaling or skewing the file applied
into the geometry itself, so the object starts unrotated and unscaled with its
own origin exactly where the file's was. Nothing is re-centred and nothing is
resized.

**What does not come across is appearance.** Colours, textures and materials in
the file are read far enough to check they are not broken and then ignored: an
imported object is one plain neutral surface, and what you get is the *geometry*.
A file with no surface directions of its own gets sensible ones worked out from
its own triangles. A double-sided material is drawn from both sides, which is the
one appearance detail that changes what you can see.

If the file cannot be imported, ForgeShape says which of three things went
wrong — it is not a GLB it can open, it uses glTF features this import does not
read, or its own geometry does not add up — and your project is left exactly as
it was. Nothing is half-imported: either every object in the file arrives or none
of them does, and a refusal leaves no trace to undo.

Reading OBJ or FBX is still not implemented, and neither is bringing in
animation, rigging, materials or textures.

## Diagnostics

**Share Diagnostics…** writes a short local report — which build you are
running, the basic device facts, and a bounded list of recent events such as
saves, recoveries and viewport problems — to a file you choose.

**It contains none of your model.** No shapes, no dimensions, no sculpted
vertices, no project file contents and no file paths. And **ForgeShape sends
nothing anywhere**: it has no network permission at all, so the only way the
report leaves the device is you putting it somewhere.

## If the viewport stops

Graphics hardware can be reset or taken away by the system. ForgeShape rebuilds
the viewport when that happens and carries on — **your model is never held on
the graphics device**, so nothing about it is at risk either way.

If the viewport cannot be rebuilt, ForgeShape says so plainly and tells you a
restart is needed, after making sure your work is checkpointed first. It does
not show you a black viewport and pretend, and it does not keep drawing through
hardware it can no longer trust.

## What survives

The object's shape, its size, its position, rotation and scale, its identity, the chosen
display unit, which mode is active, which sculpt tool is in hand, every sculpted
deformation, the camera pose, the chosen projection and its framing, and the
selection all survive sending ForgeShape to
the home screen and resuming it — the viewport comes back pixel-identical. On
resume the surfaces for the active mode are shown and every field, slider and
tool is rewritten from what the object actually is, so anything half-typed or
refused before leaving is replaced by the truth.

The object's identity is stable across a shape change: turning the box into a
sphere, then a cylinder, then a box again leaves it the same selectable object
throughout, still in the same place. The Construction Undo history survives the
home screen and a rotation too, for the session.

All of that lives for the life of the app process. What survives the process
itself is what a **Save** put in the one saved project: the bodies, their
shapes, their remembered sizes, their placements, their sculpted meshes, which
body was active and which representation you were in. Everything else returns to
its defaults when ForgeShape restarts — the camera, the selection, the display
unit, the held tool, the brush, the Undo history, and the question of how the
model begins. The **Settings** preferences — the appearance, handedness and how
the gizmo is drawn — are the one exception: they are saved on the device the
moment you choose them and are back on the next start, and they are never part
of a project.

## Not yet implemented

Standard named views (Front, Top, Right and the rest), a view cube, a
focus-on-selection command and any camera animation are not implemented — the
projection can be switched, but the camera is aimed only by hand. An editable
tessellation is not implemented, and **shape is still edited only
by typing exact values and pressing Apply**: there are no handles that change a
body's *dimensions*. A sketch is lines, polylines, rectangles, circles, arcs
and splines on one of the three principal planes or on a flat face of a CAD
body; its regions may have holes; a CAD body is a first New Body extrusion plus
up to fifteen Add and Cut extrusions on its own flat faces, each reopenable, or
a single Revolve New Body about a straight sketch edge. There
is no Through All or Up To Face extent, no Intersect, no deleting or reordering
a feature, and no fillet, chamfer, shell, sweep, loft, pattern, feature
mirror or constraint solver (sketch Trim, Extend, Offset and Mirror exist —
see *Drafting inside a sketch*), no custom construction plane, no
sketch on the inside of a Cut, and no sketch on a
curved or imported surface; a polygon profile's points are not numerically
editable, a spline's points are edited by redrawing it, and a CAD Body does not
sculpt. The Scale handles change how large it is drawn, which is a
different thing — see *Scaling is not redimensioning*.

**The handles Move, Rotate and Scale, in World or Local axes, and that is all
they do.** There is no **Mirror**: a scale must be greater than zero, so an
object cannot be flipped by making one negative. There is no parent or explicit
coordinate space beyond World and Local, no centre handle for free movement, and
no arcball or free rotation — a rotation is always about one ring. The point the
handles turn and scale about is always the object's own position and cannot be
moved. Handles are Construction's alone — sculpting has none.

**A body carrying a non-uniform scale sculpts with the same round brush as any
other body.** A brush set to a given size on screen covers that much of the
screen whatever the body's scale is, and leaves a round mark on a body stretched
along one axis exactly as it does on an unstretched one — the brush is measured
by what is displayed, not by the object's own stretched coordinates. Turning the
body as well as stretching it changes nothing about this. Scaling a body never
alters its sculpt mesh, and starting, leaving or resuming sculpting never alters
its scale.

Sculpting has exactly the seven tools above — other
brushes (Pinch, Scrape, Snake Hook and the rest), remesh, symmetry, layers,
brush presets and stylus pressure are not implemented. Masking IS implemented,
as the Mask tool and Clear Mask above; inverting, growing, shrinking or blurring
a mask, masking by topology, and keeping a mask across a reopen are not.
Sculpt Undo IS
implemented, over whole strokes, and so is the *Sculpt History* list described
above; both are Sculpt's alone. The Construction history has no such panel,
neither history has named or thumbnailed steps, there is no branching tree, and
there is no keyboard shortcut for any of the three controls.

**There is a grid, but there is no snapping of any kind.** Nothing you drag,
type or place is quantised to it, no value is ever measured off it, and there is
no snap setting to turn on. That includes the handles: dragging one gives the
exact value the drag lands on, never a rounded one. The sketch grid — the one
you draw on, which does snap — is a different thing entirely and belongs to the
sketch alone; see *Sketching a CAD Body*. The world grid is a reference the
viewport draws and nothing more.

Exactly one body is selected at a time. Selecting more than one, lasso and box
selection, and object commands that act on a *selection* rather than on one
named row — along with grouping, nesting, reordering and drag and drop — are not
implemented. The Objects list adds, selects, deletes, renames, shows and hides,
locks and unlocks, and duplicates, one row at a time; it does nothing else.

Two-finger twist/roll, inertia, camera presets, focus-on-selection, long-press
selection and UV are not implemented.

**There are five appearances and no sixth, and none of them follows the
system.** ForgeShape does not follow the system's own light/dark setting, and
there is no automatic or scheduled switching: the five palettes are chosen by
hand in Settings and nothing else changes them. The appearance is saved between
runs; the camera is not, so starting ForgeShape fresh opens on Home in the
palette you last chose. There is no gizmo handle style to choose, no custom
workspace layout beyond right- or left-handed, no account, no cloud, and no
per-project preferences.

**Selection is an outline and nothing more.** There is no x-ray or
hidden-object reveal, no multi-select, and no setting for the outline's width or
colour; see *Selection*.

Shading stops where it is. There is **one** MatCap and no way to add, import or
choose another; there are no materials, no colour or texture you can assign to
the object, no texture painting, no metalness or roughness, no environment or
HDRI, no movable or additional lights, no shadows and no ambient occlusion. A
photoreal (PBR) preview belongs to later work on materials and export.

**No unimplemented control is drawn anywhere.** Export was the last one, and it
now writes a real `.glb`; everything drawn in the workspace does what it says.

**Every entry on the Tool Rail does something**, and so does every shape in Add
Primitive. Geometry comes from one of the six exact primitives and its
dimensions, from a sketch and its extrusion, or from an imported `.glb`, and
from nothing else.

**Add Primitive offers the six shapes ForgeShape builds, and New Sketch.** A
body you create from a shape always has dimensions behind it. *Import GLB…* is
the other way objects appear, and what it makes is a mesh rather than a shape:
real, editable in place, saved with the project and sculptable, but with no
dimensions to type.

**There is one saved project and no project library.** There is no Save As, no
naming, no recent list, no thumbnails, no folders and no multi-project library:
a project is saved when you press Save and not before, and there is exactly one
of them. It lives inside the app's own storage, so it is not visible in a file
manager and is removed if the app is uninstalled — Save Copy… is how a project
reaches somewhere you can see it. There is no cloud and no account of any kind.

**Autosave protects unsaved work; it does not manage projects.** It keeps one
recovery copy of the current work and nothing else — no versions, no history of
previous sessions, no snapshots you can browse, and no way back to a project a
later Save replaced.

**A ForgeShape project file is not an interchange file.** It is ForgeShape's own
format, meant to be readable by another ForgeShape installation, and it is not
GLB, glTF, OBJ or FBX. Open File… reads ForgeShape projects and refuses
everything else.

**Interchange is GLB and nothing else, one direction at a time.** ForgeShape
writes `.glb` and reads `.glb`, and neither is a project: a `.glb` you import
becomes objects in your project and is then never consulted again. There is no
OBJ and no FBX in either direction, and no animation, rigging, material or
texture crosses in either.

**A stylus is simply another pointer.** Nothing anywhere reads pressure, tilt,
hover or which kind of pointer is touching the screen, so an S Pen grabs a
handle and draws a stroke exactly as a fingertip does and changes nothing about
what the stroke deposits. The interface is *laid out* to stay comfortable with a
stylus, which is a design constraint rather than a feature.

**ForgeShape is an Android application and runs nowhere else.** There is no
Apple, Windows or web client, no account, no login and no sync. The code is
arranged so the domain could be carried to another platform later — described in
`ARCHITECTURE.md` — which is a constraint on how this codebase is written, not a
claim that any other platform is supported.
