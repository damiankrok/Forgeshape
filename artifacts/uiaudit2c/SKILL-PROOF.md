# UI-AUDIT2C — proof that the Remotion skill was read from disk

The hard gate. Where the skill lives, what every file read hashes to, which
sections were consulted, the rules taken from them, and which ForgeShape
decision each rule actually changed.

---

## 1. Location on disk

The Remotion skill is installed as **twelve sibling skill directories** under
`C:\Users\damia\.claude\skills\`, of which `remotion-best-practices` is the
router and also bundles a nested copy of every other one:

```
C:\Users\damia\.claude\skills\remotion-best-practices\        <- router (entry point)
C:\Users\damia\.claude\skills\remotion-captions\
C:\Users\damia\.claude\skills\remotion-create\
C:\Users\damia\.claude\skills\remotion-docs\
C:\Users\damia\.claude\skills\remotion-interactivity\
C:\Users\damia\.claude\skills\remotion-maps\
C:\Users\damia\.claude\skills\remotion-markup\
C:\Users\damia\.claude\skills\remotion-multimedia\
C:\Users\damia\.claude\skills\remotion-render\
C:\Users\damia\.claude\skills\remotion-saas\
C:\Users\damia\.claude\skills\remotion-studio\
C:\Users\damia\.claude\skills\remotion-upgrade\
```

The router was invoked through the harness (`Skill: remotion-best-practices`),
which reported base directory
`C:\Users\damia\.claude\skills\remotion-best-practices` and emitted the routing
table quoted in section 5. Every file below was then read directly from disk
with `cat` / `sed`, not from the harness output.

### Nested copy vs. top-level copy

`remotion-best-practices\<name>\REFERENCE.md` and `remotion-<name>\SKILL.md` are
the **same documents with different relative link targets**. Verified, not
assumed:

```
$ diff remotion-markup/SKILL.md remotion-best-practices/remotion-markup/REFERENCE.md
8c8, 24c24, 278c278   -> three lines, all of the form ../x/SKILL.md vs ../x/REFERENCE.md
$ diff remotion-create/SKILL.md remotion-best-practices/remotion-create/REFERENCE.md
6 lines, all link-path only
```

`remotion-interactivity`, `remotion-render` and `remotion-studio` are
**byte-identical** across both locations (same SHA-256 — see the table). The
audit therefore quotes the `remotion-best-practices/*/REFERENCE.md` copies as
canonical; no rule below depends on which copy was read.

## 2. Declared version

Every reference file consulted declares the **same** version in YAML
frontmatter:

```
$ grep -n "^version:" SKILL.md remotion-markup/REFERENCE.md remotion-interactivity/REFERENCE.md \
        remotion-render/REFERENCE.md remotion-studio/REFERENCE.md remotion-create/REFERENCE.md
SKILL.md:4:version: 4.0.517
remotion-markup/REFERENCE.md:4:version: 4.0.517
remotion-interactivity/REFERENCE.md:4:version: 4.0.517
remotion-render/REFERENCE.md:4:version: 4.0.517
remotion-studio/REFERENCE.md:4:version: 4.0.517
remotion-create/REFERENCE.md:4:version: 4.0.517
```

**Declared skill version: `4.0.517`.** The sub-documents under `remotion-markup/`
(`timing.md`, `transitions.md`, `sequencing.md`, `compositions.md`, `3d.md`,
`multi-scene-video.md`, `images.md`) carry `name` / `description` /
`metadata.tags` frontmatter and **no `version:` field** — recorded as absent
rather than inherited or invented.

## 3. Files read, with size, line count, mtime and SHA-256

All paths relative to `C:\Users\damia\.claude\skills\remotion-best-practices\`.
All mtimes fall on 2026-08-25 within the same second, i.e. the whole skill was
installed in one operation.

| file | bytes | lines | mtime | SHA-256 |
| --- | --- | --- | --- | --- |
| `SKILL.md` | 2542 | 59 | 2026-08-25 19:55:52.161438900 +0200 | `bb2e3389fdb4882da3de02731d407a38769235b06406963ed1c6c87f8d33c6e6` |
| `remotion-markup/REFERENCE.md` | 11404 | 365 | 2026-08-25 19:55:52.209343700 +0200 | `b5615adc271201a3554771bac809ce3524427fc2be59a510152162f9c224b3f8` |
| `remotion-markup/timing.md` | 3426 | 111 | 2026-08-25 19:55:52.260459200 +0200 | `67b19b7c8f628030fd3e6ff4967f55c2134557f5e3710c30a14fe5a8d598b053` |
| `remotion-markup/transitions.md` | 7121 | 239 | 2026-08-25 19:55:52.261466000 +0200 | `2af74184c2bde1be094f1154c5d43561e1f9daae6242b2067f20e110c231a1c5` |
| `remotion-markup/sequencing.md` | 3998 | 169 | 2026-08-25 19:55:52.258384600 +0200 | `d8bf2eee00436703ace5c57c184da8587ca9599c44275be7d4ed9e24e99df6e3` |
| `remotion-markup/compositions.md` | 3311 | 117 | 2026-08-25 19:55:52.213382800 +0200 | `ec04aefa1c77d2f58b07c70639c11d7f89e30d9bd31ea7dccd211ffd77cd2c64` |
| `remotion-markup/multi-scene-video.md` | 2116 | 81 | 2026-08-25 19:55:52.224486300 +0200 | `b9ca17d0092fb71559d09acd1a6730d7b59bdcc226c229d2de5f40476a0a2536` |
| `remotion-markup/3d.md` | 2378 | 86 | 2026-08-25 19:55:52.208335300 +0200 | `22c36e087eefd6cfef597bda54f72ae1f39099f9cf33ec38079dd53c4b08e651` |
| `remotion-markup/images.md` | 1539 | 73 | 2026-08-25 19:55:52.219380700 +0200 | `815ff21943ce1bfb1efb82d0fd01b94874f4f1fb21af4bbc076cb64948b2df35` |
| `remotion-interactivity/REFERENCE.md` | 7776 | 247 | 2026-08-25 19:55:52.177118000 +0200 | `cd55ece115f4f9201efc989d3bf4dc21482357d2183878f8f36cdd9ea85ed85b` |
| `remotion-render/REFERENCE.md` | 497 | 27 | 2026-08-25 19:55:52.269970600 +0200 | `c0a1274bcdcff0f068809184106b09105ca7e758b8d1193a9bcfe40fddabde57` |
| `remotion-studio/REFERENCE.md` | 982 | 24 | 2026-08-25 19:55:52.278048100 +0200 | `364923d7228e2d61b2604e4f6280d8e19fdd4fc4b8867967c8c8c03a4792f755` |
| `remotion-create/REFERENCE.md` | 2109 | 68 | 2026-08-25 19:55:52.169445800 +0200 | `918260f2e6d19ee1ebabdfbfd7438f9796ae63e80be73568b4e61b79acb0d2b9` |

Top-level sibling copies, hashed for the identity comparison in section 1:

| file | SHA-256 | relation |
| --- | --- | --- |
| `..\remotion-markup\SKILL.md` | `ddc8ae668c7b71cb818cb430f199226d36e728067d1cdd26fcecc7d7c004c76e` | differs from nested copy in 3 link lines only |
| `..\remotion-interactivity\SKILL.md` | `cd55ece115f4f9201efc989d3bf4dc21482357d2183878f8f36cdd9ea85ed85b` | **byte-identical** to nested |
| `..\remotion-studio\SKILL.md` | `364923d7228e2d61b2604e4f6280d8e19fdd4fc4b8867967c8c8c03a4792f755` | **byte-identical** to nested |
| `..\remotion-render\SKILL.md` | `c0a1274bcdcff0f068809184106b09105ca7e758b8d1193a9bcfe40fddabde57` | **byte-identical** to nested |
| `..\remotion-create\SKILL.md` | `da68da61ae27c119c9ffc027f196dcf3396e8e4f2cdc3f3dddf5cecc4644a0f1` | differs from nested copy in 6 link lines only |

## 4. Exact shell commands used to inspect the skill

Run from Git Bash on Windows. Reproduced verbatim.

```bash
ls -d /c/Users/damia/.claude/skills/*remotion*
find /c/Users/damia/.claude -maxdepth 6 -iname "SKILL.md" -path "*remotion*"

cd /c/Users/damia/.claude/skills
for d in remotion-best-practices remotion-markup remotion-render remotion-studio \
         remotion-interactivity remotion-create remotion-docs; do
  echo "=== $d ==="; ls -R $d | head -40
done

cd /c/Users/damia/.claude/skills/remotion-best-practices
wc -c SKILL.md remotion-markup/REFERENCE.md remotion-markup/timing.md \
      remotion-markup/transitions.md remotion-markup/effects.md \
      remotion-markup/compositions.md remotion-markup/multi-scene-video.md \
      remotion-markup/sequencing.md remotion-render/REFERENCE.md \
      remotion-studio/REFERENCE.md remotion-interactivity/REFERENCE.md \
      remotion-create/REFERENCE.md

for f in remotion-markup/REFERENCE.md remotion-markup/timing.md remotion-markup/transitions.md; do
  echo "########## $f"; cat "$f"
done
for f in remotion-markup/compositions.md remotion-markup/sequencing.md \
         remotion-markup/multi-scene-video.md remotion-render/REFERENCE.md \
         remotion-studio/REFERENCE.md remotion-create/REFERENCE.md; do
  echo "########## $f"; cat "$f"
done
cat remotion-interactivity/REFERENCE.md
cat remotion-markup/3d.md
head -60 remotion-markup/images.md

for f in SKILL.md remotion-markup/REFERENCE.md remotion-markup/timing.md \
         remotion-markup/transitions.md remotion-markup/sequencing.md \
         remotion-markup/compositions.md remotion-markup/multi-scene-video.md \
         remotion-markup/3d.md remotion-markup/images.md \
         remotion-interactivity/REFERENCE.md remotion-render/REFERENCE.md \
         remotion-studio/REFERENCE.md remotion-create/REFERENCE.md; do
  printf '%s|' "$f"; stat -c '%s|%y|' "$f"; sha256sum "$f" | cut -d' ' -f1
done

grep -n "^version:\|^name:" SKILL.md remotion-markup/REFERENCE.md \
     remotion-interactivity/REFERENCE.md remotion-render/REFERENCE.md \
     remotion-studio/REFERENCE.md remotion-create/REFERENCE.md
wc -l SKILL.md remotion-markup/REFERENCE.md remotion-markup/timing.md \
      remotion-markup/transitions.md remotion-markup/sequencing.md \
      remotion-markup/compositions.md remotion-markup/multi-scene-video.md \
      remotion-markup/3d.md remotion-markup/images.md \
      remotion-interactivity/REFERENCE.md remotion-render/REFERENCE.md \
      remotion-studio/REFERENCE.md remotion-create/REFERENCE.md

cd /c/Users/damia/.claude/skills
for f in remotion-markup/SKILL.md remotion-interactivity/SKILL.md \
         remotion-studio/SKILL.md remotion-render/SKILL.md remotion-create/SKILL.md; do
  printf '%s ' "$f"; sha256sum "$f" | cut -d' ' -f1
done
diff -q remotion-markup/SKILL.md remotion-best-practices/remotion-markup/REFERENCE.md
diff -q remotion-markup/timing.md remotion-best-practices/remotion-markup/timing.md
diff    remotion-markup/SKILL.md remotion-best-practices/remotion-markup/REFERENCE.md | head -20
diff    remotion-create/SKILL.md remotion-best-practices/remotion-create/REFERENCE.md | head -20
```

## 5. Sections actually consulted

| document | sections read |
| --- | --- |
| `SKILL.md` (router) | *Preserve user changes*; *Creating a video*; *New project setup*; *React Markup Best Practices*; *Improving Interactivity*; *Rendering*; *Opening Remotion Studio*; *Looking up Remotion APIs*; plus the Maps / Multimedia / Captions / SaaS / Upgrading routes, read and then ruled out (section 7) |
| `remotion-markup/REFERENCE.md` | *General rules*; *Assets*; *Media components*; *Example scene*; *Delaying, trimming* (`from`, `durationInFrames`, `trimBefore`, Sequence fallback); *Visual and pixel effects* (order of preference); *3D content*; *Timing*; *Advanced compositions*; *Advanced sequencing*; *Install modules*; *Previewing markup*; *Optional: one-frame render check* |
| `remotion-markup/timing.md` | whole file — linear `interpolate`, extrapolation clamping, *Studio-editable animation patterns*, *Spring easing*, *Bézier easing*, *Animating scale*, *Multiple keyframes*, *Posterization* |
| `remotion-markup/transitions.md` | whole file — `TransitionSeries`, Transition vs Overlay, transition props, available presentations, `linearTiming` / `springTiming`, *Duration calculation* |
| `remotion-markup/sequencing.md` | whole file — `<Sequence>` delay/trim, *Premounting*, `<Series>` and negative offsets, *Frame References Inside Sequences*, *Nested Sequences*, nesting compositions |
| `remotion-markup/compositions.md` | *Default Props and scaffold metadata*; *Folders*; *Stills*; *Dynamic duration, width, and height*; *Nesting compositions* |
| `remotion-markup/3d.md` | whole file — `@remotion/three` prerequisite, `<ThreeCanvas>` width/height requirement, *No animations not driven by `useCurrentFrame()`*, `useFrame()` prohibition, `layout="none"` rule |
| `remotion-markup/multi-scene-video.md` | whole file — one scene per file, per-scene `<Composition>` registration, inline `durationInFrames` |
| `remotion-markup/images.md` | *Sizing and positioning*; *Dynamic image paths*; *Getting image dimensions* |
| `remotion-interactivity/REFERENCE.md` | whole file — `Interactive.*`, *Prefer inline text*, *Give interactive elements a descriptive name*, *Keep all CSS styles inline*, *Animate using `interpolate()`* (what the Studio can and cannot parse), *Use `scale`, `translate`, `rotate`*, *Keep composition metadata inline*, *Effects should be inline too* |
| `remotion-render/REFERENCE.md` | *General rendering strategy*; `npx remotion still`; transparent-video route (ruled out) |
| `remotion-studio/REFERENCE.md` | whole file — `npx remotion studio --no-open`, `--port`, `--force-new`, `--log` |
| `remotion-create/REFERENCE.md` | *Scaffold a project*; *Designing a video*; *Is this a multi-scene video?*; *Interactivity Best Practices*; *Open the preview*; *Render the video* ("only render if the user explicitly asks") |

## 6. Fifteen rules taken from those sections, and the ForgeShape decision each one changed

| # | rule, paraphrased | source section | ForgeShape screen / finding it affected |
| --- | --- | --- | --- |
| **R1** | The Remotion skill is a **router**: the entry document routes to one sub-reference per task, and loading the wrong one is how you end up applying video-production advice to something that is not video production. | `SKILL.md`, all routes | Set this audit's scope: markup, interactivity, timing, sequencing, compositions, 3d, studio and render are in; captions, maps, multimedia, SaaS and upgrade are out (section 7). It is why the report classifies candidates rather than proposing a video for each. |
| **R2** | **All motion must be driven by `useCurrentFrame()` + `interpolate()`.** CSS `transition`, CSS `animation` and Tailwind animation classes do not render and must be refactored away. | markup *General rules* | Every prototype spec states timings as **frame ranges**, not CSS durations — which forced the choice of `fps = 100` so ForgeShape's 190 / 150 / 220 / 120 / 90 ms constants land on integer frames instead of being rounded into a lie. |
| **R3** | Customize timing with **`Easing.bezier()`**, taking the same four numbers as CSS `cubic-bezier()`, or `Easing.spring({damping})`. | markup *General rules*; `timing.md` *Bézier easing*, *Spring easing* | ForgeShape's `ChromeMotion.anchoredEase()` is `PathInterpolator(0.23f, 1f, 0.32f, 1f)`. It maps 1:1 onto `Easing.bezier(0.23, 1, 0.32, 1)`, so **P1 and P2 reproduce the product's real curve** rather than a Remotion house curve. The skill's own example curve `(0.16, 1, 0.3, 1)` becomes an explicit *comparison variant*, not the default. |
| **R4** | Linear scale output looks wrong to the eye; scale interpolations need **`output: 'perceptual-scale'`**. | `timing.md` *Animating scale* | The anchored surfaces' `ANCHORED_START_SCALE = 0.96f` growth in **P1** and in **P2**'s arriving brush column carries `output: 'perceptual-scale'`. Without it the prototype under-shows the first third of the growth and the review judges a curve the device never plays. |
| **R5** | `interpolate()` takes **n keyframes and n−1 easings** as an array, so hold → move → settle is one call, not three. | `timing.md` *Multiple keyframes* | Every P1/P2 layer timeline is a single 4-keyframe call `[0, 5, 24, 60]` with a 3-easing array — which is what makes "does the rail share the sheet's timeline?" answerable by reading one line per layer. |
| **R6** | `posterize: n` deliberately samples every nth frame for artistic effect. | `timing.md` *Posterization* | **Explicitly rejected, not merely unused.** ForgeShape's open question is partly about frame pacing and about whether chrome and surface land on the same frame; posterizing a motion-review asset would manufacture exactly the artefact under review. Recorded so a later reader does not "improve" the spec by adding it. |
| **R7** | The Studio can only parse **inline** `interpolate()` calls on the `style` prop, with hardcoded output range, easing, extrapolation and `output`; only the `frame` variable may be interpolated; the input range may use bare `fps` / `durationInFrames` and simple arithmetic on them. Extracted constants, spreads and outside math grey the values out. | `remotion-interactivity` *Animate using `interpolate()`*, *Keep all CSS styles inline* | Why each prototype spec lists **literal per-layer keyframe values** (e.g. rail `0px 0px → 0px -316px`) instead of a formula. If the owner ever builds these, timings stay retimable in Studio by dragging, with no code round-trip — the only thing that makes a timing-variant review cheap. |
| **R8** | Prefer the individual **`scale`, `translate`, `rotate`** CSS properties over `transform` strings; only the individual properties are interactively editable. | `remotion-interactivity` *Use `scale`, `translate`, `rotate`*; `timing.md` | All ForgeShape chrome displacements in P1/P2 are specified as `translate` two-value strings and the anchored growth as `scale` — never as a composed `transform`. |
| **R9** | Give every interactive element a **descriptive, hardcoded `name`**; avoid computed names; a component with too many elements makes the timeline unreadable. | `remotion-interactivity` *Give interactive elements a descriptive name*; *Prefer inline text* | Prototype layer names are ForgeShape's own stable semantic ids and fixed UI vocabulary — `tool_rail`, transform mode selector, coordinate-space selector, precision surface, Objects capsule, history capsule, brush column — so a Remotion timeline and a ForgeShape review share one vocabulary. It also capped each prototype at 5–7 layers. |
| **R10** | Keep `width`, `height`, `fps`, `durationInFrames` and `defaultProps` **inline literals** on `<Composition>`; no extracted `defaultProps`, no spread, no `satisfies`, no type assertions; use `type` not `interface`. | `compositions.md` *Default Props and scaffold metadata*; `remotion-interactivity` *Keep composition metadata inline* | Each prototype is **one `<Composition>` per measured device size** with real pixel dimensions inline — `1080x2400` compact portrait, `2400x1080` short landscape, `1600x2560` expanded — read from the UI-AUDIT2A/2B PNG headers, not guessed. Variants become inline `defaultProps` so the owner can flip one in the Props editor. |
| **R11** | `<Still>` renders a single frame and needs neither `fps` nor `durationInFrames`; `npx remotion still --frame=N --scale=` is the cheap sanity check. | `compositions.md` *Stills*; markup *Optional: one-frame render check*; `remotion-render` | Gave the audit a precise meaning for its middle classification: **STATIC MOCKUP ENOUGH = a question a `<Still>` answers.** The appearance crossfade and the mode-pill slide land there; they need one or two frames and a stated duration, not a timeline. |
| **R12** | Inside a `<Sequence>`, `useCurrentFrame()` restarts at 0; **always premount** (`premountFor`) so a layer is not first laid out on the frame it becomes visible. | `sequencing.md` *Premounting*, *Frame References Inside Sequences* | Directly shapes **P2**: the arriving brush column is a `<Sequence premountFor={1 * fps}>`, so the prototype does not show a first-frame layout pop the real Android view would not have — and each of the three groups gets a local 0-based timeline, which is what makes "same 220 ms window, three groups" checkable. |
| **R13** | `TransitionSeries.Transition` (fade / slide / wipe / flip / clockWipe) plays **two scenes simultaneously and shortens the composition**; `TransitionSeries.Overlay` does not shorten it; overlays may not be adjacent to a transition. | `transitions.md`, whole file | **Marked IRRELEVANT and recorded as a trap.** These are video scene-cut effects. Depicting ForgeShape's Construction↔Sculpt change with `fade()` or `slide()` would render it as a *cut between two screens* — precisely the defect UI-AUDIT2B filed as E-01. P2 is therefore built as **one continuous scene with three grouped layers**, never as a `TransitionSeries`. This rule changed the shape of the accepted spec more than any other. |
| **R14** | 3D requires `@remotion/three`; content **must** be wrapped in `<ThreeCanvas width height>` with real lighting; `useFrame()` from `@react-three/fiber` is **forbidden**; nothing may self-animate — all motion from `useCurrentFrame()`; a `<Sequence>` inside `<ThreeCanvas>` must be `layout="none"`. | `3d.md`, whole file | The entire feasibility case for **P3** (World↔Local gizmo reorientation, UI-AUDIT2B finding E-10) — and also its cost. It is the only candidate needing a second package and a hand-rebuilt gizmo in Three.js, which is why P3 ranks third and is marked the first to drop. |
| **R15** | Assets live in `public/` and are referenced with `staticFile()`; add packages only via `npx remotion add`; preview with `npx remotion studio --no-open`; and **only render when the user explicitly asks**. | markup *Assets*, *Install modules*, *Previewing markup*; `remotion-create` *Render the video*; `remotion-studio` | Two decisions. (a) A prototype's screenshots must be **copied** into that project's `public/`, so ForgeShape's `artifacts/` tree stays read-only and the no-modification rule holds by construction. (b) The skill's own "only render if explicitly asked" restraint matches the owner's instruction not to create a Remotion project — so this audit produced **specifications, not a project**. |

## 7. Skill guidance that is genuinely irrelevant here — marked, not repurposed

The instruction was to mark irrelevant recommendations rather than dress them up
as UI-design rules. These were read and rejected:

| skill content | why irrelevant to ForgeShape |
| --- | --- |
| `transitions.md` in full (`TransitionSeries`, `fade` / `slide` / `wipe` / `flip` / `clockWipe`, `linearTiming` / `springTiming`, duration arithmetic) | Scene-cut grammar for edited video. ForgeShape has no scenes. Using it would misrepresent a UI state change as a cut — see R13. |
| `remotion-captions` (transcription, SRT import, caption display) | No speech, no captions, no narration in a motion-review asset. |
| `remotion-maps` (Mapbox / MapLibre / MapTiler / GeoJSON / flyovers) | No geography anywhere in ForgeShape. |
| `remotion-multimedia` / Mediabunny (browser-side trimming, cropping, metadata) | Nothing is being edited; source frames are `adb screencap` PNGs. |
| `remotion-saas` (`<Player>`, Lambda, Vercel, Cloudflare, Express) | No hosting, no service, no product integration. ForgeShape is an offline Android app and a hard rule forbids web dependencies in it. |
| `voiceover.md` (ElevenLabs TTS), `sfx.md`, `audio.md`, `audio-visualization.md`, `silence-detection.md` | A motion prototype reviewed for timing must be silent; audio biases a reviewer's perception of duration. |
| `remotion-markup/effects.md`, `light-leaks.md`, `html-in-canvas.md` shader effects | Pixel effects and light leaks are decoration. UI-AUDIT2B's standing verdict is that ForgeShape is *under*-animated and never decorative; effects in the prototype would corrupt the review. |
| `google-fonts.md`, `local-fonts.md`, `gifs.md`, `lottie.md`, `embedding-videos.md`, `ffmpeg.md`, `text-highlights.md`, `cropping.md`, `video-editing.md`, `calculate-metadata.md`, `parameters.md` (Zod), `measuring-text.md`, `measuring-dom-nodes.md` | Production conveniences for real videos. None bears on whether a ForgeShape interaction needs a prototype, or on how that prototype should be specified. |
| `remotion-render/transparent-videos.md` | The review asset is composited over ForgeShape's own screenshots; no downstream compositor needs alpha. |
| `remotion-upgrade`, `remotion-docs` | No installed Remotion project exists to upgrade, and no API lookup was needed beyond the reference files hashed above. |
| `remotion-create` *Scaffold a project* (`npx create-video@latest`) | Read, and **deliberately not executed** — the owner instructed that no Remotion project be created. Recorded here so the omission reads as a decision, not an oversight. |

## 8. What this proof does *not* claim

- No Remotion project was scaffolded, no package installed, no Studio started,
  no frame rendered. `node v24.19.0` and `npx 11.17.0` are present on this
  machine (checked, so the feasibility claims in the report are grounded), but
  nothing was run.
- No `.md` file outside the paths hashed in section 3 was used as authority for
  any rule in this audit.
- The rules in section 6 are paraphrases with their source section named, so any
  one of them can be checked against the hashed file.
