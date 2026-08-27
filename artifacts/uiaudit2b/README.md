# UI-AUDIT2B — Emil Kowalski interaction and motion audit (independent pass)

**Method: single lens, Emil only.** The Impeccable conclusions from UI-AUDIT2A
were not used as authority. Every number in this report was measured again, in
this session, from the running app and from the source — including the numbers
that happen to agree with 2A. Where 2B contradicts or extends 2A, it says so.

Read-only. **No product source, resource, shader or build file was changed.**
`git diff --stat bd3f7d5 HEAD -- . ':(exclude)artifacts'` is empty: the product
still matches the Stage 020R3 state exactly, and the only commits since are
evidence.

Hard gate: [`SKILL-PROOF.md`](SKILL-PROOF.md). The skill is a single file,
`C:\Users\damia\.claude\skills\emil-design-eng\SKILL.md`, 27900 bytes, 674
lines, SHA-256 `defffff8bea4583897b001b9173ccd6fb6f8341fffc63a1891a38372137a1848`,
**no `version:` field declared** — recorded as absent rather than invented.

---

## The instrument, and why it can be trusted

Reading `ChromeMotion.java` tells you what the product *intends* to animate. It
does not tell you what actually moves on screen. So this audit built a probe
instead, straight out of the skill's **Debugging Animations** section ("play
animations at reduced speed to spot issues invisible at full speed"):

1. `adb shell settings put global animator_duration_scale 20`. ForgeShape reads
   `Settings.Global.ANIMATOR_DURATION_SCALE` itself, and the platform applies it
   to every `ViewPropertyAnimator`. A 190 ms growth becomes 3800 ms.
2. Trigger the interaction, then take **six back-to-back** `screencap -p` frames.
   Measured cadence: **509 ms per frame**, so six frames span 3.05 s — inside the
   stretched animation.
3. Compare frame digests. **A static ForgeShape screen produces byte-identical
   PNGs** — verified before any probe ran (four frames, one digest,
   `0b55009f b2a2`). So differing digests mean motion, and identical digests from
   frame 1 onward mean *no animator ran at all*, not "an animation I missed".

That last point is what makes the result strong. At 20× scale, a 100 ms
animation would still be running two seconds later. Six identical frames is
proof of a hard cut.

Platform scales during the probe, recorded for honesty:
`animator_duration_scale = 20`, `window_animation_scale = 1.0`,
`transition_animation_scale = 1.0`. All restored to `1` afterwards, along with
`wm size`, `wm density` and auto-rotate.

Device: `ForgeShape_Stage006` / `emulator-5580`, confirmed by
`adb -s emulator-5580 emu avd name`. The skill says gestures and performance need
real hardware; nothing in this report claims a frame-rate or a gesture-feel
verdict, only what does and does not animate, from where, and how far.

---

## Evidence inventory

| id | artifact | what it establishes |
| --- | --- | --- |
| M01 | `M01-motion-probe.txt` | the full probe log: 18 interactions, frame digests, platform scales |
| M02 | `M02-precision-open-frame1..5.png` | the precision surface growing while every other control is already at its final position |
| M03 | `M03-appearance-after.png` | the appearance switch — 8 identical frames, one hard repaint |
| M04 | `M04-back-with-surface-open-frame1,2.png` | Android Back with the palette measurably open at `[155,1412][859,2158]` → launcher |
| M05 | `M05-reduced-motion.txt` | `animator_duration_scale 0`: every transition lands instantly, no transient state |
| M06 | `M06-expand-frame1,4.png` | compact → expanded window-class change |
| M07 | `M07-interruption.txt`, `M07-interrupt-midgrowth.png`, `M07-interrupt-reversal1,4.png` | a second tap mid-growth reverses continuously: 67,70,72 → 64,67,69 → 62,66,67 → 61,64,66 |
| M08 | `M08-jump-measurements.txt` | independent before/after bounds for every layout jump, in px |
| M09 | `M09-landscape-expanded-motion.txt`, `M09-landscape-precision-frame1,5.png` | landscape: the rail completes a 312 dp teleport before the panel draws one pixel |

Source read in full: `ChromeMotion.java`, `AnchoredSurfaceView.java`,
`forgeshape_selection_pulse.h/.cpp`, the `ChromeMotion` call sites in
`EditorWorkspaceView.java`, `bg_icon_button.xml`, `bg_rail_entry.xml`, and a
`grep` over all 53 drawables for press states and fade durations.

---

## The one-paragraph verdict

**The motion ForgeShape has is close to textbook. There is just far too little
of it, and it is spent on the transitions that displace nothing.**

Five of the eighteen audited interactions animate. All five are surfaces
appearing or disappearing in place. The thirteen that do not animate include
every interaction that *moves something* — the mode switch that shifts the whole
right-edge column 139 dp, the precision surface that shifts it another 120 dp and
the bottom row 260 dp, and the Construction↔Sculpt swap that replaces the
workspace's entire control inventory. The result is an app where a panel fades in
politely while four controls teleport behind it, **in the same frame** — which
this audit proved by capturing that frame.

---

## Required interaction inventory

Durations are the base constants; at probe time each was multiplied by 20.
"Instant" means six probe frames with one digest — no animator ran.
Displacements are from [`M08`](M08-jump-measurements.txt), compact portrait,
2.625 px/dp.

| interaction | start state | end state | elements moving | current duration / easing | continuity | interruption / cancel | reduced motion | Emil verdict |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Add Primitive open** | palette absent | palette at `[155,1412][859,2158]` | palette only; **zero** other bounds change | 190 ms, `cubic-bezier(.23,1,.32,1)`, alpha + uniform scale 0.96→1 | grows from its own bottom-left = the `+` it hangs off | cancel-first; verified reversal is continuous | lands instantly, no transient | **PASS** — the model the rest of the product should copy |
| **Add Primitive close** | palette open | absent | palette only | 150 ms, same curve | shrinks back into the `+` | same | instant | **PASS** — exit faster than enter, correct ratio |
| **Objects open** | popover absent | `[21,1760][599,2147]` | popover only; zero other bounds | 190 ms / 150 ms | grows from the capsule it hangs off | cancel-first | instant | **PASS** |
| **Objects close** | open | absent | popover only | 150 ms | same | same | instant | **PASS** |
| **Objects → docked (expanded)** | floating capsule | `objects_dock` panel, top-left | whole control changes identity | **instant** (config change) | none — one control is destroyed and a different one is created | n/a | n/a | **acceptable** — a window-class change, not a user act |
| **Exact Shape open** | surface absent | inspector sheet | sheet grows; rail, modes, bottom row **teleport** | 190 ms on the sheet; **0 ms** on everything else | broken — see E-02 | cancel-first on the sheet only | instant | **ISSUE E-02** |
| **Exact Shape close** | open | absent | sheet shrinks; everything else teleports back | 150 ms / 0 ms | same | same | instant | **ISSUE E-02** |
| **Exact Transform open** | absent | sheet `[21,1654][1059,2316]` | sheet grows 190 ms; rail −120.4 dp, modes −120.4 dp, space −120.4 dp, toggle −124.6 dp **and compressed 126→97 px**, objects/history −260.2 dp — all instant | 190 ms sheet / 0 ms chrome | **broken twice**: neighbours teleport (E-02), and the sheet grows from the corner farthest from its trigger (E-03) | cancel-first on the sheet | instant | **ISSUE E-02, E-03** |
| **Exact Transform close** | open | absent | reverse; bounds restore exactly | 150 ms / 0 ms | same | same | instant | **ISSUE E-02** |
| **precision toggle (the control itself)** | inactive | active | background drawable swapped | **instant**, no fade | the control that owns the surface does not acknowledge the press beyond a colour flip | n/a | n/a | **ISSUE E-06** |
| **Move ↔ Rotate ↔ Scale** | one mode | another | active pill jumps between segments; on Move→Scale the whole column shifts +56.4 dp; gizmo geometry swaps entirely | **instant** — 6 identical frames | none | n/a | n/a | **mixed**: instant is *right* (E-R1), the 56.4 dp shift is not (E-02), the pill teleport is a missed 150 ms (E-04) |
| **World ↔ Local** | one space | other | active pill jumps; gizmo axes reorient | **instant** — 6 identical frames | none | n/a | n/a | **mixed** — same as above; gizmo reorientation is E-10 |
| **Undo pressed** | Undo enabled, Redo disabled | inverted; a body disappears | both button alphas swap; scene redraws | **instant** | none; no verdict is reported | n/a | n/a | **ISSUE E-11** (state, not motion) |
| **Redo pressed** | inverted | restored | same | **instant** — 6 identical frames | none | n/a | n/a | **ISSUE E-11** |
| **Start Sculpting** | Construction workspace | Sculpt workspace | toolbar button, rail (373→719 px tall, +131.8 dp), brush column appears at `[21,927][326,1602]` from nothing, Undo/Redo and `+` destroyed, mesh replaced | **instant** — 6 identical frames | **none at all** | n/a | n/a | **ISSUE E-01** — the biggest change in the product, zero motion |
| **Back to Construction** | Sculpt | Construction | same in reverse | **instant** | none | n/a | n/a | **ISSUE E-01** |
| **Resume Sculpt** | Construction | Sculpt | same as Start Sculpting | **instant** | none | n/a | n/a | **ISSUE E-01** |
| **Sculpt tool change** | Grab | Clay | active rail entry fill moves | **instant** (frames 1-3 identical; the only later change was the system status-bar clock at `[828,60][832,76]`) | none | n/a | n/a | **PASS / rejected idea E-R1** — correctly instant |
| **Radius / Strength reveal** | Construction (absent) | Sculpt (present) | 116×257 dp column appears | **instant**, inside E-01 | none | n/a | n/a | **ISSUE E-01** |
| **Sculpt details open / close** | absent | inspector sheet, 157 dp | sheet grows; brush column −83 dp, rail −82 dp instant | 190 / 150 ms sheet, 0 ms chrome | broken | cancel-first | instant | **ISSUE E-02** |
| **Appearance switch** | one palette | another; Activity recreated | every colour on screen | **instant** — 8 identical frames | none | n/a | n/a | **ISSUE E-07** |
| **portrait ↔ landscape** | portrait | landscape | whole workspace restructures | system rotation animation only (`window_animation_scale 1.0`); app contributes nothing | system-owned | n/a | n/a | **acceptable** — see the landscape section |
| **compact ↔ expanded** | 411 dp | 1067 dp | whole workspace restructures | no app-owned motion; frames differ only because the display and swapchain are being reconfigured | n/a | n/a | n/a | **acceptable, unproven** — the probe cannot separate app from system here, and says so |
| **Android Back, surface open** | palette open `[155,1412][859,2158]` | launcher | everything | the system's task-close animation | **broken** — the palette's own 150 ms exit never runs; the surface is swept off inside an app-exit transition | nothing cancels | n/a | **ISSUE E-12** |

---

## Deep questions, answered per interaction class

**Should anything move at all?**
For the five surfaces that already animate: yes — elements appearing and
disappearing without transition feel broken, and these are occasional actions.
For mode, space and sculpt-tool switching: **no** — a modeling tool switches
those tens to hundreds of times a session, which is the skill's "remove or
drastically reduce" band. For Construction↔Sculpt: yes — it is rare, it is a
whole-context change, and it is exactly the "explanation" purpose. For the
appearance switch: yes — it is rare and every pixel changes.

**What spatial relationship is being communicated?**
The anchored growth says *this panel belongs to that button*, and it says it
correctly for three of the four surfaces. It says it **wrongly** for the
precision surface, whose growth origin is its own bottom-left while the toggle
that opened it is above its top-right. And no relationship at all is communicated
by the chrome jumps, because they are not motion — they are the same control
existing at two different addresses on consecutive frames.

**Is continuity preserved from trigger to surface?**
Objects popover, Add Primitive, Display popover: yes, verified. Precision
surface: no — measured. Everything else: there is no continuity to preserve
because there is no transition.

**Is enter faster/slower than exit appropriately?**
Yes, everywhere motion exists: 190→150 and 120→90. Both ratios are right and
both are inside the skill's bands.

**Are elements that should feel physically related moving together?**
No, and this is the audit's central finding. The Tool Rail, the mode capsule, the
space capsule and the precision toggle are one column, 4 dp apart, and they do
move together — but they move together *instantly*, while the surface that caused
them to move takes 190 ms. Three things that are physically one event are split
across two timelines, one of which has zero duration.

**Do unrelated controls jump?**
Yes. Opening the precision surface moves the Objects capsule and the history
capsule 260.2 dp. That movement is *justified* — a surface may not cover a live
control — but it is unannounced, and the user's eye is on the bottom of the
screen when it happens.

**What should interrupt or cancel?**
Cancel-first is already correct and was verified at runtime: a second tap
mid-growth reverses from the current value with no snap
(67,70,72 → 64,67,69 → 62,66,67 → 61,64,66). What is missing is a cancel path for
Android Back (E-12), and a rule that a viewport gesture beginning mid-transition
settles the chrome rather than letting it finish — the product has the inverse
rule (`setChromeMotionAllowed(false)` prevents *new* transitions during a gesture)
but nothing settles one already running.

**Should a state swap crossfade, scale, translate, or be instant?**
Active pill between segments: **translate** (150 ms) — the segments are adjacent
and the pill is one object. Appearance switch: **crossfade** (~200 ms) — two
complete images of the same layout. Undo/Redo enable/disable: **crossfade**
(~120 ms alpha) — nothing moves. Mode and tool selection: **instant**. Gizmo
geometry: **instant**, with one exception (E-10).

**Does reduced motion preserve meaning?**
Yes, and provably. See the reduced-motion section.

**Does motion help or merely decorate?**
Every animation currently in the product helps. None of it is decoration. The
problem is the inverse of the usual one: this is an under-animated product, not
an over-animated one.

---

## Portrait / landscape / expanded motion findings

### Compact portrait

Where the damage is. Independently measured from
[`M08`](M08-jump-measurements.txt), all instant:

| transition | Tool Rail | mode capsule | space capsule | precision toggle | bottom row |
| --- | --- | --- | --- | --- | --- |
| resting → Transform | 955→591 (**−138.7 dp**) | appears | appears | 1350→1715 (+139.0 dp) | — |
| Move → Scale | 591→739 (+56.4 dp) | 975→1123 | destroyed | 1715→1567 (−56.4 dp) | — |
| → precision open | 591→275 (**−120.4 dp**) | −120.4 dp | −120.4 dp | −124.6 dp **and 126→97 px tall** | 2168→1485 (**−260.2 dp**) |
| precision close | exact restore | exact | exact | exact | exact |

[`M02-precision-open-frame1.png`](M02-precision-open-frame1.png) is the proof:
the sheet is still at partial alpha with its title text sitting 11 px low, and
the rail, the mode capsule, the space capsule and the Objects capsule are already
at their final open-state positions.

### Short landscape

Same defect, larger and on the other axis, and independently confirmed by pixel
sampling rather than by inference ([`M09`](M09-landscape-expanded-motion.txt)):

- before: `tool_rail = [2200,225][2379,514]`; after: `[1380,225][1559,514]` — a
  **820 px = 312.4 dp** horizontal teleport.
- probe frame 1, new rail slot (1470,370) = `68,65,61` — **the rail is already
  there**.
- probe frame 1, panel interior (1900,600) = `50,47,45` — bare viewport; the
  panel has not drawn a pixel yet. By frame 5 it is `62,59,56`.

So in landscape the rail finishes a 312 dp journey *before* the surface that
displaced it becomes visible at all. This is the strongest single piece of
evidence in the audit.

### Expanded

The best case and still not animated. The inspector docks as a column and the
rail moves horizontally; vertically nothing moves at all, which is why the
expanded layout feels calmer than compact even with no motion. The window-class
change itself (`M06`) produces differing frames, but the display and the
swapchain are being reconfigured underneath, so the probe cannot attribute those
frames to the app. Recorded as **acceptable, unproven** rather than as a pass.

### Rotation

Portrait↔landscape produced two distinct frames then settled, with
`window_animation_scale` at 1.0 — that is the platform's rotation animation. The
app adds nothing, and should not: the skill's frequency rule and the repository's
own orientation convention both argue against the app animating a configuration
change it does not own.

---

## Transform and gizmo motion review

Four separate questions, four different answers.

**1. UI chrome on a mode or space change — should not animate, but should not
jump either.** The switch itself is high-frequency: instant is right (E-R1). The
139 dp and 56.4 dp column shifts are not the switch, they are a layout
consequence, and they must be **removed structurally**, not animated. See the
layout-jumps section.

**2. Gizmo geometry — should not animate, with one exception.** Move→Rotate
replaces three arrows and three plane quads with three rings. Morphing that would
be decoration on an action performed constantly, and would leave a frame in which
the handles mean nothing. Instant is correct, and the probe confirms it is
instant. **The exception is World↔Local (E-10):** the handles do not change
identity, they change *direction*. Reorienting the same three axes from world to
body axes is a rotation of one rigid object, and a rotation shown as a cut is the
one case where the user genuinely cannot tell which way anything turned. A short
interpolation of the orientation — under 150 ms, ease-out, and only when the two
orientations actually differ — would answer "which way is Local?" better than any
label. This is the single place in the product where a spring would be defensible,
because it is a rotation with a real start and end orientation and no fixed
distance.

**3. Active state — should transition, and does not (E-04).** The mode capsule's
three segments are adjacent, identically sized, 4 dp apart, and exactly one is
filled. The fill is applied with `setBackgroundResource`, so it is destroyed at
one address and created at another. This is the textbook segmented-control case:
the pill should **translate** between segments in ~150 ms with the product's
existing ease-out curve. It moves 48 dp; it is seen constantly; and unlike the
column jumps, this movement is *desirable* — it is one object moving a short,
legible distance, which is precisely spatial consistency.

**4. Pivot / plane / handle transitions — should not animate.** The pivot never
moves during a mode change; the plane handles appear and disappear with the mode.
They are part of the geometry swap and belong to it. No motion.

**The one gesture handoff.** A gizmo drag ends with the handle released at the
authoritative value. Nothing settles, springs back or acknowledges the commit.
The skill's gesture section is mostly about dismissal, but its principle applies:
things in real life slow down rather than stopping. A ~120 ms settle on the
handle's highlight at drag end — not on the geometry, which must stay
authoritative — would confirm the commit. Filed as a **P3 delight** item, not a
correctness defect.

---

## Construction ↔ Sculpt: a continuity model

The word this section may not use is deliberately absent from the vocabulary the
user reads, so the model below is stated in the user's terms: **Construction
Body**, **sculpt mesh**, **Start Sculpting**, **Back to Construction**, **Resume
Sculpt**.

### What actually changes, measured

| | Construction | Sculpt |
| --- | --- | --- |
| toolbar transition button | `Start Sculpting` / `Resume Sculpt`, 121-123 dp | `Back to Construction`, 150.1 dp |
| Tool Rail | 2 entries, 373 px tall | 4 entries, 719 px tall (**+131.8 dp**) |
| brush column | absent | `[21,927][326,1602]` — 116×257 dp, appears from nothing |
| Objects capsule | 421 px wide, carries `+` | 305 px wide, no `+` |
| history capsule | present | destroyed |
| the object itself | exact primitive | sculpt mesh |

All of it in one frame. Six identical probe frames at 20× scale.

### The problem, in Emil's terms

This is not a state change inside a screen; it is a change of *what the workspace
is for*. The skill's frequency table puts it in the "occasional" band — a user
enters sculpting a handful of times per session — which is exactly where a
standard animation belongs. And its purpose is the strongest of the five the
skill lists: **explanation**. The user needs to understand that the same object
is now a different kind of thing, that a different set of tools now applies, and
that the exact dimensions have not gone away but have stepped back.

Cutting it teaches nothing. Worse, it makes the two modes feel like two
applications rather than two states of one object, which is precisely the mental
model the product's own architecture insists on.

### The model I would build

Three groups, one shared 220 ms window, one shared ease-out — the curve already
in `ChromeMotion`:

1. **The object is continuous and must not move.** The Construction Body and the
   sculpt mesh occupy the same place in the same camera. Nothing about the
   viewport animates: no dissolve, no scale, no camera move. The object is the
   thing that proves the two modes are one object, so it is the thing that must
   not flinch. *(This also respects the hard rule that a chrome transition never
   touches the viewport or its surface.)*
2. **Tools that are leaving fade and fall back; tools that are arriving fade and
   come forward.** The history capsule and the `+` exit on the 150 ms exit
   duration, alpha only. The brush column enters on the 190 ms anchored duration,
   growing from the leading edge it lives on — not sliding in from off-screen,
   which would claim it came from somewhere it does not live. The Tool Rail is the
   one control present in both modes: it should **not** be destroyed and rebuilt.
   Its two entries and its four entries share a container; the container's height
   change is the one place a bounds animation is worth its cost, because the rail
   is the anchor the user's eye is already on.
3. **The transition button is the hinge, and it is where the eye starts.** It is
   the control the user just pressed, it stays in place, and only its label and
   fill change. That is a 120 ms crossfade on the label — the one place the
   skill's blur suggestion earns its keep, because two words of different lengths
   swapping in a fixed-width pill is exactly the "two distinct objects during a
   crossfade" artefact blur exists to bridge.

Direction carries meaning: **Start Sculpting and Resume Sculpt** bring tools
forward, **Back to Construction** takes them away and brings the exact-value
controls back. The same 220 ms window, mirrored. Nothing else about the two
directions should differ.

### Resume Sculpt is not Start Sculpting

Today they are the same instant cut. They are not the same act: one creates the
sculpt mesh, the other returns to one that already exists. If any motion
distinguishes them, it should be that **Start Sculpting** is the one that
explains — it may take the full 220 ms — and **Resume Sculpt** is a return, which
should be quicker (~150 ms) because the user has been here before. That is the
skill's asymmetry principle applied to a pair of transitions rather than to the
two halves of one.

---

## Reduced-motion review

Verified at runtime, not read from source. With
`animator_duration_scale = 0` ([`M05`](M05-reduced-motion.txt)):

| interaction | frames | result |
| --- | --- | --- |
| Add Primitive open | 3 identical | landed instantly |
| Add Primitive close | 3 identical | landed instantly |
| Hide UI | 3 identical | landed instantly |
| Show UI | 3 identical | landed instantly |

No transient scale, no half-faded frame, no animator posted. The contract in
`ChromeMotion.duration` — *zero means do not animate, not animate instantly* —
holds in practice, and the reasoning behind it (a zero-duration animator still
posts a frame and ends asynchronously) is correct.

The native side has its own branch and it is better than the Java side's. The
selection acknowledgement pulse (`forgeshape_selection_pulse.h`) decays a body's
tint from 0.55 to 0.20 alpha over 220 ms on a smoothstep, and under reduced motion
it **lands on the resting value, not the peak** — so a user who has asked for no
animation can still see what is selected, immediately, without the tint becoming
permanently heavy. That is exactly the skill's rule: reduced motion is fewer and
gentler animations, not a broken interface.

**One disagreement, stated fairly.** The skill says reduced motion should keep
opacity and colour transitions that aid comprehension and remove *movement*.
ForgeShape removes both. For the anchored surfaces, the movement is the 0.96 scale
and the comprehension aid is the alpha — so the skill's position would be to keep
a 190 ms fade and drop the scale. The counter-argument, which the product states
in its own comments, is that Android's "Remove animations" is a blunter and more
explicit request than the web's `prefers-reduced-motion`, and a user who sets it
expects nothing to move or fade. I find the product's position defensible and the
skill's position slightly better, and I would only change it if the alpha-only
variant were verified with someone who actually uses the setting. Filed as
**E-09, P3**, deliberately low.

---

## Layout jumps: eliminate structurally, or animate?

The instruction not to animate a bad anchor is the right one, and it decides most
of this table.

| jump | measured | eliminate or animate | why |
| --- | --- | --- | --- |
| rail column on Transform select (−138.7 dp) | M08 | **eliminate** | Caused by `Gravity.CENTER_VERTICAL` on the floating cluster: any height change moves every child by half the delta. Animating a 139 dp teleport would make a bad anchor *legible*, not correct. The expanded layout already uses `Gravity.TOP` and shows zero vertical movement. |
| rail column on Move→Scale (+56.4 dp) | M08 | **eliminate** | Same cause; the space capsule is removed and the centre re-splits the difference. |
| rail column on precision open (−120.4 dp) | M08 | **eliminate** | Same cause. |
| rail 312.4 dp horizontal, landscape | M09 | **animate, after eliminating what is avoidable** | This one is *justified*: the docked panel claims the right column and the rail must leave. It is a real spatial event, one object, one direction. It deserves the 190 ms it is currently denied — but only once the vertical jumps above are gone, so a single transition is not carrying two unrelated displacements. |
| bottom row on precision open (−260.2 dp) | M08 | **animate** | Also justified — a surface may not cover a live control. Two capsules rising together, one direction, one cause. 190 ms, same curve, same timeline as the sheet. |
| precision toggle compressing 126→97 px | M08 | **eliminate** | A control losing 23% of its height is not motion, it is a layout failure. Nothing about it should be animated; it should not happen. |
| active pill between mode segments (48 dp) | source | **animate** | One object, short distance, high frequency but *desirable*: this is the movement that says the two segments are one control. 150 ms. |
| Construction↔Sculpt inventory change | M08 | **animate** | See the continuity model. |
| appearance repaint | M03 | **animate (crossfade)** | Nothing moves; only colour changes. A crossfade is the whole fix. |

---

## Findings matrix

| ID | sev | artifact | source owner | Emil principle | current behavior | recommended behavior |
| --- | --- | --- | --- | --- | --- | --- |
| **E-01** | **P1** | M01 (`to_sculpt`, `to_constr`, `to_resume`: 6 identical frames each), M08 | `EditorWorkspaceView` mode-swap path | Purpose = explanation; occasional actions get standard animation; elements appearing without transition feel broken | The largest state change in the product — rail +131.8 dp taller, a 116×257 dp brush column created from nothing, history capsule destroyed, object representation replaced — happens in one frame | One 220 ms window, one ease-out; object never moves; leaving tools exit at 150 ms alpha, arriving tools grow at 190 ms from the edge they live on; rail container animates its height; transition button crossfades its label. Resume ~150 ms |
| **E-02** | **P1** | M02-frame1, M09 (frame1 rail at `68,65,61` while panel still `50,47,45`), M08 | `EditorWorkspaceView.java:1329` (`Gravity.CENTER_VERTICAL`) + no animator on chrome relayout | Elements that are physically related must move together; unrelated controls must not jump | The surface animates for 190 ms while the rail (−120.4 dp), mode capsule, space capsule and bottom row (−260.2 dp) teleport in frame 1 | Eliminate the centre-anchor jumps structurally; put the *justified* movements (bottom row, landscape rail) on the same 190 ms timeline and curve as the surface |
| **E-03** | **P2** | M02-frame1..5 (title row 1715→1704 px, left edge fixed at 21 px) | `AnchoredSurfaceView.applyAnchorPivot` + `EditorWorkspaceView.applyPrecisionOpen` | Popovers scale from their trigger, not an arbitrary corner; only modals stay centred | The precision sheet's pivot is its own bottom-left; the toggle that opened it is above its top-right — the growth travels away from the trigger | Pivot on the corner nearest the invoker: `pivotX = width`, `pivotY = 0` for the bottom-sheet placement. The mechanism already exists — `anchoredToTrailingEdge()` — and only the Display popover uses it |
| **E-04** | **P2** | M01 (`mode_switch`, `space_switch`: 6 identical frames) | `EditorControlStyles.setIconButtonActive` (`setBackgroundResource`) | A state swap between adjacent states should transition, not cut; spatial consistency | The active fill is destroyed at one segment and created at another, 48 dp away | Translate the pill between segments, 150 ms, existing ease-out. The mode and space capsules only |
| **E-05** | **P2** | M01 (whole log: 5 of 18 interactions animate) | `ChromeMotion` call sites — only `AnchoredSurfaceView` and the Hide-UI fade | Cohesion: the motion must cover the experience, not one corner of it | One well-built motion family, applied to the four surfaces that displace nothing | Extend the same two durations and the one curve to the mode pill, the appearance crossfade, the history state swap and the mode transition. No new curve, no new constants |
| **E-06** | **P2** | `grep -rn FadeDuration res/drawable/` → **0**; 14 of 53 drawables carry `state_pressed` | `bg_icon_button.xml`, `bg_rail_entry.xml`, and 12 more | Buttons must feel responsive to press; release should be snappy but not abrupt | Press feedback exists and is a hard colour swap in both directions | Keep the press instant; add `android:exitFadeDuration="120"` so the release settles rather than snapping. `enterFadeDuration` stays 0 |
| **E-07** | **P2** | M01 (`appearance`: 8 identical frames), M03 | `ForgeShapeActivity.requestTheme` → `recreate()` | A crossfade bridges two states that timing alone cannot; blur masks an imperfect one | Every colour in the product is replaced in one cut, through an Activity recreate | Crossfade the window content ~200 ms. Rare action, whole-screen change, nothing moves — the clearest crossfade case in the product |
| **E-08** | **P3** | M07 (67,70,72 → 64,67,69 → 62,66,67 → 61,64,66) | `AnchoredSurfaceView.animateOut` | Springs retain velocity when interrupted; transitions retarget | A reversal is *position*-continuous (verified, no snap) but takes the full 150 ms however far the entry got, so an interruption at 10% opacity crawls | Scale the exit duration by the fraction actually travelled, or read the current alpha and shorten proportionally. One line; imperceptible at 1× but correct |
| **E-09** | **P3** | M05 (all transitions: identical frames at scale 0) | `ChromeMotion.duration` | Reduced motion is fewer and gentler animations, not zero; keep opacity, drop movement | Zero duration removes the scale *and* the alpha | Consider keeping a 190 ms alpha-only fade and dropping only the 0.96 scale. Deliberately low severity — the product's counter-argument is good and this needs a real user of the setting to settle |
| **E-10** | **P3** | M01 (`space_switch`: 6 identical frames) | gizmo geometry, `forgeshape_gizmo.*` | Spatial consistency; a rotation shown as a cut cannot be read | World↔Local reorients the same three axes in one frame, so the user cannot see which way Local points | Interpolate the orientation under 150 ms, ease-out, only when the two orientations differ. The one place a spring is defensible |
| **E-11** | **P3** | M01 (`undo_press`, `redo_press`) | `EditorWorkspaceView` history wiring | State indication is a valid animation purpose | Undo/Redo alpha states swap instantly and the scene cuts, with no acknowledgement of what changed | 120 ms alpha crossfade on the two buttons; the missing verdict text is a chrome question, not a motion one |
| **E-12** | **P2** | M04 (palette open at `[155,1412][859,2158]` → `NexusLauncherActivity`) | `ForgeShapeActivity` — no Back branch | What should interrupt or cancel; handle edge cases invisibly | Back with a surface open exits the app; the surface's own 150 ms exit never runs | Back closes the topmost surface and plays its existing exit. The motion is already written; nothing calls it |

---

## Required Before/After review table

The skill mandates this format for any UI-code review. Android equivalents, one
row per issue.

| Before | After | Why |
| --- | --- | --- |
| Chrome relayout on surface open: no animator at all | Same 190 ms / `ChromeMotion.anchoredEase()` timeline as the surface, for the *justified* movements only | Related elements must move together; a 190 ms panel beside a 0 ms rail is one event on two timelines |
| `rail.gravity = Gravity.CENTER_VERTICAL` (floating) | `Gravity.TOP`, as the docked branch already uses | Do not animate a bad anchor — remove the 138.7 / 56.4 / 120.4 dp jumps instead |
| `setPivotX(0); setPivotY(getHeight())` for the bottom-sheet precision surface | `setPivotX(getWidth()); setPivotY(0)` — the toggle's corner | A popover scales from its trigger; this one grows from the corner farthest from it |
| `setBackgroundResource(active)` on the newly selected mode segment | Translate one pill 48 dp, 150 ms, existing ease-out | A fill teleporting between adjacent segments is a state cut where a state *move* is available |
| `<selector>` with no fade attributes | `android:exitFadeDuration="120"` | Press must stay instant; release should settle, not snap |
| `recreate()` on appearance change, no transition | ~200 ms crossfade of the window content | Nothing moves and everything recolours: the textbook crossfade |
| Construction↔Sculpt swapped in one frame | One 220 ms window: object still, leaving tools 150 ms alpha out, arriving tools 190 ms growth, rail height animated, label crossfaded | The rarest and largest change in the product, with the strongest explanatory purpose, currently gets the least motion |
| `animateOut(ANCHORED_EXIT_MS)` regardless of entry progress | Scale the exit by the fraction travelled | An interruption at 10% opacity should not take the full exit duration |
| Back with a surface open → `finish()` | Close the topmost surface, play its 150 ms exit, consume the event | The dismissal motion exists; the most common dismissal gesture never reaches it |
| World↔Local reorients the gizmo in one frame | <150 ms orientation interpolation when the orientations differ | A rotation shown as a cut is unreadable — the one gizmo case that needs motion |

---

## Five to ten highest-value corrections

Correctness and continuity first, delight last — as instructed, not all deferred
to "polish".

1. **[Correctness] Remove the centre-anchor jumps.** `Gravity.TOP` on the
   floating rail cluster. Kills 138.7 dp, 56.4 dp and 120.4 dp of teleport at
   once, and is the prerequisite for every other motion fix — you cannot animate
   a column whose resting address depends on its own height.
2. **[Continuity] Put the justified movements on the surface's timeline.** The
   bottom row's 260.2 dp rise and the landscape rail's 312.4 dp slide are real
   spatial events caused by the surface opening. Same 190 ms, same curve, same
   start instant. This is the fix that makes M02-frame1 stop being embarrassing.
3. **[Continuity] Give the precision surface the right growth origin.** Pivot on
   the toggle's corner. One line; the mechanism already exists for the Display
   popover.
4. **[Continuity] Let Back close a surface, and let the surface's own exit run.**
   The 150 ms exit is already written, tested and correct. Nothing calls it on the
   gesture Android users make most.
5. **[Correctness] Build the Construction↔Sculpt transition.** The 220 ms model
   above. This is the one place where adding motion changes what the user
   *understands*, not just how the app feels.
6. **[Continuity] Slide the active pill between mode and space segments.** 150 ms,
   existing curve. Seen constantly, moves 48 dp, and turns three buttons into one
   control.
7. **[Correctness] Crossfade the appearance switch.** ~200 ms. Rare, total, and
   currently the most violent frame in the product.
8. **[Polish] Add `exitFadeDuration="120"` to the pressed-state selectors.**
   Fourteen drawables, one attribute, no new mechanism.
9. **[Polish] Proportional exit duration on an interrupted growth.** One line in
   `animateOut`.
10. **[Delight] A ~120 ms settle on the gizmo handle highlight at drag end.** The
    only item on this list that is optional, and it is marked so.

---

## Rejected motion ideas — what must stay instant or static

Each of these was considered against the skill's frequency table and rejected.

- **E-R1. Move/Rotate/Scale, World/Local and sculpt-tool *selection* itself.** A
  modeling tool switches these tens to hundreds of times a session. The skill's
  table is unambiguous: remove or drastically reduce. The *pill* may move (E-04);
  the mode change must not wait for anything. **Reject.**
- **E-R2. Morphing the gizmo geometry between Move, Rotate and Scale.** Tempting
  and wrong: it is decoration on a high-frequency action, and mid-morph the
  handles would mean nothing while remaining grabbable. **Reject.**
- **E-R3. Animating the viewport, the camera or the object on any chrome
  transition.** Prohibited by the product's own architecture, and right on the
  merits: the object staying still is what proves the two modes are one object.
  **Reject.**
- **E-R4. Staggering the six Add Primitive tiles.** A fixed set of six, seen every
  time a body is created. Stagger is decorative and would delay the sixth tile.
  **Reject.**
- **E-R5. Bounce or overshoot anywhere.** This is a precision instrument; the
  skill says keep bounce for drag-to-dismiss and playful contexts. The native
  selection pulse explicitly forbids overshoot and is right to. **Reject.**
- **E-R6. Animating the Undo/Redo *scene* change.** Interpolating a body into or
  out of existence would imply geometry is in an intermediate state when it is
  not. The buttons may crossfade; the model must cut. **Reject.**
- **E-R7. App-owned rotation or window-size-class animation.** The system owns
  those transitions, and adding a second one on top would fight it — and would
  touch the viewport, which is prohibited. **Reject.**
- **E-R8. A loading or progress animation on Start Sculpting.** The sculpt mesh is
  created fast enough that six probe frames never caught an intermediate state.
  Showing progress that does not exist is the worst kind of decoration.
  **Reject.**
- **E-R9. Springs for the anchored surfaces.** They open and close from a settled
  state, the distance is fixed and small, and the existing cancel-first reversal
  is already position-continuous. A spring would add a dependency-free physics
  loop for no gain. **Reject** — springs stay proposed for E-10 only.

---

## What this audit found that the Impeccable pass did not

Recorded so the two can be compared without inferring the difference.

- The **native selection pulse** (`forgeshape_selection_pulse.h/.cpp`): 0.55→0.20
  alpha over 220 ms on a smoothstep, with a reduced-motion branch landing on the
  resting value. A second, well-judged animation the product owns.
- The **growth origin defect** on the precision surface — measured, not inferred.
- **Press feedback is colour-only with zero fade duration** across all 14
  state-list drawables.
- **Interruption behaviour verified numerically** rather than assumed from the
  cancel-first comment.
- **Frame-1 proof** that chrome teleports before the surface it belongs to draws
  anything, in both portrait and landscape.
- A **doc/constant inconsistency** worth fixing while nearby: `ChromeMotion`'s
  `ANCHORED_ENTER_MS` javadoc argues "at 140 ms the three anchored surfaces read
  as appearing near their invoker" and then "below about 180 ms the eye cannot
  resolve the origin at all", while the constant is 190. The two sentences
  contradict each other, and the next person tuning this will trust the wrong one.

---

## Next step

**Run UI-AUDIT2C — Remotion motion-prototyping audit.**
