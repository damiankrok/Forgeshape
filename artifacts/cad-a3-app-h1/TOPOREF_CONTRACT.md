# TopoRef (`ARCH-OWNER-13`)

A persistent, SEMANTIC reference to a producer feature's face. It is feature
lineage, never a render-triangle index.

```
struct TopoRef {
    ObjectId producerObjectId;        // the producer body
    uint32_t producerLocalFeatureId;  // its CAD feature (v1: always 1)
    CadFaceToken face;                // kind + edge identity, NOT a triangle
    uint64_t lineageToken;            // the producer's topology signature
};
```

`CadFaceToken` is `{ kind (CapPlane|CapFar|Side), edgeEntityId, edgeLocalIndex }`.
For a side it names the profile edge that produced the face by the sketch entity
that owns it and which of that entity's edges it is; for a cap both are zero. It
is stable across every supported parameter edit.

## Resolution and fail-closed

`resolveCadFace(producerState, token, &face)` returns the face frame, or
`ProfileNotFound` when the token names no face of the current topology. A
face-supported body resolves ONLY when: the producer exists and is a CAD body;
the producer's `cadTopologySignature` still equals the reference's
`lineageToken`; and the named face resolves and is eligible. A stale lineage, a
missing producer, a bad token or a curved side all fail closed -- the body is
not drawn at a wrong place, and no nearest-face retargeting ever happens.

## Where it lives

- In memory: on the child's `CadSketch.faceSupport`, with `hasFaceSupport`.
- In the scene: the placement is DERIVED (`ConstructionScene::resolveWorldModel`),
  never stored as a transform.
- In the file: the CADB v2 record (see `CADB_V2_DATA_CONTRACT.md`).
- Never: a triangle range, a GPU id, a matrix, a camera, or a preview mesh.

## Verified

`CADA3-27/36/37` (native + document): a face-supported project round-trips its
TopoRef bit for bit; a document whose child names a nonexistent producer, or a
stale lineage, or a cycle is refused; a curved side is refused as a support.
