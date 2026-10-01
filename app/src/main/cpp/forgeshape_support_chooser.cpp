#include "forgeshape_support_chooser.h"

#include <cmath>

#include "forgeshape_cad_face.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_picking.h"
#include "forgeshape_selection.h"

namespace forgeshape {

namespace {

// A face's world frame from its world matrix: columns u, v, n and the origin.
SketchFrame frameOf(const Mat4& faceWorld) {
    SketchFrame frame;
    frame.origin = mat4TransformPoint(faceWorld, Vec3{0, 0, 0});
    frame.u = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{1, 0, 0}));
    frame.v = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0, 1, 0}));
    frame.n = vec3Normalize(mat4TransformDirection(faceWorld, Vec3{0, 0, 1}));
    return frame;
}

}  // namespace

CadStatus refreshChosenSupport(const ConstructionScene& scene, const ChosenSupport& chosen,
                               ChosenSupport* out) {
    if (out == nullptr) {
        return CadStatus::NotSketching;
    }
    if (chosen.kind == ChosenSupport::Kind::WorldPlane) {
        *out = chosen;
        return CadStatus::Ok;
    }
    if (chosen.kind != ChosenSupport::Kind::Face) {
        return CadStatus::ProfileNotFound;
    }
    // The SAME rule a committed face-supported body is held to, now.
    const CadStatus why = scene.validateCadFaceSupport(chosen.faceRef);
    if (why != CadStatus::Ok) {
        return why;
    }
    const SceneObject* producer = scene.findBody(chosen.faceRef.producerObjectId);
    CadFace face;
    Mat4 producerModel;
    if (producer == nullptr || producer->cadOrNull() == nullptr
        || resolveCadFeatureFace(producer->cadOrNull()->state(),
                                 chosen.faceRef.producerLocalFeatureId, chosen.faceRef.face, &face)
                   != CadStatus::Ok
        || !scene.resolveWorldModel(producer->objectId(), &producerModel)) {
        return CadStatus::ProfileNotFound;
    }
    const Mat4 faceWorld = mat4Multiply(producerModel, cadFaceFrameMatrix(face));
    if (!mat4Finite(faceWorld)) {
        return CadStatus::ProfileNotFound;
    }
    ChosenSupport fresh = chosen;
    fresh.worldFrame = frameOf(faceWorld);
    *out = fresh;
    return CadStatus::Ok;
}
namespace {

void pushLine(std::vector<GizmoVertex>* out, const Vec3& a, const Vec3& b, float axis,
              float handle) {
    out->push_back(GizmoVertex{{a.x, a.y, a.z}, axis, handle});
    out->push_back(GizmoVertex{{b.x, b.y, b.z}, axis, handle});
}

float hueForWorldAxisNormal(const Vec3& n) {
    if (std::fabs(n.x) > 0.5f) return 1.0f;
    if (std::fabs(n.y) > 0.5f) return 2.0f;
    return 3.0f;
}

float planeHue(Workplane plane) {
    // The plane is tinted by its NORMAL's axis, matching the world axis colours.
    return hueForWorldAxisNormal(workplaneFrame(plane).normal);
}

// A world-plane square is drawn in its own (u, v) at the origin, from -H..+H.
void drawPlaneTarget(std::vector<GizmoVertex>* v, Workplane plane, bool emphasised) {
    const float H = kSupportPlaneHalfMeters;
    const float hue = planeHue(plane);
    const float handle = emphasised ? 1.0f : 0.0f;
    const auto p = [plane](double u, double w) { return workplaneToLocal(plane, SketchPoint{u, w}); };
    // Border.
    pushLine(v, p(-H, -H), p(H, -H), hue, handle);
    pushLine(v, p(H, -H), p(H, H), hue, handle);
    pushLine(v, p(H, H), p(-H, H), hue, handle);
    pushLine(v, p(-H, H), p(-H, -H), hue, handle);
    // A light inner grid so it reads as a plane, not a frame.
    const int step = 1;
    for (int i = -static_cast<int>(H) + step; i < static_cast<int>(H); i += step) {
        if (i == 0) continue;
        pushLine(v, p(i, -H), p(i, H), hue, handle);
        pushLine(v, p(-H, i), p(H, i), hue, handle);
    }
    // The two axes through the origin, emphasised.
    pushLine(v, p(-H, 0), p(H, 0), hue, 1.0f);
    pushLine(v, p(0, -H), p(0, H), hue, 1.0f);
}

// A face target outline, from its resolved world frame, a square around origin.
void drawFaceTarget(std::vector<GizmoVertex>* v, const SketchFrame& f) {
    const float H = 0.6f;
    const auto p = [&](float a, float b) {
        return vec3Add(f.origin, vec3Add(vec3Scale(f.u, a), vec3Scale(f.v, b)));
    };
    const float hue = hueForWorldAxisNormal(f.n);
    pushLine(v, p(-H, -H), p(H, -H), hue, 1.0f);
    pushLine(v, p(H, -H), p(H, H), hue, 1.0f);
    pushLine(v, p(H, H), p(-H, H), hue, 1.0f);
    pushLine(v, p(-H, H), p(-H, -H), hue, 1.0f);
    // A diagonal cross so a selected face is unmistakable.
    pushLine(v, p(-H, -H), p(H, H), hue, 1.0f);
    pushLine(v, p(-H, H), p(H, -H), hue, 1.0f);
}

bool sameChoice(const ChosenSupport& a, const ChosenSupport& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == ChosenSupport::Kind::WorldPlane) return a.plane == b.plane;
    if (a.kind == ChosenSupport::Kind::Face) return sameTopoRef(a.faceRef, b.faceRef);
    return true;
}

}  // namespace

void SupportChooser::begin(bool allowFaces) {
    active_ = true;
    allowFaces_ = allowFaces;
    hovered_ = ChosenSupport{};
    selected_ = ChosenSupport{};
    dirty_ = true;
    ++revision_;
}

void SupportChooser::cancel() {
    active_ = false;
    hovered_ = ChosenSupport{};
    selected_ = ChosenSupport{};
    dirty_ = true;
    ++revision_;
}

ChosenSupport SupportChooser::resolve(const CameraSnapshot& camera, float x, float y,
                                      int viewportWidth, int viewportHeight,
                                      const ConstructionScene& scene) const {
    ChosenSupport best;
    float bestDistance = 1e30f;

    // World planes: the nearest square the ray hits within its bounds.
    Ray ray;
    if (buildPickRay(camera, x, y, viewportWidth, viewportHeight, &ray)) {
        for (int i = 0; i < kWorkplaneCount; ++i) {
            Workplane plane;
            if (!workplaneFromIndex(i, &plane)) continue;
            const WorkplaneFrame frame = workplaneFrame(plane);
            Vec3 hit;
            if (!intersectRayPlane(ray, Vec3{0, 0, 0}, frame.normal, &hit)) {
                continue;
            }
            const SketchPoint uv = localToWorkplane(plane, hit);
            if (std::fabs(uv.u) > kSupportPlaneHalfMeters
                || std::fabs(uv.v) > kSupportPlaneHalfMeters) {
                continue;
            }
            const float d = std::sqrt(vec3Dot(vec3Sub(hit, camera.eye), vec3Sub(hit, camera.eye)));
            if (d < bestDistance) {
                bestDistance = d;
                best = ChosenSupport{};
                best.kind = ChosenSupport::Kind::WorldPlane;
                best.plane = plane;
            }
        }
    }

    // Faces of CAD bodies (existing project only), through the ordinary scene
    // pick so what is hittable is exactly what is drawn. A rendered triangle is
    // mapped to a semantic face; a curved side resolves but is not eligible.
    if (allowFaces_) {
        const SceneHit sceneHit =
            pickSceneSnapshot(camera, x, y, viewportWidth, viewportHeight, scene.snapshot());
        if (sceneHit.hit && sceneHit.triangleIndex >= 0) {
            const SceneObject* body = scene.findBody(sceneHit.objectId);
            if (body != nullptr && body->cadOrNull() != nullptr) {
                // The ranges of the mesh the body actually PUBLISHED -- its
                // cached regeneration -- so a triangle index from the pick
                // means the same triangle here, boolean result or prism.
                std::vector<CadFaceRange> ranges;
                std::shared_ptr<const CadBodyMesh> published;
                if (body->cadOrNull()->regenerated(&published) == CadStatus::Ok
                    && cadFaceRangesFromMesh(*published, &ranges) == CadStatus::Ok) {
                    const uint32_t firstIndex =
                        static_cast<uint32_t>(sceneHit.triangleIndex) * 3u;
                    for (const CadFaceRange& rg : ranges) {
                        if (firstIndex < rg.firstIndex || firstIndex >= rg.firstIndex + rg.indexCount) {
                            continue;
                        }
                        if (!rg.eligible) {
                            break;  // a cylindrical side: not a valid support
                        }
                        CadFace face;
                        if (resolveCadFeatureFace(body->cadOrNull()->state(), rg.featureId,
                                                  rg.token, &face)
                            != CadStatus::Ok) {
                            break;
                        }
                        Mat4 producerModel;
                        if (!scene.resolveWorldModel(body->objectId(), &producerModel)) {
                            break;
                        }
                        const float d = std::sqrt(vec3Dot(vec3Sub(sceneHit.position, camera.eye), vec3Sub(sceneHit.position, camera.eye)));
                        // A face nearer than the best plane wins; ties go to the
                        // face, which is the thing the user physically touched.
                        if (d <= bestDistance + 1e-3f) {
                            const Mat4 faceWorld = mat4Multiply(producerModel,
                                                                cadFaceFrameMatrix(face));
                            best = ChosenSupport{};
                            best.kind = ChosenSupport::Kind::Face;
                            best.plane = Workplane::XY;
                            best.faceRef.producerObjectId = body->objectId();
                            best.faceRef.producerLocalFeatureId = rg.featureId;
                            best.faceRef.face = rg.token;
                            best.faceRef.lineageToken = cadFeatureTopologySignature(
                                    body->cadOrNull()->state(), rg.featureId);
                            best.worldFrame = frameOf(faceWorld);
                        }
                        break;
                    }
                }
            }
        }
    }
    return best;
}

ChosenSupport SupportChooser::hover(const CameraSnapshot& camera, float x, float y,
                                    int viewportWidth, int viewportHeight,
                                    const ConstructionScene& scene) {
    const ChosenSupport at = resolve(camera, x, y, viewportWidth, viewportHeight, scene);
    if (!sameChoice(at, hovered_)) {
        hovered_ = at;
        dirty_ = true;
        ++revision_;
    }
    return at;
}

ChosenSupport SupportChooser::select(const CameraSnapshot& camera, float x, float y,
                                     int viewportWidth, int viewportHeight,
                                     const ConstructionScene& scene) {
    const ChosenSupport at = resolve(camera, x, y, viewportWidth, viewportHeight, scene);
    selected_ = at;
    hovered_ = at;
    dirty_ = true;
    ++revision_;
    return at;
}

std::shared_ptr<const SketchOverlay> SupportChooser::overlay() {
    if (dirty_ || !overlay_) {
        rebuildOverlay();
    }
    return overlay_;
}

void SupportChooser::rebuildOverlay() {
    auto built = std::make_shared<SketchOverlay>();
    built->revision = revision_;
    dirty_ = false;
    if (!active_) {
        overlay_ = built;
        return;
    }
    std::vector<GizmoVertex>& v = built->vertices;
    // Grid ranges reuse the sketch overlay's styles so the renderer draws them
    // with the same pipeline and weights.
    SketchOverlayRange planes;
    planes.firstVertex = 0;
    for (int i = 0; i < kWorkplaneCount; ++i) {
        Workplane plane;
        if (!workplaneFromIndex(i, &plane)) continue;
        const bool emphasised =
            (hovered_.kind == ChosenSupport::Kind::WorldPlane && hovered_.plane == plane)
            || (selected_.kind == ChosenSupport::Kind::WorldPlane && selected_.plane == plane);
        drawPlaneTarget(&v, plane, emphasised);
    }
    planes.vertexCount = static_cast<uint32_t>(v.size());
    planes.style = SketchOverlayStyle::GridMinor;
    built->ranges.push_back(planes);

    // The hovered/selected face outline, if any, in its own emphasised range.
    const ChosenSupport& face = selected_.kind == ChosenSupport::Kind::Face ? selected_ : hovered_;
    if (face.kind == ChosenSupport::Kind::Face) {
        SketchOverlayRange range;
        range.firstVertex = static_cast<uint32_t>(v.size());
        drawFaceTarget(&v, face.worldFrame);
        range.vertexCount = static_cast<uint32_t>(v.size()) - range.firstVertex;
        range.style = SketchOverlayStyle::Entities;
        built->ranges.push_back(range);
    }
    overlay_ = built;
}

SupportChooser& supportChooser() {
    static SupportChooser chooser;
    return chooser;
}

}  // namespace forgeshape
