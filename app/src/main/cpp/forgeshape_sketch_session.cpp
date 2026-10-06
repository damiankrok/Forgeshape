#include "forgeshape_sketch_session.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "forgeshape_cad_face.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_picking.h"

namespace forgeshape {

const char* sketchTapOutcomeName(SketchTapOutcome outcome) {
    switch (outcome) {
        case SketchTapOutcome::None: return "none";
        case SketchTapOutcome::Resolved: return "resolved";
        case SketchTapOutcome::Exterior: return "exterior";
        case SketchTapOutcome::ArrowHead: return "arrow_head";
        case SketchTapOutcome::Travel: return "travel";
        case SketchTapOutcome::RayParallel: return "ray_parallel";
        case SketchTapOutcome::InvalidFace: return "invalid_face";
        case SketchTapOutcome::SelectionCap: return "selection_cap";
    }
    return "none";
}

bool sketchEventReachesCamera(SketchSessionState state, bool consumed, int pointerCount,
                              bool readyTapArmed) {
    if (consumed) {
        return false;
    }
    if (pointerCount > 1) {
        return true;  // two fingers pan and pinch in every state
    }
    return state == SketchSessionState::Ready && !readyTapArmed;
}

const char* sketchSessionStateName(SketchSessionState state) {
    switch (state) {
        case SketchSessionState::Inactive: return "Inactive";
        case SketchSessionState::Editing: return "Editing";
        case SketchSessionState::Ready: return "Ready";
    }
    return "unknown";
}

const char* sketchToolName(SketchTool tool) {
    switch (tool) {
        case SketchTool::Select: return "Select";
        case SketchTool::Line: return "Line";
        case SketchTool::Polyline: return "Polyline";
        case SketchTool::Rectangle: return "Rectangle";
        case SketchTool::Circle: return "Circle";
        case SketchTool::Arc: return "Arc";
        case SketchTool::Spline: return "Spline";
    }
    return "unknown";
}

bool sketchToolFromIndex(int index, SketchTool* out) {
    if (out == nullptr || index < 0 || index >= kSketchToolCount) {
        return false;
    }
    *out = static_cast<SketchTool>(index);
    return true;
}

int sketchToolIndex(SketchTool tool) { return static_cast<int>(tool); }

const char* sketchModifyModeName(SketchModifyMode mode) {
    switch (mode) {
        case SketchModifyMode::None: return "None";
        case SketchModifyMode::Dimension: return "Dimension";
        case SketchModifyMode::Trim: return "Trim";
        case SketchModifyMode::Extend: return "Extend";
        case SketchModifyMode::Offset: return "Offset";
        case SketchModifyMode::Mirror: return "Mirror";
    }
    return "unknown";
}

namespace {

double distance(const SketchPoint& a, const SketchPoint& b) {
    const double du = a.u - b.u;
    const double dv = a.v - b.v;
    return std::sqrt(du * du + dv * dv);
}

bool nearlySame(const SketchPoint& a, const SketchPoint& b) {
    return distance(a, b) <= kSketchCoincidenceMeters;
}

// Distance from a point to a closed segment.
double segmentDistance(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    const double du = b.u - a.u;
    const double dv = b.v - a.v;
    const double len2 = du * du + dv * dv;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((p.u - a.u) * du + (p.v - a.v) * dv) / len2;
        t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    }
    const SketchPoint q{a.u + t * du, a.v + t * dv};
    return distance(p, q);
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

double adaptiveSketchGridStep(double worldPerPixel) {
    const double raw = worldPerPixel * kSketchGridMinPixels;
    if (!std::isfinite(raw) || raw <= 0.0) {
        return kSketchGridSpacingMeters;
    }
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    const double f = raw / p;  // in [1, 10)
    const double nice = f <= 1.0 ? 1.0 : (f <= 2.0 ? 2.0 : (f <= 5.0 ? 5.0 : 10.0));
    double step = nice * p;
    if (step < kSketchGridMinStepMeters) step = kSketchGridMinStepMeters;
    if (step > kSketchGridMaxStepMeters) step = kSketchGridMaxStepMeters;
    return step;
}

CadStatus SketchSession::beginOnFace(const SketchFrame& worldFrame, const TopoRef& support,
                                     const CadBodyState* producerState) {
    if (state_ != SketchSessionState::Inactive) {
        return fail(CadStatus::NotSketching);
    }
    if (support.producerObjectId == kNoObject) {
        return fail(CadStatus::ProfileNotFound);
    }
    const CadStatus started = begin(Workplane::XY);  // canonical authoring basis
    if (started != CadStatus::Ok) {
        return started;
    }
    // The sketch's support is a face; author on the producer's world frame.
    sketch_.hasFaceSupport = true;
    sketch_.faceSupport = support;
    frame_ = worldFrame;
    // The producer is also the body an Add or a Cut would modify. Its state is
    // staged now, so the candidate is a pure function of what the session
    // holds; the commit refuses if the body changed underneath.
    if (producerState != nullptr) {
        targetBodyId_ = support.producerObjectId;
        targetBaseState_ = *producerState;
        hasTargetState_ = true;
    }
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::begin(Workplane plane) {
    if (state_ != SketchSessionState::Inactive) {
        return fail(CadStatus::NotSketching);
    }
    if (workplaneIndex(plane) < 0 || workplaneIndex(plane) >= kWorkplaneCount) {
        return fail(CadStatus::InvalidWorkplane);
    }
    sketch_ = CadSketch{};
    sketch_.plane = plane;
    // The authoring frame is the plane's own frame at the world origin, so a
    // world-plane sketch behaves exactly as before; a face sketch overrides it.
    const WorkplaneFrame wf = workplaneFrame(plane);
    frame_ = SketchFrame{Vec3{0.0f, 0.0f, 0.0f}, wf.uAxis, wf.vAxis, wf.normal};
    gridStep_ = kSketchGridSpacingMeters;
    selectedIds_.clear();
    multiSelect_ = false;
    lastDeletedDimensions_ = 0;
    resetModifyState();
    snapCacheValid_ = false;
    lastSnap_ = SketchSnapResult{};
    regions_ = SketchRegionExtraction{};
    arrangement_ = SketchArrangement{};
    faceShapes_.clear();
    selectionLost_ = false;
    extrude_ = ExtrudeFeature{};
    oneSideDirection_ = ExtrudeDirection::AlongNormal;
    featureKind_ = CadFeatureKind::Extrude;
    revolve_ = RevolveFeature{};
    axisPicking_ = false;
    revolveDragPointer_ = -1;
    revolveDragLive_ = false;
    operation_ = CadFeatureOperation::NewBody;
    targetBodyId_ = kNoObject;
    targetBaseState_ = CadBodyState{};
    hasTargetState_ = false;
    editingFeatureId_ = 0;
    evaluation_ = CadCandidateEvaluation{};
    regionTapArmed_ = false;
    regionTapOnHead_ = false;
    tool_ = SketchTool::Rectangle;
    resetGesture();
    polylineInProgress_ = false;
    polylineVertices_.clear();
    arcPending_ = false;
    // A fresh sketch is looked at from the front, the right way up. The
    // navigator's state belongs to the session, not to the sketch, so it starts
    // over with it rather than carrying a previous sketch's roll into this one.
    viewFlipped_ = false;
    viewQuarterTurns_ = 0;
    editingBodyId_ = kNoObject;
    entitiesPlaced_ = 0;
    lastSnapKind_ = SketchSnapKind::None;
    state_ = SketchSessionState::Editing;
    touchCandidate();
    return fail(CadStatus::Ok);
}

void SketchSession::cancel() {
    resetGesture();
    polylineInProgress_ = false;
    polylineVertices_.clear();
    arcPending_ = false;
    sketch_ = CadSketch{};
    selectedIds_.clear();
    multiSelect_ = false;
    resetModifyState();
    snapCacheValid_ = false;
    lastSnap_ = SketchSnapResult{};
    regions_ = SketchRegionExtraction{};
    arrangement_ = SketchArrangement{};
    faceShapes_.clear();
    selectionLost_ = false;
    extrude_ = ExtrudeFeature{};
    featureKind_ = CadFeatureKind::Extrude;
    revolve_ = RevolveFeature{};
    axisPicking_ = false;
    revolveDragPointer_ = -1;
    revolveDragLive_ = false;
    // An edit session that is cancelled has, by construction, written nothing
    // to the body: the staged copy simply goes away with the session.
    editingBodyId_ = kNoObject;
    editingFeatureId_ = 0;
    operation_ = CadFeatureOperation::NewBody;
    targetBodyId_ = kNoObject;
    targetBaseState_ = CadBodyState{};
    hasTargetState_ = false;
    evaluation_ = CadCandidateEvaluation{};
    regionTapArmed_ = false;
    regionTapOnHead_ = false;
    ++candidateRevision_;
    viewFlipped_ = false;
    viewQuarterTurns_ = 0;
    state_ = SketchSessionState::Inactive;
    touchOverlay();
}

// ---------------------------------------------------------------------------
// Editing an existing body's sketch (`SKETCH-UX-R1` F)
// ---------------------------------------------------------------------------

CadStatus SketchSession::beginEdit(ObjectId bodyId, const CadBodyState& state,
                                   const SketchFrame& worldFrame) {
    return beginEditFeature(bodyId, state, kCadFeatureId, worldFrame, /*startReady=*/false);
}

CadStatus SketchSession::beginEditFeature(ObjectId bodyId, const CadBodyState& state,
                                          uint32_t featureId, const SketchFrame& worldFrame,
                                          bool startReady) {
    if (state_ != SketchSessionState::Inactive) {
        return fail(CadStatus::NotSketching);
    }
    if (bodyId == kNoObject) {
        return fail(CadStatus::NotCadBody);
    }
    // The state must be one this build can regenerate before it is staged: an
    // edit session opened over a state the domain would refuse could never be
    // finished, and would strand the user in a sketch with no way forward.
    const CadStatus valid = validateCadBodyState(state);
    if (valid != CadStatus::Ok) {
        return fail(valid);
    }
    CadFeatureView view;
    if (!findCadFeature(state, featureId, &view)) {
        return fail(CadStatus::ProfileNotFound);
    }
    const CadStatus started = begin(view.sketch->plane);
    if (started != CadStatus::Ok) {
        return started;
    }
    // The STAGED copy. Everything the feature knows about itself, including a
    // base's face support and every extrusion field, so Finish can regenerate
    // the whole chain rather than a sketch with a guessed depth. The rest of
    // the chain is staged with it: an edit to feature i is applied to the SAME
    // body and regenerates i..end.
    sketch_ = *view.sketch;
    extrude_ = *view.extrude;
    operation_ = view.operation;
    if (view.kind == CadFeatureKind::Revolve && view.revolve != nullptr) {
        // A Revolve stages as one (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`): its axis,
        // angle and direction, and its selection as the session's ONE
        // selection, so Finish reconciles it by the rules every selection is
        // reconciled by. The body's kind is its own and the edit keeps it.
        featureKind_ = CadFeatureKind::Revolve;
        revolve_ = *view.revolve;
        extrude_ = revolveSelectionCarrier(*view.revolve);
        axisPicking_ = false;
    }
    // The staged extent comes back with the body, and so does the One Side
    // memory a mode round trip needs: for a One Side body it is the side the
    // solid is on, and for a two-sided one the canonical `AlongNormal`.
    oneSideDirection_ = extrude_.extent == ExtrudeExtentMode::OneSide
                                ? extrude_.direction
                                : ExtrudeDirection::AlongNormal;
    frame_ = worldFrame;
    editingBodyId_ = bodyId;
    editingFeatureId_ = featureId;
    targetBodyId_ = bodyId;
    targetBaseState_ = state;
    hasTargetState_ = true;
    touchCandidate();
    if (startReady) {
        const CadStatus finished = finish();
        if (finished != CadStatus::Ok) {
            cancel();
            return fail(finished);
        }
    }
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::checkDependentsKeepTheirFaces(const ConstructionScene& scene,
                                                       ObjectId producerId,
                                                       const CadBodyState& candidate,
                                                       const CadBodyMesh& candidateMesh) const {
    // An edit -- or an Add or a Cut -- that would strip a planar face another
    // body's sketch is standing on is REFUSED, checked against the CANDIDATE
    // before anything is written. A size-only edit keeps the face SET and so
    // keeps every reference valid; one that removes a profile edge, or a Cut
    // that carves the face away entirely, does not, and the answer is a refusal
    // by name rather than a dependent that quietly stops being drawn.
    for (const ObjectId dependentId : scene.cadDependentsOf(producerId)) {
        const SceneObject* dependent = scene.findBody(dependentId);
        const CadBody* dependentCad = dependent != nullptr ? dependent->cadOrNull() : nullptr;
        if (dependentCad == nullptr || !dependentCad->sketch().hasFaceSupport) {
            continue;
        }
        const TopoRef& ref = dependentCad->sketch().faceSupport;
        CadFace face;
        if (cadFeatureTopologySignature(candidate, ref.producerLocalFeatureId) != ref.lineageToken
            || resolveCadFeatureFace(candidate, ref.producerLocalFeatureId, ref.face, &face)
                       != CadStatus::Ok
            || !face.eligible || !cadMeshCarriesFace(candidateMesh, face)) {
            return CadStatus::DependentFaceLost;
        }
    }
    return CadStatus::Ok;
}

CadStatus SketchSession::commitEdit(ConstructionScene& scene, ConstructionHistory& history) {
    if (state_ != SketchSessionState::Ready || editingBodyId_ == kNoObject) {
        return fail(CadStatus::NotSketching);
    }
    if (history.editInProgress()) {
        return fail(CadStatus::RefusedEditInProgress);
    }
    if (!selectionChosen()) {
        return fail(unchosenStatus());
    }
    SceneObject* object = scene.findBody(editingBodyId_);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        // The body went away under the edit -- deleted, or the project
        // replaced. The staged sketch is not applied to anything else.
        return fail(CadStatus::NotCadBody);
    }
    // The whole chain, regenerated from the candidate, BEFORE the transaction
    // opens: an edit to feature i that makes a later feature impossible is
    // refused here by that feature's own reason, and nothing is written.
    const CadCandidateEvaluation& evaluation = evaluateCandidate();
    if (evaluation.status != CadStatus::Ok || evaluation.mesh == nullptr) {
        return fail(evaluation.status == CadStatus::Ok ? CadStatus::RegenerationFailed
                                                       : evaluation.status);
    }
    const CadBodyState candidate = candidateState();
    const CadStatus dependents =
            checkDependentsKeepTheirFaces(scene, editingBodyId_, candidate, *evaluation.mesh);
    if (dependents != CadStatus::Ok) {
        return fail(dependents);
    }
    CadStatus why = CadStatus::Ok;
    {
        // ONE transaction, so one Finish is exactly one Undo. `applyState` is
        // itself all-or-nothing, and every dependent's world placement is
        // DERIVED through `resolveWorldModel` on the next snapshot, so nothing
        // here has to push the change to them.
        ScopedConstructionEdit edit(history);
        why = body->applyState(candidate);
        if (why == CadStatus::Ok) {
            publishSceneObject(*object);
        }
    }
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    cancel();  // the session is over; the truth is the body's again
    return fail(CadStatus::Ok);
}

// ---------------------------------------------------------------------------
// The orientation navigator (`SKETCH-UX-R1` C)
// ---------------------------------------------------------------------------

SketchFrame SketchSession::viewFrame() const {
    SketchFrame view = frame_;
    if (viewFlipped_) {
        // Looking at the back of the plane. The normal reverses and ONE of the
        // in-plane axes must reverse with it, or the basis would be left-handed
        // and the drawing would come out mirrored -- which is the one thing a
        // view control must never do to a sketch.
        view.n = vec3Scale(view.n, -1.0f);
        view.v = vec3Scale(view.v, -1.0f);
    }
    // Roll about the view normal, a quarter turn at a time. +1 turns the VIEW
    // clockwise, which sends what was at screen-up round to screen-left — so
    // the DRAWING appears to turn counter-clockwise, which is what the +90
    // control's content description says it does.
    for (int i = 0; i < viewQuarterTurns_; ++i) {
        const Vec3 u = view.u;
        view.u = vec3Scale(view.v, -1.0f);
        view.v = u;
    }
    return view;
}

void SketchSession::rotateView(int quarterTurns) {
    if (!active()) {
        return;
    }
    // Any integer, normalized into 0..3: the control sends +1 and -1, and a
    // negative or a large value must land somewhere defined rather than index
    // out of the loop above.
    int turns = (viewQuarterTurns_ + quarterTurns) % 4;
    if (turns < 0) {
        turns += 4;
    }
    viewQuarterTurns_ = turns;
    touchOverlay();
}

void SketchSession::setViewFlipped(bool flipped) {
    if (!active() || flipped == viewFlipped_) {
        return;
    }
    viewFlipped_ = flipped;
    touchOverlay();
}

CadStatus SketchSession::setSupportPlane(Workplane plane) {
    if (!active()) {
        return fail(CadStatus::NotSketching);
    }
    if (sketch_.hasFaceSupport) {
        return fail(CadStatus::InvalidWorkplane);
    }
    if (workplaneIndex(plane) < 0 || workplaneIndex(plane) >= kWorkplaneCount) {
        return fail(CadStatus::InvalidWorkplane);
    }
    if (plane == sketch_.plane) {
        return fail(CadStatus::Ok);
    }
    // The rule that makes this safe: the authored numbers only ever mean one
    // plane, so the plane may change while there are no numbers and never
    // afterwards. Nothing is remapped, projected or reinterpreted.
    if (!sketch_.entities.empty() || polylineInProgress_) {
        return fail(CadStatus::SketchNotEmpty);
    }
    sketch_.plane = plane;
    const WorkplaneFrame wf = workplaneFrame(plane);
    frame_ = SketchFrame{Vec3{0.0f, 0.0f, 0.0f}, wf.uAxis, wf.vAxis, wf.normal};
    resetGesture();
    arcPending_ = false;
    touchOverlay();
    return fail(CadStatus::Ok);
}

// ---------------------------------------------------------------------------
// The selected line's dimension (`SKETCH-UX-R1` E)
// ---------------------------------------------------------------------------

bool SketchSession::selectedLineLength(Meters* outLength) const {
    if (!active() || selectedEntityId() == kNoSketchEntity) {
        return false;
    }
    const SketchEntity* entity = findSketchEntity(sketch_, selectedEntityId());
    // A RETAINED Length dimension on the line, shown, already says this: the
    // transient annotation would be the same number drawn twice.
    for (const SketchDimension& dimension : sketch_.dimensions) {
        if (entity != nullptr && dimension.kind == SketchDimensionKind::LineLength
            && dimension.first.entityId == entity->id() && dimensionVisible(dimension)) {
            return false;
        }
    }
    if (entity == nullptr) {
        return false;
    }
    const SketchLine* line = entity->line();
    if (line == nullptr) {
        return false;
    }
    if (outLength != nullptr) {
        *outLength = distance(line->start, line->end);
    }
    return true;
}

namespace {

// The frame one dimension annotation is built in: the line's own direction and
// the perpendicular the annotation is offset along. The perpendicular is chosen
// deterministically (the direction turned a quarter turn counter-clockwise), so
// the annotation does not jump to the other side of the line when the line is
// redrawn the other way round.
struct DimensionFrame {
    SketchPoint start;
    SketchPoint end;
    double du = 1.0;   // unit direction
    double dv = 0.0;
    double pu = 0.0;   // unit perpendicular
    double pv = 1.0;
};

bool dimensionFrameFor(const SketchLine& line, DimensionFrame* out) {
    const double dx = line.end.u - line.start.u;
    const double dy = line.end.v - line.start.v;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(length) || length <= 0.0) {
        return false;
    }
    out->start = line.start;
    out->end = line.end;
    out->du = dx / length;
    out->dv = dy / length;
    out->pu = -out->dv;
    out->pv = out->du;
    return true;
}

SketchPoint offsetBy(const SketchPoint& p, double du, double dv, double amount) {
    return SketchPoint{p.u + du * amount, p.v + dv * amount};
}

}  // namespace

bool SketchSession::selectedLineDimensionAnchor(double worldPerUnit, SketchPoint* out) const {
    if (out == nullptr || !std::isfinite(worldPerUnit) || worldPerUnit <= 0.0) {
        return false;
    }
    if (!selectedLineLength(nullptr)) {
        return false;
    }
    const SketchEntity* entity = findSketchEntity(sketch_, selectedEntityId());
    const SketchLine* line = entity != nullptr ? entity->line() : nullptr;
    DimensionFrame f;
    if (line == nullptr || !dimensionFrameFor(*line, &f)) {
        return false;
    }
    const double offset = kSketchDimensionOffsetUnits * worldPerUnit;
    const SketchPoint mid{(f.start.u + f.end.u) * 0.5, (f.start.v + f.end.v) * 0.5};
    *out = offsetBy(mid, f.pu, f.pv, offset);
    return true;
}

CadStatus SketchSession::applyLineLength(SketchEntityId id, Meters length) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    const SketchEntity* entity = findSketchEntity(sketch_, id);
    if (entity == nullptr) {
        return fail(CadStatus::UnknownEntity);
    }
    const SketchLine* line = entity->line();
    if (line == nullptr) {
        return fail(CadStatus::UnknownEntity);
    }
    if (!std::isfinite(length)) {
        return fail(CadStatus::NonFinite);
    }
    // Zero and negative are refused rather than clamped: a length is a length,
    // and "make it -40 mm" is not a shorter line, it is a different question.
    if (validateDimensionMeters(length) != DimensionValidation::Ok
        || length > kMaxSketchCoordinateMeters) {
        return fail(CadStatus::ZeroLengthLine);
    }
    const double current = distance(line->start, line->end);
    if (!(current > 0.0)) {
        return fail(CadStatus::ZeroLengthLine);
    }
    const double scale = length / current;
    SketchLine moved;
    // P0 FIXED, direction PRESERVED: the second endpoint alone moves, along the
    // line it was already on.
    moved.start = line->start;
    moved.end = SketchPoint{line->start.u + (line->end.u - line->start.u) * scale,
                            line->start.v + (line->end.v - line->start.v) * scale};
    const CadStatus why = replaceSketchEntity(&sketch_, id, moved);
    if (why == CadStatus::Ok) {
        touchOverlay();
    }
    return fail(why);
}

bool SketchSession::setTool(SketchTool tool) {
    if (sketchToolIndex(tool) < 0 || sketchToolIndex(tool) >= kSketchToolCount) {
        return false;
    }
    if (tool == tool_) {
        if (modifyMode_ != SketchModifyMode::None) {
            resetModifyState();
            touchOverlay();
        }
        return true;
    }
    // Changing tool ends a polyline being placed and drops a drag: a gesture
    // belongs to the tool it started under.
    endPolylineInProgress();
    resetGesture();
    // A pending arc chord belongs to the Arc tool; changing tool drops it, as
    // a drag in progress is dropped.
    arcPending_ = false;
    // A drawing tool and a modify mode are two meanings for one tap: choosing
    // the tool ends the mode (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`).
    resetModifyState();
    tool_ = tool;
    touchOverlay();
    return true;
}

void SketchSession::resetGesture() {
    if (pointerId_ >= 0) {
        touchOverlay();
    }
    pointerId_ = -1;
    travelled_ = false;
    dragValid_ = false;
    // A manipulator drag in flight is CANCELLED rather than merely dropped: the
    // SIDE it was moving goes back to what the finger found, exactly as a
    // cancelled gizmo drag restores the placement. Nothing was recorded either
    // way -- an uncommitted sketch is volatile -- so this is about what the user
    // sees, not about the project.
    restoreCancelledExtrudeDrag();
    cancelRevolveDrag();
}

// ---------------------------------------------------------------------------
// Mapping and snapping
// ---------------------------------------------------------------------------

bool SketchSession::screenToSketch(const CameraSnapshot& camera, float x, float y,
                                   int viewportWidth, int viewportHeight,
                                   SketchPoint* out) const {
    if (out == nullptr) {
        return false;
    }
    Ray ray;
    if (!buildPickRay(camera, x, y, viewportWidth, viewportHeight, &ray)) {
        return false;
    }
    Vec3 hit;
    // Intersect with the authoring frame's plane, and read (u, v) off the
    // frame's own axes. For a world-plane sketch the frame is the plane at the
    // origin, so this is identical to CAD-R0; for a face sketch it is the
    // producer's face frame in world space.
    if (!intersectRayPlane(ray, frame_.origin, frame_.n, &hit)) {
        return false;
    }
    const Vec3 d = vec3Sub(hit, frame_.origin);
    out->u = static_cast<double>(vec3Dot(d, frame_.u));
    out->v = static_cast<double>(vec3Dot(d, frame_.v));
    return std::isfinite(out->u) && std::isfinite(out->v);
}

bool SketchSession::sketchToScreen(const CameraSnapshot& camera, const SketchPoint& point,
                                   int viewportWidth, int viewportHeight, float* outX,
                                   float* outY) const {
    return projectWorldToScreen(camera, sketchToWorld(point), viewportWidth, viewportHeight, outX,
                                outY);
}

Vec3 SketchSession::sketchToWorld(const SketchPoint& p) const {
    return vec3Add(frame_.origin, vec3Add(vec3Scale(frame_.u, static_cast<float>(p.u)),
                                          vec3Scale(frame_.v, static_cast<float>(p.v))));
}

Vec3 SketchSession::sketchToWorldAt(const SketchPoint& p, double offset) const {
    return vec3Add(sketchToWorld(p), vec3Scale(frame_.n, static_cast<float>(offset)));
}

void SketchSession::sketchViewAngles(Workplane plane, float* outYaw, float* outPitch) {
    // See forgeshape_workplane.h: each plane is read from its positive normal
    // with U right and V up. The camera's yaw 0 / pitch 0 looks along -Z from
    // +Z; yaw pi/2 looks along -X from +X; the pitch limit looks down from +Y.
    switch (plane) {
        case Workplane::XZ:
            *outYaw = 0.0f;
            *outPitch = kPitchLimitRadians;
            break;
        case Workplane::YZ:
            *outYaw = 1.57079632679489661923f;
            *outPitch = 0.0f;
            break;
        case Workplane::XY:
        default:
            *outYaw = 0.0f;
            *outPitch = 0.0f;
            break;
    }
}

const SketchSnapCandidates& SketchSession::snapCandidates() const {
    // Derived per sketch CONTENT, so a pointer move costs a scan of exact
    // points and never re-derives the arrangement.
    if (!snapCacheValid_ || !sameCadSketch(snapSketch_, sketch_)) {
        snapSketch_ = sketch_;
        snapCache_ = collectSketchSnapCandidates(sketch_);
        snapCacheValid_ = true;
    }
    return snapCache_;
}

SketchSession::SnapResult SketchSession::snap(const SketchPoint& raw, double snapWorld) {
    // THE one priority (forgeshape_sketch_snap.h). The polyline being placed
    // offers its own vertices as endpoints, and a gesture's start -- the
    // drag's anchor or the run's last vertex -- is a guide source, so a line
    // can be drawn exactly horizontal or vertical from where it began.
    const std::vector<SketchPoint> extra =
            polylineInProgress_ ? polylineVertices_ : std::vector<SketchPoint>{};
    const SketchPoint* guide = nullptr;
    SketchPoint guideAt;
    const SketchPoint* dragStart = pointerId_ >= 0 && dragValid_ ? &anchor_ : nullptr;
    if (polylineInProgress_ && !polylineVertices_.empty()) {
        guideAt = polylineVertices_.back();
        guide = &guideAt;
    } else if (pointerId_ >= 0 && dragValid_ && tool_ != SketchTool::Rectangle) {
        // Not for a rectangle: its corner aligned with its own start is a
        // rectangle with no width, never what the drag meant.
        guideAt = anchor_;
        guide = &guideAt;
    }
    const double step = gridStep_ > 0.0 ? gridStep_ : kSketchGridSpacingMeters;
    lastSnap_ = snapSketchPoint(snapCandidates(), raw, snapWorld, step, extra, guide, dragStart);
    SnapResult result;
    result.point = lastSnap_.point;
    result.kind = lastSnap_.kind;
    return result;
}

bool SketchSession::pointerToSketch(const CameraSnapshot& camera, float x, float y,
                                    int viewportWidth, int viewportHeight, SnapResult* out) {
    SketchPoint raw;
    if (!screenToSketch(camera, x, y, viewportWidth, viewportHeight, &raw)) {
        return false;
    }
    rawCursor_ = raw;
    *out = snap(raw, kSketchSnapToleranceUnits * worldPerUnit_);
    lastSnapKind_ = out->kind;
    return true;
}

SketchEntityId SketchSession::hitTest(const SketchPoint& point, double toleranceWorld) const {
    // ONE hit rule for every sketch tap (forgeshape_sketch_drafting.h): curves
    // against the derived polyline they are drawn with, ties to the earlier.
    return hitSketchEntity(sketch_, point, toleranceWorld);
}

// ---------------------------------------------------------------------------
// Touch
// ---------------------------------------------------------------------------

// The Ready-state gesture: one pointer on the extrude arrow (`CAD-UX-S1`).
//
// The contract is the gizmo one and is deliberately identical to it: whether
// the touch landed on the arrow is decided ONCE, on Down, against the same
// projected geometry the renderer drew, so a drag cannot turn into an orbit
// half way through. A pointer that goes down anywhere else navigates exactly as
// it always has, and a second pointer CANCELS -- putting the depth back where
// the first finger found it -- rather than trying to drag and pinch at once.
// Puts the dragged SIDE back where the finger found it and reports whether a
// drag was in fact captured. One place, because a cancel arrives four ways --
// a second pointer, a pointer-down, the wrong finger lifting and an explicit
// Cancel -- and four copies of a restore is how one of them comes to restore
// the wrong side.
bool SketchSession::restoreCancelledExtrudeDrag() {
    const bool positiveSide = extrudeDrag_.capturedPositiveSide();
    Meters restore = 0.0;
    if (!extrudeDrag_.cancelDrag(&restore)) {
        return false;
    }
    // Written directly rather than through the validating door: this is the
    // value the extrusion HAD a moment ago, so it needs no re-validation, and
    // routing it through one would let a refusal strand the gesture's undo.
    extrude_ = extrudeFeatureWithSide(extrude_, positiveSide, restore);
    touchCandidate();
    return true;
}

bool SketchSession::onExtrudeTouch(TouchAction action, int32_t actionPointerId,
                                   const TouchPointer* pointers, int count,
                                   const CameraSnapshot& camera, int viewportWidth,
                                   int viewportHeight) {
    if (count > 1 || action == TouchAction::PointerDown) {
        regionTapArmed_ = false;
        regionTapOnHead_ = false;
        restoreCancelledExtrudeDrag();
        return false;  // the gesture belongs to the camera
    }
    switch (action) {
        case TouchAction::Down: {
            if (count != 1 || pointers == nullptr) {
                return false;
            }
            // A single finger that misses the arrow still NAVIGATES (the
            // camera gets every event), and if it lifts without travelling it
            // was a TAP that picks a region (`CAD-VERTICAL-SLICE-R1`). Armed
            // here, decided on Up; a drag or a second finger disarms it.
            regionTapArmed_ = true;
            regionTapPointer_ = pointers[0].id;
            regionTapX_ = pointers[0].x;
            regionTapY_ = pointers[0].y;
            regionTapCamera_ = camera;
            regionTapOnHead_ = false;
            CadExtrudeAnchors anchors;
            if (!extrudeAnchors(&anchors)) {
                return false;
            }
            // WHICH side is decided once, on Down, exactly as whether the arrow
            // was hit at all is: a Symmetric drag cannot change sides half way
            // through any more than it can become an orbit.
            bool positiveSide = true;
            if (!extrudeDrag_.hitTest(anchors, camera, pointers[0].x, pointers[0].y, viewportWidth,
                                      viewportHeight, &positiveSide)) {
                return false;  // off the arrow: orbit, pan and tap are untouched
            }
            // ON the drawn arrow HEAD the finger means the arrow, at once.
            // Anywhere else in the grab corridor -- the shaft included -- the
            // gesture is captured, so a drag still takes the arrow, but the tap
            // stays armed: the shaft stands ON the chosen area and, with its
            // corridor, crosses the cells around it, and a still tap there is a
            // fill-bucket tap on the cell under the finger
            // (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`; before it, the whole drawn
            // shaft claimed the still tap and the tap did nothing).
            if (extrudeDrag_.onDrawnArrowHead(anchors, camera, pointers[0].x, pointers[0].y,
                                              viewportWidth, viewportHeight)) {
                regionTapArmed_ = false;
                regionTapOnHead_ = true;
            }
            if (!extrudeDrag_.beginDrag(pointers[0].id, anchors, positiveSide, camera,
                                        pointers[0].x, pointers[0].y, viewportWidth,
                                        viewportHeight)) {
                return false;
            }
            touchOverlay();
            return true;
        }
        case TouchAction::Move: {
            if (regionTapArmed_ && count == 1 && pointers != nullptr
                && pointers[0].id == regionTapPointer_
                && std::hypot(pointers[0].x - regionTapX_, pointers[0].y - regionTapY_)
                           > kSketchTapSlopPixels) {
                regionTapArmed_ = false;  // it became an orbit (or a drag)
                recordTapOutcome(SketchTapOutcome::Travel);
            }
            if (regionTapOnHead_ && count == 1 && pointers != nullptr
                && pointers[0].id == regionTapPointer_
                && std::hypot(pointers[0].x - regionTapX_, pointers[0].y - regionTapY_)
                           > kSketchTapSlopPixels) {
                regionTapOnHead_ = false;  // a head drag, not a still tap
            }
            if (!extrudeDrag_.capturing() || count != 1 || pointers == nullptr
                || pointers[0].id != extrudeDrag_.capturedPointerId()) {
                return extrudeDrag_.capturing();
            }
            if (regionTapArmed_) {
                // Still possibly a tap: the depth holds until the finger has
                // travelled past the tap slop, so a tap leaves nothing behind.
                return true;
            }
            Meters distance = 0.0;
            if (extrudeDrag_.updateDrag(pointers[0].id, camera, pointers[0].x, pointers[0].y,
                                        viewportWidth, viewportHeight, &distance)) {
                // Through the ONE writer, so a dragged distance passes exactly
                // the validation a typed one does. A refusal leaves the extent
                // alone, which is the same "hold the last good value" a
                // degenerate viewpoint produces.
                setExtrudeSide(extrudeDrag_.capturedPositiveSide(), distance);
            }
            return true;
        }
        case TouchAction::Up:
        case TouchAction::PointerUp: {
            if (!extrudeDrag_.capturing()) {
                if (regionTapArmed_ && action == TouchAction::Up
                    && (actionPointerId < 0 || actionPointerId == regionTapPointer_)) {
                    const float x = count >= 1 && pointers != nullptr ? pointers[0].x : regionTapX_;
                    const float y = count >= 1 && pointers != nullptr ? pointers[0].y : regionTapY_;
                    if (std::hypot(x - regionTapX_, y - regionTapY_) <= kSketchTapSlopPixels) {
                        // The DOWN camera and the Down pixel: the cell the
                        // finger meant when it landed.
                        toggleRegionAt(regionTapCamera_, regionTapX_, regionTapY_, viewportWidth,
                                       viewportHeight);
                    } else {
                        recordTapOutcome(SketchTapOutcome::Travel);
                    }
                }
                regionTapArmed_ = false;
                regionTapOnHead_ = false;
                // Never consumed: the camera saw the Down and must see the Up.
                return false;
            }
            if (actionPointerId >= 0 && actionPointerId != extrudeDrag_.capturedPointerId()) {
                regionTapArmed_ = false;
                regionTapOnHead_ = false;
                restoreCancelledExtrudeDrag();
                return false;
            }
            if (regionTapArmed_ && action == TouchAction::Up) {
                // A still tap in the corridor, off the arrow head: it never
                // moved the depth, so the capture ends with nothing written and
                // the cell under the finger -- at the Down, through the Down
                // camera -- toggles.
                regionTapArmed_ = false;
                restoreCancelledExtrudeDrag();
                toggleRegionAt(regionTapCamera_, regionTapX_, regionTapY_, viewportWidth,
                               viewportHeight);
                return true;
            }
            if (regionTapOnHead_ && action == TouchAction::Up) {
                recordTapOutcome(SketchTapOutcome::ArrowHead);
            }
            regionTapArmed_ = false;
            regionTapOnHead_ = false;
            extrudeDrag_.endDrag();
            touchOverlay();
            return true;
        }
        case TouchAction::Cancel: {
            regionTapArmed_ = false;
            regionTapOnHead_ = false;
            return restoreCancelledExtrudeDrag();
        }
        default:
            return extrudeDrag_.capturing();
    }
}

bool SketchSession::onTouch(TouchAction action, int32_t actionPointerId,
                            const TouchPointer* pointers, int count, const CameraSnapshot& camera,
                            int viewportWidth, int viewportHeight) {
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }
    // READY is the manipulator home, and it is the only one (`CAD-UX-S1`).
    //
    // While the sketch is being DRAWN the single finger belongs to the drawing;
    // an arrow competing for it would give one gesture two meanings. Once
    // Finish Sketch has been taken the drawing is done and the finger is free,
    // which is exactly when the extrusion becomes the thing being adjusted. Two
    // fingers still pan and pinch in both states, unchanged.
    if (state_ == SketchSessionState::Ready) {
        if (featureKind_ == CadFeatureKind::Revolve) {
            return onRevolveTouch(action, actionPointerId, pointers, count, camera, viewportWidth,
                                  viewportHeight);
        }
        return onExtrudeTouch(action, actionPointerId, pointers, count, camera, viewportWidth,
                              viewportHeight);
    }
    if (state_ != SketchSessionState::Editing) {
        return false;
    }
    // A modify mode owns the single finger outright while it is active
    // (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`): one tap, one meaning.
    if (modifyMode_ != SketchModifyMode::None) {
        return onModifyTouch(action, actionPointerId, pointers, count, camera, viewportWidth,
                             viewportHeight);
    }

    // A second pointer, however it arrives, ends the sketch's claim on the
    // gesture: the drag in progress is dropped (no entity), and the event is
    // handed on so the camera can pan or pinch. The polyline being placed
    // vertex by vertex is NOT dropped -- it is anchored in placed vertices,
    // not in the finger.
    if (count > 1 || action == TouchAction::PointerDown) {
        resetGesture();
        return false;
    }

    switch (action) {
        case TouchAction::Down: {
            if (count != 1) {
                return false;
            }
            float perPixel = 0.0f;
            if (!worldMetersPerPixel(camera, frame_.origin, viewportHeight, &perPixel)) {
                return false;
            }
            worldPerUnit_ = static_cast<double>(perPixel) * gizmoPixelsPerReferenceUnit();
            // The grid step is fixed for the drag at the zoom it started under,
            // exactly as the tolerances are.
            gridStep_ = adaptiveSketchGridStep(static_cast<double>(perPixel));
            SnapResult at;
            if (!pointerToSketch(camera, pointers[0].x, pointers[0].y, viewportWidth,
                                 viewportHeight, &at)) {
                return false;  // the ray missed the plane: navigation, not sketching
            }
            pointerId_ = pointers[0].id;
            downX_ = pointers[0].x;
            downY_ = pointers[0].y;
            travelled_ = false;
            anchor_ = at.point;
            cursor_ = at.point;
            dragValid_ = true;
            touchOverlay();
            return true;
        }
        case TouchAction::Move: {
            if (pointerId_ < 0 || count != 1 || pointers[0].id != pointerId_) {
                return pointerId_ >= 0;
            }
            const float dx = pointers[0].x - downX_;
            const float dy = pointers[0].y - downY_;
            if (dx * dx + dy * dy >= kSketchTapSlopPixels * kSketchTapSlopPixels) {
                travelled_ = true;
            }
            SnapResult at;
            if (pointerToSketch(camera, pointers[0].x, pointers[0].y, viewportWidth,
                                viewportHeight, &at)) {
                cursor_ = at.point;
                touchOverlay();
            }
            return true;
        }
        case TouchAction::Up:
        case TouchAction::PointerUp: {
            if (pointerId_ < 0) {
                return false;
            }
            if (actionPointerId != pointerId_ && count != 1) {
                resetGesture();
                return false;
            }
            const bool tap = !travelled_;
            const SnapResult at{cursor_, lastSnapKind_};
            switch (tool_) {
                case SketchTool::Select: {
                    if (tap) {
                        // Hit-tested at the RAW point: a snap is about where a
                        // new point lands, never about which stroke was meant.
                        const SketchEntityId hit =
                                hitTest(rawCursor_, kSketchHitToleranceUnits * worldPerUnit_);
                        if (hit == kNoSketchEntity) {
                            selectedIds_.clear();  // empty space clears, in either mode
                        } else if (multiSelect_) {
                            toggleSelect(hit);
                        } else {
                            selectedIds_.assign(1, hit);
                        }
                        lastStatus_ = CadStatus::Ok;
                    }
                    break;
                }
                case SketchTool::Polyline:
                case SketchTool::Spline:
                    // Both are a run of tapped points; the tool held when the
                    // run ends decides which entity it becomes.
                    if (tap) {
                        placePolylineVertex(at);
                    }
                    break;
                case SketchTool::Arc:
                    // Two steps. The DRAG sets the chord -- where the arc
                    // starts and where it ends -- and the next TAP sets the
                    // point it passes through, which is what decides the bulge
                    // and which of the circle's two arcs was meant.
                    if (arcPending_) {
                        placeArcThrough(cursor_);
                    } else if (dragValid_ && travelled_ && !nearlySame(anchor_, cursor_)) {
                        arcPending_ = true;
                        arcStart_ = anchor_;
                        arcEnd_ = cursor_;
                        lastStatus_ = CadStatus::Ok;
                    }
                    break;
                case SketchTool::Line:
                case SketchTool::Rectangle:
                case SketchTool::Circle:
                    if (dragValid_) {
                        placeFromDrag();
                    }
                    break;
            }
            pointerId_ = -1;
            travelled_ = false;
            dragValid_ = false;
            touchOverlay();
            return true;
        }
        case TouchAction::Cancel: {
            const bool owned = pointerId_ >= 0;
            resetGesture();
            return owned;
        }
        default:
            return false;
    }
}

void SketchSession::placeFromDrag() {
    SketchEntityId id = kNoSketchEntity;
    CadStatus why = CadStatus::Ok;
    switch (tool_) {
        case SketchTool::Line: {
            if (nearlySame(anchor_, cursor_)) {
                return;  // a tap with the line tool places nothing
            }
            why = addSketchEntity(&sketch_, SketchLine{anchor_, cursor_}, &id);
            break;
        }
        case SketchTool::Rectangle: {
            SketchRectangle rectangle;
            rectangle.center = SketchPoint{(anchor_.u + cursor_.u) * 0.5,
                                           (anchor_.v + cursor_.v) * 0.5};
            rectangle.width = std::fabs(cursor_.u - anchor_.u);
            rectangle.height = std::fabs(cursor_.v - anchor_.v);
            if (rectangle.width <= kSketchCoincidenceMeters
                || rectangle.height <= kSketchCoincidenceMeters) {
                return;  // a tap, or a drag along one axis: nothing to place
            }
            why = addSketchEntity(&sketch_, rectangle, &id);
            break;
        }
        case SketchTool::Circle: {
            SketchCircle circle;
            circle.center = anchor_;
            circle.radius = distance(anchor_, cursor_);
            if (circle.radius <= kSketchCoincidenceMeters) {
                return;
            }
            why = addSketchEntity(&sketch_, circle, &id);
            break;
        }
        default:
            return;
    }
    lastStatus_ = why;
    if (why == CadStatus::Ok) {
        ++entitiesPlaced_;
        selectedIds_.assign(1, id);
    }
}

void SketchSession::placeArcThrough(const SketchPoint& through) {
    SketchArc arc;
    arc.start = arcStart_;
    arc.mid = through;
    arc.end = arcEnd_;
    SketchEntityId id = kNoSketchEntity;
    // Held to `validateSketchEntity` like every other placement: a tap that
    // lands on the chord is collinear, and an arc with no circle through its
    // three points is refused by name rather than becoming a straight line the
    // user did not ask for. The pending chord is dropped either way, so a
    // refusal does not leave the tool half-armed.
    const CadStatus why = addSketchEntity(&sketch_, arc, &id);
    arcPending_ = false;
    lastStatus_ = why;
    if (why == CadStatus::Ok) {
        ++entitiesPlaced_;
        selectedIds_.assign(1, id);
    }
}

void SketchSession::placePolylineVertex(const SnapResult& at) {
    if (!polylineInProgress_) {
        polylineInProgress_ = true;
        polylineVertices_.clear();
        polylineVertices_.push_back(at.point);
        lastStatus_ = CadStatus::Ok;
        return;
    }
    // Tapping the FIRST vertex again closes the loop; tapping the LAST one
    // again ends it open. Anything else is one more vertex.
    //
    // A SPLINE has no closed form -- a curve whose two ends meet is the one
    // shape the single chain walker cannot read, and `validateSketchEntity`
    // refuses it -- so for the Spline tool a tap on the first point ends the
    // run open, exactly as a tap on the last one does.
    if (tool_ == SketchTool::Spline && polylineVertices_.size() >= 2
        && nearlySame(at.point, polylineVertices_.front())) {
        endPolylineInProgress();
        return;
    }
    if (tool_ != SketchTool::Spline && polylineVertices_.size() >= 3
        && nearlySame(at.point, polylineVertices_.front())) {
        SketchPolyline polyline;
        polyline.vertices = polylineVertices_;
        polyline.closed = true;
        SketchEntityId id = kNoSketchEntity;
        lastStatus_ = addSketchEntity(&sketch_, std::move(polyline), &id);
        polylineInProgress_ = false;
        polylineVertices_.clear();
        if (lastStatus_ == CadStatus::Ok) {
            ++entitiesPlaced_;
            selectedIds_.assign(1, id);
        }
        return;
    }
    if (nearlySame(at.point, polylineVertices_.back())) {
        endPolylineInProgress();
        return;
    }
    // Each tool is held to its OWN cap, because a spline's authored points are
    // a much smaller budget than a polyline's vertices.
    const size_t cap = tool_ == SketchTool::Spline ? kMaxSplinePoints : kMaxPolylineVertices;
    if (polylineVertices_.size() >= cap) {
        lastStatus_ = CadStatus::TooManyEntities;
        return;
    }
    polylineVertices_.push_back(at.point);
    lastStatus_ = CadStatus::Ok;
}

void SketchSession::endPolylineInProgress() {
    if (!polylineInProgress_) {
        return;
    }
    polylineInProgress_ = false;
    if (polylineVertices_.size() >= 2) {
        SketchEntityId id = kNoSketchEntity;
        if (tool_ == SketchTool::Spline) {
            SketchSpline spline;
            spline.points = polylineVertices_;
            lastStatus_ = addSketchEntity(&sketch_, std::move(spline), &id);
        } else {
            SketchPolyline polyline;
            polyline.vertices = polylineVertices_;
            polyline.closed = false;
            lastStatus_ = addSketchEntity(&sketch_, std::move(polyline), &id);
        }
        if (lastStatus_ == CadStatus::Ok) {
            ++entitiesPlaced_;
            selectedIds_.assign(1, id);
        }
    }
    polylineVertices_.clear();
    touchOverlay();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool SketchSession::entitySelected(SketchEntityId id) const {
    return std::find(selectedIds_.begin(), selectedIds_.end(), id) != selectedIds_.end();
}

bool SketchSession::select(SketchEntityId id) {
    if (!active() || findSketchEntity(sketch_, id) == nullptr) {
        return false;
    }
    selectedIds_.assign(1, id);
    touchOverlay();
    return true;
}

bool SketchSession::toggleSelect(SketchEntityId id) {
    if (!active() || findSketchEntity(sketch_, id) == nullptr) {
        return false;
    }
    const auto at = std::find(selectedIds_.begin(), selectedIds_.end(), id);
    if (at != selectedIds_.end()) {
        selectedIds_.erase(at);
    } else {
        selectedIds_.push_back(id);
    }
    touchOverlay();
    return true;
}

void SketchSession::clearSelection() {
    selectedIds_.clear();
    touchOverlay();
}

void SketchSession::setMultiSelect(bool on) {
    if (multiSelect_ == on) {
        return;
    }
    multiSelect_ = on;
    touchOverlay();
}

CadStatus SketchSession::deleteSelected() {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    if (selectedIds_.empty()) {
        return fail(CadStatus::UnknownEntity);
    }
    uint32_t removed = 0;
    const CadStatus why = deleteSketchEntities(&sketch_, selectedIds_, &removed);
    if (why == CadStatus::Ok) {
        lastDeletedDimensions_ = removed;
        selectedIds_.clear();
        resetModifyState();
        touchOverlay();
    }
    return fail(why);
}

CadStatus SketchSession::replaceEntity(SketchEntityId id, SketchEntity::Payload payload) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    const CadStatus why = replaceSketchEntity(&sketch_, id, std::move(payload));
    if (why == CadStatus::Ok) {
        touchOverlay();
    }
    return fail(why);
}

// ---------------------------------------------------------------------------
// Drafting (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`)
// ---------------------------------------------------------------------------
//
// Every act here edits the STAGED sketch the session already owns, exactly as
// placing a line does: nothing is a history step or project truth until the
// one commit, a Cancel costs the project nothing, and the derived profiles are
// simply re-read at the next Finish or preview.

void SketchSession::resetModifyState() {
    modifyMode_ = SketchModifyMode::None;
    dimensionTargetSet_ = false;
    dimensionFirst_ = CadSketchEdgeRef{};
    dimensionAwaitSecond_ = false;
    offsetSource_ = kNoSketchEntity;
    offsetDistance_ = 0.0;
    mirrorAxisSet_ = false;
    mirrorAxis_ = CadSketchEdgeRef{};
    modifyPreview_.clear();
}

SketchEntityRole SketchSession::selectionConstructionTarget() const {
    if (selectedIds_.empty()) {
        return SketchEntityRole::Regular;
    }
    return sketchRoleToggleTarget(sketch_, selectedIds_);
}

CadStatus SketchSession::toggleSelectionConstruction(SketchEntityRole* outApplied) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    if (selectedIds_.empty()) {
        return fail(CadStatus::UnknownEntity);
    }
    const SketchEntityRole role = sketchRoleToggleTarget(sketch_, selectedIds_);
    const CadStatus why = setSketchEntitiesRole(&sketch_, selectedIds_, role);
    if (why == CadStatus::Ok) {
        if (outApplied != nullptr) *outApplied = role;
        touchCandidate();
    }
    return fail(why);
}

CadStatus SketchSession::setModifyMode(SketchModifyMode mode) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    if (static_cast<unsigned>(mode) >= static_cast<unsigned>(kSketchModifyModeCount)) {
        return fail(CadStatus::NotSketching);
    }
    if (mode == SketchModifyMode::Mirror && selectedIds_.empty()) {
        return fail(CadStatus::MirrorNothingSelected);
    }
    // A run being placed and a pending arc belong to the drawing tool; a mode
    // change ends them exactly as a tool change does.
    endPolylineInProgress();
    arcPending_ = false;
    resetGesture();
    resetModifyState();
    modifyMode_ = mode;
    const SketchEntityId single = selectedEntityId();
    const SketchEntity* entity = single != kNoSketchEntity ? findSketchEntity(sketch_, single) : nullptr;
    if (mode == SketchModifyMode::Offset && entity != nullptr && sketchEntityOffsettable(*entity)) {
        offsetSource_ = single;
        offsetDistance_ = 4.0 * (gridStep_ > 0.0 ? gridStep_ : kSketchGridSpacingMeters);
    }
    if (mode == SketchModifyMode::Dimension && entity != nullptr) {
        dimensionFirst_ = CadSketchEdgeRef{single, 0u};
        dimensionTargetSet_ = true;
    }
    touchOverlay();
    return fail(CadStatus::Ok);
}

bool SketchSession::dimensionTarget(CadSketchEdgeRef* out) const {
    if (modifyMode_ != SketchModifyMode::Dimension || !dimensionTargetSet_) {
        return false;
    }
    if (out != nullptr) *out = dimensionFirst_;
    return true;
}

std::vector<SketchDimensionKind> SketchSession::dimensionTargetKinds() const {
    std::vector<SketchDimensionKind> kinds;
    if (!dimensionTarget(nullptr)) {
        return kinds;
    }
    // Entity-level kinds are asked of the whole entity (edge 0); edge kinds of
    // the edge the finger chose -- so a tap on a rectangle's right side offers
    // its width and height AND that side's own length.
    const CadSketchEdgeRef entityRef{dimensionFirst_.entityId, 0u};
    for (SketchDimensionKind kind : applicableSketchDimensionKinds(sketch_, entityRef)) {
        if (!sketchDimensionKindNamesEdge(kind)) kinds.push_back(kind);
    }
    for (SketchDimensionKind kind : applicableSketchDimensionKinds(sketch_, dimensionFirst_)) {
        if (sketchDimensionKindNamesEdge(kind)) kinds.push_back(kind);
    }
    std::sort(kinds.begin(), kinds.end());
    return kinds;
}

CadStatus SketchSession::setDimensionTarget(const CadSketchEdgeRef& ref) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Dimension) {
        return fail(CadStatus::NotSketching);
    }
    if (findSketchEntity(sketch_, ref.entityId) == nullptr) {
        return fail(CadStatus::UnknownEntity);
    }
    dimensionFirst_ = ref;
    dimensionTargetSet_ = true;
    dimensionAwaitSecond_ = false;
    selectedIds_.assign(1, ref.entityId);
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::beginDimensionAngle() {
    if (!dimensionTarget(nullptr)) {
        return fail(CadStatus::NotSketching);
    }
    const SketchEntity* entity = findSketchEntity(sketch_, dimensionFirst_.entityId);
    bool straight = false;
    for (const SketchStraightEdge& edge :
         entity != nullptr ? sketchEntityStraightEdges(*entity) : std::vector<SketchStraightEdge>{}) {
        straight = straight || edge.edgeLocalIndex == dimensionFirst_.edgeLocalIndex;
    }
    if (!straight) {
        return fail(CadStatus::SketchDimensionInvalid);
    }
    dimensionAwaitSecond_ = true;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::addDimension(SketchDimensionKind kind, SketchDimensionMode mode,
                                      SketchDimensionId* outId) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    if (!dimensionTarget(nullptr) || kind == SketchDimensionKind::EdgeAngle) {
        // An angle between two edges is made by its second tap.
        return fail(CadStatus::SketchDimensionInvalid);
    }
    const CadSketchEdgeRef first = sketchDimensionKindNamesEdge(kind)
                                           ? dimensionFirst_
                                           : CadSketchEdgeRef{dimensionFirst_.entityId, 0u};
    SketchDimensionId id = kNoSketchDimension;
    const CadStatus why = addSketchDimension(&sketch_, kind, mode, first, CadSketchEdgeRef{}, &id);
    if (why == CadStatus::Ok) {
        if (outId != nullptr) *outId = id;
        touchOverlay();
    }
    return fail(why);
}

CadStatus SketchSession::removeDimension(SketchDimensionId id) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    const CadStatus why = removeSketchDimension(&sketch_, id);
    if (why == CadStatus::Ok) {
        touchOverlay();
    }
    return fail(why);
}

CadStatus SketchSession::applyDimensionValue(SketchDimensionId id, double value) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    const CadStatus why = applySketchDimensionValue(&sketch_, id, value);
    if (why == CadStatus::Ok) {
        touchCandidate();
    }
    return fail(why);
}

void SketchSession::setDimensionVisibility(SketchDimensionVisibility visibility) {
    if (static_cast<unsigned>(visibility) >= static_cast<unsigned>(kSketchDimensionVisibilityCount)
        || visibility == dimensionVisibility_) {
        return;
    }
    dimensionVisibility_ = visibility;
    touchOverlay();
}

bool SketchSession::dimensionVisible(const SketchDimension& dimension) const {
    if (state_ != SketchSessionState::Editing) {
        return false;  // a drawing annotation; Ready has left the drawing
    }
    switch (dimensionVisibility_) {
        case SketchDimensionVisibility::Off:
            return false;
        case SketchDimensionVisibility::All:
            return true;
        case SketchDimensionVisibility::Selected:
            break;
    }
    if (entitySelected(dimension.first.entityId)
        || (dimension.kind == SketchDimensionKind::EdgeAngle
            && entitySelected(dimension.second.entityId))) {
        return true;
    }
    return dimensionTarget(nullptr) && dimensionFirst_.entityId == dimension.first.entityId;
}

std::vector<SketchDimensionAnnotation> SketchSession::visibleDimensionAnnotations(
        double worldPerUnit) const {
    std::vector<SketchDimensionAnnotation> out;
    for (const SketchDimension& dimension : sketch_.dimensions) {
        if (!dimensionVisible(dimension)) continue;
        SketchDimensionAnnotation annotation;
        if (buildSketchDimensionAnnotation(sketch_, dimension, worldPerUnit, &annotation)) {
            out.push_back(std::move(annotation));
        }
    }
    return out;
}

CadStatus SketchSession::trimAt(const SketchPoint& point, double toleranceMeters,
                                SketchTrimPlan* outPlan) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    SketchTrimPlan plan;
    const CadStatus why = planSketchTrim(sketch_, point, toleranceMeters, &plan);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    sketch_ = plan.result;
    lastTrimConvertedRectangle_ = plan.rectangleConverted;
    // A piece that kept the id stays selectable as it was; anything the trim
    // removed or replaced leaves the selection.
    selectedIds_.erase(std::remove_if(selectedIds_.begin(), selectedIds_.end(),
                                      [this](SketchEntityId id) {
                                          return findSketchEntity(sketch_, id) == nullptr;
                                      }),
                       selectedIds_.end());
    if (outPlan != nullptr) *outPlan = std::move(plan);
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::extendAt(const SketchPoint& point, double toleranceMeters,
                                  SketchExtendPlan* outPlan) {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    SketchExtendPlan plan;
    const CadStatus why = planSketchExtend(sketch_, point, toleranceMeters, &plan);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    sketch_ = plan.result;
    if (outPlan != nullptr) *outPlan = std::move(plan);
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setOffsetSource(SketchEntityId id) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Offset) {
        return fail(CadStatus::NotSketching);
    }
    const SketchEntity* entity = findSketchEntity(sketch_, id);
    if (entity == nullptr) {
        return fail(CadStatus::UnknownEntity);
    }
    if (!sketchEntityOffsettable(*entity)) {
        return fail(CadStatus::OffsetSplineUnsupported);
    }
    offsetSource_ = id;
    if (offsetDistance_ == 0.0) {
        offsetDistance_ = 4.0 * (gridStep_ > 0.0 ? gridStep_ : kSketchGridSpacingMeters);
    }
    selectedIds_.assign(1, id);
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setOffsetDistance(double signedMeters) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Offset) {
        return fail(CadStatus::NotSketching);
    }
    if (!std::isfinite(signedMeters) || std::fabs(signedMeters) > kMaxSketchCoordinateMeters) {
        return fail(CadStatus::OffsetInvalid);
    }
    offsetDistance_ = signedMeters;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::offsetPreviewStatus() const {
    if (modifyMode_ != SketchModifyMode::Offset || offsetSource_ == kNoSketchEntity) {
        return CadStatus::DraftingNoTarget;
    }
    const SketchEntity* entity = findSketchEntity(sketch_, offsetSource_);
    if (entity == nullptr) {
        return CadStatus::UnknownEntity;
    }
    std::vector<SketchEntity::Payload> pieces;
    return offsetSketchEntity(*entity, offsetDistance_, &pieces);
}

CadStatus SketchSession::confirmOffset(std::vector<SketchEntityId>* outIds) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Offset) {
        return fail(CadStatus::NotSketching);
    }
    if (offsetSource_ == kNoSketchEntity) {
        return fail(CadStatus::DraftingNoTarget);
    }
    std::vector<SketchEntityId> ids;
    const CadStatus why = applySketchOffset(&sketch_, offsetSource_, offsetDistance_, &ids);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    entitiesPlaced_ += static_cast<uint32_t>(ids.size());
    selectedIds_ = ids;
    if (outIds != nullptr) *outIds = std::move(ids);
    resetModifyState();
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setMirrorAxis(const CadSketchEdgeRef& axis) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Mirror) {
        return fail(CadStatus::NotSketching);
    }
    const CadStatus why = resolveSketchMirrorAxis(sketch_, axis, nullptr, nullptr, nullptr);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    mirrorAxis_ = axis;
    mirrorAxisSet_ = true;
    touchOverlay();
    return fail(CadStatus::Ok);
}

bool SketchSession::mirrorAxis(CadSketchEdgeRef* out) const {
    if (modifyMode_ != SketchModifyMode::Mirror || !mirrorAxisSet_) {
        return false;
    }
    if (out != nullptr) *out = mirrorAxis_;
    return true;
}

CadStatus SketchSession::confirmMirror(std::vector<SketchEntityId>* outIds) {
    if (state_ != SketchSessionState::Editing || modifyMode_ != SketchModifyMode::Mirror) {
        return fail(CadStatus::NotSketching);
    }
    if (!mirrorAxisSet_) {
        return fail(CadStatus::MirrorAxisInvalid);
    }
    std::vector<SketchEntityId> ids;
    const CadStatus why = applySketchMirror(&sketch_, selectedIds_, mirrorAxis_, &ids);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    entitiesPlaced_ += static_cast<uint32_t>(ids.size());
    selectedIds_ = ids;
    if (outIds != nullptr) *outIds = std::move(ids);
    resetModifyState();
    touchCandidate();
    return fail(CadStatus::Ok);
}

void SketchSession::updateModifyPreview(const SketchPoint& raw) {
    modifyPreview_.clear();
    const double tolerance = kSketchHitToleranceUnits * worldPerUnit_;
    if (modifyMode_ == SketchModifyMode::Trim) {
        SketchTrimPlan plan;
        if (planSketchTrim(sketch_, raw, tolerance, &plan) == CadStatus::Ok) {
            modifyPreview_ = std::move(plan.removed);
        }
    } else if (modifyMode_ == SketchModifyMode::Extend) {
        SketchExtendPlan plan;
        if (planSketchExtend(sketch_, raw, tolerance, &plan) == CadStatus::Ok) {
            modifyPreview_ = std::move(plan.added);
        }
    }
}

void SketchSession::applyModifyTap(const SketchPoint& raw) {
    const double tolerance = kSketchHitToleranceUnits * worldPerUnit_;
    switch (modifyMode_) {
        case SketchModifyMode::None:
            return;
        case SketchModifyMode::Trim:
            trimAt(raw, tolerance);
            return;
        case SketchModifyMode::Extend:
            extendAt(raw, tolerance);
            return;
        case SketchModifyMode::Dimension:
        case SketchModifyMode::Offset:
        case SketchModifyMode::Mirror:
            break;
    }
    const SketchEntityId hit = hitTest(raw, tolerance);
    if (hit == kNoSketchEntity) {
        fail(CadStatus::DraftingNoTarget);
        return;
    }
    CadSketchEdgeRef ref{hit, 0u};
    const bool straight = nearestSketchStraightEdge(sketch_, hit, raw, &ref);
    if (modifyMode_ == SketchModifyMode::Dimension) {
        if (dimensionAwaitSecond_) {
            if (!straight || sameCadSketchEdgeRef(ref, dimensionFirst_)) {
                fail(CadStatus::SketchDimensionInvalid);
                return;
            }
            SketchDimensionId id = kNoSketchDimension;
            const CadStatus why = addSketchDimension(&sketch_, SketchDimensionKind::EdgeAngle,
                                                     SketchDimensionMode::Reference, dimensionFirst_,
                                                     ref, &id);
            dimensionAwaitSecond_ = false;
            if (why == CadStatus::Ok) {
                selectedIds_.assign(1, dimensionFirst_.entityId);
                if (hit != dimensionFirst_.entityId) selectedIds_.push_back(hit);
                touchOverlay();
            }
            fail(why);
            return;
        }
        setDimensionTarget(ref);
        return;
    }
    if (modifyMode_ == SketchModifyMode::Offset) {
        setOffsetSource(hit);
        return;
    }
    // Mirror: the tap chooses the AXIS, a straight edge (Regular or
    // Construction); a curve is refused by name and the axis stands.
    if (!straight) {
        fail(CadStatus::MirrorAxisInvalid);
        return;
    }
    setMirrorAxis(ref);
}

bool SketchSession::onModifyTouch(TouchAction action, int32_t actionPointerId,
                                  const TouchPointer* pointers, int count,
                                  const CameraSnapshot& camera, int viewportWidth,
                                  int viewportHeight) {
    // A second pointer ends the claim exactly as it does for a drawing tool:
    // nothing is applied, and pan and pinch stay available.
    if (count > 1 || action == TouchAction::PointerDown) {
        resetGesture();
        modifyPreview_.clear();
        touchOverlay();
        return false;
    }
    switch (action) {
        case TouchAction::Down: {
            float perPixel = 0.0f;
            if (!worldMetersPerPixel(camera, frame_.origin, viewportHeight, &perPixel)) {
                return false;
            }
            worldPerUnit_ = static_cast<double>(perPixel) * gizmoPixelsPerReferenceUnit();
            gridStep_ = adaptiveSketchGridStep(static_cast<double>(perPixel));
            SketchPoint raw;
            if (!screenToSketch(camera, pointers[0].x, pointers[0].y, viewportWidth, viewportHeight,
                                &raw)) {
                return false;
            }
            pointerId_ = pointers[0].id;
            downX_ = pointers[0].x;
            downY_ = pointers[0].y;
            travelled_ = false;
            rawCursor_ = raw;
            cursor_ = raw;
            anchor_ = raw;
            updateModifyPreview(raw);
            touchOverlay();
            return true;
        }
        case TouchAction::Move: {
            if (pointerId_ < 0 || pointers[0].id != pointerId_) {
                return pointerId_ >= 0;
            }
            const float dx = pointers[0].x - downX_;
            const float dy = pointers[0].y - downY_;
            if (dx * dx + dy * dy >= kSketchTapSlopPixels * kSketchTapSlopPixels) {
                travelled_ = true;
            }
            SketchPoint raw;
            if (!screenToSketch(camera, pointers[0].x, pointers[0].y, viewportWidth, viewportHeight,
                                &raw)) {
                return true;
            }
            rawCursor_ = raw;
            cursor_ = raw;
            if (modifyMode_ == SketchModifyMode::Offset && travelled_
                && offsetSource_ != kNoSketchEntity) {
                // The offset FOLLOWS THE FINGER: its signed distance is the
                // finger's, in exact multiples of the grid step.
                const SketchEntity* source = findSketchEntity(sketch_, offsetSource_);
                double d = 0.0;
                if (source != nullptr && sketchOffsetDistanceAt(*source, raw, &d)) {
                    const double step = gridStep_ > 0.0 ? gridStep_ : kSketchGridSpacingMeters;
                    offsetDistance_ = std::round(d / step) * step;
                }
            } else {
                updateModifyPreview(raw);
            }
            touchOverlay();
            return true;
        }
        case TouchAction::Up:
        case TouchAction::PointerUp: {
            if (pointerId_ < 0) {
                return false;
            }
            if (actionPointerId != pointerId_ && count != 1) {
                resetGesture();
                return false;
            }
            const bool offsetDrag = modifyMode_ == SketchModifyMode::Offset && travelled_
                                    && offsetSource_ != kNoSketchEntity;
            // Trim and Extend apply what the preview showed under the finger;
            // the choosing modes take only a still tap.
            if (!offsetDrag
                && (!travelled_ || modifyMode_ == SketchModifyMode::Trim
                    || modifyMode_ == SketchModifyMode::Extend)) {
                applyModifyTap(rawCursor_);
            }
            pointerId_ = -1;
            travelled_ = false;
            dragValid_ = false;
            modifyPreview_.clear();
            touchOverlay();
            return true;
        }
        case TouchAction::Cancel: {
            const bool owned = pointerId_ >= 0;
            resetGesture();
            modifyPreview_.clear();
            touchOverlay();
            return owned;
        }
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// Finish and extrude
// ---------------------------------------------------------------------------

namespace {

// The atomic face that IS a loop region exactly (`CAD-V6-S2`): every boundary
// fragment of the face is a WHOLE source edge, the outer cycle's entities are
// exactly the region's outer loop members, and each hole likewise. Identity
// equivalence, never a nearest match: a region any crossing split names no
// face and answers false.
bool regionAsPlanarFace(const SketchRegionExtraction& regions, const SketchRegion& region,
                        const SketchArrangement& arrangement, size_t* outIndex) {
    const auto wholeMembers = [](const FragmentCycle& cycle, std::vector<SketchEntityId>* ids) {
        for (const FragmentRef& f : cycle) {
            if (f.startCut.kind != ArrangementCutKind::SourceStart
                || f.endCut.kind != ArrangementCutKind::SourceEnd) {
                return false;
            }
            ids->push_back(f.sourceEntityId);
        }
        std::sort(ids->begin(), ids->end());
        ids->erase(std::unique(ids->begin(), ids->end()), ids->end());
        return true;
    };
    const auto loopMembers = [&regions](uint32_t loop) {
        std::vector<SketchEntityId> ids = regions.loops.profiles[loop].memberEntityIds;
        std::sort(ids.begin(), ids.end());
        return ids;
    };
    std::vector<std::vector<SketchEntityId>> wantHoles;
    for (uint32_t h : region.holeLoops) wantHoles.push_back(loopMembers(h));
    std::sort(wantHoles.begin(), wantHoles.end());
    const std::vector<SketchEntityId> wantOuter = loopMembers(region.outerLoop);
    for (size_t i = 0; i < arrangement.faces.size(); ++i) {
        const PlanarFaceRef& ref = arrangement.faces[i].ref;
        std::vector<SketchEntityId> outer;
        if (!wholeMembers(ref.outer, &outer) || outer != wantOuter
            || ref.holes.size() != wantHoles.size()) {
            continue;
        }
        std::vector<std::vector<SketchEntityId>> holes;
        bool whole = true;
        for (const FragmentCycle& hole : ref.holes) {
            std::vector<SketchEntityId> ids;
            whole = whole && wholeMembers(hole, &ids);
            holes.push_back(std::move(ids));
        }
        std::sort(holes.begin(), holes.end());
        if (whole && holes == wantHoles) {
            *outIndex = i;
            return true;
        }
    }
    return false;
}

}  // namespace

CadStatus SketchSession::finish() {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    endPolylineInProgress();
    resetGesture();
    resetModifyState();
    const CadStatus sketchWhy = validateCadSketch(sketch_);
    if (sketchWhy != CadStatus::Ok) {
        return fail(sketchWhy);
    }
    SketchRegionExtraction extraction = extractSketchRegions(sketch_);
    // `CAD-V6-S2`: the arrangement decides whether this sketch's areas need
    // planar faces. It is derived every Finish from the authored entities and
    // stored nowhere.
    SketchArrangement arrangement = deriveSketchArrangement(sketch_);
    // ONE decision (`CAD-V6-S2-CORRECTION-FILL-HUD-R1`): a sketch whose loops
    // cross but whose arrangement cannot be derived is refused by the
    // arrangement's own name, never handed to the loop model to be read as
    // whole overlapping loops.
    const SketchSelectionModeDecision mode = decideSketchSelectionMode(arrangement, extraction);
    if (mode.status != CadStatus::Ok) {
        return fail(mode.status);
    }
    const bool planar = mode.kind == CadSelectionKind::PlanarFaces;
    if (!planar && extraction.loops.profiles.empty()) {
        // The FIRST rejection is the most useful thing to say: "your polyline
        // is open" beats "no closed profile".
        const CadStatus why = extraction.loops.rejections.empty()
                                      ? CadStatus::NoClosedProfile
                                      : extraction.loops.rejections.front().why;
        return fail(why);
    }
    regions_ = std::move(extraction);
    arrangement_ = std::move(arrangement);
    faceShapes_.clear();
    selectionLost_ = false;
    if (planar) {
        // One shape per atomic face -- its own one-face union -- for the tap,
        // the labels and the hatch. A face whose shape cannot be built (never
        // for a derived face) makes the whole sketch unselectable by name.
        for (size_t i = 0; i < arrangement_.faces.size(); ++i) {
            std::vector<PlanarProfileComponent> shape;
            const CadStatus why = mergePlanarFaceSelection(arrangement_, {i}, &shape);
            if (why != CadStatus::Ok || shape.size() != 1u) {
                regions_ = SketchRegionExtraction{};
                arrangement_ = SketchArrangement{};
                faceShapes_.clear();
                return fail(why == CadStatus::Ok ? CadStatus::PlanarFaceDegenerate : why);
            }
            faceShapes_.push_back(std::move(shape.front()));
        }
        reconcilePlanarSelection();
    } else {
        if (extrude_.selection == CadSelectionKind::PlanarFaces) {
            // A face-selected feature whose sketch no longer needs faces: its
            // stored faces are not re-read as regions (`CAD-V6-S2`, no
            // nearest rebind); the user chooses again.
            selectionLost_ = !extrude_.planarFaces.empty();
            clearPlanarSelection();
        }
        reconcileRegionSelection();
    }
    state_ = SketchSessionState::Ready;
    touchCandidate();
    return fail(CadStatus::Ok);
}

void SketchSession::reconcileRegionSelection() {
    // A selection the sketch still derives -- same outer loops, same holes --
    // survives a Back to Sketch and an Edit Sketch untouched.
    if (extrude_.profileEntityId != kNoSketchEntity
        && validateRegionSelection(regions_, extrudeRegions(extrude_)) == CadStatus::Ok) {
        return;
    }
    // Otherwise: exactly ONE region that can be selected is chosen for the
    // user, and with more than one NOTHING is chosen. A rectangle around a
    // circle offers the disk and the ring, and picking either on the user's
    // behalf is exactly the guess `CAD-VERTICAL-SLICE-R1` exists to remove.
    const SketchRegion* only = nullptr;
    uint32_t selectable = 0;
    for (const SketchRegion& region : regions_.regions) {
        if (region.status == CadStatus::Ok) {
            ++selectable;
            only = &region;
        }
    }
    if (selectable == 1u && regions_.regions.size() == 1u) {
        setExtrudeRegions(&extrude_, {sketchRegionRef(*only)});
    } else {
        setExtrudeRegions(&extrude_, {});
    }
}

void SketchSession::backToEditing() {
    if (state_ != SketchSessionState::Ready) {
        return;
    }
    regions_ = SketchRegionExtraction{};
    arrangement_ = SketchArrangement{};
    faceShapes_.clear();
    regionTapArmed_ = false;
    regionTapOnHead_ = false;
    cancelRevolveDrag();
    state_ = SketchSessionState::Editing;
    touchCandidate();
}

CadStatus SketchSession::selectProfile(SketchEntityId anchorEntityId) {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    const SketchRegion* region = findSketchRegion(regions_, anchorEntityId);
    if (region == nullptr) {
        return fail(CadStatus::ProfileNotFound);
    }
    if (!faceShapes_.empty()) {
        // Planar faces (`CAD-V6-S2`): a region is choosable only as the one
        // atomic face it exactly is; a region a crossing split names none.
        size_t index = 0;
        if (!regionAsPlanarFace(regions_, *region, arrangement_, &index)) {
            return fail(CadStatus::PlanarFaceUnresolved);
        }
        return selectPlanarFace(index);
    }
    if (region->status != CadStatus::Ok) {
        return fail(region->status);
    }
    setExtrudeRegions(&extrude_, {sketchRegionRef(*region)});
    touchCandidate();
    return fail(CadStatus::Ok);
}

bool SketchSession::regionSelected(SketchEntityId outerAnchorId) const {
    for (const ProfileRegionRef& ref : extrudeRegions(extrude_)) {
        if (ref.outerAnchorId == outerAnchorId) {
            return true;
        }
    }
    return false;
}

CadStatus SketchSession::toggleRegion(SketchEntityId outerAnchorId) {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    const SketchRegion* region = findSketchRegion(regions_, outerAnchorId);
    if (region == nullptr) {
        return fail(CadStatus::ProfileNotFound);
    }
    if (!faceShapes_.empty()) {
        size_t index = 0;
        if (!regionAsPlanarFace(regions_, *region, arrangement_, &index)) {
            return fail(CadStatus::PlanarFaceUnresolved);
        }
        return togglePlanarFace(index);
    }
    std::vector<ProfileRegionRef> current = extrudeRegions(extrude_);
    if (regionSelected(outerAnchorId)) {
        setExtrudeRegions(&extrude_, toggleRegionSelection(current, *region));
        touchCandidate();
        return fail(CadStatus::Ok);
    }
    if (region->status != CadStatus::Ok) {
        return fail(region->status);
    }
    // A PURE toggle (`CAD-FOUNDATION-C1`): the tapped region joins the
    // selection and no other region changes. A region beside its own hole is
    // legal -- the selection means their union -- so the only additions left
    // to refuse are the ones no rule can merge without guessing (loops that
    // touch or cross) and the bound; those are refused BY NAME and the
    // selection stands exactly as it was. Nothing is ever dropped silently.
    std::vector<ProfileRegionRef> next = toggleRegionSelection(current, *region);
    if (next.size() > kMaxProfileRegions) {
        return fail(CadStatus::TooManyRegions);
    }
    const CadStatus why = validateRegionSelection(regions_, next);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    setExtrudeRegions(&extrude_, std::move(next));
    touchCandidate();
    return fail(CadStatus::Ok);
}

bool SketchSession::toggleRegionAt(const CameraSnapshot& camera, float x, float y,
                                   int viewportWidth, int viewportHeight) {
    if (state_ != SketchSessionState::Ready) {
        recordTapOutcome(SketchTapOutcome::InvalidFace);
        return false;
    }
    SketchPoint point;
    if (!screenToSketch(camera, x, y, viewportWidth, viewportHeight, &point)) {
        // The existing ray-plane math found no point: the ray runs along the
        // plane. No band around edge-on is added here -- a finite hit at any
        // grazing angle is a hit.
        recordTapOutcome(SketchTapOutcome::RayParallel);
        return false;
    }
    const auto toggled = [this](CadStatus why) {
        recordTapOutcome(why == CadStatus::Ok             ? SketchTapOutcome::Resolved
                         : why == CadStatus::TooManyRegions ? SketchTapOutcome::SelectionCap
                                                            : SketchTapOutcome::InvalidFace);
        return why == CadStatus::Ok;
    };
    if (!faceShapes_.empty()) {
        // The atomic face under the finger: strictly inside its outer loop and
        // outside its holes. Faces do not overlap; the smallest wins a tie.
        size_t hit = faceShapes_.size();
        for (size_t i = 0; i < faceShapes_.size(); ++i) {
            const PlanarProfileComponent& shape = faceShapes_[i];
            bool inside = sketchPointStrictlyInside(point, shape.outer.polygon);
            for (const PlanarProfileLoop& hole : shape.holes) {
                inside = inside && !sketchPointStrictlyInside(point, hole.polygon);
            }
            if (inside && (hit == faceShapes_.size() || shape.area < faceShapes_[hit].area)) {
                hit = i;
            }
        }
        if (hit == faceShapes_.size()) {
            recordTapOutcome(SketchTapOutcome::Exterior);
            return false;
        }
        return toggled(togglePlanarFace(hit));
    }
    SketchEntityId anchor = kNoSketchEntity;
    if (!sketchRegionAt(regions_, point, &anchor)) {
        recordTapOutcome(SketchTapOutcome::Exterior);
        return false;
    }
    return toggled(toggleRegion(anchor));
}

// ---------------------------------------------------------------------------
// Planar faces (`CAD-V6-S2`)
// ---------------------------------------------------------------------------

bool SketchSession::selectionChosen() const {
    return extrude_.selection == CadSelectionKind::PlanarFaces
                   ? !extrude_.planarFaces.empty()
                   : extrude_.profileEntityId != kNoSketchEntity;
}

size_t SketchSession::selectedAreaCount() const {
    return extrude_.selection == CadSelectionKind::PlanarFaces ? extrude_.planarFaces.size()
                                                               : extrudeRegions(extrude_).size();
}

CadStatus SketchSession::unchosenStatus() const {
    if (selectionLost_) {
        return CadStatus::PlanarFaceUnresolved;
    }
    const size_t areas = faceShapes_.empty() ? regions_.regions.size() : faceShapes_.size();
    return areas > 1 ? CadStatus::AmbiguousProfile : CadStatus::ProfileNotFound;
}

void SketchSession::setPlanarSelection(std::vector<PlanarFaceRef> faces) {
    std::sort(faces.begin(), faces.end(), [](const PlanarFaceRef& a, const PlanarFaceRef& b) {
        return comparePlanarFaceRef(a, b) < 0;
    });
    extrude_.selection = CadSelectionKind::PlanarFaces;
    extrude_.planarFaces = std::move(faces);
    extrude_.profileEntityId = kNoSketchEntity;
    extrude_.profileHoleIds.clear();
    extrude_.additionalRegions.clear();
}

void SketchSession::clearPlanarSelection() {
    extrude_.selection = CadSelectionKind::LoopRegions;
    extrude_.planarFaces.clear();
}

std::vector<size_t> SketchSession::selectedPlanarFaceIndices() const {
    std::vector<size_t> indices;
    if (extrude_.selection != CadSelectionKind::PlanarFaces) {
        return indices;
    }
    for (const PlanarFaceRef& ref : extrude_.planarFaces) {
        size_t index = 0;
        if (resolvePlanarFaceRef(arrangement_, ref, &index)) {
            indices.push_back(index);
        }
    }
    return indices;
}

void SketchSession::reconcilePlanarSelection() {
    // A stored face selection whose every face still resolves EXACTLY
    // survives Back to Sketch and Edit Sketch untouched. Whether its union
    // extrudes is the candidate's question (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`):
    // a selection is never dropped here for what the preview would refuse.
    if (extrude_.selection == CadSelectionKind::PlanarFaces && !extrude_.planarFaces.empty()) {
        const std::vector<size_t> indices = selectedPlanarFaceIndices();
        if (indices.size() == extrude_.planarFaces.size()) {
            setPlanarSelection(extrude_.planarFaces);
            return;
        }
        // Lost: never re-bound to a nearest face. The user chooses again.
        selectionLost_ = true;
    } else if (extrude_.selection == CadSelectionKind::LoopRegions
               && extrude_.profileEntityId != kNoSketchEntity) {
        // A region selection over a sketch that now needs faces: kept only
        // when EVERY chosen region is exactly an atomic face (its whole loop,
        // unsplit) -- never re-read as a nearest face.
        std::vector<PlanarFaceRef> faces;
        bool exact = true;
        for (const ProfileRegionRef& ref : extrudeRegions(extrude_)) {
            const SketchRegion* region = findSketchRegion(regions_, ref.outerAnchorId);
            size_t index = 0;
            exact = exact && region != nullptr && region->holeAnchorIds == ref.holeAnchorIds
                    && regionAsPlanarFace(regions_, *region, arrangement_, &index);
            if (exact) faces.push_back(arrangement_.faces[index].ref);
        }
        if (exact && !faces.empty()) {
            setPlanarSelection(std::move(faces));
            return;
        }
        selectionLost_ = editingFeatureId_ != 0;
    }
    // Exactly ONE face is chosen for the user; with more, nothing is -- the
    // loop-region policy, unchanged.
    if (faceShapes_.size() == 1u) {
        setPlanarSelection({arrangement_.faces.front().ref});
        selectionLost_ = false;
    } else {
        setPlanarSelection({});
    }
}

bool SketchSession::planarFaceSelected(size_t index) const {
    if (index >= arrangement_.faces.size() || faceShapes_.empty()
        || extrude_.selection != CadSelectionKind::PlanarFaces) {
        return false;
    }
    // The selection is canonical (ascending), so membership is one binary
    // search: the chooser asks this once per face on every refresh.
    const PlanarFaceRef& ref = arrangement_.faces[index].ref;
    const auto at = std::lower_bound(extrude_.planarFaces.begin(), extrude_.planarFaces.end(), ref,
                                     [](const PlanarFaceRef& a, const PlanarFaceRef& b) {
                                         return comparePlanarFaceRef(a, b) < 0;
                                     });
    return at != extrude_.planarFaces.end() && samePlanarFaceRef(*at, ref);
}

bool SketchSession::planarFaceInfo(size_t index, SketchPoint* outInterior, double* outArea) const {
    if (index >= faceShapes_.size()) {
        return false;
    }
    const PlanarProfileComponent& shape = faceShapes_[index];
    if (outArea != nullptr) {
        *outArea = arrangement_.faces[index].area;
    }
    if (outInterior != nullptr) {
        std::vector<std::vector<SketchPoint>> loops{shape.outer.polygon};
        for (const PlanarProfileLoop& hole : shape.holes) loops.push_back(hole.polygon);
        if (!sketchLoopsInteriorPoint(loops, outInterior)) {
            return false;
        }
    }
    return true;
}

CadStatus SketchSession::togglePlanarFace(size_t index) {
    if (state_ != SketchSessionState::Ready || faceShapes_.empty()) {
        return fail(CadStatus::NotSketching);
    }
    if (index >= arrangement_.faces.size()) {
        return fail(CadStatus::ProfileNotFound);
    }
    const PlanarFaceRef& tapped = arrangement_.faces[index].ref;
    // The stored selection is kept in canonical (ascending) order by
    // `setPlanarSelection`, so the tapped face is found -- and a new one is
    // placed -- by one binary search rather than a scan and a re-sort: a tap
    // costs O(log n) comparisons however many faces are chosen.
    if (extrude_.selection != CadSelectionKind::PlanarFaces) {
        setPlanarSelection(std::vector<PlanarFaceRef>(extrude_.planarFaces));
    }
    std::vector<PlanarFaceRef>& faces = extrude_.planarFaces;
    const auto at = std::lower_bound(faces.begin(), faces.end(), tapped,
                                     [](const PlanarFaceRef& a, const PlanarFaceRef& b) {
                                         return comparePlanarFaceRef(a, b) < 0;
                                     });
    if (at != faces.end() && samePlanarFaceRef(*at, tapped)) {
        faces.erase(at);
    } else {
        // A PURE set toggle (`CAD-V6-S2-CORRECTION-FILL-PICK-R2`): the tapped
        // face joins and no other face changes. The selection is a SET of
        // exact face refs, so the only addition refused is the bound -- never
        // because of which OTHER faces are chosen. Whether the set extrudes
        // (a pinch, an Add that lands nowhere) is the candidate's question,
        // named on the preview, and is never answered by trimming the set.
        // The bound is the arrangement's own face bound
        // (`CAD-V6-S2-OWNER-FEEDBACK-MULTIFACE-E2E-R1`), so a selection of
        // distinct faces of a derived arrangement cannot reach it.
        if (faces.size() + 1u > kMaxPlanarFaceSelection) {
            return fail(CadStatus::TooManyRegions);
        }
        faces.insert(at, tapped);
    }
    selectionLost_ = false;
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::selectPlanarFace(size_t index) {
    if (state_ != SketchSessionState::Ready || faceShapes_.empty()) {
        return fail(CadStatus::NotSketching);
    }
    if (index >= arrangement_.faces.size()) {
        return fail(CadStatus::ProfileNotFound);
    }
    setPlanarSelection({arrangement_.faces[index].ref});
    selectionLost_ = false;
    touchCandidate();
    return fail(CadStatus::Ok);
}

bool SketchSession::planarSelectionAnchor(SketchPoint* out) const {
    std::vector<PlanarProfileComponent> components;
    if (mergePlanarFaceSelection(arrangement_, selectedPlanarFaceIndices(), &components)
                != CadStatus::Ok
        || components.empty()) {
        return false;
    }
    // The FIRST union component's area centroid when it stands on material,
    // otherwise a point that does -- the loop-region anchor's own rule.
    const PlanarProfileComponent& first = components.front();
    std::vector<std::vector<SketchPoint>> loops{first.outer.polygon};
    for (const PlanarProfileLoop& hole : first.holes) loops.push_back(hole.polygon);
    return sketchLoopsInteriorPoint(loops, out);
}

// THE one writer of the extrusion. Every typed value, every drag sample, every
// Flip and every extent change lands here, so all of them pass exactly the same
// validation and none of them can write a state the body would later refuse.
CadStatus SketchSession::applyExtrudeFeature(const ExtrudeFeature& requested) {
    if (!active()) {
        return fail(CadStatus::NotSketching);
    }
    if (!std::isfinite(requested.depth) || !std::isfinite(requested.secondDistance)) {
        return fail(CadStatus::NonFinite);
    }
    if (extrudeDirectionIndex(requested.direction) < 0
        || extrudeDirectionIndex(requested.direction) >= kExtrudeDirectionCount) {
        return fail(CadStatus::InvalidExtrudeDirection);
    }
    if (extrudeExtentModeIndex(requested.extent) < 0
        || extrudeExtentModeIndex(requested.extent) >= kExtrudeExtentModeCount
        || !extrudeFeatureCanonical(requested)) {
        return fail(CadStatus::InvalidExtrudeExtent);
    }
    // The DISTANCES, by the same rule `validateCadBodyState` applies, so a
    // value the session accepts is one the commit will accept too. A side may
    // be zero only in Two Sides, and the span always states a real length.
    const Meters positive = extrudePositiveDistance(requested);
    const Meters negative = extrudeNegativeDistance(requested);
    const bool sidesOk =
            positive >= 0.0 && negative >= 0.0 && positive <= kMaxSketchCoordinateMeters
            && negative <= kMaxSketchCoordinateMeters
            && (positive == 0.0 || validateDimensionMeters(positive) == DimensionValidation::Ok)
            && (negative == 0.0 || validateDimensionMeters(negative) == DimensionValidation::Ok)
            && (requested.extent == ExtrudeExtentMode::TwoSides
                || validateDimensionMeters(requested.depth) == DimensionValidation::Ok)
            && validateDimensionMeters(positive + negative) == DimensionValidation::Ok
            && positive + negative <= kMaxSketchCoordinateMeters;
    if (!sidesOk) {
        return fail(CadStatus::InvalidExtrudeDepth);
    }
    // The selection is chosen elsewhere, never here: only the extent moves --
    // whichever kind of selection it is.
    const std::vector<ProfileRegionRef> regions = extrudeRegions(extrude_);
    const CadSelectionKind kind = extrude_.selection;
    std::vector<PlanarFaceRef> faces = extrude_.planarFaces;
    extrude_ = requested;
    setExtrudeRegions(&extrude_, regions);
    extrude_.selection = kind;
    extrude_.planarFaces = std::move(faces);
    if (extrude_.extent == ExtrudeExtentMode::OneSide) {
        // The user's live One Side choice IS the transition memory; nothing
        // else writes it, so the two can never disagree.
        oneSideDirection_ = extrude_.direction;
    }
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setExtrude(Meters depth, ExtrudeDirection direction) {
    if (!active()) {
        return fail(CadStatus::NotSketching);
    }
    return applyExtrudeFeature(extrudeFeatureWithPrimary(extrude_, depth, direction));
}

CadStatus SketchSession::setExtrudeExtent(ExtrudeExtentMode extent) {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    if (extrudeExtentModeIndex(extent) < 0
        || extrudeExtentModeIndex(extent) >= kExtrudeExtentModeCount) {
        return fail(CadStatus::InvalidExtrudeExtent);
    }
    return applyExtrudeFeature(extrudeFeatureWithExtent(extrude_, extent, oneSideDirection_));
}

CadStatus SketchSession::setExtrudeSide(bool positiveSide, Meters distance) {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    // In One Side there is a handle on exactly one side, and a write to the
    // other is a refusal by name rather than a silent no-op.
    if (extrude_.extent == ExtrudeExtentMode::OneSide
        && positiveSide != (extrude_.direction == ExtrudeDirection::AlongNormal)) {
        return fail(CadStatus::InvalidExtrudeExtent);
    }
    return applyExtrudeFeature(extrudeFeatureWithSide(extrude_, positiveSide, distance));
}

// ---------------------------------------------------------------------------
// The canvas extrude manipulator (`CAD-UX-S1`)
// ---------------------------------------------------------------------------

bool SketchSession::selectionAnchorPoint(SketchPoint* out) const {
    if (extrude_.selection == CadSelectionKind::PlanarFaces) {
        return planarSelectionAnchor(out);
    }
    return extrudeSelectionAnchorPoint(regions_, extrude_, out);
}

bool SketchSession::extrudeAnchors(CadExtrudeAnchors* out) const {
    // A Revolve has no extrude arrow: the extrude HUD is hidden whole, on its
    // own "no anchor, no control" terms, rather than standing over a revolve.
    if (out == nullptr || state_ != SketchSessionState::Ready
        || featureKind_ == CadFeatureKind::Revolve) {
        return false;
    }
    SketchPoint base;
    if (!selectionAnchorPoint(&base)) {
        return false;
    }
    return cadExtrudeAnchorsAt(frame_, base, extrude_, out);
}

CadStatus SketchSession::flipExtrudeDirection() {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    // A One Side control alone. Symmetric already reaches both sides and Two
    // Sides states both explicitly, so there is no side left for a Flip to
    // choose; using it to swap A and B would be a second, hidden meaning for
    // one control.
    if (extrude_.extent != ExtrudeExtentMode::OneSide) {
        return fail(CadStatus::InvalidExtrudeExtent);
    }
    // Straight through the one writer, so the flip passes exactly the
    // validation a typed direction does and the overlay is touched once.
    return setExtrude(extrude_.depth,
                      extrude_.direction == ExtrudeDirection::AlongNormal
                              ? ExtrudeDirection::AgainstNormal
                              : ExtrudeDirection::AlongNormal);
}

RevolveFeature SketchSession::candidateRevolve() const {
    RevolveFeature revolve = revolve_;
    setRevolveSelectionFrom(&revolve, extrude_);
    return revolve;
}

CadBodyState SketchSession::candidateState() const {
    const bool revolve = featureKind_ == CadFeatureKind::Revolve;
    // A new body: the sketch and its extrusion, exactly what R0 built -- or,
    // since `CAD-V6-REVOLVE-NEWBODY-E2E-R1`, its Revolve.
    if (editingFeatureId_ == 0 && operation_ == CadFeatureOperation::NewBody) {
        return revolve ? makeCadRevolveBodyState(sketch_, candidateRevolve())
                       : makeCadBodyState(sketch_, extrude_);
    }
    // Everything else is the TARGET body's chain with one feature appended or
    // replaced -- the same SceneObject, never a copy of it.
    CadBodyState state = targetBaseState_;
    if (editingFeatureId_ == kCadFeatureId) {
        cadBaseSketch(state) = sketch_;
        if (revolve) {
            state.baseKind = CadFeatureKind::Revolve;
            state.extrude = ExtrudeFeature{};
            state.revolve = candidateRevolve();
        } else {
            state.extrude = extrude_;
        }
        return state;
    }
    if (editingFeatureId_ > kCadFeatureId) {
        // The feature's sketch is edited IN THE TABLE, where every feature
        // that extrudes it reads it; its placement stays the record's own. A
        // sketch on one of the body's faces is authored on its canonical XY
        // with no TopoRef, which is exactly what was staged from it.
        CadSketchRecord* record = cadFeatureSketchRecord(state, editingFeatureId_);
        if (record != nullptr) {
            record->sketch = sketch_;
            if (record->hasFeatureSupport) {
                record->sketch.plane = Workplane::XY;
                record->sketch.hasFaceSupport = false;
                record->sketch.faceSupport = TopoRef{};
            }
        }
        for (CadFeature& existing : state.laterFeatures) {
            if (existing.featureId == editingFeatureId_) {
                existing.operation = operation_;
                existing.extrude = extrude_;
            }
        }
        return state;
    }
    // A NEW Add or Cut: a new retained sketch on the face the session authored
    // against, and a new feature extruding it. The TopoRef the session used (a
    // face of this very body) becomes the record's own-feature support; carried
    // into the chain as a TopoRef it would be a cycle.
    CadSketch sketch = sketch_;
    sketch.plane = Workplane::XY;
    sketch.hasFaceSupport = false;
    sketch.faceSupport = TopoRef{};
    CadFeatureSupport support;
    support.featureId = sketch_.faceSupport.producerLocalFeatureId;
    support.face = sketch_.faceSupport.face;
    support.lineageToken = sketch_.faceSupport.lineageToken;
    if (appendCadLaterFeatureWithSketch(&state, operation_, support, std::move(sketch), extrude_)
        == 0) {
        // A full chain or table cannot take another feature. The candidate
        // still carries the attempt -- a feature naming no sketch -- so the
        // evaluation refuses it by name (`TooManyFeatures`/`SketchNotFound`)
        // rather than committing the unchanged target as if it had worked.
        CadFeature refused;
        refused.featureId = state.nextFeatureId;
        refused.operation = operation_;
        refused.extrude = extrude_;
        state.laterFeatures.push_back(std::move(refused));
    }
    return state;
}

bool SketchSession::operationAvailable(CadFeatureOperation operation) const {
    if (!active()) {
        return false;
    }
    // A Revolve is a New Body in R1: Add and Cut are not offered for one.
    if (featureKind_ == CadFeatureKind::Revolve) {
        return operation == CadFeatureOperation::NewBody;
    }
    const bool addOrCut = operation == CadFeatureOperation::Add
                          || operation == CadFeatureOperation::Cut;
    if (editingFeatureId_ == kCadFeatureId) {
        return operation == CadFeatureOperation::NewBody;
    }
    if (editingFeatureId_ > kCadFeatureId) {
        return addOrCut;
    }
    if (operation == CadFeatureOperation::NewBody) {
        return true;
    }
    // Add and Cut need a body to act on: the CAD body this sketch stands on,
    // whose chain is staged. A world-plane sketch has none.
    return addOrCut && hasTargetState_ && sketch_.hasFaceSupport
           && cadFeatureCount(targetBaseState_) < kMaxCadFeatures;
}

CadStatus SketchSession::setOperation(CadFeatureOperation operation) {
    if (!active()) {
        return fail(CadStatus::NotSketching);
    }
    const int index = cadFeatureOperationIndex(operation);
    if (index < 0 || index >= kCadFeatureOperationCount) {
        return fail(CadStatus::InvalidFeatureOperation);
    }
    if (!operationAvailable(operation)) {
        const bool addOrCut = operation != CadFeatureOperation::NewBody;
        return fail(addOrCut && editingFeatureId_ == 0 ? CadStatus::OperationNeedsTarget
                                                       : CadStatus::InvalidFeatureOperation);
    }
    if (operation_ != operation) {
        operation_ = operation;
        touchCandidate();
        // A NEW feature on a face starts facing the way its operation works:
        // every face frame's normal points OUT of the producer, so a Cut grows
        // INTO the body and an Add or a New Body grows out of it. Only for One
        // Side, where the direction is the one side there is; only for a new
        // feature, because an edit keeps the side the user already chose; and
        // through the one writer, so it is a direction change and never a
        // negative depth. Flip still reverses it afterwards.
        if (editingFeatureId_ == 0 && sketch_.hasFaceSupport
            && extrude_.extent == ExtrudeExtentMode::OneSide) {
            const ExtrudeDirection wanted = operation == CadFeatureOperation::Cut
                                                    ? ExtrudeDirection::AgainstNormal
                                                    : ExtrudeDirection::AlongNormal;
            if (extrude_.direction != wanted) {
                const CadStatus turned = setExtrude(extrude_.depth, wanted);
                if (turned != CadStatus::Ok) {
                    return turned;
                }
            }
        }
    }
    return fail(CadStatus::Ok);
}

ObjectId SketchSession::operationTargetId() const {
    if (editingFeatureId_ != 0) {
        return editingBodyId_;
    }
    return operation_ == CadFeatureOperation::NewBody ? kNoObject : targetBodyId_;
}

const CadCandidateEvaluation& SketchSession::evaluateCandidate() {
    if (evaluation_.valid && evaluation_.revision == candidateRevision_) {
        return evaluation_;
    }
    CadCandidateEvaluation next;
    next.revision = candidateRevision_;
    next.valid = true;
    next.operation = operation_;
    next.targetBodyId = operationTargetId();
    if (state_ != SketchSessionState::Ready) {
        next.status = CadStatus::NotSketching;
        evaluation_ = next;
        return evaluation_;
    }
    if (!selectionChosen()) {
        next.status = unchosenStatus();
        evaluation_ = next;
        return evaluation_;
    }
    const auto t0 = std::chrono::steady_clock::now();
    auto mesh = std::make_shared<CadBodyMesh>();
    CadRegenerationReport report;
    next.status = regenerateCadBody(candidateState(), mesh.get(), &report);
    next.failedFeatureId = report.failedFeatureId;
    next.kernelMicros = report.kernelMicros;
    next.micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0)
                          .count();
    if (next.status == CadStatus::Ok) {
        next.mesh = std::move(mesh);
    }
    evaluation_ = std::move(next);
    return evaluation_;
}

bool SketchSession::candidateWorldModel(ConstructionScene& scene, Mat4* out) const {
    if (out == nullptr || !active()) {
        return false;
    }
    const ObjectId target = operationTargetId();
    if (target != kNoObject) {
        return scene.resolveWorldModel(target, out);
    }
    if (sketch_.hasFaceSupport) {
        *out = mat4FromBasis(frame_.u, frame_.v, frame_.n, frame_.origin);
        return mat4Finite(*out);
    }
    *out = mat4Identity();
    return true;
}

CadStatus SketchSession::commit(ConstructionScene& scene, ConstructionHistory& history,
                                ObjectId* outId) {
    if (outId != nullptr) {
        *outId = kNoObject;
    }
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    // An edit session commits through `commitEdit`, into the body it edits;
    // letting `commit` run over one would create the second body an edit must
    // never create.
    if (editingBodyId_ != kNoObject) {
        return fail(CadStatus::NotSketching);
    }
    if (history.editInProgress()) {
        return fail(CadStatus::RefusedEditInProgress);
    }
    if (!selectionChosen()) {
        return fail(unchosenStatus());
    }
    if (operation_ != CadFeatureOperation::NewBody) {
        return commitIntoTarget(scene, history, outId);
    }
    const CadBodyState state = candidateState();
    CadStatus why = CadStatus::Ok;
    SceneObject* body = nullptr;
    {
        // ONE transaction. The scope owns the edit -- `editInProgress` was
        // false a moment ago -- and the commit compares scene state, so a
        // refused add records nothing at all.
        ScopedConstructionEdit edit(history);
        body = scene.addCadBody(state, &why);
        if (body != nullptr) {
            // Published inside the edit so the body is on screen the moment
            // it exists, exactly as an imported body is.
            publishSceneObject(*body);
        }
    }
    if (body == nullptr) {
        return fail(why == CadStatus::Ok ? CadStatus::RegenerationFailed : why);
    }
    if (outId != nullptr) {
        *outId = body->objectId();
    }
    cancel();  // the session is over; the truth is now the scene's
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::commitIntoTarget(ConstructionScene& scene, ConstructionHistory& history,
                                          ObjectId* outId) {
    SceneObject* object = scene.findBody(targetBodyId_);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr || !hasTargetState_) {
        return fail(CadStatus::OperationNeedsTarget);
    }
    // The staged target must still be the body's truth: the candidate was
    // built on it. Nothing can change a body while a sketch is open, so this is
    // a guard, not a behaviour.
    if (!sameCadBodyState(body->state(), targetBaseState_)) {
        return fail(CadStatus::FeatureSupportInvalid);
    }
    // THE preview's own result, so what was seen is what is committed: a
    // disjoint Add, a Cut that misses and a Cut that would remove everything
    // are refused here by those names and never become a new body.
    const CadCandidateEvaluation& evaluation = evaluateCandidate();
    if (evaluation.status != CadStatus::Ok || evaluation.mesh == nullptr) {
        return fail(evaluation.status == CadStatus::Ok ? CadStatus::RegenerationFailed
                                                       : evaluation.status);
    }
    const CadBodyState candidate = candidateState();
    const CadStatus dependents =
            checkDependentsKeepTheirFaces(scene, targetBodyId_, candidate, *evaluation.mesh);
    if (dependents != CadStatus::Ok) {
        return fail(dependents);
    }
    CadStatus why = CadStatus::Ok;
    {
        // ONE transaction into the SAME body: the id, the transform and the
        // Objects list do not move, and one Undo removes exactly this feature.
        ScopedConstructionEdit edit(history);
        why = body->applyState(candidate);
        if (why == CadStatus::Ok) {
            publishSceneObject(*object);
        }
    }
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    if (outId != nullptr) {
        *outId = targetBodyId_;
    }
    cancel();
    return fail(CadStatus::Ok);
}

// ---------------------------------------------------------------------------
// Revolve (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`)
// ---------------------------------------------------------------------------

bool SketchSession::revolveAvailable() const {
    if (state_ != SketchSessionState::Ready) {
        return false;
    }
    if (editingFeatureId_ == kCadFeatureId) {
        // An edit keeps the body's own kind: a Revolve body revolves.
        return hasTargetState_ && targetBaseState_.baseKind == CadFeatureKind::Revolve;
    }
    // A new body only; an edit of a later feature is an Add or a Cut.
    return editingFeatureId_ == 0;
}

CadStatus SketchSession::beginRevolve() {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    if (!revolveAvailable()) {
        return fail(CadStatus::InvalidFeatureOperation);
    }
    if (featureKind_ == CadFeatureKind::Revolve) {
        return fail(CadStatus::Ok);
    }
    restoreCancelledExtrudeDrag();
    regionTapArmed_ = false;
    regionTapOnHead_ = false;
    featureKind_ = CadFeatureKind::Revolve;
    // A Revolve makes a NEW body in R1, whatever the sketch stands on.
    operation_ = CadFeatureOperation::NewBody;
    // The axis is chosen next unless one is already held (Back to Sketch and
    // Finish again keep it, and so does an edit).
    axisPicking_ = revolve_.axis.entityId == kNoSketchEntity
                   || resolveRevolveAxis(sketch_, revolve_.axis, nullptr) != CadStatus::Ok;
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::endRevolve() {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    if (featureKind_ != CadFeatureKind::Revolve) {
        return fail(CadStatus::Ok);
    }
    if (editingFeatureId_ == kCadFeatureId) {
        return fail(CadStatus::InvalidFeatureKind);
    }
    cancelRevolveDrag();
    regionTapArmed_ = false;
    featureKind_ = CadFeatureKind::Extrude;
    axisPicking_ = false;
    touchCandidate();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::beginRevolveAxisPick() {
    if (state_ != SketchSessionState::Ready || featureKind_ != CadFeatureKind::Revolve) {
        return fail(CadStatus::NotSketching);
    }
    cancelRevolveDrag();
    axisPicking_ = true;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setRevolveAxis(const CadSketchEdgeRef& axis) {
    if (state_ != SketchSessionState::Ready || featureKind_ != CadFeatureKind::Revolve) {
        return fail(CadStatus::NotSketching);
    }
    // Resolved EXACTLY or refused by name; the held axis stands on a refusal.
    const CadStatus why = resolveRevolveAxis(sketch_, axis, nullptr);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    revolve_.axis = axis;
    axisPicking_ = false;
    touchCandidate();
    return fail(CadStatus::Ok);
}

bool SketchSession::pickRevolveAxisAt(const CameraSnapshot& camera, float x, float y,
                                      int viewportWidth, int viewportHeight) {
    if (!revolveAxisPicking()) {
        recordTapOutcome(SketchTapOutcome::InvalidFace);
        return false;
    }
    SketchPoint point;
    if (!screenToSketch(camera, x, y, viewportWidth, viewportHeight, &point)) {
        recordTapOutcome(SketchTapOutcome::RayParallel);
        return false;
    }
    float perPixel = 0.0f;
    if (!worldMetersPerPixel(camera, sketchToWorld(point), viewportHeight, &perPixel)) {
        recordTapOutcome(SketchTapOutcome::RayParallel);
        return false;
    }
    // The SAME hit tolerance the Select tool uses, so what a finger can pick
    // as an axis is what it can pick as an entity.
    const double tolerance =
            kSketchHitToleranceUnits * static_cast<double>(perPixel) * gizmoPixelsPerReferenceUnit();
    const SketchEntityId id = hitTest(point, tolerance);
    const SketchEntity* entity = id == kNoSketchEntity ? nullptr : findSketchEntity(sketch_, id);
    if (entity == nullptr) {
        recordTapOutcome(SketchTapOutcome::Exterior);
        return false;
    }
    const std::vector<SketchStraightEdge> edges = sketchEntityStraightEdges(*entity);
    if (edges.empty()) {
        // A curve is not an axis: said by name, nothing changes.
        fail(CadStatus::RevolveAxisNotStraight);
        recordTapOutcome(SketchTapOutcome::InvalidFace);
        return false;
    }
    const SketchStraightEdge* nearest = &edges.front();
    double best = segmentDistance(point, nearest->start, nearest->end);
    for (const SketchStraightEdge& edge : edges) {
        const double d = segmentDistance(point, edge.start, edge.end);
        if (d < best) {
            best = d;
            nearest = &edge;
        }
    }
    const CadStatus why = setRevolveAxis(CadSketchEdgeRef{id, nearest->edgeLocalIndex});
    recordTapOutcome(why == CadStatus::Ok ? SketchTapOutcome::Resolved
                                          : SketchTapOutcome::InvalidFace);
    return why == CadStatus::Ok;
}

CadStatus SketchSession::setRevolveAngle(double degrees) {
    if (state_ != SketchSessionState::Ready || featureKind_ != CadFeatureKind::Revolve) {
        return fail(CadStatus::NotSketching);
    }
    RevolveFeature requested = revolve_;
    requested.angleDegrees = degrees;
    const CadStatus why = validateRevolveParameters(requested);
    if (why != CadStatus::Ok) {
        return fail(why);
    }
    if (requested.angleDegrees != revolve_.angleDegrees) {
        revolve_.angleDegrees = requested.angleDegrees;
        touchCandidate();
    }
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::flipRevolveDirection() {
    if (state_ != SketchSessionState::Ready || featureKind_ != CadFeatureKind::Revolve) {
        return fail(CadStatus::NotSketching);
    }
    // A SENSE change: the exact angle is untouched.
    revolve_.direction = revolve_.direction == RevolveDirection::Positive
                                 ? RevolveDirection::Negative
                                 : RevolveDirection::Positive;
    touchCandidate();
    return fail(CadStatus::Ok);
}

bool SketchSession::revolveRing(RevolveRing* out) const {
    if (out == nullptr || state_ != SketchSessionState::Ready
        || featureKind_ != CadFeatureKind::Revolve) {
        return false;
    }
    RevolveAxis2D axis;
    if (resolveRevolveAxis(sketch_, revolve_.axis, &axis) != CadStatus::Ok) {
        return false;
    }
    // The selected area's extent along the axis and away from it, from the
    // SAME union loops the hatch draws; the ring stands at the area's axial
    // middle, on the side the area is on, a little outside its outermost point.
    std::vector<std::vector<SketchPoint>> loops;
    if (extrude_.selection == CadSelectionKind::PlanarFaces) {
        std::vector<PlanarProfileComponent> planar;
        if (mergePlanarFaceSelection(arrangement_, selectedPlanarFaceIndices(), &planar)
            == CadStatus::Ok) {
            for (const PlanarProfileComponent& component : planar) {
                loops.push_back(component.outer.polygon);
            }
        }
    } else {
        for (const SketchRegionComponent& component :
             mergeSelectedRegions(regions_, extrudeRegions(extrude_))) {
            const std::vector<std::vector<SketchPoint>> l = sketchComponentLoops(regions_, component);
            if (!l.empty()) loops.push_back(l.front());
        }
    }
    double tMin = 0.0;
    double tMax = axis.length;
    double reach = 0.0;
    double sideSum = 0.0;
    bool any = false;
    for (const std::vector<SketchPoint>& loop : loops) {
        for (const SketchPoint& p : loop) {
            const double t = (p.u - axis.start.u) * axis.du + (p.v - axis.start.v) * axis.dv;
            const double r = (p.u - axis.start.u) * -axis.dv + (p.v - axis.start.v) * axis.du;
            tMin = any ? std::min(tMin, t) : t;
            tMax = any ? std::max(tMax, t) : t;
            reach = std::max(reach, std::fabs(r));
            sideSum += r;
            any = true;
        }
    }
    if (!any || !(reach > 0.0)) {
        reach = std::max(axis.length * 0.5, 0.1);
    }
    const double side = sideSum < 0.0 ? -1.0 : 1.0;
    const double mid = (tMin + tMax) * 0.5;
    const double radius = reach * 1.18;
    const SketchPoint centre2{axis.start.u + axis.du * mid, axis.start.v + axis.dv * mid};
    const Vec3 axisWorld = vec3Normalize(vec3Add(vec3Scale(frame_.u, static_cast<float>(axis.du)),
                                                 vec3Scale(frame_.v, static_cast<float>(axis.dv))));
    const Vec3 radialWorld = vec3Normalize(
            vec3Add(vec3Scale(frame_.u, static_cast<float>(-axis.dv * side)),
                    vec3Scale(frame_.v, static_cast<float>(axis.du * side))));
    const float sense = revolve_.direction == RevolveDirection::Negative ? -1.0f : 1.0f;
    const Vec3 tangentWorld = vec3Scale(vec3Cross(axisWorld, radialWorld), sense);
    // The axis is drawn across the whole area it sweeps, a little beyond.
    const double over = std::max(0.15 * (tMax - tMin), 0.1);
    out->axisStart = sketchToWorld(
            SketchPoint{axis.start.u + axis.du * (tMin - over), axis.start.v + axis.dv * (tMin - over)});
    out->axisEnd = sketchToWorld(
            SketchPoint{axis.start.u + axis.du * (tMax + over), axis.start.v + axis.dv * (tMax + over)});
    out->centre = sketchToWorld(centre2);
    out->axis = axisWorld;
    out->radial = radialWorld;
    out->tangent = tangentWorld;
    out->radius = static_cast<float>(radius);
    const double a = revolve_.angleDegrees * 3.14159265358979323846 / 180.0;
    const auto onRing = [&](double theta, float scale) {
        return vec3Add(out->centre,
                       vec3Add(vec3Scale(radialWorld, out->radius * scale * static_cast<float>(std::cos(theta))),
                               vec3Scale(tangentWorld, out->radius * scale * static_cast<float>(std::sin(theta)))));
    };
    out->handle = onRing(a, 1.0f);
    out->label = onRing(a * 0.5, 1.22f);
    return vec3Finite(out->handle) && vec3Finite(out->centre);
}

bool SketchSession::revolveViewPose(const CameraController::Pose& current,
                                    CameraController::Pose* out) const {
    RevolveRing ring;
    if (out == nullptr || !revolveRing(&ring)) {
        return false;
    }
    const Vec3 n = vec3Normalize(frame_.n);
    // The ring plane's normal is the axis: a view needs a real component along
    // it to see the ring as an ellipse, and one along +n to keep the sketch
    // side towards the eye.
    constexpr float kAlongNormal = 0.55f;
    constexpr float kAlongAxis = 0.65f;
    constexpr float kAlongRadial = 0.52f;
    constexpr float kMinAxisDot = 0.35f;
    const float signs[4][2] = {{1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}};
    for (const auto& sign : signs) {
        const Vec3 candidate = vec3Normalize(
                vec3Add(vec3Scale(n, kAlongNormal),
                        vec3Add(vec3Scale(ring.axis, kAlongAxis * sign[0]),
                                vec3Scale(ring.radial, kAlongRadial * sign[1]))));
        float yaw = 0.0f;
        float pitch = 0.0f;
        if (!cadFeatureViewYawPitch(candidate, &yaw, &pitch)) {
            continue;
        }
        pitch = std::min(kPitchLimitRadians, std::max(-kPitchLimitRadians, pitch));
        const Vec3 installed = cadFeatureViewDirection(yaw, pitch);
        if (std::fabs(vec3Dot(installed, ring.axis)) < kMinAxisDot || vec3Dot(installed, n) <= 0.1f) {
            continue;
        }
        *out = current;
        out->target = ring.centre;
        out->yaw = yaw;
        out->pitch = pitch;
        out->orthoHalfHeightMeters = std::max(current.orthoHalfHeightMeters, ring.radius * 1.8f);
        out->distance = std::max(current.distance, ring.radius * 4.0f);
        return true;
    }
    return false;
}

bool SketchSession::revolvePointerAngle(const RevolveRing& ring, const CameraSnapshot& camera,
                                        float x, float y, int viewportWidth, int viewportHeight,
                                        double* out) const {
    Ray ray;
    if (!buildPickRay(camera, x, y, viewportWidth, viewportHeight, &ray)) {
        return false;
    }
    // A ring seen edge-on gives no angle: the last good value holds, exactly
    // as a degenerate viewpoint holds a gizmo drag.
    if (std::fabs(vec3Dot(ray.direction, ring.axis)) < 0.02f) {
        return false;
    }
    Vec3 hit;
    if (!intersectRayPlane(ray, ring.centre, ring.axis, &hit)) {
        return false;
    }
    const Vec3 w = vec3Sub(hit, ring.centre);
    const double a = std::atan2(static_cast<double>(vec3Dot(w, ring.tangent)),
                                static_cast<double>(vec3Dot(w, ring.radial)));
    if (!std::isfinite(a)) {
        return false;
    }
    *out = a;
    return true;
}

void SketchSession::cancelRevolveDrag() {
    if (revolveDragPointer_ < 0) {
        return;
    }
    // Put the angle back to what the finger found; nothing was recorded either
    // way -- the session is volatile -- so this is about what the user sees.
    if (revolveDragLive_ && revolve_.angleDegrees != revolveDragStartDegrees_) {
        revolve_.angleDegrees = revolveDragStartDegrees_;
        touchCandidate();
    }
    revolveDragPointer_ = -1;
    revolveDragLive_ = false;
    touchOverlay();
}

bool SketchSession::onRevolveTouch(TouchAction action, int32_t actionPointerId,
                                   const TouchPointer* pointers, int count,
                                   const CameraSnapshot& camera, int viewportWidth,
                                   int viewportHeight) {
    if (count > 1 || action == TouchAction::PointerDown) {
        regionTapArmed_ = false;
        cancelRevolveDrag();
        return false;  // two fingers pan and pinch
    }
    switch (action) {
        case TouchAction::Down: {
            if (count != 1 || pointers == nullptr) {
                return false;
            }
            // Armed as a TAP exactly as an Extrude Ready tap is: resolved on Up
            // at the Down pixel through the Down camera; while armed the camera
            // sees nothing, so jitter inside the slop orbits nothing.
            regionTapArmed_ = true;
            regionTapPointer_ = pointers[0].id;
            regionTapX_ = pointers[0].x;
            regionTapY_ = pointers[0].y;
            regionTapCamera_ = camera;
            regionTapOnHead_ = false;
            RevolveRing ring;
            if (axisPicking_ || !revolveRing(&ring)) {
                return false;
            }
            float hx = 0.0f;
            float hy = 0.0f;
            if (!projectWorldToScreen(camera, ring.handle, viewportWidth, viewportHeight, &hx, &hy)
                || std::hypot(hx - pointers[0].x, hy - pointers[0].y)
                           > kRevolveHandleGrabUnits * gizmoPixelsPerReferenceUnit()) {
                return false;  // off the handle: a tap, or (after the slop) navigation
            }
            // ON the handle: the gesture is the handle's from here on, and the
            // ring is FROZEN so the growing preview cannot move it.
            revolveDragPointer_ = pointers[0].id;
            revolveDragLive_ = false;
            revolveDragRing_ = ring;
            revolveDragStartDegrees_ = revolve_.angleDegrees;
            revolveDragDegrees_ = revolve_.angleDegrees;
            double a = 0.0;
            revolveDragPointerAngle_ =
                    revolvePointerAngle(ring, camera, pointers[0].x, pointers[0].y, viewportWidth,
                                        viewportHeight, &a)
                            ? a
                            : revolve_.angleDegrees * 3.14159265358979323846 / 180.0;
            regionTapOnHead_ = true;
            touchOverlay();
            return true;
        }
        case TouchAction::Move: {
            const bool ours = count == 1 && pointers != nullptr && pointers[0].id == regionTapPointer_;
            if (regionTapArmed_ && ours
                && std::hypot(pointers[0].x - regionTapX_, pointers[0].y - regionTapY_)
                           > kSketchTapSlopPixels) {
                regionTapArmed_ = false;
                if (revolveDragPointer_ < 0) {
                    recordTapOutcome(SketchTapOutcome::Travel);
                }
            }
            if (revolveDragPointer_ < 0 || count != 1 || pointers == nullptr
                || pointers[0].id != revolveDragPointer_) {
                return revolveDragPointer_ >= 0;
            }
            if (regionTapArmed_) {
                return true;  // still a possible tap: the angle holds
            }
            revolveDragLive_ = true;
            double a = 0.0;
            if (revolvePointerAngle(revolveDragRing_, camera, pointers[0].x, pointers[0].y,
                                    viewportWidth, viewportHeight, &a)) {
                // Unwrapped, so a drag can run the whole way round to 360.
                const double delta = std::remainder(a - revolveDragPointerAngle_,
                                                    2.0 * 3.14159265358979323846);
                revolveDragPointerAngle_ = a;
                revolveDragDegrees_ = std::min(
                        kMaxRevolveAngleDegrees,
                        std::max(kRevolveDragMinDegrees,
                                 revolveDragDegrees_ + delta * 180.0 / 3.14159265358979323846));
                // Whole degrees under the finger: a readable value at every
                // sample, and 360 reachable exactly. A typed value is exact.
                setRevolveAngle(std::min(kMaxRevolveAngleDegrees,
                                         std::max(kRevolveDragMinDegrees,
                                                  std::round(revolveDragDegrees_))));
            }
            return true;
        }
        case TouchAction::Up:
        case TouchAction::PointerUp: {
            if (revolveDragPointer_ < 0) {
                if (regionTapArmed_ && action == TouchAction::Up
                    && (actionPointerId < 0 || actionPointerId == regionTapPointer_)) {
                    const float x = count >= 1 && pointers != nullptr ? pointers[0].x : regionTapX_;
                    const float y = count >= 1 && pointers != nullptr ? pointers[0].y : regionTapY_;
                    if (std::hypot(x - regionTapX_, y - regionTapY_) <= kSketchTapSlopPixels) {
                        if (axisPicking_) {
                            pickRevolveAxisAt(regionTapCamera_, regionTapX_, regionTapY_,
                                              viewportWidth, viewportHeight);
                        } else {
                            toggleRegionAt(regionTapCamera_, regionTapX_, regionTapY_,
                                           viewportWidth, viewportHeight);
                        }
                    } else {
                        recordTapOutcome(SketchTapOutcome::Travel);
                    }
                }
                regionTapArmed_ = false;
                regionTapOnHead_ = false;
                return false;
            }
            if (actionPointerId >= 0 && actionPointerId != revolveDragPointer_) {
                regionTapArmed_ = false;
                cancelRevolveDrag();
                return false;
            }
            if (regionTapArmed_ && action == TouchAction::Up) {
                // A still tap on the handle: the manipulator's, changing nothing.
                recordTapOutcome(SketchTapOutcome::ArrowHead);
            }
            regionTapArmed_ = false;
            regionTapOnHead_ = false;
            revolveDragPointer_ = -1;
            revolveDragLive_ = false;
            touchOverlay();
            return true;
        }
        case TouchAction::Cancel: {
            const bool owned = revolveDragPointer_ >= 0;
            regionTapArmed_ = false;
            regionTapOnHead_ = false;
            cancelRevolveDrag();
            return owned;
        }
        default:
            return revolveDragPointer_ >= 0;
    }
}

// ---------------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------------

namespace {

void pushLine(std::vector<GizmoVertex>* out, const Vec3& a, const Vec3& b, float axis,
              float handle) {
    out->push_back(GizmoVertex{{a.x, a.y, a.z}, axis, handle});
    out->push_back(GizmoVertex{{b.x, b.y, b.z}, axis, handle});
}

float hueForWorldAxis(const Vec3& direction) {
    if (std::fabs(direction.x) > 0.5f) return 1.0f;
    if (std::fabs(direction.y) > 0.5f) return 2.0f;
    return 3.0f;
}

// The strokes an entity is drawn with, in sketch coordinates: one polyline per
// stroke, closed ones repeating their first point. The SAME derived geometry
// the hit test and the profile engine read.
std::vector<std::vector<SketchPoint>> entityStrokes(const SketchEntity& entity) {
    std::vector<std::vector<SketchPoint>> strokes;
    if (const SketchLine* line = entity.line()) {
        strokes.push_back({line->start, line->end});
    } else if (const SketchPolyline* polyline = entity.polyline()) {
        std::vector<SketchPoint> points = polyline->vertices;
        if (polyline->closed && points.size() >= 3) points.push_back(points.front());
        strokes.push_back(std::move(points));
    } else if (const SketchRectangle* rectangle = entity.rectangle()) {
        std::vector<SketchPoint> points = rectangleProfilePolygon(*rectangle);
        points.push_back(points.front());
        strokes.push_back(std::move(points));
    } else if (const SketchCircle* circle = entity.circle()) {
        std::vector<SketchPoint> points = circleProfilePolygon(*circle);
        points.push_back(points.front());
        strokes.push_back(std::move(points));
    } else {
        std::vector<SketchPoint> points;
        if (tessellateSketchCurve(entity, &points) == CadStatus::Ok) {
            strokes.push_back(std::move(points));
        }
    }
    return strokes;
}

// A polyline as DASHES (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`): `dash` drawn,
// `gap` skipped, the rhythm carried across vertices so a dashed circle reads
// as one dashed stroke. Bounded: a stroke that would need more than
// `kMaxDashes` dashes at this zoom is drawn with proportionally longer ones.
constexpr double kSketchDashUnits = 7.0;
constexpr double kSketchDashGapUnits = 5.0;
constexpr size_t kMaxDashesPerStroke = 400;

void appendDashed(std::vector<SketchPoint>* out, const std::vector<SketchPoint>& points,
                  double dash, double gap) {
    double total = 0.0;
    for (size_t i = 1; i < points.size(); ++i) total += distance(points[i - 1], points[i]);
    if (!(total > 0.0) || !(dash > 0.0) || !(gap >= 0.0)) return;
    const double period = dash + gap;
    if (total / period > static_cast<double>(kMaxDashesPerStroke)) {
        const double k = total / (period * static_cast<double>(kMaxDashesPerStroke));
        dash *= k;
        gap *= k;
    }
    bool drawing = true;
    double left = dash;
    for (size_t i = 1; i < points.size(); ++i) {
        SketchPoint a = points[i - 1];
        const SketchPoint& b = points[i];
        double segment = distance(a, b);
        while (segment > 0.0) {
            const double step = std::fmin(left, segment);
            const double t = step / segment;
            const SketchPoint c{a.u + (b.u - a.u) * t, a.v + (b.v - a.v) * t};
            if (drawing) {
                out->push_back(a);
                out->push_back(c);
            }
            segment -= step;
            left -= step;
            a = c;
            if (left <= 0.0) {
                drawing = !drawing;
                left = drawing ? dash : gap;
            }
        }
    }
}

// A snap marker in the shape of its KIND, half-size `h` (sketch metres), as
// point pairs: endpoint square, intersection X, midpoint triangle, centre
// circle, origin double square, grid plus. Guides draw a plus at the cursor;
// their dashed alignment line is drawn with the construction range.
std::vector<SketchPoint> snapMarker(SketchSnapKind kind, const SketchPoint& c, double h) {
    std::vector<SketchPoint> out;
    const auto seg = [&out](double u0, double v0, double u1, double v1) {
        out.push_back(SketchPoint{u0, v0});
        out.push_back(SketchPoint{u1, v1});
    };
    const auto square = [&](double r) {
        seg(c.u - r, c.v - r, c.u + r, c.v - r);
        seg(c.u + r, c.v - r, c.u + r, c.v + r);
        seg(c.u + r, c.v + r, c.u - r, c.v + r);
        seg(c.u - r, c.v + r, c.u - r, c.v - r);
    };
    switch (kind) {
        case SketchSnapKind::Endpoint:
            square(h * 0.8);
            break;
        case SketchSnapKind::Intersection:
            seg(c.u - h, c.v - h, c.u + h, c.v + h);
            seg(c.u - h, c.v + h, c.u + h, c.v - h);
            break;
        case SketchSnapKind::Midpoint:
            seg(c.u - h, c.v - h * 0.7, c.u + h, c.v - h * 0.7);
            seg(c.u + h, c.v - h * 0.7, c.u, c.v + h);
            seg(c.u, c.v + h, c.u - h, c.v - h * 0.7);
            break;
        case SketchSnapKind::Center: {
            const int n = 12;
            for (int i = 0; i < n; ++i) {
                const double a0 = 2.0 * 3.14159265358979323846 * i / n;
                const double a1 = 2.0 * 3.14159265358979323846 * (i + 1) / n;
                seg(c.u + h * 0.8 * std::cos(a0), c.v + h * 0.8 * std::sin(a0),
                    c.u + h * 0.8 * std::cos(a1), c.v + h * 0.8 * std::sin(a1));
            }
            break;
        }
        case SketchSnapKind::Origin:
            square(h);
            square(h * 0.45);
            break;
        case SketchSnapKind::HorizontalGuide:
        case SketchSnapKind::VerticalGuide:
        case SketchSnapKind::Grid:
        case SketchSnapKind::None:
            seg(c.u - h, c.v, c.u + h, c.v);
            seg(c.u, c.v - h, c.u, c.v + h);
            break;
    }
    return out;
}

}  // namespace

SketchOverlayPtr SketchSession::overlay(float worldPerUnit) {
    return overlay(worldPerUnit, CadExtrudeViewFacts{});
}

SketchOverlayPtr SketchSession::overlay(float worldPerUnit, const CadExtrudeViewFacts& view) {
    if (!active()) {
        if (overlay_ && !overlay_->vertices.empty()) {
            overlay_ = std::make_shared<SketchOverlay>();
            overlay_->revision = overlayRevision_;
        }
        if (!overlay_) {
            overlay_ = std::make_shared<SketchOverlay>();
        }
        return overlay_;
    }
    const bool cameraChanged = overlay_
                               && (overlayWorldPerUnit_ != worldPerUnit
                                   || !sameCadExtrudeViewFacts(overlayView_, view));
    if (overlayDirty_ || !overlay_ || cameraChanged) {
        // An authored change already advanced the revision (`touchOverlay`).
        // A rebuild the camera alone caused moves vertex POSITIONS -- the
        // snap marker, the hatch, the head, the dimension -- often at an
        // unchanged vertex count, so it must be a new revision too or the
        // renderer's upload gate would keep drawing the previous zoom.
        if (!overlayDirty_ && cameraChanged) {
            ++overlayRevision_;
        }
        buildOverlay(worldPerUnit, view);
    }
    return overlay_;
}

bool SketchSession::extrudeViewFacts(const CameraSnapshot& camera, int viewportWidth,
                                     int viewportHeight, CadExtrudeViewFacts* out) const {
    if (out == nullptr) {
        return false;
    }
    *out = CadExtrudeViewFacts{};
    CadExtrudeAnchors anchors;
    if (!extrudeAnchors(&anchors)) {
        return false;
    }
    CadExtrudeViewFacts built;
    if (!cadExtrudeManipulatorScale(anchors, camera, viewportHeight, &built.scale)) {
        return false;
    }
    built.leaderValid =
            cadExtrudeLeaderSide(anchors, camera, viewportWidth, viewportHeight, &built.leaderSide);
    built.valid = true;
    *out = built;
    return true;
}

bool SketchSession::extrudeDock(const CameraSnapshot& camera, int viewportWidth,
                                int viewportHeight, CadExtrudeDock* out) const {
    if (out == nullptr) {
        return false;
    }
    *out = CadExtrudeDock{};
    CadExtrudeAnchors anchors;
    CadExtrudeControlScale scale;
    if (!extrudeAnchors(&anchors)
        || !cadExtrudeManipulatorScale(anchors, camera, viewportHeight, &scale)) {
        return false;
    }
    return cadExtrudeDockFor(anchors, camera, scale, frame_.u, viewportWidth, viewportHeight,
                             out);
}

void SketchSession::buildOverlay(float worldPerUnit, const CadExtrudeViewFacts& view) {
    auto built = std::make_shared<SketchOverlay>();
    built->revision = overlayRevision_;
    std::vector<GizmoVertex>& v = built->vertices;
    // Map a sketch point onto the authoring frame in world space. For a
    // world-plane sketch this is the plane at the origin; for a face sketch the
    // producer's face frame.
    const auto local = [this](const SketchPoint& p) { return sketchToWorld(p); };
    const auto localAt = [this](const SketchPoint& p, double offset) {
        return sketchToWorldAt(p, offset);
    };

    // The adaptive minor step for the current zoom, and a grid drawn as a
    // bounded number of lines each side of the frame origin. worldPerUnit was
    // reference-unit metres; the per-pixel value is that over the dp scale.
    const double perPixel = worldPerUnit > 0.0f
                                ? static_cast<double>(worldPerUnit) / gizmoPixelsPerReferenceUnit()
                                : 0.0;
    const double step = adaptiveSketchGridStep(perPixel);
    const double extent = step * kSketchGridLinesPerSide;
    const int lines = kSketchGridLinesPerSide;

    // Grid, minor then major, as two ranges so they take two weights. In
    // Ready the drawing is done and the grid is withdrawn (`CAD-VERTICAL-
    // SLICE-R1`): the viewport answers one question at a time, and the
    // question now is the extrusion. The two ranges stay, empty, so every
    // range keeps its index.
    const bool drawGrid = state_ != SketchSessionState::Ready;
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = pass == 1;
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        for (int i = -lines; drawGrid && i <= lines; ++i) {
            const bool isMajor = (i % kSketchGridMajorEveryNMinor) == 0;
            if (isMajor != major || i == 0) {
                continue;  // the two axes are their own range
            }
            const double t = i * step;
            pushLine(&v, local(SketchPoint{t, -extent}), local(SketchPoint{t, extent}), 0.0f, 0.0f);
            pushLine(&v, local(SketchPoint{-extent, t}), local(SketchPoint{extent, t}), 0.0f, 0.0f);
        }
        range.vertexCount = static_cast<uint32_t>(v.size()) - range.firstVertex;
        range.style = major ? SketchOverlayStyle::GridMajor : SketchOverlayStyle::GridMinor;
        built->ranges.push_back(range);
    }
    {
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        pushLine(&v, local(SketchPoint{-extent, 0.0}), local(SketchPoint{extent, 0.0}),
                 hueForWorldAxis(frame_.u), 0.0f);
        pushLine(&v, local(SketchPoint{0.0, -extent}), local(SketchPoint{0.0, extent}),
                 hueForWorldAxis(frame_.v), 0.0f);
        range.vertexCount = static_cast<uint32_t>(v.size()) - range.firstVertex;
        range.style = SketchOverlayStyle::Axes;
        built->ranges.push_back(range);
    }

    // Entities. The selected one, the one being drawn and every loop of the
    // regions about to be extruded -- outer boundaries AND holes -- are
    // emphasised; everything else is neutral.
    SketchOverlayRange entities;
    entities.firstVertex = static_cast<uint32_t>(v.size());
    // What is drawn as chosen is the UNION the extrusion will make
    // (`CAD-FOUNDATION-C1`), so a hole the selection filled is hatched as
    // material and its loop is not emphasised as a boundary, while a hole the
    // selection left open stays empty and emphasised.
    std::vector<SketchRegionComponent> chosenComponents;
    // `CAD-V6-S2`: in PlanarFaces mode what is drawn as chosen is the union of
    // the chosen ATOMIC faces -- their own fragment loops, never a whole
    // source loop -- so the hatch covers exactly what will extrude.
    std::vector<std::vector<std::vector<SketchPoint>>> chosenLoops;
    if (state_ == SketchSessionState::Ready) {
        if (extrude_.selection == CadSelectionKind::PlanarFaces) {
            std::vector<PlanarProfileComponent> planar;
            if (mergePlanarFaceSelection(arrangement_, selectedPlanarFaceIndices(), &planar)
                == CadStatus::Ok) {
                for (const PlanarProfileComponent& component : planar) {
                    std::vector<std::vector<SketchPoint>> loops{component.outer.polygon};
                    for (const PlanarProfileLoop& hole : component.holes) {
                        loops.push_back(hole.polygon);
                    }
                    chosenLoops.push_back(std::move(loops));
                }
            }
        } else {
            chosenComponents = mergeSelectedRegions(regions_, extrudeRegions(extrude_));
            for (const SketchRegionComponent& component : chosenComponents) {
                chosenLoops.push_back(sketchComponentLoops(regions_, component));
            }
        }
    }
    std::vector<SketchEntityId> emphasisedMembers;
    for (const SketchRegionComponent& component : chosenComponents) {
        const std::vector<ClosedProfile>& loops = regions_.loops.profiles;
        std::vector<uint32_t> loopIndices = component.holeLoops;
        loopIndices.push_back(component.outerLoop);
        for (uint32_t l : loopIndices) {
            if (l < loops.size()) {
                emphasisedMembers.insert(emphasisedMembers.end(), loops[l].memberEntityIds.begin(),
                                         loops[l].memberEntityIds.end());
            }
        }
    }
    // What the extrusion does, as colour AND as the lines themselves: a Cut's
    // tool is drawn in the destructive (X) red, an Add's in the positive (Y)
    // green, a New Body's in the neutral highlight. Colour is never the only
    // carrier -- the HUD badge names the operation by shape and by label.
    const float opAxis = operation_ == CadFeatureOperation::Cut   ? 1.0f
                         : operation_ == CadFeatureOperation::Add ? 2.0f
                                                                  : 0.0f;
    const float opHandle = operation_ == CadFeatureOperation::NewBody ? 1.0f : 0.0f;
    for (const SketchEntity& entity : sketch_.entities) {
        if (entity.construction()) {
            continue;  // dashed, in the Construction range below
        }
        bool emphasised = entitySelected(entity.id());
        for (SketchEntityId member : emphasisedMembers) {
            if (member == entity.id()) emphasised = true;
        }
        const float handle = emphasised ? 1.0f : 0.0f;
        if (const SketchLine* line = entity.line()) {
            pushLine(&v, local(line->start), local(line->end), 0.0f, handle);
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            const size_t n = polyline->vertices.size();
            for (size_t i = 0; i + 1 < n; ++i) {
                pushLine(&v, local(polyline->vertices[i]), local(polyline->vertices[i + 1]), 0.0f,
                         handle);
            }
            if (polyline->closed && n >= 3) {
                pushLine(&v, local(polyline->vertices[n - 1]), local(polyline->vertices[0]), 0.0f,
                         handle);
            }
        } else if (const SketchRectangle* rectangle = entity.rectangle()) {
            const std::vector<SketchPoint> corners = rectangleProfilePolygon(*rectangle);
            for (size_t i = 0; i < 4; ++i) {
                pushLine(&v, local(corners[i]), local(corners[(i + 1) % 4]), 0.0f, handle);
            }
        } else if (const SketchCircle* circle = entity.circle()) {
            const std::vector<SketchPoint> ring = circleProfilePolygon(*circle);
            for (size_t i = 0; i < ring.size(); ++i) {
                pushLine(&v, local(ring[i]), local(ring[(i + 1) % ring.size()]), 0.0f, handle);
            }
        } else if (sketchEntityIsCurved(entity)) {
            // Drawn from the SAME derived polyline the profile engine and the
            // hit test use, so what is seen, what can be grabbed and what is
            // extruded are one shape rather than three approximations of one.
            std::vector<SketchPoint> points;
            if (tessellateSketchCurve(entity, &points) == CadStatus::Ok) {
                for (size_t i = 1; i < points.size(); ++i) {
                    pushLine(&v, local(points[i - 1]), local(points[i]), 0.0f, handle);
                }
            }
        }
    }
    // The polyline being placed, and the rubber band from its last vertex.
    if (polylineInProgress_) {
        for (size_t i = 0; i + 1 < polylineVertices_.size(); ++i) {
            pushLine(&v, local(polylineVertices_[i]), local(polylineVertices_[i + 1]), 0.0f, 1.0f);
        }
        if (pointerId_ >= 0 && !polylineVertices_.empty()) {
            pushLine(&v, local(polylineVertices_.back()), local(cursor_), 0.0f, 1.0f);
        }
    }
    // The Arc tool's pending chord, and the arc it would become through the
    // cursor. Shown between the two taps so the bulge is chosen by eye.
    if (arcPending_ && state_ == SketchSessionState::Editing) {
        pushLine(&v, local(arcStart_), local(arcEnd_), 0.0f, 0.0f);
        SketchArc preview;
        preview.start = arcStart_;
        preview.mid = cursor_;
        preview.end = arcEnd_;
        std::vector<SketchPoint> points;
        if (tessellateSketchCurve(SketchEntity(1, preview), &points) == CadStatus::Ok) {
            for (size_t i = 1; i < points.size(); ++i) {
                pushLine(&v, local(points[i - 1]), local(points[i]), 0.0f, 1.0f);
            }
        }
    }
    // The drag in progress.
    if (pointerId_ >= 0 && dragValid_ && state_ == SketchSessionState::Editing) {
        switch (tool_) {
            case SketchTool::Arc:
                // Before the chord is set, the drag rubber-bands it.
                if (!arcPending_) {
                    pushLine(&v, local(anchor_), local(cursor_), 0.0f, 1.0f);
                }
                break;
            case SketchTool::Line:
                pushLine(&v, local(anchor_), local(cursor_), 0.0f, 1.0f);
                break;
            case SketchTool::Rectangle: {
                const SketchPoint a = anchor_;
                const SketchPoint c = cursor_;
                const SketchPoint b{c.u, a.v};
                const SketchPoint d{a.u, c.v};
                pushLine(&v, local(a), local(b), 0.0f, 1.0f);
                pushLine(&v, local(b), local(c), 0.0f, 1.0f);
                pushLine(&v, local(c), local(d), 0.0f, 1.0f);
                pushLine(&v, local(d), local(a), 0.0f, 1.0f);
                break;
            }
            case SketchTool::Circle: {
                SketchCircle circle;
                circle.center = anchor_;
                circle.radius = distance(anchor_, cursor_);
                if (circle.radius > kSketchCoincidenceMeters) {
                    const std::vector<SketchPoint> ring = circleProfilePolygon(circle);
                    for (size_t i = 0; i < ring.size(); ++i) {
                        pushLine(&v, local(ring[i]), local(ring[(i + 1) % ring.size()]), 0.0f,
                                 1.0f);
                    }
                }
                break;
            }
            default:
                break;
        }
        // The snap marker: a shape that NAMES the snap's kind (square,
        // X, triangle, circle, double square, plus), sized in reference
        // units so it reads the same at any zoom. No text.
        const double half = kSketchSnapMarkerUnits * static_cast<double>(worldPerUnit);
        if (half > 0.0 && std::isfinite(half)) {
            const std::vector<SketchPoint> marker = snapMarker(lastSnapKind_, cursor_, half);
            for (size_t i = 0; i + 1 < marker.size(); i += 2) {
                pushLine(&v, local(marker[i]), local(marker[i + 1]), 0.0f, 1.0f);
            }
        }
    }
    // The modify previews (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`), in the
    // entity weight and emphasised: what Trim would REMOVE in the destructive
    // hue, what Extend would ADD in the positive one; the Offset and Mirror
    // results in the highlight, with the mirror axis in the third hue.
    if (state_ == SketchSessionState::Editing && modifyMode_ != SketchModifyMode::None) {
        const float previewHue = modifyMode_ == SketchModifyMode::Trim ? 1.0f : 2.0f;
        for (size_t i = 1; i < modifyPreview_.size(); ++i) {
            pushLine(&v, local(modifyPreview_[i - 1]), local(modifyPreview_[i]), previewHue, 1.0f);
        }
        std::vector<SketchEntity> previews;
        if (modifyMode_ == SketchModifyMode::Offset && offsetSource_ != kNoSketchEntity) {
            const SketchEntity* source = findSketchEntity(sketch_, offsetSource_);
            std::vector<SketchEntity::Payload> pieces;
            if (source != nullptr
                && offsetSketchEntity(*source, offsetDistance_, &pieces) == CadStatus::Ok) {
                for (SketchEntity::Payload& piece : pieces) previews.emplace_back(1u, piece);
            }
        }
        if (modifyMode_ == SketchModifyMode::Mirror && mirrorAxisSet_) {
            std::vector<SketchMirrorPiece> pieces;
            if (mirrorSketchEntities(sketch_, selectedIds_, mirrorAxis_, &pieces) == CadStatus::Ok) {
                for (SketchMirrorPiece& piece : pieces) previews.emplace_back(1u, piece.payload);
            }
            SketchPoint a;
            double du = 0.0;
            double dv = 0.0;
            if (resolveSketchMirrorAxis(sketch_, mirrorAxis_, &a, &du, &dv) == CadStatus::Ok) {
                const double reach = 4000.0 * static_cast<double>(worldPerUnit);
                pushLine(&v, local(SketchPoint{a.u - du * reach, a.v - dv * reach}),
                         local(SketchPoint{a.u + du * reach, a.v + dv * reach}), 3.0f, 1.0f);
            }
        }
        for (const SketchEntity& preview : previews) {
            for (const std::vector<SketchPoint>& stroke : entityStrokes(preview)) {
                for (size_t i = 1; i < stroke.size(); ++i) {
                    pushLine(&v, local(stroke[i - 1]), local(stroke[i]), 0.0f, 1.0f);
                }
            }
        }
    }
    // The union's hatch: lines clipped to each component by the even-odd
    // rule, so a hole left open reads as EMPTY rather than as selected
    // material and a filled one reads as material. A few
    // reference units apart at any zoom, bounded per component.
    if (!chosenLoops.empty() && worldPerUnit > 0.0f) {
        const double spacing = 14.0 * static_cast<double>(worldPerUnit);
        for (const std::vector<std::vector<SketchPoint>>& loops : chosenLoops) {
            const std::vector<SketchPoint> hatch = sketchLoopsHatch(loops, spacing);
            for (size_t i = 0; i + 1 < hatch.size(); i += 2) {
                pushLine(&v, local(hatch[i]), local(hatch[i + 1]), opAxis, opHandle);
            }
        }
    }
    // The Revolve manipulator (`CAD-V6-REVOLVE-NEWBODY-E2E-R1`): the axis drawn
    // across the area it sweeps, the ring arc from the profile to the current
    // angle in the plane square to the axis, its two spokes and the handle --
    // world geometry like the extrude arrow, in the same range, so the renderer
    // needs no new style. In axis-pick mode every straight edge that could be
    // an axis is drawn in the axis colour, so the choice is visible.
    if (state_ == SketchSessionState::Ready && featureKind_ == CadFeatureKind::Revolve) {
        if (axisPicking_) {
            for (const SketchEntity& entity : sketch_.entities) {
                for (const SketchStraightEdge& edge : sketchEntityStraightEdges(entity)) {
                    pushLine(&v, local(edge.start), local(edge.end), 3.0f, 0.0f);
                }
            }
        }
        RevolveRing ring;
        if (revolveRing(&ring)) {
            pushLine(&v, ring.axisStart, ring.axisEnd, 3.0f, 1.0f);
            const double angle = revolve_.angleDegrees * 3.14159265358979323846 / 180.0;
            const uint32_t steps = std::max<uint32_t>(
                    8u, static_cast<uint32_t>(std::ceil(64.0 * revolve_.angleDegrees / 360.0)));
            const auto onRing = [&ring](double theta) {
                return vec3Add(ring.centre,
                               vec3Add(vec3Scale(ring.radial, ring.radius * static_cast<float>(std::cos(theta))),
                                       vec3Scale(ring.tangent, ring.radius * static_cast<float>(std::sin(theta)))));
            };
            for (uint32_t k = 0; k < steps; ++k) {
                pushLine(&v, onRing(angle * k / steps), onRing(angle * (k + 1) / steps), 0.0f, 1.0f);
            }
            pushLine(&v, ring.centre, onRing(0.0), 0.0f, 1.0f);
            pushLine(&v, ring.centre, ring.handle, 0.0f, 1.0f);
            // The handle: a diamond a fixed number of reference units across,
            // in the ring's own plane, so it reads the same at every zoom.
            const float h = 9.0f * (worldPerUnit > 0.0f ? worldPerUnit : 0.01f)
                            * (revolveDragging() ? 1.4f : 1.0f);
            const Vec3 radial = vec3Normalize(vec3Sub(ring.handle, ring.centre));
            const Vec3 along = vec3Cross(ring.axis, radial);
            const Vec3 corners[4] = {vec3Add(ring.handle, vec3Scale(radial, h)),
                                     vec3Add(ring.handle, vec3Scale(along, h)),
                                     vec3Sub(ring.handle, vec3Scale(radial, h)),
                                     vec3Sub(ring.handle, vec3Scale(along, h))};
            for (int c = 0; c < 4; ++c) {
                pushLine(&v, corners[c], corners[(c + 1) % 4], 0.0f, 1.0f);
            }
        }
    }
    // The extrude preview: every union loop's far cap, near cap and edges --
    // holes included, so the preview of a ring shows its bore.
    if (!chosenLoops.empty() && featureKind_ != CadFeatureKind::Revolve) {
        // The SAME two offsets `generateCadMesh` extrudes between, so the
        // preview and the solid it previews cannot disagree about where the
        // caps are in any extent mode.
        const double nearOffset = -extrudeNegativeDistance(extrude_);
        const double farOffset = extrudePositiveDistance(extrude_);
        for (const std::vector<std::vector<SketchPoint>>& loops : chosenLoops) {
            for (const std::vector<SketchPoint>& loop : loops) {
                const size_t n = loop.size();
                for (size_t i = 0; i < n; ++i) {
                    const SketchPoint& a = loop[i];
                    const SketchPoint& b = loop[(i + 1) % n];
                    pushLine(&v, localAt(a, farOffset), localAt(b, farOffset), opAxis, opHandle);
                    pushLine(&v, localAt(a, nearOffset), localAt(a, farOffset), opAxis, opHandle);
                    if (nearOffset != 0.0) {
                        pushLine(&v, localAt(a, nearOffset), localAt(b, nearOffset), opAxis,
                                 opHandle);
                    }
                }
            }
        }
        // The canvas manipulator (`CAD-UX-S1`): one arrow along the extrusion
        // axis, in the SAME range and at the same weight as the preview it
        // belongs to, so the renderer needed no new style and no new case. Its
        // shaft is the depth; only its head and base tick take the camera-
        // attached control scale -- the frame's ONE manipulator scale fact,
        // read at the arrow's own base (`cadExtrudeManipulatorScale`), which
        // is exactly the number the hit test grabs with. No facts, no arrow.
        CadExtrudeAnchors anchors;
        if (view.valid && extrudeAnchors(&anchors)) {
            appendCadExtrudeArrow(&v, anchors, view.scale.world, extrudeDrag_.capturing(),
                                  extrudeDrag_.capturedPositiveSide());
        }
    }
    entities.vertexCount = static_cast<uint32_t>(v.size()) - entities.firstVertex;
    entities.style = SketchOverlayStyle::Entities;
    built->ranges.push_back(entities);

    // The technical-drawing dimension on a selected straight Line
    // (`SKETCH-UX-R1` E1). Its own range, so the renderer can draw it in the
    // annotation weight rather than as geometry -- which it is not.
    {
        SketchOverlayRange dimension;
        dimension.firstVertex = static_cast<uint32_t>(v.size());
        const SketchEntity* selected = !selectedLineLength(nullptr)
                                           ? nullptr
                                           : findSketchEntity(sketch_, selectedEntityId());
        const SketchLine* line = selected != nullptr ? selected->line() : nullptr;
        DimensionFrame f;
        const double unit = static_cast<double>(worldPerUnit);
        if (line != nullptr && unit > 0.0 && std::isfinite(unit) && dimensionFrameFor(*line, &f)) {
            const double offset = kSketchDimensionOffsetUnits * unit;
            const double gap = kSketchDimensionExtensionGapUnits * unit;
            const double overshoot = kSketchDimensionOvershootUnits * unit;
            const double tick = kSketchDimensionTickUnits * unit;
            // Extension lines: from just clear of each endpoint, past the
            // dimension line. They never touch the geometry, which is what
            // keeps the annotation legible over the stroke it measures.
            for (const SketchPoint& end : {f.start, f.end}) {
                pushLine(&v, local(offsetBy(end, f.pu, f.pv, gap)),
                         local(offsetBy(end, f.pu, f.pv, offset + overshoot)), 0.0f, 1.0f);
            }
            const SketchPoint a = offsetBy(f.start, f.pu, f.pv, offset);
            const SketchPoint b = offsetBy(f.end, f.pu, f.pv, offset);
            pushLine(&v, local(a), local(b), 0.0f, 1.0f);
            // The two end ticks: a slash at 45 degrees to the dimension line,
            // the draughting convention, drawn as direction + perpendicular.
            const double su = (f.du + f.pu) * 0.70710678118654752;
            const double sv = (f.dv + f.pv) * 0.70710678118654752;
            for (const SketchPoint& at : {a, b}) {
                pushLine(&v, local(offsetBy(at, su, sv, -tick)),
                         local(offsetBy(at, su, sv, tick)), 0.0f, 1.0f);
            }
        }
        // The extrusion's own technical-drawing leader (`CAD-FOUNDATION-C1`),
        // in the same annotation range and drawing language, sized by the
        // frame's one manipulator scale fact so it cannot disagree with the
        // head beside it.
        CadExtrudeAnchors leaderAnchors;
        CadExtrudeLeader leader;
        if (view.valid && view.leaderValid && extrudeAnchors(&leaderAnchors)
            && cadExtrudeLeaderFor(leaderAnchors, view.leaderSide, view.scale.world, &leader)) {
            appendCadExtrudeLeader(&v, leaderAnchors, leader, view.scale.world);
        }
        // The retained DRIVING dimensions (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`)
        // in this same annotation range; the reference ones get their own.
        if (worldPerUnit > 0.0f) {
            for (const SketchDimensionAnnotation& annotation :
                 visibleDimensionAnnotations(static_cast<double>(worldPerUnit))) {
                if (annotation.mode != SketchDimensionMode::Driving) continue;
                for (size_t i = 0; i + 1 < annotation.segments.size(); i += 2) {
                    pushLine(&v, local(annotation.segments[i]), local(annotation.segments[i + 1]),
                             0.0f, 1.0f);
                }
            }
        }
        dimension.vertexCount = static_cast<uint32_t>(v.size()) - dimension.firstVertex;
        dimension.style = SketchOverlayStyle::Dimension;
        built->ranges.push_back(dimension);
    }

    // Construction geometry, DASHED, and the transient inference guides
    // (`CAD-SKETCH-DRAFTING-TOOLKIT-E2E-R1`). Appended after every older range.
    {
        SketchOverlayRange construction;
        construction.firstVertex = static_cast<uint32_t>(v.size());
        const double unit = static_cast<double>(worldPerUnit);
        if (unit > 0.0 && std::isfinite(unit)) {
            const double dash = kSketchDashUnits * unit;
            const double gap = kSketchDashGapUnits * unit;
            for (const SketchEntity& entity : sketch_.entities) {
                if (!entity.construction()) continue;
                const float handle = entitySelected(entity.id()) ? 1.0f : 0.0f;
                for (const std::vector<SketchPoint>& stroke : entityStrokes(entity)) {
                    std::vector<SketchPoint> dashes;
                    appendDashed(&dashes, stroke, dash, gap);
                    for (size_t i = 0; i + 1 < dashes.size(); i += 2) {
                        pushLine(&v, local(dashes[i]), local(dashes[i + 1]), 0.0f, handle);
                    }
                }
            }
            // A guide: from the point it aligns with to the cursor, dashed,
            // while a drawing gesture holds it. It records nothing.
            if (pointerId_ >= 0 && state_ == SketchSessionState::Editing
                && modifyMode_ == SketchModifyMode::None) {
                for (int g = 0; g < 2; ++g) {
                    const bool on = g == 0 ? lastSnap_.horizontalGuide : lastSnap_.verticalGuide;
                    if (!on) continue;
                    const SketchPoint& from = g == 0 ? lastSnap_.horizontalSource
                                                     : lastSnap_.verticalSource;
                    std::vector<SketchPoint> dashes;
                    appendDashed(&dashes, {from, cursor_}, dash, gap);
                    for (size_t i = 0; i + 1 < dashes.size(); i += 2) {
                        pushLine(&v, local(dashes[i]), local(dashes[i + 1]), 3.0f, 1.0f);
                    }
                }
            }
        }
        construction.vertexCount = static_cast<uint32_t>(v.size()) - construction.firstVertex;
        construction.style = SketchOverlayStyle::Construction;
        built->ranges.push_back(construction);
    }
    // The retained REFERENCE dimensions, lighter than the driving ones.
    {
        SketchOverlayRange reference;
        reference.firstVertex = static_cast<uint32_t>(v.size());
        if (worldPerUnit > 0.0f) {
            for (const SketchDimensionAnnotation& annotation :
                 visibleDimensionAnnotations(static_cast<double>(worldPerUnit))) {
                if (annotation.mode != SketchDimensionMode::Reference) continue;
                for (size_t i = 0; i + 1 < annotation.segments.size(); i += 2) {
                    pushLine(&v, local(annotation.segments[i]), local(annotation.segments[i + 1]),
                             0.0f, 1.0f);
                }
            }
        }
        reference.vertexCount = static_cast<uint32_t>(v.size()) - reference.firstVertex;
        reference.style = SketchOverlayStyle::DimensionReference;
        built->ranges.push_back(reference);
    }

    if (v.size() > kMaxSketchOverlayVertices) {
        // Cannot happen within the sketch's own bounds (256 entities of at most
        // 256 vertices is far below the ceiling); refused rather than trusted.
        v.clear();
        built->ranges.clear();
    }
    overlay_ = built;
    overlayDirty_ = false;
    overlayWorldPerUnit_ = worldPerUnit;
    overlayView_ = view;
}

SketchSession& sketchSession() {
    static SketchSession session;
    return session;
}

}  // namespace forgeshape
