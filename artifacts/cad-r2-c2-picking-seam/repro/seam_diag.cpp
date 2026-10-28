// CAD-R2-C2 diagnosis only (never committed as product): rebuild the C1_CH_04
// scene, aim at the rectangular pocket floor centre with the far orthographic
// lookAlong camera, and dump every layer of the pick.
#include <cmath>
#include <cstdio>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_feature.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_camera.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_mesh.h"
#include "forgeshape_picking.h"
#include "forgeshape_scene.h"
#include "forgeshape_selection.h"
#include "forgeshape_sketch.h"
#include "forgeshape_support_chooser.h"

using namespace forgeshape;

static SketchEntityId add(CadSketch* s, SketchEntity::Payload p) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(s, std::move(p), &id);
    return id;
}
static SketchRectangle rect(double cu, double cv, double w, double h) {
    SketchRectangle r; r.center = SketchPoint{cu, cv}; r.width = w; r.height = h; return r;
}
static ExtrudeFeature oneSide(SketchEntityId p, double d, ExtrudeDirection dir = ExtrudeDirection::AlongNormal) {
    ExtrudeFeature e; e.profileEntityId = p; e.depth = d; e.direction = dir; return e;
}
static CadFeatureSupport capFar(const CadBodyState& s) {
    CadFeatureSupport f; f.featureId = kCadFeatureId; f.face.kind = CadFaceKind::CapFar;
    f.lineageToken = cadFeatureTopologySignature(s, kCadFeatureId); return f;
}

int main(int argc, char** argv) {
    // The shop body (r2 selftest shopState) without the round pocket: the floor
    // triangulation is what matters.
    CadSketch base; const SketchEntityId b = add(&base, rect(0, 0, 2, 2));
    CadBodyState st = makeCadBodyState(base, oneSide(b, 1.0));
    CadSketch boss; const SketchEntityId bo = add(&boss, rect(-0.5, 0.5, 0.6, 0.6));
    appendCadLaterFeatureWithSketch(&st, CadFeatureOperation::Add, capFar(st), std::move(boss), oneSide(bo, 0.5));
    CadSketch pocket; const SketchEntityId po = add(&pocket, rect(0.5, -0.5, 0.6, 0.6));
    appendCadLaterFeatureWithSketch(&st, CadFeatureOperation::Cut, capFar(st), std::move(pocket),
                                    oneSide(po, 0.4, ExtrudeDirection::AgainstNormal));
    ConstructionScene scene{NoProjectTag{}};
    SceneObject* body = scene.addCadBody(st);
    if (!body) { std::printf("no body\n"); return 2; }
    publishSceneObject(*body);

    const float dist = argc > 1 ? std::atof(argv[1]) : -1.0f;  // -1 = default lookAlong
    const Vec3 target{0.5f, -0.5f, 0.6f};
    Vec3 n = vec3Normalize(Vec3{0.2f, 0.0f, 1.0f});
    Vec3 seed{0, 1, 0};
    Vec3 u = vec3Normalize(vec3Sub(seed, vec3Scale(n, vec3Dot(seed, n))));
    Vec3 v = vec3Cross(n, u);
    CameraController cam; cam.setViewport(1000, 1000);
    cam.frameSketchView(target, u, v, n);
    const CameraSnapshot snap = cam.snapshot();
    float px, py;
    projectWorldToScreen(snap, target, 1000, 1000, &px, &py);
    std::printf("eye=(%.4f,%.4f,%.4f) |eye-target|=%.3f pixel=(%.6f,%.6f) proj=%d\n", snap.eye.x, snap.eye.y,
                snap.eye.z, std::sqrt(vec3Dot(vec3Sub(snap.eye, target), vec3Sub(snap.eye, target))), px, py,
                (int)snap.projection);
    Ray ray; buildPickRay(snap, px, py, 1000, 1000, &ray);
    if (dist > 0) ray.origin = vec3Sub(target, vec3Scale(ray.direction, dist));  // near-eye control: same direction, origin dist metres away
    std::printf("ray o=(%.7f,%.7f,%.7f) d=(%.8f,%.8f,%.8f)\n", ray.origin.x, ray.origin.y, ray.origin.z,
                ray.direction.x, ray.direction.y, ray.direction.z);
    const SceneSnapshot ss = scene.snapshot();
    const SceneHit hit = pickSceneSnapshot(snap, px, py, 1000, 1000, ss);
    std::printf("sceneHit=%d tri=%d t=%.6f pos=(%.6f,%.6f,%.6f)\n", hit.hit, hit.triangleIndex, hit.distance,
                hit.position.x, hit.position.y, hit.position.z);
    // Per-triangle dump for triangles on z = 0.6 near the target.
    const RuntimeMeshPtr& mesh = ss[0].mesh;
    const TriangleMeshView tv = mesh->triangleView();
    Ray local; transformRayToLocal(ray, ss[0].inverseModel, &local);
    for (uint32_t t = 0; t < tv.indexCount / 3; ++t) {
        Vec3 p[3];
        for (int k = 0; k < 3; ++k) {
            const float* f = reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(tv.positions) +
                                                            tv.positionStride * tv.indices[t * 3 + k]);
            p[k] = Vec3{f[0], f[1], f[2]};
        }
        if (std::fabs(p[0].z - 0.6f) > 1e-4f || std::fabs(p[1].z - 0.6f) > 1e-4f || std::fabs(p[2].z - 0.6f) > 1e-4f) continue;
        if (std::fabs(p[0].x - 0.5f) > 0.31f || std::fabs(p[0].y + 0.5f) > 0.31f) continue;
        // float32 Moller-Trumbore exactly as the product
        const Vec3 e1 = vec3Sub(p[1], p[0]), e2 = vec3Sub(p[2], p[0]);
        const Vec3 pp = vec3Cross(local.direction, e2);
        const float det = vec3Dot(e1, pp);
        const float inv = 1.0f / det;
        const Vec3 s = vec3Sub(local.origin, p[0]);
        const float uu = vec3Dot(s, pp) * inv;
        const Vec3 q = vec3Cross(s, e1);
        const float vv = vec3Dot(local.direction, q) * inv;
        const float tt = vec3Dot(e2, q) * inv;
        // double reference
        double D[3][3], O[3], Dd[3];
        for (int k = 0; k < 3; ++k) { D[k][0] = p[k].x; D[k][1] = p[k].y; D[k][2] = p[k].z; }
        O[0] = local.origin.x; O[1] = local.origin.y; O[2] = local.origin.z;
        Dd[0] = local.direction.x; Dd[1] = local.direction.y; Dd[2] = local.direction.z;
        double E1[3], E2[3], P[3], S[3], Q[3];
        for (int k = 0; k < 3; ++k) { E1[k] = D[1][k] - D[0][k]; E2[k] = D[2][k] - D[0][k]; S[k] = O[k] - D[0][k]; }
        auto cr = [](const double* a, const double* b, double* o) { o[0] = a[1]*b[2]-a[2]*b[1]; o[1] = a[2]*b[0]-a[0]*b[2]; o[2] = a[0]*b[1]-a[1]*b[0]; };
        auto dt = [](const double* a, const double* b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; };
        cr(Dd, E2, P); double ddet = dt(E1, P); cr(S, E1, Q);
        const double du = dt(S, P) / ddet, dv = dt(Dd, Q) / ddet, dtt = dt(E2, Q) / ddet;
        float tHit = 0; const bool hitF = intersectRayTriangle(local, p[0], p[1], p[2], true, &tHit);
        std::printf("tri %u v0=(%.3f,%.3f) v1=(%.3f,%.3f) v2=(%.3f,%.3f) det=%.6g  f32 u=%.9g v=%.9g u+v=%.9g t=%.6f | f64 u=%.9g v=%.9g u+v=%.9g t=%.6f | product=%d\n",
                    t, p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y, det, uu, vv, uu + vv, tt, du, dv, du + dv, dtt, hitF);
    }
    // Chooser layer.
    SupportChooser& ch = supportChooser(); ch.begin(true);
    const ChosenSupport c = ch.select(snap, px, py, 1000, 1000, scene); ch.cancel();
    std::printf("chooser kind=%d (0 none 1 plane 2 face) plane=%d feature=%u facekind=%d\n", (int)c.kind, (int)c.plane,
                c.faceRef.producerLocalFeatureId, (int)c.faceRef.face.kind);
    return hit.hit ? 0 : 1;
}
