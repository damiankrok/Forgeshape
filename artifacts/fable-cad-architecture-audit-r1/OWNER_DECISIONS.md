# OWNER decisions vs engineering decisions (FABLE-CAD-ARCHITECTURE-AUDIT-R1)

Only product choices are put to the OWNER. Everything that is "which class
owns this" is listed under engineering and needs no OWNER input.

## A. OWNER PRODUCT DECISIONS — the top five, in the order implementation would need them

### D1 — How should the extrude controls scale and sit? (O1, blocks B2)

The audit's recommendation (`EXTRUDE_HUD_AUDIT.md` §4 C): the arrow gains a
technical-drawing **dimension leader** (extension lines, dimension line
parallel to the axis, end ticks) drawn as world geometry; the **value sits
above that line and rotates with it**, screen-readable inside a legibility
band; glyphs shrink with the model inside a wider band; every hit target stays
an invisible 48 dp proxy. Three numbers are the OWNER's, because no vendor
documents them and the physical device is the only judge:

- D1a. The visual band: how small may the head, ticks and glyphs get at
  zoom-out (today's floor is 0.80 of reference; a drawing feel needs something
  like 0.35–0.5) and how large near the camera (today 1.60).
- D1b. The value's legibility band (e.g. 11–16 sp) and its placement policy:
  above the line (ISO/metric default), or centred on it (ANSI). The audit
  recommends "above, aligned, upright past ±90°".
- D1c. What Tool Labels means under a drawing-style HUD: captions on the
  palettes and on focus only (recommended), or always beside each glyph at a
  fixed size (which reintroduces a screen-sized cluster).

### D2 — What does selecting regions mean? (O2, blocks B1)

The audit's recommendation (`PROFILE_SELECTION_AUDIT.md` §5): **a selection
is any set of atomic regions and the extrusion is their union**; every tap
toggles exactly the region under the finger and never changes another; a hole
is "filled" by selecting the disk inside it. For the rectangle + two circles:
ring + disk A → a plate with hole B. Two sub-choices:

- D2a. Confirm the union semantic (versus today's "one region may not stand
  beside its own hole", which no reference product has).
- D2b. Should the hatch show the merged area as one fill (recommended) or
  keep per-region hatching with the absorbed hole drawn differently?

No format change is needed; an older build refuses such a file by name. The
OWNER may prefer an explicit `CADB` v6 so an older build says "unknown
version" instead — taste, not correctness.

### D3 — Is a sketch a thing of its own? (O3, blocks any reuse; a data-model change)

Today a sketch is a field of its feature: it cannot be shown after commit,
pointed at, hidden, or driven by two features. Every reference product treats
it as a retained object with visibility (Fusion Browser, Inventor shared
sketch, Shapr3D Items, Onshape row). The OWNER's workflow step 7 ("create
another feature from the same sketch") requires:

- D3a. A sketch identity inside the body and features that REFERENCE it
  (`CADB` v6). Yes/no.
- D3b. Retained-sketch visibility on the canvas after commit: shown by
  default with a per-sketch hide (Shapr3D; recommended for a phone, because
  Onshape documents the "hidden sketch → tap selects the face" trap), or hidden
  by default (Fusion/Inventor/Onshape).
- D3c. Whether that visibility is project truth (persisted, one more `SCNE`-
  style flag) or session-only.

### D4 — What does a sketch on a face DO? (raised by FUNCTION-COUNCIL-R1; still open)

Today: a face of ANOTHER body → a new, following body (New Body only); a face
of the SAME body → Add/Cut offered. The canvas does not show which will
happen. Options: (a) keep both, and show it (the operation badge appears at
support-chooser time, or the chooser labels the face "this body / other
body"); (b) any face sketch offers New Body / Add / Cut into the face's body,
which makes cross-body Add/Cut a new feature kind. The audit recommends (a)
now and (b) only with a stated dependency rule, because (b) turns the acyclic
body graph into a multi-body boolean graph.

### D5 — Where do typed CAD values live? (blocks T1)

Today a base rectangle/circle/depth has TWO doors (the Shape panel's typed
fields, base-only, and Edit Sketch), and a later feature's depth has no typed
field at all. Options: (a) retire the Shape-panel size fields into Edit Sketch
and keep only a typed depth there per feature (Shapr3D's card model:
"the card is the editor"); (b) generalise the panel's fields to the selected
feature. The audit recommends (a): one door, and the feature list becomes the
card list.

## B. Further OWNER-level questions, not blocking

- D6. Should the feature list be reachable from the canvas (tap a face →
  its features, Shapr3D's "Related to selection") and shown even for a
  one-feature body? Recommended yes for both; it is chrome, not truth.
- D7. Commit by tapping empty canvas (Shapr3D) in addition to the Extrude
  control? Product feel; no architecture cost.
- D8. The standing questions carried over unchanged: OQ-01/OQ-02, the lean
  validation tiers, the JVM layout-test dependency, the next vertical slice
  (`artifacts/function-council-r1/NEXT_VERTICAL_SLICE_OPTIONS.md`). This
  audit adds: **B1 (union selection) and D3 (sketch identity) should precede
  Through All, Revolve and projected edges**, because all three would
  otherwise be built on a region rule and a sketch model that are about to
  change (`EXECUTIVE_SUMMARY.md` C10).

## C. ENGINEERING DECISIONS — no OWNER input needed

| Decision | Recommendation | Where |
| --- | --- | --- |
| Which class owns projection | stays native, `projectWorldToScreen`, one implementation | `CODE_ARCHITECTURE_MAP.md` §6 |
| One `metersPerPixel` for the extrude overlay and the HUD | read at `anchors.base`, once | `REFACTOR_SEQUENCE.md` S3 |
| How to expose a stable feature handle to Java | `featureId` (already stable); add a `sketchId` with D3 | `SKETCH_FEATURE_HISTORY_AUDIT.md` §5 |
| How to store a composite region selection | unchanged `ProfileRegionRef` list; union derived by parity | `PROFILE_SELECTION_AUDIT.md` §5.2 |
| How to split JNI | by family, debug TU first, `touchEvent` moved whole then restructured | `REFACTOR_SEQUENCE.md` B4, T4 |
| Which Java coordinator first | `ProjectLifecycleCoordinator` (clearest seam; D3-council lives there) | B6 |
| One session reset | `resetProcessScopedSessions()` from the union of the five lists; native `seedSculptProject` | B5 |
| `CADB` version mechanism | one ordered version table; no bytes change | B3 |
| Shared anchored value editor | one class for the three views; adds the missing `EditText` label | S7 |
| Dead exports, dead helpers, unguarded test exports | remove / guard | S1, S2 |
| Where the extrude annotation is drawn | the existing `Dimension` overlay range; no new pipeline | `EXTRUDE_HUD_AUDIT.md` §4 C |
| Feature-id high-water mark | add with `CADB` v6 | T9 |
| Documentation drift | the S6 list | S6 |
