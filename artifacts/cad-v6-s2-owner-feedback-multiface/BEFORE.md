# BEFORE — source facts on the baseline, before any product edit

Baseline: task branch `feature/cad-v6-s2-owner-feedback-multiface-r1` created at
`d66dd89dd559480d3876222696c6b07c80b42e04` (the previous task's HEAD; product
`8ca3e6b`). `origin/main = 6c9f156c1df91fe1a55445aed6186105a075ab31`,
`origin/feature/cad-v6-sketch-face-r1 = bbae765645a318f83451e9a61b6a45cf2cb17e33`
— all three as expected, so no ref drift.

Paths are under `app/src/main/`; line numbers are at `d66dd89`.

## 3.1 The legacy cap

`cpp/forgeshape_sketch_region.h:83`

```cpp
constexpr uint32_t kMaxProfileRegions = 16;
```

## 3.2 The planar-face cap aliases it

`cpp/forgeshape_cad_body.h:157`

```cpp
constexpr uint32_t kMaxPlanarFaceSelection = kMaxProfileRegions;
```

(`:158` aliases the holes cap the same way: `kMaxPlanarFaceHoles = kMaxRegionHoles`.)

## 3.3 The tap refusal

`cpp/forgeshape_sketch_session.cpp:1725-1727`, inside `SketchSession::togglePlanarFace`:

```cpp
        if (next.size() + 1u > kMaxPlanarFaceSelection) {
            return fail(CadStatus::TooManyRegions);
        }
```

The same cap is enforced again by `resolvePlanarFaceSelection`
(`cpp/forgeshape_cad_body.cpp:758`, `faces.size() > kMaxPlanarFaceSelection`
→ `TooManyRegions`).

## 3.4 The `CADB` v6 field is a u32

`cpp/forgeshape_project_document.cpp:1150` (writer):

```cpp
            out.u32(static_cast<uint32_t>(extrude.planarFaces.size()));
```

`cpp/forgeshape_project_document.cpp:2101-2106` (reader):

```cpp
            uint32_t faceCount = 0;
            if (!in.u32(&faceCount)) {
                return ProjectCodecStatus::Truncated;
            }
            if (faceCount == 0 || faceCount > kMaxPlanarFaceSelection) {
                return ProjectCodecStatus::ImpossibleCount;
```

So 16 is a VALIDATION bound, not the field width: the field holds up to
2^32 − 1, and `DATA_PACKAGE_SPEC.md:977` documents it as `u32 faceCount 1 .. 16`.

## 3.5 The status the owner saw

`res/values/strings.xml:537`

```xml
<string name="status_cad_ambiguous_profile">Several profiles are closed — choose one in Sketch values.</string>
```

`java/com/forgeshape/app/CadStatusMessages.java:28-29` maps
`NativeViewport.CAD_AMBIGUOUS_PROFILE` to it.

`cpp/forgeshape_sketch_session.cpp:1590-1596`:

```cpp
CadStatus SketchSession::unchosenStatus() const {
    if (selectionLost_) {
        return CadStatus::PlanarFaceUnresolved;
    }
    const size_t areas = faceShapes_.empty() ? regions_.regions.size() : faceShapes_.size();
    return areas > 1 ? CadStatus::AmbiguousProfile : CadStatus::ProfileNotFound;
}
```

It is reached only when `selectionChosen()` is false (`commit` `:2086-2088`,
`commitEdit` `:352-354`, `evaluateCandidate` `:2033-2036`), and
`selectionChosen()` reads the PlanarFaces list in PlanarFaces mode — so the
ordinary session paths are correct for a non-empty face selection.

### The path that is NOT correct

`cpp/forgeshape_project_bootstrap.cpp:37-41`, in `commitFirstCadProject` — the
commit the CAD bootstrap (New Project → CAD, the first Extrude) takes instead of
`SketchSession::commit` (`cpp/forgeshape_jni.cpp:4368-4378`):

```cpp
    if (sketch.selectedProfileId() == kNoSketchEntity) {
        return finish(sketch, sketch.profiles().profiles.size() > 1
                                  ? CadStatus::AmbiguousProfile
                                  : CadStatus::ProfileNotFound);
    }
```

`selectedProfileId()` is `extrude_.profileEntityId`
(`cpp/forgeshape_sketch_session.h:448`), the LOOP-REGION field, and
`setPlanarSelection` clears it on every face selection
(`cpp/forgeshape_sketch_session.cpp:1604`). So in a new CAD project every
PlanarFaces selection — one face, two, sixteen — is refused, and with more
than one closed LOOP in the sketch (a rectangle plus circles) the refusal is
`AmbiguousProfile`. Meanwhile the candidate is `Ok`, so the toolbar's Extrude
is drawn (`EditorWorkspaceView.refreshExtrudeReadiness`, `:3595-3600`), the tap
reaches `sketchCommit`, and `onExtrudeRequested` shows
`CadStatusMessages.describe(sketchLastStatus())` (`:3709`) — the screenshot.

Across all native sources `AmbiguousProfile` is produced in exactly three
places: this bootstrap check, `unchosenStatus()` (empty selection only), and
`validateRegionSelection` with an EMPTY loop-region list
(`cpp/forgeshape_sketch_region.cpp:306`). Only the bootstrap can produce it
with a non-empty face selection.

## Host reproduction (test-only commit, product untouched)

`cpp/forgeshape_cad_multiface_selftest.cpp`, run inside `CAD_FEATURE`
(`bash scripts/host-native-selftests.sh CAD_FEATURE`) on the baseline product:

```
HOST_SUITE_OK CAD_FEATURE (382 checks, 0 failed, 1018 ms)
... multiface_before=cap:16,17th:TooManyRegions,count:16,candidate:Ok,first_project_grid:ProfileNotFound,first_project_owner:AmbiguousProfile
```

| check | what it pins on the baseline |
| --- | --- |
| `MF_B01` | `kMaxPlanarFaceSelection == kMaxProfileRegions == 16` |
| `MF_B02` | on a 6 × 4 grid (24 faces), faces 1..16 select; the 17th returns `TooManyRegions`, `lastStatus` is `TooManyRegions`, the set stays exactly 16 and the 17th is not selected |
| `MF_B03` | the 16-face candidate is still `Ok` with a mesh after that refusal |
| `MF_B04` | first project (empty scene, `commitFirstCadProject`) with 2 grid faces chosen and the candidate `Ok`: refused `ProfileNotFound` (the grid closes one loop) |
| `MF_B04B` | first project, the owner's kind of sketch (4 × 3 rectangle, circles r 0.8 at (−0.8, 0) and (0, 0), r 0.6 at (2, 0)), 3 faces chosen, candidate `Ok`: refused **`AmbiguousProfile`** — the screenshot's message |
| `MF_B05` | the same 2-face selection inside a LIVE project commits through `SketchSession::commit` |

### Where the status comes from (the traced sequence)

1. New Project → CAD → draw → Finish: `sketchFinish`, PlanarFaces mode, many
   faces, none chosen; status `%d regions found — none chosen yet; tap one.`
2. Taps 1..16: `sketchToggleRegion`/the Ready tap → `togglePlanarFace` `Ok` →
   `onSketchGestureSettled` sees a new `regionSelectionSignature` →
   `refreshExtrudeReadiness` (candidate `Ok`, Extrude drawn) →
   `reportCandidateVerdict("Regions in the extrusion: N.")`.
3. Tap 17: `TooManyRegions` (`FORGESHAPE_SKETCH_TAP:selection_cap`); the
   signature does not change, so the lower branch reports
   `Too many regions in one extrusion.`
4. Extrude (drawn, because the candidate is `Ok`) → `sketchCommit` → no project
   open → `commitFirstCadProject` → `AmbiguousProfile` →
   `onExtrudeRequested` shows `Several profiles are closed — choose one in
   Sketch values.`
5. Removing faces down to "a couple" changes nothing in step 4: the bootstrap
   check never reads the face list.

So the message is not stale and not a UI synchronisation fault: it is the
CURRENT commit's own refusal, from the one commit path that tested the wrong
selection field. The 16 cap is a separate, second defect.
