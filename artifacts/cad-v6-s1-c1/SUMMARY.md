# CAD-V6-S1-C1-ID-LIFETIME-R1 — how long a feature id and a sketch id live

Intermediate branch `feature/cad-v6-sketch-face-r1`, continued from `dfb5091`
(`origin/main = 6c9f156`, merge-base `6c9f156`). **Not merged to `main`, by
design. S2 not started.** `BEFORE.md` (commit `f8e5c07`) was written, with the
five BEFORE checks, before any product or documentation edit.

## What the branch did before this correction (measured, `BEFORE.md`)

* Undo rewinds `nextFeatureId` and `nextSketchId` with the rest of the
  `CadBodyState` snapshot, and the next Add after an Undo is handed feature 2 /
  sketch 2 again (`CADV6C1_IDL_B01`).
* The redo step still holds the undone feature 2 / sketch 2; Redo restores
  exactly that identity (`B02`). S1's "nothing references an undone feature"
  was therefore false while the redo step stands.
* The commit that re-mints the id is the same `ConstructionHistory::commitEdit`
  call that clears redo; no Undo/Redo walk reaches the abandoned Add (`B03`).
* A cancelled session or a cancelled open edit burns no id (`B04`).
* Undo back to a saved `CADB` v1 state is byte- and fingerprint-equal (`B05`).
* Nothing forced the marks to stay up on a forward edit: a committed edit could
  have LOWERED them — no product path did, but a future delete-feature command
  could have handed a freed id straight back.

## The two models

**Model A — history-branch identity.** Ids unique along one forward history
branch; Undo rewinds the marks; an edit after Undo may re-mint an id only the
redo side held. Its five conditions, checked against the source:

1. *No live reference outside the snapshot can mistake a re-minted id.* The
   sketch session holds a feature id and a staged state, and Undo/Redo are
   refused while it is active (`runHistoryStep`, `jni.cpp:5945`). The support
   chooser's selection can survive an Undo, but the only thing that mints an id
   is a sketch-session commit, every session begin cancels the chooser first,
   and cancel clears the selection — so it can never see a re-minted id.
2. *Session/UI/JNI caches.* The Java feature list is rebuilt on every
   Construction refresh and every Undo/Redo ends in one; a row only asks native
   to stage an edit of the id against the CURRENT state. JNI keeps nothing. No
   `CadSketchId` crosses JNI.
3. *No persisted reference outside `CadBodyState`.* Another body's `TopoRef` is
   the only one, and it lives in the same whole-scene snapshot as its producer.
   Detached bodies come back with the step's own state. Renderer and picking
   name no feature or sketch id.
4. *Redo and the current branch cannot mix.* Redo applies a whole snapshot;
   redo is cleared in the call that records the step that re-mints.
5. *S2 can keep it.* Only if every Undo-reachable state holds ids below the
   current marks — which required ONE change (below).

**Model B — body/process-lifetime monotonic identity.** Two ways to build it,
both measured or traced:

* *Floor kept inside `CadBodyState` across Undo.* Undo would no longer restore
  the snapshot exactly, and the post-Undo state (base only, marks 3/3) is not
  legacy-shaped: its file is `CADB` v6 and its fingerprint differs from the save
  (`CADV6C1_IDL_08`, measured) — Undo to a saved project would read unsaved and
  upgrade the format for invisible allocator metadata. Fails B's conditions 1
  and 2.
* *Floor outside the snapshot (per `ObjectId`, runtime-only).* A second
  allocator truth beside the stored marks, which must follow Duplicate, Delete,
  undone creation, Open and Recover; the pure `candidateState()` would need it
  plumbed in; the file would state a mark lower than the runtime one; and it
  resets on reopen anyway, so the "lifetime" would be the process's. Fails B's
  condition 5, and buys protection only for handles that — by the inventory
  above — cannot exist.

The `ObjectId` allocator IS process-monotonic, and deliberately: it lives
outside the snapshot, bodies held for Undo AND Redo are keyed by it
(`detached_`), and the renderer and mesh store key resources by it, so an
undo-side step could name an id a new body wore. Feature and sketch ids are
body-local, inside the snapshot, and named by nothing outside one.

## Decision: Model A, made structural

`CadBody::applyState` — the one door every committed CAD edit takes (session
Add/Cut/New Body, Edit Sketch/Feature, typed edits; history restores use
`restoreState`) — now **refuses a request that would LOWER either high-water
mark** (`HighWaterInvalid`, counted as a rejected update, nothing written).
With it, every state on the Undo stack was reached through that door, so every
id it holds is below the current marks, and a re-mint can only collide with
the redo side, which the minting commit clears. No existing test or product
path relied on lowering a mark (host aggregate unchanged apart from the new
checks). Removing the guard fails `CADV6C1_IDL_01` (checked).

## The one definition (the `CadSketchId` comment, `forgeshape_cad_body.h`)

* **CadSketchId / CadFeatureId**: unique along ONE FORWARD HISTORY BRANCH — the
  body's states from its creation, or from the Open that loaded it, to the
  current one through committed edits. A committed edit never lowers
  `nextSketchId`/`nextFeatureId`, so an id a committed deletion freed is never
  minted again on that branch.
* Undo restores the snapshot with its marks; Redo restores the forward one
  exactly; an edit after Undo may re-mint an id only the redo step held, in the
  same commit that clears redo.
* Cancelled and refused edits burn nothing.
* The marks are persisted only where a legacy read would not derive them
  (`CADB` v6); a reopened project continues from what its file states; Undo to
  the saved state is byte- and fingerprint-equal to the save.
* Not "for the body's lifetime", and why, stated beside it.

`ARCHITECTURE.md`, `DATA_PACKAGE_SPEC.md` (§7g: a file states the marks of the
state it was written from and nothing else), `CLAUDE.md` and
`PROJECT_STATUS.md` point to it. "State lineage" and "body lifetime" no longer
appear as definitions anywhere.

## Tests (`forgeshape_cad_feature_selftest.cpp`, CAD_FEATURE 263 → 278)

All drive the product's own session commit, `ScopedConstructionEdit` and
`ConstructionHistory` over a scene of their own, opened with an empty history.

| Check | Pins |
| --- | --- |
| `CADV6C1_IDL_B01..B05` | the BEFORE behaviour above (unchanged by the decision) |
| `IDL_01` | committed deletion of feature 3 / sketch 3 keeps marks 4/4; the next feature is 4 on sketch 4; a state handing 3 back (either mark) is refused `HighWaterInvalid` and records nothing |
| `IDL_02` | Undo restores the snapshot bit-exactly, marks 2/2, legacy-shaped |
| `IDL_03` | while undone, id 2 exists only in the redo snapshot; Redo restores feature 2 / sketch 2 exactly, once |
| `IDL_04` | Undo + a Cut: handed 2/2 with the circle as sketch 2's content, redo emptied by that commit, no reachable state is the old Add |
| `IDL_05` | features 2 and 3 (3 on 2's cap), Undo ×2 → marks 2/2, new branch mints 2 then 3; nothing reachable stands on the abandoned feature 2 |
| `IDL_06` | a cancelled session, a refused commit (`AddNoEffect`) and a cancelled open edit burn no id, record nothing, change no byte or fingerprint; the next Add gets 2 |
| `IDL_07` | save/reopen: after Add the reopened project mints 3; after Add+Undo the file equals the pre-Add file and the reopened project mints 2; after a committed deletion the file is v6 with marks 4/4 and the reopened project mints 4 |
| `IDL_08` | Add then Undo is byte- and fingerprint-equal to the save; the Model-B floor state would be `CADB` v6 with another fingerprint |
| `IDL_09` | sketch 2 shared by feature 2 (Add) and feature 3 (Cut): Undo/Redo exact; the re-minted feature 3 gets a NEW sketch 3, sketch 2 keeps its content and only feature 2 names it |
| `IDL_10` | feature 3 on feature 2's cap and a New Body with a `TopoRef` to it; Undo ×3, new circle feature 2: redo gone, the dependent body gone, no reachable state stands on the square's lineage, every reachable state validates; grafting the abandoned feature 3 is refused `FeatureSupportInvalid` — and for a SAME-shape re-mint the lineage token cannot tell them apart (asserted), which is why the guarantee is the redo clear, not the token |

## Format and fixtures

No `CADB` layout, version, section or fixture changed (no stop token needed).
The 44 legacy fixtures and the 12 v6 fixtures are byte-identical; the
independent PowerShell encoder regenerates all 56 byte-identically
(`FORGE_CORPUS_PARITY` 56/56, local).

## Residual observation (not an id-lifetime defect, not fixed here)

A support-chooser selection survives an Undo and is not re-resolved at
confirm. Its worst case is a staged sketch framed on a face that moved or no
longer exists; every commit re-validates the support, so it cannot land on the
wrong face, and it cannot observe a re-minted id. Recorded as debt.

## Gates

| Gate | Result |
| --- | --- |
| Host self-tests | `HOST_SELFTESTS_OK (3907 checks, 0 failed)`; CAD_FEATURE 263 → 278 |
| Corpus parity (local, pwsh) | 56/56 byte-identical; `testdata/forge/v1` unchanged |
| NDK debug + release (local, pinned 29.0.14206865) | built (`GRADLE_EXIT=0`) |
| Release self-test guard (local) | `RELEASE_SELFTEST_GUARD=PASS` — release 0 symbols / 0 strings on both ABIs |
| `CI FAST` `36775468595` on tested candidate `ab5e071` | **success** (build, JVM tests, release guard, corpus parity 56/56, device-free guards) |
| `CI DEVICE` | NOT RUN |
| FullSharded | NOT RUN |
| Merge to `main` | NOT MERGED — **TECH PASS / V6 INTERMEDIATE BRANCH / NOT MERGED** |
