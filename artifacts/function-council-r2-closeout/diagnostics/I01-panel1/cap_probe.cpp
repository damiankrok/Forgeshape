// Panel check I-01: run PROJECT (and CAMERA) at several result caps and show
// what the device log line would say. Mirrors forgeshape_jni.cpp:821-874 verdict logic.
#include <cstdio>
#include <vector>
#include "forgeshape_project_selftest.h"
#include "forgeshape_camera_selftest.h"
using namespace forgeshape;
template <typename R, typename F>
static void probe(const char* name, F run, int cap) {
    std::vector<R> r(cap, R{});
    int count = run(r.data(), cap);
    int failed = 0;
    for (int i = 0; i < count; ++i) if (!r[i].passed) ++failed;
    std::printf("%s cap=%d recorded=%d failed=%d device_line=%s last=%s\n", name, cap, count, failed,
                failed ? "SELFTEST_FAIL" : "SELFTEST_OK",
                count ? (r[count-1].name ? r[count-1].name : "?") : "-");
}
int main() {
    for (int cap : {8192, 257, 256, 255, 200})
        probe<ProjectSelfTestResult>("PROJECT", runProjectSelfTests, cap);
    for (int cap : {8192, 128, 119, 118})
        probe<CameraSelfTestResult>("CAMERA", runCameraSelfTests, cap);
    return 0;
}
