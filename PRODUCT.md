# ForgeShape — Product Behaviour

Only behaviour that has been verified at runtime, through the real Android touch
path, is documented here.

## What the viewport shows

The viewport shows one **Construction object**: an exactly dimensioned shape,
which is a **box**, a **cylinder**, a **sphere**, a **cone** or a **capsule**. Its dimensions are real
lengths in **meters**, and they are what the shape is made from — what is on
screen is generated from those numbers. Where it sits is a separate matter of its
position and rotation, and it starts at the world origin, unrotated.

There is exactly one object. Changing it from one of those shapes to another
changes what that object *is*; it does not create a second one, and nothing is
added to the scene.

The object comes up as a box, **2.0 m wide × 1.0 m high × 0.5 m deep**, so it is
visibly not a cube.

## Choosing and sizing the shape

A compact panel across the top of the viewport carries a **Shape** selector —
Box, Cylinder, Sphere, Cone and Capsule — the selected shape's dimensions, a display unit
(**mm**, **cm** or **m**) shared by every length, and an **Apply Shape** button.
Only the selected shape's fields are on screen, so nothing on the panel can be
mistaken for another shape's dimension. It also carries
the object's position and rotation, described further down. The rest of the
viewport stays visible and usable while editing.

The numbers in the fields are always the object's real dimensions, read from the
model itself — at startup they read 2, 1 and 0.5 in meters, matching the box on
screen.

### A box is width, height and depth

Those three are its extents, and it is centred on its own position.

### A cylinder is diameter and height

The cylinder is authored by **diameter**, not radius, because that is what a
drawing and a caliper give you. It stands along its own vertical axis, centred on
its position, and it defaults to 1.0 m across by 2.0 m tall.

How finely the curved surface is divided is not a setting. It is a detail of how
the exact cylinder is drawn, not part of what the cylinder is, so there is
nothing to choose and nothing to get wrong.

### A sphere is one diameter

A sphere is authored by **diameter** alone — there is nothing else to say about
it. It is centred on its own position, it reaches exactly half its diameter in
every direction, and it defaults to 1.0 m across.

As with the cylinder, how finely the curved surface is divided is not a setting.

### A cone is a bottom diameter and a height

A cone stands on a **flat, closed circular base** and comes to a **point** at the
top. It is authored by the diameter of that base and its height, it stands along
its own vertical axis centred on its position, and it defaults to 1.0 m across by
1.0 m tall.

There is no top diameter. The tip is a point, not a small circle, so a cone has
nothing to say about its top — a shape with a flat top of its own size is a
different shape, not a cone with an extra number.

A cone applied at 1.6 m across by 2.4 m tall showed exactly that: a solid cone,
apex up, tapering evenly to a base that is closed — looked at from underneath it
is a filled disc, not a hollow shell — and tapping it confirms the taper is real,
with the base sitting exactly 1.2 m below the centre.

### A capsule is a diameter and a total height

A capsule is a straight cylindrical middle with a **rounded hemispherical end**
on each end, like a pill. It is authored by its diameter and its **total**
height — the whole thing, rounded ends included, which is what you would measure
across the object — and it defaults to 1.0 m across by 2.0 m tall.

The middle is not entered separately: it is whatever is left after the two
rounded ends, so entering the total height is enough to say what the capsule is.
The rounded ends are true half-spheres of the capsule's own radius, so the shape
is exactly its diameter across at the waist and exactly its total height from tip
to tip.

Because the two rounded ends alone are already as tall as the capsule is wide,
**the total height cannot be less than the diameter**. Entering one that is says
so in as many words and changes nothing:

> Rejected: Total Height cannot be less than Diameter — the two rounded ends
> alone are that tall. Object unchanged.

A total height *equal* to the diameter is perfectly valid — it is a capsule with
no middle at all, which is a sphere, and it is drawn as a clean one.

A capsule applied at 1.2 m across by 3.0 m tall showed exactly that: parallel
straight sides through the middle with smoothly rounded ends and no visible seam
where they meet. Tapping it confirms both parts are real — two taps at different
heights in the middle land at the same distance from the axis, which is what
makes it a cylinder there, while a tap on an end lands on a true sphere surface
about that end's own centre.

### Choosing a shape changes nothing by itself

Moving the Shape selector only swaps which fields are on screen. The object stays
exactly what it was — same shape, same size, same place — until **Apply Shape**
is pressed.

Each shape remembers its own dimensions, so switching Box → Sphere → Cylinder →
Box brings each one's numbers back rather than resetting them, and the three sets
never mix: a sphere 1.5 m across and a box 1.5 m wide are the same number in two
different shapes, and neither is read as the other.

### Typing never changes the object

Editing the text does nothing on its own either. The object changes only when
**Apply Shape** is pressed. Nothing is applied field by field: the shape and all
of its dimensions are submitted together, and the result is one of three things.

- **Applied** — the object takes the new shape and size, the viewport shows it
  immediately, and what can be tapped is the new surface. Entering 125, 250 and
  75 in cm gives a box of exactly 1.25 × 2.5 × 0.75 m; a tap on its top face
  reports a hit at 1.25 m, exactly half its height. Choosing Cylinder and
  entering 120 and 240 in cm gives a cylinder of exactly 1.2 m across and 2.4 m
  tall; a tap on its end reports a hit at 1.2 m, exactly half its height.
  Choosing Sphere and entering 150 in cm gives a sphere of exactly 1.5 m across;
  taps on it land three quarters of a meter from its centre in every direction,
  and a tap just outside its round outline misses.
- **Unchanged** — the values are valid but describe the object it already is, so
  nothing happens at all. This is judged by the actual shape and size, not by the
  text: re-applying 125 × 250 × 75 cm and then the same box written as
  1.25 × 2.5 × 0.75 m both report Unchanged.
- **Rejected** — the panel shows what is wrong and the object does not change. A
  rejected edit changes nothing: not the dimensions that were fine, not the
  shape, and not where the object sits.

### Choosing a display unit

Switching between mm, cm and m rewrites every length into the chosen unit and
does nothing else. The object is not touched, not redrawn and not re-uploaded;
only the way the same size is written down changes. 2 m, 200 cm and 2000 mm are
the same length.

The conversion is exact, so switching m → cm → mm → m returns the original
numbers digit for digit, and no amount of unit switching can drift the object's
real size. Values are written without a thousands separator and without trailing
zeros.

### Typing dimensions

The fields take a numeric keypad. Either `.` or `,` may be used as the decimal
separator — typing `333,3` in cm produces exactly 3.333 m, and the field is
rewritten as `333.3` once applied.

Dimensions are not silently corrected. An empty field, a value of zero or less,
and text that is not a number are each refused with a message naming the field,
and the offending field is focused ready to be fixed. That holds for a cylinder
and a sphere exactly as for a box: a zero diameter is reported as "Diameter must
be greater than 0", `1.2.3` as "Diameter is not a number", and the object keeps
the shape, size and place it already had. Letters cannot be typed into a
dimension field at all.

## Moving and rotating the object

Below the shape fields the same panel carries the object's **placement**: a
Position X/Y/Z in the shared display unit, a Rotation X/Y/Z in degrees, and a
separate **Apply Transform** button.

The object starts at position 0, 0, 0 m with rotation 0, 0, 0°, so it begins
centred at the world origin and unrotated. Placement belongs to the object, not
to the shape, so it survives any change of shape untouched — Box, Cylinder,
Sphere, Cone and Capsule alike. A cone moved to 1.5 m along X and turned 90°
about Z lies on its side exactly where those numbers say, and tapping it still
lands on the surface that is drawn.

### Moving is not resizing

Shape and placement are two independent things, and each has its own Apply.

- **Apply Shape** changes what the object *is*. It is rebuilt at the new shape and
  dimensions and keeps exactly where it was — turning a moved, rotated box into a
  cylinder leaves the cylinder moved and rotated the same way.
- **Apply Transform** changes where the object *sits*. Its shape and dimensions
  are untouched, and so is the geometry itself: the object is moved and rotated,
  never rebuilt.

### Position

Position is measured in the display unit, from the world origin, and is applied
to the centre of the object. Zero and negative values are ordinary — a coordinate
is a place, not a size — so −1.25 m is as valid as 1.25 m.

Moving the object moves what can be tapped with it. A box 1 m tall lifted to
y = 0.25 m reports a tap on its top face at exactly 0.75 m.

### Rotation

Rotation is always in **degrees**, never in the display unit, and switching
between mm, cm and m leaves the rotation fields alone.

Each axis turns the object by the right-hand rule: looking down that axis toward the
origin, a positive angle turns counter-clockwise. The three are applied in the
order X, then Y, then Z, and the position is applied last — so rotating a box
does not swing it around the origin, it turns in place wherever it has been put.

Negative angles are ordinary, and an angle is kept exactly as typed: 370° stays
370° in the fields rather than being rewritten as 10°, even though the two point
the same way.

Rotating the object rotates what can be tapped with it. A 2 m wide box turned 90°
about Z stands on end, and a tap finds its top face at 1 m above its centre —
where the 2 m width now reaches — rather than at the half-height it had before.

A sphere is the honest exception: it is the same shape from every direction, so
rotating one changes nothing about its outline or about where it can be tapped.
The rotation is still real and is kept exactly as typed — it simply has nothing
to show.

### Apply Transform

Like the shape, the six values are submitted together and the result is **applied**,
**unchanged**, or **rejected**. Rejected leaves all six exactly as they were, and
a value that is not a number at all is reported with the field named.

Editing the text does nothing until Apply Transform is pressed.

## What survives

The object's shape, its size, its position and rotation, its identity, the chosen
display unit, which mode is active, which sculpt tool is in hand and every
sculpted deformation all survive sending ForgeShape to the home screen and
resuming it — the viewport comes back pixel-identical. On resume the panel for
the active mode is shown and every field, slider and tool button is rewritten
from what the object actually is, so anything half-typed or refused before
leaving is replaced by the truth.

Shape, placement and sculpting live only for the life of the app process: there
is nothing to save or load, and no undo.

## Sculpting

The object can be **sculpted** — pulled around by hand — as well as constructed
from exact numbers. Those are two different ways of describing the same object,
and ForgeShape keeps both.

### Freeze to Sculpt

**Freeze to Sculpt**, at the bottom of the properties panel, takes the shape as
it currently stands and makes a sculptable copy of it, then switches to Sculpt
Mode. Nothing about the Construction object changes: its shape, its dimensions
and its placement are exactly what they were, and they are still there when
Sculpt Mode is left.

Freezing a 2 m sphere gives a sculptable copy that is, at that moment, exactly
that sphere — the picture on screen before and after pressing the button is
identical, pixel for pixel.

### The Sculpt panel

Sculpt Mode replaces the properties panel with a compact strip carrying the four
tools — **Grab**, **Clay**, **Smooth** and **Inflate** — a **Radius**, a
**Strength**, and the way back. The shape and placement fields are not merely
greyed out; they are not there, because in Sculpt Mode there is nothing on screen
that edits the constructed shape.

One tool is highlighted, and it is always the one that is actually active:
tapping a tool asks for it and the panel then shows what it got. A line below the
sliders says what the finger will do with the tool that is in hand.

Choosing a tool changes nothing about the model. Nothing is redrawn or rebuilt,
no sculpting is lost, and the Radius and Strength stay exactly where they were —
they belong to the brush, not to the tool.

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
touched at all — not moved a fraction, and nothing to undo.

A tap on the model changes nothing.

Every tool holds the surface where it was touched and falls off smoothly outwards
to nothing at the edge of the brush, so a stroke leaves a smooth result rather
than a crease. What a stroke has hold of is fixed when the finger lands, so the
deformation stays one coherent shape however far the finger travels.

### Grab

Grab drags the surface with the finger. The result depends only on where the
finger ends up, not on how fast it got there or how far it wandered on the way.

### Clay, Smooth and Inflate

The other three work as you **drag over** the surface, and how much they do
depends on how far the finger has travelled — not on how long you held it there.
A finger that stops moving does nothing at all, and the same path always does the
same amount whether you draw it quickly or slowly. Keep dragging over the same
place and the effect keeps building.

- **Clay** deposits material outward, building a raised shape where you work.
  Repeated passes make it taller.
- **Smooth** evens out roughness by pulling each point toward the middle of its
  neighbours. It can only ever move part of the way there, so repeated
  smoothing settles down instead of overshooting. A sharp spike pulled out with
  Grab is flattened back into the surface by a couple of Smooth strokes.
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
obvious effect on every tool: on a frozen 2 m sphere, halving the radius from
312 px to 151 px took Clay from 51 points to 11 and Inflate from 29 to 8.

**Strength** is how much each tool does. For Grab it is how much of the finger's
travel the grabbed surface follows: at the maximum the surface keeps up with the
finger, at the minimum it barely moves. For the other three it scales how much is
deposited, relaxed or expanded — measured on the same object with the same
stroke, going from the low end of the slider to the top moved the surface about
seventeen times as far, for both Clay and Inflate.

Neither can be set to a useless value: both stop at the ends of their range.

### Switching back and forth

**Back to Construction** shows the constructed object again, exactly as it was
before any sculpting — the original shape, at its original dimensions, in its
original place. The sculpting is not lost and nothing is copied back into the
constructed shape.

**Resume Sculpt** returns to the sculpted version with every deformation still
there. It does not start again from the constructed shape; only pressing Freeze
to Sculpt does that, and that is the only thing that discards sculpting.

### Changing the shape after freezing

Changing the constructed shape while a sculpted version exists never touches the
sculpting. The Sculpt panel says so — it reports that the shape changed after the
freeze and that the sculpt has been kept as it is — and leaves the decision
alone: Freeze to Sculpt again to start from the new shape, or carry on sculpting
what is already there. ForgeShape does not transfer sculpting onto a new shape by
itself, and does not throw sculpting away without being asked.

### What sculpting can and cannot do

Sculpting moves the surface. It never adds or removes any of it: the sculpted
copy always has exactly as many points and faces as the shape it was frozen
from, through every stroke of every tool.

There are four tools. Flatten, Crease, masking, symmetry, layers, stylus
pressure, brush presets, remesh and sculpt undo are not implemented. A stroke
that is interrupted stops where it is; it is not undone, because there is no
undo.

### What sculpting never touches

Sculpting changes only the sculpted copy. After working over an object with all
four tools, going back to Construction shows the constructed shape at exactly its
original dimensions in exactly its original place — the picture is identical, and
the numbers in the panel are unchanged. Returning to Sculpt brings every
deformation back exactly as it was, down to the pixel.

What can be tapped follows the sculpting, not the constructed shape: on a
sculpted sphere, a point out on a raised protrusion is part of the model and can
be touched there, while the same distance from the centre in an untouched
direction is empty space.

## Viewport navigation

Touches that land on the properties panel stay in the panel: dragging across it
never orbits, pans or zooms, and never selects. Everything outside the panel is
the viewport and behaves exactly as described below. Touching the viewport also
ends any text edit — focus and the keyboard come back to the model.

Navigation never moves the object. Unless its transform is changed, it stays
exactly where it is and everything the user sees move is the camera, which
orbits a **target** point and is described by target, yaw, pitch and distance.

### One finger — Orbit

Dragging with one finger orbits the camera around the target. The target does not
move and the distance does not change.

- Drag **right** → the model spins to the right (yaw decreases).
- Drag **down** → the camera rises above the model (pitch increases).
- Sensitivity: `0.005` radians per pixel, so a full 1080 px sweep rotates about
  310 degrees.
- Yaw wraps freely. Pitch is clamped to ±1.52 rad (±87.1°) so the view never
  flips over the pole.

### Two fingers — Pan

Moving two fingers together slides the target in the camera plane, so the scene
follows the fingers. Orbit angles and distance are unchanged.

One pixel of finger travel moves the scene by one pixel measured at the target
plane, so panning feels identical at every zoom level and on every screen size.
Panning moves the camera only; it never modifies geometry.

### Pinch — Zoom

Changing the distance between two fingers changes the camera's distance to the
target. Spreading zooms in, pinching zooms out.

Zoom is multiplicative: distance is scaled by `exp(-Δspan × 0.0035)`, so the same
finger travel always produces the same zoom ratio, and the distance can never
reach zero or go negative. Distance is clamped to `[0.35, 400]`.

Pan and pinch are interpreted from the same two-finger gesture, so they can be
performed simultaneously.

### Gesture transitions

Adding or removing a finger re-anchors the gesture and produces no camera jump.
Lifting all fingers, or an interrupted gesture (`ACTION_CANCEL`), clears the
gesture without moving the camera and leaves no stale movement for the next
gesture.

## Selection

The Construction object can be selected. Selection is a highlight only: it changes
nothing about the model, and there is nothing yet that acts on the selected
object.

In Sculpt Mode one finger on the model sculpts instead of selecting — that is
what one finger on the model means there — so tap-to-select as described below is
Construction Mode behaviour. A tap on empty space still clears the selection in
either mode.

### Tap to select

A short single-finger tap on the object selects it. The whole object is tinted towards
a warm orange, which is unmistakable against its normal colouring.

### Tap empty space to clear

A short single-finger tap that misses the object clears the selection, and it
returns to exactly its unselected appearance.

### Navigation never selects

- Dragging with one finger orbits and never selects or clears, no matter where
  the finger lifts. A gesture stops being a tap once the finger has travelled
  more than about 24 pixels from where it went down; that measurement is taken
  from the original touch point, so a slow drag cannot sneak past it.
- Any gesture that uses two or more fingers is navigation only. Pan and pinch
  never select or clear, including when the second finger lifts first and the
  gesture ends on one finger.
- A gesture interrupted by the system leaves the selection untouched.

### What gets selected

The tap selects whatever visible surface is under the finger, so it follows the
camera: after orbiting or zooming, tapping the object where it now appears on
screen selects the same object. Only faces that are actually drawn can be hit —
tapping never reaches through the object to a surface on the far side.

What can be tapped is the object's real surface at its real dimensions. A tap on a
box's top face reports a hit at exactly half its height; a tap on a cylinder's end
reports a hit at exactly half its height, and a tap just outside its round
silhouette misses — where a box of the same width would have been hit. A tap
anywhere on a sphere lands half its diameter from its centre, and a tap beside
its outline misses, including at the corner a box of the same width would fill.

Resizing follows immediately: after a 1.5 m sphere is changed to 0.6 m, the place
that used to be its surface is empty, and taps now land 0.3 m from its centre.

## Camera persistence

The camera pose (target, yaw, pitch, distance) is preserved for as long as the
app process lives. Sending ForgeShape to the home screen and resuming it
recreates the Surface but restores the exact same view; verified by a
byte-identical screenshot across a home/resume cycle.

The camera resets to its default framing only when the process is restarted.

## Selection persistence

Orbiting, panning and zooming never disturb the selection. Sending ForgeShape to
the home screen and resuming it keeps the object selected and still highlighted;
verified by a screenshot byte-identical to the one taken before leaving.

Like the camera, the selection is cleared only when the process is restarted.

## Object persistence

The object's shape, its dimensions, its position and rotation, and its identity
all survive a home/resume cycle: the same object, in the same shape at the same
size and in the same place, is still there and still selectable afterwards.

Its identity is stable across a shape change too: turning the box into a sphere,
then a cylinder, then a box again leaves it the same selectable object
throughout, still in the same place.

## Not yet implemented

Scaling the object, transform gizmos, snapping, creating a second object, other
primitives (cone, capsule, plane), an editable tessellation and booleans are not
implemented. Shape, position and rotation are edited only by typing exact values
and pressing an Apply button.

Sculpting has four tools: Grab, Clay, Smooth and Inflate. Other brushes
(Flatten, Crease, Pinch and the rest), remesh, sculpt undo, symmetry, masking,
layers, brush presets and stylus pressure are not implemented, and there is only
one sculptable object.

Selecting more than one object, an object list or outliner, lasso and box
selection, and anything that acts on the selection (moving, editing, deleting)
are not implemented — there is exactly one selectable object.

Two-finger twist/roll, inertia, camera presets, orthographic camera,
focus-on-selection, long-press selection, grid, gizmos, modelling,
UV, export, save/load and undo are not implemented.

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
rectangles, circles, with a grid, snapping and exact typed values — and extrude it
into a body. Sketching is a way of constructing, not a separate place to be: it
stays inside Construction, and the same exactness rule applies, so a sketch
dimension and an extrusion distance are authored values of the same kind as a box
width and are never inferred back from the geometry they produced.

The workflow is meant to be **repeatable** rather than a one-shot: several
sketches, on different planes and eventually on flat faces of an existing body,
building a shape up in steps that remain editable Construction history.

**The first extrusion will create a new body.** Extruding to *add* material to an
existing body, or to *cut* material out of it, is accepted as required direction
but depends on boolean infrastructure that does not exist, so it comes after that
and not before it.

None of this changes what Freeze means. Construction and sculpting stay two
separate representations of the one object, and moving from the first to the
second stays something the user asks for explicitly. A sketch or an extrusion is
Construction work, and it can no more be altered by sculpting than a box's width
can.

**Apple devices are a future direction, not current support.** ForgeShape is an
Android application and runs nowhere else. The owner's decision is that it must
stay *portable* to Apple platforms later — which is an internal constraint on how
the code is arranged, described in `ARCHITECTURE.md` — not a claim that an
iPhone, iPad or Apple Pencil is supported today. Nothing in the product targets
them.

**Stylus support is a design constraint, not a feature yet.** ForgeShape must
stay comfortable with a stylus — an S Pen today, an Apple Pencil in whatever
comes later — and that shapes how the interface is laid out. It is not a feature
in the product: nothing anywhere reads pressure, tilt, hover or which kind of
pointer is touching the screen, so a stylus is simply another pointer and changes
nothing about a brush stroke.
