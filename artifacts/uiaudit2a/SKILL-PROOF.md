# UI-AUDIT2A — proof that the Impeccable skill was read from disk

This file is the hard gate. It records what was read, where it lives, what it
hashes to, which sections were consulted, the rules taken from them, and which
ForgeShape screen each rule decided. It is not a claim that a skill was used; it
is the evidence.

## 1. Location on disk

Skill base directory (as reported by the runtime and confirmed by `ls`):

```
C:\Users\damia\.claude\skills\impeccable
```

Declared version, from the YAML frontmatter of `SKILL.md`:

```
name: impeccable
version: 4.1.1
```

## 2. Files read, with size, mtime and SHA-256

All paths are relative to the skill base directory above.

| file | bytes | mtime | SHA-256 |
| --- | --- | --- | --- |
| `SKILL.md` | 10741 | 2026-08-25 19:55:26.972631500 +0200 | `38a000bead4d27c06cbd768f996914131c05f7386f7770bc318952d5f6debf66` |
| `reference/audit.native.md` | 8496 | 2026-08-25 19:55:26.980137000 +0200 | `55299f6059ce8ecf01874b2dd4bb3a3f21d39f459e5ee6e8e786b2343494db5c` |
| `reference/android.md` | 4139 | 2026-08-25 19:55:26.978631000 +0200 | `a3757686a0c2778ccbc0ea321f4460368f26bc0229d2b24c3c694a42ce40b1a0` |
| `reference/layout.md` | 5243 | 2026-08-25 19:55:26.993185400 +0200 | `35f1ead201a9b00656aa923105aa130490a2c0f0ce7e8ba9182daa8592779ad9` |
| `reference/adapt.native.md` | 3968 | 2026-08-25 19:55:26.977630800 +0200 | `0abc89f44b1416f173cd2576b6a5ed5e54d4030348e8f5142017a29a7e19e688` |
| `reference/critique.md` | 46538 | 2026-08-25 19:55:26.984147700 +0200 | `789d4952f93ebda4beee9bdaa12d84159461e0f4349b6ff29d40e78beb86c453` |
| `reference/routing.md` | 2933 | 2026-08-25 19:55:26.999185100 +0200 | `936c4957d8e6380a913d83ee7c6aba6cc50da78faebcfabb39a512e275305f80` |
| `scripts/context.mjs` | 66702 | 2026-08-25 19:55:27.003844000 +0200 | `42858ea77ea8be6367ad33b2ea81a1bb825cf1618cc23638ef57cd57aed24aa2` |

`reference/craft-floor.md` was deliberately **not** loaded. `SKILL.md` Setup
step 3 says to load it "immediately before editing UI" and "Do not load it for
planning-only work." UI-AUDIT2A edits nothing, so loading it would have gone
against the skill's own instruction.

## 3. Exact shell commands used to inspect the skill

```bash
ls -d ~/.claude/skills/* 2>/dev/null | head -50
find ~/.claude/plugins -maxdepth 4 -iname "*impeccable*" 2>/dev/null | head -20
cd ~/.claude/skills/impeccable && find . -type f | sort
cd ~/.claude/skills/impeccable && du -sh .
cd ~/.claude/skills/impeccable && wc -l SKILL.md reference/audit.md reference/audit.native.md \
    reference/android.md reference/craft-floor.md reference/routing.md reference/layout.md \
    reference/critique.md reference/adapt.native.md reference/typeset.md
cd ~/.claude/skills/impeccable && cat SKILL.md
cd ~/.claude/skills/impeccable && cat reference/audit.native.md
cd ~/.claude/skills/impeccable && cat reference/android.md
cd ~/.claude/skills/impeccable && cat reference/routing.md
cd ~/.claude/skills/impeccable && cat reference/layout.md
cd ~/.claude/skills/impeccable && cat reference/adapt.native.md
cd ~/.claude/skills/impeccable && grep -n "^#\{1,4\} " reference/critique.md
cd ~/.claude/skills/impeccable && sed -n '106,200p;299,412p;620,651p' reference/critique.md
cd ~/.claude/skills/impeccable && for f in SKILL.md reference/audit.native.md reference/android.md \
    reference/layout.md reference/adapt.native.md reference/critique.md reference/routing.md \
    scripts/context.mjs; do echo "== $f"; sha256sum "$f"; stat -c '%s bytes  mtime=%y' "$f"; done

# Setup step 1, run once from the project root:
node ~/.claude/skills/impeccable/scripts/context.mjs
```

`context.mjs` resolved context:

```json
{ "targetPath": null, "projectRoot": "D:\\TRAVELAPPS\\ForgeShape",
  "productPath": "PRODUCT.md", "designPath": null, "surfaceBriefPath": null,
  "hasVisualImplementation": false, "platform": null }
```

It wrote nothing (`git status --porcelain` was empty immediately after). Its
`MANUAL_DETECTOR_REQUIRED` directive names `detect.mjs`, which `SKILL.md` and
`routing.md` both scope to web targets only ("`live` and the bundled
`detect.mjs` are web-only ... the browser overlay and the HTML rule engine do
not apply to native app code"). ForgeShape is a native Android app with no HTML
or CSS, so the mechanical detector is **N/A** for this run and was not run. That
is the skill's own instruction, not a skipped step.

## 4. Sections actually consulted

- `SKILL.md` — frontmatter/version; Setup 1-3; How to design; Modes; the Commands
  routing table (row `audit ... native: reference/audit.native.md`); Routing.
- `reference/audit.native.md` — whole file: the five scored dimensions
  (Accessibility, Performance, Appearance and Theming, Platform Conformance,
  Adaptivity), the 0-4 bands, the Platform Conformance Verdict, the P0-P3
  severity definitions, the per-issue documentation shape, Patterns and Systemic
  Issues, Positive Findings, Recommended Actions.
- `reference/android.md` — whole file: the Android slop test; Layout and
  structure; Touch targets; Typography; Color and theming; Components and motion;
  Verifying the build.
- `reference/layout.md` — Two isolated assessments (squint test, grouping,
  rhythm, structure, density, adaptation, extremes); Set the spatial thesis;
  Apply; Verify.
- `reference/adapt.native.md` — Assess Adaptation Challenge; Phone to Tablet;
  Orientation and foldables; Implement and Verify; the NEVER list.
- `reference/critique.md` — Design Health Score and mode applicability; the
  Priority Issues shape; Cognitive Load Assessment (three load types, the 8-item
  checklist, the Working Memory Rule, the 8 common violations); Score Summary;
  Issue Severity (P0-P3).
- `reference/routing.md` — the native carve-out for `detect.mjs` and `live`.

## 5. Rules taken from those sections, and what each decided here

| # | rule (paraphrased) | source section | ForgeShape screen / finding it decided |
| --- | --- | --- | --- |
| 1 | Every touch target is at least 48x48 dp, with at least 8 dp between targets. | android.md, Touch targets | Precision toggle measured 37.0 dp (compact portrait, precision surface open), 29.0 dp (short landscape sculpt), 17.1 dp (font scale 1.3) -> **I-01**. Grid On/Off chips 37.0/39.2 dp wide -> **I-06**. All four right-edge capsules separated by 4 dp -> **I-07**. Gizmo handles are 30 dp targets whose nearest centres are 47.2 dp apart -> **I-04**. |
| 2 | Layouts must not clip or overlap at large text sizes; check `font_scale 1.3` as part of the pass. | audit.native.md 1 Accessibility; android.md Verifying the build | Ran `settings put system font_scale 1.3`: the precision toggle collapsed to 17.1 dp and its active fill clipped square inside its pill -> **I-01**, artifacts `D03`, `D04`. |
| 3 | Text must pass contrast in every appearance shipped; role tokens must resolve legibly. | audit.native.md 1 and 3 | Measured `p1_text_secondary` at 3.93:1 and `p1_text_error` at 2.96:1, with the same secondary token failing in all three appearances (3.69-3.97:1) -> **I-05**. |
| 4 | Inputs must never be hidden behind the keyboard; IME insets must be applied. | audit.native.md 5 Adaptivity | With the IME open the transform-mode capsule was crushed to 2.3 dp and the space capsule and the precision toggle left the view tree entirely -> **I-02**, artifact `E01`. |
| 5 | System Back always works; never trap or hijack it. | android.md Layout and structure; audit.native.md 4 | One Back press with Add Primitive open exits to the launcher instead of closing the surface -> **I-03**, artifact `F03`. |
| 6 | Restructure for expanded width, never stretch a phone layout; drive it from window size classes. | adapt.native.md Phone to Tablet | At 1067x1707 dp the Objects capsule becomes a docked panel, the inspector docks as a 320 dp column, and everything fits with no scrolling -> **positive finding P-2**; expanded is the product's strongest window class. |
| 7 | Landscape restructures; it must not clip or letterbox. | adapt.native.md Orientation | Short landscape restructures correctly (rail entries 56x48 dp, selector row horizontal) but clips the precision toggle to 29 dp -> **I-01**. |
| 8 | Rhythm comes from deliberate contrast between tight and generous intervals; one value repeated everywhere flattens hierarchy. | layout.md Two isolated assessments (Rhythm), Apply | The right-edge column separates four semantically different capsules by the identical 4 dp (`row_gap_small`), so grouping carries no information -> **I-07**. |
| 9 | Group by meaning; use proximity before containers. | layout.md Apply | The "Display unit" chips sit against the Scale group, which is unitless by the product's own hard rule; they govern Position -> **I-09**. |
| 10 | The squint test must still reveal primary, secondary and the major groups in order. | layout.md Two isolated assessments (Reading order) | Squinting compact portrait leaves exactly one high-chroma object, the blue mode-transition pill, and the same blue is worn by Apply Transform, a commit -> **I-10**. |
| 11 | Extremes — long content, overlays, sticky elements, safe areas — expose structural failures. | layout.md Two isolated assessments (Extremes) | `-98765.4321098` clips at the **left**, hiding the minus sign, with no ellipsis or fade -> **I-08**, artifact `E01`. |
| 12 | Same type of action equals same type of UI; inconsistent patterns prevent learning. | critique.md Common Cognitive Load Violations 6; audit.native.md 4 System drift | The same six primitives are drawn as 2-column 76 dp icon tiles in Add Primitive and as 3-column 48 dp text-only chips in the Shape editor -> **I-11**. |
| 13 | At most 4 items at a decision point; 5-7 pushes the boundary; 8 or more overloads. | critique.md The Working Memory Rule | Transform in compact portrait presents 2 rail entries + 3 modes + 2 spaces + 1 toggle = **8 stacked controls** in one 500 dp column, plus 7 gizmo handles on the model -> **I-12**. |
| 14 | Always show current location; do not make the user build a mental map. | critique.md Common Cognitive Load Violations 3 | `editing_context_label` exists in landscape and expanded and is absent in compact portrait -> **I-13**. |
| 15 | Report issues with impact; never report a false positive without verification; record what works. | audit.native.md NEVER, Positive Findings | Six candidate findings were tested and **rejected** (see the report's Rejected section), including a modal-scrim leak I first mis-measured; eight positive findings are recorded. |

## 6. Skill-directed steps that were N/A, and why

| step | status | reason (from the skill text) |
| --- | --- | --- |
| `detect.mjs` mechanical scan | N/A | `routing.md`: web-only, the HTML rule engine does not apply to native app code. ForgeShape has no HTML or CSS. |
| Browser overlay / `live` | N/A | same. |
| `reference/craft-floor.md` | not loaded | `SKILL.md` Setup 3: do not load it for planning-only work. This audit edits nothing. |
| `reference/ios.md` | N/A | Android-only product; the repository forbids an Apple target. |
| Dual-agent assessment split | single-context, declared | The run was explicitly scoped Impeccable-only, read-only, with no second opinion. Declared here rather than left silent. |
