# UI-AUDIT2C — Remotion motion-prototyping audit (independent read-only pass)

**Method: single lens, Remotion only.** This pass does not re-litigate the
Impeccable (UI-AUDIT2A) or Emil Kowalski (UI-AUDIT2B) verdicts and does not
propose product fixes of its own. It answers exactly one question, the one the
Remotion skill is actually competent to answer:

> Which ForgeShape motion questions are still genuinely **open**, and of those,
> which are open in a way that a frame-accurate external prototype would close
> faster or more honestly than writing the Android code and looking at it?

Read-only. **No product source, resource, shader or build file was changed.**
`git diff --stat bd3f7d5 HEAD -- . ':(exclude)artifacts'` is empty: the product
still matches the Stage 020R3 state exactly, and every commit since is evidence.
**No React, no Remotion, no npm package and no web dependency was added to
ForgeShape, and no Remotion project was created anywhere.**

Hard gate: [`SKILL-PROOF.md`](SKILL-PROOF.md). Router at
`C:\Users\damia\.claude\skills\remotion-best-practices\SKILL.md`, declared
version **`4.0.517`**, thirteen files read and hashed, fifteen rules mapped to
the decisions they changed, and the irrelevant two-thirds of the skill marked as
irrelevant rather than repurposed.

---

## What Remotion is, and what this audit refuses to pretend it is

Remotion renders React to video frame by frame. Its skill is a set of rules for
writing deterministic, frame-driven, Studio-editable compositions. It is
**not** a UI critic, it has no opinion about Android, it has never seen a
segmented control, and nothing in the thirteen files read says anything about
what good interface motion feels like.

So the honest use is narrow and it is real: Remotion is a **specification and
review instrument**. It can turn "220 ms, ease-out, three groups" from a
sentence in an audit into an artefact where frame 11 either shows the rail
half-way or it does not — reviewable by the owner, at any speed, without
building the Android code first and without a device.

That framing decides everything below. A candidate earns a prototype only when:

1. the *design question is still open* — the previous audits proposed a value or
   a shape but could not settle it;
2. **more than one element moves**, or motion is the only way to see the answer,
   so a still frame cannot carry it; and
3. building the Android version to find out would cost meaningfully more than
   building the prototype, or would risk committing the product to a shape the
   owner has not seen.

Every candidate that fails any of those three is rejected below, with the reason
named.

---

## Section 1 — Proof-of-skill summary

Full detail in [`SKILL-PROOF.md`](SKILL-PROOF.md). Condensed:

- **Location.** Twelve `remotion-*` skill directories under
  `C:\Users\damia\.claude\skills\`. `remotion-best-practices` is the router and
  bundles a nested copy of the other eleven. Nested `REFERENCE.md` and top-level
  `SKILL.md` copies were **diffed**, not assumed equal: three are byte-identical,
  two differ only in relative link paths.
- **Version.** Every reference file declares `version: 4.0.517`. The seven
  sub-documents under `remotion-markup/` declare **no version** — recorded as
  absent, not inherited.
- **Files read and hashed.** Thirteen, totalling 49 199 bytes / 1 666 lines, all
  with mtime 2026-08-25 19:55:52. Router, markup reference, `timing.md`,
  `transitions.md`, `sequencing.md`, `compositions.md`, `multi-scene-video.md`,
  `3d.md`, `images.md`, interactivity reference, render, studio, create.
- **Rules extracted.** Fifteen, each mapped to the ForgeShape screen or finding
  it changed. The four that changed this audit most:
  - **R13** — `TransitionSeries` is scene-cut grammar for edited video.
    Using `fade()` or `slide()` to depict Construction↔Sculpt would render it as
    a cut between two screens, which *is* the defect (E-01). P2 is therefore one
    continuous scene with grouped layers. This rewrote the accepted spec.
  - **R2/R3** — everything is `useCurrentFrame()` + `interpolate()`, and
    `Easing.bezier` takes CSS `cubic-bezier` arguments. ForgeShape's
    `PathInterpolator(0.23f, 1f, 0.32f, 1f)` maps across exactly, so the
    prototypes play the **product's own curve**, not a Remotion house curve.
  - **R14** — 3D must live in `<ThreeCanvas>` and `useFrame()` is forbidden;
    only `useCurrentFrame()` may drive motion. This is both the feasibility case
    and the cost case for the gizmo prototype (P3).
  - **R11** — `<Still>` needs no fps and no duration. That gave this audit a
    precise definition for its middle verdict: **STATIC MOCKUP ENOUGH means the
    question is answered by a `<Still>`.**
- **Marked irrelevant.** Captions, maps, multimedia/Mediabunny, SaaS/`<Player>`/
  Lambda, voiceover and all audio, effects and light leaks, fonts, GIF/Lottie,
  FFmpeg, Zod parameters, transparent video, upgrade — and `transitions.md` in
  full. Roughly two-thirds of the skill by volume does not apply here, and is
  listed as not applying rather than bent into UI advice.
- **Not claimed.** Nothing was scaffolded, installed, previewed or rendered.
  `node v24.19.0` / `npx 11.17.0` were confirmed present so the feasibility
  statements below are grounded, and that is all.

---

## Section 2 — Inputs this audit read

| input | what it supplied |
| --- | --- |
| [`../uiaudit2b/README.md`](../uiaudit2b/README.md) | the eighteen-interaction motion inventory, findings E-01..E-12, rejected ideas E-R1..E-R9, and the measured claim that 5 of 18 interactions animate |
| [`../uiaudit2b/M08-jump-measurements.txt`](../uiaudit2b/M08-jump-measurements.txt) | every displacement in dp and px — the numbers in the keyframe tables below are these, converted at 2.625 px/dp, not re-derived |
| [`../uiaudit2b/M09-landscape-expanded-motion.txt`](../uiaudit2b/M09-landscape-expanded-motion.txt) | the 820 px landscape rail teleport and the frame-1 pixel samples |
| [`../uiaudit2b/M02-precision-open-frame1..5.png`](../uiaudit2b/) | the five stretched frames of the precision surface opening — the reference asset P1 composites against |
| [`../uiaudit2a/README.md`](../uiaudit2a/README.md) | the resting-state control inventory and the screen catalogue |
| `../uiaudit2a/A01, A02, A07, A19, A21, B01, B04, C02` | the source stills the prototypes would use, at their real capture sizes |
| `app/src/main/java/com/forgeshape/app/ChromeMotion.java` (read only) | the authoritative constants: `ENTER_MS 120`, `EXIT_MS 90`, `ANCHORED_ENTER_MS 190`, `ANCHORED_EXIT_MS 150`, `ANCHORED_START_SCALE 0.96f`, `PathInterpolator(0.23, 1, 0.32, 1)` |

Capture geometry, read from the PNG headers rather than assumed:

| window class | pixels | source |
| --- | --- | --- |
| compact portrait | **1080 x 2400** | `A01-construction-resting.png`, `M02-precision-open-frame1.png` |
| short landscape | **2400 x 1080** | `M09-landscape-precision-frame1.png` |
| expanded | **1600 x 2560** | `C02-expanded-construction.png` |

### The one engineering decision that shapes every spec: fps = 100

ForgeShape's durations are 190, 150, 220, 120, 90 and 260 ms. At 60 fps none of
them is an integer number of frames (190 ms = 11.4 frames), so a prototype at 60
fps reviews a *rounded* timing and then the owner approves a number the product
does not have. At **100 fps, one frame is exactly 10 ms** and every constant
lands on an integer:

| product duration | frames @ 100 fps |
| --- | --- |
| 90 ms (`EXIT_MS`) | 9 |
| 120 ms (`ENTER_MS`) | 12 |
| 150 ms (`ANCHORED_EXIT_MS`) | 15 |
| 190 ms (`ANCHORED_ENTER_MS`) | 19 |
| 220 ms (proposed E-01 window) | 22 |
| 260 ms (candidate long variant) | 26 |

Every frame number below is a real 10 ms tick. The trade is that 100 fps is not
a device refresh rate, so these assets review **timing and choreography, never
frame pacing** — stated again in the limits section. The skill's `posterize`
option was considered and rejected for exactly this reason (proof file, R6).

---

## Section 3 — Candidate classification, all thirteen

`NO PROTOTYPE` — the question is closed, or motion is correctly absent, or a
prototype would answer nothing the app already shows.
`STATIC MOCKUP ENOUGH` — a `<Still>` (or a pair) plus a stated duration settles
it; no timeline needed.
`REMOTION PROTOTYPE WORTHWHILE` — multi-element choreography or a motion-only
question, still open, cheaper to see than to build.

| # | candidate | open question after 2A/2B | elements in motion | verdict | rationale |
| --- | --- | --- | --- | --- | --- |
| 1 | **Add Primitive anchored open/close** | none | 1 (palette only, zero other bounds change) | **NO PROTOTYPE** | 2B rates it PASS on measured evidence and calls it the model the rest of the product should copy. Growth origin verified correct, 190/150 ratio verified correct, reversal verified position-continuous. Nothing is open. Prototyping a solved interaction produces a video of the status quo. |
| 2 | **Exact Transform surface (open/close)** | *Do the four displaced chrome groups share the sheet's 19-frame timeline exactly, lead it, or need longer for their much greater distance — and does the corrected top-right pivot actually read as "grew from the toggle"?* | **5** — sheet + Tool Rail + mode capsule + space capsule + bottom row, over displacements of 316 px, 316 px, 316 px, 683 px | **REMOTION PROTOTYPE WORTHWHILE — P1** | The single densest choreography question in the product, and the one where the audits proposed a *principle* ("same timeline, same curve") without being able to test whether one shared duration survives a 683 px travel next to a 662 px-tall sheet's fade. Five simultaneous timelines cannot be judged from a still or from prose. |
| 3 | **Move ↔ Rotate ↔ Scale** | *the pill slide only* — 2B's E-04, one object, 48 dp, 150 ms | 1 (the active pill) | **STATIC MOCKUP ENOUGH** | The mode change itself must stay instant (E-R1, and this audit agrees — a modeling tool switches modes hundreds of times a session). What remains is one small object translating a fixed short distance on the product's existing curve. A two-frame `<Still>` pair plus the number settles the shape; the feel is a one-line Android change that can be judged on device faster than a composition can be written. The column shift that accompanies Move→Scale (+148 px) is a *layout* defect to be removed structurally, and animating it in a prototype would make a bad anchor look legible — the exact error the prior audit warns against. |
| 4 | **World ↔ Local** | *Does a sub-150 ms orientation interpolation make the Local axis direction readable, without implying the body itself rotated?* | 1 rigid object, but in **3D**, and the change is only visible as motion | **REMOTION PROTOTYPE WORTHWHILE — P3** (accepted third, first to drop) | This is the one question in the product that a static frame structurally cannot answer: two stills of a gizmo before and after show two orientations and tell you nothing about whether the interpolation between them is readable or alarming. `@remotion/three` makes it buildable. It is ranked last because it costs a second package and a hand-rebuilt gizmo, and because a wrong prototype here is worse than none — a Three.js gizmo is not ForgeShape's gizmo. |
| 5 | **Gizmo visual mode transition** (Move↔Rotate↔Scale geometry) | none | 3 arrows + 3 plane quads → 3 rings | **NO PROTOTYPE** | Already rejected as E-R2 and the rejection is right: morphing handles that mean different things would leave frames where the handles mean nothing while remaining grabbable. A prototype would exist only to confirm that a rejected idea is bad. |
| 6 | **Start Sculpting** | *Does the three-group 220 ms model actually read as "one object, different tools" — and is 22 frames enough for a change this large?* | **6** — Tool Rail (height 373→719 px), brush column (305 x 675 px, from nothing), history capsule (destroyed), `+` (destroyed), Objects capsule (421→305 px), transition-button label | **REMOTION PROTOTYPE WORTHWHILE — P2 (highest value)** | The largest state change in the product, currently a single cut, with the strongest explanatory purpose and the most expensive Android implementation. This is where seeing before building has the highest ratio: the proposed model has three groups on two durations with one deliberately-static viewport, and whether that composition *explains* or merely *shuffles* is not knowable from the description. |
| 7 | **Back to Construction** | same, mirrored | same, reversed | **REMOTION PROTOTYPE WORTHWHILE — folded into P2** | Not a separate prototype. The proposed model is explicitly the same window mirrored, so it is a `direction` prop on the same composition and a second render of the same timeline. Giving it its own prototype would burn one of the three accepted slots on a variant. |
| 8 | **Resume Sculpt** | *should the return be shorter (15 frames) than the first entry (22)?* | same as 6 | **REMOTION PROTOTYPE WORTHWHILE — folded into P2** | Same reasoning. It is a duration variant of P2, and the asymmetry question is answered by rendering P2 twice and watching them back to back — which is precisely what a timing-variant strip is for. |
| 9 | **Contextual trailing-cluster changes** (Objects + history capsule rising 683 px on precision open; history capsule destroyed on entering sculpt) | none independently | 2 capsules | **NO PROTOTYPE — covered** | The 683 px rise *is* one of P1's five layers, and the capsule's destruction *is* one of P2's six. There is no residual question that is not already inside an accepted prototype. Filing it separately would double-count. |
| 10 | **Objects open / dock** | none | 1 (popover), or a window-class change | **NO PROTOTYPE** | The floating popover is a verified PASS on the same mechanism as candidate 1. The docked form is a configuration change: one control is destroyed and a different one created by the system's layout pass, which the app neither owns nor should animate. Nothing open. |
| 11 | **Appearance switch** | *200 ms — is that right?* | **0** — nothing moves; every pixel recolours | **STATIC MOCKUP ENOUGH** | Textbook `<Still>` case, and the skill's own `<Still>` doc is what names it. Two full-screen images and a single crossfade duration: `A14-appearance-light-charcoal.png` and its dark counterpart side by side, plus a three-frame strip at 0 % / 50 % / 100 %, answers everything a video would. The only real unknown is whether Android can crossfade an Activity `recreate()` at all — an implementation question no prototype touches. |
| 12 | **Future Body Dimensions mode** | *nothing is decided; the mode does not exist* | unknown | **NO PROTOTYPE** | It has no design, no controls, no measured layout, and appears nowhere in `PROJECT_STATUS.md`, `ARCHITECTURE.md` or either prior audit — verified by grep, not assumed. A motion prototype for an undesigned mode would invent the layout in order to animate it, and the invented layout would then be mistaken for a decision. **When it exists and has a resting layout, it will most likely be a P1-shaped question** (a mode selector plus displaced chrome), and the P1 composition is the one to extend. |
| 13 | **Future one-sided / symmetric Scale mode** | *nothing is decided* | unknown | **NO PROTOTYPE** | Same absence, verified the same way — a case-insensitive `grep -rn` for "body dimensions", "one-sided" and "symmetric scale" over `PROJECT_STATUS.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `README.md` and all three prior audits returns **no hit for a Scale mode**; the three `one-sided` hits are about mesh sidedness (`renderBothSides`, the Plane, a sculpted body keeping its inside) and are unrelated. It is also, on its merits, a *static* question first: one-sided vs symmetric scaling is about where the anchor sits and what the handle does to the geometry, which a diagram answers better than motion. When specified, it is closer to STATIC MOCKUP ENOUGH than to a Remotion candidate — and it belongs to the gizmo, where the standing rule is that geometry changes are instant. |

**Tally:** 6 NO PROTOTYPE, 2 STATIC MOCKUP ENOUGH, 5 REMOTION PROTOTYPE
WORTHWHILE — collapsing to **3 accepted prototypes**, because Back to
Construction and Resume Sculpt are directions and durations of P2 rather than
separate assets.

Ranked: **P2 (Construction↔Sculpt) > P1 (Exact Transform choreography) >
P3 (World↔Local gizmo)**. If only one is ever built, build P2. If P3 is dropped
for cost, nothing else in the list changes.

---

## Section 4 — Accepted prototype P2: the Construction ↔ Sculpt continuity model

Highest value, and specified first because the other two borrow its structure.

### 4.1 The question it must answer

> **Does the proposed 220 ms three-group model make the user understand that the
> Construction Body and the sculpt mesh are one object with a different toolset —
> or does it merely make the same inventory swap take longer?**

Three sub-questions the render must settle, in this order:

1. **Does the still viewport carry the continuity?** The model's whole claim is
   that the object not flinching is what proves the two modes are one object. If
   the surrounding chrome moving while the object holds reads as *broken* rather
   than as *anchored*, the model is wrong and 2B's E-01 needs a different shape.
2. **Is 22 frames (220 ms) enough for a change this large, or does it need 26?**
   The rail grows 346 px, a 305 x 675 px column arrives, two controls leave and a
   third resizes. 220 ms was reasoned from a frequency table, not measured.
3. **Is the asymmetry real?** Should Resume Sculpt be 15 frames where Start
   Sculpting is 22 — or does a shorter return just read as a different, sloppier
   animation?

### 4.2 Composition, sizes, and frames

Registered as one `<Composition>` per window class, metadata inline (R10), one
scene file per direction (`multi-scene-video.md` structure, without any
`TransitionSeries` — R13).

| composition id | width | height | fps | durationInFrames | reference asset pair |
| --- | --- | --- | --- | --- | --- |
| `SculptEnter-Portrait` | 1080 | 2400 | 100 | **70** (700 ms) | `A01-construction-resting.png` → `A19-sculpt-resting.png` |
| `SculptExit-Portrait` | 1080 | 2400 | 100 | **70** | `A19-sculpt-resting.png` → `A21-back-to-construction.png` |
| `SculptEnter-Landscape` | 2400 | 1080 | 100 | **70** | `B01-landscape-construction.png` → `B02-landscape-sculpt.png` |

**Start frame 0, end frame 69.** Structure shared by all three:

| phase | frames | ms | what is on screen |
| --- | --- | --- | --- |
| idle hold | 0 – 4 | 0 – 50 | the resting start state, untouched — gives the reviewer a baseline and makes the trigger frame unambiguous |
| **trigger** | **5** | **50** | the transition button is pressed; every timeline starts here |
| motion window | 5 – 27 | 50 – 270 | the 22-frame window; individual groups end earlier |
| settle hold | 27 – 69 | 270 – 700 | the final state, held for 430 ms so the reviewer sees where it lands |

### 4.3 Layers

Seven, named with ForgeShape's own vocabulary and semantic ids (R9). Each is a
cropped region of the two reference screenshots, composited — not a redrawn UI.

| layer name | source crop | role | moves? |
| --- | --- | --- | --- |
| `viewport` | full-frame background of both stills, identical region | the Construction Body / sculpt mesh | **never** — this is the control condition |
| `transition button` | toolbar, `Start Sculpting` 121–123 dp / `Back to Construction` 150.1 dp | the hinge; position fixed, label crossfades | label only |
| `tool_rail` | rail container, 373 px tall (2 entries) → 719 px tall (4 entries) | the one control present in both modes | height |
| `brush column` | `[21,927][326,1602]`, 305 x 675 px | arriving tools | enters |
| `history capsule` | bottom trailing capsule | leaving tools | exits |
| `objects capsule` | 421 px wide with `+` → 305 px wide without | resizes, stays put | width, `+` exits |
| `status line` | chrome status row | verdict text swap | opacity only |

### 4.4 Keyframe timeline — Start Sculpting, 22-frame variant

All values are literal, inline, per R7/R8. `translate` two-value strings,
`scale` with `output: 'perceptual-scale'` (R4), 4 keyframes with a 3-easing
array (R5), both extrapolations clamped.

| layer | property | keyframes (frames) | output range | easing array | note |
| --- | --- | --- | --- | --- | --- |
| `viewport` | — | — | — | — | **no interpolate call at all.** Its stillness must be structural, not a 0-range animation someone can later "fix" |
| `history capsule` | `opacity` | `[0, 5, 20, 69]` | `[1, 1, 0, 0]` | `[linear, bezier(0.23,1,0.32,1), linear]` | 15 frames = `ANCHORED_EXIT_MS` |
| `history capsule` | `translate` | `[0, 5, 20, 69]` | `["0px 0px", "0px 0px", "0px 12px", "0px 12px"]` | same | 12 px fall-back, the "leaving tools recede" half of the model |
| `objects capsule` (`+` glyph) | `opacity` | `[0, 5, 20, 69]` | `[1, 1, 0, 0]` | same | the `+` cannot exist in Sculpt — a control that cannot succeed is not drawn |
| `objects capsule` | `translate` | `[0, 5, 27, 69]` | `["0px 0px", "0px 0px", "116px 0px", "116px 0px"]` | `[linear, bezier(0.23,1,0.32,1), linear]` | 421→305 px, expressed as the leading edge moving 116 px |
| `tool_rail` | `height` | `[0, 5, 27, 69]` | `[373, 373, 719, 719]` | same | the one bounds animation the model pays for; the rail must **not** be destroyed and rebuilt |
| `brush column` | `opacity` | `[0, 5, 24, 69]` | `[0, 0, 1, 1]` | same | 19 frames = `ANCHORED_ENTER_MS` |
| `brush column` | `scale` | `[0, 5, 24, 69]` | `[0.96, 0.96, 1, 1]` | same, **`output: 'perceptual-scale'`** | matches `ANCHORED_START_SCALE` exactly |
| `transition button` label (out) | `opacity` | `[0, 5, 17, 69]` | `[1, 1, 0, 0]` | same | 12 frames = `ENTER_MS` |
| `transition button` label (in) | `opacity` | `[0, 5, 17, 69]` | `[0, 0, 1, 1]` | same | the one crossfade in the model |
| `transition button` label (in) | `filter: blur` | `[0, 5, 17, 69]` | `["blur(3px)", "blur(3px)", "blur(0px)", "blur(0px)"]` | same | **variant only** — two words of different lengths swapping in a fixed pill; a comparison render must exist with and without |
| `status line` | `opacity` | `[0, 5, 12, 20, 69]` | `[1, 1, 0, 1, 1]` | 4 easings | text swap; deliberately the fastest thing on screen |

**Transform origins.** `brush column`: `transformOrigin: "0% 50%"` — the leading
edge it lives on. It must **not** slide in from off-screen, which would claim it
came from somewhere it does not live. `tool_rail`: `transformOrigin: "50% 0%"`,
so growth is downward from a fixed top edge — which is also the visual argument
for the pending `Gravity.TOP` correction. `transition button`: `"50% 50%"`,
fixed.

### 4.5 Timing variants to compare

Four renders of the same composition, one `defaultProps` flag each (R10).

| variant | window | brush column enter | leaving tools exit | asks |
| --- | --- | --- | --- | --- |
| **V0 — status quo** | 0 frames | instant | instant | the control. Every review must include the current cut, or the reviewer has nothing to prefer *over* |
| **V1 — proposed** | 22 | 19 | 15 | is the 2B model right as written? |
| **V2 — longer** | 26 | 22 | 18 | does a change this large need 260 ms? |
| **V3 — shorter (Resume)** | 15 | 12 | 9 | is a return legitimately quicker, or just sloppier? |

### 4.6 Easing variants

| variant | curve | why it is in the comparison |
| --- | --- | --- |
| **E-A (default)** | `Easing.bezier(0.23, 1, 0.32, 1)` | ForgeShape's actual `ChromeMotion.anchoredEase()`. The prototype's job is to test the *product's* curve first |
| **E-B** | `Easing.bezier(0.16, 1, 0.3, 1)` | the skill's own example curve — a slightly sharper start; included because `ChromeMotion`'s own javadoc contradicts itself about origin legibility below 180 ms |
| **E-C** | `Easing.spring({ damping: 200 })` | included **only to be rejected on camera.** 2B rejected springs for anchored surfaces (E-R9); rendering one makes the rejection reviewable instead of asserted |

### 4.7 Reduced-motion variant

A `reducedMotion: true` inline prop. Under it, **every** interpolate input range
collapses to `[0, 5, 5, 69]` — the state changes on the trigger frame and never
between. Nothing scales, nothing translates, nothing crossfades.

The verification is mechanical and matches how 2B proved the real product's
behaviour: render frames 6..69 and require **one digest**. If any two differ,
the reduced-motion variant has a transient state and is wrong.

This also renders 2B's one open disagreement (E-09) reviewable rather than
argued: a second reduced-motion render keeping the 19-frame **alpha only** and
dropping the 0.96 scale entirely, side by side with the collapse-to-instant
version. That comparison is the whole content of E-09, and it costs one extra
prop.

### 4.8 Success and failure

**Success** — all four must hold:

- At every frame in 5..27 the viewport crop is **byte-identical** to frame 0's.
  Automatable: `sha256` the cropped region of every rendered frame. One digest,
  or the model is not being tested.
- A reviewer watching V1 once at 1x can say *"the same object, different tools"*.
  A reviewer watching V0 once says *"the screen changed"*.
- No frame exists in which the brush column is at full opacity while the rail is
  still growing, or vice versa by more than ~15 % of progress. This is the
  measurable form of "elements that are physically related move together."
- V3 (15 frames) is preferred for *Resume* and V1 (22) for *Start* by the same
  reviewer, in a blind pairing. If the same variant wins both, the asymmetry
  proposal is unsupported and should be dropped rather than shipped on theory.

**Failure** — any one is decisive:

- The still viewport reads as frozen or broken rather than anchored. Then the
  model is wrong at its root, and E-01 needs a different shape — which is
  exactly the outcome worth discovering **before** writing the Android code.
- 22 frames looks slow on repeat viewing. A modeling session enters sculpting a
  handful of times, but the reviewer will watch this twenty times; if it drags
  on the second viewing it will drag on the fifth session.
- The label crossfade reads as two overlapping words. Then the blur variant is
  required, not optional.
- The reviewer cannot tell V1 from V2. Then the duration question is not
  perceptible and 22 should be kept for the cheaper reason of matching the
  existing constants.

### 4.9 Assets required

| asset | source | preparation |
| --- | --- | --- |
| Construction resting, portrait | `../uiaudit2a/A01-construction-resting.png` | copied to the prototype's `public/`, per R15 — never referenced out of `artifacts/` |
| Sculpt resting, portrait | `../uiaudit2a/A19-sculpt-resting.png` | same |
| Back-to-Construction resting | `../uiaudit2a/A21-back-to-construction.png` | same |
| Landscape pair | `../uiaudit2a/B01-landscape-construction.png`, `B02-landscape-sculpt.png` | same |
| seven layer crops | cut from the above at the bounds in section 4.3 | **must be alpha-cut against the viewport**, since the viewport layer shows through where chrome leaves |
| viewport plate | the region common to both stills | one image, used by both directions |

**One asset gap, stated plainly.** The current screenshots capture *resting*
states only. The chrome crops carry whatever the viewport showed behind them at
capture time, so cutting a clean `brush column` with a transparent background
needs either a matched pair captured against a flat viewport, or manual masking.
This is real work — roughly the largest single cost in P2 — and pretending
otherwise would be the sort of estimate that makes a prototype seem cheaper than
building the feature.

### 4.10 Output for review

| deliverable | how | reviewed for |
| --- | --- | --- |
| `P2-V0..V3-portrait.mp4` | `npx remotion render` per variant | 1x preference between the four |
| `P2-V1-quarter.mp4` | same composition, 25 frames of source per output frame | the frame-by-frame check 2B did with `animator_duration_scale 20`, without a device |
| `P2-V1-strip.png` | 12 `<Still>` renders at frames 5, 7, 9, 11, 13, 15, 17, 19, 21, 24, 27, 40 | the "do related elements move together" test, on one printable sheet |
| `P2-viewport-digests.txt` | `sha256` of the viewport crop, all 70 frames | the still-viewport invariant, mechanically |
| `P2-reduced.mp4` + digests | `reducedMotion: true` | one digest from frame 6 on |

---

## Section 5 — Accepted prototype P1: the Exact Transform choreography

### 5.1 The question it must answer

> **When the precision surface opens, do the four displaced chrome groups belong
> on the sheet's own 19-frame timeline — and does the corrected pivot make the
> sheet read as growing out of the toggle that opened it?**

Two sub-questions:

1. **One duration or two?** The bottom row travels 683 px; the rail travels
   316 px; the sheet only fades and grows 4 %. "Same 190 ms for all" is elegant
   and might be visibly wrong for the longest travel.
2. **Does the pivot fix actually read?** Moving from bottom-left to top-right is
   a one-line change, but "the panel comes from the toggle" is a perceptual
   claim, and the two candidate origins are 1038 x 662 px apart diagonally.

### 5.2 Composition, sizes, frames

| composition id | width | height | fps | durationInFrames | reference |
| --- | --- | --- | --- | --- | --- |
| `PrecisionOpen-Portrait` | 1080 | 2400 | 100 | **60** (600 ms) | `A02-transform-move.png` → `A07-exact-transform-open.png` |
| `PrecisionOpen-Landscape` | 2400 | 1080 | 100 | **60** | `B01-landscape-construction.png` → `B04-landscape-exact-transform.png` |
| `PrecisionClose-Portrait` | 1080 | 2400 | 100 | **60** | reverse, 15-frame window |

| phase | frames | ms |
| --- | --- | --- |
| idle hold | 0 – 4 | 0 – 50 |
| **trigger** (precision toggle pressed) | **5** | 50 |
| motion window | 5 – 24 | 50 – 240 |
| settle hold | 24 – 59 | 240 – 600 |

### 5.3 Layers and keyframes — portrait, V1 (shared timeline)

Displacements converted from the 2B measurements at 2.625 px/dp. These are the
numbers, not estimates.

| layer | property | keyframes | output range | measured source |
| --- | --- | --- | --- | --- |
| `precision surface` | `opacity` | `[0, 5, 24, 59]` | `[0, 0, 1, 1]` | sheet `[21,1654][1059,2316]`, 1038 x 662 px |
| `precision surface` | `scale` | `[0, 5, 24, 59]` | `[0.96, 0.96, 1, 1]`, `output: 'perceptual-scale'` | `ANCHORED_START_SCALE` |
| `tool_rail` | `translate` | `[0, 5, 24, 59]` | `["0px 0px", "0px 0px", "0px -316px", "0px -316px"]` | 591 → 275 px top, −120.4 dp |
| transform mode selector | `translate` | `[0, 5, 24, 59]` | `["0px 0px", "0px 0px", "0px -316px", "0px -316px"]` | −120.4 dp |
| coordinate-space selector | `translate` | `[0, 5, 24, 59]` | `["0px 0px", "0px 0px", "0px -316px", "0px -316px"]` | −120.4 dp |
| `precision toggle` | `translate` | `[0, 5, 24, 59]` | `["0px 0px", "0px 0px", "0px -327px", "0px -327px"]` | −124.6 dp |
| bottom row (Objects + history) | `translate` | `[0, 5, 24, 59]` | `["0px 0px", "0px 0px", "0px -683px", "0px -683px"]` | 2168 → 1485 px, −260.2 dp |

Landscape swaps one line: `tool_rail` translates `["0px 0px", "0px 0px",
"-820px 0px", "-820px 0px"]` — the measured `[2200,225]` → `[1380,225]`, 312.4 dp
horizontal.

Easing: `Easing.bezier(0.23, 1, 0.32, 1)` throughout, 3-easing array
`[linear, bezier, linear]`, both extrapolations clamped.

**Deliberately not modelled:** the precision toggle compressing 126 → 97 px.
2B classifies that as a layout failure to be eliminated, not motion to be
animated, and putting it in the prototype would legitimise it. Its absence is a
specification decision and is recorded as one.

**Transform origins.** The whole pivot question is one property:

| variant | `transformOrigin` | which corner |
| --- | --- | --- |
| **O-cur** | `"0% 100%"` | bottom-left — the current, measured behaviour |
| **O-fix** | `"100% 0%"` | top-right — the toggle's corner |

Everything else is held identical between the two, so any perceived difference
is attributable.

### 5.4 Timing variants

| variant | sheet | chrome | asks |
| --- | --- | --- | --- |
| **V0 — status quo** | 19 | **0** | the control: the measured defect, reproduced faithfully. It should look wrong on camera; if it does not, the finding's severity is overstated |
| **V1 — shared** | 19 | 19, same start frame | is one timeline sufficient? |
| **V2 — distance-scaled** | 19 | 26 for the 683 px bottom row, 19 for the rest | does the longest travel need longer? |
| **V3 — chrome leads** | 19 | 19, starting at frame 2 (20 ms earlier) | does the room appearing *before* the panel read better, or as a stutter? |

Easing variants as P2: E-A default, E-B `(0.16, 1, 0.3, 1)`, E-C spring
(again, included to be rejected visibly).

### 5.5 Reduced-motion variant

Identical construction to P2 section 4.7: input ranges collapse to
`[0, 5, 5, 59]`, and frames 6..59 must produce one digest. The alpha-only E-09
comparison applies here too and is arguably more informative here than in P2,
because the precision surface is the one users open most.

### 5.6 Success and failure

**Success:**

- At every frame, `chrome displacement fraction` and `sheet opacity fraction`
  agree within **5 %**. Directly computable from the rendered frames; this is
  the numeric form of the thing `M02-precision-open-frame1.png` proves is
  currently violated.
- A reviewer shown O-cur and O-fix blind picks O-fix as "grew from the button".
  If the pick is a coin flip, the pivot fix is still correct on principle but
  should not be described as a perceptual improvement.
- V1 is preferred to V0 immediately and without explanation.

**Failure:**

- The reviewer prefers V2 to V1. Then "same timeline for everything" is wrong
  and the correction needs a distance rule — a materially larger Android change
  than the audits assumed, and much better to learn here.
- The four chrome groups moving together at 19 frames read as the whole screen
  sliding. Then the movements are not as *justified* as 2B concluded, and the
  structural fix (`Gravity.TOP`) has to land first and alone.
- V3 is preferred. Then the transition has an internal ordering, and the "one
  event, one timeline" principle needs qualifying.

### 5.7 Assets required

`A02-transform-move.png` (before), `A07-exact-transform-open.png` (after),
`M02-precision-open-frame1..5.png` (the stretched reference — useful as a
side-by-side sanity plate, since it is the only ground truth for what the real
transition looks like mid-flight), `B01`/`B04` for landscape. Same masking cost
as P2: five chrome crops need to be separable from the viewport behind them.

### 5.8 Output for review

`P1-V0..V3-portrait.mp4`; `P1-O-cur-vs-O-fix.mp4` (split-screen, same frame
clock); `P1-V1-strip.png` at frames 5, 8, 11, 14, 17, 20, 24, 40;
`P1-landscape-V1.mp4` (the 820 px case, which is the most visually dramatic and
the easiest to judge); `P1-sync-report.txt` — the per-frame fraction comparison
that turns the 5 % criterion into a number.

---

## Section 6 — Accepted prototype P3: World ↔ Local gizmo reorientation

Accepted third, and explicitly the first to drop. Read section 6.6 before
commissioning it.

### 6.1 The question it must answer

> **Does a sub-150 ms interpolation of the gizmo's orientation from world axes to
> body axes make "which way is Local?" readable — without implying that the body
> itself rotated?**

The second half is the risk. ForgeShape's hardest invariant in this area is that
a coordinate-space change is a change of *instrument*, not of *object*. An
animation that turns the handles can easily be read as turning the body, which
would be worse than the current cut.

### 6.2 Why this one cannot be a `<Still>`

Two stills of a gizmo in world axes and in body axes show two orientations and
say nothing about the path between them, which is the entire question. This is
the only candidate in the thirteen where motion is not a presentational choice
but the subject itself.

### 6.3 Composition and structure

Per R14: `@remotion/three`, everything inside `<ThreeCanvas width height>` with
explicit lighting, **no `useFrame()`**, all motion from `useCurrentFrame()`, any
inner `<Sequence>` at `layout="none"`.

| composition id | width | height | fps | durationInFrames |
| --- | --- | --- | --- | --- |
| `GizmoSpaceSwitch` | 1080 | 1080 | 100 | **40** (400 ms) |

Square, not device-shaped: the question is about the instrument, and screen
chrome would only invite the reviewer to critique something else.

| phase | frames | ms |
| --- | --- | --- |
| idle hold, World axes | 0 – 4 | 0 – 50 |
| **switch pressed** | **5** | 50 |
| orientation interpolation | 5 – 17 | 50 – 170 |
| settle hold, Local axes | 17 – 39 | 170 – 400 |

### 6.4 Layers

| layer | content | moves? |
| --- | --- | --- |
| `body` | a box at a **mixed** orientation — approximately `(25, 40, 15)` degrees, so world and local axes are unmistakably different and no two of the three Euler components are zero | **never.** Its stillness is the invariant under test |
| `gizmo axes` | three axis handles, one rigid object | rotates, world basis → body basis |
| `axis labels` | X / Y / Z tags | ride the handles |
| `space capsule` | a flat World/Local indicator | fill swaps instantly at frame 5 |
| `ground plane` | faint world grid | **never** — the fixed reference that lets the eye judge whether the *body* moved |

The mixed orientation matters. At `(0, 0, 0)` world and local coincide and the
prototype would show nothing; a single-axis rotation would understate it. This
mirrors the repository's own rule that a rotation test asserts an orientation
rather than one Euler field.

### 6.5 Variants, and the one that is not optional

| axis | variants |
| --- | --- |
| duration | **0** (the cut, control) / 8 (80 ms) / **12 (120 ms, proposed)** / 15 (150 ms, the stated upper bound) |
| easing | `Easing.bezier(0.23, 1, 0.32, 1)` (product curve) vs `Easing.spring({ damping: 200 })` — this is the one place 2B calls a spring defensible, so the comparison is the point |
| interpolation | **quaternion slerp** vs **per-component Euler lerp** |

**The interpolation variant is mandatory.** Euler-lerping between two
orientations produces a path that is not a rotation about any single axis, and
it will look subtly wrong in a way a reviewer feels but cannot name. Rendering
both is how a prototype earns its keep here — and it is the same distinction the
product already enforces natively, where a rotation is composed as a matrix and
only decoded to Euler at the boundary.

**Transform origin:** the body's Construction placement origin, which is the
gizmo's pivot. Not a mesh AABB centre, not a screen centroid. Getting this wrong
in the prototype would test a gizmo ForgeShape does not have.

**Ranges:** orientation only. No opacity change, no scale change, no
translation. Anything else moving would contaminate the answer.

### 6.6 Reduced motion, success, failure — and the honest caveat

**Reduced motion:** duration 0 — the cut, which is exactly today's behaviour. The
reduced-motion variant of this prototype *is* the control variant, which is a
small pleasing property: the fix degrades to the status quo for free.

**Success:** a reviewer who has not seen ForgeShape watches the 12-frame slerp
variant once and can point at which way Local Y points — and, asked whether the
object moved, says no. Both halves are required.

**Failure — any of these kills the idea, not the prototype:**

- The reviewer says the object turned. Then the animation is actively harmful
  and the current cut is correct.
- The reviewer cannot tell 0 from 12 frames. Then E-10 does not survive contact
  and should be closed as "no change".
- The spring overshoot reads as the axes wobbling. Then the product's standing
  no-overshoot position holds even here, and the one exception 2B allowed is
  withdrawn.

**The caveat that ranks this third.** A Three.js gizmo is *not* ForgeShape's
gizmo. Its handle proportions, its camera-derived scale, its depth cueing and
its shading all come from the native Vulkan renderer, and a prototype
approximates every one of them. A verdict from P3 is therefore a verdict about
**orientation interpolation in the abstract**, not about ForgeShape's gizmo —
and it must be reported that way or it will be over-trusted. Compare with P1 and
P2, which composite the product's own pixels and carry no such gap. If P3 is
commissioned, its output must be labelled with this limitation on the asset
itself.

**Assets:** none from ForgeShape, which is precisely the problem. Reference
stills `A15-gizmo-move-world.png`, `A16-gizmo-rotate-world.png`,
`A17-gizmo-rotate-local.png`, `A18-gizmo-scale-local.png` and the 3x crops
`A15b`/`A16b`/`A18b` are needed to match proportions and colour — as *targets*
to build against, not as composited layers.

**Output:** `P3-slerp-{0,8,12,15}.mp4`; `P3-slerp-vs-euler.mp4` split-screen at
the same frame clock; `P3-spring-vs-bezier.mp4`; `P3-strip.png` at frames 5, 7,
9, 11, 13, 15, 17; and a `body`-crop digest list proving the body never moved,
identical in construction to P2's viewport check.

---

## Section 7 — Consolidated frame / timeline tables

Every accepted prototype on one clock, so the three can be reviewed as a family.
`fps = 100`, one frame = 10 ms, trigger at frame 5 in all three.

| prototype | duration | trigger | window ends | settle to | product constants it plays |
| --- | --- | --- | --- | --- | --- |
| **P1** Exact Transform | 60 f / 600 ms | 5 | 24 (V1) / 31 (V2 long) | 59 | `ANCHORED_ENTER_MS 190` = 19 f, `ANCHORED_EXIT_MS 150` = 15 f |
| **P2** Construction↔Sculpt | 70 f / 700 ms | 5 | 27 (V1) / 31 (V2) / 20 (V3) | 69 | proposed 220 ms = 22 f, plus 19 f and 15 f and `ENTER_MS 120` = 12 f |
| **P3** gizmo space switch | 40 f / 400 ms | 5 | 17 (12 f) | 39 | none — proposes 120 ms against the existing sub-150 ms bound |

Per-group ends within each window:

| prototype | group | starts | ends | frames | ms |
| --- | --- | --- | --- | --- | --- |
| P1 | precision surface (fade + 0.96 scale) | 5 | 24 | 19 | 190 |
| P1 | rail / mode / space capsules | 5 | 24 | 19 | 190 |
| P1 | precision toggle | 5 | 24 | 19 | 190 |
| P1 | bottom row (683 px) | 5 | 24 (V1) / 31 (V2) | 19 / 26 | 190 / 260 |
| P2 | leaving tools (history capsule, `+`) | 5 | 20 | 15 | 150 |
| P2 | arriving tools (brush column) | 5 | 24 | 19 | 190 |
| P2 | Tool Rail height 373→719 px | 5 | 27 | 22 | 220 |
| P2 | transition-button label crossfade | 5 | 17 | 12 | 120 |
| P2 | status line text swap | 5 | 20 | 15 | 150 |
| P3 | gizmo orientation | 5 | 17 | 12 | 120 |
| P3 | space capsule fill | 5 | 5 | 0 | instant, by design |

**Frame-strip sampling** — the still frames that go on the printed review sheet:

| prototype | frames sampled | why these |
| --- | --- | --- |
| P1 | 5, 8, 11, 14, 17, 20, 24, 40 | one before the trigger's effect is visible, five inside the window, the landing frame, one settled |
| P2 | 5, 7, 9, 11, 13, 15, 17, 19, 21, 24, 27, 40 | dense, because three groups end on three different frames (17, 20, 24, 27) and the sheet must show each landing |
| P3 | 5, 7, 9, 11, 13, 15, 17 | every second frame — a 12-frame rotation has no room for a coarser sample |

---

## Section 8 — Consolidated reduced-motion variants

One rule across all three, and it is the product's own: **zero duration means do
not animate, not animate instantly.**

| prototype | reduced-motion construction | mechanical check |
| --- | --- | --- |
| P1 | every input range collapses to `[0, 5, 5, 59]`; no scale, no translate, no crossfade | frames 6..59 must yield **one** SHA-256 |
| P2 | every input range collapses to `[0, 5, 5, 69]`; the viewport was never animated, so it is unaffected — which is itself worth showing | frames 6..69 must yield **one** SHA-256 |
| P3 | duration variant 0 — identical to the control | frames 6..39 must yield **one** SHA-256 |

**The E-09 comparison, made reviewable.** 2B recorded one honest disagreement:
the skill it used says reduced motion should keep comprehension-aiding opacity
and remove *movement*, while ForgeShape removes both. That is a two-render
question and it belongs here:

| render | keeps | drops |
| --- | --- | --- |
| `P1-reduced-collapse.mp4` | nothing | scale and alpha — today's behaviour |
| `P1-reduced-alpha-only.mp4` | the 19-frame alpha fade | the 0.96 scale only |

Shown side by side, this is the *entire* content of E-09, and it costs one
boolean prop. It is the cheapest thing in this audit and one of the most
decision-relevant — with one caveat that must travel with the asset: this
settles what the two options *look like*, not what a person who actually relies
on "Remove animations" wants. Only that person can settle that.

---

## Section 9 — Asset and data requirements, consolidated

| requirement | P1 | P2 | P3 | notes |
| --- | --- | --- | --- | --- |
| existing ForgeShape stills | 6 | 5 | 7 (as targets only) | all already committed under `artifacts/uiaudit2a/` and `uiaudit2b/` |
| **new captures needed** | possibly 2 | possibly 2 | 0 | for clean alpha-cut chrome — see below |
| layer crops with alpha | 5 | 7 | 0 | the single largest cost in both P1 and P2 |
| measured displacements | ✔ all present | ✔ all present | n/a | from `M08` / `M09`; nothing needed re-measuring |
| product constants | ✔ | ✔ | n/a | from `ChromeMotion.java`, read-only |
| extra npm packages | none beyond the Remotion scaffold | none | **`@remotion/three` + three + @react-three/fiber** | via `npx remotion add`, per R15 |
| device time | none | none | none | all three are desktop-only work |

**The capture gap, stated once and plainly.** Every existing screenshot is a
*resting* state. To composite chrome moving over a still viewport, each chrome
group must be separable from what was behind it. Two ways: capture a matched
pair against a deliberately flat viewport (one short, safe, `-Serial`-scoped
evidence run on `ForgeShape_Stage006`), or mask by hand. The first is cleaner and
faster and would be the first task of any prototype build. **Neither was done in
this audit**, because this audit was read-only and specified nothing it was asked
to build.

**Where a prototype project would live.** Not in this repository. ForgeShape's
hard rules forbid third-party runtime libraries in the product and forbid
committing generated build output, and a `node_modules` tree beside the Android
app would violate the spirit of both. A Remotion project belongs in a scratch or
sibling directory, and its **rendered outputs only** — the MP4s, the strips, the
digest lists — would ever come back into `artifacts/`. Screenshots would be
**copied** into its `public/` per R15, never referenced across.

---

## Section 10 — The exact questions each accepted prototype resolves

Stated as questions with answers, so a reviewer knows what they are being asked
to decide and what each answer commits the project to.

| # | question | prototype | if the answer is A | if the answer is B |
| --- | --- | --- | --- | --- |
| Q1 | Does a still viewport carry the Construction↔Sculpt continuity? | P2 | build E-01 as specified | E-01 needs a different shape entirely — found for the cost of a render instead of a stage |
| Q2 | Is 220 ms the right window for that change? | P2 V1 vs V2 | keep 22 f and reuse existing constants | adopt 260 ms and add one constant |
| Q3 | Should Resume Sculpt be shorter than Start Sculpting? | P2 V1 vs V3 | ship the asymmetry | drop it; one duration, less code, no loss |
| Q4 | Does the label crossfade need blur? | P2, blur on/off | add blur | one less mechanism |
| Q5 | Do the four displaced chrome groups belong on the sheet's 19-frame timeline? | P1 V1 vs V2 vs V3 | one timeline, one curve, simplest possible fix | a distance rule is needed — materially more Android work, better known now |
| Q6 | Does the top-right pivot read as "grew from the toggle"? | P1 O-cur vs O-fix | ship it as a perceptual improvement | ship it as a correctness fix and claim nothing more |
| Q7 | Is the status-quo teleport as bad as measured? | P1 V0 | severity confirmed | severity was overstated and priority drops |
| Q8 | Does gizmo orientation interpolation read as the instrument turning, or the body? | P3 | build E-10 | close E-10; the cut was right |
| Q9 | Slerp or Euler lerp? | P3 | slerp, matching the product's own rotation discipline | — (there is no defensible B here; the variant exists to make the difference *visible*, not to leave it open) |
| Q10 | Alpha-only or collapse-to-instant under reduced motion? | P1/P2 reduced pair | reconsider `ChromeMotion.duration`'s contract | keep it, now with evidence rather than argument |

---

## Section 11 — Why the rejected candidates do not need Remotion

Grouped by the reason, because the reasons are few and each covers several.

**Because the question is already closed by measured evidence** — Add Primitive
open/close, Objects popover open/close. 2B verified growth origin, the 190/150
ratio, position-continuous reversal and instant reduced-motion landing, on the
running app. A prototype would render the status quo and confirm what a
screenshot already confirms. Building a video of a passing test is not review, it
is decoration — and the skill that would be used to build it says nothing that
would change the verdict.

**Because the correct answer is "no motion", and prototyping it means building
the thing you rejected** — the gizmo Move/Rotate/Scale geometry swap (E-R2), and
mode / space / sculpt-tool *selection* itself (E-R1). The argument against is
about frequency and about handles that mean nothing mid-morph. Both are
settled by reasoning about how often a user performs the act, which no render
can measure. Rendering the morph would make it look *appealing* — decoration
usually does — which is the precise trap here.

**Because a `<Still>` answers it** — the appearance switch, and the mode pill's
48 dp slide. The appearance switch moves *nothing*; it recolours everything. Two
full-screen images plus one duration is the whole design. The pill is one object
travelling a fixed short distance on a curve the product already ships. In both
cases the Android change is small enough that the device is a faster and more
truthful review surface than a video. The skill's own `<Still>` doc is what makes
this a category rather than a dodge.

**Because it is already inside an accepted prototype** — the contextual
trailing-cluster changes. The 683 px capsule rise is P1's fifth layer; the
history capsule's destruction is P2's fourth. A separate prototype would review
the same frames twice and invite two different verdicts on one behaviour.

**Because it is a configuration change the app does not own** — Objects docking,
portrait↔landscape, compact↔expanded. The system destroys and recreates the
layout. 2B's rejection E-R7 is right, and it is reinforced by this repository's
own orientation convention: the app must not add motion to a transition the
platform owns, and must not touch the viewport to do it. There is nothing to
prototype because there is nothing the app may animate.

**Because the design does not exist yet** — Body Dimensions mode, one-sided /
symmetric Scale mode. Neither is named in `PROJECT_STATUS.md`, `ARCHITECTURE.md`, `PRODUCT.md`,
`README.md` or any of the three prior audits; that was checked by grep, not
assumed. (The only `one-sided` hits are about mesh sidedness and have nothing to
do with a Scale mode.) A
motion prototype for an undesigned mode must invent the layout in order to
animate it, and the invented layout then gets mistaken for a decision — the most
expensive failure mode available here, because it looks like progress. When each
is specified: Body Dimensions will most likely be a **P1-shaped** question and
should extend that composition; one-sided/symmetric Scale is a **static** anchor
question first, closer to a diagram than to motion, and it lives in the gizmo,
where the standing rule is that geometry changes are instant.

---

## Section 12 — What Remotion cannot tell us

The instruction was to be honest about this. Five of these are load-bearing.

1. **It cannot test interruption, cancellation or gesture handoff.** A Remotion
   composition is a pure function of frame number. There is no input, no pointer,
   no second tap. So finding **E-08** — should an interrupted growth scale its
   exit duration by the fraction travelled? — is **entirely outside** what any
   prototype here can answer, and 2B's runtime probe
   (67,70,72 → 64,67,69 → 62,66,67 → 61,64,66) remains the only evidence that
   exists. Anything a prototype "showed" about interruption would be a scripted
   re-enactment of an assumption.
2. **It cannot tell you how it feels under a finger.** Touch latency, frame
   pacing, jank, the delay between press and first moved pixel — none of it
   survives an offline render. A 190 ms animation that starts 60 ms late feels
   like 250 ms, and the prototype shows 190 ms every time.
3. **It cannot validate the Android implementation.** `ViewPropertyAnimator`,
   `PathInterpolator`, `LayoutTransition`, `TransitionManager`, view recreation
   across a configuration change, `recreate()` — a prototype says nothing about
   whether any of the proposed motion is *achievable* in the Android layout
   system. The appearance crossfade (E-07) is the sharpest case: its real
   difficulty is crossfading across an Activity recreate, which no composition
   touches.
4. **It cannot reproduce the Vulkan viewport.** The renderer is native, the
   camera is native, the gizmo is native. P1 and P2 sidestep this by compositing
   real screenshots and never animating the viewport, which is why they are
   trustworthy. P3 does not have that option, and its verdict is correspondingly
   weaker — stated in 6.6 and required to travel with the asset.
5. **It cannot settle accessibility.** Whether the alpha-only reduced-motion
   variant serves a person who relies on "Remove animations" is a question for
   that person. The prototype shows the two options; it decides nothing.
6. **It carries no design opinion at all.** Nothing in the 48 KB read says what
   duration is right, what should animate, or what "feels" correct. Every such
   judgement in this report is inherited from UI-AUDIT2A/2B or from the
   repository's own rules. Remotion contributed the *instrument*, and it should
   never be cited as the authority for a design verdict.

**Where it is genuinely strong**, stated with the same directness: deterministic
frame-exact timing; side-by-side variants on one clock; slow-motion review with
no device and no `animator_duration_scale`; a printable frame strip; mechanically
verifiable invariants (the still-viewport digest check in P2 and P3 is a real
test, not an impression); and a reviewable artefact that costs a render instead
of a stage.

---

## Section 13 — Recommendation: is Remotion genuinely useful for the next correction stage?

**Qualified yes, for exactly one thing — and it is not on the critical path.**

The reasoning, in the order it actually matters:

**The top four corrections need no prototype at all.** 2B's ranked list opens
with: `Gravity.TOP` on the floating rail cluster; putting the justified
movements on the surface's timeline; fixing the precision surface's pivot; and
letting Android Back close a surface so its already-written 150 ms exit runs.
The first is a layout property. The third is one line, and the mechanism already
exists for the Display popover. The fourth calls code that is already written and
tested. **None of these is an open design question**, and a stage that shipped
only those four would remove every teleport the audits measured. Commissioning
prototypes before that work would delay the fixes that need no help.

**One correction is genuinely uncertain, expensive, and hard to unwind — and
that is where the prototype pays.** The Construction↔Sculpt transition (E-01,
prototype **P2**) proposes three groups on two durations with a deliberately
static viewport, across six controls, in the most architecturally sensitive
region of the product. If the model is wrong, it is wrong after the Android work
is done, and undoing it touches the mode-swap path. **Rendering P2 first is the
single highest-value use of Remotion in this project**, and it is worth roughly a
day.

**P1 is worth it only if the Exact Transform choreography ships in the same stage
as E-01.** Its core question — one timeline or a distance rule — is real, and
V2-beats-V1 would be a genuinely expensive surprise to hit mid-implementation.
But it is also answerable by building V1 in Android and looking, because the
Android change is smaller than P2's. Verdict: **build P1 only as a companion to
P2**, when the composition scaffold and the masked crops already exist and the
marginal cost is a few hours rather than a day.

**P3 should not be commissioned yet.** Its verdict is about orientation
interpolation in the abstract, not about ForgeShape's gizmo, and E-10 is a P3
severity finding on a product whose P1 findings are still open. Revisit only if
E-10 is promoted.

**And the cheapest item in this audit is the one to do first if anything is
done at all:** the two-render reduced-motion pair from section 8. One boolean
prop, two renders, and 2B's one open disagreement (E-09) stops being an argument
between two audits and becomes something the owner can look at.

**Net.** Remotion is a real instrument for this project and a narrow one. It is
worth using **once**, on P2, **before** the Construction↔Sculpt transition is
built — and it is worth *not* using for the four corrections that should ship
first. The most defensible sequence is: land the structural fixes; render P2;
decide E-01 from the render; build it; and treat P1 and P3 as optional
companions. A recommendation to prototype everything would be easier to write
and would be wrong.

**Boundary restated, because it is a hard rule and not a preference.** No React,
no Remotion, no npm package and no web dependency enters ForgeShape. Any
prototype project lives outside this repository; only rendered outputs would ever
be committed under `artifacts/`. This audit created no project and rendered
nothing.

---

## Section 14 — Evidence inventory for this audit

| id | artifact | what it establishes |
| --- | --- | --- |
| — | [`SKILL-PROOF.md`](SKILL-PROOF.md) | the hard gate: paths, sizes, mtimes, SHA-256 for 13 files, declared version 4.0.517, sections consulted, 15 rules mapped to decisions, irrelevant guidance marked, exact commands |
| — | this `README.md` | classification of all 13 candidates, 3 prototype specifications, frame tables, reduced-motion variants, asset requirements, limits, recommendation |

No screenshots, probes or device runs were produced by UI-AUDIT2C. It is a
read-only analytical pass over evidence that already exists, and it says so
rather than manufacturing artefacts to look thorough. Every measurement it
quotes is attributed to the UI-AUDIT2B artifact that produced it.

**Product-tree state:** `git diff --stat bd3f7d5 HEAD -- . ':(exclude)artifacts'`
returns empty. The product is byte-identical to Stage 020R3.

---

## Next step

**Return the three audit reports — UI-AUDIT2A, UI-AUDIT2B and this
UI-AUDIT2C — to the owner/coordinator for cross-audit synthesis.**
