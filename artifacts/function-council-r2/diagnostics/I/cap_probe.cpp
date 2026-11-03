// FC2 seat I diagnostic: does a suite Recorder silently truncate past the device cap?
#include <cstdio>
#include <vector>
#include "forgeshape_project_selftest.h"
#include "forgeshape_camera_selftest.h"
using namespace forgeshape;
int main() {
    const int caps[] = {8192, 256, 255, 200};
    for (int cap : caps) {
        std::vector<ProjectSelfTestResult> r(cap);
        int n = runProjectSelfTests(r.data(), cap);
        int failed = 0; for (int i = 0; i < n; ++i) if (!r[i].passed) ++failed;
        std::printf("PROJECT cap=%d recorded=%d failed=%d verdict=%s\n", cap, n, failed, failed ? "FAIL" : "OK");
        if (cap == 8192) std::printf("PROJECT last_check=%s\n", n ? r[n-1].name : "-");
        if (cap == 256) std::printf("PROJECT last_check_at_device_cap=%s\n", n ? r[n-1].name : "-");
    }
    std::vector<CameraSelfTestResult> c(8192);
    std::printf("CAMERA host=%d device_cap=128\n", runCameraSelfTests(c.data(), 8192));
}
