// CR-06 closeout diagnostic. NOT part of the build. Links against the host
// objects that scripts/host-native-selftests.sh builds from the audited tree
// (baseline 70a6c14). Two parts:
//   (A) what the three canvas editors' parsed double does to the model, by
//       calling the SAME native domain methods their JNI entry points call
//       (sketchSetExtrudeSide -> SketchSession::setExtrudeSide,
//        sketchApplyLineLength -> SketchSession::applyLineLength,
//        applyBodyDimension -> forgeshape::applyBodyDimension), with 25.0
//       (what "2,5" becomes after the ',' is filtered) and 2.5 (what was meant);
//   (B) what JNI beginSketchView() does to the camera before and after the
//       navigator's Flip and +-90: it frames from SketchSession::frame(),
//       copied verbatim from forgeshape_jni.cpp:313-320, versus the same
//       framing from SketchSession::viewFrame() (control).
#include <cmath>
#include <cstdio>
#include <cstring>
#include "forgeshape_body_dimensions.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_construction.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"
using namespace forgeshape;

static void printSnap(const char* tag, const CameraSnapshot& s) {
    std::printf("%s eye=(%.4f,%.4f,%.4f) target=(%.4f,%.4f,%.4f)\n", tag, s.eye.x, s.eye.y,
                s.eye.z, s.target.x, s.target.y, s.target.z);
    std::printf("%s view=[", tag);
    for (int i = 0; i < 16; ++i) std::printf("%s%.4f", i ? "," : "", s.view.m[i]);
    std::printf("]\n");
}
static void printFrame(const char* tag, const SketchFrame& f) {
    std::printf("%s u=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) n=(%.0f,%.0f,%.0f)\n", tag, f.u.x, f.u.y,
                f.u.z, f.v.x, f.v.y, f.v.z, f.n.x, f.n.y, f.n.z);
}
static bool sameView(const CameraSnapshot& a, const CameraSnapshot& b) {
    return std::memcmp(&a.view, &b.view, sizeof(a.view)) == 0;
}

int main() {
    // ---------------- (A) comma: parsed value -> model ----------------
    const double typedValues[3] = {25.0, 2.5, 10005.0};  // "2,5" filtered, "2.5", "1 000,5" filtered
    for (double meters : typedValues) {
        // A1: canvas extrude value (CadExtrudeCanvasView.submitField ->
        // EditorWorkspaceView.onExtrudeSideEntered -> sketchSetExtrudeSide).
        {
            SketchSession s;
            s.begin(Workplane::XY);
            SketchEntityId rect = kNoSketchEntity;
            addSketchEntity(const_cast<CadSketch*>(&s.sketch()),
                            SketchRectangle{SketchPoint{0.0, 0.0}, 1.0, 1.0}, &rect);
            const CadStatus fin = s.finish();
            const double before = s.extrude().depth;
            const CadStatus st = s.setExtrudeSide(true, meters);
            std::printf("A1 extrude finish=%s depth_before=%.6g setExtrudeSide(true,%.6g)=%s depth_after=%.6g\n",
                        cadStatusName(fin), before, meters, cadStatusName(st), s.extrude().depth);
            s.cancel();
        }
        // A2: sketch line dimension (SketchDimensionLabelView.submit ->
        // onSketchLineLengthEntered -> sketchApplyLineLength).
        {
            SketchSession s;
            s.begin(Workplane::XY);
            SketchEntityId line = kNoSketchEntity;
            addSketchEntity(const_cast<CadSketch*>(&s.sketch()),
                            SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{3.0, 4.0}}, &line);
            s.select(line);
            const CadStatus st = s.applyLineLength(line, meters);
            const SketchLine* l = findSketchEntity(s.sketch(), line)->line();
            const double len = std::hypot(l->end.u - l->start.u, l->end.v - l->start.v);
            std::printf("A2 line applyLineLength(%.6g)=%s end=(%.6g,%.6g) length=%.6g\n", meters,
                        cadStatusName(st), l->end.u, l->end.v, len);
            s.cancel();
        }
        // A3: body dimension (BodyDimensionLabelsView.submit ->
        // onBodyDimensionEntered -> applyBodyDimension), default seeded body.
        {
            ConstructionScene scene;
            ConstructionHistory history{scene};
            publishConstructionObject(scene.activeBody().construction(),
                                      scene.activeBody().meshStore());
            const TransformValues before = scene.activeBody().transform().values();
            const BodySizeResult r = applyBodyDimension(scene.activeBody().objectId(), 0, meters,
                                                        ResizeAnchor::Center, scene, history);
            const TransformValues after = scene.activeBody().transform().values();
            std::printf("A3 body applyBodyDimension(axis=0,%.6g) status=%d scaleX %.6g -> %.6g undoDepth=%zu\n",
                        meters, static_cast<int>(r.status), before.scaleX, after.scaleX,
                        static_cast<size_t>(history.undoDepth()));
        }
    }

    // ---------------- (B) navigator: camera before/after Flip and +-90 ----------------
    {
        SketchSession s;
        s.begin(Workplane::XY);
        CameraController cam;
        cam.setViewport(1080, 2400);
        // Verbatim the body of JNI beginSketchView() (forgeshape_jni.cpp:313-320),
        // minus the pose save: it reads frame(), the AUTHORING frame.
        auto beginSketchView = [&]() {
            const SketchFrame& f = s.frame();
            cam.frameSketchView(f.origin, f.u, f.v, f.n);
            return cam.snapshot();
        };
        auto frameFromViewFrame = [&]() {
            const SketchFrame f = s.viewFrame();
            CameraController c2;
            c2.setViewport(1080, 2400);
            c2.frameSketchView(f.origin, f.u, f.v, f.n);
            return c2.snapshot();
        };
        const CameraSnapshot base = beginSketchView();
        printFrame("B0 frame()    ", s.frame());
        printFrame("B0 viewFrame()", s.viewFrame());
        printSnap("B0 camera(beginSketchView)", base);

        s.setViewFlipped(true);  // JNI sketchSetViewFlipped(true) -> setViewFlipped; beginSketchView()
        const CameraSnapshot flipped = beginSketchView();
        printFrame("B1 frame()    ", s.frame());
        printFrame("B1 viewFrame()", s.viewFrame());
        printSnap("B1 camera(beginSketchView)", flipped);
        printSnap("B1 control camera(viewFrame)", frameFromViewFrame());
        std::printf("B1 viewFlipped=%d camera_view_identical_to_B0=%d control_differs_from_B0=%d\n",
                    s.viewFlipped(), sameView(base, flipped), !sameView(base, frameFromViewFrame()));

        s.setViewFlipped(false);
        s.rotateView(+1);  // JNI sketchRotateView(+1)
        const CameraSnapshot plus90 = beginSketchView();
        printFrame("B2 viewFrame()", s.viewFrame());
        printSnap("B2 camera(beginSketchView)", plus90);
        printSnap("B2 control camera(viewFrame)", frameFromViewFrame());
        std::printf("B2 quarterTurns=%d camera_view_identical_to_B0=%d control_differs_from_B0=%d\n",
                    s.viewQuarterTurns(), sameView(base, plus90), !sameView(base, frameFromViewFrame()));

        s.rotateView(-2);  // net -1 = 3 quarter turns
        const CameraSnapshot minus90 = beginSketchView();
        printFrame("B3 viewFrame()", s.viewFrame());
        std::printf("B3 quarterTurns=%d camera_view_identical_to_B0=%d control_differs_from_B0=%d\n",
                    s.viewQuarterTurns(), sameView(base, minus90), !sameView(base, frameFromViewFrame()));
        s.cancel();
    }
    return 0;
}
