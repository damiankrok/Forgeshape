# STAGE027-R0 — Recommended bounded contract

**This is a recommendation, not OWNER acceptance.** Every still-material fork is
marked `OWNER_DECISION_REQUIRED` and is left unresolved.

The historical phrase resolves, against live source, into **one** thing to
build, **one** thing already built, **one** thing that needs defining before it
can be scoped, and **two** defects the phrase did not name but that sit directly
in its path.

---

## 1. The recommendation in one paragraph

Stage027 delivers **Sculpt Isolate** — a transient, session-only, native-owned
viewport restriction to the body being sculpted, filtered inside
`ConstructionScene::snapshot()` so that drawn and picked stay one fact — plus
the two named guards that the current Sculpt workflow is missing
(`FINDING-A`, `FINDING-B`). It delivers **no new Hide**, because Hide is already
durable, versioned and correct. It delivers **no Mesh Preview** until the OWNER
says which of two concrete things that phrase means, because the renderer
cannot supply either of them without a change the OWNER should choose
deliberately.

---

## 2. Exact user-visible actions

1. **Isolate** — one control, available in Sculpt Mode only, with two states
   (Isolate / Exit Isolate). While on, the viewport shows the body being
   sculpted and nothing else.
2. **Nothing else.** No new Hide control, no per-row transient state, no
   preview toggle, no shading entry.

`OWNER_DECISION_REQUIRED — UI-1`: **where the control sits.** Two placements fit
the existing workspace grammar and they are not equivalent:

- **the Display popover**, beside `Grid` and `Selection Outline` — correct by
  ownership (it is a transient, native-owned viewport setting, exactly what that
  group is), but two taps away from a hand that is holding a brush;
- **the Sculpt Property Inspector** (`SculptContextView`), beside the mask and
  stale-source rows — correct by context (it is Sculpt's alone and appears
  nowhere else), and it is where the other Sculpt-only state already lives.

A third-party-style floating toggle over the viewport is **not** proposed: no
surface owns the resting workspace (`CLAUDE.md`).

---

## 3. Isolate — transient or durable

**Transient.** `VARIANTS.md` §1 Variant A, sub-shape **A2**: the exclusion is a
value `ConstructionScene::snapshot()` is given, not a session it reaches out to
read, so the single-list invariant is strengthened rather than forked.

Durable-visibility isolate (Variant B) is **rejected on source grounds**: it
writes `.forge` bytes and the fingerprint for a viewing choice, it needs
transient state anyway in order to restore correctly, it leaves a project
isolated across a crash, and it must weaken the existing
`objectCommandsBlockedByMode()` guard to run at all.

---

## 4. What Hide means, and which state it uses

**Hide is `ALREADY_IMPLEMENTED` and is removed from Stage027 implementation
scope.** It stays exactly what Stage 018A made it: `SceneObject::visible_`,
enforced in `snapshot()`, one `ScopedConstructionEdit`, persisted as `SCNE` v2
bit0. Stage027 creates no second visibility truth and adds no second per-row
control.

`OWNER_DECISION_REQUIRED — HIDE-1`: **should durable Show/Hide become reachable
from inside Sculpt** (`VARIANTS.md` §1 Variant C)?

- **For:** no new concept; the user may genuinely want a body gone for good, not
  just out of the way.
- **Against:** the guard's stated reason is still true — Undo in Sculpt means
  `SculptHistory` (`forgeshape_jni.cpp:5495-5512`), so a Construction step taken
  in Sculpt could not be undone until the user left the mode.
- **The audit's reading:** if Isolate lands, the *occlusion* need is met and
  Variant C is a convenience whose cost is reopening a guard. Recommend leaving
  the guard alone. **Not decided here.**

---

## 5. Exact meaning of Mesh Preview

**Unresolvable from source.** No product source defines it, and the two credible
readings need materially different work.

`OWNER_DECISION_REQUIRED — PREVIEW-1`: which does "mesh preview" mean?

| Option | What it is | What it costs |
| --- | --- | --- |
| **P0 — nothing** | It already exists as MatCap + Flat shading | zero; Stage027 drops it |
| **P1 — wireframe by polygon mode** | topology edges over the sculpt mesh | enable `fillModeNonSolid` at device creation (currently **no features at all** are enabled, `renderer.cpp:601-608`), a runtime feature query, a no-feature fallback, and the same decision re-made on device rebuild |
| **P2 — wireframe by CPU edge list** | same picture, drawn through the existing `LINE_LIST` pipeline | bounded by `kMaxSketchOverlayVertices = 65536` — a frozen sphere fits, a 4 M-vertex Imported Mesh does not; needs its own buffer/bound or a stated refusal, plus per-revision CPU work on the stroke path (a `PERF-BASELINE` risk) |

**The audit's reading: P0.** A wireframe is a real sculpting tool, but both
routes to it are larger than the rest of Stage027 put together, one touches
device creation and render recovery and the other touches the hottest path in
the product. It is a stage of its own, and it is the natural companion to
Stage028 density work rather than to an isolate. **Not decided here.** If the
OWNER wants P1 or P2, it should be split out — see `STAGING_PLAN.md` §5.

---

## 6. Lifecycle

| Event | Isolate state |
| --- | --- |
| Enter Sculpt | **off** — a mode is entered un-isolated |
| Isolate on, then a stroke | unchanged; a stroke is not a view event |
| Back to Construction | **cleared** — Isolate is Sculpt's alone, and a Construction scene showing one body would be a lie about the project |
| Resume Sculpt | **off**, not restored — it is a view decision, and a remembered one is a view the user did not ask for |
| Body switch | cannot occur in Sculpt (and `FINDING-A` is guarded below); if the OWNER later allows switching, Isolate clears |
| Freeze / Reset from source | unchanged — Isolate is not geometry |
| Rotation, window resize, HOME/resume | **survives**, like every other `DisplaySettingsStore` value |
| Save | `.forge` bytes identical whether isolated or not |
| Open / Recover / New Project | **off** — a fresh project is not isolated |
| Close project (Home) | **off** |
| Process death | gone; nothing persisted it |
| Device loss / render rebuild | survives — it is CPU state the GPU cannot reach |

`OWNER_DECISION_REQUIRED — LIFE-1`: **should Isolate survive Back → Resume?**
The recommendation above says no (a view decision, cleared on leaving). An
argument exists for yes (a user who isolated to reach a spot wants it back on
Resume). Both are defensible; the audit does not resolve it by preference.

---

## 7. History ownership

**Neither.** No `ProjectHistory` step, no `SculptHistory` entry. Isolate is a
view decision and both histories are about edits. Undo in Sculpt continues to
mean the last stroke; Undo in Construction continues to mean the last
Construction transaction. Neither gains a step that a user would have to step
past to reach real work.

## 8. Persistence ownership

**Runtime-only.** No `.forge` byte, no section, no version, no fixture, no
corpus change, no checkpoint, no fingerprint movement. A project reached with
Isolate on encodes byte-identically to one reached with it off, and
`SettingsPreferencesTest`-style byte equality is the way to prove it.

It is **not** an `AppPreferences` field: the Settings page owns persistent
preferences and a transient viewport restriction does not belong there — the
same rule that keeps `Grid` and `Selection Outline` out of it.

## 9. Selection and picking rules

- Picking follows Isolate **because it reads the same list**, not because a
  second rule says so. This is why the filter belongs inside `snapshot()`
  (`VARIANTS.md` §1 A2) rather than at the two consumers.
- The selection **does not move** when Isolate turns on. That is the rule
  `setSceneBodyVisible` already follows when the active body is hidden
  (`forgeshape_body_commands.h:135-141`), and Stage027 should not invent a
  second answer.
- The selection outline follows for free: the mask pass rasterises the snapshot.
- A tap on empty space behaves as it does today.

## 10. Construction vs Imported Sculpt behaviour

**Identical, and by construction rather than by a branch.** Isolate restricts a
list of `SceneDrawItem`s by `ObjectId`; it never asks what representation a body
has. A Construction Body, an Imported Mesh and (were it ever sculptable) a CAD
Body isolate the same way, exactly as `deleteSceneBody` and
`setSceneBodyVisible` are representation-neutral. No parity work is needed and
no parity test can fail for a reason the others cannot.

## 11. Minimum UI placement

One control, two states, 48 dp hit area, semantic id in `res/values/ids.xml`
naming the act (`sculpt_isolate`), accessible label stating the current state,
drawn only in Sculpt Mode and **absent** in Construction and Sketch — not
disabled, absent, because a control that cannot succeed is not drawn. Placement
is `OWNER_DECISION_REQUIRED — UI-1` above.

## 12. Renderer / resource changes

**None.** No new pipeline, no new shader, no new image, no new buffer, no new
descriptor, no push-constant slot, no device feature, no per-frame mesh work,
no upload, no `MeshRevision`, no CAD regeneration. The snapshot already copies
only `shared_ptr`s and matrices, so a shorter snapshot is strictly cheaper than
a longer one. `forgeshape_renderer.cpp` should not need to change at all — and
if a design draft requires it to, that is the signal the filter has been put in
the wrong place.

## 13. The two guards Stage027 should carry

These are not features, and the historical phrase did not name them. They are in
scope because Isolate is built exactly where they are missing, and because
shipping Isolate without them would let Isolate *appear* to fix one of them.

**G1 — the Sculpt target must be fixed against a viewport tap** (`FINDING-A`,
`CURRENT_TRUTH.md` §4.1). `forgeshape_jni.cpp:7283` calls
`constructionScene().setActiveBody(hit.objectId)` directly, bypassing the
`sceneSelectBody` guard that refuses in Sculpt. The fix is a named refusal on
that path, logged like every other. Isolate would mask this, and masking it is
the wrong outcome.

`OWNER_DECISION_REQUIRED — GUARD-1`: does a tap that misses the sculpt mesh
**clear the selection**, or change nothing at all? Today it would do both
(clear, then re-pick). The audit recommends *change nothing* in Sculpt, on the
grounds that the Objects row already refuses the same act — but the current
behaviour is a third answer and the OWNER should pick.

**G2 — Sculpt on a hidden body** (`FINDING-B`, `CURRENT_TRUTH.md` §4.2).
Neither `freezeToSculpt` nor `enterSculptMode` consults `visible()`. Three
possible answers, all coherent, and the audit does not choose:

`OWNER_DECISION_REQUIRED — GUARD-2`:
- **(a)** refuse Start/Resume Sculpt on a hidden body by name, and withdraw the
  control — consistent with "a control that cannot succeed is not drawn";
- **(b)** entering Sculpt **shows** the body as part of the transition — but
  that is a durable write for a mode change, which is the Variant B objection
  in miniature;
- **(c)** leave it, and let Isolate make it visible — **rejected by the audit**:
  Isolate restricts a list; it cannot un-hide a body, so (c) is not actually an
  answer.

## 14. Explicit non-goals

Stage027 does **not**: add a second visibility truth; add a per-row transient
hide; write any `.forge` byte, section, version or fixture; touch
`ProjectHistory` or `SculptHistory` semantics; add a second sculpt history;
change Stage025 mask semantics, history or persistence; add any brush, topology
mutation, subdivision, remesh, dyntopo or density work (Stage028); add stylus
pressure, tilt or hover (`STYLUS-G1` / Stage026); add multi-select, grouping,
nesting or reorder; add a focus-on-selection or any camera command; add a
material, colour, texture or UV; enable any Vulkan device feature; promote the
Imported Mesh Preview or any other diagnostic to a product feature; or allow
body switching in Sculpt.

---

## 15. Every open fork, collected

| Id | Question | Status |
| --- | --- | --- |
| `UI-1` | Isolate control in the Display popover or in the Sculpt Property Inspector? | `OWNER_DECISION_REQUIRED` |
| `HIDE-1` | Make durable Show/Hide reachable from Sculpt, or leave the guard? | `OWNER_DECISION_REQUIRED` |
| `PREVIEW-1` | Mesh Preview = P0 (nothing, already delivered), P1 (polygon-mode wireframe) or P2 (CPU edge list)? | `OWNER_DECISION_REQUIRED` |
| `LIFE-1` | Does Isolate survive Back → Resume Sculpt? | `OWNER_DECISION_REQUIRED` |
| `GUARD-1` | In Sculpt, does a tap that misses the mesh clear the selection or change nothing? | `OWNER_DECISION_REQUIRED` |
| `GUARD-2` | Start/Resume Sculpt on a hidden body: refuse by name, or show on entry? | `OWNER_DECISION_REQUIRED` |
