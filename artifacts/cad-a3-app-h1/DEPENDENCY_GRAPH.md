# CAD dependency graph (`CAD-A3` F/G)

A face-supported CAD body depends on the producer its TopoRef names. The
dependencies form a directed graph.

## Resolution

`ConstructionScene::resolveWorldModel(id)` walks the producer chain and composes
`producerWorldModel * cadFaceFrameMatrix(resolvedFace)`, fresh every scene
snapshot. Because it is lazy and recomputed, moving/rotating/scaling a producer
carries every dependent with it and NO follow-state is stored. The walk is
bounded by the body count, so a cycle fails closed (nothing drawn) rather than
recursing without end. The renderer and the picker consume the resolved model
from the snapshot, so what is seen and what is hit both follow the producer.

## Cycles

At R1 only producer -> newly-created-dependent references arise, but cycle
detection is enforced anyway: the scene resolve is depth-bounded, and the codec
walks the producer chain of every face-supported body and refuses a chain longer
than the body count or one that returns to the body itself.

## Delete

A producer with dependents is REFUSED (`RefusedHasDependents` /
`DELETE_REFUSED_HAS_DEPENDENTS`), never cascaded: a cascade would destroy bodies
the user did not name. Delete the dependents first, then the producer. Undo/Redo
of a dependent's creation restores the exact TopoRef.

## History

Creating a face-supported body is ONE project-history step. Undo removes only
the dependent, leaving the producer untouched; Redo restores the same ObjectId,
state and TopoRef. A parent parameter edit is one step whose dependent
regeneration is a derived consequence, not a second entry.

## Verified

`CADA3-28..38` (native): dependents reported, parent Move/Rotate carries the
child, a cap- and a side-supported child follow parent edits, an A->B->C chain
resolves (32 deep in ~70 us), and every cycle/bad-ref/delete path fails closed.
Device `E2E-CADA3`: the producer cannot be deleted while a dependent stands.
