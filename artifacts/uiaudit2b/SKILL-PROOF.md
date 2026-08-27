# UI-AUDIT2B — proof that the Emil Kowalski design-engineering skill was read from disk

The hard gate. What was read, where it lives, what it hashes to, which sections
were consulted, the rules taken from them, and which ForgeShape interaction each
rule decided.

## 1. Location on disk

```
C:\Users\damia\.claude\skills\emil-design-eng\SKILL.md
```

The skill is a **single file**. `find . -type f` over the skill directory returns
`./SKILL.md` and nothing else — there is no `reference/` directory, no scripts,
no agents. Every rule used in this audit therefore comes from that one file, read
end to end in three passes (lines 1-240, 240-480, 480-674).

## 2. Declared version

The YAML frontmatter declares `name: emil-design-eng` and a `description`, and
**no `version:` field**. Recorded as absent rather than invented. The digest
below is the identity for this run.

## 3. File read, with size, mtime and SHA-256

| file | bytes | lines | mtime | SHA-256 |
| --- | --- | --- | --- | --- |
| `SKILL.md` | 27900 | 674 | 2026-08-25 19:55:41.060835300 +0200 | `defffff8bea4583897b001b9173ccd6fb6f8341fffc63a1891a38372137a1848` |

## 4. Exact shell commands used to inspect the skill

```bash
cd ~/.claude/skills/emil-design-eng && find . -type f | sort
cd ~/.claude/skills/emil-design-eng && wc -l $(find . -type f -name "*.md")
cd ~/.claude/skills/emil-design-eng && sed -n '1,240p'   SKILL.md
cd ~/.claude/skills/emil-design-eng && sed -n '240,480p' SKILL.md
cd ~/.claude/skills/emil-design-eng && sed -n '480,674p' SKILL.md
cd ~/.claude/skills/emil-design-eng && sha256sum SKILL.md
cd ~/.claude/skills/emil-design-eng && stat -c '%s bytes  mtime=%y' SKILL.md
cd ~/.claude/skills/emil-design-eng && grep -n "^version\|^name" SKILL.md | head -3
```

## 5. Sections actually consulted

- **Core Philosophy** — Taste is trained; Unseen details compound; Beauty is leverage.
- **Review Format (Required)** — the mandatory Before/After/Why markdown table.
- **The Animation Decision Framework** — 1. Should this animate at all (the
  frequency table); 2. What is the purpose (the five valid purposes); 3. What
  easing (the decision tree, the three custom curves, the ban on `ease-in`);
  4. How fast (the duration table, the sub-300 ms rule).
- **Perceived performance.**
- **Spring Animations** — when to use springs; the interruptibility advantage.
- **Component Building Principles** — Buttons must feel responsive
  (`scale(0.97)` on press); Never animate from `scale(0)`; Make popovers
  origin-aware (and the modal exception); Use CSS transitions over keyframes for
  interruptible UI; Use blur to mask imperfect transitions.
- **CSS Transform Mastery** — `translateY` percentages; `transform-origin`.
- **Gesture and Drag Interactions** — pointer capture; multi-touch protection.
- **Performance Rules** — only animate transform and opacity; CSS beats JS under
  load.
- **Accessibility** — `prefers-reduced-motion` ("fewer and gentler animations,
  not zero ... keep opacity and colour transitions that aid comprehension").
- **The Sonner Principles** — good defaults over options; handle edge cases
  invisibly; transitions not keyframes; cohesion matters; asymmetric enter/exit
  timing; review your work the next day.
- **Stagger Animations.**
- **Debugging Animations** — slow-motion testing; frame-by-frame inspection;
  test on real devices.
- **Review Checklist** — the full table.

## 6. Rules taken from those sections, and what each decided here

| # | rule (paraphrased) | source section | ForgeShape interaction / finding it decided |
| --- | --- | --- | --- |
| 1 | Ask first whether it should animate at all; frequency decides. Actions performed hundreds of times a day get no animation, ever. | Animation Decision Framework 1 | Kept **Move/Rotate/Scale**, **World/Local** and **Sculpt tool change** as correctly instant — a modeling tool switches mode constantly. Recorded as **rejected motion ideas**, not findings. |
| 2 | Every animation needs a purpose; "preventing jarring changes" and "spatial consistency" are valid, "it looks cool" is not. | Animation Decision Framework 2 | **E-01**: the whole workspace swaps between Construction and Sculpt in a single cut — the largest jarring change in the product, and the one place motion has a purpose it is not being spent on. |
| 3 | Popovers must scale from their trigger, not from an arbitrary corner; only modals stay centred. | Component Building Principles — origin-aware popovers | **E-03**: the precision surface's growth origin is its own **bottom-left** corner while the control that opened it sits above its **top-right** — measured in source (`pivotX=0, pivotY=height`) and at runtime (title row 1715 → 1704 px through the growth, left edge fixed at 21 px). |
| 4 | Never animate from `scale(0)`; start at ~0.95 with opacity so nothing appears from nothing. | Component Building Principles | **PASS**: `ANCHORED_START_SCALE = 0.96f`, uniform on both axes. Recorded as a positive finding. |
| 5 | Use strong custom easing; `cubic-bezier(0.23, 1, 0.32, 1)` for UI ease-out; never `ease-in`. | Animation Decision Framework 3 | **PASS**: `ChromeMotion` ships exactly `cubic-bezier(0.23, 1, 0.32, 1)` as its one curve, and the header records that it exists to replace the platform's default ease-in-out. Exact match to the skill's recommended value. |
| 6 | UI animations stay under 300 ms; dropdowns 150-250 ms; small popovers 125-200 ms. | Animation Decision Framework 4 | **PASS**: 190/150 ms anchored, 120/90 ms fade. All four inside the bands. |
| 7 | Exit must be faster than enter; slow where the user decides, fast where the system responds. | Review Checklist; Asymmetric enter/exit timing | **PASS**: 190→150 and 120→90, both ratios correct. |
| 8 | Buttons must feel responsive to press — `scale(0.97)` on `:active`, with a ~160 ms transition. | Component Building Principles | **E-06**: press feedback exists on 14 drawables but is an **instant colour swap** with no scale and no fade — `grep -rn "FadeDuration" res/drawable/` returns **0**. |
| 9 | A state swap between two visual states should transition, not cut; blur or crossfade bridges what timing alone cannot. | Use blur to mask imperfect transitions | **E-04**: the active pill in the mode and space capsules teleports between segments via `setBackgroundResource`. **E-07**: the appearance switch repaints every colour in the product in one cut (8 identical probe frames). |
| 10 | Prefer interruptible transitions over keyframes; a second trigger must retarget from the current value, never restart or queue. | Transitions over keyframes; Interruptibility advantage | **PASS**: cancel-first is enforced in `ChromeMotion.begin`, and the runtime reversal test shows a monotone fade-down from the interrupted value (67,70,72 → 64,67,69 → 62,66,67 → 61,64,66) with no snap. **E-08** notes the one residual: the exit takes its full duration however far the enter got. |
| 11 | Reduced motion means fewer and gentler animations, not zero; keep the opacity and colour transitions that aid comprehension, drop the movement. | Accessibility — prefers-reduced-motion | **E-09**: at `animator_duration_scale 0` every transition lands instantly with no transient state (verified — identical frames). The contract is honoured exactly and the movement is correctly gone; the *opacity* the skill would keep is gone too. Judgment call, argued both ways in the report. |
| 12 | Only animate transform and opacity; they skip layout and paint. | Performance Rules | **PASS**: every ForgeShape transition animates alpha, scaleX and scaleY only — nothing animates a layout property. **E-02** is the mirror image: the properties that *change* on a chrome transition are layout positions, which is why they cannot be animated as things stand. |
| 13 | Elements that are physically related must move together; unrelated controls must not jump. | Animation Decision Framework 2 (spatial consistency); Review Checklist | **E-02**: opening the precision surface moves the Tool Rail 120 dp, the mode capsule 120 dp and the bottom row 260 dp **instantly**, in the same frame the surface begins a 190 ms growth. Proven from probe frame 1, where the sheet is still at partial alpha and every other control is already at its final position. |
| 14 | Review animations in slow motion; timing faults invisible at full speed show up frame by frame. | Debugging Animations | The whole probe method: `animator_duration_scale` 20, ~509 ms between screencaps, byte-identical PNGs on a static screen. This is what turned "the rail jumps" into "the rail has already arrived while the sheet is at 30% alpha". |
| 15 | Cohesion matters — the motion must match the personality of the thing; and good defaults matter more than options. | Sonner Principles | **E-05**: the product owns exactly one motion family (anchored growth) and one native feedback animation (the selection pulse). Both are well judged. The defect is coverage, not taste: 5 of 18 audited interactions animate. |

## 7. Skill guidance that is N/A here, and why

| guidance | status | reason |
| --- | --- | --- |
| The "Initial Response" script | N/A | That instruction applies when the skill is invoked conversationally with no question. This is a directed audit. |
| CSS syntax specifics (`@starting-style`, `clip-path`, `transition: all`, `:active`) | translated, not applied literally | ForgeShape has no CSS. Each rule was mapped to its Android equivalent: `transition` → `ViewPropertyAnimator`, `transform-origin` → `setPivotX/Y`, `:active` → `android:state_pressed`, `prefers-reduced-motion` → `Settings.Global.ANIMATOR_DURATION_SCALE`. |
| Framer Motion / React / Sonner specifics | N/A | No JavaScript, no React, no third-party runtime — the repository forbids all three. |
| Hover-state media queries | N/A | Touch and stylus only; there is no hover state in the product. |
| Spring animations | considered, not required | Recommended in the report only for the one place a gesture hands off (the gizmo drag release), and explicitly rejected everywhere else. |
| Stagger | considered, rejected | See the rejected-ideas section: the Add Primitive tiles are a fixed set of six seen constantly. |
