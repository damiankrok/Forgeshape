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
is **selected**, and only the selected body is tinted; the rest are drawn
normally. Changing a body from one of those shapes to another changes what *that
body* is — it does not create a second one, and it does not touch any other
body.

ForgeShape comes up with a single body: a box **2.0 m wide × 1.0 m high × 0.5 m
deep**, centred at the world origin and unrotated, so it is visibly not a cube.
It is already selected, so there is never a step where nothing is being edited.

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

*View* picks what the viewport draws besides the model. Today that is one thing:

- **Grid — On** (the default) or **Off**. The reference floor described above.
  Turning it off leaves the bare model; turning it back on costs nothing and
  changes nothing about what you are building. The grid is correct in both
  projections.

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

A selected object is tinted, and stays readable as a shape while tinted, in both
shading modes and in every appearance — the resting tint is deliberately light
enough that the object's own light and shade still carry its form. See
*Selection*.

While sculpting, the lighting follows the surface as it moves: a stroke that
pulls a lobe out of a sphere lights that lobe immediately, with no stale shading
left behind.

## Starting a model

The first time ForgeShape runs, it asks one question — **New Project** — and
offers exactly two ways to begin. The question stands over the live viewport
rather than replacing it, and it is asked once: turning the phone, leaving to
the home screen and coming back, or anything else that rebuilds the screen does
not ask again. Starting the app fresh does, because nothing is saved anywhere.

**Construction / CAD** begins where the product always did: one default body,
its shape chosen from the six exact primitives, its dimensions typed.

**Sculpt** begins on a sphere already prepared for sculpting, so the first thing
you can do is make a stroke. Nothing is skipped to get there — the sphere is an
ordinary exact Construction sphere and it was prepared the ordinary way, so
**Back to Construction** shows that sphere with its diameter, and **Resume
Sculpt** returns to the same sculpt mesh with the strokes already on it.
Choosing Sculpt does not create a second body and does not consume the
Construction Source.

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
carrying the viewport's appearance settings and the control that hides the chrome.

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
shape palette, the list of bodies, the exact values or the appearance settings
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

**ForgeShape comes in three appearances, and you choose which.** *Display*
carries an **Appearance** group listing **Warm Graphite**, **Neutral Charcoal**
and **Light Charcoal**. All three are dark workspaces, and the viewport's own
ground changes with each: Warm Graphite — what a fresh start gives you — is a
warm dark studio ground, Neutral Charcoal is a cooler steel grey, and Light
Charcoal is the lightest of the three. They are three grounds to work a model
against rather than a light option and a dark one; a light canvas makes a
neutral clay render read as grey and washed out, which is the one thing a
modelling viewport must not do.

Switching changes how everything is drawn and nothing about what you are
drawing. The model, its exact dimensions, its placement, which body is selected,
whether you are in Construction or Sculpt, and any sculpting already done all
come through untouched. So does what you were doing: the display unit you had
chosen, the panel you had open and the editor you were in are all still there.
The whole workspace is rebuilt in the new appearance, so it blinks once.

The choice lasts as long as the app is running — turning the phone or leaving and
coming back keeps it — and is not saved. Starting ForgeShape fresh gives you Warm
Graphite again.

Tools, modes and panels are named by drawn icons rather than by characters, and
a control that is reserved for a feature the product does not have yet is drawn
dimmed, says so when read aloud, and does nothing.

A tap on a Tool Rail entry selects that tool even if the finger drifts a little,
and dragging the rail scrolls it without selecting whatever it started on.

**Panels move, briefly, and never make you wait.** Every panel that opens from a
control grows out of that control — the list of bodies and the shape palette out
of the Objects capsule, the exact values out of the control beside the rail, the
appearance settings out of the button in the toolbar — and all four move the same
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
version, and a newly selected body goes straight to its resting tint with no
flash — so selection is still just as easy to see, it simply takes no time.

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

What the scene deliberately does **not** offer: there is no way to delete,
duplicate, rename, hide or lock a body, no groups or nesting and no reordering
the list. Adding a body *is* undoable — see *Undo and Redo* — but the list
itself carries no commands. Bodies are not saved when the app closes.

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

## Undo and Redo

Two controls sit in a small capsule at the trailing end of the bottom edge,
opposite the Objects capsule, and they are drawn only in Construction. **Undo**
takes back the last change to the model; **Redo** puts it back. Each is live
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

**Sculpting has no undo.** The two controls are not drawn at all while you are
sculpting, rather than sitting there greyed, because a stroke cannot be taken
back and a control that looked like it could would be a lie. Coming back to
Construction and pressing Undo takes back a *Construction* change; it never
touches a single sculpted vertex. If that Construction change was the shape your
sculpt mesh was built from, the usual out-of-date warning appears, exactly as it
does when you retype the shape by hand — the sculpt work itself is untouched, and
*Resume Sculpt* brings it back as it was.

## Sculpting

The object can be **sculpted** — pulled around by hand — as well as constructed
from exact numbers. Those are two different ways of describing the same object,
and ForgeShape keeps both.

### Start Sculpting, and the Sculpt workspace

**Start Sculpting**, in the Global Toolbar, takes the **selected** body's shape
as it currently stands and makes a sculptable mesh from it, then switches to
Sculpt Mode. Nothing about that Construction Body changes: its shape, its
dimensions and its placement are exactly what they were, and they are still
there when Sculpt Mode is left. Starting changes nothing visible — the picture
on screen before and after pressing the button is identical, pixel for pixel.

**Sculpting is per body, and each body keeps its own.** Sculpt one body, go back
to Construction, select another body and sculpt that one too — then come back to
the first and Resume, and its own sculpting is exactly as it was left. The two do
not share a mesh, a history or a stale-source warning, and sculpting one can
never move a vertex of the other. The other bodies stay visible while you sculpt,
so you can see what you are working against.

Which body is being sculpted is fixed for as long as Sculpt Mode lasts: to work
on a different one, go **Back to Construction**, select it there, and Start
Sculpting or Resume. What is *not* per body is the brush — the tool you are
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

In Sculpt Mode the Tool Rail carries the four tools — **Grab**, **Clay**,
**Smooth** and **Inflate** — and a **Radius** and a **Strength** sit directly at
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

### The four tools

**Grab** drags the surface with the finger. The result depends only on where the
finger ends up, not on how fast it got there or how far it wandered on the way.

The other three work as you **drag over** the surface, and how much they do
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
- **Inflate** expands the surface outward, following the shape as the shape
  changes — so a bulge it makes widens and rounds rather than stretching further
  in the direction it started.

Clay and Inflate are not the same brush with different names. Clay pushes along
the directions the surface faced when the stroke began, so it lays down one
coherent slab; Inflate re-reads the surface as it goes. Side by side on the same
sphere, with the same brush and the same stroke, Clay leaves an angular
protrusion and Inflate a broader, rounder swell.

Because these three work by travel rather than by time, a **short** drag with a
**large** brush does very little. If a stroke seems to do nothing, either drag
further or make the brush smaller.

### Radius and Strength

Radius and Strength are shared by all four tools, so switching tool never changes
how big or how strong the brush is.

**Radius** is measured on screen, in pixels, so the brush covers the same amount
of what you can see regardless of how far you have zoomed in. It has a large and
obvious effect on every tool: on a 2 m sphere, halving the radius took
Clay from 51 affected points to 11 and Inflate from 29 to 8.

**Strength** is how much each tool does. For Grab it is how much of the finger's
travel the grabbed surface follows: at the maximum the surface keeps up with the
finger, at the minimum it barely moves. For the other three it scales how much is
deposited, relaxed or expanded, roughly linearly across the slider. Neither
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
working over an object with all four tools, going back to Construction shows the
constructed shape at exactly its original dimensions in exactly its original
place, the picture identical and the numbers in the inspector unchanged.

What can be tapped follows the sculpting, not the constructed shape: on a
sculpted sphere, a point out on a raised protrusion is part of the model and can
be touched there, while the same distance from the centre in an untouched
direction is empty space.

A stroke that is interrupted stops where it is; it is not undone, because
sculpting has no undo — the Construction Undo described above never touches a
sculpted vertex.

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

The Construction Body can be selected. Selection is a highlight only: it changes
nothing about the model, and there is nothing yet that acts on the selected
object.

A short single-finger tap on the object selects it. **Selection announces itself
and then gets out of the way**: the moment a body becomes selected it flushes to
a strong warm orange, and over about a fifth of a second that settles to a much
lighter warm tint it then keeps for as long as it stays selected. The
acknowledgement is what tells you the tap landed; the resting tint is what tells
you which body you are editing, and it is light enough that the object's own
light and shade still read — a face that was brighter than its neighbour still
is, in every appearance. Tapping a body that is already selected does not flash
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
  you put it.
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
`.glb` back — a `.glb` is a picture of the geometry, not your project, so keep
saving `.forge` files for the work itself. Exporting does not alter your model,
your Undo history or your saved project; cancelling does nothing at all; and if
a file cannot be written, ForgeShape says so and your work is untouched.

Not there yet: materials, colours and textures of your choosing, UVs, object
grouping and export options are not implemented — every body is exported with
one plain default surface. Nothing else can be exported: there is no OBJ, no
FBX, and no import of any format.

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
unit, the appearance, the held tool, the brush, the Undo history, and the
question of how the model begins.

## Not yet implemented

Standard named views (Front, Top, Right and the rest), a view cube, a
focus-on-selection command and any camera animation are not implemented — the
projection can be switched, but the camera is aimed only by hand. An editable
tessellation and booleans are not implemented, and **shape is still edited only
by typing exact values and pressing Apply**: there are no handles that change a
body's *dimensions*. The Scale handles change how large it is drawn, which is a
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

Sculpting has exactly the four tools above — other
brushes (Flatten, Crease, Pinch and the rest), remesh, **sculpt undo**, symmetry,
masking, layers, brush presets and stylus pressure are not implemented — the Undo
described above is Construction's alone. There is no undo *history panel*, no
named steps and no keyboard shortcut for either control.

**There is a grid, but there is no snapping of any kind.** Nothing you drag,
type or place is quantised to it, no value is ever measured off it, and there is
no snap setting to turn on. That includes the handles: dragging one gives the
exact value the drag lands on, never a rounded one. A sketch grid — the one you would draw on, with
snapping — is a different thing entirely and does not exist. The grid is a
reference the viewport draws and nothing more.

Exactly one body is selected at a time. Selecting more than one, lasso and box
selection, and object commands that act on a selection — delete, duplicate,
rename, hide, lock, group, nesting and reordering — are not implemented. The
Objects list adds and selects; it does nothing else.

Two-finger twist/roll, inertia, camera presets, focus-on-selection, long-press
selection and UV are not implemented.

**There are three appearances and no fourth, and none of them is light.**
ForgeShape does not follow the system's own light/dark setting, and there is no
automatic or scheduled switching: the three palettes are chosen by hand and
nothing else changes them. The appearance is not saved between runs, and neither
is the start choice or the camera, so starting ForgeShape fresh always gives you
Warm Graphite and always asks again how the model begins — even when you then
open a saved project.

**Selection is still a tint over the whole body, not an outline.** It is much
lighter at rest than it used to be and it announces itself when it changes, but
nothing draws a line around the selected object, and nothing else marks it.

Shading stops where it is. There is **one** MatCap and no way to add, import or
choose another; there are no materials, no colour or texture you can assign to
the object, no texture painting, no metalness or roughness, no environment or
HDRI, no movable or additional lights, no shadows, no ambient occlusion, and no
outline around the selected object — selection is a tint. A photoreal (PBR)
preview belongs to later work on materials and export.

**No unimplemented control is drawn anywhere.** Export was the last one, and it
now writes a real `.glb`; everything drawn in the workspace does what it says.

**Every entry on the Tool Rail does something**, and so does every shape in Add
Primitive. There is no sketching and no extruding: a body's shape comes from one
of the six exact primitives and its dimensions, and nothing else makes geometry.

There is no **Add from file** and no import of any kind. Add Primitive offers the
six shapes ForgeShape builds and nothing else.

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

**Export goes one way, in one format.** ForgeShape writes `.glb` and reads none
of it back. There is no OBJ, no FBX, no import of any model format, and no way
to bring a mesh in from another program.

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
