#include "forgeshape_freeform_session.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "forgeshape_picking.h"

namespace forgeshape {

namespace {

Vec3 worldOf(const Mat4& model, const DVec3& local) {
    return mat4TransformPoint(model, vec3FromDVec3(local));
}

float pointSegmentDistanceSq(float px, float py, float ax, float ay, float bx, float by,
                             float* outT) {
    const float dx = bx - ax;
    const float dy = by - ay;
    const float lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 0.0f) {
        t = ((px - ax) * dx + (py - ay) * dy) / lengthSq;
        t = std::max(0.0f, std::min(1.0f, t));
    }
    if (outT != nullptr) *outT = t;
    const float cx = ax + dx * t - px;
    const float cy = ay + dy * t - py;
    return cx * cx + cy * cy;
}

// Depth along the view axis, so "nearer the eye" means the same thing in both
// projections.
float viewDepth(const CameraSnapshot& camera, const Vec3& world) {
    Vec3 forward = vec3Sub(camera.target, camera.eye);
    const float length = std::sqrt(vec3Dot(forward, forward));
    if (!(length > 0.0f)) return 0.0f;
    forward = vec3Scale(forward, 1.0f / length);
    return vec3Dot(vec3Sub(world, camera.eye), forward);
}

// A 3x3 rotation about local axis `a` by `radians`, row-major, in double.
void rotationAbout(int a, double radians, double out[9]) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    for (int i = 0; i < 9; ++i) out[i] = (i % 4 == 0) ? 1.0 : 0.0;
    const int u = (a + 1) % 3;
    const int v = (a + 2) % 3;
    out[u * 3 + u] = c;
    out[u * 3 + v] = -s;
    out[v * 3 + u] = s;
    out[v * 3 + v] = c;
}

}  // namespace

// ---------------------------------------------------------------------------
// Picking
// ---------------------------------------------------------------------------

bool pickFreeformElement(const FreeformCage& cage, const FreeformMesh* derived, const Mat4& model,
                         const CameraSnapshot& camera, float screenX, float screenY,
                         int viewportWidth, int viewportHeight, FreeformElement element,
                         float radiusPixels, uint32_t* outId) {
    if (!std::isfinite(screenX) || !std::isfinite(screenY) || viewportWidth <= 0
        || viewportHeight <= 0 || !(radiusPixels > 0.0f)) {
        return false;
    }
    const float radiusSq = radiusPixels * radiusPixels;
    uint32_t best = 0;
    float bestDistanceSq = std::numeric_limits<float>::max();
    float bestDepth = std::numeric_limits<float>::max();
    // Nearer the finger wins; between two at the same distance (a vertex and
    // the one behind it, projected together) the one nearer the eye wins.
    auto consider = [&](uint32_t id, float distanceSq, float depth) {
        if (distanceSq > radiusSq) return;
        const bool closer = distanceSq + 1.0f < bestDistanceSq;
        const bool tie = std::fabs(distanceSq - bestDistanceSq) <= 1.0f && depth < bestDepth;
        if (best == 0 || closer || tie) {
            best = id;
            bestDistanceSq = distanceSq;
            bestDepth = depth;
        }
    };
    if (element == FreeformElement::Vertex) {
        for (const FreeformVertex& v : cage.vertices) {
            const Vec3 world = worldOf(model, v.position);
            float px = 0.0f, py = 0.0f;
            if (!projectWorldToScreen(camera, world, viewportWidth, viewportHeight, &px, &py)) continue;
            const float dx = px - screenX;
            const float dy = py - screenY;
            consider(idOf(v.id), dx * dx + dy * dy, viewDepth(camera, world));
        }
    } else if (element == FreeformElement::Edge) {
        for (const FreeformEdge& e : cage.edges) {
            const FreeformVertex* a = findFreeformVertex(cage, e.v0);
            const FreeformVertex* b = findFreeformVertex(cage, e.v1);
            if (a == nullptr || b == nullptr) continue;
            const Vec3 wa = worldOf(model, a->position);
            const Vec3 wb = worldOf(model, b->position);
            float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
            if (!projectWorldToScreen(camera, wa, viewportWidth, viewportHeight, &ax, &ay)
                || !projectWorldToScreen(camera, wb, viewportWidth, viewportHeight, &bx, &by)) {
                continue;
            }
            float t = 0.0f;
            const float d = pointSegmentDistanceSq(screenX, screenY, ax, ay, bx, by, &t);
            const Vec3 at = vec3Add(wa, vec3Scale(vec3Sub(wb, wa), t));
            consider(idOf(e.id), d, viewDepth(camera, at));
        }
    } else {
        // A face is picked where the user SEES it: through the derived surface,
        // whose every triangle carries the control face it came from. The cage
        // quads are the fallback (level 0 draws exactly them anyway).
        Ray ray{};
        if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) return false;
        Mat4 inverse;
        if (!mat4AffineInverse(model, &inverse)) return false;
        Ray local{};
        local.origin = mat4TransformPoint(inverse, ray.origin);
        local.direction = mat4TransformDirection(inverse, ray.direction);
        float nearest = std::numeric_limits<float>::max();
        uint32_t face = 0;
        if (derived != nullptr) {
            const std::vector<MeshVertex>& vertices = derived->render.vertices;
            const std::vector<uint32_t>& indices = derived->render.indices;
            for (size_t t = 0; t + 2 < indices.size(); t += 3) {
                const float* p0 = vertices[indices[t]].position;
                const float* p1 = vertices[indices[t + 1]].position;
                const float* p2 = vertices[indices[t + 2]].position;
                float hitT = 0.0f;
                if (intersectRayTriangle(local, Vec3{p0[0], p0[1], p0[2]}, Vec3{p1[0], p1[1], p1[2]},
                                         Vec3{p2[0], p2[1], p2[2]}, false, &hitT)
                    && hitT < nearest) {
                    nearest = hitT;
                    face = idOf(derived->triangleFace[t / 3]);
                }
            }
        }
        if (face == 0) {
            for (const FreeformFace& f : cage.faces) {
                Vec3 q[4];
                bool ok = true;
                for (int k = 0; k < 4; ++k) {
                    const FreeformVertex* v = findFreeformVertex(cage, f.loop[k]);
                    if (v == nullptr) ok = false;
                    else q[k] = vec3FromDVec3(v->position);
                }
                if (!ok) continue;
                float hitT = 0.0f;
                if ((intersectRayTriangle(local, q[0], q[1], q[2], false, &hitT)
                     || intersectRayTriangle(local, q[0], q[2], q[3], false, &hitT))
                    && hitT < nearest) {
                    nearest = hitT;
                    face = idOf(f.id);
                }
            }
        }
        if (face == 0) return false;
        if (outId != nullptr) *outId = face;
        return true;
    }
    if (best == 0) return false;
    if (outId != nullptr) *outId = best;
    return true;
}

// ---------------------------------------------------------------------------
// The overlay
// ---------------------------------------------------------------------------

SketchOverlayPtr buildFreeformCageOverlay(const FreeformCage& cage, const Mat4& model,
                                          FreeformElement element,
                                          const std::vector<uint32_t>& selection,
                                          float worldPerUnit, uint64_t revision) {
    auto built = std::make_shared<SketchOverlay>();
    built->revision = revision;
    if (!(worldPerUnit > 0.0f) || !std::isfinite(worldPerUnit)) return built;
    // The selection's mirror images move with it, so they are shown with it.
    const std::vector<uint32_t> shown = freeformSymmetricSelection(cage, element, selection);
    auto selected = [&shown](uint32_t id) { return std::binary_search(shown.begin(), shown.end(), id); };
    std::vector<GizmoVertex> neutral;
    std::vector<GizmoVertex> highlighted;
    auto line = [](std::vector<GizmoVertex>* out, const Vec3& a, const Vec3& b, float handle) {
        out->push_back(GizmoVertex{{a.x, a.y, a.z}, 0.0f, handle});
        out->push_back(GizmoVertex{{b.x, b.y, b.z}, 0.0f, handle});
    };
    // Edges of selected faces and selected edges are highlighted.
    std::vector<uint8_t> edgeLit(cage.edges.size(), 0u);
    if (element == FreeformElement::Edge) {
        for (size_t e = 0; e < cage.edges.size(); ++e) edgeLit[e] = selected(idOf(cage.edges[e].id)) ? 1u : 0u;
    } else if (element == FreeformElement::Face) {
        for (const FreeformFace& f : cage.faces) {
            if (!selected(idOf(f.id))) continue;
            for (int k = 0; k < 4; ++k) {
                const FreeformEdge* e = findFreeformEdgeBetween(cage, f.loop[k], f.loop[(k + 1) % 4]);
                if (e == nullptr) continue;
                const auto it = std::lower_bound(cage.edges.begin(), cage.edges.end(), e->id,
                                                 [](const FreeformEdge& x, FreeformEdgeId id) {
                                                     return idOf(x.id) < idOf(id);
                                                 });
                edgeLit[static_cast<size_t>(it - cage.edges.begin())] = 1u;
            }
        }
    }
    for (size_t e = 0; e < cage.edges.size(); ++e) {
        const FreeformVertex* a = findFreeformVertex(cage, cage.edges[e].v0);
        const FreeformVertex* b = findFreeformVertex(cage, cage.edges[e].v1);
        if (a == nullptr || b == nullptr) continue;
        line(edgeLit[e] ? &highlighted : &neutral, worldOf(model, a->position), worldOf(model, b->position),
             edgeLit[e] ? 1.0f : 0.0f);
    }
    // A selected face also carries its two diagonals, so a face selection
    // reads as an AREA rather than as four selected edges.
    if (element == FreeformElement::Face) {
        for (const FreeformFace& f : cage.faces) {
            if (!selected(idOf(f.id))) continue;
            Vec3 q[4];
            for (int k = 0; k < 4; ++k) q[k] = worldOf(model, findFreeformVertex(cage, f.loop[k])->position);
            line(&highlighted, q[0], q[2], 1.0f);
            line(&highlighted, q[1], q[3], 1.0f);
        }
    }
    // Vertices as small crosses, in Vertex mode only: elsewhere they are
    // clutter the finger cannot act on.
    if (element == FreeformElement::Vertex) {
        const float arm = 5.0f * worldPerUnit;
        for (const FreeformVertex& v : cage.vertices) {
            const Vec3 c = worldOf(model, v.position);
            const bool lit = selected(idOf(v.id));
            const float size = lit ? arm * 1.6f : arm;
            for (int axis = 0; axis < 3; ++axis) {
                Vec3 d{0.0f, 0.0f, 0.0f};
                (axis == 0 ? d.x : axis == 1 ? d.y : d.z) = size;
                line(lit ? &highlighted : &neutral, vec3Sub(c, d), vec3Add(c, d), lit ? 1.0f : 0.0f);
            }
        }
    }
    if (neutral.size() + highlighted.size() > kMaxSketchOverlayVertices) return built;
    built->vertices = neutral;
    SketchOverlayRange quiet;
    quiet.firstVertex = 0;
    quiet.vertexCount = static_cast<uint32_t>(neutral.size());
    quiet.style = SketchOverlayStyle::Construction;
    built->ranges.push_back(quiet);
    SketchOverlayRange lit;
    lit.firstVertex = static_cast<uint32_t>(built->vertices.size());
    lit.vertexCount = static_cast<uint32_t>(highlighted.size());
    lit.style = SketchOverlayStyle::Entities;
    built->vertices.insert(built->vertices.end(), highlighted.begin(), highlighted.end());
    built->ranges.push_back(lit);
    return built;
}

// ---------------------------------------------------------------------------
// The session
// ---------------------------------------------------------------------------

SceneObject* FreeformEditSession::body() const {
    if (!active_) return nullptr;
    SceneObject* b = scene_.findBody(bodyId_);
    return b != nullptr && b->freeformOrNull() != nullptr ? b : nullptr;
}

bool FreeformEditSession::modelMatrix(Mat4* out) const {
    const SceneObject* b = body();
    if (b == nullptr) return false;
    return scene_.resolveWorldModel(bodyId_, out);
}

FreeformStatus FreeformEditSession::begin(ObjectId id) {
    if (capturing_) cancelDrag();
    SceneObject* b = scene_.findBody(id);
    if (b == nullptr || b->freeformOrNull() == nullptr) return lastStatus_ = FreeformStatus::NotFreeformBody;
    if (!b->visible()) return lastStatus_ = FreeformStatus::BodyHidden;
    if (b->locked()) return lastStatus_ = FreeformStatus::BodyLocked;
    active_ = true;
    bodyId_ = id;
    element_ = FreeformElement::Face;
    multiSelect_ = false;
    mode_ = GizmoMode::Move;
    selection_.clear();
    tapArmed_ = false;
    return lastStatus_ = FreeformStatus::Ok;
}

void FreeformEditSession::end() {
    if (capturing_) cancelDrag();
    active_ = false;
    bodyId_ = kNoObject;
    selection_.clear();
    tapArmed_ = false;
    overlay_.reset();
    overlayCage_.reset();
}

void FreeformEditSession::reconcile() {
    if (!active_) return;
    const SceneObject* b = scene_.findBody(bodyId_);
    if (b == nullptr || b->freeformOrNull() == nullptr || !b->visible() || b->locked()
        || scene_.activeBodyId() != bodyId_) {
        end();
        return;
    }
    if (!capturing_) pruneSelection();
}

void FreeformEditSession::pruneSelection() {
    const SceneObject* b = body();
    if (b == nullptr) {
        selection_.clear();
        return;
    }
    const FreeformCage& cage = b->freeformOrNull()->cage();
    selection_.erase(std::remove_if(selection_.begin(), selection_.end(),
                                    [&](uint32_t id) {
                                        switch (element_) {
                                            case FreeformElement::Vertex:
                                                return findFreeformVertex(cage, FreeformVertexId{id}) == nullptr;
                                            case FreeformElement::Edge:
                                                return findFreeformEdge(cage, FreeformEdgeId{id}) == nullptr;
                                            case FreeformElement::Face:
                                                return findFreeformFace(cage, FreeformFaceId{id}) == nullptr;
                                        }
                                        return true;
                                    }),
                     selection_.end());
}

FreeformStatus FreeformEditSession::usable() const {
    if (!active_ || body() == nullptr) return FreeformStatus::NotEditing;
    if (capturing_) return FreeformStatus::EditInProgress;
    if (history_.editInProgress()) return FreeformStatus::EditInProgress;
    return FreeformStatus::Ok;
}

FreeformStatus FreeformEditSession::setElement(FreeformElement element) {
    reconcile();
    if (!active_) return lastStatus_ = FreeformStatus::NotEditing;
    if (capturing_) return lastStatus_ = FreeformStatus::EditInProgress;
    if (element != element_) {
        element_ = element;
        selection_.clear();
    }
    return lastStatus_ = FreeformStatus::Ok;
}

void FreeformEditSession::setMultiSelect(bool multi) { multiSelect_ = multi; }

FreeformStatus FreeformEditSession::setTransformMode(GizmoMode mode) {
    if (capturing_) return lastStatus_ = FreeformStatus::EditInProgress;
    mode_ = mode;
    return lastStatus_ = FreeformStatus::Ok;
}

FreeformStatus FreeformEditSession::select(const std::vector<uint32_t>& ids) {
    reconcile();
    if (!active_) return lastStatus_ = FreeformStatus::NotEditing;
    if (capturing_) return lastStatus_ = FreeformStatus::EditInProgress;
    const FreeformCage& cage = body()->freeformOrNull()->cage();
    for (uint32_t id : ids) {
        const bool known = element_ == FreeformElement::Vertex ? findFreeformVertex(cage, FreeformVertexId{id}) != nullptr
                           : element_ == FreeformElement::Edge ? findFreeformEdge(cage, FreeformEdgeId{id}) != nullptr
                                                               : findFreeformFace(cage, FreeformFaceId{id}) != nullptr;
        if (!known) {
            return lastStatus_ = element_ == FreeformElement::Vertex ? FreeformStatus::UnknownVertex
                                 : element_ == FreeformElement::Edge ? FreeformStatus::UnknownEdge
                                                                     : FreeformStatus::UnknownFace;
        }
    }
    selection_ = ids;
    std::sort(selection_.begin(), selection_.end());
    selection_.erase(std::unique(selection_.begin(), selection_.end()), selection_.end());
    return lastStatus_ = FreeformStatus::Ok;
}

void FreeformEditSession::clearSelection() {
    if (!capturing_) selection_.clear();
}

FreeformStatus FreeformEditSession::tap(const CameraSnapshot& camera, float screenX, float screenY,
                                        int viewportWidth, int viewportHeight, bool* outHit) {
    if (outHit != nullptr) *outHit = false;
    reconcile();
    if (!active_) return lastStatus_ = FreeformStatus::NotEditing;
    if (capturing_) return lastStatus_ = FreeformStatus::EditInProgress;
    const SceneObject* b = body();
    Mat4 model;
    if (!scene_.resolveWorldModel(bodyId_, &model)) return lastStatus_ = FreeformStatus::NotEditing;
    std::shared_ptr<const FreeformMesh> derived;
    b->freeformOrNull()->derived(&derived);
    uint32_t id = 0;
    const bool hit = pickFreeformElement(b->freeformOrNull()->cage(), derived.get(), model, camera, screenX,
                                         screenY, viewportWidth, viewportHeight, element_,
                                         kFreeformPickRadiusUnits * gizmoPixelsPerReferenceUnit(), &id);
    if (!hit) {
        if (!multiSelect_) selection_.clear();
        return lastStatus_ = FreeformStatus::Ok;
    }
    if (outHit != nullptr) *outHit = true;
    const auto at = std::lower_bound(selection_.begin(), selection_.end(), id);
    if (multiSelect_) {
        if (at != selection_.end() && *at == id) selection_.erase(at);
        else selection_.insert(at, id);
    } else {
        selection_.assign(1, id);
    }
    return lastStatus_ = FreeformStatus::Ok;
}

bool FreeformEditSession::selectionCentroid(DVec3* out) const {
    const SceneObject* b = body();
    if (b == nullptr || selection_.empty()) return false;
    return freeformSelectionCentroid(b->freeformOrNull()->cage(), element_, selection_, out);
}

GizmoSnapshot FreeformEditSession::gizmoSnapshot(const CameraSnapshot& camera, int viewportWidth,
                                                 int viewportHeight) const {
    GizmoSnapshot out;
    const SceneObject* b = body();
    if (b == nullptr || viewportWidth <= 0 || viewportHeight <= 0) return out;
    if (capturing_) {
        out.pivot = pivot_;
        out.orientation = mat4FromBasis(axes_[0], axes_[1], axes_[2], Vec3{0.0f, 0.0f, 0.0f});
        out.mode = dragMode_;
        out.activeHandle = handle_;
    } else {
        DVec3 centroid;
        Mat4 model;
        if (selection_.empty() || !selectionCentroid(&centroid) || !scene_.resolveWorldModel(bodyId_, &model)) {
            return out;
        }
        out.pivot = worldOf(model, centroid);
        out.orientation = b->transform().rotationMatrix();
        out.mode = mode_;
    }
    float scale = 0.0f;
    if (!gizmoWorldScale(camera, out.pivot, viewportHeight, &scale)) return GizmoSnapshot{};
    out.visible = true;
    out.space = GizmoSpace::Local;
    out.worldPerReferenceUnit = scale;
    out.visualScale = gizmoSession().visualScale();
    return out;
}

bool FreeformEditSession::beginDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX,
                                    float screenY, int viewportWidth, int viewportHeight) {
    reconcile();
    if (!active_ || capturing_ || history_.editInProgress()) return false;
    const GizmoSnapshot state = gizmoSnapshot(camera, viewportWidth, viewportHeight);
    if (!state.visible) return false;
    const GizmoHandle handle =
            gizmoHitTestSnapshot(state, camera, screenX, screenY, viewportWidth, viewportHeight);
    if (handle == GizmoHandle::None) return false;
    Ray ray{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) return false;
    SceneObject* b = body();
    DVec3 centroid;
    if (b == nullptr || !selectionCentroid(&centroid)) return false;

    handle_ = handle;
    dragMode_ = state.mode;
    pivot_ = state.pivot;
    for (int a = 0; a < 3; ++a) {
        axes_[a] = Vec3{state.orientation.m[a * 4 + 0], state.orientation.m[a * 4 + 1],
                        state.orientation.m[a * 4 + 2]};
    }
    const TransformValues values = b->transform().values();
    bodyScale_[0] = values.scaleX;
    bodyScale_[1] = values.scaleY;
    bodyScale_[2] = values.scaleZ;
    localPivot_ = centroid;
    accumulatedAngle_ = 0.0f;
    const int axisIndex = gizmoHandleAxisIndex(handle_);
    bool anchored = true;
    if (dragMode_ == GizmoMode::Move) {
        if (gizmoHandleIsPlane(handle_)) {
            anchored = intersectRayPlane(ray, pivot_, axes_[gizmoPlaneNormalIndex(handle_)], &startPlaneHit_);
        } else {
            anchored = solveAxisParameter(ray, pivot_, axes_[axisIndex], &startAxisT_)
                       != AxisSolveStatus::Unresolvable;
        }
    } else if (dragMode_ == GizmoMode::Rotate) {
        ringNormal_ = axes_[axisIndex];
        Vec3 hit{};
        anchored = intersectRayPlane(ray, pivot_, ringNormal_, &hit)
                   && (signedAngleAround(ringNormal_, Vec3{1.0f, 0.0f, 0.0f}, vec3Sub(hit, pivot_), &lastRingAngle_)
                       || signedAngleAround(ringNormal_, Vec3{0.0f, 1.0f, 0.0f}, vec3Sub(hit, pivot_),
                                            &lastRingAngle_));
    } else {
        const float pixelsPerUnit = gizmoPixelsPerReferenceUnit();
        if (handle_ == GizmoHandle::Uniform) {
            scaleDirX_ = 0.70710678f;
            scaleDirY_ = -0.70710678f;
            scaleReferencePixels_ = kGizmoUniformScaleReferenceUnits * pixelsPerUnit;
        } else {
            GizmoSnapshot canonical = state;
            canonical.visualScale = kGizmoDefaultVisualScale;
            Vec3 reference{};
            if (gizmoHandleIsPlane(handle_)) {
                anchored = gizmoHandleGrabPoint(canonical, handle_, &reference);
            } else {
                reference = vec3Add(pivot_, vec3Scale(axes_[axisIndex],
                                                      kGizmoHandleLengthUnits * state.worldPerReferenceUnit));
            }
            float px = 0.0f, py = 0.0f, hx = 0.0f, hy = 0.0f;
            anchored = anchored
                       && projectWorldToScreen(camera, pivot_, viewportWidth, viewportHeight, &px, &py)
                       && projectWorldToScreen(camera, reference, viewportWidth, viewportHeight, &hx, &hy);
            const float dx = hx - px;
            const float dy = hy - py;
            const float length = std::sqrt(dx * dx + dy * dy);
            anchored = anchored && std::isfinite(length) && length >= kGizmoScaleMinReferenceUnits * pixelsPerUnit;
            if (anchored) {
                scaleDirX_ = dx / length;
                scaleDirY_ = dy / length;
                scaleReferencePixels_ = length;
            }
        }
        scaleDownX_ = screenX;
        scaleDownY_ = screenY;
    }
    if (!anchored) {
        handle_ = GizmoHandle::None;
        return false;
    }
    startCage_ = b->freeformOrNull()->cagePointer();
    dragSelection_ = selection_;
    dragElement_ = element_;
    // ONE edit for the whole drag, closed exactly once in commit or cancel.
    history_.beginEdit();
    capturing_ = true;
    pointerId_ = pointerId;
    return true;
}

bool FreeformEditSession::updateDrag(int32_t pointerId, const CameraSnapshot& camera, float screenX,
                                     float screenY, int viewportWidth, int viewportHeight) {
    if (!capturing_ || pointerId != pointerId_ || body() == nullptr) return false;
    FreeformAffine affine;
    affine.pivot = localPivot_;
    if (dragMode_ == GizmoMode::Scale) {
        const double along = static_cast<double>(screenX - scaleDownX_) * scaleDirX_
                             + static_cast<double>(screenY - scaleDownY_) * scaleDirY_;
        double factor = 1.0 + along / static_cast<double>(scaleReferencePixels_);
        if (!std::isfinite(factor)) return false;
        factor = std::max(factor, kGizmoMinScaleFactor);
        bool on[3] = {false, false, false};
        if (handle_ == GizmoHandle::Uniform) {
            on[0] = on[1] = on[2] = true;
        } else if (gizmoHandleIsPlane(handle_)) {
            int a = 0, c = 0;
            gizmoPlaneAxisIndices(handle_, &a, &c);
            on[a] = on[c] = true;
        } else {
            on[gizmoHandleAxisIndex(handle_)] = true;
        }
        for (int i = 0; i < 3; ++i) affine.linear[i * 4] = on[i] ? factor : 1.0;
    } else {
        Ray ray{};
        if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &ray)) return false;
        if (dragMode_ == GizmoMode::Move) {
            // A world displacement along the frozen local axes, carried into the
            // cage's own (scaled) coordinates: d_local = S^-1 R^T d_world, which
            // for a displacement written in the R columns is just a division.
            double local[3] = {0.0, 0.0, 0.0};
            if (gizmoHandleIsPlane(handle_)) {
                int a = 0, c = 0;
                gizmoPlaneAxisIndices(handle_, &a, &c);
                Vec3 hit{};
                if (!intersectRayPlane(ray, pivot_, axes_[gizmoPlaneNormalIndex(handle_)], &hit)) return false;
                const Vec3 raw = vec3Sub(hit, startPlaneHit_);
                local[a] = static_cast<double>(vec3Dot(raw, axes_[a])) / bodyScale_[a];
                local[c] = static_cast<double>(vec3Dot(raw, axes_[c])) / bodyScale_[c];
            } else {
                const int a = gizmoHandleAxisIndex(handle_);
                float t = 0.0f;
                if (solveAxisParameter(ray, pivot_, axes_[a], &t) == AxisSolveStatus::Unresolvable) return false;
                local[a] = static_cast<double>(t - startAxisT_) / bodyScale_[a];
            }
            affine.translation = DVec3{local[0], local[1], local[2]};
        } else {
            Vec3 hit{};
            if (!intersectRayPlane(ray, pivot_, ringNormal_, &hit)) return false;
            float angle = 0.0f;
            if (!signedAngleAround(ringNormal_, Vec3{1.0f, 0.0f, 0.0f}, vec3Sub(hit, pivot_), &angle)
                && !signedAngleAround(ringNormal_, Vec3{0.0f, 1.0f, 0.0f}, vec3Sub(hit, pivot_), &angle)) {
                return false;
            }
            accumulatedAngle_ += unwrapAngleDelta(angle - lastRingAngle_);
            lastRingAngle_ = angle;
            // A world turn about R e_a is, in the cage's scaled local frame,
            // L = S^-1 Rot_a(theta) S.
            const int a = gizmoHandleAxisIndex(handle_);
            double rot[9];
            rotationAbout(a, static_cast<double>(accumulatedAngle_), rot);
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    affine.linear[r * 3 + c] = rot[r * 3 + c] * bodyScale_[c] / bodyScale_[r];
                }
            }
        }
    }
    FreeformCage next;
    if (freeformTransform(*startCage_, dragElement_, dragSelection_, affine, &next) != FreeformStatus::Ok) {
        return false;  // hold the last good cage
    }
    bool changed = false;
    return applyLocked(next, &changed) == FreeformStatus::Ok && changed;
}

bool FreeformEditSession::commitDrag() {
    if (!capturing_) return false;
    capturing_ = false;
    pointerId_ = -1;
    handle_ = GizmoHandle::None;
    startCage_.reset();
    const bool recorded = history_.commitEdit();
    if (recorded) ++committedDrags_;
    return recorded;
}

void FreeformEditSession::cancelDrag() {
    if (!capturing_) return;
    capturing_ = false;
    pointerId_ = -1;
    handle_ = GizmoHandle::None;
    startCage_.reset();
    history_.cancelEdit();
}

FreeformTouchResult FreeformEditSession::onTouch(TouchAction action, int32_t actionPointerId,
                                                 const TouchPointer* pointers, int count,
                                                 const CameraSnapshot& camera, int viewportWidth,
                                                 int viewportHeight) {
    FreeformTouchResult result;
    if (!active_) return result;
    const float slop = kFreeformTapSlopUnits * gizmoPixelsPerReferenceUnit();
    if (capturing_) {
        int index = -1;
        for (int i = 0; i < count; ++i) {
            if (pointers[i].id == pointerId_) index = i;
        }
        switch (action) {
            case TouchAction::Move:
                if (count == 1 && index >= 0) {
                    result.dragMoved = updateDrag(pointerId_, camera, pointers[index].x, pointers[index].y,
                                                  viewportWidth, viewportHeight);
                    result.consumed = true;
                } else {
                    cancelDrag();
                    result.dragCancelled = true;
                }
                break;
            case TouchAction::Up:
                result.dragCommitted = commitDrag();
                result.consumed = true;
                break;
            case TouchAction::PointerUp:
                if (actionPointerId == pointerId_) {
                    result.dragCommitted = commitDrag();
                    result.consumed = true;
                } else {
                    cancelDrag();
                    result.dragCancelled = true;
                }
                break;
            default:  // a second finger, or a Cancel
                cancelDrag();
                result.dragCancelled = true;
                result.consumed = action == TouchAction::Cancel;
                break;
        }
        return result;
    }
    if (count > 1 || action == TouchAction::PointerDown || action == TouchAction::Cancel) {
        tapArmed_ = false;  // navigation, and never a tap
        return result;
    }
    if (action == TouchAction::Down && count == 1) {
        if (beginDrag(pointers[0].id, camera, pointers[0].x, pointers[0].y, viewportWidth, viewportHeight)) {
            result.dragBegan = true;
            result.consumed = true;
            return result;
        }
        tapArmed_ = true;
        tapPointerId_ = pointers[0].id;
        tapDownX_ = pointers[0].x;
        tapDownY_ = pointers[0].y;
        tapCamera_ = camera;
        result.consumed = true;
        return result;
    }
    if (tapArmed_ && action == TouchAction::Move && count == 1) {
        const float dx = pointers[0].x - tapDownX_;
        const float dy = pointers[0].y - tapDownY_;
        if (dx * dx + dy * dy > slop * slop) {
            tapArmed_ = false;  // this event re-anchors the orbit where it is
            return result;
        }
        result.consumed = true;
        return result;
    }
    if (tapArmed_ && action == TouchAction::Up) {
        tapArmed_ = false;
        // Resolved at the DOWN pixel through the camera the Down saw.
        tap(tapCamera_, tapDownX_, tapDownY_, viewportWidth, viewportHeight);
        result.tapResolved = true;
        result.consumed = true;
        return result;
    }
    return result;
}

// ---------------------------------------------------------------------------
// The typed tools
// ---------------------------------------------------------------------------

FreeformStatus FreeformEditSession::applyLocked(const FreeformCage& next, bool* outChanged) {
    SceneObject* b = body();
    if (b == nullptr) return FreeformStatus::NotEditing;
    bool changed = false;
    const FreeformStatus why = b->freeformOrNull()->applyCage(std::make_shared<const FreeformCage>(next), &changed);
    if (why == FreeformStatus::Ok && changed) publishSceneObject(*b);
    if (outChanged != nullptr) *outChanged = changed;
    return why;
}

template <typename Tool>
FreeformStatus FreeformEditSession::runTool(Tool tool) {
    reconcile();
    const FreeformStatus ready = usable();
    if (ready != FreeformStatus::Ok) return lastStatus_ = ready;
    SceneObject* b = body();
    const std::shared_ptr<const FreeformCage> current = b->freeformOrNull()->cagePointer();
    FreeformCage next;
    const FreeformStatus why = tool(*current, &next);
    if (why != FreeformStatus::Ok) return lastStatus_ = why;
    ScopedConstructionEdit edit(history_);
    const FreeformStatus applied = applyLocked(next);
    if (applied == FreeformStatus::Ok) pruneSelection();
    return lastStatus_ = applied;
}

FreeformStatus FreeformEditSession::pushPull(double distance) {
    if (active_ && element_ != FreeformElement::Face) return lastStatus_ = FreeformStatus::EmptySelection;
    const std::vector<uint32_t> faces = selection_;
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformPushPull(cage, faces, distance, out);
    });
}

FreeformStatus FreeformEditSession::extrude(double distance) {
    if (active_ && element_ != FreeformElement::Face) return lastStatus_ = FreeformStatus::EmptySelection;
    const std::vector<uint32_t> faces = selection_;
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformExtrudeFaces(cage, faces, distance, out);
    });
}

FreeformStatus FreeformEditSession::insertLoop(double ratio) {
    if (active_ && (element_ != FreeformElement::Edge || selection_.size() != 1u)) {
        return lastStatus_ = FreeformStatus::EmptySelection;
    }
    const uint32_t edge = selection_.empty() ? 0u : selection_.front();
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformInsertEdgeLoop(cage, edge, ratio, out);
    });
}

FreeformStatus FreeformEditSession::setCrease(double weight) {
    if (active_ && element_ != FreeformElement::Edge) return lastStatus_ = FreeformStatus::EmptySelection;
    const std::vector<uint32_t> edges = selection_;
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformSetCrease(cage, edges, weight, out);
    });
}

FreeformStatus FreeformEditSession::deleteFaces() {
    if (active_ && element_ != FreeformElement::Face) return lastStatus_ = FreeformStatus::EmptySelection;
    const std::vector<uint32_t> faces = selection_;
    const FreeformStatus why = runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformDeleteFaces(cage, faces, out);
    });
    if (why == FreeformStatus::Ok) selection_.clear();
    return why;
}

FreeformStatus FreeformEditSession::setSymmetry(uint8_t symmetry) {
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformSetSymmetry(cage, symmetry, out);
    });
}

FreeformStatus FreeformEditSession::setLevel(int level) {
    return runTool([&](const FreeformCage& cage, FreeformCage* out) {
        return freeformSetSubdivisionLevel(cage, level, out);
    });
}

SketchOverlayPtr FreeformEditSession::overlay(float worldPerUnit) {
    reconcile();
    const SceneObject* b = body();
    Mat4 model;
    if (b == nullptr || !scene_.resolveWorldModel(bodyId_, &model)) {
        overlay_.reset();
        return overlay_;
    }
    const std::shared_ptr<const FreeformCage>& cage = b->freeformOrNull()->cagePointer();
    const bool same = overlay_ != nullptr && overlayCage_ == cage && overlayElement_ == element_
                      && overlaySelection_ == selection_ && overlayWorldPerUnit_ == worldPerUnit
                      && std::equal(std::begin(overlayModel_.m), std::end(overlayModel_.m), std::begin(model.m));
    if (same) return overlay_;
    ++overlayRevision_;
    overlay_ = buildFreeformCageOverlay(*cage, model, element_, selection_, worldPerUnit, overlayRevision_);
    overlayCage_ = cage;
    overlayElement_ = element_;
    overlaySelection_ = selection_;
    overlayWorldPerUnit_ = worldPerUnit;
    overlayModel_ = model;
    return overlay_;
}

FreeformEditSession& freeformEditSession() {
    static FreeformEditSession session(constructionScene(), constructionHistory());
    return session;
}

FreeformStatus createFreeformBody(ConstructionScene& scene, ConstructionHistory& history, int form,
                                  ObjectId* outId) {
    FreeformCage cage;
    switch (form) {
        case 0: cage = makeFreeformBox(); break;
        case 1: cage = makeFreeformPlane(); break;
        case 2: cage = makeFreeformCylinder(); break;
        default: return FreeformStatus::OutOfRange;
    }
    if (history.editInProgress() && !history.sessionInitializing()) return FreeformStatus::EditInProgress;
    ScopedConstructionEdit edit(history);
    FreeformStatus why = FreeformStatus::Ok;
    SceneObject* body = scene.addFreeformBody(std::move(cage), &why);
    if (body == nullptr) return why;
    publishSceneObject(*body);
    if (outId != nullptr) *outId = body->objectId();
    return FreeformStatus::Ok;
}

}  // namespace forgeshape
