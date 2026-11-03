#include "forgeshape_project_surface.h"

namespace forgeshape {

namespace {

// The smallest sketch record (id, offset, plane, next entity id, entity count,
// one circle, next dimension id, dimension count) and the fixed part of a
// feature record, for the truncation checks made before any allocation.
constexpr uint64_t kSurfaceSketchMinBytes = 4u + 8u + 1u + 4u + 4u + (4u + 1u + 1u + 24u) + 4u + 4u;
constexpr uint64_t kSurfaceFeatureFixedBytes =
        4u + 1u + (4u + 4u) + 4u + (8u + 1u) + (4u + 4u + 8u + 1u) + (4u + 4u) + (1u + 4u)
        + (4u + 1u) + 4u + (4u + 8u);

void writeSection(ByteWriter& out, const SurfaceSection& section) {
    out.u32(section.sketchId);
    out.u32(static_cast<uint32_t>(section.curves.size()));
    for (SketchEntityId id : section.curves) out.u32(id);
}

ProjectCodecStatus readSection(ByteReader& in, SurfaceSection* section) {
    uint32_t count = 0;
    if (!in.u32(&section->sketchId) || !in.u32(&count)) return ProjectCodecStatus::Truncated;
    if (count > kMaxSurfaceSectionCurves) return ProjectCodecStatus::ImpossibleCount;
    if (static_cast<uint64_t>(count) * 4u > in.remaining()) return ProjectCodecStatus::Truncated;
    section->curves.resize(count);
    for (SketchEntityId& id : section->curves) {
        if (!in.u32(&id)) return ProjectCodecStatus::Truncated;
    }
    return ProjectCodecStatus::Ok;
}

bool readBool(ByteReader& in, bool* out, ProjectCodecStatus* why) {
    uint8_t value = 0;
    if (!in.u8(&value)) {
        *why = ProjectCodecStatus::Truncated;
        return false;
    }
    if (value > 1u) {
        // A boolean byte is 0 or 1; anything else is refused, never masked.
        *why = ProjectCodecStatus::BadPayload;
        return false;
    }
    *out = value == 1u;
    return true;
}

void writeFeature(ByteWriter& out, const SurfaceFeature& f) {
    out.u32(idOf(f.id));
    out.u8(static_cast<uint8_t>(f.kind));
    writeSection(out, f.section);
    out.u32(static_cast<uint32_t>(f.regions.size()));
    for (const ProfileRegionRef& region : f.regions) {
        out.u32(region.outerAnchorId);
        out.u32(static_cast<uint32_t>(region.holeAnchorIds.size()));
        for (SketchEntityId hole : region.holeAnchorIds) out.u32(hole);
    }
    out.f64(f.distance);
    out.u8(extrudeDirectionFileCode(f.direction));
    out.u32(f.axis.entityId);
    out.u32(f.axis.edgeLocalIndex);
    out.f64(f.angleDegrees);
    out.u8(revolveDirectionFileCode(f.revolveDirection));
    writeSection(out, f.sectionB);
    out.u8(f.reverseB ? 1u : 0u);
    out.u32(f.startOffsetB);
    out.u32(idOf(f.target));
    out.u8(f.keepInside ? 1u : 0u);
    out.u32(static_cast<uint32_t>(f.stitchFeatures.size()));
    for (SurfaceFeatureId id : f.stitchFeatures) out.u32(idOf(id));
    out.u32(idOf(f.source));
    out.f64(f.thickness);
}

ProjectCodecStatus readFeature(ByteReader& in, SurfaceFeature* f) {
    uint32_t id = 0;
    uint8_t kind = 0;
    if (!in.u32(&id) || !in.u8(&kind)) return ProjectCodecStatus::Truncated;
    f->id = SurfaceFeatureId{id};
    if (!surfaceFeatureKindFromCode(kind, &f->kind)) return ProjectCodecStatus::InvalidSemanticValue;
    ProjectCodecStatus why = readSection(in, &f->section);
    if (why != ProjectCodecStatus::Ok) return why;
    uint32_t regionCount = 0;
    if (!in.u32(&regionCount)) return ProjectCodecStatus::Truncated;
    if (regionCount > kMaxProfileRegions) return ProjectCodecStatus::ImpossibleCount;
    if (static_cast<uint64_t>(regionCount) * 8u > in.remaining()) return ProjectCodecStatus::Truncated;
    f->regions.resize(regionCount);
    for (ProfileRegionRef& region : f->regions) {
        uint32_t holes = 0;
        if (!in.u32(&region.outerAnchorId) || !in.u32(&holes)) return ProjectCodecStatus::Truncated;
        if (holes > kMaxRegionHoles) return ProjectCodecStatus::ImpossibleCount;
        if (static_cast<uint64_t>(holes) * 4u > in.remaining()) return ProjectCodecStatus::Truncated;
        region.holeAnchorIds.resize(holes);
        for (SketchEntityId& hole : region.holeAnchorIds) {
            if (!in.u32(&hole)) return ProjectCodecStatus::Truncated;
        }
    }
    uint8_t code = 0;
    if (!in.f64(&f->distance) || !in.u8(&code)) return ProjectCodecStatus::Truncated;
    if (!extrudeDirectionFromFileCode(code, &f->direction)) return ProjectCodecStatus::InvalidSemanticValue;
    if (!in.u32(&f->axis.entityId) || !in.u32(&f->axis.edgeLocalIndex) || !in.f64(&f->angleDegrees)
        || !in.u8(&code)) {
        return ProjectCodecStatus::Truncated;
    }
    if (!revolveDirectionFromFileCode(code, &f->revolveDirection)) return ProjectCodecStatus::InvalidSemanticValue;
    why = readSection(in, &f->sectionB);
    if (why != ProjectCodecStatus::Ok) return why;
    if (!readBool(in, &f->reverseB, &why)) return why;
    uint32_t value = 0;
    if (!in.u32(&f->startOffsetB) || !in.u32(&value)) return ProjectCodecStatus::Truncated;
    f->target = SurfaceFeatureId{value};
    if (!readBool(in, &f->keepInside, &why)) return why;
    uint32_t stitchCount = 0;
    if (!in.u32(&stitchCount)) return ProjectCodecStatus::Truncated;
    if (stitchCount > kMaxSurfaceStitchFeatures) return ProjectCodecStatus::ImpossibleCount;
    if (static_cast<uint64_t>(stitchCount) * 4u > in.remaining()) return ProjectCodecStatus::Truncated;
    f->stitchFeatures.resize(stitchCount);
    for (SurfaceFeatureId& stitched : f->stitchFeatures) {
        if (!in.u32(&value)) return ProjectCodecStatus::Truncated;
        stitched = SurfaceFeatureId{value};
    }
    if (!in.u32(&value) || !in.f64(&f->thickness)) return ProjectCodecStatus::Truncated;
    f->source = SurfaceFeatureId{value};
    return ProjectCodecStatus::Ok;
}

}  // namespace

void writeSurfacePayload(ByteWriter& out, const ProjectSurfaceRecord& record) {
    out.u32(static_cast<uint32_t>(record.bodies.size()));
    for (const ProjectSurfaceBody& body : record.bodies) {
        const SurfaceBodyState& state = body.state;
        out.u64(body.objectId);
        out.u32(state.nextSketchId);
        out.u32(state.nextFeatureId);
        out.u32(static_cast<uint32_t>(state.sketches.size()));
        out.u32(static_cast<uint32_t>(state.features.size()));
        for (const SurfaceSketchRecord& sketch : state.sketches) {
            out.u32(sketch.id);
            out.f64(sketch.offset);
            out.u8(workplaneFileCode(sketch.sketch.plane));
            out.u32(sketch.sketch.nextEntityId);
            writeProjectSketchEntities(out, sketch.sketch);
            writeProjectSketchDimensions(out, sketch.sketch);
        }
        for (const SurfaceFeature& feature : state.features) writeFeature(out, feature);
    }
}

ProjectCodecStatus decodeSurfacePayload(ByteReader& in, ProjectSurfaceRecord* record) {
    uint32_t bodyCount = 0;
    if (!in.u32(&bodyCount)) return ProjectCodecStatus::Truncated;
    if (bodyCount == 0 || bodyCount > kMaxProjectBodies) return ProjectCodecStatus::ImpossibleCount;
    if (static_cast<uint64_t>(bodyCount) * (8u + 16u) > in.remaining()) return ProjectCodecStatus::Truncated;
    record->bodies.resize(bodyCount);
    for (ProjectSurfaceBody& body : record->bodies) {
        SurfaceBodyState& state = body.state;
        uint32_t sketchCount = 0;
        uint32_t featureCount = 0;
        if (!in.u64(&body.objectId) || !in.u32(&state.nextSketchId) || !in.u32(&state.nextFeatureId)
            || !in.u32(&sketchCount) || !in.u32(&featureCount)) {
            return ProjectCodecStatus::Truncated;
        }
        if (sketchCount > kMaxSurfaceSketches || featureCount == 0 || featureCount > kMaxSurfaceFeatures) {
            return ProjectCodecStatus::ImpossibleCount;
        }
        if (sketchCount * kSurfaceSketchMinBytes + featureCount * kSurfaceFeatureFixedBytes > in.remaining()) {
            return ProjectCodecStatus::Truncated;
        }
        state.sketches.resize(sketchCount);
        for (SurfaceSketchRecord& sketch : state.sketches) {
            uint8_t plane = 0;
            uint32_t entityCount = 0;
            if (!in.u32(&sketch.id) || !in.f64(&sketch.offset) || !in.u8(&plane)
                || !in.u32(&sketch.sketch.nextEntityId) || !in.u32(&entityCount)) {
                return ProjectCodecStatus::Truncated;
            }
            if (!workplaneFromFileCode(plane, &sketch.sketch.plane)) return ProjectCodecStatus::InvalidSemanticValue;
            ProjectCodecStatus why = readProjectSketchEntities(in, entityCount, &sketch.sketch);
            if (why != ProjectCodecStatus::Ok) return why;
            why = readProjectSketchDimensions(in, &sketch.sketch);
            if (why != ProjectCodecStatus::Ok) return why;
        }
        state.features.resize(featureCount);
        for (SurfaceFeature& feature : state.features) {
            const ProjectCodecStatus why = readFeature(in, &feature);
            if (why != ProjectCodecStatus::Ok) return why;
        }
    }
    if (!in.atEnd()) return ProjectCodecStatus::BadPayload;
    return ProjectCodecStatus::Ok;
}

bool sameProjectSurfaceRecord(const ProjectSurfaceRecord& a, const ProjectSurfaceRecord& b) {
    if (a.bodies.size() != b.bodies.size()) return false;
    for (size_t i = 0; i < a.bodies.size(); ++i) {
        if (a.bodies[i].objectId != b.bodies[i].objectId
            || !sameSurfaceBodyState(a.bodies[i].state, b.bodies[i].state)) {
            return false;
        }
    }
    return true;
}

}  // namespace forgeshape
