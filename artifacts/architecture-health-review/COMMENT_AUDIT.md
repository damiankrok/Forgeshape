# Comment audit — ForgeShape architecture health review (2026-09-01)

Scope: `app/src/main/cpp`, `app/src/main/java`, `app/src/androidTest`,
`app/src/test`, `scripts/`, the root Markdown. Search was case-insensitive
ripgrep for `TODO|FIXME|HACK|XXX|TEMP|WORKAROUND|DEBT`, then a second pass for
`NOTE`, `R0`, `R1`, `preview`, `stale`, `Stage 0`, `previously`, `used to`.

## Marker counts

| Marker | Hits in code | Classification |
| --- | --- | --- |
| TODO / FIXME / HACK / XXX | 0 | — |
| TEMP / WORKAROUND / DEBT | 0 in code; `DEBT` appears only in `PROJECT_STATUS.md` §Technical Debt (owner-facing, intended) | — |
| NOTE (case-insensitive) | 15 | all benign prose ("note that…"), none a deferred-work marker |
| R0 / R1 | reader/preview provenance (`GLB-IMPORT-R0`/`R1`) in import files and CLAUDE.md | accurate: names the stage that built the reader; not stale |
| preview | confined to `forgeshape_import_preview.*`, `forgeshape_glb_roundtrip.cpp`, the R0/R1 suites, and the Java diagnostic entry with no UI control | accurate; the preview is documented as a diagnostic with no user-facing control |
| stale | `staleSource` flag on `FrozenSculpt` and its UI | product concept, not a marker |

## Stale or wrong comments found and corrected (all comment-only)

| File:line (baseline) | Was | Now |
| --- | --- | --- |
| `forgeshape_transform.h:~324` | described `constructionTransform()` as `constructionObject().transform()` — a function that no longer exists since the placement moved to the body | the active body's own `transform()`, whichever representation it has |
| `forgeshape_scene.h:1–13` | header said the scene is "a collection of Construction Bodies" | "bodies, each a Construction Body or an Imported Mesh"; names `activeConstructionOrNull()` |
| `forgeshape_scene.h:~324` | accessor list named the old reference-returning accessor | lists `activeConstructionOrNull()`, `meshStore()`, `sculptSession()`, `constructionTransform()` |
| `forgeshape_scene.cpp:156–168` | migration prose ("Keeping the same three names … Stage 017") | states the invariant that stands on its own: the Construction accessor returns a POINTER since IMPORT-01A |
| `forgeshape_history.h:91–96` | "identical … with the same one active" — the implementation (`history.cpp:10–32`) deliberately does NOT compare `activeBodyId` | says so, and says representation IS compared |
| `forgeshape_jni.cpp:~2003` (`sceneAddBody`) | "no lock is held across geometry generation anywhere else either" — false: `runHistoryStep` and `loadProject` generate under `g_stateMutex` | explains why THIS call may publish outside the lock and names the three sites that generate under it |

## Historical references kept on purpose

- `forgeshape_construction.h:874` and `forgeshape_selection.cpp:30` refer to a
  previous shape of the code in the past tense to explain WHY the current one
  is what it is. Accurate; kept.
- Stage tokens in comments (`ARCH-OWNER-07`, `IMPORT-01A`, `GLB-IMPORT-R0`)
  are cross-references into `PROJECT_STATUS.md`/`CLAUDE.md`, where the rule
  they cite is stated in full. Kept: they are pointers, not jargon a reader
  must decode.

## Comments that instruct rather than explain

None found. The code's comment density is high and consistently about
ownership, units, lifecycle and "why", as CLAUDE.md asks.
