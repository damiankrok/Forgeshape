#include "forgeshape_sketch_session.h"

#include <cmath>

#include "forgeshape_cad_face.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_picking.h"

namespace forgeshape {

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

// Every point a new point may snap to: line ends, polyline vertices,
// rectangle corners and centres, circle centres.
void collectSnapPoints(const CadSketch& sketch, std::vector<SketchPoint>* out) {
    for (const SketchEntity& entity : sketch.entities) {
        if (const SketchLine* line = entity.line()) {
            out->push_back(line->start);
            out->push_back(line->end);
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            for (const SketchPoint& p : polyline->vertices) {
                out->push_back(p);
            }
        } else if (const SketchRectangle* rectangle = entity.rectangle()) {
            for (const SketchPoint& p : rectangleProfilePolygon(*rectangle)) {
                out->push_back(p);
            }
            out->push_back(rectangle->center);
        } else if (const SketchCircle* circle = entity.circle()) {
            out->push_back(circle->center);
        } else if (const SketchArc* arc = entity.arc()) {
            // The three AUTHORED points, not a tessellated sample: snapping to
            // a curve means snapping to something the user placed.
            out->push_back(arc->start);
            out->push_back(arc->mid);
            out->push_back(arc->end);
        } else if (const SketchSpline* spline = entity.spline()) {
            for (const SketchPoint& p : spline->points) {
                out->push_back(p);
            }
        }
    }
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

CadStatus SketchSession::beginOnFace(const SketchFrame& worldFrame, const TopoRef& support) {
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
    touchOverlay();
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
    selectedEntityId_ = kNoSketchEntity;
    profiles_ = ProfileExtraction{};
    extrude_ = ExtrudeFeature{};
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
    touchOverlay();
    return fail(CadStatus::Ok);
}

void SketchSession::cancel() {
    resetGesture();
    polylineInProgress_ = false;
    polylineVertices_.clear();
    arcPending_ = false;
    sketch_ = CadSketch{};
    selectedEntityId_ = kNoSketchEntity;
    profiles_ = ProfileExtraction{};
    extrude_ = ExtrudeFeature{};
    // An edit session that is cancelled has, by construction, written nothing
    // to the body: the staged copy simply goes away with the session.
    editingBodyId_ = kNoObject;
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
    const CadStatus started = begin(state.sketch.plane);
    if (started != CadStatus::Ok) {
        return started;
    }
    // The STAGED copy. Everything the body knows about itself, including its
    // face support and its extrusion, so Finish can regenerate the whole body
    // rather than a sketch with a guessed depth.
    sketch_ = state.sketch;
    extrude_ = state.extrude;
    frame_ = worldFrame;
    editingBodyId_ = bodyId;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::commitEdit(ConstructionScene& scene, ConstructionHistory& history) {
    if (state_ != SketchSessionState::Ready || editingBodyId_ == kNoObject) {
        return fail(CadStatus::NotSketching);
    }
    if (history.editInProgress()) {
        return fail(CadStatus::RefusedEditInProgress);
    }
    if (extrude_.profileEntityId == kNoSketchEntity) {
        return fail(profiles_.profiles.size() > 1 ? CadStatus::AmbiguousProfile
                                                  : CadStatus::ProfileNotFound);
    }
    SceneObject* object = scene.findBody(editingBodyId_);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        // The body went away under the edit -- deleted, or the project
        // replaced. The staged sketch is not applied to anything else.
        return fail(CadStatus::NotCadBody);
    }
    const CadBodyState candidate = candidateState();
    // Validated BEFORE the transaction opens, so a refusal never opens an edit
    // the history would then have to discard.
    const CadStatus valid = validateCadBodyState(candidate);
    if (valid != CadStatus::Ok) {
        return fail(valid);
    }
    // An edit that would strip a planar face another body's sketch is standing
    // on is REFUSED, checked against the CANDIDATE before anything is written.
    // A size-only edit keeps the face SET and so keeps every reference valid;
    // an edit that removes a profile edge does not, and the answer is a refusal
    // by name rather than a dependent that quietly stops being drawn.
    for (const ObjectId dependentId : scene.cadDependentsOf(editingBodyId_)) {
        const SceneObject* dependent = scene.findBody(dependentId);
        const CadBody* dependentCad = dependent != nullptr ? dependent->cadOrNull() : nullptr;
        if (dependentCad == nullptr || !dependentCad->sketch().hasFaceSupport) {
            continue;
        }
        const TopoRef& ref = dependentCad->sketch().faceSupport;
        CadFace face;
        if (cadTopologySignature(candidate) != ref.lineageToken
            || resolveCadFace(candidate, ref.face, &face) != CadStatus::Ok || !face.eligible) {
            return fail(CadStatus::DependentFaceLost);
        }
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
    if (!active() || selectedEntityId_ == kNoSketchEntity) {
        return false;
    }
    const SketchEntity* entity = findSketchEntity(sketch_, selectedEntityId_);
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
    if (!active() || selectedEntityId_ == kNoSketchEntity) {
        return false;
    }
    const SketchEntity* entity = findSketchEntity(sketch_, selectedEntityId_);
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
        return true;
    }
    // Changing tool ends a polyline being placed and drops a drag: a gesture
    // belongs to the tool it started under.
    endPolylineInProgress();
    resetGesture();
    // A pending arc chord belongs to the Arc tool; changing tool drops it, as
    // a drag in progress is dropped.
    arcPending_ = false;
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

SketchSession::SnapResult SketchSession::snap(const SketchPoint& raw, double snapWorld) const {
    SnapResult result;
    result.point = raw;
    result.kind = SketchSnapKind::None;

    // Endpoints first: an existing point is the more specific intent.
    std::vector<SketchPoint> candidates;
    collectSnapPoints(sketch_, &candidates);
    if (polylineInProgress_) {
        for (const SketchPoint& p : polylineVertices_) {
            candidates.push_back(p);
        }
    }
    double best = snapWorld;
    for (const SketchPoint& candidate : candidates) {
        const double d = distance(raw, candidate);
        if (d <= best) {
            best = d;
            result.point = candidate;
            result.kind = SketchSnapKind::Endpoint;
        }
    }
    if (result.kind == SketchSnapKind::Endpoint) {
        return result;
    }
    // Then the grid: exact multiples of the CURRENT adaptive step.
    const double step = gridStep_ > 0.0 ? gridStep_ : kSketchGridSpacingMeters;
    result.point.u = std::round(raw.u / step) * step;
    result.point.v = std::round(raw.v / step) * step;
    result.kind = SketchSnapKind::Grid;
    return result;
}

bool SketchSession::pointerToSketch(const CameraSnapshot& camera, float x, float y,
                                    int viewportWidth, int viewportHeight, SnapResult* out) {
    SketchPoint raw;
    if (!screenToSketch(camera, x, y, viewportWidth, viewportHeight, &raw)) {
        return false;
    }
    *out = snap(raw, kSketchSnapToleranceUnits * worldPerUnit_);
    lastSnapKind_ = out->kind;
    return true;
}

SketchEntityId SketchSession::hitTest(const SketchPoint& point, double toleranceWorld) const {
    SketchEntityId best = kNoSketchEntity;
    double bestDistance = toleranceWorld;
    for (const SketchEntity& entity : sketch_.entities) {
        double d = toleranceWorld * 2.0;
        if (const SketchLine* line = entity.line()) {
            d = segmentDistance(point, line->start, line->end);
        } else if (const SketchPolyline* polyline = entity.polyline()) {
            const size_t n = polyline->vertices.size();
            for (size_t i = 0; i + 1 < n; ++i) {
                d = std::fmin(d, segmentDistance(point, polyline->vertices[i],
                                                 polyline->vertices[i + 1]));
            }
            if (polyline->closed && n >= 3) {
                d = std::fmin(d, segmentDistance(point, polyline->vertices[n - 1],
                                                 polyline->vertices[0]));
            }
        } else if (const SketchRectangle* rectangle = entity.rectangle()) {
            const std::vector<SketchPoint> corners = rectangleProfilePolygon(*rectangle);
            for (size_t i = 0; i < 4; ++i) {
                d = std::fmin(d, segmentDistance(point, corners[i], corners[(i + 1) % 4]));
            }
        } else if (const SketchCircle* circle = entity.circle()) {
            d = std::fabs(distance(point, circle->center) - circle->radius);
        } else if (sketchEntityIsCurved(entity)) {
            // A curve is hit-tested against its DERIVED polyline: what the user
            // is aiming at is the stroke they can see, and the stroke is that
            // polyline. Bounded by the same tessellation the profile uses, so
            // hit-testing and extrusion agree about where the curve is.
            std::vector<SketchPoint> points;
            if (tessellateSketchCurve(entity, &points) == CadStatus::Ok && points.size() >= 2) {
                for (size_t i = 1; i < points.size(); ++i) {
                    d = std::fmin(d, segmentDistance(point, points[i - 1], points[i]));
                }
            }
        }
        // Ties keep the earlier entity: deterministic, and the same rule
        // scene picking uses.
        if (d < bestDistance) {
            bestDistance = d;
            best = entity.id();
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Touch
// ---------------------------------------------------------------------------

bool SketchSession::onTouch(TouchAction action, int32_t actionPointerId,
                            const TouchPointer* pointers, int count, const CameraSnapshot& camera,
                            int viewportWidth, int viewportHeight) {
    if (state_ != SketchSessionState::Editing || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
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
                        selectedEntityId_ =
                                hitTest(cursor_, kSketchHitToleranceUnits * worldPerUnit_);
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
        selectedEntityId_ = id;
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
        selectedEntityId_ = id;
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
            selectedEntityId_ = id;
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
            selectedEntityId_ = id;
        }
    }
    polylineVertices_.clear();
    touchOverlay();
}

// ---------------------------------------------------------------------------
// Editing
// ---------------------------------------------------------------------------

bool SketchSession::select(SketchEntityId id) {
    if (!active() || findSketchEntity(sketch_, id) == nullptr) {
        return false;
    }
    selectedEntityId_ = id;
    touchOverlay();
    return true;
}

void SketchSession::clearSelection() {
    selectedEntityId_ = kNoSketchEntity;
    touchOverlay();
}

CadStatus SketchSession::deleteSelected() {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    if (selectedEntityId_ == kNoSketchEntity) {
        return fail(CadStatus::UnknownEntity);
    }
    const CadStatus why = removeSketchEntity(&sketch_, selectedEntityId_);
    if (why == CadStatus::Ok) {
        selectedEntityId_ = kNoSketchEntity;
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
// Finish and extrude
// ---------------------------------------------------------------------------

CadStatus SketchSession::finish() {
    if (state_ != SketchSessionState::Editing) {
        return fail(CadStatus::NotSketching);
    }
    endPolylineInProgress();
    resetGesture();
    const CadStatus sketchWhy = validateCadSketch(sketch_);
    if (sketchWhy != CadStatus::Ok) {
        return fail(sketchWhy);
    }
    ProfileExtraction extraction = extractClosedProfiles(sketch_);
    if (extraction.profiles.empty()) {
        // The FIRST rejection is the most useful thing to say: "your polyline
        // is open" beats "no closed profile".
        const CadStatus why = extraction.rejections.empty() ? CadStatus::NoClosedProfile
                                                            : extraction.rejections.front().why;
        return fail(why);
    }
    profiles_ = std::move(extraction);
    if (profiles_.profiles.size() == 1) {
        extrude_.profileEntityId = profiles_.profiles.front().anchorEntityId;
    } else if (findClosedProfile(profiles_, extrude_.profileEntityId) == nullptr) {
        extrude_.profileEntityId = kNoSketchEntity;
    }
    state_ = SketchSessionState::Ready;
    touchOverlay();
    return fail(CadStatus::Ok);
}

void SketchSession::backToEditing() {
    if (state_ != SketchSessionState::Ready) {
        return;
    }
    profiles_ = ProfileExtraction{};
    state_ = SketchSessionState::Editing;
    touchOverlay();
}

CadStatus SketchSession::selectProfile(SketchEntityId anchorEntityId) {
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    if (findClosedProfile(profiles_, anchorEntityId) == nullptr) {
        return fail(CadStatus::ProfileNotFound);
    }
    extrude_.profileEntityId = anchorEntityId;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadStatus SketchSession::setExtrude(Meters depth, ExtrudeDirection direction) {
    if (!active()) {
        return fail(CadStatus::NotSketching);
    }
    if (!std::isfinite(depth)) {
        return fail(CadStatus::NonFinite);
    }
    if (validateDimensionMeters(depth) != DimensionValidation::Ok
        || depth > kMaxSketchCoordinateMeters) {
        return fail(CadStatus::InvalidExtrudeDepth);
    }
    if (extrudeDirectionIndex(direction) < 0
        || extrudeDirectionIndex(direction) >= kExtrudeDirectionCount) {
        return fail(CadStatus::InvalidExtrudeDirection);
    }
    extrude_.depth = depth;
    extrude_.direction = direction;
    touchOverlay();
    return fail(CadStatus::Ok);
}

CadBodyState SketchSession::candidateState() const {
    CadBodyState state;
    state.sketch = sketch_;
    state.extrude = extrude_;
    return state;
}

CadStatus SketchSession::commit(ConstructionScene& scene, ConstructionHistory& history,
                                ObjectId* outId) {
    if (outId != nullptr) {
        *outId = kNoObject;
    }
    if (state_ != SketchSessionState::Ready) {
        return fail(CadStatus::NotSketching);
    }
    if (history.editInProgress()) {
        return fail(CadStatus::RefusedEditInProgress);
    }
    if (extrude_.profileEntityId == kNoSketchEntity) {
        return fail(profiles_.profiles.size() > 1 ? CadStatus::AmbiguousProfile
                                                  : CadStatus::ProfileNotFound);
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

}  // namespace

SketchOverlayPtr SketchSession::overlay(float worldPerUnit) {
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
    if (overlayDirty_ || !overlay_ || overlayWorldPerUnit_ != worldPerUnit) {
        buildOverlay(worldPerUnit);
    }
    return overlay_;
}

void SketchSession::buildOverlay(float worldPerUnit) {
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

    // Grid, minor then major, as two ranges so they take two weights.
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = pass == 1;
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        for (int i = -lines; i <= lines; ++i) {
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

    // Entities. The selected one, the one being drawn and the profile about
    // to be extruded are emphasised; everything else is neutral.
    SketchOverlayRange entities;
    entities.firstVertex = static_cast<uint32_t>(v.size());
    const ClosedProfile* chosen = (state_ == SketchSessionState::Ready)
                                      ? findClosedProfile(profiles_, extrude_.profileEntityId)
                                      : nullptr;
    for (const SketchEntity& entity : sketch_.entities) {
        bool emphasised = entity.id() == selectedEntityId_;
        if (chosen != nullptr) {
            for (SketchEntityId member : chosen->memberEntityIds) {
                if (member == entity.id()) emphasised = true;
            }
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
        // The snap marker: a small cross at the snapped cursor, sized in
        // reference units so it reads the same at any zoom.
        const double half = kSketchSnapMarkerUnits * static_cast<double>(worldPerUnit);
        if (half > 0.0 && std::isfinite(half)) {
            pushLine(&v, local(SketchPoint{cursor_.u - half, cursor_.v}),
                     local(SketchPoint{cursor_.u + half, cursor_.v}), 0.0f, 1.0f);
            pushLine(&v, local(SketchPoint{cursor_.u, cursor_.v - half}),
                     local(SketchPoint{cursor_.u, cursor_.v + half}), 0.0f, 1.0f);
        }
    }
    // The extrude preview: the chosen profile's far cap and its edges.
    if (chosen != nullptr) {
        const double nearOffset = (extrude_.direction == ExtrudeDirection::AlongNormal)
                                      ? 0.0
                                      : -extrude_.depth;
        const double farOffset = nearOffset + extrude_.depth;
        const size_t n = chosen->polygon.size();
        for (size_t i = 0; i < n; ++i) {
            const SketchPoint& a = chosen->polygon[i];
            const SketchPoint& b = chosen->polygon[(i + 1) % n];
            pushLine(&v, localAt(a, farOffset), localAt(b, farOffset), 0.0f, 1.0f);
            pushLine(&v, localAt(a, nearOffset), localAt(a, farOffset), 0.0f, 1.0f);
            if (nearOffset != 0.0) {
                pushLine(&v, localAt(a, nearOffset), localAt(b, nearOffset), 0.0f, 1.0f);
            }
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
        const SketchEntity* selected = selectedEntityId_ == kNoSketchEntity
                                           ? nullptr
                                           : findSketchEntity(sketch_, selectedEntityId_);
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
        dimension.vertexCount = static_cast<uint32_t>(v.size()) - dimension.firstVertex;
        dimension.style = SketchOverlayStyle::Dimension;
        built->ranges.push_back(dimension);
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
}

SketchSession& sketchSession() {
    static SketchSession session;
    return session;
}

}  // namespace forgeshape
