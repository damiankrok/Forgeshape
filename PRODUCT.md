# ForgeShape — Product Behaviour

Only behaviour that has been verified at runtime, through the real Android touch
path, is documented here. The last section — *Accepted product direction* — is
explicitly not behaviour: it is direction the owner has approved and nothing in
it exists.

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

**Display** in the Global Toolbar opens a small panel with three choices.

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
shading modes.

While sculpting, the lighting follows the surface as it moves: a stroke that
pulls a lobe out of a sphere lights that lobe immediately, with no stale shading
left behind.

## The Editor Workspace

The editor is one workspace, and the model is the largest thing in it. A thin
**Global Toolbar** across the top says what is being edited, carries the one
control that crosses between constructing and sculpting, and shows every status
and error message. A **Tool Rail** stands at the trailing edge. A **Property
Inspector** at the bottom edge — at the side when the window is short or wide —
carries the exact values, and it collapses to a single strip when the model is
what matters. A control in the toolbar hides all of it at once, leaving the bare
model and one chip to bring it back.

The same three regions are there in both modes. What changes is what they carry,
never where they are.

Touching any of that chrome never moves the camera and never sculpts: a control
owns its own gesture completely. Touching the viewport anywhere else navigates
exactly as it always has.

The workspace re-arranges itself for the window it is in — a phone in portrait, a
phone in landscape, a split-screen half, a tablet — and the arrangement is
decided by the window's size, not by the device or the orientation. The model
stays visible in all of them. On a tablet the inspector docks beside the model
instead of over it, and the controls do not grow: only the viewport does.

Turning the phone changes how much of the model you can see, never what shape it
is. A sphere is a circle on screen in portrait and a circle in landscape, and a
box keeps its proportions in both; rotating the device only makes the picture
larger or smaller, and never stretches it one way. What you can tap follows what
you see, so a tap that hits the model in portrait hits it in landscape too.

## Several bodies, and which one you are editing

Choosing **Shape** puts an **Objects** list at the top of the Property
Inspector: one row per body, labelled `Body #1`, `Body #2` and so on, with the
selected one visibly active. Below it is **Add Body**.

**Add Body** adds one more body with the same defaults the first one had — the
2.0 × 1.0 × 0.5 m box, unrotated, at the world origin — and selects it. The new
body therefore starts out sitting exactly on top of whatever is already at the
origin; move it with the ordinary Position fields. Nothing is offset for you,
because guessing where you wanted it would be worse than putting it somewhere
you can see and type over.

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

What Stage 017 deliberately does **not** offer: there is no way to delete,
duplicate, rename, hide or lock a body, no groups or nesting, no reordering the
list, and no Undo. Bodies are not saved when the app closes.

## Choosing and sizing the shape

The Tool Rail in Construction carries **Shape** and **Place**, plus **Sketch**
and **Extrude**, which are drawn but clearly not available. Choosing Shape puts
the Objects list described above, a
**primitive chooser** — Box, Cylinder, Sphere, Cone, Capsule and Plane — the
chosen shape's dimensions, a display unit (**mm**, **cm** or **m**) shared by
every length, and an **Apply Shape** button into the Property Inspector. Only the
chosen shape's fields are on screen, so nothing in the inspector can be mistaken
for another shape's dimension. Choosing Place puts the object's position and
rotation there instead, described further down. The rest of the viewport stays
visible and usable while editing.

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

**Freezing a plane keeps it two-sided.** Once frozen, the sheet still draws,
selects and *sculpts* from either side: a brush works on the underside exactly
as it does on top. Being usable from both sides belongs to the sheet itself, so
it survives Freeze and it stays true no matter what the Construction shape is
later changed to. The converse holds too: a frozen box or sphere stays
one-sided — it keeps its inside — even if the Construction shape is afterwards
changed to a plane. What decides is the mesh being worked on, never whichever
shape the Shape selector happens to be showing.

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

## Moving and rotating the object

Choosing **Place** in the Tool Rail puts the object's **placement** in the
Property Inspector: a Position X/Y/Z in the shared display unit, a Rotation
X/Y/Z in degrees, and a separate **Apply Transform** button.

The object starts at position 0, 0, 0 m with rotation 0, 0, 0°. Placement belongs
to the object, not to the shape, so it survives any change of shape untouched —
Box, Cylinder, Sphere, Cone and Capsule alike.

### Moving is not resizing

Shape and placement are two independent things, and each has its own Apply.

- **Apply Shape** changes what the object *is*. It is rebuilt at the new shape
  and dimensions and keeps exactly where it was — turning a moved, rotated box
  into a cylinder leaves the cylinder moved and rotated the same way.
- **Apply Transform** changes where the object *sits*. Its shape and dimensions
  are untouched, and so is the geometry itself: the object is moved and rotated,
  never rebuilt.

### Position and rotation

Position is measured in the display unit, from the world origin, and is applied
to the centre of the object. Zero and negative values are ordinary — a coordinate
is a place, not a size — so −1.25 m is as valid as 1.25 m.

Rotation is always in **degrees**, never in the display unit, and switching
between mm, cm and m leaves the rotation fields alone. Each axis turns the object
by the right-hand rule: looking down that axis toward the origin, a positive
angle turns counter-clockwise. The three are applied in the order X, then Y, then
Z, and the position is applied last — so rotating the object does not swing it
around the origin, it turns in place wherever it has been put. Negative angles
are ordinary, and an angle is kept exactly as typed: 370° stays 370° in the
fields rather than being rewritten as 10°, even though the two point the same
way.

Moving or rotating the object moves and rotates what can be tapped with it. A
sphere is the honest exception: it is the same shape from every direction, so
rotating one changes nothing about its outline or about where it can be tapped.
The rotation is still real and is kept exactly as typed — it simply has nothing
to show.

### Apply Transform

Like the shape, the six values are submitted together and the result is
**applied**, **unchanged** or **rejected**. Rejected leaves all six exactly as
they were, and a value that is not a number at all is reported with the field
named. Editing the text does nothing until Apply Transform is pressed.

## Sculpting

The object can be **sculpted** — pulled around by hand — as well as constructed
from exact numbers. Those are two different ways of describing the same object,
and ForgeShape keeps both.

### Freeze to Sculpt, and the Sculpt workspace

**Freeze to Sculpt**, in the Global Toolbar, takes the **selected** body's shape
as it currently stands and makes a sculptable copy of it, then switches to
Sculpt Mode. Nothing about that Construction Body changes: its shape, its
dimensions and its placement are exactly what they were, and they are still
there when Sculpt Mode is left. Freezing changes nothing visible — the picture
on screen before and after pressing the button is identical, pixel for pixel.

**Sculpting is per body, and each body keeps its own.** Freeze one body, sculpt
it, go back to Construction, select another body and freeze and sculpt that one
too — then come back to the first and Resume, and its own sculpting is exactly
as it was left. The two do not share a mesh, a history or a stale-source
warning, and sculpting one can never move a vertex of the other. The other
bodies stay visible while you sculpt, so you can see what you are working
against.

Which body is being sculpted is fixed for as long as Sculpt Mode lasts: to work
on a different one, go **Back to Construction**, select it there, and Freeze or
Resume. What is *not* per body is the brush — the tool you are holding and its
Radius and Strength stay exactly as you set them when you move between bodies,
for the same reason they stay put when you switch tools.

That button says **Freeze to Sculpt** only while nothing has been frozen yet.
Once there is a sculpted mesh it says **Resume Sculpt** instead, because those
are genuinely different acts and the wording is the only thing that tells them
apart before they happen: freezing starts from the constructed shape, resuming
goes back to the sculpting exactly as it was left.

In Sculpt Mode the Tool Rail carries the four tools — **Grab**, **Clay**,
**Smooth** and **Inflate** — and a **Radius** and a **Strength** sit directly at
the opposite edge, always on screen, adjustable without opening anything. The
shape and placement fields are not merely greyed out; they are not there, because
in Sculpt Mode there is nothing on screen that edits the constructed shape.

One tool is filled and outlined, and it is always the one that is actually
active: tapping a tool asks for it and the rail then shows what it got. The
status line says what the finger will do with the tool that is in hand. Choosing
a tool changes nothing about the model — nothing is redrawn or rebuilt, no
sculpting is lost, and the Radius and Strength stay where they were, because they
belong to the brush rather than to the tool.

The Property Inspector in Sculpt Mode carries what the sculpted mesh currently
is, the stale-source warning when there is one, and **Freeze again…** — the one
act in ForgeShape that cannot be undone. Pressing it when sculpting has actually
been done **to the mesh that exists right now** asks first, and labels the button
with what it does rather than *OK*. Cancelling changes nothing at all. Pressing
it on a mesh nothing has been done to just freezes, because there is nothing to
lose and a warning that cries wolf is worse than no warning.

The question asked is only ever about the current Frozen Sculpt Mesh. Sculpting
that was done, and then deliberately discarded by an earlier confirmed *Freeze
again*, is already gone; it is not something a later Freeze can take away. So
freezing a fresh mesh is silent no matter how much sculpting happened earlier in
the session. For the same reason the message names no number: what is at stake
is this mesh's sculpting, and a count carried over from meshes that no longer
exist would be a false claim about what is being lost.

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
obvious effect on every tool: on a frozen 2 m sphere, halving the radius took
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
constructed shape. Only Freeze to Sculpt does that, and it is the only thing that
discards sculpting.

### Changing the shape after freezing

Changing the constructed shape while a sculpted version exists never touches the
sculpting. The Sculpt inspector says so — it reports that the shape changed after
the freeze and that the sculpt has been kept as it is — and leaves the decision
alone: Freeze to Sculpt again to start from the new shape, or carry on sculpting
what is already there. ForgeShape does not transfer sculpting onto a new shape by
itself, and does not throw sculpting away without being asked.

### What sculpting can and cannot do

Sculpting moves the surface and never adds or removes any of it: the sculpted
copy always has exactly as many points and faces as the shape it was frozen from,
through every stroke of every tool. It changes only the sculpted copy — after
working over an object with all four tools, going back to Construction shows the
constructed shape at exactly its original dimensions in exactly its original
place, the picture identical and the numbers in the inspector unchanged.

What can be tapped follows the sculpting, not the constructed shape: on a
sculpted sphere, a point out on a raised protrusion is part of the model and can
be touched there, while the same distance from the centre in an untouched
direction is empty space.

A stroke that is interrupted stops where it is; it is not undone, because there
is no undo.

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

A short single-finger tap on the object selects it and tints the whole object
towards a warm orange. A short single-finger tap that misses clears the
selection, and it returns to exactly its unselected appearance. In Sculpt Mode
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

## What survives

The object's shape, its size, its position and rotation, its identity, the chosen
display unit, which mode is active, which sculpt tool is in hand, every sculpted
deformation, the camera pose, the chosen projection and its framing, and the
selection all survive sending ForgeShape to
the home screen and resuming it — the viewport comes back pixel-identical. On
resume the surfaces for the active mode are shown and every field, slider and
tool is rewritten from what the object actually is, so anything half-typed or
refused before leaving is replaced by the truth.

The object's identity is stable across a shape change: turning the box into a
sphere, then a cylinder, then a box again leaves it the same selectable object
throughout, still in the same place. All of it lives only for the life of the app
process — there is nothing to save or load and no undo, and camera, selection,
shape, placement and sculpting all return to their defaults when the process
restarts.

## Not yet implemented

Standard named views (Front, Top, Right and the rest), a view cube, a grid, a
focus-on-selection command and any camera animation are not implemented — the
projection can be switched, but the camera is aimed only by hand. Scaling the
object, transform gizmos, snapping, creating a second object, further
primitives (plane), an editable tessellation and booleans are not implemented:
shape, position and rotation are edited only by typing exact values and pressing
an Apply button. Sculpting has exactly the four tools above — other brushes
(Flatten, Crease, Pinch and the rest), remesh, sculpt undo, symmetry, masking,
layers, brush presets and stylus pressure are not implemented.

Exactly one body is selected at a time. Selecting more than one, lasso and box
selection, and object commands that act on a selection — delete, duplicate,
rename, hide, lock, group, nesting and reordering — are not implemented. The
Objects list adds and selects; it does nothing else.

Two-finger twist/roll, inertia, camera presets, orthographic camera,
focus-on-selection, long-press selection, grid, modelling, UV, save/load and undo
are not implemented.

Shading stops where it is. There is **one** MatCap and no way to add, import or
choose another; there are no materials, no colour or texture you can assign to
the object, no texture painting, no metalness or roughness, no environment or
HDRI, no movable or additional lights, no shadows, no ambient occlusion, and no
outline around the selected object — selection is a tint. A photoreal (PBR)
preview belongs to later work on materials and export.

**Sketch, Extrude and Export have visible homes and no implementation.** They
appear in the Editor Workspace — Sketch and Extrude in the Construction Tool
Rail, Export in the Global Toolbar — drawn so they can be read and clearly not
available, and pressing them does nothing. They are there so the workspace does
not change shape when they arrive; nothing about them works today. Nothing in the
workspace exports anything.

## Accepted product direction — decided, not built

Everything above this line is behaviour verified at runtime. Everything below it
is **direction the owner has accepted and nothing else**: none of it exists, none
of it can be tried, and it is recorded here only so the product's intended shape
is not carried in someone's head. The decisions themselves are in
`PROJECT_STATUS.md`.

**A Construction Body will be able to start from a sketch as well as from a
primitive.** Today the only way to make a shape is to choose one of the five
exact primitives and type its dimensions. The accepted direction adds a second
starting point: draw a closed 2D profile on a plane — lines and polylines,
rectangles, circles, with a grid, snapping and exact typed values — and extrude
it into a body. Sketching is a way of constructing, not a separate place to be:
it stays inside Construction, and the same exactness rule applies, so a sketch
dimension and an extrusion distance are authored values of the same kind as a box
width and are never inferred back from the geometry they produced. The workflow
is meant to be **repeatable** rather than a one-shot: several sketches, on
different planes and eventually on flat faces of an existing body, building a
shape up in steps that remain editable Construction history.

**The first extrusion will create a new body.** Extruding to *add* material to an
existing body, or to *cut* material out of it, is accepted as required direction
but depends on boolean infrastructure that does not exist, so it comes after that
and not before it.

None of this changes what Freeze means. Construction and sculpting stay two
separate representations of the one object, and moving from the first to the
second stays something the user asks for explicitly. A sketch or an extrusion is
Construction work, and it can no more be altered by sculpting than a box's width
can.

**The editor will become a viewport-first Editor Workspace on phone and tablet.**
The accepted shell keeps the model in view and puts tools at the edges rather
than in a block across the top, and it adapts across compact, medium and expanded
window sizes rather than assuming a portrait phone. Export is a global action
rather than an editing mode. A re-Freeze that would genuinely discard existing
sculpt work will ask first; an ordinary Resume Sculpt will not, because it
destroys nothing. None of this is built.

**Stylus support is a design constraint, not a feature yet.** ForgeShape must
stay comfortable with a stylus — an S Pen today, an Apple Pencil in whatever
comes later — and that shapes how the interface is laid out. It is not a feature
in the product: nothing anywhere reads pressure, tilt, hover or which kind of
pointer is touching the screen, so a stylus is simply another pointer and changes
nothing about a brush stroke. Relatedly, **Apple devices are a future direction,
not current support**: ForgeShape is an Android application and runs nowhere
else. The owner's decision is that it must stay *portable* to Apple platforms
later — an internal constraint on how the code is arranged, described in
`ARCHITECTURE.md` — not a claim that an iPhone, iPad or Apple Pencil is supported
today. Nothing in the product targets them.
