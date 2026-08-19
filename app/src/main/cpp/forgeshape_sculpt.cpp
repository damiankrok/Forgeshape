#include "forgeshape_sculpt.h"

#include <algorithm>
#include <cmath>

namespace forgeshape {
namespace {

// The camera basis, read out of the snapshot's own view matrix exactly as
// forgeshape_picking does. FOV, aspect and the Vulkan Y flip are never restated
// here: there is one camera truth and this reads it.
//
//     r0 = camera right, r1 = camera up, r2 = camera BACKWARD (= -forward)
Vec3 viewRight(const CameraSnapshot& camera) {
    return Vec3{camera.view.m[0], camera.view.m[4], camera.view.m[8]};
}

Vec3 viewUp(const CameraSnapshot& camera) {
    return Vec3{camera.view.m[1], camera.view.m[5], camera.view.m[9]};
}

Vec3 viewForward(const CameraSnapshot& camera) {
    return Vec3{-camera.view.m[2], -camera.view.m[6], -camera.view.m[10]};
}

// World meters per screen pixel on the plane `depth` in front of the eye.
//
// The projection's own vertical term carries the field of view
// (proj.m[5] = -1 / tan(fovY / 2) after the Vulkan Y flip), so the half-height
// of the view at that depth is depth / |proj.m[5]| and one pixel spans twice
// that over the viewport height. Nothing here restates kFovYRadians.
bool worldPerPixelAtDepth(const CameraSnapshot& camera, float depth, int viewportHeight,
                          float* out) {
    if (out == nullptr || viewportHeight <= 0 || !std::isfinite(depth) || depth <= 0.0f) {
        return false;
    }
    const float projY = std::fabs(camera.proj.m[5]);
    if (!std::isfinite(projY) || projY < 1e-8f) {
        return false;
    }
    const float scale = (2.0f * depth) / (projY * static_cast<float>(viewportHeight));
    if (!std::isfinite(scale) || scale <= 0.0f) {
        return false;
    }
    *out = scale;
    return true;
}

Vec3 positionOf(const MeshVertex& v) { return Vec3{v.position[0], v.position[1], v.position[2]}; }

}  // namespace

const char* productModeName(ProductMode mode) {
    switch (mode) {
        case ProductMode::Construction: return "construction";
        case ProductMode::Sculpt: return "sculpt";
    }
    return "unknown";
}

// ---------------------------------------------------------------------------
// The tools
// ---------------------------------------------------------------------------

const char* sculptToolName(SculptTool tool) {
    switch (tool) {
        case SculptTool::Grab: return "grab";
        case SculptTool::Clay: return "clay";
        case SculptTool::Smooth: return "smooth";
        case SculptTool::Inflate: return "inflate";
    }
    return "unknown";
}

int sculptToolIndex(SculptTool tool) { return static_cast<int>(tool); }

bool sculptToolFromIndex(int index, SculptTool* out) {
    if (out == nullptr || index < 0 || index >= kSculptToolCount) {
        return false;
    }
    *out = static_cast<SculptTool>(index);
    return true;
}

bool sculptToolUsesNormals(SculptTool tool) {
    return tool == SculptTool::Clay || tool == SculptTool::Inflate;
}

// ---------------------------------------------------------------------------
// Brush parameters
// ---------------------------------------------------------------------------

float clampBrushRadiusPixels(float requested) {
    if (!std::isfinite(requested)) {
        return kDefaultBrushRadiusPixels;
    }
    if (requested < kMinBrushRadiusPixels) return kMinBrushRadiusPixels;
    if (requested > kMaxBrushRadiusPixels) return kMaxBrushRadiusPixels;
    return requested;
}

float clampBrushStrength(float requested) {
    if (!std::isfinite(requested)) {
        return kDefaultBrushStrength;
    }
    if (requested < kMinBrushStrength) return kMinBrushStrength;
    if (requested > kMaxBrushStrength) return kMaxBrushStrength;
    return requested;
}

float sculptFalloff(float distance, float radius) {
    if (!std::isfinite(distance) || !std::isfinite(radius) || radius <= 0.0f || distance < 0.0f) {
        return 0.0f;
    }
    if (distance >= radius) {
        return 0.0f;
    }
    const float t = distance / radius;
    const float base = 1.0f - t * t;
    return base * base;
}

// ---------------------------------------------------------------------------
// Fixed-topology adjacency
// ---------------------------------------------------------------------------

void SculptTopology::clear() {
    vertexCount_ = 0;
    neighborStart_.clear();
    neighborList_.clear();
    triangleStart_.clear();
    triangleList_.clear();
}

void SculptTopology::build(const std::vector<MeshVertex>& vertices,
                           const std::vector<uint32_t>& indices) {
    clear();
    const auto vertexCount = static_cast<uint32_t>(vertices.size());
    if (vertexCount == 0 || indices.size() < 3) {
        return;
    }
    const size_t triangleCount = indices.size() / 3;

    // Two counting passes then two fill passes, so the CSR arrays are allocated
    // exactly once each and nothing here grows per triangle.
    std::vector<uint32_t> neighborCounts(vertexCount, 0);
    std::vector<uint32_t> triangleCounts(vertexCount, 0);

    auto cornerValid = [&](uint32_t a, uint32_t b, uint32_t c) {
        // Out-of-range indices cannot reach here from a validated RuntimeMesh,
        // but the check is kept: a neighbour list must never contain an index a
        // caller could dereference out of bounds.
        return a < vertexCount && b < vertexCount && c < vertexCount;
    };

    for (size_t t = 0; t < triangleCount; ++t) {
        const uint32_t a = indices[t * 3 + 0];
        const uint32_t b = indices[t * 3 + 1];
        const uint32_t c = indices[t * 3 + 2];
        if (!cornerValid(a, b, c)) {
            continue;
        }
        triangleCounts[a]++;
        triangleCounts[b]++;
        triangleCounts[c]++;
        // Upper bound before deduplication: each corner sees its two others.
        neighborCounts[a] += 2;
        neighborCounts[b] += 2;
        neighborCounts[c] += 2;
    }

    triangleStart_.assign(vertexCount + 1, 0);
    for (uint32_t v = 0; v < vertexCount; ++v) {
        triangleStart_[v + 1] = triangleStart_[v] + triangleCounts[v];
    }
    triangleList_.assign(triangleStart_[vertexCount], 0);

    std::vector<uint32_t> rawStart(vertexCount + 1, 0);
    for (uint32_t v = 0; v < vertexCount; ++v) {
        rawStart[v + 1] = rawStart[v] + neighborCounts[v];
    }
    std::vector<uint32_t> raw(rawStart[vertexCount], 0);

    std::vector<uint32_t> triangleCursor(triangleStart_.begin(), triangleStart_.end() - 1);
    std::vector<uint32_t> rawCursor(rawStart.begin(), rawStart.end() - 1);

    auto addNeighbor = [&](uint32_t of, uint32_t other) {
        if (of == other) {
            return;  // a degenerate corner never becomes its own neighbour
        }
        raw[rawCursor[of]++] = other;
    };

    for (size_t t = 0; t < triangleCount; ++t) {
        const uint32_t a = indices[t * 3 + 0];
        const uint32_t b = indices[t * 3 + 1];
        const uint32_t c = indices[t * 3 + 2];
        if (!cornerValid(a, b, c)) {
            continue;
        }
        const auto triangle = static_cast<uint32_t>(t);
        triangleList_[triangleCursor[a]++] = triangle;
        triangleList_[triangleCursor[b]++] = triangle;
        triangleList_[triangleCursor[c]++] = triangle;
        addNeighbor(a, b);
        addNeighbor(a, c);
        addNeighbor(b, a);
        addNeighbor(b, c);
        addNeighbor(c, a);
        addNeighbor(c, b);
    }

    // Sort + unique per vertex: a shared edge is seen once from each of its two
    // triangles, so without this every interior neighbour would appear twice and
    // a neighbour average would silently weight interior neighbours double.
    neighborStart_.assign(vertexCount + 1, 0);
    neighborList_.clear();
    neighborList_.reserve(raw.size());
    for (uint32_t v = 0; v < vertexCount; ++v) {
        const uint32_t from = rawStart[v];
        const uint32_t to = rawCursor[v];
        std::sort(raw.begin() + from, raw.begin() + to);
        const auto last = std::unique(raw.begin() + from, raw.begin() + to);
        for (auto it = raw.begin() + from; it != last; ++it) {
            neighborList_.push_back(*it);
        }
        neighborStart_[v + 1] = static_cast<uint32_t>(neighborList_.size());
    }

    vertexCount_ = vertexCount;
    ++buildCount_;
}

const uint32_t* SculptTopology::neighbors(uint32_t vertex, uint32_t* outCount) const {
    if (outCount != nullptr) {
        *outCount = 0;
    }
    if (vertex >= vertexCount_) {
        return nullptr;
    }
    const uint32_t from = neighborStart_[vertex];
    const uint32_t to = neighborStart_[vertex + 1];
    if (outCount != nullptr) {
        *outCount = to - from;
    }
    return (to > from) ? &neighborList_[from] : nullptr;
}

uint32_t SculptTopology::neighborCount(uint32_t vertex) const {
    if (vertex >= vertexCount_) {
        return 0;
    }
    return neighborStart_[vertex + 1] - neighborStart_[vertex];
}

const uint32_t* SculptTopology::incidentTriangles(uint32_t vertex, uint32_t* outCount) const {
    if (outCount != nullptr) {
        *outCount = 0;
    }
    if (vertex >= vertexCount_) {
        return nullptr;
    }
    const uint32_t from = triangleStart_[vertex];
    const uint32_t to = triangleStart_[vertex + 1];
    if (outCount != nullptr) {
        *outCount = to - from;
    }
    return (to > from) ? &triangleList_[from] : nullptr;
}

uint32_t SculptTopology::incidentTriangleCount(uint32_t vertex) const {
    if (vertex >= vertexCount_) {
        return 0;
    }
    return triangleStart_[vertex + 1] - triangleStart_[vertex];
}

// ---------------------------------------------------------------------------
// Vertex normals
// ---------------------------------------------------------------------------

void computeVertexNormals(const std::vector<MeshVertex>& vertices,
                          const std::vector<uint32_t>& indices, std::vector<Vec3>* out) {
    if (out == nullptr) {
        return;
    }
    const auto vertexCount = static_cast<uint32_t>(vertices.size());
    out->assign(vertexCount, Vec3{0.0f, 0.0f, 0.0f});
    if (vertexCount == 0 || indices.size() < 3) {
        return;
    }

    const size_t triangleCount = indices.size() / 3;
    for (size_t t = 0; t < triangleCount; ++t) {
        const uint32_t ia = indices[t * 3 + 0];
        const uint32_t ib = indices[t * 3 + 1];
        const uint32_t ic = indices[t * 3 + 2];
        if (ia >= vertexCount || ib >= vertexCount || ic >= vertexCount) {
            continue;
        }
        const Vec3 a = positionOf(vertices[ia]);
        const Vec3 b = positionOf(vertices[ib]);
        const Vec3 c = positionOf(vertices[ic]);
        // The canonical winding is counter-clockwise seen from OUTSIDE, so this
        // cross product points away from the solid — the same rule picking uses
        // to decide a front face. Length is twice the triangle's area, which is
        // exactly the area weighting we want, so it is deliberately not
        // normalized here.
        const Vec3 n = vec3Cross(vec3Sub(b, a), vec3Sub(c, a));
        if (!vec3Finite(n)) {
            continue;  // a degenerate or poisoned triangle contributes nothing
        }
        (*out)[ia] = vec3Add((*out)[ia], n);
        (*out)[ib] = vec3Add((*out)[ib], n);
        (*out)[ic] = vec3Add((*out)[ic], n);
    }

    for (uint32_t v = 0; v < vertexCount; ++v) {
        const Vec3 n = (*out)[v];
        const float lengthSquared = vec3Dot(n, n);
        if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) {
            // No usable direction here. Zero is the honest answer, and a
            // normal-based brush simply does not move such a vertex — far better
            // than inventing an axis and pushing geometry somewhere arbitrary.
            (*out)[v] = Vec3{0.0f, 0.0f, 0.0f};
            continue;
        }
        const float length = std::sqrt(lengthSquared);
        if (!std::isfinite(length) || length <= 0.0f) {
            (*out)[v] = Vec3{0.0f, 0.0f, 0.0f};
            continue;
        }
        const Vec3 unit{n.x / length, n.y / length, n.z / length};
        (*out)[v] = vec3Finite(unit) ? unit : Vec3{0.0f, 0.0f, 0.0f};
    }
}

// ---------------------------------------------------------------------------
// The Frozen Sculpt Mesh
// ---------------------------------------------------------------------------

bool SculptMesh::freezeFrom(const ConstructionMesh& source, ObjectId objectId,
                            MeshValidation* outWhy) {
    const auto vertexCount = static_cast<uint32_t>(source.vertices.size());
    const auto indexCount = static_cast<uint32_t>(source.indices.size());
    const MeshValidation why =
        validateMeshData(source.vertices.empty() ? nullptr : source.vertices.data(), vertexCount,
                         source.indices.empty() ? nullptr : source.indices.data(), indexCount);
    if (outWhy != nullptr) {
        *outWhy = why;
    }
    if (why != MeshValidation::Ok) {
        // Fails closed: a previous frozen mesh, if any, is left exactly as it
        // was rather than being half replaced.
        return false;
    }

    // A wholesale COPY. Nothing is shared with the Construction object, so no
    // sculpt edit can reach back into Construction data, and the source stays
    // available to regenerate its own mesh at any time.
    vertices_ = source.vertices;
    indices_ = source.indices;
    objectId_ = objectId;
    // The adjacency is a function of the index buffer alone, and no API on this
    // class can change an index, so building it here — once per Freeze — is
    // building it once for the life of this frozen mesh.
    topology_.build(vertices_, indices_);
    normalsDirty_ = true;
    // Every Freeze restarts this mesh's own revision at 1. It is not, and never
    // becomes, comparable with a MeshStore revision.
    revision_ = 1;
    ++freezeCount_;
    return true;
}

const std::vector<Vec3>& SculptMesh::vertexNormals() const {
    if (normalsDirty_ || normals_.size() != vertices_.size()) {
        computeVertexNormals(vertices_, indices_, &normals_);
        normalsDirty_ = false;
        ++normalRecomputeCount_;
    }
    return normals_;
}

Vec3 SculptMesh::vertexNormal(uint32_t index) const {
    const std::vector<Vec3>& normals = vertexNormals();
    if (index >= normals.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return normals[index];
}

Vec3 SculptMesh::vertexPosition(uint32_t index) const {
    if (index >= vertices_.size()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    return positionOf(vertices_[index]);
}

bool SculptMesh::setVertexPosition(uint32_t index, const Vec3& position) {
    if (index >= vertices_.size() || !vec3Finite(position)) {
        return false;
    }
    MeshVertex& v = vertices_[index];
    v.position[0] = position.x;
    v.position[1] = position.y;
    v.position[2] = position.z;
    // Derived data follows the positions it was derived from. This is the ONLY
    // place normals are invalidated, so the recomputation rule is one line.
    normalsDirty_ = true;
    return true;
}

SculptRevision SculptMesh::advanceRevision() {
    if (!frozen()) {
        return kNoSculptRevision;
    }
    return ++revision_;
}

TriangleMeshView SculptMesh::triangleView() const {
    TriangleMeshView view{};
    view.positions = vertices_.empty() ? nullptr : vertices_[0].position;
    view.positionStride = sizeof(MeshVertex);
    view.vertexCount = static_cast<uint32_t>(vertices_.size());
    view.indices = indices_.empty() ? nullptr : indices_.data();
    view.indexCount = static_cast<uint32_t>(indices_.size());
    return view;
}

MeshRevision publishSculptMesh(const SculptMesh& mesh, MeshStore& store, MeshValidation* outWhy) {
    if (!mesh.frozen()) {
        if (outWhy != nullptr) {
            *outWhy = MeshValidation::EmptyVertices;
        }
        return kNoMeshRevision;
    }
    return store.publish(mesh.vertices().data(), mesh.vertexCount(), mesh.indices().data(),
                         mesh.indexCount(), outWhy);
}

// ---------------------------------------------------------------------------
// The stroke kernel — everything all four tools share
// ---------------------------------------------------------------------------

bool SculptStroke::begin(SculptTool tool, const SculptMesh& mesh, const CameraSnapshot& camera,
                         float screenX, float screenY, int viewportWidth, int viewportHeight,
                         const Mat4& model, const Mat4& inverseModel, float radiusPixels) {
    active_ = false;
    affected_.clear();
    lastLocalDisplacement_ = Vec3{0.0f, 0.0f, 0.0f};
    lastAmount_ = 0.0f;
    travelPixels_ = 0.0f;

    if (!mesh.frozen() || viewportWidth <= 0 || viewportHeight <= 0) {
        return false;
    }
    if (!mat4Finite(model) || !mat4Finite(inverseModel)) {
        return false;
    }
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return false;
    }

    // Raycast the CURRENT Frozen Sculpt Mesh — not the Construction mesh and not
    // GPU memory — using the same ray builder, the same front-face rule and the
    // same local-space trick picking already uses. This is identical for all
    // four tools: what starts a stroke does not depend on which tool is held.
    Ray worldRay{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &worldRay)) {
        return false;
    }
    Ray localRay{};
    if (!transformRayToLocal(worldRay, inverseModel, &localRay)) {
        return false;
    }
    const TriangleHit hit = pickTriangleMesh(localRay, mesh.triangleView(), /*frontFacesOnly=*/true);
    if (!hit.hit) {
        return false;  // a miss starts NO stroke
    }

    const Vec3 worldHit = mat4TransformPoint(model, hit.position);
    if (!vec3Finite(worldHit)) {
        return false;
    }

    // Brush size is authored in pixels, so it has to be resolved at the depth of
    // the thing being worked on. Depth is measured along the camera FORWARD
    // axis, not along the ray, so the scale is the same everywhere on screen.
    const float depth = vec3Dot(vec3Sub(worldHit, camera.eye), viewForward(camera));
    float worldPerPixel = 0.0f;
    if (!worldPerPixelAtDepth(camera, depth, viewportHeight, &worldPerPixel)) {
        return false;
    }

    // The transform is rigid, so a length in world space is the same length in
    // object space and the radius needs no conversion.
    const float clampedRadiusPixels = clampBrushRadiusPixels(radiusPixels);
    const float localRadius = clampedRadiusPixels * worldPerPixel;
    if (!std::isfinite(localRadius) || localRadius <= 0.0f) {
        return false;
    }

    // The affected set and its weights are captured ONCE, here, from the sculpt
    // geometry as it stands at stroke start. They do not change while the finger
    // moves, so a vertex cannot wander into or out of the brush mid-stroke and
    // the stroke stays a single coherent deformation — for every tool.
    //
    // The base NORMAL is captured with the base position because Clay deposits
    // along the direction the surface faced when the stroke began.
    const std::vector<Vec3>& normals = mesh.vertexNormals();
    const uint32_t vertexCount = mesh.vertexCount();
    for (uint32_t i = 0; i < vertexCount; ++i) {
        const Vec3 p = mesh.vertexPosition(i);
        const Vec3 d = vec3Sub(p, hit.position);
        const float distanceSquared = vec3Dot(d, d);
        if (!std::isfinite(distanceSquared) || distanceSquared >= localRadius * localRadius) {
            continue;
        }
        const float weight = sculptFalloff(std::sqrt(distanceSquared), localRadius);
        if (!(weight > 0.0f)) {
            continue;
        }
        SculptStrokeVertex captured;
        captured.index = i;
        captured.weight = weight;
        captured.basePosition = p;
        captured.baseNormal = (i < normals.size()) ? normals[i] : Vec3{0.0f, 0.0f, 0.0f};
        affected_.push_back(captured);
    }
    if (affected_.empty()) {
        return false;  // a brush that would move nothing is not a stroke
    }

    centreSlot_ = 0;
    for (size_t i = 1; i < affected_.size(); ++i) {
        if (affected_[i].weight > affected_[centreSlot_].weight) {
            centreSlot_ = i;
        }
    }

    tool_ = tool;
    anchorX_ = screenX;
    anchorY_ = screenY;
    lastX_ = screenX;
    lastY_ = screenY;
    localCenter_ = hit.position;
    localRadius_ = localRadius;
    radiusPixels_ = clampedRadiusPixels;
    worldPerPixel_ = worldPerPixel;
    hitDepth_ = depth;
    cameraRight_ = viewRight(camera);
    cameraUp_ = viewUp(camera);
    inverseModel_ = inverseModel;
    scratchTargets_.clear();
    active_ = true;
    return true;
}

bool SculptStroke::update(SculptMesh& mesh, float screenX, float screenY, float strength) {
    if (!active_ || !mesh.frozen()) {
        return false;
    }
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return false;
    }
    const float clampedStrength = clampBrushStrength(strength);

    // Pointer travel SINCE THE PREVIOUS MOVE, which is what the three
    // path-driven tools apply their amount over. Measuring path length rather
    // than counting events is what makes them independent of the event rate: the
    // same finger path always deposits, relaxes or expands the same amount,
    // whether Android delivered it in five events or fifty.
    const float stepX = screenX - lastX_;
    const float stepY = screenY - lastY_;
    float stepPixels = std::sqrt(stepX * stepX + stepY * stepY);
    if (!std::isfinite(stepPixels)) {
        return false;
    }
    lastX_ = screenX;
    lastY_ = screenY;
    travelPixels_ += stepPixels;

    if (tool_ == SculptTool::Grab) {
        // Grab is POSITION-driven, not path-driven: it depends only on where the
        // finger is relative to where it went down.
        return applyGrab(mesh, screenX, screenY, clampedStrength);
    }

    if (!(radiusPixels_ > 0.0f) || !(stepPixels > 0.0f)) {
        return false;  // a stationary finger deposits nothing and costs nothing
    }
    const float travelFraction = stepPixels / radiusPixels_;
    if (!std::isfinite(travelFraction) || travelFraction <= 0.0f) {
        return false;
    }

    if (tool_ == SculptTool::Smooth) {
        return applySmooth(mesh, clampedStrength, travelFraction);
    }

    float amount = clampedStrength * localRadius_ * kNormalBrushGain * travelFraction;
    if (!std::isfinite(amount) || amount <= 0.0f) {
        return false;
    }
    // Bounded by construction: however far a single event claims the pointer
    // jumped, one move can never displace a vertex by more than one brush
    // radius, so no stroke can throw geometry off to infinity.
    if (amount > localRadius_) {
        amount = localRadius_;
    }
    lastAmount_ = amount;

    return (tool_ == SculptTool::Clay) ? applyClay(mesh, amount) : applyInflate(mesh, amount);
}

// --- Grab: drag the surface with the finger, in the camera plane -------------
//
// Unchanged from the Stage 012 contract, now expressed as one rule inside the
// shared kernel.
bool SculptStroke::applyGrab(SculptMesh& mesh, float screenX, float screenY, float strength) {
    // Pointer travel since the stroke started, converted to a displacement in
    // the camera plane at the hit depth. Screen Y grows downward and camera up
    // grows upward, which is the one sign flip in the whole brush.
    const float dx = (screenX - anchorX_) * worldPerPixel_;
    const float dy = (screenY - anchorY_) * worldPerPixel_;
    Vec3 worldDelta = vec3Scale(cameraRight_, dx);
    worldDelta = vec3Add(worldDelta, vec3Scale(cameraUp_, -dy));
    if (!vec3Finite(worldDelta)) {
        return false;
    }

    // The mesh is LOCAL geometry under the object's transform, so the world
    // displacement has to be carried into object space. It is a DIRECTION, so
    // only the rotation applies; the transform is rigid, so its length survives.
    Vec3 localDelta = mat4TransformDirection(inverseModel_, worldDelta);
    if (!vec3Finite(localDelta)) {
        return false;
    }
    localDelta = vec3Scale(localDelta, strength);
    if (!vec3Finite(localDelta)) {
        return false;
    }
    lastLocalDisplacement_ = localDelta;

    // Every Move recomputes base + delta * weight rather than accumulating, so
    // the result depends only on where the finger IS, never on how many events
    // it took to get there.
    bool changed = false;
    for (const SculptStrokeVertex& v : affected_) {
        const Vec3 target = vec3Add(v.basePosition, vec3Scale(localDelta, v.weight));
        if (!vec3Finite(target)) {
            continue;
        }
        const Vec3 current = mesh.vertexPosition(v.index);
        if (current.x == target.x && current.y == target.y && current.z == target.z) {
            continue;
        }
        if (mesh.setVertexPosition(v.index, target)) {
            changed = true;
        }
    }
    return changed;
}

// --- Clay: deposit along the normals the surface had at stroke start ---------
//
// Clay is DEPOSITION. Every affected vertex moves along the direction that
// vertex faced when the finger landed — a direction field that is captured once
// and then held fixed for the whole stroke, exactly as the affected set and the
// falloff weights are. The result is a coherent slab of material laid down in
// one consistent set of directions, so the shape the stroke builds is the brush
// profile itself and repeated passes make it taller without changing what it is.
//
// This is why Clay is NOT Inflate: Clay's total displacement of a vertex is
// exactly parallel to that vertex's ORIGINAL normal, no matter how many moves it
// took or how far the surface has already been pushed.
bool SculptStroke::applyClay(SculptMesh& mesh, float amount) {
    bool changed = false;
    for (size_t i = 0; i < affected_.size(); ++i) {
        const SculptStrokeVertex& v = affected_[i];
        const Vec3 step = vec3Scale(v.baseNormal, amount * v.weight);
        if (!vec3Finite(step)) {
            continue;
        }
        const Vec3 current = mesh.vertexPosition(v.index);
        const Vec3 target = vec3Add(current, step);
        if (!vec3Finite(target)) {
            continue;
        }
        if (i == centreSlot_) {
            lastLocalDisplacement_ = step;
        }
        if (target.x == current.x && target.y == current.y && target.z == current.z) {
            continue;
        }
        if (mesh.setVertexPosition(v.index, target)) {
            changed = true;
        }
    }
    return changed;
}

// --- Inflate: expand along the normals the surface has RIGHT NOW -------------
//
// Inflate re-reads the vertex normals from the CURRENT positions on every move,
// so the direction each vertex expands in follows the surface as the surface
// changes. On a bulge that is already forming, the flank vertices' normals have
// tilted outward, so Inflate widens and rounds the bulge instead of extruding it
// along the directions it started with.
//
// That is the whole difference from Clay, and it is a difference in the formula,
// not in a constant: Clay reads `baseNormal`, captured once; Inflate reads
// `mesh.vertexNormals()`, recomputed from the geometry it is deforming. It is
// directly observable — after a few moves an inflated vertex's TOTAL
// displacement is no longer parallel to the normal it started with, while a
// clayed vertex's always is.
bool SculptStroke::applyInflate(SculptMesh& mesh, float amount) {
    // The normals are snapshotted into the scratch buffer before ANY position is
    // written, so every vertex in this move expands along the surface as it
    // stood at the start of the move. Reading them one at a time while writing
    // would make the result depend on the order of the affected set.
    const std::vector<Vec3>& live = mesh.vertexNormals();
    scratchTargets_.assign(affected_.size(), Vec3{0.0f, 0.0f, 0.0f});
    for (size_t i = 0; i < affected_.size(); ++i) {
        const uint32_t index = affected_[i].index;
        scratchTargets_[i] = (index < live.size()) ? live[index] : Vec3{0.0f, 0.0f, 0.0f};
    }

    bool changed = false;
    for (size_t i = 0; i < affected_.size(); ++i) {
        const SculptStrokeVertex& v = affected_[i];
        const Vec3 step = vec3Scale(scratchTargets_[i], amount * v.weight);
        if (!vec3Finite(step)) {
            continue;
        }
        const Vec3 current = mesh.vertexPosition(v.index);
        const Vec3 target = vec3Add(current, step);
        if (!vec3Finite(target)) {
            continue;
        }
        if (i == centreSlot_) {
            lastLocalDisplacement_ = step;
        }
        if (target.x == current.x && target.y == current.y && target.z == current.z) {
            continue;
        }
        if (mesh.setVertexPosition(v.index, target)) {
            changed = true;
        }
    }
    return changed;
}

// --- Smooth: relax each vertex toward its 1-ring neighbour average -----------
//
// Bounded interpolation, never extrapolation:
//
//     p := p + (neighbourAverage - p) * lambda,   0 < lambda <= kMaxSmoothLambda
//
// so a vertex can only ever move part of the way toward a point it is already
// surrounded by. That is what makes repeated smoothing converge — the deviation
// from the neighbour mean shrinks by (1 - lambda) each application and can never
// change sign — and it is why no clamp on the result is needed to keep it
// finite.
//
// Every target is computed from a coherent snapshot of the CURRENT positions
// before anything is written (Jacobi, not Gauss-Seidel), so the outcome does not
// depend on the order the affected set happens to be in. Only affected vertices
// are written; neighbours outside the brush are read and never modified.
bool SculptStroke::applySmooth(SculptMesh& mesh, float strength, float travelFraction) {
    const SculptTopology& topology = mesh.topology();
    if (!topology.built()) {
        return false;
    }

    scratchTargets_.assign(affected_.size(), Vec3{0.0f, 0.0f, 0.0f});
    std::vector<bool> usable(affected_.size(), false);

    for (size_t i = 0; i < affected_.size(); ++i) {
        const uint32_t index = affected_[i].index;
        uint32_t neighborCount = 0;
        const uint32_t* neighbors = topology.neighbors(index, &neighborCount);
        if (neighbors == nullptr || neighborCount == 0) {
            continue;  // an isolated vertex has no average to move toward
        }
        Vec3 sum{0.0f, 0.0f, 0.0f};
        uint32_t used = 0;
        for (uint32_t n = 0; n < neighborCount; ++n) {
            const Vec3 p = mesh.vertexPosition(neighbors[n]);
            if (!vec3Finite(p)) {
                continue;
            }
            sum = vec3Add(sum, p);
            ++used;
        }
        if (used == 0) {
            continue;
        }
        const Vec3 mean = vec3Scale(sum, 1.0f / static_cast<float>(used));
        if (!vec3Finite(mean)) {
            continue;
        }
        scratchTargets_[i] = mean;
        usable[i] = true;
    }

    bool changed = false;
    for (size_t i = 0; i < affected_.size(); ++i) {
        if (!usable[i]) {
            continue;
        }
        const SculptStrokeVertex& v = affected_[i];
        float lambda = strength * v.weight * kSmoothGain * travelFraction;
        if (!std::isfinite(lambda) || lambda <= 0.0f) {
            continue;
        }
        if (lambda > kMaxSmoothLambda) {
            lambda = kMaxSmoothLambda;
        }
        const Vec3 current = mesh.vertexPosition(v.index);
        const Vec3 toMean = vec3Sub(scratchTargets_[i], current);
        const Vec3 step = vec3Scale(toMean, lambda);
        const Vec3 target = vec3Add(current, step);
        if (!vec3Finite(target)) {
            continue;
        }
        if (i == centreSlot_) {
            lastLocalDisplacement_ = step;
        }
        if (target.x == current.x && target.y == current.y && target.z == current.z) {
            continue;
        }
        if (mesh.setVertexPosition(v.index, target)) {
            changed = true;
        }
    }
    lastAmount_ = 0.0f;
    return changed;
}

void SculptStroke::end() {
    active_ = false;
    affected_.clear();
}

void SculptStroke::cancel() {
    active_ = false;
    affected_.clear();
    lastLocalDisplacement_ = Vec3{0.0f, 0.0f, 0.0f};
    lastAmount_ = 0.0f;
    travelPixels_ = 0.0f;
}

float SculptStroke::weightOfVertex(uint32_t meshVertexIndex) const {
    for (const SculptStrokeVertex& v : affected_) {
        if (v.index == meshVertexIndex) {
            return v.weight;
        }
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// The sculpt session
// ---------------------------------------------------------------------------

bool SculptSession::freezeToSculpt(const ConstructionMesh& source, ObjectId objectId,
                                   MeshValidation* outWhy) {
    // A live stroke cannot survive a Freeze: its captured vertex indices and
    // base positions describe the mesh that is being replaced.
    stroke_.cancel();
    if (!mesh_.freezeFrom(source, objectId, outWhy)) {
        return false;  // mode and any previous frozen mesh both stand
    }
    sourceStale_ = false;
    mode_ = ProductMode::Sculpt;
    return true;
}

void SculptSession::enterConstruction() {
    stroke_.cancel();
    mode_ = ProductMode::Construction;
    // The Frozen Sculpt Mesh is deliberately kept, untouched, so returning to
    // Sculpt restores the prior edits without re-freezing. Nothing at all is
    // written back into the Construction Source. The active tool is kept too:
    // it is a property of how the user is working, not of the mesh.
}

bool SculptSession::enterSculpt() {
    if (!mesh_.frozen()) {
        return false;  // there is nothing to sculpt until something is frozen
    }
    stroke_.cancel();
    mode_ = ProductMode::Sculpt;
    return true;
}

bool SculptSession::hitsSculptMesh(const CameraSnapshot& camera, float screenX, float screenY,
                                   int viewportWidth, int viewportHeight,
                                   const Mat4& inverseModel) const {
    if (mode_ != ProductMode::Sculpt || !mesh_.frozen()) {
        return false;
    }
    if (viewportWidth <= 0 || viewportHeight <= 0 || !mat4Finite(inverseModel)) {
        return false;
    }
    if (!std::isfinite(screenX) || !std::isfinite(screenY)) {
        return false;
    }
    Ray worldRay{};
    if (!buildPickRay(camera, screenX, screenY, viewportWidth, viewportHeight, &worldRay)) {
        return false;
    }
    Ray localRay{};
    if (!transformRayToLocal(worldRay, inverseModel, &localRay)) {
        return false;
    }
    return pickTriangleMesh(localRay, mesh_.triangleView(), /*frontFacesOnly=*/true).hit;
}

bool SculptSession::beginStroke(const CameraSnapshot& camera, float screenX, float screenY,
                                int viewportWidth, int viewportHeight, const Mat4& model,
                                const Mat4& inverseModel) {
    if (mode_ != ProductMode::Sculpt) {
        return false;  // the brush exists only in Sculpt mode
    }
    if (!stroke_.begin(tool_, mesh_, camera, screenX, screenY, viewportWidth, viewportHeight, model,
                       inverseModel, radiusPixels_)) {
        return false;
    }
    ++strokeCount_;
    return true;
}

bool SculptSession::updateStroke(float screenX, float screenY) {
    if (!stroke_.active()) {
        return false;
    }
    if (!stroke_.update(mesh_, screenX, screenY, strength_)) {
        return false;
    }
    // One revision per coherent batch of position writes.
    mesh_.advanceRevision();
    return true;
}

void SculptSession::endStroke() { stroke_.end(); }

void SculptSession::cancelStroke() { stroke_.cancel(); }

SculptSession& sculptSession() {
    static SculptSession session;
    return session;
}

}  // namespace forgeshape
