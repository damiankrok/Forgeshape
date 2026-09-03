# CADB v2 (`CAD-A3` H)

The `.forge` CADB section gains version 2, adding the face support. Section
versions evolve independently of the file's major/minor.

## When v2 is written

Only when at least one CAD body is face-supported. A world-only CAD project
still writes CADB v1, byte-identical to what CAD-R0 wrote -- proven by the
sixteen corpus fixtures decoding to identical digests after this stage.

## The v2 per-body record

Same as v1, with a support block inserted after the plane code:

```
u64 objectId
u8  planeCode
u8  supportKind            // v2 only: 0 world plane, 1 face
if supportKind == 1:       // v2 only, face support
    u64 producerObjectId
    u32 producerLocalFeatureId
    u8  faceKind            // 1 CapPlane, 2 CapFar, 3 Side
    u32 faceEdgeEntityId
    u32 faceEdgeLocalIndex
    u64 lineageToken
u32 nextEntityId
u32 profileEntityId
u8  directionCode
f64 depth
u32 entityCount
per entity: ...
```

No source path, matrix, triangle range, GPU id, camera or preview mesh is ever
written -- only the authored truth needed to regenerate.

## Load ordering and fail-closed

`decodeProject` reads the whole document into temporary state, then
`validateProjectDocument` validates the ENTIRE dependency graph before anything
is applied: every face-supported body's producer must be another CAD body in the
document, its topology signature must equal the reference's lineage token, the
named face must resolve and be eligible, and the graph must be acyclic (no
self-support, no cycle). A bad reference or a cycle refuses the file; a
successful load starts a fresh session history; a failed load changes nothing.

## Compatibility

- All legacy fixtures unchanged; the twelve v1 Construction/Imported/Sculpt
  fixtures and the four v1 CAD fixtures decode identically.
- An older build refuses a v2 CADB as a required section at an unknown version
  (`UnsupportedSectionVersion`) rather than opening half a body.

`DATA_PACKAGE_SPEC.md` owns the authoritative layout.

## Deferred

The independent PowerShell corpus builder was NOT extended to emit v2 fixtures
this pass, and the named v2 fixtures (`cad_face_sketch_cap_v2` etc.) are not
committed. The v2 codec is proved instead by the native round-trip and
fail-closed checks (`CADA3-40/41/42/36/37`) over documents the C++ encoder
builds. Recorded as bounded debt.
