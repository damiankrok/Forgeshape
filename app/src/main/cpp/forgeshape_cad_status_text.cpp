#include "forgeshape_cad_status_text.h"

namespace forgeshape {

const char* cadStatusUserText(CadStatus status) {
    switch (status) {
        case CadStatus::Ok:
            return "Done.";
        case CadStatus::NonFinite:
            return "That value is not a number.";
        case CadStatus::OutOfRange:
            return "That value is out of range.";
        case CadStatus::ZeroLengthLine:
            return "A length must be a positive number.";
        case CadStatus::DuplicateEdge:
            return "Two curves lie on top of each other; delete one.";
        case CadStatus::TooFewVertices:
            return "A polyline needs at least two points.";
        case CadStatus::ZeroSizeRectangle:
            return "A rectangle needs a width and a height above zero.";
        case CadStatus::InvalidCircleRadius:
            return "A circle needs a radius above zero.";
        case CadStatus::TooManyEntities:
            return "The sketch has as many curves as it can hold.";
        case CadStatus::UnknownEntity:
            return "That curve is no longer in the sketch.";
        case CadStatus::InvalidWorkplane:
            return "A sketch on a face cannot move to a world plane.";
        case CadStatus::OpenProfile:
            return "The profile is open — close the loop, or tap the polyline's first point to "
                   "close it.";
        case CadStatus::SelfIntersectingProfile:
            return "The profile crosses itself. Redraw it without crossings.";
        case CadStatus::ZeroAreaProfile:
            return "The profile has no area.";
        case CadStatus::BranchingChain:
            return "Lines meet at a fork — a profile is one loop.";
        case CadStatus::NoClosedProfile:
            return "No closed profile yet — draw a rectangle, a circle, or a closed loop.";
        case CadStatus::AmbiguousProfile:
            return "Several profiles are closed — choose one in Sketch values.";
        case CadStatus::ProfileNotFound:
            return "The region chosen is no longer in the sketch; choose a region.";
        case CadStatus::NestedProfileUnsupported:
            return "One profile lies inside another. Holes are not supported yet — extrude the "
                   "inner one, or delete it.";
        case CadStatus::InvalidExtrudeDepth:
            return "Depth must be a positive length.";
        case CadStatus::InvalidExtrudeDirection:
            return "That direction cannot be used for this extrusion.";
        case CadStatus::TriangulationFailed:
            return "The shape could not be made into a solid; simplify the sketch.";
        case CadStatus::RegenerationFailed:
            return "The part could not be rebuilt with that change.";
        case CadStatus::NotSketching:
            return "Not available now.";
        case CadStatus::NotCadBody:
            return "That works on a CAD part only.";
        case CadStatus::RefusedEditInProgress:
            return "Finish the current edit first.";
        case CadStatus::InvalidArc:
            return "Those three points do not describe an arc — they lie on a straight line, or "
                   "two of them are the same point.";
        case CadStatus::InvalidSpline:
            return "A spline needs at least two distinct points, and its two ends must be "
                   "different points.";
        case CadStatus::SketchNotEmpty:
            return "The support plane is fixed once the sketch has geometry. Delete it, or cancel "
                   "the sketch, to change plane.";
        case CadStatus::DependentFaceLost:
            return "That edit would remove a face another body is sketched on. Edit or delete the "
                   "dependent body first.";
        case CadStatus::InvalidExtrudeExtent:
            return "That extent is not one this build can carry.";
        case CadStatus::ProfileRegionMismatch:
            return "That region is no longer in the sketch — choose it again.";
        case CadStatus::OverlappingRegions:
            return "Those regions overlap — one extrusion cannot use both.";
        case CadStatus::OverlappingHoles:
            return "Holes in one region may not overlap.";
        case CadStatus::TooManyRegions:
            return "Too many regions in one extrusion.";
        case CadStatus::InvalidFeatureOperation:
            return "That operation is not available here.";
        case CadStatus::TooManyFeatures:
            return "This body has as many features as it can hold.";
        case CadStatus::FeatureSupportInvalid:
            return "The face this feature stands on no longer exists.";
        case CadStatus::SupportFaceLost:
            return "That change would remove the face a later feature stands on.";
        case CadStatus::OperationNeedsTarget:
            return "Add and Cut need a sketch on a body's face.";
        case CadStatus::AddDisjoint:
            return "Add does not touch the body — it would make a separate piece.";
        case CadStatus::AddNoEffect:
            return "Add changes nothing — the new material is already inside the body.";
        case CadStatus::CutNoIntersection:
            return "Cut misses the body — nothing would be removed.";
        case CadStatus::CutRemovesBody:
            return "Cut would remove the whole body.";
        case CadStatus::KernelFailed:
            return "The solid could not be computed from this feature.";
        case CadStatus::SketchIdInvalid:
            return "That sketch cannot be found.";
        case CadStatus::DuplicateSketchId:
            return "Two sketches share an id; the project is damaged.";
        case CadStatus::SketchNotFound:
            return "That sketch is no longer in the part.";
        case CadStatus::HighWaterInvalid:
            return "The part's ids are damaged and it cannot be changed.";
        case CadStatus::TooManySketches:
            return "The part has as many sketches as it can hold.";
        case CadStatus::SketchSupportInvalid:
            return "That sketch has nothing to stand on any more.";
        case CadStatus::InvalidSelectionKind:
            return "That selection cannot be used.";
        case CadStatus::PlanarFaceRefMalformed:
            return "A chosen area is damaged; choose it again.";
        case CadStatus::PlanarFaceRefNotCanonical:
            return "A chosen area is damaged; choose it again.";
        case CadStatus::DuplicatePlanarFace:
            return "That area is chosen twice.";
        case CadStatus::PlanarFaceUnresolved:
            return "A chosen area is no longer in the sketch; choose it again.";
        case CadStatus::PlanarFaceUnsupportedCurve:
            return "Areas bounded by that curve cannot be chosen one by one.";
        case CadStatus::PlanarFaceAmbiguousOverlap:
            return "Two curves run along each other — the areas between them cannot be told apart.";
        case CadStatus::PlanarFaceCapExceeded:
            return "Too many crossings in this sketch to split it into areas.";
        case CadStatus::PlanarFaceDegenerate:
            return "Two curves enclose an area too thin to extrude.";
        case CadStatus::PlanarFaceRegenerationUnavailable:
            return "The chosen areas cannot be rebuilt; choose them again.";
        case CadStatus::PlanarFacesTouchAtPoint:
            return "Those areas meet only at a point — add the area between them, or extrude them "
                   "separately.";
        case CadStatus::InvalidFeatureKind:
            return "That kind of feature cannot be used here.";
        case CadStatus::RevolveAxisUnresolved:
            return "The axis edge is no longer in the sketch. Choose another axis.";
        case CadStatus::RevolveAxisNotStraight:
            return "That edge is curved. An axis must be a straight edge: a line, a polyline "
                   "segment or a rectangle side.";
        case CadStatus::RevolveAxisDegenerate:
            return "The revolve axis has no length; choose a longer straight edge.";
        case CadStatus::RevolveAngleInvalid:
            return "The angle must be more than 0° and at most 360°.";
        case CadStatus::RevolveDirectionInvalid:
            return "That revolve direction cannot be used.";
        case CadStatus::RevolveProfileCrossesAxis:
            return "The area crosses the axis. Revolve an area that lies on one side of it.";
        case CadStatus::RevolveZeroRadius:
            return "The area lies on the axis, so it sweeps no volume.";
        case CadStatus::RevolveComponentsOverlap:
            return "Areas on both sides of the axis would overlap when revolved. Revolve them "
                   "separately.";
        case CadStatus::RevolvePayloadMismatch:
            return "That revolve is damaged and cannot be used.";
        case CadStatus::RevolveLaterFeatureUnsupported:
            return "A revolve here is not supported.";
        case CadStatus::RevolveNeedsAxis:
            return "Choose an axis — tap a straight edge of the sketch.";
        case CadStatus::SketchDimensionInvalid:
            return "That dimension does not apply to this geometry.";
        case CadStatus::SketchDimensionConflict:
            return "A dimension already drives this value.";
        case CadStatus::SketchDimensionDependency:
            return "Remove the dimension before trimming this entity.";
        case CadStatus::SketchDimensionLocked:
            return "A driving dimension holds this length. Remove it first.";
        case CadStatus::SketchDimensionReadOnly:
            return "A reference dimension is measured, not typed.";
        case CadStatus::SketchDimensionValueInvalid:
            return "Rejected: that value is out of range. Sketch unchanged.";
        case CadStatus::TrimSplineUnsupported:
            return "Trim does not support a spline.";
        case CadStatus::DraftingNoTarget:
            return "Nothing under your finger to change.";
        case CadStatus::ExtendUnsupported:
            return "Only a line, an open polyline or an arc can be extended.";
        case CadStatus::ExtendNoTarget:
            return "Nothing to extend to.";
        case CadStatus::ExtendAmbiguous:
            return "The extension would run along another curve.";
        case CadStatus::OffsetSplineUnsupported:
            return "Offset does not support a spline.";
        case CadStatus::OffsetInvalid:
            return "Rejected: that offset would collapse the shape.";
        case CadStatus::OffsetMiterLimit:
            return "Rejected: a corner is too sharp to offset.";
        case CadStatus::OffsetSelfIntersecting:
            return "Rejected: the offset would cross itself.";
        case CadStatus::MirrorAxisInvalid:
            return "Choose a straight edge as the mirror axis.";
        case CadStatus::MirrorNothingSelected:
            return "Select what to mirror first.";
        case CadStatus::SketchInUse:
            return "A feature still uses this sketch.";
        case CadStatus::SketchIsRoot:
            return "The body's first sketch says where the body is and can't be removed.";
        case CadStatus::EdgeNotFound:
            return "That edge is no longer on the body, so the finish cannot find it.";
        case CadStatus::EdgeNotEligible:
            return "Only straight, outward edges between two flat faces can be rounded or "
                   "bevelled.";
        case CadStatus::EdgeFinishTargetUnsupported:
            return "Edges of a cut or a revolve cannot be rounded or bevelled yet.";
        case CadStatus::EdgeFinishInvalid:
            return "Type a positive size for the finish.";
        case CadStatus::EdgeFinishTooLarge:
            return "That size does not fit: it is wider than one of the faces at the edge.";
        case CadStatus::EdgeFinishNoEffect:
            return "The finished edge is gone: a later step removed it.";
        case CadStatus::TooManyEdgeFinishes:
            return "This body already has the most finishes it can hold.";
        case CadStatus::PatternInvalid:
            return "Those pattern values do not work: use at least 2 copies (at most 64), a "
                   "positive spacing and an angle up to 360°.";
        case CadStatus::PatternFeatureNotFound:
            return "The feature this pattern repeats is no longer there.";
        case CadStatus::PatternTargetUnsupported:
            return "Only a later Add or Cut can be patterned, not the first shape of a part.";
        case CadStatus::PatternInstanceNoEffect:
            return "One of the copies changes nothing: it misses the part or lands on another "
                   "copy. Change the spacing, the count or the axis.";
        case CadStatus::TooManyPatterns:
            return "This part already has the most patterns it can hold.";
        case CadStatus::HoleInvalid:
            return "Those hole values do not work: a counterbore or a cone must be wider than the "
                   "hole and shallower than its depth, and the cone angle between 30° and 150°.";
        case CadStatus::HoleSupportInvalid:
            return "The face this hole was drilled into is no longer there.";
        case CadStatus::HoleNoEffect:
            return "That hole removes nothing: it misses the part there. Move it onto the "
                   "material.";
        case CadStatus::TooManyHoles:
            return "This part already has the most holes it can hold.";
        case CadStatus::HoleNotFound:
            return "That hole is no longer there.";
        case CadStatus::ThroughAllUnsupported:
            return "Only a Cut can go through all: an Add or the first shape of a part needs a "
                   "distance.";
        case CadStatus::ThroughAllReachesNothing:
            return "There is no material on that side to cut through. Flip the cut into the part.";
        case CadStatus::EdgeNotOnBody:
            return "That edge is where two features meet, not an outside edge of the part, so it "
                   "cannot be rounded or bevelled.";
        case CadStatus::ToFaceUnsupported:
            return "To Face ends an Add or a Cut on a face of the part parallel to the sketch, and "
                   "there is none here.";
        case CadStatus::ToFaceTargetInvalid:
            return "That face cannot end this extrusion: choose a flat face of the same part made "
                   "before it.";
        case CadStatus::ToFaceNotParallel:
            return "That face is not parallel to the sketch, so the extrusion cannot end on it.";
        case CadStatus::ToFaceBehind:
            return "There is no face of the part to reach on that side.";
        case CadStatus::HoleEdgeNotFound:
            return "That is not an edge of the hole's face.";
        case CadStatus::HoleEdgesParallel:
            return "The two edges run the same way, so they cannot place the hole.";
        case CadStatus::SketchPlaneInvalid:
            return "That plane cannot be used: the angle must be within 90° and the offset within "
                   "the sketch's reach, and only a sketch on a face of the part can stand off it.";
        case CadStatus::PrimitiveNotConvertible:
            return "Only a box, a cylinder, a sphere, a cone or a capsule that has not been "
                   "sculpted can become a CAD part.";
        case CadStatus::IntersectRemovesBody:
            return "The extrusion shares no material with the part, so Intersect would leave "
                   "nothing.";
        case CadStatus::IntersectNoEffect:
            return "The extrusion holds the whole part, so Intersect would change nothing.";
    }
    return "That change was refused.";
}

}  // namespace forgeshape
