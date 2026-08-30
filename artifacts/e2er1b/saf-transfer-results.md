# Scoped Storage transfer — result table (`FSR1B-10..13`, `E2ER1B-06`)

Every row below was produced by driving the **production** result handlers
(`EditorWorkspaceView#onCreateProjectDocumentChosen`,
`#onOpenProjectDocumentChosen`, `#onCreateDiagnosticsDocumentChosen`) against a
real `ContentResolver`. The system document picker's own UI is not tapped
through — it belongs to another app and differs per device — so what is asserted
instead is the exact Intent that would be sent, plus everything that happens to
the bytes once a destination or a source exists.

Raw run: `focused-fsr1b.txt` (`ProjectTransferTest`, 13/13).

| Case | What was driven | Result |
| --- | --- | --- |
| Save Copy writes canonical bytes | `onCreateProjectDocumentChosen(uri)` | The document holds **exactly** the bytes `encodeProject()` produced, and `validateProject` accepts them |
| Save Copy truncates | destination pre-filled with a longer file | The document is exactly the project's length — no foreign trailing bytes behind a valid header |
| Open File applies | `onOpenProjectDocumentChosen(uri)` | The project's values are live; session history starts fresh |
| Open File refuses damage | one bit flipped in the `SCNE` payload | Refused; **no native value moved** |
| Open File survives an unreadable source | a `Uri` naming nothing | Refused, non-destructive, no exception |
| Cancel | `null` Uri through the same handler | No-op in both directions; the staged copy bytes are released, so a later pick cannot write the cancelled project |
| Internal slot, on open | manual Save, then Open File of a different project | The opened project is live; the internal slot is **byte-identical** |
| Internal slot, on copy | manual Save, then Save Copy | The internal slot is **byte-identical** |
| No provenance in the document | opened from `zz-provenance-marker-4242.forge`, then re-encoded | The re-encoded bytes equal the ORIGINAL exactly; no filename, `file:`, `content:` or `/data/` appears anywhere in them |
| The codec cannot see a `Uri` | reflection over `NativeViewport` | `loadProject` and `validateProject` each take exactly one `byte[]`; `encodeProject` takes nothing and returns `byte[]` |
| Create intent | `ProjectTransfer.createDocumentIntent()` | `ACTION_CREATE_DOCUMENT`, `CATEGORY_OPENABLE`, `application/octet-stream`, `EXTRA_TITLE` ending `.forge` |
| Open intent | `ProjectTransfer.openDocumentIntent()` | `ACTION_OPEN_DOCUMENT`, `CATEGORY_OPENABLE`, `*/*` — deliberately permissive, because a `.forge` file has no registered type and filtering narrowly would hide the user's own project from the picker. The fail-closed decoder is the real guard. |
| Diagnostics intent | `ProjectTransfer.createDiagnosticsIntent()` | `ACTION_CREATE_DOCUMENT`, `text/plain` — a document the user places, never a network target |

## Interchange

No GLB, glTF, OBJ, FBX, STL, COLLADA or USDZ capability, converter, string or
menu entry was added. `ProjectTransferTest#fsr1b18_...` walks every `TextView`
and content description on the project surface and fails on any of those words —
including `import` and `export`. Export keeps its single reserved, recessed,
unimplemented home in the Global Toolbar, untouched by this stage.
