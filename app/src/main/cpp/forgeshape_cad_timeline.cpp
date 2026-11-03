#include "forgeshape_cad_timeline.h"

#include <algorithm>
#include <cstdio>

namespace forgeshape {

const char* cadTimelineRowKindName(CadTimelineRowKind kind) {
    switch (kind) {
        case CadTimelineRowKind::Sketch: return "Sketch";
        case CadTimelineRowKind::Feature: return "Feature";
    }
    return "unknown";
}

const char* cadTimelineRowStateName(CadTimelineRowState state) {
    switch (state) {
        case CadTimelineRowState::Ok: return "Ok";
        case CadTimelineRowState::Failed: return "Failed";
        case CadTimelineRowState::NotRegenerated: return "NotRegenerated";
        case CadTimelineRowState::Pending: return "Pending";
        case CadTimelineRowState::Unused: return "Unused";
    }
    return "unknown";
}

namespace {

// The selection a feature row summarizes, read through the one carrier a
// Revolve already lends its selection to, so a Revolve's regions are counted by
// the rule an Extrude's are.
void fillSelection(const ExtrudeFeature& selection, CadTimelineRow* row) {
    row->selection = selection.selection;
    if (selection.selection == CadSelectionKind::PlanarFaces) {
        row->regionCount = static_cast<uint32_t>(selection.planarFaces.size());
        uint32_t holes = 0;
        for (const PlanarFaceRef& face : selection.planarFaces) {
            holes += static_cast<uint32_t>(face.holes.size());
        }
        row->holeCount = holes;
        return;
    }
    const std::vector<ProfileRegionRef> regions = extrudeRegions(selection);
    row->regionCount = static_cast<uint32_t>(regions.size());
    uint32_t holes = 0;
    for (const ProfileRegionRef& region : regions) {
        holes += static_cast<uint32_t>(region.holeAnchorIds.size());
    }
    row->holeCount = holes;
}

void fillSketch(const CadSketchRecord& record, CadTimelineRow* row) {
    row->entityCount = static_cast<uint32_t>(record.sketch.entities.size());
    row->dimensionCount = static_cast<uint32_t>(record.sketch.dimensions.size());
    row->plane = record.sketch.plane;
    row->onBodyFace = !record.hasFeatureSupport && record.sketch.hasFaceSupport;
    row->onFeatureFace = record.hasFeatureSupport;
    row->supportFeatureId = record.hasFeatureSupport ? record.featureSupport.featureId : 0u;
}

}  // namespace

CadTimeline buildCadTimeline(const CadBodyState& state, const CadRegenerationReport& report,
                             bool evaluated, uint32_t editingFeatureId) {
    CadTimeline timeline;
    timeline.status = evaluated ? report.status : CadStatus::NotSketching;
    timeline.failedFeatureId = evaluated ? report.failedFeatureId : 0u;
    timeline.editingFeatureId = editingFeatureId;
    timeline.evaluated = evaluated;

    // The sketch the staged edit changes: the edited feature's own.
    CadSketchId editedSketch = kNoCadSketch;
    if (editingFeatureId != 0) {
        CadFeatureView edited;
        if (findCadFeature(state, editingFeatureId, &edited)) {
            editedSketch = edited.sketchId;
        }
    }

    std::vector<CadSketchId> listed;
    uint32_t sketchOrdinal = 0;
    uint32_t featureOrdinal = 0;
    // Once the failing feature (or, unevaluated, the edited one) is passed,
    // every later feature is not known to regenerate.
    bool pastFailure = false;
    bool pastEdit = false;
    const uint32_t count = cadFeatureCount(state);
    for (uint32_t i = 0; i < count && timeline.rows.size() < kMaxCadTimelineRows; ++i) {
        CadFeatureView view;
        if (!cadFeatureAt(state, i, &view)) {
            break;
        }
        const bool sketchListed =
                std::find(listed.begin(), listed.end(), view.sketchId) != listed.end();
        if (!sketchListed && view.sketchId != kNoCadSketch) {
            listed.push_back(view.sketchId);
            CadTimelineRow row;
            row.kind = CadTimelineRowKind::Sketch;
            row.id = view.sketchId;
            row.sketchId = view.sketchId;
            row.editFeatureId = view.featureId;
            row.ordinal = ++sketchOrdinal;
            row.editing = editedSketch != kNoCadSketch && view.sketchId == editedSketch;
            if (const CadSketchRecord* record = findCadSketchRecord(state, view.sketchId)) {
                fillSketch(*record, &row);
            }
            timeline.rows.push_back(row);
        }

        CadTimelineRow row;
        row.kind = CadTimelineRowKind::Feature;
        row.id = view.featureId;
        row.sketchId = view.sketchId;
        row.editFeatureId = view.featureId;
        row.ordinal = ++featureOrdinal;
        row.editing = editingFeatureId != 0 && view.featureId == editingFeatureId;
        row.featureKind = view.kind;
        row.operation = view.operation;
        if (view.kind == CadFeatureKind::Revolve && view.revolve != nullptr) {
            fillSelection(revolveSelectionCarrier(*view.revolve), &row);
            row.angleDegrees = view.revolve->angleDegrees;
            row.revolveDirection = view.revolve->direction;
            row.axis = view.revolve->axis;
        } else if (view.extrude != nullptr) {
            fillSelection(*view.extrude, &row);
            row.extent = view.extrude->extent;
            row.direction = view.extrude->direction;
            row.positiveDistance = extrudePositiveDistance(*view.extrude);
            row.negativeDistance = extrudeNegativeDistance(*view.extrude);
        }
        if (!evaluated) {
            if (row.editing) {
                pastEdit = true;
            }
            row.state = pastEdit ? CadTimelineRowState::Pending : CadTimelineRowState::Ok;
        } else if (pastFailure) {
            row.state = CadTimelineRowState::NotRegenerated;
        } else if (report.status != CadStatus::Ok && report.failedFeatureId == view.featureId) {
            row.state = CadTimelineRowState::Failed;
            row.status = report.status;
            pastFailure = true;
        }
        timeline.rows.push_back(row);
    }

    // Retained sketches no feature consumes, ascending by id (the table's own
    // order), after everything that builds the body.
    for (const CadSketchRecord& record : state.sketches) {
        if (timeline.rows.size() >= kMaxCadTimelineRows) {
            break;
        }
        if (std::find(listed.begin(), listed.end(), record.sketchId) != listed.end()) {
            continue;
        }
        CadTimelineRow row;
        row.kind = CadTimelineRowKind::Sketch;
        row.id = record.sketchId;
        row.sketchId = record.sketchId;
        row.editFeatureId = 0;
        row.ordinal = ++sketchOrdinal;
        row.state = CadTimelineRowState::Unused;
        fillSketch(record, &row);
        timeline.rows.push_back(row);
    }
    return timeline;
}

CadTimeline buildCadTimelineFor(const CadBodyState& state) {
    CadBodyMesh mesh;
    CadRegenerationReport report;
    regenerateCadBody(state, &mesh, &report);
    return buildCadTimeline(state, report, /*evaluated=*/true, /*editingFeatureId=*/0);
}

std::string cadTimelineRowSummary(const CadTimelineRow& row) {
    char buffer[192];
    if (row.kind == CadTimelineRowKind::Sketch) {
        const char* where = row.onFeatureFace ? "face" : (row.onBodyFace ? "body face"
                                                                          : workplaneName(row.plane));
        std::snprintf(buffer, sizeof(buffer), "Sketch %u #%u %s %u entities %u dimensions %s",
                      row.ordinal, row.id, where, row.entityCount, row.dimensionCount,
                      cadTimelineRowStateName(row.state));
        return buffer;
    }
    if (row.featureKind == CadFeatureKind::Revolve) {
        std::snprintf(buffer, sizeof(buffer), "Revolve %s #%u sketch %u angle %.6g %s axis %u:%u %s",
                      cadFeatureOperationName(row.operation), row.id, row.sketchId,
                      row.angleDegrees, revolveDirectionName(row.revolveDirection),
                      row.axis.entityId, row.axis.edgeLocalIndex,
                      cadTimelineRowStateName(row.state));
    } else {
        std::snprintf(buffer, sizeof(buffer), "Extrude %s #%u sketch %u %s +%.6g -%.6g %s",
                      cadFeatureOperationName(row.operation), row.id, row.sketchId,
                      extrudeExtentModeName(row.extent), row.positiveDistance,
                      row.negativeDistance, cadTimelineRowStateName(row.state));
    }
    std::string text = buffer;
    if (row.state == CadTimelineRowState::Failed) {
        text += " ";
        text += cadStatusName(row.status);
    }
    return text;
}

}  // namespace forgeshape
