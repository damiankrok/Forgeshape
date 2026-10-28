#include "forgeshape_cad_sketch_inventory.h"

#include <algorithm>
#include <utility>

#include "forgeshape_cad_feature.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"

namespace forgeshape {

const char* cadSketchSupportKindName(CadSketchSupportKind kind) {
    switch (kind) {
        case CadSketchSupportKind::WorldPlane: return "WorldPlane";
        case CadSketchSupportKind::BodyFace: return "BodyFace";
        case CadSketchSupportKind::FeatureFace: return "FeatureFace";
    }
    return "unknown";
}

const char* cadSketchRowStateName(CadSketchRowState state) {
    switch (state) {
        case CadSketchRowState::Ok: return "Ok";
        case CadSketchRowState::Unused: return "Unused";
        case CadSketchRowState::Failed: return "Failed";
        case CadSketchRowState::NotRegenerated: return "NotRegenerated";
        case CadSketchRowState::SupportLost: return "SupportLost";
    }
    return "unknown";
}

namespace {

// Every feature that extrudes `sketchId`, in chain order (ids ascend along it).
std::vector<uint32_t> consumersOf(const CadBodyState& state, CadSketchId sketchId) {
    std::vector<uint32_t> out;
    CadFeatureView view;
    for (uint32_t i = 0; cadFeatureAt(state, i, &view); ++i) {
        if (view.sketchId == sketchId) {
            out.push_back(view.featureId);
        }
    }
    return out;
}

// Whether a finished sketch would offer at least one area: the very decision
// Finish takes (`decideSketchSelectionMode`), so reuse is offered for exactly
// the sketches a Finish would accept.
CadStatus sketchClosesAnArea(const CadSketch& sketch) {
    const CadStatus valid = validateCadSketch(sketch);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    const SketchRegionExtraction regions = extractSketchRegions(sketch);
    const SketchArrangement arrangement = deriveSketchArrangement(sketch);
    const SketchSelectionModeDecision mode = decideSketchSelectionMode(arrangement, regions);
    if (mode.status != CadStatus::Ok) {
        return mode.status;
    }
    if (mode.kind == CadSelectionKind::PlanarFaces) {
        return arrangement.faces.empty() ? CadStatus::NoClosedProfile : CadStatus::Ok;
    }
    if (regions.loops.profiles.empty()) {
        return regions.loops.rejections.empty() ? CadStatus::NoClosedProfile
                                                : regions.loops.rejections.front().why;
    }
    return CadStatus::Ok;
}

}  // namespace

CadStatus cadSketchReuseRefusal(const CadBodyState& state, CadSketchId sketchId) {
    const CadSketchRecord* record = findCadSketchRecord(state, sketchId);
    if (record == nullptr) {
        return CadStatus::SketchNotFound;
    }
    if (state.baseKind == CadFeatureKind::Revolve) {
        return CadStatus::RevolveLaterFeatureUnsupported;
    }
    if (cadFeatureCount(state) >= kMaxCadFeatures) {
        return CadStatus::TooManyFeatures;
    }
    CadFrame64 placement;
    const CadStatus placed = cadSketchPlacement(state, sketchId, &placement);
    if (placed != CadStatus::Ok) {
        return placed;
    }
    return sketchClosesAnArea(record->sketch);
}

CadSketchInventory buildCadSketchInventory(const CadBodyState& state,
                                           const CadRegenerationReport& report) {
    CadSketchInventory inventory;
    inventory.status = report.status;
    inventory.failedFeatureId = report.failedFeatureId;
    inventory.nextSketchId = state.nextSketchId;

    // Construction order: first consumption in chain order, then the unused
    // sketches ascending by id -- the timeline's own order.
    std::vector<CadSketchId> order;
    CadFeatureView view;
    for (uint32_t i = 0; cadFeatureAt(state, i, &view); ++i) {
        if (std::find(order.begin(), order.end(), view.sketchId) == order.end()) {
            order.push_back(view.sketchId);
        }
    }
    for (const CadSketchRecord& record : state.sketches) {
        if (std::find(order.begin(), order.end(), record.sketchId) == order.end()) {
            order.push_back(record.sketchId);
        }
    }

    uint32_t ordinal = 0;
    for (const CadSketchId id : order) {
        const CadSketchRecord* record = findCadSketchRecord(state, id);
        if (record == nullptr) {
            continue;  // a feature naming a missing sketch: the timeline names it
        }
        CadSketchInventoryRow row;
        row.sketchId = id;
        row.ordinal = ++ordinal;
        row.root = id == state.baseSketchId;
        if (record->hasFeatureSupport) {
            row.support = CadSketchSupportKind::FeatureFace;
            row.supportFeatureId = record->featureSupport.featureId;
            row.supportFace = record->featureSupport.face;
            row.plane = Workplane::XY;
        } else {
            row.support = record->sketch.hasFaceSupport ? CadSketchSupportKind::BodyFace
                                                        : CadSketchSupportKind::WorldPlane;
            row.plane = record->sketch.plane;
        }
        row.entityCount = static_cast<uint32_t>(record->sketch.entities.size());
        row.dimensionCount = static_cast<uint32_t>(record->sketch.dimensions.size());
        row.consumers = consumersOf(state, id);

        if (!row.consumers.empty()) {
            row.state = CadSketchRowState::Ok;
            if (report.status != CadStatus::Ok && report.failedFeatureId != 0) {
                bool failed = false;
                bool allAfter = true;
                for (const uint32_t consumer : row.consumers) {
                    failed = failed || consumer == report.failedFeatureId;
                    allAfter = allAfter && consumer > report.failedFeatureId;
                }
                if (failed) {
                    row.state = CadSketchRowState::Failed;
                    row.status = report.status;
                } else if (allAfter) {
                    row.state = CadSketchRowState::NotRegenerated;
                }
            }
        } else {
            CadFrame64 placement;
            const CadStatus placed = cadSketchPlacement(state, id, &placement);
            row.state = placed == CadStatus::Ok ? CadSketchRowState::Unused
                                                : CadSketchRowState::SupportLost;
            row.status = placed;
        }

        // Acts. Edit: any sketch a staged session can open -- a consumed one
        // through its first consumer, an unused one sketch-only (which needs
        // its placement to resolve).
        if (row.state != CadSketchRowState::SupportLost) {
            row.acts |= kCadSketchActEdit;
        }
        if (row.root) {
            row.deleteRefusal = CadStatus::SketchIsRoot;
        } else if (!row.consumers.empty()) {
            row.deleteRefusal = CadStatus::SketchInUse;
        } else {
            row.acts |= kCadSketchActDelete;
        }
        row.reuseRefusal = report.status != CadStatus::Ok ? report.status
                                                          : cadSketchReuseRefusal(state, id);
        if (row.reuseRefusal == CadStatus::Ok) {
            row.acts |= kCadSketchActReuseAdd | kCadSketchActReuseCut;
        }
        inventory.rows.push_back(std::move(row));
    }
    return inventory;
}

CadSketchInventory buildCadSketchInventoryFor(const CadBodyState& state) {
    CadBodyMesh mesh;
    CadRegenerationReport report;
    regenerateCadBody(state, &mesh, &report);
    return buildCadSketchInventory(state, report);
}

CadStatus removeUnusedCadSketch(const CadBodyState& state, CadSketchId sketchId,
                                CadBodyState* out) {
    if (out == nullptr) {
        return CadStatus::RegenerationFailed;
    }
    if (findCadSketchRecord(state, sketchId) == nullptr) {
        return CadStatus::SketchNotFound;
    }
    if (sketchId == state.baseSketchId) {
        return CadStatus::SketchIsRoot;
    }
    if (!consumersOf(state, sketchId).empty()) {
        return CadStatus::SketchInUse;
    }
    CadBodyState next = state;
    next.sketches.erase(std::remove_if(next.sketches.begin(), next.sketches.end(),
                                       [sketchId](const CadSketchRecord& record) {
                                           return record.sketchId == sketchId;
                                       }),
                        next.sketches.end());
    // `nextSketchId` stays where it is: the freed id is never minted again on
    // this forward branch.
    *out = std::move(next);
    return CadStatus::Ok;
}

CadStatus deleteCadSketch(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                          CadSketchId sketchId) {
    if (history.editInProgress()) {
        return CadStatus::RefusedEditInProgress;
    }
    SceneObject* object = scene.findBody(bodyId);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        return CadStatus::NotCadBody;
    }
    CadBodyState next;
    const CadStatus removed = removeUnusedCadSketch(body->state(), sketchId, &next);
    if (removed != CadStatus::Ok) {
        return removed;
    }
    CadStatus why = CadStatus::Ok;
    {
        // ONE transaction: one Delete is one Undo, and Undo restores the whole
        // snapshot -- the record, its id and the marks -- exactly.
        ScopedConstructionEdit edit(history);
        why = body->applyState(next);
        if (why == CadStatus::Ok) {
            publishSceneObject(*object);
        }
    }
    return why;
}

}  // namespace forgeshape
