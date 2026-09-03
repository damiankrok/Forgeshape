#include "forgeshape_sketch_session.h"

#include <cmath>

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
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

CadStatus SketchSession::begin(Workplane plane) {
    if (state_ != SketchSessionState::Inactive) {
        return fail(CadStatus::NotSketching);
    }
    if (workplaneIndex(plane) < 0 || workplaneIndex(plane) >= kWorkplaneCount) {
        return fail(CadStatus::InvalidWorkplane);
    }
    sketch_ = CadSketch{};
    sketch_.plane = plane;
    selectedEntityId_ = kNoSketchEntity;
    profiles_ = ProfileExtraction{};
    extrude_ = ExtrudeFeature{};
    tool_ = SketchTool::Rectangle;
    resetGesture();
    polylineInProgress_ = false;
    polylineVertices_.clear();
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
    sketch_ = CadSketch{};
    selectedEntityId_ = kNoSketchEntity;
    profiles_ = ProfileExtraction{};
    extrude_ = ExtrudeFeature{};
    state_ = SketchSessionState::Inactive;
    touchOverlay();
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
    const WorkplaneFrame frame = workplaneFrame(sketch_.plane);
    Vec3 hit;
    // A sketch body starts at the identity placement, so the plane's local
    // frame IS its world frame and the origin is the world origin.
    if (!intersectRayPlane(ray, Vec3{0.0f, 0.0f, 0.0f}, frame.normal, &hit)) {
        return false;
    }
    *out = localToWorkplane(sketch_.plane, hit);
    return std::isfinite(out->u) && std::isfinite(out->v);
}

bool SketchSession::sketchToScreen(const CameraSnapshot& camera, const SketchPoint& point,
                                   int viewportWidth, int viewportHeight, float* outX,
                                   float* outY) const {
    return projectWorldToScreen(camera, workplaneToLocal(sketch_.plane, point), viewportWidth,
                                viewportHeight, outX, outY);
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
    // Then the grid: exact multiples of the spacing.
    result.point.u = std::round(raw.u / kSketchGridSpacingMeters) * kSketchGridSpacingMeters;
    result.point.v = std::round(raw.v / kSketchGridSpacingMeters) * kSketchGridSpacingMeters;
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
            const Vec3 origin{0.0f, 0.0f, 0.0f};
            if (!worldMetersPerPixel(camera, origin, viewportHeight, &perPixel)) {
                return false;
            }
            worldPerUnit_ = static_cast<double>(perPixel) * gizmoPixelsPerReferenceUnit();
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
                    if (tap) {
                        placePolylineVertex(at);
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
    if (polylineVertices_.size() >= 3 && nearlySame(at.point, polylineVertices_.front())) {
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
    if (polylineVertices_.size() >= kMaxPolylineVertices) {
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
        SketchPolyline polyline;
        polyline.vertices = polylineVertices_;
        polyline.closed = false;
        SketchEntityId id = kNoSketchEntity;
        lastStatus_ = addSketchEntity(&sketch_, std::move(polyline), &id);
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
    const Workplane plane = sketch_.plane;
    const WorkplaneFrame frame = workplaneFrame(plane);
    const auto local = [plane](const SketchPoint& p) { return workplaneToLocal(plane, p); };
    const auto localAt = [plane](const SketchPoint& p, double offset) {
        return workplaneToLocalAtOffset(plane, p, offset);
    };

    // Grid, minor then major, as two ranges so they take two weights.
    const int lines = static_cast<int>(std::round(kSketchGridHalfExtentMeters
                                                  / kSketchGridSpacingMeters));
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = pass == 1;
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        for (int i = -lines; i <= lines; ++i) {
            const bool isMajor = (i % kSketchGridMajorEveryNMinor) == 0;
            if (isMajor != major || i == 0) {
                continue;  // the two axes are their own range
            }
            const double t = i * kSketchGridSpacingMeters;
            pushLine(&v, local(SketchPoint{t, -kSketchGridHalfExtentMeters}),
                     local(SketchPoint{t, kSketchGridHalfExtentMeters}), 0.0f, 0.0f);
            pushLine(&v, local(SketchPoint{-kSketchGridHalfExtentMeters, t}),
                     local(SketchPoint{kSketchGridHalfExtentMeters, t}), 0.0f, 0.0f);
        }
        range.vertexCount = static_cast<uint32_t>(v.size()) - range.firstVertex;
        range.style = major ? SketchOverlayStyle::GridMajor : SketchOverlayStyle::GridMinor;
        built->ranges.push_back(range);
    }
    {
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        pushLine(&v, local(SketchPoint{-kSketchGridHalfExtentMeters, 0.0}),
                 local(SketchPoint{kSketchGridHalfExtentMeters, 0.0}),
                 hueForWorldAxis(frame.uAxis), 0.0f);
        pushLine(&v, local(SketchPoint{0.0, -kSketchGridHalfExtentMeters}),
                 local(SketchPoint{0.0, kSketchGridHalfExtentMeters}),
                 hueForWorldAxis(frame.vAxis), 0.0f);
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
    // The drag in progress.
    if (pointerId_ >= 0 && dragValid_ && state_ == SketchSessionState::Editing) {
        switch (tool_) {
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
