#include "forgeshape_cad_from_primitive.h"

#include <memory>
#include <string>
#include <utility>

#include "forgeshape_sketch_region.h"

namespace forgeshape {

namespace {

// A symmetric extrusion of the sketch's one region: the solid spans
// -half .. +half along the plane's normal, centred like the primitive.
CadBodyState symmetricPrism(CadSketch sketch, SketchEntityId profile, Meters half) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = profile;
    extrude.extent = ExtrudeExtentMode::Symmetric;
    extrude.depth = half;
    extrude.direction = ExtrudeDirection::AlongNormal;
    return makeCadBodyState(std::move(sketch), extrude);
}

// A full-turn revolve of the sketch's one region about `axis`.
CadStatus fullTurn(CadSketch sketch, SketchEntityId axis, CadBodyState* out) {
    const SketchRegionExtraction regions = extractSketchRegions(sketch);
    if (regions.regions.size() != 1u) {
        return CadStatus::NoClosedProfile;
    }
    RevolveFeature revolve;
    revolve.profileEntityId = regions.regions.front().outerAnchorId;
    revolve.axis = CadSketchEdgeRef{axis, 0u};
    revolve.angleDegrees = kMaxRevolveAngleDegrees;
    *out = makeCadRevolveBodyState(std::move(sketch), revolve);
    return CadStatus::Ok;
}

}  // namespace

bool primitiveHasCadPart(const PrimitiveSpec& spec) {
    switch (spec.kind()) {
        case PrimitiveKind::Box:
        case PrimitiveKind::Cylinder:
        case PrimitiveKind::Sphere:
        case PrimitiveKind::Cone:
            return true;
        case PrimitiveKind::Capsule:
        case PrimitiveKind::Plane:
            return false;
    }
    return false;
}

CadStatus cadBodyStateFromPrimitive(const PrimitiveSpec& spec, CadBodyState* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    CadBodyState state;
    CadStatus why = CadStatus::Ok;
    CadSketch sketch;
    SketchEntityId id = kNoSketchEntity;
    switch (spec.kind()) {
        case PrimitiveKind::Box: {
            // Width along X, depth along Z, height along Y: a rectangle on XZ
            // (u = +X, v = -Z, n = +Y) extruded half the height each way.
            const BoxDimensionsMeters& box = *spec.box();
            sketch.plane = Workplane::XZ;
            SketchRectangle rectangle;
            rectangle.center = SketchPoint{0.0, 0.0};
            rectangle.width = box.width;
            rectangle.height = box.depth;
            why = addSketchEntity(&sketch, rectangle, &id);
            if (why == CadStatus::Ok) state = symmetricPrism(sketch, id, 0.5 * box.height);
            break;
        }
        case PrimitiveKind::Cylinder: {
            const CylinderDimensionsMeters& cylinder = *spec.cylinder();
            sketch.plane = Workplane::XZ;
            SketchCircle circle;
            circle.center = SketchPoint{0.0, 0.0};
            circle.radius = 0.5 * cylinder.diameter;
            why = addSketchEntity(&sketch, circle, &id);
            if (why == CadStatus::Ok) state = symmetricPrism(sketch, id, 0.5 * cylinder.height);
            break;
        }
        case PrimitiveKind::Sphere: {
            // A half disc on XY (u = +X, v = +Y), its diameter on the Y axis,
            // revolved a full turn about that diameter.
            const double r = 0.5 * spec.sphere()->diameter;
            sketch.plane = Workplane::XY;
            SketchArc arc;
            arc.start = SketchPoint{0.0, -r};
            arc.mid = SketchPoint{r, 0.0};
            arc.end = SketchPoint{0.0, r};
            SketchEntityId arcId = kNoSketchEntity;
            why = addSketchEntity(&sketch, arc, &arcId);
            SketchLine diameter;
            diameter.start = SketchPoint{0.0, r};
            diameter.end = SketchPoint{0.0, -r};
            if (why == CadStatus::Ok) why = addSketchEntity(&sketch, diameter, &id);
            if (why == CadStatus::Ok) why = fullTurn(sketch, id, &state);
            break;
        }
        case PrimitiveKind::Cone: {
            // A right triangle on XY: the base from the axis out to the radius
            // at y = -h/2, the slant up to the apex at y = +h/2, and the axis.
            const ConeDimensionsMeters& cone = *spec.cone();
            const double r = 0.5 * cone.bottomDiameter;
            const double h = 0.5 * cone.height;
            sketch.plane = Workplane::XY;
            SketchLine base;
            base.start = SketchPoint{0.0, -h};
            base.end = SketchPoint{r, -h};
            SketchLine slant;
            slant.start = SketchPoint{r, -h};
            slant.end = SketchPoint{0.0, h};
            SketchLine axis;
            axis.start = SketchPoint{0.0, h};
            axis.end = SketchPoint{0.0, -h};
            SketchEntityId ignored = kNoSketchEntity;
            why = addSketchEntity(&sketch, base, &ignored);
            if (why == CadStatus::Ok) why = addSketchEntity(&sketch, slant, &ignored);
            if (why == CadStatus::Ok) why = addSketchEntity(&sketch, axis, &id);
            if (why == CadStatus::Ok) why = fullTurn(sketch, id, &state);
            break;
        }
        case PrimitiveKind::Capsule:
        case PrimitiveKind::Plane:
            return CadStatus::PrimitiveNotConvertible;
    }
    if (why != CadStatus::Ok) {
        return why;
    }
    const CadStatus valid = validateCadBodyState(state);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    *out = std::move(state);
    return CadStatus::Ok;
}

CadStatus convertPrimitiveToCadPart(ConstructionScene& scene, ConstructionHistory& history,
                                    ObjectId bodyId, ObjectId* outPartId) {
    if (history.editInProgress()) {
        return CadStatus::PrimitiveNotConvertible;
    }
    SceneObject* source = scene.findBody(bodyId);
    const ConstructionObject* construction =
            source != nullptr ? source->constructionOrNull() : nullptr;
    if (construction == nullptr || source->frozenSculpt().mesh.frozen()) {
        return CadStatus::PrimitiveNotConvertible;
    }
    CadBodyState state;
    const CadStatus built = cadBodyStateFromPrimitive(construction->spec(), &state);
    if (built != CadStatus::Ok) {
        return built;
    }
    // Read before the scene changes: adding a body can move the list.
    const TransformValues placement = source->transform().values();
    const std::string name = source->name();
    const bool visible = source->visible();
    const bool locked = source->locked();
    ObjectId partId = kNoObject;
    {
        ScopedConstructionEdit edit(history);
        SceneObject* part = scene.addCadBody(std::move(state));
        if (part == nullptr) {
            return CadStatus::RegenerationFailed;  // nothing was added
        }
        partId = part->objectId();
        part->transform().setValues(placement);
        part->setName(name);
        part->setVisible(visible);
        part->setLocked(locked);
        publishSceneObject(*part);
        // The primitive is HELD by the history, as a Delete holds it, so one
        // Undo brings back the very same body.
        history.holdDetachedBody(scene.detachBody(bodyId));
        scene.setActiveBody(partId);
    }
    if (outPartId != nullptr) *outPartId = partId;
    return CadStatus::Ok;
}

}  // namespace forgeshape
