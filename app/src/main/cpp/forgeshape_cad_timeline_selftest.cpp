#include "forgeshape_cad_timeline_selftest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_cad_timeline.h"
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {
namespace {

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

SketchRectangle rectangle(double cu, double cv, double w, double h) {
    SketchRectangle r;
    r.center = SketchPoint{cu, cv};
    r.width = w;
    r.height = h;
    return r;
}

SketchCircle circle(double cu, double cv, double radius) {
    SketchCircle c;
    c.center = SketchPoint{cu, cv};
    c.radius = radius;
    return c;
}

ExtrudeFeature extrudeOf(SketchEntityId profile, double depth,
                         ExtrudeDirection direction = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature extrude;
    extrude.profileEntityId = profile;
    extrude.depth = depth;
    extrude.direction = direction;
    return extrude;
}

// The 32-gon a circle is extruded as, measured -- never pi r^2.
double circleArea(double radius) {
    return 0.5 * polygonSignedAreaTwice(circleProfilePolygon(circle(0.0, 0.0, radius)));
}

const SketchDimensionId kWidthDimension = 1;

// The base: a 2 x 2 rectangle carrying a DRIVING width dimension, extruded 1 m
// on XY. What PAR-03 types a new width into.
CadBodyState baseState() {
    CadSketch base;
    const SketchEntityId rect = add(&base, rectangle(0.0, 0.0, 2.0, 2.0));
    SketchDimension width;
    width.id = kWidthDimension;
    width.kind = SketchDimensionKind::RectangleWidth;
    width.mode = SketchDimensionMode::Driving;
    width.first = CadSketchEdgeRef{rect, 0};
    base.dimensions.push_back(width);
    base.nextDimensionId = kWidthDimension + 1u;
    return makeCadBodyState(base, extrudeOf(rect, 1.0));
}

CadFeatureSupport farCap(const CadBodyState& state, uint32_t featureId) {
    CadFeatureSupport support;
    support.featureId = featureId;
    support.face.kind = CadFaceKind::CapFar;
    support.lineageToken = cadFeatureTopologySignature(state, featureId);
    return support;
}

// The chain every PAR case edits: the base, an Add boss in the middle of its far
// cap (feature 2), a Cut pocket near the (+u, +v) corner (feature 3) and -- with
// `secondCut` -- another pocket near the opposite corner (feature 4).
CadBodyState chainState(bool secondCut) {
    CadBodyState state = baseState();
    CadSketch boss;
    const SketchEntityId bossRect = add(&boss, rectangle(0.0, 0.0, 0.6, 0.6));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add, farCap(state, kCadFeatureId),
                                    boss, extrudeOf(bossRect, 0.5));
    CadSketch pocket;
    const SketchEntityId pocketCircle = add(&pocket, circle(0.7, 0.7, 0.15));
    appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut, farCap(state, kCadFeatureId),
                                    pocket,
                                    extrudeOf(pocketCircle, 0.5, ExtrudeDirection::AgainstNormal));
    if (secondCut) {
        CadSketch second;
        const SketchEntityId secondCircle = add(&second, circle(-0.7, -0.7, 0.15));
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Cut,
                                        farCap(state, kCadFeatureId), second,
                                        extrudeOf(secondCircle, 0.5,
                                                  ExtrudeDirection::AgainstNormal));
    }
    return state;
}

// A revolve: a 1 x 1 square beside a straight line, swept about the line.
CadBodyState revolveState() {
    CadSketch sketch;
    const SketchEntityId square = add(&sketch, rectangle(1.5, 0.0, 1.0, 1.0));
    SketchLine axis;
    axis.start = SketchPoint{0.0, -1.0};
    axis.end = SketchPoint{0.0, 1.0};
    const SketchEntityId line = add(&sketch, axis);
    RevolveFeature revolve;
    revolve.profileEntityId = square;
    revolve.axis = CadSketchEdgeRef{line, 0};
    revolve.angleDegrees = 360.0;
    return makeCadRevolveBodyState(sketch, revolve);
}

bool regenerate(const CadBodyState& state, CadBodyMesh* out, CadRegenerationReport* report = nullptr) {
    return regenerateCadBody(state, out, report) == CadStatus::Ok;
}

bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }

void meshBounds(const ConstructionMesh& mesh, float lo[3], float hi[3]) {
    for (int c = 0; c < 3; ++c) {
        lo[c] = 1e30f;
        hi[c] = -1e30f;
    }
    for (const MeshVertex& v : mesh.vertices) {
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], v.position[c]);
            hi[c] = std::max(hi[c], v.position[c]);
        }
    }
}

bool meshHasFeature(const CadBodyMesh& mesh, uint32_t featureId) {
    for (uint32_t tag : mesh.triangleFace) {
        if (tag < mesh.faces.size() && mesh.faces[tag].featureId == featureId) {
            return true;
        }
    }
    return false;
}

const SketchFrame kXyFrame{Vec3{0, 0, 0}, Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}};

// A live one-body project holding `state`, with its own history.
struct Rig {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history{scene};
    ObjectId id = kNoObject;

    explicit Rig(const CadBodyState& state) {
        history.beginSessionInitialization();
        SceneObject* body = scene.addCadBody(state);
        if (body != nullptr) {
            id = body->objectId();
            publishSceneObject(*body);
        }
        history.endSessionInitialization();
    }
    const CadBodyState& state() const { return scene.findBody(id)->cadOrNull()->state(); }
    uint64_t fingerprint() const { return projectSemanticFingerprint(scene, ProjectKind::Construction); }
    std::vector<uint8_t> bytes() const {
        return encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
    }
};

bool sameRows(const CadTimeline& a, const CadTimeline& b) {
    if (a.rows.size() != b.rows.size()) return false;
    for (size_t i = 0; i < a.rows.size(); ++i) {
        const CadTimelineRow& x = a.rows[i];
        const CadTimelineRow& y = b.rows[i];
        if (x.kind != y.kind || x.id != y.id || x.sketchId != y.sketchId || x.state != y.state
            || x.featureKind != y.featureKind || x.operation != y.operation
            || x.positiveDistance != y.positiveDistance || x.negativeDistance != y.negativeDistance
            || x.angleDegrees != y.angleDegrees || x.entityCount != y.entityCount) {
            return false;
        }
    }
    return true;
}

// The rows' (kind, id) sequence as a compact string, for exact-order checks.
std::string rowKey(const CadTimeline& timeline) {
    std::string key;
    for (const CadTimelineRow& row : timeline.rows) {
        key += row.kind == CadTimelineRowKind::Sketch ? 'S' : 'F';
        key += std::to_string(row.id);
        key += ' ';
    }
    return key;
}

// ---------------------------------------------------------------------------
// PAR-01, PAR-02: order and identity
// ---------------------------------------------------------------------------

void testTimelineShape(Checks& r) {
    const CadBodyState chain = chainState(true);
    const CadTimeline timeline = buildCadTimelineFor(chain);
    // Construction order: each feature's sketch, then the feature.
    bool orderMatchesChain = rowKey(timeline) == "S1 F1 S2 F2 S3 F3 S4 F4 "
                             && timeline.status == CadStatus::Ok;
    for (uint32_t i = 0, row = 1; i < cadFeatureCount(chain) && orderMatchesChain; ++i, row += 2) {
        CadFeatureView view;
        orderMatchesChain = cadFeatureAt(chain, i, &view)
                            && timeline.rows[row].kind == CadTimelineRowKind::Feature
                            && timeline.rows[row].id == view.featureId
                            && timeline.rows[row].sketchId == view.sketchId
                            && timeline.rows[row].operation == view.operation
                            && timeline.rows[row].state == CadTimelineRowState::Ok;
    }
    r.check("PAR_01_the_timeline_lists_each_sketch_then_its_feature_in_chain_order",
            orderMatchesChain && timeline.rows.size() == 8u && timeline.rows[3].ordinal == 2u
                    && timeline.rows[3].operation == CadFeatureOperation::Add
                    && timeline.rows[5].operation == CadFeatureOperation::Cut);

    // PAR-02: identities are the durable ids. A feature that extrudes the ROOT
    // sketch lists it once, a sketch nobody consumes comes last, and the ids
    // follow the state through a structural edit rather than the row index.
    CadBodyState shared = chainState(false);
    ExtrudeFeature again = extrudeOf(1, 0.25);
    again.extent = ExtrudeExtentMode::Symmetric;
    const uint32_t sharedFeature =
            appendCadLaterFeature(&shared, CadFeatureOperation::Add, kBaseCadSketchId, again);
    CadSketch spare;
    add(&spare, rectangle(0.0, 0.0, 0.2, 0.2));
    const CadSketchId spareId = addCadSketchRecord(&shared, spare, nullptr);
    CadRegenerationReport okReport;
    const CadTimeline sharedTimeline = buildCadTimeline(shared, okReport, true, 0);
    const std::string expected = "S1 F1 S2 F2 S3 F3 F" + std::to_string(sharedFeature) + " S"
                                 + std::to_string(spareId) + " ";
    bool idsDurable = rowKey(sharedTimeline) == expected
                      && sharedTimeline.rows.back().state == CadTimelineRowState::Unused
                      && sharedTimeline.rows.back().editFeatureId == 0u
                      && sharedTimeline.rows[6].sketchId == kBaseCadSketchId
                      && sharedTimeline.rows[0].editFeatureId == kCadFeatureId;
    for (const CadTimelineRow& row : sharedTimeline.rows) {
        if (row.kind == CadTimelineRowKind::Sketch) {
            idsDurable = idsDurable && findCadSketchRecord(shared, row.id) != nullptr;
        } else {
            CadFeatureView view;
            idsDurable = idsDurable && findCadFeature(shared, row.id, &view);
        }
    }
    // Remove feature 2 (and its sketch): feature 3 keeps id 3 at a new row.
    CadBodyState removed = chainState(true);
    const CadSketchId removedSketch = removed.laterFeatures.front().sketchId;
    removed.laterFeatures.erase(removed.laterFeatures.begin());
    removed.sketches.erase(std::remove_if(removed.sketches.begin(), removed.sketches.end(),
                                          [removedSketch](const CadSketchRecord& record) {
                                              return record.sketchId == removedSketch;
                                          }),
                           removed.sketches.end());
    const CadTimeline afterRemoval = buildCadTimelineFor(removed);
    r.check("PAR_02_rows_carry_the_durable_sketch_and_feature_ids_never_a_position",
            idsDurable && rowKey(afterRemoval) == "S1 F1 S3 F3 S4 F4 "
                    && afterRemoval.rows[3].ordinal == 2u && afterRemoval.rows[3].id == 3u);
}

// ---------------------------------------------------------------------------
// PAR-03..06, PAR-12: upstream edits through the real session
// ---------------------------------------------------------------------------

void testUpstreamEdits(Checks& r) {
    Rig rig(chainState(false));
    const CadBodyState before = rig.state();
    const size_t depthBefore = rig.history.undoDepth();

    // PAR-03: type a new width into the base sketch's Driving dimension.
    SketchSession edit;
    bool committed = edit.beginEditFeature(rig.id, before, kCadFeatureId, kXyFrame, false)
                     == CadStatus::Ok;
    committed = committed && edit.applyDimensionValue(kWidthDimension, 3.0) == CadStatus::Ok
                && edit.finish() == CadStatus::Ok && edit.evaluateCandidate().status == CadStatus::Ok;
    committed = committed && edit.commitEdit(rig.scene, rig.history) == CadStatus::Ok;
    const CadBodyState after = rig.state();
    CadBodyMesh mesh;
    const bool regenerated = regenerate(after, &mesh);
    float lo[3];
    float hi[3];
    meshBounds(mesh.mesh, lo, hi);
    const SketchEntity* rect = findSketchEntity(cadBaseSketch(after), 1);
    r.check("PAR_03_a_base_sketch_dimension_edit_regenerates_the_base_extrusion",
            committed && regenerated && rect != nullptr && rect->rectangle() != nullptr
                    && rect->rectangle()->width == 3.0 && near(lo[0], -1.5, 1e-6)
                    && near(hi[0], 1.5, 1e-6) && near(lo[1], -1.0, 1e-6)
                    && near(hi[1], 1.0, 1e-6) && !edit.active());

    // PAR-04: the later Add regenerated on the widened base -- its boss is in
    // the solid (its tagged faces, the top at 1.5) and its volume is counted.
    const double expected = 3.0 * 2.0 * 1.0 + 0.6 * 0.6 * 0.5 - circleArea(0.15) * 0.5;
    r.check("PAR_04_the_later_add_regenerates_after_the_base_sketch_edit",
            regenerated && meshHasFeature(mesh, 2u) && near(hi[2], 1.5, 1e-6)
                    && near(mesh.volume, expected, 1e-7));

    // PAR-05: the later Cut regenerated too: its pocket's faces are there and
    // the volume above already subtracts it.
    CadRegenerationReport report;
    CadBodyMesh again;
    regenerate(after, &again, &report);
    r.check("PAR_05_the_later_cut_regenerates_after_the_base_sketch_edit",
            meshHasFeature(mesh, 3u) && report.status == CadStatus::Ok
                    && report.failedFeatureId == 0u && mesh.components == 1u);

    // PAR-12: that whole edit was ONE step; Undo and Redo are exact.
    const bool oneStep = rig.history.undoDepth() == depthBefore + 1u;
    const bool undone = rig.history.undo() && sameCadBodyState(rig.state(), before);
    const bool redone = rig.history.redo() && sameCadBodyState(rig.state(), after);
    r.check("PAR_12_a_valid_upstream_edit_is_one_step_and_undo_redo_restore_it_exactly",
            oneStep && undone && redone);

    // PAR-06: change the base EXTRUDE depth (opened straight on the extrusion):
    // both later features stand on its far cap and move with it.
    SketchSession depthEdit;
    bool deepened = depthEdit.beginEditFeature(rig.id, rig.state(), kCadFeatureId, kXyFrame, true)
                    == CadStatus::Ok;
    deepened = deepened && depthEdit.setExtrude(2.0, ExtrudeDirection::AlongNormal) == CadStatus::Ok
               && depthEdit.commitEdit(rig.scene, rig.history) == CadStatus::Ok;
    CadBodyMesh deep;
    const bool deepRegenerated = regenerate(rig.state(), &deep);
    meshBounds(deep.mesh, lo, hi);
    const double deepExpected = 3.0 * 2.0 * 2.0 + 0.6 * 0.6 * 0.5 - circleArea(0.15) * 0.5;
    r.check("PAR_06_an_extrude_depth_edit_regenerates_every_later_feature_on_its_moved_cap",
            deepened && deepRegenerated && rig.state().extrude.depth == 2.0
                    && near(hi[2], 2.5, 1e-6) && near(deep.volume, deepExpected, 1e-7)
                    && meshHasFeature(deep, 2u) && meshHasFeature(deep, 3u));
}

// ---------------------------------------------------------------------------
// PAR-07, PAR-08: Revolve angle and axis from the timeline
// ---------------------------------------------------------------------------

void testRevolveEdits(Checks& r) {
    Rig rig(revolveState());
    const CadTimeline before = buildCadTimelineFor(rig.state());
    const uint32_t revolveRow = 1;
    const bool listed = before.rows.size() == 2u
                        && before.rows[revolveRow].featureKind == CadFeatureKind::Revolve
                        && before.rows[revolveRow].angleDegrees == 360.0;

    SketchSession angle;
    const bool angled =
            angle.beginEditFeature(rig.id, rig.state(), before.rows[revolveRow].editFeatureId,
                                   kXyFrame, true)
                    == CadStatus::Ok
            && angle.setRevolveAngle(137.25) == CadStatus::Ok
            && angle.commitEdit(rig.scene, rig.history) == CadStatus::Ok;
    const CadTimeline afterAngle = buildCadTimelineFor(rig.state());
    r.check("PAR_07_a_revolve_angle_edit_from_the_timeline_is_exact_to_the_bit",
            listed && angled && rig.state().revolve.angleDegrees == 137.25
                    && afterAngle.rows[revolveRow].angleDegrees == 137.25
                    && afterAngle.status == CadStatus::Ok);

    SketchSession axis;
    const bool axisChanged =
            axis.beginEditFeature(rig.id, rig.state(), kCadFeatureId, kXyFrame, true) == CadStatus::Ok
            && axis.beginRevolveAxisPick() == CadStatus::Ok
            && axis.setRevolveAxis(CadSketchEdgeRef{1, 3}) == CadStatus::Ok
            && axis.commitEdit(rig.scene, rig.history) == CadStatus::Ok;
    const CadTimeline afterAxis = buildCadTimelineFor(rig.state());
    // A nonexistent edge is refused by name and nothing moves.
    SketchSession wrong;
    const bool refused =
            wrong.beginEditFeature(rig.id, rig.state(), kCadFeatureId, kXyFrame, true) == CadStatus::Ok
            && wrong.setRevolveAxis(CadSketchEdgeRef{9, 0}) == CadStatus::RevolveAxisUnresolved;
    wrong.cancel();
    r.check("PAR_08_a_revolve_axis_edit_from_the_timeline_names_the_exact_edge",
            axisChanged && sameCadSketchEdgeRef(rig.state().revolve.axis, CadSketchEdgeRef{1, 3})
                    && sameCadSketchEdgeRef(afterAxis.rows[revolveRow].axis, CadSketchEdgeRef{1, 3})
                    && rig.state().revolve.angleDegrees == 137.25 && refused);
}

// ---------------------------------------------------------------------------
// PAR-09..11: an upstream edit that breaks a later feature
// ---------------------------------------------------------------------------

void testFailingEdits(Checks& r) {
    Rig rig(chainState(true));
    const CadBodyState before = rig.state();
    const uint64_t fingerprintBefore = rig.fingerprint();
    const std::vector<uint8_t> bytesBefore = rig.bytes();
    const size_t depthBefore = rig.history.undoDepth();

    // Narrow the base to 1 m: both pockets now lie outside it. Regeneration
    // stops at the FIRST, feature 3, and never reaches feature 4.
    SketchSession edit;
    bool staged = edit.beginEditFeature(rig.id, before, kCadFeatureId, kXyFrame, false)
                  == CadStatus::Ok;
    // Before Finish the candidate has no verdict: the edited feature and
    // everything after it are Pending, never Ok.
    const CadTimeline drawing =
            buildCadTimeline(edit.candidateState(), CadRegenerationReport{}, false,
                             edit.editingFeatureId());
    staged = staged && edit.applyDimensionValue(kWidthDimension, 1.0) == CadStatus::Ok
             && edit.finish() == CadStatus::Ok;
    const CadCandidateEvaluation& evaluation = edit.evaluateCandidate();
    CadRegenerationReport report;
    report.status = evaluation.status;
    report.failedFeatureId = evaluation.failedFeatureId;
    const CadTimeline stagedTimeline =
            buildCadTimeline(edit.candidateState(), report, true, edit.editingFeatureId());
    r.check("PAR_09_an_invalid_upstream_edit_names_the_first_failing_downstream_feature",
            staged && evaluation.status == CadStatus::CutNoIntersection
                    && evaluation.failedFeatureId == 3u
                    && rowKey(stagedTimeline) == "S1 F1 S2 F2 S3 F3 S4 F4 "
                    && stagedTimeline.rows[0].editing && stagedTimeline.rows[1].editing
                    && stagedTimeline.rows[1].state == CadTimelineRowState::Ok
                    && stagedTimeline.rows[3].state == CadTimelineRowState::Ok
                    && stagedTimeline.rows[5].state == CadTimelineRowState::Failed
                    && stagedTimeline.rows[5].status == CadStatus::CutNoIntersection
                    && stagedTimeline.rows[7].state == CadTimelineRowState::NotRegenerated
                    && stagedTimeline.failedFeatureId == 3u
                    && drawing.rows[1].state == CadTimelineRowState::Pending
                    && drawing.rows[7].state == CadTimelineRowState::Pending
                    && !drawing.evaluated);

    // PAR-10: the commit is refused by that feature's reason and the project
    // is untouched; the edit stays open so the value can be fixed.
    const CadStatus refused = edit.commitEdit(rig.scene, rig.history);
    r.check("PAR_10_an_invalid_staged_edit_mutates_no_project_truth_and_stays_open",
            refused == CadStatus::CutNoIntersection && sameCadBodyState(rig.state(), before)
                    && rig.history.undoDepth() == depthBefore && edit.active()
                    && edit.state() == SketchSessionState::Ready
                    && rig.fingerprint() == fingerprintBefore);

    // Fix: back to the sketch, a width that keeps both pockets, and the same
    // session's candidate regenerates clean.
    edit.backToEditing();
    const bool fixed = edit.state() == SketchSessionState::Editing
                       && edit.applyDimensionValue(kWidthDimension, 2.5) == CadStatus::Ok
                       && edit.finish() == CadStatus::Ok
                       && edit.evaluateCandidate().status == CadStatus::Ok;

    // PAR-11: Cancel instead -- the body, its fingerprint and its bytes are
    // exactly what they were.
    edit.cancel();
    r.check("PAR_11_cancel_restores_the_exact_prior_fingerprint_and_bytes",
            fixed && !edit.active() && sameCadBodyState(rig.state(), before)
                    && rig.fingerprint() == fingerprintBefore && rig.bytes() == bytesBefore
                    && rig.history.undoDepth() == depthBefore);
}

// ---------------------------------------------------------------------------
// PAR-13, PAR-14: persistence
// ---------------------------------------------------------------------------

void testPersistence(Checks& r) {
    Rig rig(chainState(true));
    const CadTimeline before = buildCadTimelineFor(rig.state());
    const std::vector<uint8_t> bytes = rig.bytes();
    ProjectDocument decoded;
    const bool decodedOk =
            decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok;
    ConstructionScene reopened{NoProjectTag{}};
    ConstructionHistory history(reopened);
    SculptSession sculpt;
    const bool loaded =
            decodedOk && loadProjectDocument(decoded, reopened, sculpt, history) == ProjectCodecStatus::Ok;
    const SceneObject* body = loaded ? reopened.findBody(rig.id) : nullptr;
    const bool same = body != nullptr && body->cadOrNull() != nullptr
                      && sameCadBodyState(body->cadOrNull()->state(), rig.state());
    r.check("PAR_13_save_and_reopen_preserves_the_feature_chain_and_its_timeline",
            same && sameRows(buildCadTimelineFor(body->cadOrNull()->state()), before)
                    && history.undoDepth() == 0u);

    // PAR-14: building (and reading) the timeline writes nothing: the bytes and
    // the fingerprint are what they were, and the file carries only the
    // sections it always had.
    const uint64_t fingerprint = rig.fingerprint();
    for (int i = 0; i < 3; ++i) {
        (void)buildCadTimelineFor(rig.state());
    }
    bool onlyKnownSections = bytes.size() > kForgeHeaderBytes;
    size_t offset = kForgeHeaderBytes;
    while (onlyKnownSections && offset + kForgeSectionHeaderBytes <= bytes.size()) {
        const bool known = std::memcmp(&bytes[offset], kSectionTagScene, 4) == 0
                           || std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0;
        uint64_t payload = 0;
        std::memcpy(&payload, &bytes[offset + 8], 8);
        onlyKnownSections = known;
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payload);
    }
    r.check("PAR_14_the_timeline_is_derived_and_never_serialized",
            rig.bytes() == bytes && rig.fingerprint() == fingerprint && onlyKnownSections
                    && offset == bytes.size());
}

// ---------------------------------------------------------------------------
// PAR-15: every legacy CADB layout loads and reads as a timeline
// ---------------------------------------------------------------------------

ProjectDocument documentOf(std::vector<CadBodyState> states) {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.hasCad = true;
    ObjectId id = 1;
    for (CadBodyState& state : states) {
        ProjectBodyPlacement placement;
        placement.objectId = id;
        document.scene.bodies.push_back(placement);
        ProjectCadBody body;
        body.objectId = id;
        body.state = std::move(state);
        document.cad.bodies.push_back(std::move(body));
        ++id;
    }
    document.scene.nextObjectId = id;
    document.scene.activeObjectId = 1;
    return document;
}

uint16_t cadSectionVersion(const std::vector<uint8_t>& bytes) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payload = 0;
        std::memcpy(&payload, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            uint16_t version = 0;
            std::memcpy(&version, &bytes[offset + 4], 2);
            return version;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payload);
    }
    return 0;
}

void testLegacy(Checks& r) {
    std::vector<std::pair<uint16_t, ProjectDocument>> cases;
    {  // v1: one rectangle, one extrusion.
        CadSketch s;
        const SketchEntityId rect = add(&s, rectangle(0.0, 0.0, 1.0, 1.0));
        cases.emplace_back(1, documentOf({makeCadBodyState(s, extrudeOf(rect, 1.0))}));
    }
    {  // v2: a second body whose sketch stands on the first one's far cap.
        CadSketch s;
        const SketchEntityId rect = add(&s, rectangle(0.0, 0.0, 1.0, 1.0));
        const CadBodyState producer = makeCadBodyState(s, extrudeOf(rect, 1.0));
        CadSketch face;
        const SketchEntityId small = add(&face, rectangle(0.0, 0.0, 0.4, 0.4));
        face.hasFaceSupport = true;
        face.faceSupport.producerObjectId = 1;
        face.faceSupport.producerLocalFeatureId = kCadFeatureId;
        face.faceSupport.face.kind = CadFaceKind::CapFar;
        face.faceSupport.lineageToken = cadFeatureTopologySignature(producer, kCadFeatureId);
        cases.emplace_back(2, documentOf({producer, makeCadBodyState(face, extrudeOf(small, 0.5))}));
    }
    {  // v3: an arc closes the profile with a line.
        CadSketch s;
        SketchArc arc;
        arc.start = SketchPoint{-1.0, 0.0};
        arc.mid = SketchPoint{0.0, 1.0};
        arc.end = SketchPoint{1.0, 0.0};
        const SketchEntityId arcId = add(&s, arc);
        SketchLine chord;
        chord.start = SketchPoint{1.0, 0.0};
        chord.end = SketchPoint{-1.0, 0.0};
        add(&s, chord);
        cases.emplace_back(3, documentOf({makeCadBodyState(s, extrudeOf(arcId, 0.5))}));
    }
    {  // v4: a symmetric extent.
        CadSketch s;
        const SketchEntityId rect = add(&s, rectangle(0.0, 0.0, 1.0, 1.0));
        ExtrudeFeature symmetric = extrudeOf(rect, 0.5);
        symmetric.extent = ExtrudeExtentMode::Symmetric;
        cases.emplace_back(4, documentOf({makeCadBodyState(s, symmetric)}));
    }
    // v5: the chain; v8: the chain's base carries a Driving dimension.
    {
        CadBodyState chain = chainState(false);
        cadBaseSketch(chain).dimensions.clear();
        cadBaseSketch(chain).nextDimensionId = 1;
        cases.emplace_back(5, documentOf({chain}));
    }
    {  // v6: a later feature extruding the root sketch again.
        CadBodyState shared = chainState(false);
        cadBaseSketch(shared).dimensions.clear();
        cadBaseSketch(shared).nextDimensionId = 1;
        ExtrudeFeature below = extrudeOf(1, 0.5, ExtrudeDirection::AgainstNormal);
        appendCadLaterFeature(&shared, CadFeatureOperation::Add, kBaseCadSketchId, below);
        cases.emplace_back(6, documentOf({shared}));
    }
    cases.emplace_back(7, documentOf({revolveState()}));
    cases.emplace_back(8, documentOf({chainState(true)}));

    bool all = true;
    std::string versions;
    for (const auto& entry : cases) {
        const std::vector<uint8_t> bytes = encodeProjectV1(entry.second);
        const uint16_t version = cadSectionVersion(bytes);
        versions += std::to_string(version);
        ProjectDocument decoded;
        bool ok = version == entry.first
                  && decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok;
        ConstructionScene scene{NoProjectTag{}};
        ConstructionHistory history(scene);
        SculptSession sculpt;
        ok = ok && loadProjectDocument(decoded, scene, sculpt, history) == ProjectCodecStatus::Ok;
        for (size_t b = 0; ok && b < scene.bodyCount(); ++b) {
            const CadBody* cad = scene.bodyAt(b).cadOrNull();
            const CadTimeline timeline = buildCadTimelineFor(cad->state());
            size_t features = 0;
            for (const CadTimelineRow& row : timeline.rows) {
                features += row.kind == CadTimelineRowKind::Feature ? 1u : 0u;
            }
            ok = timeline.status == CadStatus::Ok && features == cadFeatureCount(cad->state())
                 && timeline.rows.size() == features + cad->state().sketches.size();
        }
        // Reading a timeline moved no byte of the reopened project.
        ok = ok && encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction)) == bytes;
        all = all && ok;
    }
    r.check("PAR_15_every_legacy_cadb_layout_v1_to_v8_still_loads_and_reads_as_a_timeline",
            all && versions == "12345678");
}

// ---------------------------------------------------------------------------
// Performance: the longest chain the cap allows, and an upstream edit of it
// ---------------------------------------------------------------------------

void measure(Checks& r, std::string* perf) {
    // A 4 x 4 block and fifteen Add bosses on its far cap: kMaxCadFeatures.
    CadSketch base;
    const SketchEntityId rect = add(&base, rectangle(0.0, 0.0, 4.0, 4.0));
    CadBodyState state = makeCadBodyState(base, extrudeOf(rect, 1.0));
    for (uint32_t i = 0; cadFeatureCount(state) < kMaxCadFeatures; ++i) {
        CadSketch boss;
        const double u = -1.5 + static_cast<double>(i % 4u);
        const double v = -1.5 + static_cast<double>(i / 4u);
        const SketchEntityId bossRect = add(&boss, rectangle(u, v, 0.4, 0.4));
        appendCadLaterFeatureWithSketch(&state, CadFeatureOperation::Add,
                                        farCap(state, kCadFeatureId), boss,
                                        extrudeOf(bossRect, 0.3));
    }
    const auto t0 = std::chrono::steady_clock::now();
    CadBodyMesh mesh;
    CadRegenerationReport report;
    const bool built = regenerate(state, &mesh, &report);
    const double fullMicros =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    // An upstream edit: the base depth, which every boss stands on.
    CadBodyState edited = state;
    edited.extrude.depth = 1.5;
    const auto t1 = std::chrono::steady_clock::now();
    CadBodyMesh editedMesh;
    const bool rebuilt = regenerate(edited, &editedMesh);
    const CadTimeline timeline = buildCadTimelineFor(edited);
    const double editMicros =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t1).count();
    r.check("PAR_PERF_the_capped_sixteen_feature_chain_regenerates_and_reads_as_a_timeline",
            built && rebuilt && cadFeatureCount(state) == kMaxCadFeatures
                    && timeline.rows.size() == 2u * kMaxCadFeatures
                    && timeline.status == CadStatus::Ok);
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer),
                  "parametric_chain16_regen_us=%.0f parametric_upstream_edit_us=%.0f "
                  "parametric_chain16_triangles=%zu",
                  fullMicros, editMicros, editedMesh.mesh.indices.size() / 3u);
    *perf = buffer;
}

}  // namespace

void runCadTimelineSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    if (out == nullptr) {
        return;
    }
    Checks r{out};
    std::string perf;
    testTimelineShape(r);
    testUpstreamEdits(r);
    testRevolveEdits(r);
    testFailingEdits(r);
    testPersistence(r);
    testLegacy(r);
    measure(r, &perf);
    if (performance != nullptr) {
        *performance = perf;
    }
}

}  // namespace forgeshape
