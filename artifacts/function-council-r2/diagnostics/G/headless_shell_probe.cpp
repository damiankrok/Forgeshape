// FUNCTION-COUNCIL-R2 seat G: a HEADLESS "desktop shell" probe.
// Diagnostic only; never part of the product build. It drives the platform-neutral
// domain the way forgeshape_jni.cpp does (process-scoped accessors, the same load /
// capture / encode / pick / camera calls) with no Android, JNI or Vulkan type, to show
// what a non-Android shell can reuse today and which numbers are pixel-unit assumptions.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_selection.h"

using namespace forgeshape;

static std::vector<uint8_t> readAll(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main(int argc, char** argv) {
    int failures = 0;
    for (int i = 1; i < argc; ++i) {
        const std::vector<uint8_t> bytes = readAll(argv[i]);
        ProjectDocument doc;
        ProjectCodecStatus st = decodeProject(bytes.data(), bytes.size(), &doc);
        if (st != ProjectCodecStatus::Ok) {
            std::printf("PROBE %s decode=%s\n", argv[i], projectCodecStatusName(st));
            continue;
        }
        ProjectLoadReport report;
        st = loadProjectDocument(doc, constructionScene(), sculptSession(), constructionHistory(), &report);
        const uint64_t fp = projectSemanticFingerprint(constructionScene(), report.kind);
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const ProjectDocument captured = captureProjectDocument(constructionScene(), report.kind);
        const std::vector<uint8_t> again = encodeProjectV1(captured, &why);
        const bool identical = again == bytes;
        // Camera + pick at a desktop-like 1920x1080 surface.
        CameraController cam;
        cam.setViewport(1920, 1080);
        const float yaw0 = cam.snapshot().yaw;
        SceneHit centre = pickScene(cam.snapshot(), 960.0f, 540.0f, 1920, 1080);
        // A MOUSE drag of 100 physical pixels, through the platform-neutral input seam.
        TouchPointer p{0, 900.0f, 540.0f};
        p.toolType = PointerToolType::Mouse;
        cam.onTouch(TouchAction::Down, 0, &p, 1);
        p.x = 1000.0f;
        cam.onTouch(TouchAction::Move, -1, &p, 1);
        cam.onTouch(TouchAction::Up, 0, &p, 1);
        const float yawDelta = cam.snapshot().yaw - yaw0;
        std::printf("PROBE %s load=%s bodies=%d fingerprint=%016llx reencode=%s bytes=%zu/%zu "
                    "centre_hit=%d centre_obj=%llu mouse_100px_yaw_delta_rad=%.4f\n",
                    argv[i], projectCodecStatusName(st), report.bodies,
                    static_cast<unsigned long long>(fp), identical ? "identical" : "different",
                    again.size(), bytes.size(), centre.hit ? 1 : 0,
                    static_cast<unsigned long long>(centre.objectId), yawDelta);
        if (st != ProjectCodecStatus::Ok) ++failures;
    }
    return failures ? 1 : 0;
}
