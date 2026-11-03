// FC2 Seat B diagnostic 3: adversarial Mirror at gimbal / wrapped angles, Relative Scale underflow,
// dimension on a turned body with a positive anchor.
#include <cstdio>
#include <cmath>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_body_mirror.h"
#include "forgeshape_body_dimensions.h"
#include "forgeshape_transform.h"
using namespace forgeshape;

static Vec3 apply(const Mat4& m, Vec3 p) {
    return Vec3{m.m[0]*p.x + m.m[4]*p.y + m.m[8]*p.z + m.m[12],
                m.m[1]*p.x + m.m[5]*p.y + m.m[9]*p.z + m.m[13],
                m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14]};
}
int main() {
    const TransformValues poses[] = {
        {1.5, -2.0, 0.25, 0.0, 90.0, 0.0, 1.0, 2.0, 0.5},      // gimbal
        {0.0, 0.0, 0.0, 720.5, -270.0, 1080.25, 3.0, 0.2, 1.7}, // wrapped angles
        {-4.0, 7.0, 3.0, 89.9999, 45.0, -179.999, 1e-3, 1e3, 1.0},
    };
    double worst = 0.0;
    for (const auto& s : poses) for (int pl = 0; pl < 3; ++pl) {
        MirrorPlane plane; mirrorPlaneFromIndex(pl, &plane);
        TransformValues m{}; MirrorStatus st = mirrorPlacement(s, plane, &m);
        if (st != MirrorStatus::Ok) { std::printf("mirror status %s\n", mirrorStatusName(st)); continue; }
        ConstructionTransform ts; ts.setValues(s); ConstructionTransform tm; TransformValidation why; 
        TransformUpdateStatus u = tm.setValues(m, &why);
        if (u == TransformUpdateStatus::Rejected) { std::printf("mirrored placement rejected %s\n", transformValidationName(why)); continue; }
        const int ax = mirrorReflectedAxis(plane);
        const Vec3 qs[] = {{0.3f,0.7f,-0.2f},{-1,0.5f,2},{0.9f,-0.9f,0.1f}};
        for (Vec3 q : qs) {
            Vec3 lhs = apply(tm.modelMatrix(), q);
            Vec3 rhs = apply(ts.modelMatrix(), Vec3{-q.x, q.y, q.z});
            float* r = &rhs.x; r[ax] = -r[ax];
            double d = std::fabs(lhs.x-rhs.x)+std::fabs(lhs.y-rhs.y)+std::fabs(lhs.z-rhs.z);
            double mag = std::fabs(rhs.x)+std::fabs(rhs.y)+std::fabs(rhs.z)+1.0;
            if (d/mag > worst) worst = d/mag;
        }
        std::printf("pose(r=%.4g,%.4g,%.4g s=%.3g,%.3g,%.3g) plane=%s -> scale=(%.6g,%.6g,%.6g) rot=(%.4f,%.4f,%.4f)\n",
            s.rotationX,s.rotationY,s.rotationZ,s.scaleX,s.scaleY,s.scaleZ, mirrorPlaneName(plane), m.scaleX,m.scaleY,m.scaleZ, m.rotationX,m.rotationY,m.rotationZ);
    }
    std::printf("MIRROR identity worst relative error = %.3g\n", worst);

    // Relative Scale underflow and dimension on a turned body
    ConstructionScene scene; ConstructionHistory history(scene);
    publishSceneObject(scene.activeBody());
    TransformValues t = scene.activeBody().transform().values();
    t.rotationX = 30; t.rotationY = -40; t.rotationZ = 55; t.positionX = 1; t.scaleX = 2e-6;
    scene.activeBody().transform().setValues(t);
    BodySizeResult r = applyBodyRelativeScale(1, 0.4, 1.0, 1.0, scene, history);  // 8e-7 < kMinScaleFactor
    std::printf("relscale underflow -> %s resize=%s scaleX=%.3g undo=%zu\n", bodySizeStatusName(r.status), resizeStatusName(r.resize), scene.activeBody().transform().values().scaleX, history.undoDepth());
    r = applyBodyRelativeScale(1, -1.0, 1.0, 1.0, scene, history);
    std::printf("relscale negative -> %s resize=%s\n", bodySizeStatusName(r.status), resizeStatusName(r.resize));
    t.scaleX = 1.0; scene.activeBody().transform().setValues(t);
    LocalBounds b; BodyDimensions d0; sceneBodyDimensions(1, scene, &b, &d0);
    // world position of the negative-X face centre before
    Mat4 m0 = scene.activeBody().transform().modelMatrix();
    Vec3 negFace0 = apply(m0, Vec3{(float)b.minX, 0, 0});
    r = applyBodyDimension(1, 0, 5.0, ResizeAnchor::NegativeSide, scene, history);
    Mat4 m1 = scene.activeBody().transform().modelMatrix();
    Vec3 negFace1 = apply(m1, Vec3{(float)b.minX, 0, 0});
    BodyDimensions d1; sceneBodyDimensions(1, scene, nullptr, &d1);
    std::printf("dimension X=5 anchor Negative on turned body -> %s dimX=%.6f negFaceDrift=%.3g undo=%zu\n",
        bodySizeStatusName(r.status), d1.x, std::fabs(negFace1.x-negFace0.x)+std::fabs(negFace1.y-negFace0.y)+std::fabs(negFace1.z-negFace0.z), history.undoDepth());
    return 0;
}
