// A Construction primitive becomes a CAD part of the same size
// (`CAD-PRIMITIVES-R1`): a Box or a Cylinder as a sketch extruded
// symmetrically about its centre, a Sphere, a Cone or a Capsule as a profile revolved
// about its axis -- every one in the primitive's own local frame, so the part
// stands exactly where the primitive stood and then takes holes, fillets,
// patterns and later features like any CAD body.
//
// Platform-neutral C++17. A body keeps ONE representation for its whole life,
// so this is not a change of representation: the act ADDS a new CAD body with
// the primitive's placement, name, visibility and lock and REMOVES the
// primitive, in ONE `ScopedConstructionEdit` -- one Undo brings the primitive
// back, Redo the part. A Plane has no CAD part (it has no volume), and a body
// carrying a sculpt mesh is refused (the strokes would be lost).
#pragma once

#include "forgeshape_cad_body.h"
#include "forgeshape_construction.h"
#include "forgeshape_history.h"
#include "forgeshape_scene.h"

namespace forgeshape {

// Whether `spec` has a CAD part.
bool primitiveHasCadPart(const PrimitiveSpec& spec);

// The CAD part of `spec`, in the primitive's local frame. `PrimitiveNotConvertible`
// for a Plane; the state is validated (`validateCadBodyState`).
CadStatus cadBodyStateFromPrimitive(const PrimitiveSpec& spec, CadBodyState* out);

// The act. Refused (nothing changes) for no such body, a body that is not a
// Construction primitive with a CAD part, a body carrying a sculpt mesh, or an
// edit in progress. The new part's id into `outPartId`; it becomes active.
CadStatus convertPrimitiveToCadPart(ConstructionScene& scene, ConstructionHistory& history,
                                    ObjectId bodyId, ObjectId* outPartId);

}  // namespace forgeshape
