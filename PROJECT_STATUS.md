# ForgeShape — Project Status

**Status Version:** 0.25.0
**Updated:** 2026-08-24
**Result:** COMPLETE — Stage 017 is closed; the product is a multi-object scene
**Current Phase:** Phase 1 — Native Viewport
**Workspace:** `D:\TRAVELAPPS\ForgeShape`
**Accepted implementation baseline:** Stage 017 — multi-object scene +
hierarchy foundation, on top of the Pre-017 Correctness Repair (active
representation sidedness + re-Freeze guard + device verifier), Gate P1
(physical ARM64 closure), Stage 016-R2, Stage 016 (Plane), Stage 015D (camera
projection), Stage 015C-R (front-face culling), Stage 015C (shading), Platform
Fix P2, Stage 015B, Stage 014, the NDK r29 migration (Gate P0) and the owner
decision baseline.
**Next Stage:** Stage 018 — Object Commands.

This is a current snapshot, not a chronology. Per-stage verification chapters,
superseded environment states and old next-stage recommendations live in Git
history and are deliberately not repeated here.

## Stage 017 — Multi-object Scene + Hierarchy Foundation (COMPLETE)

The product is no longer one object. A platform-neutral `ConstructionScene`
owns an ordered collection of Construction Bodies; each body owns its own
Construction Source, its own placement, its own published mesh chain and its
own Frozen Sculpt Mesh, and the renderer, CPU picking and the Objects UI all
iterate the scene instead of assuming a single global object.

**What replaced the singletons.** Before this stage "the object" was three
process-global accessors — `constructionObject()`, `meshStore()` and
`sculptSession()` — and whatever each of them happened to hold. A body is now a
`SceneObject` owning a `ConstructionObject` (which already carried its own
`ConstructionTransform`), a `MeshStore` and a `FrozenSculpt`. The three
accessors survive with their original names but are **redefined as "the ACTIVE
body's"**, in `forgeshape_scene.cpp`. That is what kept the migration small:
every existing caller that meant "the object the user is editing" kept working
unchanged and correctly, and only the code that means "every body in the scene"
— the renderer and scene picking — had to be rewritten.

**ObjectId.** Monotonic, minted by the scene, never reused, and never derived
from a collection index, a `MeshRevision` or any GPU resource. It survives
primitive edits, transform edits, Freeze/Resume/re-Freeze and selection
changes. The first body keeps `kConstructionBoxObjectId`, so startup is
byte-for-byte the single-object product's: one default Box at identity,
already selected. Stage 017 has no delete, so there is deliberately no reuse
policy to design.

**Publication is per body.** Each body owns a `MeshStore`, so revisions are
per-body chains that start at 1 and an edit to A cannot replace, invalidate or
renumber B's published mesh. `ConstructionScene::snapshot()` returns an
immutable `SceneSnapshot` — per item an ObjectId, a `RuntimeMeshPtr`, a model
and inverse-model matrix, and a selection flag. It copies a `shared_ptr` and
two matrices per body and **no geometry**, so it is cheap enough to take under
the existing state mutex and then be used with that mutex released; old
snapshots stay valid because they hold the revisions they name alive.

**The renderer draws the scene.** The per-object GPU state that used to be flat
`Renderer` members — device-local vertex/index buffers and capacities, the
`RenderMeshCache`, and the uploaded/failed revision and shading — moved into
`BodyRenderResources`, held in a map keyed by stable ObjectId (never by scene
index, which would rebind a body's buffers to a different body if the
collection were ever reordered). Staging, the upload command buffer and the
upload fence stay shared: they are transient scratch used inside one upload.
`syncScene()` runs the existing per-frame gate once per body, and
`recordBodyDraw` issues one draw per body with that body's own model matrix and
its own selection flag. The global `bool selectionHighlight_` is gone; it would
have tinted every body at once the moment anything was picked.

Body independence is therefore structural rather than a promise: an edit to A
mints a revision in **A's own** store, so B's cached revision still equals B's
published revision and B's branch returns before rebuilding or uploading
anything. `FORGESHAPE_RENDER_MESH_BUILD` and `FORGESHAPE_MESH_UPLOAD_OK` gained
a `body=` field (appended, so existing evidence tooling keeps parsing) because
with several bodies an upload line is otherwise ambiguous about which one it
describes — and that is exactly the evidence this claim rests on.

**Picking iterates the scene.** `pickScene` takes a snapshot, intersects each
item with **its own** transform and **its own** sidedness, and keeps the nearest
positive hit; ties keep the earlier body in scene order. Distances are directly
comparable across bodies because every Construction transform is rigid. The
Pre-017 invariant is unchanged and now doubly load-bearing: sidedness comes from
the active published mesh, never from a `PrimitiveKind` — the active body's kind
says nothing whatever about a *different* body's geometry.

**Sculpt: what is per body and what is not.** `SculptSession` was per-object in
the first cut of this stage, and that was wrong twice over: the product mode
became ambiguous (`productMode()` answered for whichever body was active, so
switching to a body that was itself in Sculpt mode refused every later
selection), and the documented "Radius and Strength are shared" contract would
have broken silently, because switching bodies would have switched brushes. The
split is now explicit. **Per body** (`FrozenSculpt`, owned by `SceneObject`):
the Frozen Sculpt Mesh and its stale flag. **Global** (one `SculptSession`):
the product mode, the held tool, the brush radius and strength, the stroke in
progress and the session-lifetime stroke count. `sculptSession()` re-points the
session at the active body's `FrozenSculpt` on every access — one pointer
write, and it removes the whole class of bug where the session is left pointing
at the body the user just navigated away from.

Freeze, Resume, re-Freeze, the stale-source flag and the current-mesh
`hasEdits()` predicate are therefore all per body, exactly as Pre-017 defined
them. Body switching is refused while in Sculpt mode: the Sculpt target is
fixed for the duration, and the user returns to Construction to change bodies,
which avoids having to decide what a body switch does to a half-finished
stroke.

**Objects UI.** A minimal `Objects` section at the top of the Construction
inspector: one row per root body labelled `Body #<ObjectId>`, active
indication, and one `Add Body` action. The Java layer holds **no** body list
and **no** model selection — every refresh re-reads native state, which is what
keeps the list, the Inspector and the viewport from disagreeing. Rows share the
`object_row` id (a per-body id cannot exist at build time) and identify their
body by tag; tests select by that tag, never by row position. A viewport pick
also makes the hit body active, and the workspace re-reads when a gesture
settles.

**One defect found and fixed during the runtime walkthrough.** The chrome
tracked the last active body only inside the gesture listener that consulted
it, so Add Body left it stale; a later viewport pick that landed back on the
stale value compared equal and skipped the refresh, leaving the Inspector
showing another body's numbers. It is now recorded in `onNativeStateChanged()`
— the one place every surface re-reads — so the comparison means what it says.

**Scope.** Flat root-level collection only: deterministic insertion order,
stable enumeration, no parent/child, no groups, no reparenting, no rename, no
reorder, and no speculative parent field anywhere. No delete, duplicate, hide
or lock. No Undo, no command framework, no persistence, no import/export
change, no dirty ranges and no spatial acceleration.

**Verification.** Eleven native suites, **1530 checks, zero failures** (the new
scene suite is 79 checks, S17-01..20); 26/26 JVM; **56 instrumented, 55 green**
— the one failure is `ui11`, the documented environment-dependent IME case,
**proven pre-existing** by stashing every Stage 017 change and reproducing the
identical failure on `d04e0b4`. DEV2-01..07 + DEV3-01..06 all PASS.

The scene suite builds its **own** `ConstructionScene` per case rather than
touching the process-scoped one — the Stage 016-R2 lesson applied to the scene,
and possible only because `ConstructionScene` is an ordinary class with no
hidden global state. Teeth were verified by reverting: making the highlight a
global "something is selected" fails both S17-08 checks, and replacing the
nearest-hit rule with first-hit-wins fails both S17-10 checks.

**Runtime**, on `ForgeShape_Stage006` / `emulator-5580` through the real touch
path. Two bodies render at once with independent placements, and **only the
selected one is tinted**. Alternating viewport picks return
`FORGESHAPE_PICK_HIT:1:…` and `:2:…` and the Inspector's Pos X follows
`0 → 2 → 0`. Editing body 1's width 2 → 4 published exactly one revision
(`objectId=1`), caused exactly one `RENDER_MESH_BUILD body=1` and exactly one
`MESH_UPLOAD_OK body=1`, with **zero** of either for body 2 — and an
accidentally triggered debug stress harness that republished body 1 forty-seven
times in a row still produced zero work for body 2. Per-body sidedness in one
scene, two consecutive builds: `body=1 src=482:2880 render=482:2880` (not
doubled) beside `body=2 src=4:6 render=8:12` (doubled). The mandatory round
trip completed through the product's own controls: Freeze A → real Grab stroke
(`STROKE_BEGIN:grab:91`, A reaching `sculptRev=40`) → Back → select B → Freeze
B → real stroke (`STROKE_BEGIN:smooth:91`, B reaching `sculptRev=45`) → Back →
select A → **Resume A returns `sculptRev=40`, `objectId=1`, `freezes=1`,
`storeRev=114`** while B keeps its own. Projection changes and Studio↔MatCap
caused zero rebuilds; Smooth↔Faceted rebuilt each body exactly once. The Plane
body picked from the front and, after orbiting to `pitch=-1.5200`, from behind.
HOME/resume reproduced the identical `ActivityRecord` with no self-test rerun
and zero uploads; rotation held `1080x2400 → 2400x1080 → 1080x2400` at
`preTransform=0x1`; both bodies survived both.

**P17 criteria.**

| ID | Verdict | Evidence |
| --- | --- | --- |
| P17-01 | PASS | clean tree at `d04e0b4` audited before any change |
| P17-02 | PASS | `ConstructionScene`, platform-neutral, insertion-ordered; S17-02/20 |
| P17-03 | PASS | monotonic ids, independent of index/revision/GPU; S17-02/03/04 |
| P17-04 | PASS | one default Box at identity, already selected; S17-01 |
| P17-05 | PASS | Add Body appends and selects; S17-02, S17-21, runtime |
| P17-06 | PASS | order stable across edits, selection and publication; S17-20 |
| P17-07 | PASS | primitive and transform edits touch only the active body; S17-03/04 |
| P17-08 | PASS | per-body `MeshStore`; immutable snapshot; S17-05 |
| P17-09 | PASS | two bodies drawn with independent transforms; runtime screenshot |
| P17-10 | PASS | one build + one upload, both `body=1`, zero for body 2; S17-07 |
| P17-11 | PASS | only the selected body is tinted; S17-08 (teeth-verified) |
| P17-12 | PASS | nearest correct ObjectId; S17-09/10 (teeth-verified) |
| P17-13 | PASS | `body=1` undoubled beside `body=2` doubled; S17-11; Plane front/back |
| P17-14 | PASS | A/B independent Freeze/Resume; S17-13/14/17, runtime round trip |
| P17-15 | PASS | stale-source and `hasEdits()` per body; S17-15/16 |
| P17-16 | PASS | A→B→A returns A's own mesh and revision; S17-17, S17-25, runtime |
| P17-17 | PASS | a stroke on A leaves B's source, mesh and revision untouched; S17-18 |
| P17-18 | PASS | picking suite still 128/128; S17-12 re-asserts it inside a scene |
| P17-19 | PASS | S17-01..20 green (79 checks) |
| P17-20 | PASS | S17-21..26 green within 56 instrumented |
| P17-21 | PASS | native 1530/1530, JVM 26/26, instrumented 55/56 (1 proven pre-existing), both ABIs build |
| P17-22 | PASS | lifecycle, rotation, both projections and both shading models green |
| P17-23 | PASS | no delete/duplicate/group/Undo/Sketch/persistence/BVH/dirty-range work |
| P17-24 | PASS | see doc line counts below; PROJECT_STATUS compacted |
| P17-25 | PASS | one focused commit, clean tree |

**Result: COMPLETE.** P17-01..25 PASS.

## Closed stages — durable facts only

Full narrative for every stage below lives in Git history. What is kept here is
only what still constrains the code.

**Pre-017 Correctness Repair (COMPLETE).** Two contract defects and one
verifier weakness.

*Sidedness ownership.* `SculptMesh::freezeFrom` did not copy
`ConstructionMesh::renderBothSides`, `publishSculptMesh` took the `false`
default so every Frozen Sculpt Mesh published single-sided, the Sculpt
hit-test hard-coded `frontFacesOnly=true`, and `pickScene` asked
`constructionObject().kind()`. That last is wrong in both directions once a
frozen mesh outlives its Source. **The rule now: sidedness is a property of the
active published representation**, carried `ConstructionMesh::renderBothSides`
→ `SculptMesh::renderBothSides()` → `RuntimeMesh::renderBothSides()`, and read
from there by render, selection picking and the Sculpt hit-test alike.
`forgeshape_selection.cpp` deliberately does not include
`forgeshape_construction.h`. Asserted by `SIDE-01`..`09`.

*The destructive re-Freeze guard.* It read the session-lifetime stroke count,
which is never reset by a Freeze, so every later re-Freeze of an untouched mesh
raised a dialog with nothing behind it. **The predicate is now
`SculptMesh::hasEdits()`** — `revision > kFrozenSculptRevision`, restarted by
every `freezeFrom` and advanced only when a stroke actually moved a vertex, so
an abandoned gesture correctly reports no edits. Exposed as `SCULPT_HAS_EDITS`;
the dialog quotes no number, because any count available describes meshes that
no longer exist. Asserted by `REFR-01`..`07`. UI-OWNER-05 intact.

*The device verifier* gained `DEV3-01`..`06` on top of the unchanged
`DEV2-01`..`07`: it now recognises a bare `adb` in any form, scans
`.ps1`/`.cmd`/`.bat`/`.sh` and the Gradle files, detects an executable
`connected*AndroidTest` fan-out, proves an argument array actually carries `-s`
instead of trusting its variable name, and reports which surfaces it scanned so
a check that covered nothing cannot pass. It also fixed a latent vacuous pass:
`Where-Object` returning exactly one object has no `.Count`, so the script
printed "All checks PASS" and exited 0 whenever exactly one check failed.

**Gate P1 — physical ARM64 (COMPLETE).** Closed on a Samsung Galaxy S25 Ultra
(`SM-S938B`, Snapdragon 8 Elite, Android 16 / API 36, 1440×3120), attached over
Wi-Fi adb; its serial and address are session facts and are deliberately not
recorded here. `ro.product.cpu.abi` and `abilist` report **`arm64-v8a`** only,
and after install `dumpsys package` reports **`primaryCpuAbi=arm64-v8a`**. That
phone's `getconf PAGE_SIZE` is **4096**, which is the ordinary case and not a
failure: the 16 KB dimension was closed separately on a dedicated 16 KB target
(`ForgeShape_16K`, `PAGE_SIZE` = 16384), and the two pieces of evidence are
complementary.

*The ARM64 picking defect, and why the tolerance exists.* Six self-test checks
failed on the physical device against 1421/1421 green on x86_64 from identical
source. `intersectRayTriangle` tested barycentric containment with exact bounds,
and a ray landing on an edge two triangles SHARE has a coordinate that is
mathematically exactly 0 — its sign decided purely by rounding, and if it
rounds negative for one triangle it does for its neighbour too, so both reject a
ray that geometrically hits. The rounding is ABI-dependent: no
`-ffp-contract=off` is set, so Clang contracts the dot/cross products into fused
multiply-adds on arm64-v8a, which baseline x86-64 cannot. An on-device probe
measured the failing ray at **u = -9e-9**. Product-visible, not a test artefact:
the centre of a Plane *is* the shared diagonal of its two triangles, so tapping
the middle of a Plane selected nothing. Fixed by `kBarycentricEpsilon = 1e-6f`
applied to both containment bounds — widening rather than pinning the FP model,
which would only re-hide the same knife-edge geometry. **Do not remove or
tighten it.**

*Heavy-mesh ladder, measured on that phone.* Single-run figures from one device;
they are existence evidence that the mandatory tiers work on physical ARM64, not
a supported capacity limit and not a statement about any other device.

| tier | vertices | triangles | gen ms | publish ms | render build ms | pick | PSS |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ~10k | 10086 | 19200 | 2.763 | 1.695 | 19.577 | tri 15592 | 219 MB |
| ~50k | 49686 | 97200 | 17.776 | 7.880 | 72.204 | tri 78824 | 231 MB |
| ~100k | 99846 | 196608 | 32.405 | 15.426 | 104.387 | tri 159210 | 248 MB |

Sculpt at ~100k: freeze 110.126 ms building 592896 adjacency entries, two real
Grab strokes, every post-stroke upload `reuse`, PSS 264 MB. No crash, OOM, ANR
or thermal symptom at any tier. The last stable mandatory tier is ~100k.
250k/500k were not run. Stylus/S Pen remains **UNVERIFIED** — closing it needs a
person physically moving a pen.

*Also closed under Gate P1:* arm64-v8a packaged beside x86_64, both `.so`s at
0x4000 ELF `LOAD` alignment with `zipalign -P 16 -c` OK; a debug-only Vulkan
validation session with **zero ForgeShape-caused messages** of any severity
(the layer was adb-pushed through Android's own per-app GPU debug layer
settings and never bundled, never committed, and removed afterwards); and the
debug-only `buildStressMesh` density fixture behind `ForgeShapeActivity` keys
A–F.

**Stage 016-R2.** Self-tests must not depend on live process-scoped state: three
cases were reading whatever primitive or transform a prior UI test had left, and
now publish their own fixture and pick through the explicit-transform overload.
`scripts\start-forgeshape-emulator.ps1` takes an explicit `-Avd`/`-Port`, hard-
rejects port 5554 before any OS or adb call, and BLOCKS rather than falling back
when the requested port is occupied. Every adb call in every repo script is
scoped to an explicit serial.

**Stage 016 — the Plane.** The sixth and final Construction MVP primitive: a
flat, zero-thickness rectangular sheet authored by width (local X) and depth
(local Z), centred on the local origin at `y = 0`, canonical front along local
`+Y`. Source topology is exactly **4 vertices, 2 triangles, 6 indices**
whatever the dimensions, CCW seen from the front. Two-sided render and pick are
one bounded, named exception carried by the published mesh — see the Pre-017
entry above for who owns that fact now. `ConstructionObject::setPrimitive`
`std::visit`s the requested spec, so a kind added to the variant with no
matching overload is a compile error rather than a silently skipped branch.

**Stage 015D — camera projection.** Orthographic is a true parallel projection
(`mat4Orthographic`, `m[11] = 0`), not a narrow FOV or a huge distance. The
existing 60° Perspective camera was audited and left unchanged. Switching
converts the framing rather than resetting it
(`orthoHalfHeight = distance × tan(fovY/2)` and its inverse), so the frame never
jumps. Orthographic pinch changes the world span and deliberately leaves the
orbit distance alone, because moving the eye along its own axis changes nothing
in a parallel projection. Picking is structurally different per projection —
Perspective keeps one origin with fanning directions, Orthographic shares one
direction with a per-pixel origin — and the orthographic view plane is pulled
back to `kFarPlane/2`, so `snapshot.eye` is **not** `target + dir × distance` in
Orthographic. The sculpt brush radius must not scale with depth in Orthographic.

**Stage 015C-R — winding and culling.** The pipeline named
`VK_FRONT_FACE_CLOCKWISE`, which double-counted the projection's Y flip and
inverted back-face culling, so every convex primitive drew its far walls and
read as a hollow interior. The convention is now
`VK_CULL_MODE_BACK_BIT` + `VK_FRONT_FACE_COUNTER_CLOCKWISE`, owned by
`ARCHITECTURE.md`. The `NOR-01`..`10` direction family exists because every
earlier normal check measured an axis or a magnitude and so passed unchanged on
a mesh whose normals had all been negated.

## Owner Decision Baseline

Active owner decisions are identified by `UI-OWNER-*`, `ARCH-OWNER-*`,
`INPUT-OWNER-*` and `DOC-OWNER-*`. Recorded 2026-08-20.

**Implemented by Stage 015B:** UI-OWNER-01 (the shell), UI-OWNER-02 (compact /
medium / expanded), UI-OWNER-03 (Export as a global action, drawn and reserved),
UI-OWNER-05 (the destructive re-Freeze guard) and UI-OWNER-06 (stylus-friendly,
no pressure). **Still decisions only, not behaviour:** UI-OWNER-04 — Sketch and
Extrude have visible, disabled homes in the Tool Rail and no implementation
whatsoever.

The historical bare `D1`–`D6` tables from Stage 015A and Stage 015A-R are
**superseded and non-authoritative**, and the two tables did not even mean the
same things, which is why bare `D` numbers were retired. No stage gate,
acceptance table or preflight may cite a bare `D` number again.

| ID | Decision | Approved value |
| --- | --- | --- |
| UI-OWNER-01 | Shell family | **Forge Shell** — a modernized viewport-first shell: direct edge controls and a Tool Rail for Sculpt, the same shell language plus a contextual exact-value Property Inspector for Construction. One shell; mode and tool decide content, never structure. |
| UI-OWNER-02 | Device scope | **phone + tablet** — Stage 015B implements adaptive compact / medium / expanded behaviour now. A tablet may dock more surfaces but must not become desktop-CAD clutter. |
| UI-OWNER-03 | Export placement | **global action** — Construction, Sculpt and UV remain editing contexts; Export is not one of them and later opens its own output surface. |
| UI-OWNER-04 | Sketch + Extrude | **approved as an iterative CAD workflow**, not implemented and not part of Stage 015B geometry. See below. |
| UI-OWNER-05 | Confirmation before a destructive re-Freeze | **yes**, and **only** when existing Frozen Sculpt Mesh edits would actually be replaced. A normal Resume Sculpt has no confirmation — it destroys nothing, and guarding it would train the user to dismiss the guard that matters. |
| UI-OWNER-06 | Stylus pressure in Stage 015B | **no** — deferred to a dedicated Sculpt stage. 015B must still be stylus-friendly and preserve a clean path for pressure, tilt and hover. |
| ARCH-OWNER-01 | Future Apple portability | **required architectural constraint**. See below. |
| INPUT-OWNER-01 | Stylus-first interaction | **required product/architecture constraint**. See below. |
| DOC-OWNER-01 | Clear naming and code comments | **required documentation/maintainability rule**, recorded durably in `CLAUDE.md`. |

**UI-OWNER-04 in detail.** A Construction Body may begin either from an exact
primitive **or** from a 2D sketch: standard sketch planes (later on planar
Construction faces), multiple sketches on different planes, Line/Polyline,
Rectangle, Circle, select/delete, grid and snap, exact numeric entry, closed-
profile detection and validation, and repeated Sketch → Extrude workflows. The
first Extrude vertical slice supports **New Body**; Extrude **Add** and **Cut**
are required integration **after** boolean infrastructure exists. The result stays
Construction source and history until an explicit Freeze to Sculpt. **This
approval is not permission for** a geometric constraint solver, assemblies,
NURBS, engineering drawings, Revolve, Fillet, Chamfer or Shell — each needs its
own approval.

**ARCH-OWNER-01 in detail.** Android remains the **first production platform**.
Construction, Geometry, Sculpt and domain code stay platform-neutral C++; Android
`View`/`Activity`/JNI types never become domain truth; the Android UI is a
platform shell/adapter; input crossing the boundary moves toward semantic,
platform-neutral pointer/tool samples; future file and platform services sit
behind narrow boundaries; renderer/platform-surface coupling stays explicit.
**Not authorized now:** an iOS/iPadOS application, an Xcode project, a Metal
backend, a MoltenVK dependency, or a cross-platform UI framework. A future Apple
UI may be native to Apple — sharing Android View code is not a goal — and the
Apple render path is deliberately undecided, belonging to a later evidence-based
spike. The architectural consequences are owned by `ARCHITECTURE.md`.

**INPUT-OWNER-01 in detail.** ForgeShape must stay comfortable for finger input,
for an Android stylus/S Pen, for tablets and for a future Apple Pencil: touch
targets practical for both a stylus tip and a fingertip, a shell that does not
unnecessarily cover the model, explicit gesture ownership between viewport,
chrome and tool, and a semantic input boundary able to carry pressure, tilt,
hover and tool type. Stage 015B does **not** change Sculpt deformation from
pressure.

**DOC-OWNER-01 in detail.** The rule itself lives in `CLAUDE.md`; the fixed UI
vocabulary is *Editor Workspace*, *Global Toolbar*, *Tool Rail*, *Property
Inspector*, *Construction Body*, *Frozen Sculpt Mesh*. Existing working code is
not mass-renamed for it.

## Product Direction

ForgeShape is an author-owned offline 3D modeling/sculpting application.
Production architecture must NOT use Godot, GDExtension, Unity, Unreal or any
other general-purpose engine that owns the viewport or render loop.

## Environment and toolchain

| | pinned / installed |
| --- | --- |
| JDK | 21.0.9 (Android Studio JBR, `JAVA_HOME`) |
| Android SDK | `C:\Users\damia\AppData\Local\Android\Sdk`; platforms 33/34/35/36/36.1 |
| Build tools | 36.1.0 in use (27.0.0, 33.0.3, 35.0.0, 35.0.1 also installed) |
| **NDK** | **pinned to `29.0.14206865`** in `app/build.gradle`. 25.2.9519653 and 27.2.12479018 are also installed but unused. An r30 beta is prohibited. |
| CMake | 3.22.1 in use (4.1.2 also installed) |
| Gradle / AGP | 8.14.3 wrapper / 8.13.2 |
| SDK levels | compileSdk 36, targetSdk 36, minSdk 26 |
| ABI filter | `x86_64` (emulator) + `arm64-v8a` (physical devices, Gate P1) |
| C++ / STL | C++17, `c++_static`; one packaged `.so`, `lib/x86_64/libforgeshape_native.so` |
| Shaders | `glslc` at `<ndk>/shader-tools/windows-x86_64/glslc.exe`, AOT from CMake |
| System images | only `system-images;android-36.1;google_apis_playstore;x86_64` |

No Godot/GDExtension files remain in the workspace.

**Git.** The root repository is initialized, **local only**: no remote, nothing
pushed, and no global Git config — the committer identity lives in `.git/config`
alone. Baseline commit `5d386f03e7b33de47ea717a4a1d629231b073b56`
(`baseline: accepted Stage 014`, 171 files). `.gitignore` excludes Gradle/CMake/
NDK output, `.cxx`, `.gradle`, IDE state, `local.properties` and APK/AAB output.
It deliberately does **not** ignore `artifacts/`, which holds the runtime
evidence screenshots cited by past stage acceptance.

### Android runtime targets

| AVD / serial | Status |
| --- | --- |
| `Medium_Phone_API_36.1` / `emulator-5554` | **Reserved by another program.** ForgeShape must not use, start, stop, wipe, reconfigure, install to, send input to, log or screenshot it until the owner lifts this. |
| `ForgeShape_Stage004` / `emulator-5556` | **Contended, non-authoritative.** Another program runs `com.damian.wlochyikafalonia.claude.debug` on it, steals the foreground and injects taps that reach ForgeShape. Do not use it for authoritative evidence; do not stop, wipe or reconfigure it. |
| `ForgeShape_Stage006` / port varies, boot with `scripts\start-forgeshape-emulator.ps1` (default port `5580`) | **Current ForgeShape-owned evidence target.** Isolated, own AVD definition and data dir. Always confirm identity with `adb -s <serial> emu avd name`, never by port alone; this AVD has been seen on `5556`, `5558` and `5580` across sessions purely by allocation order. |
| `ForgeShape_16K` / port varies, boot with `scripts\start-forgeshape-emulator.ps1 -Avd ForgeShape_16K -Port <explicit>` | **Gate P1's 16 KB runtime target.** x86_64, `google_apis_playstore_ps16k`; `getconf PAGE_SIZE` = 16384. Not a substitute for physical ARM64 — same isolation rules as `ForgeShape_Stage006` apply (explicit port, confirm identity by name, never `5554`). |
| **Physical ARM64 phone** — owner-supplied, attached over Wi-Fi adb; serial supplied per session and deliberately not recorded in this repo | **Gate P1's physical ARM64 evidence target.** Samsung Galaxy S25 Ultra (`SM-S938B`), Snapdragon 8 Elite (`SM8750`), Android 16 / API 36, 1440×3120, `arm64-v8a` only, `getconf PAGE_SIZE` = 4096. Vulkan: swapchain format 37, 5 images, FIFO. `/system/bin/uinput` is usable from the adb shell (the shell user is in the `uhid` group), so real multi-touch injection works. Same rules as every other target: explicit `-s <serial>` on every command, confirm ForgeShape is resumed before evidence, never a bare `adb devices`. |

`ForgeShape_Stage006` is pixel_6, 1080×2400, density 420, multi-touch, GPU host,
`x86_64`, API 36 (`google_apis_playstore`). Vulkan: loader instance 1.4.0,
physical device "Goldfish GFXStream (AMD Radeon RX 9070 XT)", device API 1.3.0,
swapchain format 37 (`R8G8B8A8_UNORM`), 4 images, FIFO. `ForgeShape_16K` shares
the same profile and Vulkan stack on the `google_apis_playstore_ps16k` image.

Rules that apply to every run:

- Every `adb` command names its target explicitly: `adb -s <serial> ...`.
- Launch the emulator **detached** (e.g. WMI `Win32_Process.Create`); it dies if
  spawned as a child of a tool shell.
- Before any evidence-sensitive input or screenshot, confirm ForgeShape is the
  resumed activity; invalidate any run contaminated by foreign input.
- Creating a new AVD from already-installed tooling is allowed; installing or
  updating SDK/NDK/JDK/system images is not.
- `adb root` is unavailable on Play Store images, so `sendevent` injection is
  impossible; multi-touch is injected with the on-device `uinput` tool, which
  consumes concatenated JSON objects (no array, no commas).

## Architecture

`ARCHITECTURE.md` is the authoritative ownership map. In one paragraph: a
standalone Android application (`com.forgeshape.app`) with a Java shell that owns
no domain truth, a plain `SurfaceView` viewport, a JNI boundary that carries
whole sections and semantic pointer samples, and a platform-neutral C++17 domain
(`ConstructionObject` and its six primitives, `ConstructionTransform`,
`SculptSession` and its one brush kernel, `MeshStore`, picking, selection,
camera) beneath a native Vulkan renderer that owns no geometry truth. No Compose,
no AndroidX, no third-party runtime library, no engine.

## Verified capability matrix

Everything below is verified at runtime on `emulator-5558` through the real
Android touch path. `PRODUCT.md` owns the user-facing description.

| Capability | State |
| --- | --- |
| Vulkan viewport, swapchain, depth, pipeline, indexed draw, surface recreation | VERIFIED |
| Camera: orbit / pan / pinch, pointer-set re-anchoring, cancel handling | VERIFIED |
| Perspective (60° FOV) and true Orthographic projection, switchable | VERIFIED |
| Switching projection preserves target-plane framing; the frame does not jump | VERIFIED |
| Orthographic pinch changes the world span, not the orbit distance | VERIFIED |
| Picking correct in both projections; ortho ray origin moves per pixel | VERIFIED |
| Sculpt hit and brush radius correct in both; ortho radius is depth-independent | VERIFIED |
| Projection change mints no revision and touches no geometry truth | VERIFIED |
| Projection mode and framing survive HOME/resume and surface recreation | VERIFIED |
| Tap-to-select, tap-to-clear, drag and multi-touch never select | VERIFIED |
| CPU picking follows camera, dimensions, transform and sculpt deformation | VERIFIED |
| Dynamic mesh: immutable revisions, fail-closed validation, capacity reuse/growth | VERIFIED |
| SEVERAL Construction Bodies in one scene, each with exact Box / Cylinder / Sphere / Cone / Capsule / Plane | VERIFIED |
| Stable per-body ObjectIds, independent of collection index, mesh revision and GPU allocation | VERIFIED |
| Add Body appends and selects; deterministic insertion order across edits and selection | VERIFIED |
| Primitive and transform edits reach only the active body; A↔B round-trips the exact spec and placement | VERIFIED |
| Per-body mesh publication: each body its own revision chain; A's edit cannot replace B's mesh | VERIFIED |
| Renderer draws every body with its own transform and its own GPU buffers | VERIFIED |
| Editing A rebuilds and uploads nothing for B | VERIFIED |
| Only the selected body is highlighted | VERIFIED |
| Scene picking returns the nearest hit's correct ObjectId; a viewport pick re-points the editors | VERIFIED |
| Sidedness is per body: a Plane body does not make its neighbour two-sided | VERIFIED |
| Per-body Freeze / Resume / stale-source / current-mesh edit predicate, independent across bodies | VERIFIED |
| A→B→A returns A's own Frozen Sculpt Mesh, revision and edits, without re-freezing | VERIFIED |
| Mode, held tool and brush Radius/Strength are session-wide and unchanged by switching bodies | VERIFIED |
| Objects list holds no Java-side model selection; every refresh re-reads native state | VERIFIED |
| Plane: 4:6 open source topology, exact bounds, two-sided render and pick as one bounded, named exception | VERIFIED |
| Sidedness is owned by the active published representation; render, selection picking and Sculpt hit-test all read that one value | VERIFIED |
| Frozen Plane renders, picks and **sculpts** from both sides; a real stroke lands on the underside | VERIFIED |
| A stale frozen solid never inherits two-sidedness from a Construction Source later changed to a Plane (and the converse) | VERIFIED |
| Destructive re-Freeze confirms only for edits on the CURRENT frozen mesh; historical strokes never raise it | VERIFIED |
| The re-Freeze message states no stroke count, because no honest per-mesh count exists to state | VERIFIED |
| Exact dimensions in meters; mm/cm/m display unit above JNI only, lossless | VERIFIED |
| Apply Shape: Applied / Unchanged / Rejected, atomic, kind change is a change | VERIFIED |
| Every primitive's parameters remembered independently across kind changes | VERIFIED |
| Capsule relation (`totalHeight >= diameter`) rejected in words, object unchanged | VERIFIED |
| Capsule equality case generated as a sphere (482 : 2880) | VERIFIED |
| Position/rotation transform, degrees, right-hand rule, local X→Y→Z | VERIFIED |
| Transform-only edit publishes no revision and triggers no GPU upload | VERIFIED |
| Freeze to Sculpt / Back to Construction / Resume Sculpt without re-freezing | VERIFIED |
| Stale-source policy: Construction change never touches the sculpt mesh | VERIFIED |
| Four sculpt tools (Grab, Clay, Smooth, Inflate) on one shared kernel | VERIFIED |
| Shared Radius and Strength, clamped, unchanged by tool switching | VERIFIED |
| Clay ≠ Inflate, measured (start-normal vs current-normal) | VERIFIED |
| Pending-then-promote: multi-touch navigation cannot mutate the sculpt mesh | VERIFIED |
| Sculpt topology fixed; buffers reused, never reallocated during a stroke | VERIFIED |
| Construction Source bit-identical after sculpting with all four tools | VERIFIED |
| Lifecycle: shape, placement, identity, unit, mode, tool, sculpt, camera and selection survive home/resume with no re-upload | VERIFIED |
| Editor Workspace: Global Toolbar, Tool Rail, Property Inspector, direct brush controls | VERIFIED |
| Adaptive layout: compact portrait, phone landscape, expanded/tablet, decided by window dp | VERIFIED |
| Landscape occlusion: 0 % unoccluded viewport → **60.1 %**, status line on screen | VERIFIED |
| Inspector collapse and chrome hide restore viewport area (57.5 % → 82.6 % → 100 %) | VERIFIED |
| Edge-to-edge with WindowInsets on chrome only; IME never resizes the Vulkan surface | VERIFIED |
| Every chrome surface consumes its own gesture; viewport pixel-identical across chrome drags | VERIFIED |
| Freeze / Resume wording follows whether a Frozen Sculpt Mesh exists | VERIFIED |
| Destructive re-Freeze confirms only when the current mesh's edits would be discarded; Cancel is inert | VERIFIED |
| Stable semantic ids on every control; 32 instrumented + 22 JVM tests | VERIFIED |
| Correct geometric proportions in portrait, physical 90° landscape and a non-rotated wide window | VERIFIED |
| One orientation convention: identity pre-transform, swapchain image = window | VERIFIED |
| Rotation mutates no Construction or Sculpt state and triggers no mesh upload | VERIFIED |
| Studio Solid replaces the per-vertex rainbow as the default appearance | VERIFIED |
| Derived render-only normals; source RuntimeMesh and picking untouched | VERIFIED |
| One 40° crease policy gives all six primitives their hard/smooth contracts | VERIFIED |
| Smooth ↔ Faceted is presentation only, on the same source revision | VERIFIED |
| ForgeShape-generated MatCap from view-space normals; one preset, no asset file | VERIFIED |
| Studio ↔ MatCap rebuilds no geometry and uploads nothing | VERIFIED |
| Sculpt deformation relights immediately; no stale lighting, no NaN | VERIFIED |
| Selection stays obvious and form stays readable in both shading modes | VERIFIED |
| Display settings are native-owned and survive HOME/resume | VERIFIED |
| No per-frame normal or render-data rebuild: 4448 frames, 2 rebuilds | VERIFIED |
| 16 KB page-size runtime behaviour (x86_64) | VERIFIED (Gate P1) |
| Physical ARM64 execution: `primaryCpuAbi=arm64-v8a`, full self-test suite, smoke, lifecycle and both orientations | VERIFIED (Gate P1) |
| Picking is watertight across a shared triangle edge, and identical on arm64-v8a and x86_64 | VERIFIED (Gate P1) |
| Heavy-mesh ladder ~10k / ~50k / ~100k on physical ARM64: publish, render build, GPU upload, pick, lifecycle | VERIFIED (Gate P1) |
| Sculpt at ~100k vertices on physical ARM64: freeze, adjacency, real strokes, buffer reuse | VERIFIED (Gate P1) |
| Real injected multi-touch on physical hardware never mutates the sculpt mesh | VERIFIED (Gate P1) |
| 16 KB page size *and* ARM64 in one target | **UNVERIFIED** — see Known Issues |
| Stylus / S Pen tool type and pressure on real hardware | **UNVERIFIED** — needs a person physically moving an S Pen |

## Self-test suite

Eleven debug-only native suites run once from `NativeViewport.start()` — never
per frame — and total **1530 checks, zero failures** at the accepted baseline
under NDK r29:

| suite token | checks |
| --- | --- |
| `FORGESHAPE_CAMERA_SELFTEST_OK` | 119 |
| `FORGESHAPE_PICKING_SELFTEST_OK` | 128 |
| `FORGESHAPE_DYNAMIC_MESH_SELFTEST_OK` | 91 |
| `FORGESHAPE_CONSTRUCTION_BOX_SELFTEST_OK` | 100 |
| `FORGESHAPE_CONSTRUCTION_TRANSFORM_SELFTEST_OK` | 94 |
| `FORGESHAPE_CONSTRUCTION_PRIMITIVE_SELFTEST_OK` | 125 |
| `FORGESHAPE_CONSTRUCTION_SPHERE_SELFTEST_OK` | 105 |
| `FORGESHAPE_CONE_CAPSULE_SELFTEST_OK` | 163 |
| `FORGESHAPE_SCULPT_BRUSH_KERNEL_SELFTEST_OK` | 302 |
| `FORGESHAPE_RENDER_SHADING_SELFTEST_OK` | 224 |
| `FORGESHAPE_SCENE_SELFTEST_OK` | 79 |

followed by `FORGESHAPE_MESH_UPLOAD_OK` and `FORGESHAPE_NATIVE_VIEWPORT_OK`.
The tenth suite covers the crease policy, all six primitives' Smooth contracts
and the capsule equality case, Faceted, NaN/Inf and fail-closed behaviour,
determinism, the render-data rebuild policy, the generated MatCap asset, the
display settings, the proof that building render data leaves the
authoritative `RuntimeMesh` bit-identical, and (Stage 016) the Plane's
two-sided render duplication and its inertness to display switching
(`PLN-09`/`10`/`20`). Stage 016 also added `PLN-01`..`08` to the primitive
suite, `PLN-11`..`16` to the picking suite, and `PLN-17`..`19` to the sculpt
suite.

Stage 015D added the **projection family**, `CAMPROJ-01`..`CAMPROJ-14`, split
across three suites by module ownership rather than kept in one file: the camera
suite carries `CAMPROJ-01`..`08` and `12`..`14` (the perspective audit, the
orthographic matrix, depth-independence versus foreshortening, both framing
conversions, orbit invariance, ortho pinch and clamps, purity, resume, and the
P2 orientation convention in both modes on a non-square viewport); the picking
suite carries `CAMPROJ-09`/`10`; the sculpt suite carries `CAMPROJ-11`. Each
family member has a counterpart that would fail if the behaviour collapsed to a
constant — `CAMPROJ-04` against `03`, and the perspective depth check against the
orthographic one — which is the property the `NOR` family was added for.

Stage 015C-R added the **direction family** to it, `NOR-01`..`NOR-10`: source
winding on all five primitives, outward render normals per primitive, Faceted
orientation against the source triangle, duplication-preserves-winding, the
model→view normal transform under six representative placements, and display
modes proven inert against picking and revisions. Its guard check
`nor_outwardness_fails_on_global_normal_flip` asserts the measurement inverts
under a global `normal *= -1`, which is the property the rest of the suite
lacked.

Three further non-per-frame diagnostics exist. `FORGESHAPE_SURFACE_CONFIG` (one
per swapchain creation) and `FORGESHAPE_CAMERA_VIEWPORT` (one per
`surfaceChanged`) audit the orientation chain; `FORGESHAPE_RENDER_MESH_BUILD` (one
per accepted geometry or surface-shading change) reports source vs render counts,
the rebuild count and the frame counter. `README.md` documents how to read all
three.

## Android UI suites

Added by Stage 015B; the first automated tests the Android layer has ever had.
Build and verification commands are in `README.md`.

| suite | scope | tests |
| --- | --- | --- |
| `WorkspaceLayoutModeTest` (JVM) | breakpoints, placement, chrome sizing arithmetic | 9 |
| `EditorUiStateTest` (JVM) | what the UI may remember, and what it refuses | 8 |
| `LengthUnitTest` (JVM) | exact mm/cm/m round-tripping and parse refusal | 5 |
| `EditorWorkspaceControlsTest` | UI-01/02/03/04/05/06/13 — control sets, fields, validation, freeze wording, tools, presentation-only actions; `PLN-07`/`08` — the six-kind round trip and the Plane chip's no-native-call contract | 19 |
| `EditorWorkspaceLayoutTest` | UI-07/08/09/12 — measured viewport floor, landscape, expanded, collapse | 6 |
| `EditorWorkspaceGestureTest` | UI-10/11 — chrome gesture ownership, IME | 5 |
| `EditorWorkspaceLifecycleTest` | UI-14 — HOME/resume rebuilt from native truth | 3 |
| `DisplaySettingsContractTest` (JVM) | the shading/surface index contract across JNI | 4 |
| `EditorWorkspaceDisplayTest` | SHD-13/15/16 — display ids, presentation-only, resume; PROJ-12/13/14 — projection ids, inertness against domain state, refused index, resume | 12 |

**72 tests** (26 JVM, 46 instrumented). No Java test asserts a rendered pixel;
every control is reached by its stable semantic id and no assertion uses a screen
coordinate.

**55 of the 56 instrumented tests pass as of Stage 017** on
`ForgeShape_Stage006`; the one failure is the `ui11` IME case described just
below. Pre-017 took the count 46 → 50, replacing one self-skipping re-Freeze
test with the five `REFR-01..07` cases; Stage 017 took it 50 → 56 with
`EditorWorkspaceObjectsTest` (`S17-21..26`).

**The scene is process-scoped and there is no delete, so bodies ACCUMULATE
across the tests in a run**, and the product may be left in Sculpt mode by an
earlier one. No instrumented test may therefore assume a body count, which body
sits at the origin, or which mode is current: each establishes what it needs and
asserts relative to what it found. That is the Stage 016-R2 lesson — a test that
reads live global state passes or fails on what ran before it — applied to the
instrumentation, and it is why `EditorWorkspaceObjectsTest` has an
`isolateAtOrigin` helper rather than hard-coded placements.

`EditorWorkspaceGestureTest.ui11_theImeLeavesTheFieldAndTheCommitPathUsableAndTheSurfaceUntouched`
is the one case that has moved in both directions across recent stages: it
opens with a precondition guard — "the soft keyboard did not appear, so this
case proves nothing" — and fails on that guard alone, never reaching an
assertion about product behaviour, whenever the keyboard is slow to appear. It
failed at the Stage 015C baseline, passed at Stage 015C-R, failed again at
Stage 015D (proven pre-existing there against a stashed, unmodified `171c7ae`
tree), and **passed at Stage 016**. Treat a future failure of this one case as
a harness symptom to confirm against the current baseline before calling it a
regression.

## Current evidence summary

Latest acceptance run, on `ForgeShape_Stage006` / `emulator-5580` unless a line
says otherwise:

- **Native self-tests:** eleven suites, **1530 checks, zero failures** on a
  clean launch. The Gate P1 picking suite is unchanged at 128, so the ARM64
  shared-edge fix is intact; `SIDE`, `REFR`, `NOR` and `CAMPROJ` all still green.
- **JVM:** 26/26.
- **Instrumented:** 56 run, **55 green**, through
  `scripts\run-instrumented-tests.ps1 -Serial emulator-5580`. The one failure is
  `EditorWorkspaceGestureTest.ui11_…`, which fails its own precondition guard
  ("the soft keyboard did not appear, so this case proves nothing") — **proven
  pre-existing** for Stage 017 by stashing every change and reproducing the
  identical failure on `d04e0b4`. See *Android UI suites*.
- **Device guards:** `DEV2-01`..`07` and `DEV3-01`..`06` all PASS, with no
  device attached and zero `emulator-5554` interaction.
- **Physical ARM64 (Gate P1):** closed on a Galaxy S25 Ultra —
  `primaryCpuAbi=arm64-v8a`, `PAGE_SIZE` 4096, the mandatory ~10k/~50k/~100k
  ladder and Sculpt at 100k measured on real hardware. Stylus stays UNVERIFIED.
- **Runtime:** the Stage 017 walkthrough (two bodies, alternating picks,
  isolated edits, per-body Freeze/Sculpt/Resume round trip, Plane front/back,
  both projections, both shading models, HOME/resume, rotation) is summarised
  in the Stage 017 chapter at the top of this file.

**One caveat about capturing self-test evidence.** On both the emulator and the
physical phone the logcat ring buffer intermittently drops whole suites from the
*middle* of a startup capture, which reads exactly like a suite that never ran.
The reliable signal is that the suites which do appear always report their full
expected check counts and `_SELFTEST_FAIL` is always absent; read a partial
capture as "no failures observed", and re-run until one capture is complete
before quoting a total.

## Display-control motion, and what was rejected

Motion research for the display popover used Mobbin Pro against creative and
canvas applications on iOS. Five patterns were worth having an opinion about:

| Pattern | Seen in | Decision |
| --- | --- | --- |
| Popover **grows from the control that opened it**, rather than sliding in from a screen edge | Craft (line style), Apple Mail (shapes), Freeform | **Adopted.** 120 ms scale-plus-fade from the anchor corner; it reads as belonging to the button instead of arriving from nowhere. |
| **Selection feedback in place** — the chip repaints, the surface does not move, re-animate or close | Freeform's alignment settings toggles checkmarks across three frames with the menu stationary | **Adopted.** This is the one that matters: comparing Studio against MatCap means switching repeatedly, and a panel that re-animated on each choice would make it feel like four separate acts. |
| **Non-modal, stays open** across several changes | Craft, Freeform | **Adopted.** Dismissed only by tapping Display again, or by hiding chrome. |
| **Grouped labelled chip rows** rather than one long list | eBay "Edit Scene" (Material / Shadow), Photoroom (Shadow / Backdrop / Size) | **Adopted as layout**, not as styling: two groups, Shading and Surface. |
| **Live preview thumbnails** with a check badge per option | Photoroom | **Rejected.** A rendered thumbnail per shading mode needs a second offscreen render target, for a two-option choice whose result is already filling the screen behind the panel. |
| **Bottom sheet with drag detents** | Canva, Play, Unfold | **Rejected for this control.** A sheet covers the model, which is the thing being judged. ForgeShape already has a Property Inspector for sheet-shaped content. |

Nothing decorative was taken. There is no continuous or looping model animation,
no motion on the viewport itself, and no animation anywhere on the path of a
pointer sample. Both animations are interruptible, and a zero system animator
duration scale — the platform's own reduce-motion signal — skips them outright
rather than shortening them.

**The §12 readability enhancement is deliberately deferred.** Neither candidate
was implemented. A selected-object outline needs either a second pass over the
geometry or a screen-space depth/normal edge filter, and the latter is the start
of the post-processing framework this stage is told not to build; cheap cavity
shading from a screen-space depth derivative is the same problem, and the
vertex-based alternative would expose triangle structure in Smooth mode, which
the stage explicitly forbids. Neither is a small addition to what exists, and the
required shading is complete without them.

## Known Issues / Blockers

- **16 KB page-size runtime behaviour is VERIFIED (Gate P1)**, on the x86_64
  `ForgeShape_16K` AVD (`getconf PAGE_SIZE` = 16384): full self-test suite,
  Construction/picking/projection/shading/Freeze/Resume/Sculpt smoke all
  green. **Still open: 16 KB pages *and* ARM64 in the same target.** The
  physical device Gate P1 closed on is a 4 KB-page phone, and the 16 KB
  target is x86_64, so each dimension is proven but not both at once. This
  needs 16 KB-page ARM64 hardware, which is a device-availability matter, not
  a code one; both `.so`s already carry 0x4000 ELF `LOAD` alignment.
- **Floating-point rounding is ABI-dependent, and exact FP comparisons in
  geometry code are therefore an ARM64 hazard.** No `-ffp-contract=off` is
  set, so Clang contracts multiply-adds into `fmadd` on arm64-v8a where
  baseline x86-64 cannot — the direct cause of the shared-edge picking defect
  Gate P1 found. Anything that compares a computed geometric quantity against
  an exact bound should be assumed to differ between the emulator and a real
  phone until measured on both.
- **This phone drops self-test lines from the logcat ring buffer, and no
  capture method fully prevents it.** It caps the buffer at 5 MiB
  (`logcat -G 16M` is silently reduced), and the ~1400-line self-test burst
  competes with a busy system log, so whole suites vanish from the *middle* of
  a capture — which reads exactly like a suite that never ran. Confirmed
  during Gate P1 across four methods: `logcat -d` after the fact, a host-side
  stream started before launch, a PID/tag-filtered stream, and even an
  on-device `logcat -f` (which rules out Wi-Fi adb transport as the cause).
  Each dropped a *different* subset. The reliable signal is that the suites
  which do appear always report their full expected check counts and
  `_SELFTEST_FAIL` is always absent — so read a partial capture as "no
  failures observed", and re-run until one capture is complete before
  claiming a total. A complete 1425/1425 capture with all ten suites and
  `FORGESHAPE_NATIVE_VIEWPORT_OK` was obtained this Gate; the retries after it
  were partial, all with zero failures.
- **The `EditorWorkspaceView` layout decision runs inside `onMeasure`.** That is
  deliberate and documented — running it in `onSizeChanged` measures newly added
  chrome against the previous pass and lays it out at zero height, which is
  exactly the bug that was hit and fixed during this stage — but mutating the
  view tree during measure is unusual enough to be worth naming. It is
  idempotent and converges in one traversal.
- **A horizontal drag along the Property Inspector header toggles it.** The
  header is a full-width clickable row, and Android's default click detection
  fires on `ACTION_UP` while the pointer is still inside the view's bounds,
  regardless of how far it travelled. Harmless — the gesture never reaches the
  viewport — but it reads as a stray toggle. Not fixed, to avoid hand-written
  touch handling on a surface whose touch-opacity is load-bearing.
- **Test-harness note, not a product defect:** a stroke driven at the centre of a
  face of a frozen **box** logs `STROKE_PENDING` → `STROKE_ABANDONED:navigation`
  and never promotes, because a box has 8 vertices, all at its corners, and a
  brush that would capture no vertex starts no stroke. Reproduced identically
  under r27 and r29. Drive evidence strokes on a dense primitive (sphere 482 v,
  capsule 514 v) or widen the radius.
- **A viewport tap while the soft keyboard is up** must land clear of the
  Property Inspector and above the keyboard or it never reaches the
  `SurfaceView`; focus stays in the text field and a following `keyevent` types a
  digit instead. Looks exactly like a command that did not run. Test-harness
  issue only.
- **`uiautomator dump` omits inspector content scrolled out of view**, so a
  script that looks for `apply_shape` without scrolling the inspector first gets
  `MISSING` and reads like a lost control. Scroll, then resolve the id.
- **`connectedDebugAndroidTest` uninstalls the app when it finishes**, and it has
  no `-s <serial>` equivalent. Reinstall before taking runtime evidence, and run
  it with exactly one target attached.
- **The default logcat ring buffer drops part of the startup self-test output.**
  Run `adb -s <serial> logcat -G 16M` first.
- **The Android emulator dies if launched as a child of a tool shell**; spawn it
  detached.
- Android input resampling can emit a one-finger MOVE between `ACTION_DOWN` and
  `ACTION_POINTER_DOWN`, so starting a two-finger gesture may orbit by a few
  pixels first. Correct for the contract, not a defect.
- The path-driven tools deposit in proportion to pointer travel measured in brush
  radii, so a **short stroke with a large brush** does very little — the one
  thing that reads as "the brush did nothing" on a first try. Documented
  behaviour, not a defect.
- The arbitration's remaining deliberate seam: a first finger that **drags more
  than 8 px** before a second one lands does commit a stroke. That is a gesture
  the user drove as a stroke; closing it completely needs a buffered or undoable
  stroke, which does not exist.
- On `emulator-5558`, `screencap` output can acquire a uniform whole-framebuffer
  black-level lift partway through a session. It is a system/compositing effect,
  not a rendering change — the swapchain format is unchanged and home/resume
  within one display state stays byte-identical — but it means screenshots from
  different points in a session may not compare.

## Technical Debt

**"Debug-only" code is proven debug-*guarded*, not proven absent from a release
binary.** Every self-test and mesh-fixture entry point is behind `#ifndef
NDEBUG` or an equivalent guard, so a release build calls none of them — that
much is verified by reading the call sites. But the self-test and fixture
translation units sit on the CMake source list unconditionally, and no release
`.so` has ever been inspected to confirm the linker drops the symbols. Until
someone builds a release binary and checks, the honest claim is "the calls are
debug-guarded", not "the code is not shipped". Closing this means examining a
real release artifact, and possibly moving the files behind a CMake condition —
a change to release/test compilation that deliberately was not made as a side
effect of the Pre-017 repair.

**The selection tint costs about half the surface's form contrast, measured.**
Selection is a whole-object tint mixed over the final shaded colour at
`alpha = 0.55` (`kSelectedTint` in `forgeshape_renderer.cpp`). On the default box
that takes the three visible faces from luminance 0.832 / 0.559 / 0.310 —
a 2.69x spread with clear steps between adjacent faces — to 0.739 / 0.616 /
0.504, a 1.47x spread whose face-to-face steps drop from ~0.26 to ~0.11, with
every face pushed into the same saturated orange so hue carries no form cue
either. A Construction Body is selected for the whole time the user is editing
it, so this is the state the object is normally *worked* in.

This is **not** the Stage 015C-R defect and was not touched by it: the inverted
culling was a separate, larger fault that the numbers above are measured after
fixing. It is recorded because it is a real, quantified readability cost, and
because the obvious repair is small — compose selection as a per-channel gain on
the shaded colour (`shaded * mix(vec3(1.0), tint, a)`) instead of a lerp toward a
flat colour, which preserves the luminance ratios between faces exactly while
leaving the object unmistakably orange. That is a deliberate change to a
user-visible convention, so it belongs to a stage that owns it, not to a fix
whose scope was culling.

**Sculpt cost model.** Sculpt publication is synchronous and republishes the
whole mesh per move — O(vertices) regardless of how few the brush touched — and
Inflate's normal recompute is O(triangles) per move for the same reason. Measured
free at 482–514 vertices (1,170 uploads, no stall, no growth), so nothing was
redesigned pre-emptively; that measurement is the baseline a future partial or
asynchronous path must beat. The affected set is found by a linear scan at stroke
start, and picking is a linear scan too: both want the same missing spatial
acceleration.

**Sculpt state and feel.** Sculpt state is process-scoped with no save, load or
undo, so a stroke is unrecoverable the moment it lands and Freeze silently
discards the previous sculpt (the panel says so; the honest fix is undo). The
path-driven gain constants (`kNormalBrushGain` 0.35, `kSmoothGain` 1.0,
`kMaxSmoothLambda` 0.9) are chosen, not derived, and have never been tuned
against a real modelling session; if brush feel is tuned they should move
together. `FORGESHAPE_SCULPT_STROKE_ABANDONED:navigation` names two different
causes — a gesture that really became navigation, and a promoted stroke that
captured no vertex — and the log cannot tell them apart; the fix is a second
token, not a behaviour change. Freeze regenerates the Construction mesh to copy
it rather than copying the store's current revision: correct (the store may hold
a debug fixture) and cheap, but a second generation of geometry that already
exists. The Sculpt panel's status line is written on refresh and does not update
during a stroke, because a live readout would need a native→Java notification
that does not exist.

**Primitive surface.** Adding a primitive still costs four parallel edits — a
member on `ConstructionObject`, a `PrimitiveKind` case, a variant alternative,
and a JNI method plus its Java declaration. That remains deliberate and visible
rather than hidden behind a registry. Stage 016 paid down the one instance of
it that dispatched with an if-else chain over typed accessors:
`ConstructionObject::setPrimitive`'s update half now `std::visit`s the
requested payload against two small per-kind overload sets
(`parametersDiffer` / `writeParameters`), so a kind added to the variant with
no matching overload is a compile error rather than a branch that is silently
never taken — the same property the pre-existing `validateParameters` overload
family already had. The plain `switch (kind_)` statements elsewhere
(`spec()`, `generateMesh()`, `primitiveKindName`) were left as they were: each
is one line per kind, visually complete at a glance, and — Stage 016 found one
real instance of exactly this risk in `forgeshape_jni.cpp`'s separate
`describeSpec` if-else chain, which had no Plane branch until the runtime pass
caught it — a `switch` with a trailing fallback is a smaller, more visible
version of the same gap than an if-else chain was, not a solved one; a future
stage that wants it closed for these too has a proven pattern to reuse. Every
primitive's parameters are always resident even though one is active.
Tessellation is a compile-time constant, so a very large
curved primitive shows its facets and nothing adapts; relatedly, the capsule's
cylindrical middle carries no interior rings however long it is, which makes
sculpt fidelity there coarse (a 120 px brush captured 8 vertices on a 514-vertex
capsule), and the float-resolvability check is the one validation coupled to that
tessellation constant. Capsule topology is a function of its parameters (514 :
3072, or 482 : 2880 at equality), which is deliberate and is also the only case
where a shape edit can change a vertex count and trigger a buffer growth.

**Android layer.** The workspace still builds its view trees in code — colours,
dimensions, strings and ids are resources, but there are no XML layouts, so the
structure is only readable by reading Java. The Property Inspector has two
detents (collapsed / expanded) rather than the three the UI architecture pack
proposed; the third would only matter once a body is long enough that a partial
peek is useful. Chrome regions are placed by nested `LinearLayout` weights rather
than a constraint solver, which is why the rail sits in a `ScrollView` instead of
being able to compress. There is no focus-on-selection and no camera framing
helper. Focus handling is minimal: a field keeps focus after a rejected Apply and
keeps swallowing hardware/`adb` number keys until the viewport is touched. The
inspector refreshes from native truth on resume and after any Apply, discarding a
half-typed edit — deliberate, but a future edit-session or undo feature will need
a model for it. `LengthUnit.format` strips trailing zeros, so 2.0 m displays as
`2`; exact and unambiguous, but a future significant-figures policy will replace
it. The Tool Rail's glyphs are Unicode geometric characters rather than drawable
assets, so they depend on the platform font.

**Android test infrastructure.** `androidTest` is the project's only AndroidX,
and `android.useAndroidX=true` is now set for it — a build-configuration change
that adds nothing to the product APK, which still has no runtime dependency of
any kind. The instrumented suites share one process, so native state (in
particular *whether anything has ever been frozen*) carries across tests; the
freeze-wording test therefore asserts the state machine from whatever state it
finds rather than assuming a pristine process, and the pristine branch is covered
by running the suite on a clean install. There is no camera read-back across JNI,
so "a chrome gesture did not move the camera" is proven by runtime screenshot
comparison rather than by an automated assertion.

**Transform and math.** `modelMatrix()` / `inverseModelMatrix()` recompute six
trig calls per frame and per pick, for a value that only changes on Apply. The
transform is rigid by design: the exact composed inverse and the "local distance
is world distance" shortcut in picking both depend on that, so adding scale is a
domain change rather than a matrix change. A large translation makes the float
round-trip residual grow linearly with `|p|` (about `|p| * 2^-23`); at kilometre
scale the derived `float` matrix, not the `double` domain, is the precision
limit.

**Mesh and renderer.** Each body owns a `MeshStore`, and the renderer keeps a
`BodyRenderResources` per body, so a scene of N bodies is N independent
publication chains and N buffer pairs — correct, and deliberately not pooled or
batched. `MeshStore::publish` validates the data twice (before
taking the lock and again inside `createRuntimeMesh`) — negligible now, wasteful
at sculpt sizes. Retired GPU buffers are freed inline after the fence wait rather
than through a deferred-destruction queue, which is what makes the synchronous
wait necessary. One redundant swapchain rebuild occurs at startup and again on
each resume, because `surfaceChanged` arrives immediately after `surfaceCreated`
(cosmetic). The identity-pre-transform orientation convention costs one
compositor rotation while the display is rotated — the same cost every
non-pre-rotated Android application pays, and not measurable here — but on a
tiled mobile GPU true pre-rotation is the cheaper path, so this is the one
renderer decision that a future performance stage might revisit; it would have to
move the extent, the clip-space rotation and the camera aspect together.
`createSwapchain`'s fallback for a presentation engine that does **not** support
an identity transform is **UNVERIFIED**: it exists because declaring identity
when it is absent from `supportedTransforms` would be invalid usage, not because
anything can reach it — `emulator-5558` reports `supportedTransforms=0x1ff`, so
identity is always available and the branch has never executed. Its extent swap
for 90/270 is reasoned from the Vulkan pre-transform contract, not measured, and
it would present rotated content. Carried and untouched: static viewport and
scissor, no `oldSwapchain` handling,
no validation layers, a single global viewport, and a selection highlight that is
a whole-object tint rather than an outline.

**Documentation size.** Every core document is inside the 2000-line hard limit,
but four are over their preferred target budgets: `ARCHITECTURE.md` 1714
against a 700–1000 target, `PROJECT_STATUS.md` 1061 against 500–800,
`PRODUCT.md` 609 against 300–450, `README.md` 320 against 150–250.
(`CLAUDE.md` 180 is inside its.) Stage 016 paid part of this back rather than
only adding to it: the Stage 015C-R chapter here was compacted from 49 lines to
15, keeping its conclusions and moving its measurement detail to Git history,
the same pattern Stage 015D used on the chapters before it. The remaining
overshoot predates this stage and is concentrated in `ARCHITECTURE.md`, where
closing it means compacting prose about shading, sculpt and layout that a
primitive-only stage does not own. That is a deliberate deferral, the same one
Stage 015D recorded: bundling a documentation rewrite into a stage whose scope
is one primitive is the unrelated-debt mixing the rules forbid.

**Shading and render data.** The crease policy is a single global angle. It is
correct for every primitive ForgeShape has, but it is a *policy*, not a per-object
property: a future imported or sketched body that genuinely wants a different
threshold has nowhere to say so, and the honest fix is a per-object crease value
rather than a second constant. Render data is rebuilt whole on every accepted
change, including every sculpt move — 0.56–0.73 ms for a 482-vertex mesh, free at
these sizes, and the same O(vertices) shape as the sculpt publication it rides on;
both want the same missing partial-update path. The renderer keeps a per-frame
gate *and* `RenderMeshCache` keeps its own, so the cache's skipped-refresh counter
reads zero forever in production and is exercised only by the self-tests; it is
deliberately not logged, but the duplication is real. Colour still travels in
`RenderVertex` purely so the debug source-colour mode has something to draw,
costing 12 bytes per render vertex in a build where nothing else reads it — the
primitive generators' per-vertex rainbow is likewise now dead data on the product
path, retained rather than deleted so the before/after comparison stays one tap
away. Studio Solid and the MatCap are authored **directly in display space**,
because the swapchain is `R8G8B8A8_UNORM` and every colour in ForgeShape (the
clear colour, the debug vertex colours) already is; there is no linear workflow,
and introducing one is a PBR-stage decision that has to move all of them at once.
The MatCap is regenerated from scratch on every device creation rather than
cached, which is 64 KiB of trivial arithmetic and has never been measured, but it
is work repeated for a constant. Selection remains a whole-object tint rather
than an outline; the §12 readability enhancement (outline or cavity) was
**explicitly deferred** — see below.

**Camera and projection.** Three things are deliberate but worth naming. The
orthographic view plane sits a fixed `kFarPlane/2` in front of the target, which
makes `snapshot.eye` mean something different in the two modes and makes a
reported orthographic pick distance ~250 m rather than a distance from the orbit
eye; the field is only ever used as a ray origin and a view reference, so this is
correct, but it is a name that no longer describes both cases equally well and a
future stage that adds a second camera should rename it. The ortho depth slab is
a fixed 500 m rather than fitted to the scene — free at these sizes because ortho
depth is linear, but it is a constant, not a policy. And
`kInitialOrthoHalfHeightMeters` is a literal because `std::tan` is not
`constexpr`; a self-test asserts it still equals `kInitialDistance × tan(fovY/2)`
so the two cannot drift, but a computed constant would be better than a checked
one. Separately, the camera still has no read-back of pose across JNI, so
"a chrome gesture did not move the camera" is still proven by screenshot rather
than by assertion — the new projection getter is the first camera value the Java
layer can read at all.

**Naming and test infrastructure.** `kConstructionBoxObjectId` and
`kDemoCubeObjectId` are the same value under two names and are now doubly
misnamed: the object is not always a box and never was a demo cube. Renaming has
been deferred to avoid churning unrelated code. There is still no checked-in
`uinput` harness file, so the multi-touch gesture is regenerated per stage.

## Current Files / Modules

| Path | Ownership |
| --- | --- |
| `settings.gradle`, `build.gradle`, `gradle.properties`, `local.properties` | Gradle project config |
| `gradlew(.bat)`, `gradle/wrapper/*` | Gradle 8.14.3 wrapper |
| `app/build.gradle` | Android app module config, SDK/NDK/CMake/ABI pinning |
| `app/src/main/AndroidManifest.xml` | App/activity declaration, Vulkan feature requirement |
| `app/src/main/java/.../ForgeShapeActivity.java` | Android lifecycle, edge-to-edge window, resume refresh, DEBUG key hook |
| `app/src/main/java/.../ForgeShapeSurfaceView.java` | Viewport surface, forwards lifecycle + raw pointer state, takes focus back from an editor |
| `app/src/main/java/.../EditorWorkspaceView.java` | The whole editor UI: region composition, adaptive layout, window insets, chrome visibility, mode/tool wiring, and `syncFromNative()`. Owns no product state |
| `app/src/main/java/.../WorkspaceLayoutMode.java` | Window-dp breakpoints, inspector placement and chrome sizing, as arithmetic. No Android type |
| `app/src/main/java/.../EditorUiState.java` | The closed list of UI-owned state: display unit, draft kind, rail selection, detent per mode, chrome-hidden |
| `app/src/main/java/.../GlobalToolbarView.java` | Editing context, the three mutually exclusive mode transitions, reserved Export, the Display control, chrome hide, and the one status message |
| `app/src/main/java/.../DisplaySettingsPopoverView.java` | The compact display popover: Shading (Studio / MatCap / Debug), Surface (Smooth / Faceted) and Projection (Perspective / Orthographic), with short interruptible open/close motion that honours the system animator scale. Owns no state |
| `app/src/main/java/.../ToolRailView.java` | The edge tool selector for either mode, including reserved entries. Selects; decides nothing |
| `app/src/main/java/.../BrushEdgeControlsView.java`, `VerticalSliderView.java` | Direct Radius and Strength, and the custom vertical control behind them. Own no brush value |
| `app/src/main/java/.../PropertyInspectorView.java` | Contextual, collapsible, scrolling container with a measured height cap. Owns no value |
| `app/src/main/java/.../ConstructionShapeEditorView.java` | Primitive chooser, that primitive's exact fields, unit chips, Apply Shape. Owns field text and a DRAFT kind only |
| `app/src/main/java/.../ConstructionPlacementEditorView.java` | Position/rotation fields, unit chips, Apply Transform. Owns field text only |
| `app/src/main/java/.../SculptContextView.java` | Frozen-mesh summary, stale-source warning, and the guarded re-Freeze |
| `app/src/main/java/.../InspectorHost.java`, `NumericPropertyRow.java`, `UnitChipsView.java`, `EditorControlStyles.java` | The four small shared pieces: what a body may ask of the workspace, one labelled exact field, the mm/cm/m selector, and the one place controls get their look |
| `app/src/main/java/.../LengthUnit.java` | Exact `BigDecimal` mm/cm/m ↔ meter conversion, parsing and formatting |
| `app/src/main/res/values/*` | `ids.xml` (the stable semantic id contract), `dimens.xml`, `colors.xml`, `strings.xml`, `themes.xml` (edge-to-edge) |
| `app/src/test/java/...` | JVM suites: layout arithmetic, UI-owned state, unit conversion |
| `app/src/androidTest/java/...` | Instrumented Editor Workspace suites plus `WorkspaceTestSupport` (native snapshots, drag consumption, exact chrome-union viewport measurement) |
| `app/src/main/java/.../NativeViewport.java` | JNI declarations, library load, `APPLY_*` / `SCULPT_*` status codes, `MODE_*`, `TOOL_*` |
| `app/src/main/cpp/forgeshape_jni.cpp` | JNI boundary, render thread, `ANativeWindow`, MotionEvent→`TouchAction`, camera + selection locking, stroke arbitration, `publishActiveRepresentation` |
| `app/src/main/cpp/forgeshape_input.h` | Platform-neutral touch event data (`TouchAction`, `TouchPointer`) |
| `app/src/main/cpp/forgeshape_camera.{h,cpp}` | Camera pose, the `ProjectionMode` enum and the orthographic world span, both projections, the framing-preserving switch, gesture state machine |
| `app/src/main/cpp/forgeshape_construction.{h,cpp}` | `ConstructionObject` (identity + active `PrimitiveKind` + all five primitives + transform), the five `Construction*` generators, shared tessellation constants, the typed `PrimitiveSpec` payload variant, dimension validation including `validateCapsuleMeters`, publication into `MeshStore`, and `applyPrimitive` — the one update-and-publish entry point |
| `app/src/main/cpp/forgeshape_transform.{h,cpp}` | `ConstructionTransform`: authoritative double-meter position and double-degree rotation, THE axis/Euler convention, validation, atomic apply, derived model and inverse-model matrices |
| `app/src/main/cpp/forgeshape_sculpt.{h,cpp}` | `ProductMode`, `SculptTool`, `SculptSession` (mode + tool + brush + live stroke + the `hitsSculptMesh` probe), `SculptMesh`, `SculptTopology`, `computeVertexNormals`, `SculptStroke` (the one kernel plus one `apply*` per tool), sculpt publication |
| `app/src/main/cpp/forgeshape_picking.{h,cpp}` | Screen→world ray for **both** projections (perspective: one origin, fanning directions; orthographic: one direction, per-pixel origin), `transformRayToLocal`, ray/triangle, nearest hit, winding check |
| `app/src/main/cpp/forgeshape_selection.{h,cpp}` | `ObjectId`, `SelectionController`, tap-vs-navigation, `pickScene` |
| `app/src/main/cpp/forgeshape_object_id.h` | `ObjectId` type and reserved values, shared by the mesh and selection layers |
| `app/src/main/cpp/forgeshape_mesh.{h,cpp}` | `RuntimeMesh` (immutable revision), `MeshStore`, validation, capacity policy, upload diagnostics including source-vs-render counts |
| `app/src/main/cpp/forgeshape_render_mesh.{h,cpp}` | Derived render geometry: `RenderVertex` (position + normal + colour), `SurfaceShading`, THE crease policy (`kCreaseAngleDegrees`), per-vertex crease grouping with render-only duplication, and `RenderMeshCache`'s rebuild gate. Presentation only |
| `app/src/main/cpp/forgeshape_matcap.{h,cpp}` | The one ForgeShape-owned MatCap, computed at device init from the closed-form model in that file. No asset, no decoder, one preset |
| `app/src/main/cpp/forgeshape_display.{h,cpp}` | `ShadingModel`, the process-scoped `DisplaySettingsStore`, and the UI index mapping. Presentation state, never truth |
| `app/src/main/cpp/forgeshape_renderer.{h,cpp}` | Vulkan renderer, frame loop, camera snapshot + model transform + selection highlight consumer |
| `app/src/main/cpp/forgeshape_math.h` | Minimal self-owned vec3/mat4. No GLM |
| `app/src/main/cpp/forgeshape_demo_mesh.{h,cpp}` | Baseline cube numbers; source data for the baseline debug fixture only |
| `app/src/main/cpp/forgeshape_mesh_fixtures.{h,cpp}` | DEBUG test fixtures (baseline / same-topology / larger / stress step) |
| `app/src/main/cpp/forgeshape_*_selftest.{h,cpp}` | The ten debug-only deterministic suites: camera, picking, mesh, construction (box), transform, primitive, sphere, cone/capsule, sculpt brush kernel, render shading |
| `app/src/main/cpp/shaders/surface.{vert,frag}` | GLSL source for the surface pipeline: view-space normals, Studio Solid, the MatCap lookup and the debug colour path. AOT compiled to SPIR-V by `glslc` in CMake |
| `app/src/main/cpp/CMakeLists.txt` | Native build + glslc shader step |
| `artifacts/` | Runtime evidence screenshots from accepted stages, plus `stage015c_shading_comparison.md`, the Stage 015C comparison sheet |
| `docs/ui/UX_ARCHITECTURE_DECISION_PACK.md` | Stage 015A-R UI proposal. **Proposal only** — the Owner Decision Baseline above is authoritative, not the pack |
| `README.md`, `ARCHITECTURE.md`, `PRODUCT.md`, `CLAUDE.md` | See the ownership table in `CLAUDE.md` |

## Shading cost record

Not a benchmark stage; these are the numbers a future change has to beat, taken
from `FORGESHAPE_RENDER_MESH_BUILD` on `emulator-5558`.

| Mesh | Source (v:i) | Render (v:i) | Rebuild |
| --- | --- | --- | --- |
| Box | 8:36 | 24:36 | 0.03 ms |
| Cylinder | 66:384 | 130:384 | 0.55 ms |
| Sphere | 482:2880 | 482:2880 | 0.66–1.25 ms |
| Cone | 34:192 | 66:192 | 0.07 ms |
| Capsule | 514:3072 | 514:3072 | 0.78 ms |
| Sphere, Faceted | 482:2880 | 2880:2880 | 0.30 ms |
| Sculpt mesh, per accepted move | 482:2880 | 491–492:2880 | 0.56–0.73 ms |

A rebuild happens per accepted geometry change, not per frame, so none of this is
on the frame budget: 4448 frames cost 2 rebuilds. **MatCap costs no more than
Studio Solid** and if anything less — it is one texture fetch against Studio's two
Lambert terms, a hemisphere lerp, a Blinn-Phong power and a rim power — and both
remained normally interactive throughout, with orbiting, sculpting and rotation
indistinguishable from the pre-stage build by eye. No frame-time instrumentation
was added and no marketing claim is made.

## Next Stage

**Stage 018 — Object Commands**

Stage 017 gave the product a scene: several Construction Bodies, stable
identity, selection, per-body state and an Objects list. What it deliberately
did not give it is any way to *manage* those bodies. The only object command
that exists is Add.

Stage 018 is the command set that a scene needs before anything else can be
built on it — delete, duplicate, and the visibility/lock kind of state that
decides what a command may touch — together with the question Add Body dodged
by having no answer to give: what an ObjectId means once ids can stop existing.
Stage 017 has no reuse policy precisely because it has no delete, and that is
the first thing Stage 018 has to settle.

It is still not the hierarchy stage: groups, nesting and reparenting stay out,
as do Undo/Redo, Sketch/Extrude, booleans and persistence. A command framework
is a decision for whichever stage first needs commands to be undoable, and
Stage 017 deliberately left one unbuilt.
