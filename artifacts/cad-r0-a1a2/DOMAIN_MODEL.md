# CAD-R0-A1A2 — domain model

The smallest reusable CAD architecture the live code allowed, and why each piece
is where it is.

```
Workplane (XY | XZ | YZ)              forgeshape_workplane.{h,cpp}
    |
CadSketch                              forgeshape_sketch.{h,cpp}
    plane
    entities[]  : SketchEntity = id + variant<Line, Polyline, Rectangle, Circle>
    nextEntityId
    |
extractClosedProfiles()  ->  ClosedProfile[] (anchor id, CCW polygon, area, members)
triangulateSimplePolygon()   (derived; never stored)
    |
ExtrudeFeature { profileEntityId, depth, direction }     forgeshape_cad_body.{h,cpp}
CadBodyState  { sketch, extrude }        <- the WHOLE truth of a CAD Body
generateCadMesh(state) -> ConstructionMesh (derived)
CadBody { objectId, state; applyState (atomic), generateMesh, capture/restore }
    |
SceneObject(ObjectId, CadBodyState)      BodyRepresentation::Cad = 3
ConstructionScene::addCadBody / makeCadBody
    |
ConstructionHistory  BodyConstructionState.cad = CadBodyState (bounded)
ProjectDocument      CADB section  <-> CadBodyState
GLB export           regenerates through generateCadMesh
    |
SketchSession        volatile edit session; touch -> entities; overlay; commit
```

## Identities

| Thing | Identity | Minted by | Reused |
| --- | --- | --- | --- |
| a body | `ObjectId` | `ConstructionScene` | never |
| a sketch entity | `SketchEntityId` (u32, per sketch) | `CadSketch::nextEntityId` | never within a sketch |
| a closed profile | its **anchor entity id** | derived from the members (the smallest id of a line chain; the entity itself otherwise) | follows the entity |
| the extrude feature | implicit: v1 has exactly one per body | - | - |

A sketch element is deliberately not an `ObjectId`: it must never be listable,
selectable or deletable as a scene body. The single sketch and single extrusion
per body are a struct, not a list, so nothing pretends a feature tree exists; a
second feature kind takes a new `CADB` section version.

## What is truth and what is derived

| Truth (stored, compared, undone) | Derived (regenerated, never stored) |
| --- | --- |
| workplane | the plane's 3D frame (fixed constants) |
| every entity's own values and id | which entities close a profile |
| the sketch's id high-water mark | each profile's polygon and area |
| the chosen profile's anchor id | the triangulation |
| depth, direction | the extruded mesh, its normals, the published revision |

`sameCadBodyState` is bit-exact, so the history commit, the fingerprint and the
codec all agree about whether anything changed.

## Bounds

| Bound | Value | Why |
| --- | ---: | --- |
| `kMaxSketchEntities` | 256 | a history step and a `.forge` record stay bounded |
| `kMaxPolylineVertices` | 256 | same |
| `kMaxProfileVertices` | 256 | bounds the O(n^2) crossing test and the ear-clipping pass |
| `kMaxSketchCoordinateMeters` | 1e5 m | every float conversion downstream is provably finite |
| `kSketchCoincidenceMeters` | 1e-6 m | one micrometre: two points closer than this are one point |
| `kMinProfileAreaSquareMeters` | 1e-12 m^2 | the coincidence tolerance squared |
| `kSketchCircleSegments` | 32 | the same count every round primitive uses; divisible by four so the cardinal points are exact |

## Status vocabulary

One closed enum, `CadStatus` (27 values), for sketch validation, profile
extraction, triangulation, extrusion, the session and the CAD edits. It crosses
JNI as its ordinal (`NativeViewport.CAD_*`) and `cadStatusToken` turns a code
back into its name for a log line. See `forgeshape_sketch.h`.
