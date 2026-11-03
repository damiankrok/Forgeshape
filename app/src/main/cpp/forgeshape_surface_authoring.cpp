#include "forgeshape_surface_authoring.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"

namespace forgeshape {

const char* surfaceCreateKindName(SurfaceCreateKind kind) {
    switch (kind) {
        case SurfaceCreateKind::Patch: return "Patch";
        case SurfaceCreateKind::Extrude: return "Extrude";
        case SurfaceCreateKind::Revolve: return "Revolve";
        case SurfaceCreateKind::Loft: return "Loft";
        case SurfaceCreateKind::Trim: return "Trim";
        case SurfaceCreateKind::Section: return "Section";
    }
    return "unknown";
}

bool surfaceCreateKindFromCode(int code, SurfaceCreateKind* out) {
    if (code < 1 || code > kSurfaceCreateKindCount) return false;
    if (out != nullptr) *out = static_cast<SurfaceCreateKind>(code);
    return true;
}

std::vector<SketchEntityId> surfaceChosenCurves(const SketchSession& sketch) {
    // A drawing tool leaves the entity it just placed selected; only a
    // selection the user made with Select is a choice of curves.
    return sketch.tool() == SketchTool::Select ? sketch.selectedEntityIds() : std::vector<SketchEntityId>{};
}

SurfaceSketchPurpose& surfaceSketchPurpose() {
    static SurfaceSketchPurpose purpose;
    return purpose;
}

namespace {

bool nearlySame(const DVec3& x, const DVec3& y) {
    return std::fabs(x.x - y.x) <= 1e-12 && std::fabs(x.y - y.y) <= 1e-12 && std::fabs(x.z - y.z) <= 1e-12;
}

bool samePlane(const CadFrame64& a, const CadFrame64& b) {
    return nearlySame(a.u, b.u) && nearlySame(a.v, b.v) && nearlySame(a.n, b.n) && nearlySame(a.origin, b.origin);
}

bool straightEdge(const SketchEntity& entity) {
    if (entity.line() != nullptr) return true;
    const SketchPolyline* p = entity.polyline();
    return p != nullptr && !p->closed && p->vertices.size() == 2u;
}

SurfaceStatus regenerateCandidate(const SurfaceBodyState& state, SurfaceRegenerationReport* report) {
    SurfaceRegenerationReport local;
    const SurfaceStatus why = regenerateSurfaceBody(state, nullptr, &local);
    if (report != nullptr) *report = local;
    return why;
}

// The planar patch feature a Trim on this sketch clips: the latest live
// planar patch standing on exactly the sketch's plane.
SurfaceFeatureId trimTarget(const SurfaceBodyState& base, const CadSketch& sketch, double offset) {
    SurfaceBodyMesh mesh;
    if (regenerateSurfaceBody(base, &mesh) != SurfaceStatus::Ok) return kNoSurfaceFeature;
    SurfaceSketchRecord probe;
    probe.offset = offset;
    probe.sketch = sketch;
    const CadFrame64 frame = surfaceSketchFrame(probe);
    for (auto it = mesh.patches.rbegin(); it != mesh.patches.rend(); ++it) {
        if (it->shape == SurfacePatchShape::Planar && samePlane(it->frame, frame)) return it->id.feature;
    }
    return kNoSurfaceFeature;
}

// One Construction transaction over an existing Surface body.
SurfaceStatus applyToBody(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                          const SurfaceBodyState& next, SurfaceRegenerationReport* report) {
    if (history.editInProgress()) return SurfaceStatus::EditInProgress;
    SceneObject* object = scene.findBody(bodyId);
    if (object == nullptr || object->surfaceOrNull() == nullptr) return SurfaceStatus::NotSurfaceBody;
    SurfaceStatus why = SurfaceStatus::Ok;
    {
        ScopedConstructionEdit edit(history);
        why = object->surfaceOrNull()->applyState(next, nullptr, report);
        if (why == SurfaceStatus::Ok) publishSceneObject(*object);
    }
    return why;
}

const SurfaceBody* surfaceOf(const ConstructionScene& scene, ObjectId bodyId) {
    const SceneObject* object = scene.findBody(bodyId);
    return object != nullptr ? object->surfaceOrNull() : nullptr;
}

}  // namespace

std::vector<SketchEntityId> surfaceSweepCurves(const CadSketch& sketch, const std::vector<SketchEntityId>& selected) {
    std::vector<SketchEntityId> out;
    for (SketchEntityId id : selected) {
        const SketchEntity* entity = findSketchEntity(sketch, id);
        if (entity != nullptr && !entity->construction()) out.push_back(id);
    }
    if (out.empty()) out = surfaceDefaultCurves(sketch, kNoSketchEntity);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

SurfaceStatus surfaceRevolveAxis(const CadSketch& sketch, CadSketchEdgeRef* out) {
    const SketchEntity* axis = nullptr;
    for (const SketchEntity& entity : sketch.entities) {
        if (!entity.construction() || !straightEdge(entity)) continue;
        if (axis != nullptr) return SurfaceStatus::AxisUnresolved;
        axis = &entity;
    }
    if (axis == nullptr) return SurfaceStatus::AxisUnresolved;
    if (out != nullptr) *out = CadSketchEdgeRef{axis->id(), 0};
    return SurfaceStatus::Ok;
}

uint32_t surfacePendingSection(const SurfaceBodyState& state) {
    for (auto it = state.sketches.rbegin(); it != state.sketches.rend(); ++it) {
        bool used = false;
        for (const SurfaceFeature& f : state.features) {
            used = used || f.section.sketchId == it->id || f.sectionB.sketchId == it->id;
        }
        if (!used) return it->id;
    }
    return 0u;
}

SurfaceStatus surfaceCandidateFromSketch(const SurfaceBodyState* base, const CadSketch& sketch, double offset,
                                         const std::vector<SketchEntityId>& selected,
                                         const SurfaceCreateRequest& request, SurfaceBodyState* out,
                                         SurfaceRegenerationReport* report) {
    if (report != nullptr) *report = SurfaceRegenerationReport{};
    SurfaceBodyState state = base != nullptr ? *base : SurfaceBodyState{};
    // A Loft, a Trim and a Section need a body to belong to.
    if (base == nullptr && (request.kind == SurfaceCreateKind::Loft || request.kind == SurfaceCreateKind::Trim
                            || request.kind == SurfaceCreateKind::Section)) {
        return SurfaceStatus::FirstFeatureInvalid;
    }
    const uint32_t pending = base != nullptr ? surfacePendingSection(*base) : 0u;
    if (request.kind == SurfaceCreateKind::Loft && pending == 0u) return SurfaceStatus::SectionMissing;
    SurfaceFeatureId target = kNoSurfaceFeature;
    if (request.kind == SurfaceCreateKind::Trim) {
        target = trimTarget(*base, sketch, offset);
        if (target == kNoSurfaceFeature) return SurfaceStatus::TrimTargetMissing;
    }
    const uint32_t sketchId = appendSurfaceSketch(&state, sketch, offset);
    if (sketchId == 0u) {
        return state.sketches.size() >= kMaxSurfaceSketches ? SurfaceStatus::TooManySketches
                                                            : SurfaceStatus::SketchInvalid;
    }
    if (request.kind != SurfaceCreateKind::Section) {
        SurfaceFeature f;
        f.section.sketchId = sketchId;
        switch (request.kind) {
            case SurfaceCreateKind::Patch:
                f.kind = SurfaceFeatureKind::PlanarPatch;
                f.regions = surfaceDefaultPatchRegions(sketch);
                if (f.regions.empty()) return SurfaceStatus::RegionInvalid;
                break;
            case SurfaceCreateKind::Extrude:
                f.kind = SurfaceFeatureKind::ExtrudedSurface;
                f.section.curves = surfaceSweepCurves(sketch, selected);
                if (f.section.curves.empty()) return SurfaceStatus::CurveSectionEmpty;
                f.distance = request.distance;
                break;
            case SurfaceCreateKind::Revolve: {
                f.kind = SurfaceFeatureKind::RevolvedSurface;
                const SurfaceStatus axis = surfaceRevolveAxis(sketch, &f.axis);
                if (axis != SurfaceStatus::Ok) return axis;
                f.section.curves = surfaceSweepCurves(sketch, selected);
                if (f.section.curves.empty()) return SurfaceStatus::CurveSectionEmpty;
                f.angleDegrees = request.angleDegrees;
                break;
            }
            case SurfaceCreateKind::Loft: {
                f.kind = SurfaceFeatureKind::LoftSurface;
                const SurfaceSketchRecord* first = findSurfaceSketch(state, pending);
                f.section.sketchId = pending;
                f.section.curves = surfaceDefaultCurves(first->sketch, kNoSketchEntity);
                f.sectionB.sketchId = sketchId;
                f.sectionB.curves = surfaceSweepCurves(sketch, selected);
                if (f.section.curves.empty() || f.sectionB.curves.empty()) return SurfaceStatus::CurveSectionEmpty;
                break;
            }
            case SurfaceCreateKind::Trim:
                f.kind = SurfaceFeatureKind::TrimSurface;
                f.regions = surfaceDefaultPatchRegions(sketch);
                if (f.regions.empty()) return SurfaceStatus::TrimRegionInvalid;
                f.target = target;
                f.keepInside = request.keepInside;
                break;
            case SurfaceCreateKind::Section:
                break;
        }
        if (appendSurfaceFeature(&state, f) == kNoSurfaceFeature) return SurfaceStatus::TooManyFeatures;
    }
    if (state.features.empty()) return SurfaceStatus::NoFeatures;
    const SurfaceStatus why = regenerateCandidate(state, report);
    if (why != SurfaceStatus::Ok) return why;
    if (out != nullptr) *out = std::move(state);
    return SurfaceStatus::Ok;
}

SurfaceStatus surfaceCommitSketch(SketchSession& sketch, ConstructionScene& scene, SculptSession& sculpt,
                                  ConstructionHistory& history, const SurfaceCreateRequest& request,
                                  ObjectId* outBody, SurfaceRegenerationReport* report) {
    if (outBody != nullptr) *outBody = kNoObject;
    SurfaceSketchPurpose& purpose = surfaceSketchPurpose();
    if (!purpose.active || !sketch.active() || sketch.editingExistingBody()) return SurfaceStatus::NotSketching;
    if (history.editInProgress()) return SurfaceStatus::EditInProgress;
    const SurfaceBody* target = purpose.body != kNoObject ? surfaceOf(scene, purpose.body) : nullptr;
    if (purpose.body != kNoObject && target == nullptr) return SurfaceStatus::NotSurfaceBody;
    SurfaceBodyState state;
    const SurfaceStatus why =
        surfaceCandidateFromSketch(target != nullptr ? &target->state() : nullptr, sketch.sketch(),
                                   sketch.planeOffset(), surfaceChosenCurves(sketch), request, &state, report);
    if (why != SurfaceStatus::Ok) return why;

    ObjectId made = purpose.body;
    if (!scene.hasProject()) {
        // The first project: the same validated, all-or-nothing replace-the-
        // scene path Open takes, so it starts with an empty history.
        const ObjectId bodyId = scene.nextObjectId();
        ProjectDocument document;
        document.kind = ProjectKind::Construction;
        document.scene.nextObjectId = bodyId + 1;
        document.scene.activeObjectId = bodyId;
        ProjectBodyPlacement placement;
        placement.objectId = bodyId;
        document.scene.bodies.push_back(placement);
        document.hasSurface = true;
        ProjectSurfaceBody body;
        body.objectId = bodyId;
        body.state = state;
        document.surface.bodies.push_back(body);
        if (loadProjectDocument(document, scene, sculpt, history) != ProjectCodecStatus::Ok) {
            return SurfaceStatus::SketchInvalid;
        }
        made = bodyId;
    } else if (target == nullptr) {
        SceneObject* body = nullptr;
        SurfaceStatus added = SurfaceStatus::Ok;
        {
            ScopedConstructionEdit edit(history);
            body = scene.addSurfaceBody(state, &added);
            if (body != nullptr) publishSceneObject(*body);
        }
        if (body == nullptr) return added;
        made = body->objectId();
    } else {
        const SurfaceStatus applied = applyToBody(scene, history, purpose.body, state, report);
        if (applied != SurfaceStatus::Ok) return applied;
    }
    sketch.cancel();  // the sketch is over; the truth is now the body's
    purpose = SurfaceSketchPurpose{};
    if (outBody != nullptr) *outBody = made;
    return SurfaceStatus::Ok;
}

std::vector<SurfaceFeatureId> surfaceLiveFeatures(const SurfaceBodyMesh& mesh) {
    std::vector<SurfaceFeatureId> out;
    for (const SurfacePatch& patch : mesh.patches) {
        if (std::find(out.begin(), out.end(), patch.id.feature) == out.end()) out.push_back(patch.id.feature);
    }
    std::sort(out.begin(), out.end(), [](SurfaceFeatureId a, SurfaceFeatureId b) { return idOf(a) < idOf(b); });
    return out;
}

SurfaceStatus surfaceStitchCandidate(const SurfaceBody& body, SurfaceBodyState* out,
                                     SurfaceRegenerationReport* report) {
    if (report != nullptr) *report = SurfaceRegenerationReport{};
    const std::vector<SurfaceFeatureId> live = surfaceLiveFeatures(body.mesh());
    if (live.size() < 2u) return SurfaceStatus::NothingToStitch;
    SurfaceBodyState state = body.state();
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::Stitch;
    f.stitchFeatures = live;
    if (appendSurfaceFeature(&state, f) == kNoSurfaceFeature) return SurfaceStatus::TooManyFeatures;
    const SurfaceStatus why = regenerateCandidate(state, report);
    if (why == SurfaceStatus::Ok && out != nullptr) *out = std::move(state);
    return why;
}

SurfaceStatus surfaceStitch(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                            SurfaceRegenerationReport* report) {
    const SurfaceBody* body = surfaceOf(scene, bodyId);
    if (body == nullptr) return SurfaceStatus::NotSurfaceBody;
    SurfaceBodyState state;
    const SurfaceStatus why = surfaceStitchCandidate(*body, &state, report);
    return why != SurfaceStatus::Ok ? why : applyToBody(scene, history, bodyId, state, report);
}

SurfaceStatus surfaceThickenCandidate(const SurfaceBody& body, SurfaceFeatureId source, double thickness,
                                      SurfaceBodyState* out, SurfaceRegenerationReport* report) {
    if (report != nullptr) *report = SurfaceRegenerationReport{};
    SurfaceBodyState state = body.state();
    SurfaceFeature f;
    f.kind = SurfaceFeatureKind::Thicken;
    f.source = source;
    f.thickness = thickness;
    if (appendSurfaceFeature(&state, f) == kNoSurfaceFeature) return SurfaceStatus::TooManyFeatures;
    const SurfaceStatus why = regenerateCandidate(state, report);
    if (why == SurfaceStatus::Ok && out != nullptr) *out = std::move(state);
    return why;
}

SurfaceStatus surfaceThicken(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                             SurfaceFeatureId source, double thickness, SurfaceRegenerationReport* report) {
    const SurfaceBody* body = surfaceOf(scene, bodyId);
    if (body == nullptr) return SurfaceStatus::NotSurfaceBody;
    SurfaceBodyState state;
    const SurfaceStatus why = surfaceThickenCandidate(*body, source, thickness, &state, report);
    return why != SurfaceStatus::Ok ? why : applyToBody(scene, history, bodyId, state, report);
}

bool surfaceFeatureHasValue(SurfaceFeatureKind kind) {
    return kind == SurfaceFeatureKind::ExtrudedSurface || kind == SurfaceFeatureKind::RevolvedSurface
           || kind == SurfaceFeatureKind::Thicken;
}

bool surfaceValueOf(const SurfaceBodyState& state, SurfaceValueTarget target, uint32_t id, double* out) {
    if (target == SurfaceValueTarget::Sketch) {
        const SurfaceSketchRecord* record = findSurfaceSketch(state, id);
        if (record == nullptr) return false;
        if (out != nullptr) *out = record->offset;
        return true;
    }
    const SurfaceFeature* f = findSurfaceFeature(state, SurfaceFeatureId{id});
    if (f == nullptr || !surfaceFeatureHasValue(f->kind)) return false;
    if (out != nullptr) {
        *out = f->kind == SurfaceFeatureKind::ExtrudedSurface   ? f->distance
               : f->kind == SurfaceFeatureKind::RevolvedSurface ? f->angleDegrees
                                                                : f->thickness;
    }
    return true;
}

SurfaceStatus surfaceStateWithValue(const SurfaceBodyState& state, SurfaceValueTarget target, uint32_t id,
                                    double value, SurfaceBodyState* out) {
    if (!std::isfinite(value)) return SurfaceStatus::NonFinite;
    SurfaceBodyState next = state;
    if (target == SurfaceValueTarget::Sketch) {
        auto it = std::find_if(next.sketches.begin(), next.sketches.end(),
                               [id](const SurfaceSketchRecord& r) { return r.id == id; });
        if (it == next.sketches.end()) return SurfaceStatus::UnknownSketch;
        if (std::fabs(value) > kMaxSurfaceDistanceMeters) return SurfaceStatus::OutOfRange;
        it->offset = value;
    } else {
        auto it = std::find_if(next.features.begin(), next.features.end(),
                               [id](const SurfaceFeature& f) { return idOf(f.id) == id; });
        if (it == next.features.end()) return SurfaceStatus::UnknownFeature;
        switch (it->kind) {
            case SurfaceFeatureKind::ExtrudedSurface: it->distance = value; break;
            case SurfaceFeatureKind::RevolvedSurface: it->angleDegrees = value; break;
            case SurfaceFeatureKind::Thicken: it->thickness = value; break;
            default: return SurfaceStatus::PayloadMismatch;
        }
    }
    if (out != nullptr) *out = std::move(next);
    return SurfaceStatus::Ok;
}

SurfaceStatus surfaceApplyValue(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                                SurfaceValueTarget target, uint32_t id, double value,
                                SurfaceRegenerationReport* report) {
    if (report != nullptr) *report = SurfaceRegenerationReport{};
    const SurfaceBody* body = surfaceOf(scene, bodyId);
    if (body == nullptr) return SurfaceStatus::NotSurfaceBody;
    SurfaceBodyState next;
    const SurfaceStatus staged = surfaceStateWithValue(body->state(), target, id, value, &next);
    if (staged != SurfaceStatus::Ok) return staged;
    const SurfaceStatus why = regenerateCandidate(next, report);
    return why != SurfaceStatus::Ok ? why : applyToBody(scene, history, bodyId, next, report);
}

SurfaceTimeline buildSurfaceTimeline(const SurfaceBodyState& state, const SurfaceRegenerationReport& report,
                                     bool editingSet, SurfaceValueTarget editTarget, uint32_t editId) {
    SurfaceTimeline timeline;
    timeline.status = report.status;
    timeline.failedFeature = report.failedFeature;
    std::vector<uint8_t> listed(state.sketches.size(), 0u);
    uint32_t sketchOrdinal = 0;
    uint32_t featureOrdinal = 0;
    auto sketchRow = [&](size_t index) {
        if (listed[index]) return;
        listed[index] = 1u;
        const SurfaceSketchRecord& record = state.sketches[index];
        SurfaceTimelineRow row;
        row.id = record.id;
        row.sketchId = record.id;
        row.ordinal = ++sketchOrdinal;
        row.entityCount = static_cast<uint32_t>(record.sketch.entities.size());
        row.plane = record.sketch.plane;
        row.hasValue = true;
        row.value = record.offset;
        row.editing = editingSet && editTarget == SurfaceValueTarget::Sketch && editId == record.id;
        row.state = SurfaceTimelineRowState::Unused;
        for (const SurfaceFeature& f : state.features) {
            if (f.section.sketchId == record.id || f.sectionB.sketchId == record.id) {
                row.state = SurfaceTimelineRowState::Ok;
            }
        }
        timeline.rows.push_back(row);
    };
    auto indexOfSketch = [&state](uint32_t id) {
        for (size_t i = 0; i < state.sketches.size(); ++i) {
            if (state.sketches[i].id == id) return i;
        }
        return state.sketches.size();
    };
    bool failedSeen = false;
    for (const SurfaceFeature& f : state.features) {
        for (uint32_t id : {f.section.sketchId, f.sectionB.sketchId}) {
            const size_t at = id != 0u ? indexOfSketch(id) : state.sketches.size();
            if (at < state.sketches.size()) sketchRow(at);
        }
        SurfaceTimelineRow row;
        row.feature = true;
        row.id = idOf(f.id);
        row.sketchId = f.section.sketchId;
        row.ordinal = ++featureOrdinal;
        row.kind = f.kind;
        row.hasValue = surfaceValueOf(state, SurfaceValueTarget::Feature, row.id, &row.value);
        row.editing = editingSet && editTarget == SurfaceValueTarget::Feature && editId == row.id;
        if (failedSeen) {
            row.state = SurfaceTimelineRowState::NotRegenerated;
        } else if (report.status != SurfaceStatus::Ok && (report.failedFeature == f.id
                                                          || report.failedFeature == kNoSurfaceFeature)) {
            row.state = SurfaceTimelineRowState::Failed;
            row.status = report.status;
            failedSeen = true;
        }
        timeline.rows.push_back(row);
    }
    for (size_t i = 0; i < state.sketches.size(); ++i) sketchRow(i);
    return timeline;
}

}  // namespace forgeshape
