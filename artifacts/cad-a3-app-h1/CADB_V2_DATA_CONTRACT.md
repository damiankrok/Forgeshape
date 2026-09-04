# CADB v2 (`CAD-A3` H, closed by `CAD-A3-C1`)

The `.forge` CADB section gains version 2, adding the face support. Section
versions evolve independently of the file's major/minor.

## When v2 is written

Only when at least one CAD body is face-supported. A world-only CAD project
still writes CADB v1, byte-identical to what CAD-R0 wrote -- proven by the
sixteen v1 corpus fixtures decoding to identical digests after this stage, and
by `CADA3_46` reading the section version word off the encoded files.

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

## The lineage token is a format field

`lineageToken` is the producer's topology signature, and because a reader
recomputes it and compares, its rule is part of the format:
`DATA_PACKAGE_SPEC.md` §7c now states it in full (FNV-1a 64, basis
`0xCBF29CE484222325`, prime `0x100000001B3`, over the profile anchor id, the
face count, and each face's token code and eligibility in enumeration order),
and `scripts/build-forge-corpus.ps1` reimplements it from that text. That
independent reimplementation is what found the production basis mistyped one
digit short; see `CORPUS.md`.

## Load ordering and fail-closed

`decodeProject` reads the whole document into temporary state, then
`validateProjectDocument` validates the ENTIRE dependency graph before anything
is applied: every face-supported body's producer must be another CAD body in
the document (`UnresolvedReference`), its topology signature must equal the
reference's lineage token and the named face must resolve and be eligible
(`InvalidSemanticValue`), and the graph must be acyclic — no self-support, no
cycle (`UnresolvedReference`). A successful load starts a fresh session
history; a failed load changes nothing.

## Compatibility

- All sixteen v1 fixtures unchanged; the twelve Construction/Imported/Sculpt
  fixtures and the four v1 CAD fixtures decode identically.
- An older build refuses a v2 CADB as a required section at an unknown version
  (`UnsupportedSectionVersion`) rather than opening half a body.

`DATA_PACKAGE_SPEC.md` owns the authoritative layout and the fixture table.

## Independent corpus

Delivered: six v2 fixtures written by the independent PowerShell encoder,
pinned by `CADA3-46..51`, their bytes identical to the production encoder's
for the four valid files and their refusals exact for the two corrupt ones.
See `CORPUS.md`.
