# UI-CRAFT-R1 — the Android shell's look and feel, audited

OWNER brief (2026-11-04): "the app is the basis of CAD modelling that can
then be turned into Sculpt to add detail. Audit it for look, professionalism,
ergonomics and how intuitive the interface is, and fix the UI/UX, because it
looks like AI slop now."

**Method.** Source (`app/src/main/java/com/forgeshape/app`, `res/values`,
`res/drawable`) read in full for the shell, and the newest device captures in
the repository (`artifacts/function-council-r2/evidence/device-run-*/screens/`,
2026-11-03, API 36 emulator, Warm Graphite): Home, New Project, the first
sketch, a committed CAD body, the extent palette, Sculpt, a Freeform cage, a
Surface body with its precision sheet, Add Primitive. No device was available
to this session, so every change below is argued from the code and the
captures and is **UNVERIFIED on a screen** until the OWNER's review.

## 1. Findings

### A. The visual grammar — why it reads as "AI slop"

1. **Everything is a pill.** `radius_capsule` was 26 dp on a 56 dp-tall
   group, so every control group — the toolbar's three capsules, the Objects
   capsule, the history capsule, the brush column, the extent palette, the
   restore chip — drew the same half-round end, and the Tool Rail was a
   900-pixel pill. A single corner on everything is the one choice that most
   makes a shell look generated: nothing is sharper than anything else, so
   nothing is an instrument. Professional modelling tools (Shapr3D, Nomad,
   Onshape, Fusion, Plasticity) keep their chrome at 6–12 dp with a straight
   run between the corners.
2. **No edges.** Every floating surface was fill plus a 6 dp shadow and no
   hairline. On a dark viewport a translucent fill with a soft shadow has no
   edge; the capsules read as smudges and the precision sheet as a slab. The
   palette already HAD a hairline role (`fsChromeHairline`, approved per
   palette) that no floating surface used.
3. **Four radii on one screen with no hierarchy.** In the sketch capture the
   navigator wore the SHEET radius (24), the Modify chip the control radius
   (10), the rail the capsule (26) and the status pill 10: four unrelated
   corners in one composition.
4. **Deep shadows on a dark ground** (6 / 10 / 16 dp) read as dirt around
   each capsule rather than as depth.

### B. Composition defects

5. **A stray dot beside the ForgeShape mark.** `GlobalToolbarView.
   applyEditingComposition` painted the editing capsule around ZERO visible
   members — a Surface or Freeform body offers no transition — leaving an
   8 dp dot of padding beside the mark (visible in `03_first_feature_edited`
   and `01_freeform_box`). It reads as a broken indicator light.
6. The status line in the top-left corner is a quiet capsule by design; with
   the pill corner it read as a toast. (Kept; it takes the control radius and
   a hairline now.)
7. The rail's two icon-only entries (Cancel Sketch, the precision toggle)
   stand under seven captioned ones; they are owner designs and are not
   changed here, but see §3.

### C. The start pages

8. Home and New Project were correct in structure (full-window pages, two
   and four acts) but generic in execution: a wordmark with nothing under
   it, a heading reading "Start", two flat grey rectangles of prose, and a
   bare word "Settings" at the foot. The New Project copy on Home still named
   only CAD and Sculpt, although Freeform and Surface have existed since
   `MODELING-FOUNDATIONS-R1`.

### D. Ergonomics (what is already right)

9. The 48 dp floor is real everywhere and verified by the device suites;
   the rail keeps its glyphs in landscape; the brush sliders are 48 dp wide
   in the dimension a thumb misses in; the transition button never
   abbreviates critical navigation; the viewport is full-bleed. None of this
   needed touching and none of it was.

### E. Intuitiveness — recorded, NOT changed (OWNER decisions)

10. **New Sketch is behind the product mark.** A CAD-first product's most
    frequent creation act lives in the project drawer (an OWNER decision,
    `MODELING-R1-OWNER-CORRECTION`); a first-time user looks for it beside
    `+`. The face tap's *Sketch here* chip covers the common case once a
    body exists, but the drawer is the only route to a world-plane sketch.
11. **Export is the most prominent control in the utility group** for an act
    done once per session, while Display and Hide UI are icons beside it.
12. **"Body #n"** names every body by its id; a part, a sketch and a sculpt
    all read alike in the Objects capsule and the History.
13. The sketch grid's minor lines in the captures are nearly as bright as
    the major ones (native, `WORKBENCH-UX-R1` already quietened them after
    these captures were taken; judged on a real GPU).

## 2. What this stage changed

All of it is presentation. No `.forge` byte, history step, checkpoint,
fingerprint, test id or touch target moved; no colour was written in Java;
every new value is a theme role or a `dimens.xml` token, so all five palettes
and both handednesses take it unchanged.

- **Radii** (`dimens.xml`): control 10→8, floating 18→12, sheet 24→16,
  capsule 26→16; the concentric members follow by the existing rule
  (inset 22→12, rail entry 20→10, CAD HUD member 24→14). The hierarchy
  is now 8 / 12 / 16 on the 4 dp grid, and nothing is a pill.
- **Depth**: floating 6→3 dp, sheet 10→6 dp, chooser 16→12 dp.
- **Hairlines**: `bg_chrome_overlay` (tier 1), `bg_surface_context`
  (tier 2), `bg_inspector_sheet` (tier 3), `bg_chooser_panel`,
  `bg_sketch_navigator`, `bg_status_pill`, `bg_hud_lone_control`,
  `bg_pill_tonal` and `bg_start_page_action` draw a 1 dp
  `fsChromeHairline`. The selected-control fills are untouched (still fill
  plus indicator bar, no stroke — `UIR4B-14` holds).
- **The navigator** takes the floating radius, not the sheet's.
- **The empty editing group paints nothing** (finding 5).
- **Start pages**: a tagline under the wordmark (`start_page_tagline`), each
  action card with a 44 dp tonal icon well, the title, one line of
  description and a trailing chevron (hidden in the wide tile layout); the
  foot action carries its glyph (Settings the gear, Back the arrow); the
  Home and New Project copy shortened and brought up to date (Home now names
  all four representations). Ids and content descriptions are unchanged.
- `text_display` 21→22 sp.

## 3. Recommended next (each needs an OWNER decision; none is implemented)

1. A *Sketch* entry beside the six primitives in Add Primitive — the `+`
   becomes "add a shape or a sketch" — keeping the drawer's New Sketch as
   the second door. Changes an owner-decided surface.
2. Move Export into the project drawer's Transfer group and reduce the
   utility group to two icons, giving the transition button the row it
   competes for (`UIR4B-16`'s constraint disappears with it).
3. Caption the rail's Cancel and precision entries, or seat Cancel in the
   transition capsule (`CAD-R0-A1A2` decided against the latter).
4. Default names by representation ("Part 1", "Sculpt 1", "Freeform 1",
   "Surface 1") through `derivedBodyName`'s collision rule; Rename exists.
5. Judge the hairline's weight and the 3 dp shadow on the Galaxy Tab S9
   Ultra in all five palettes; both are one-token changes.
