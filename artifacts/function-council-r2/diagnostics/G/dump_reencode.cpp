// seat G diagnostic: write the load->capture->encode bytes of one fixture to a file.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
using namespace forgeshape;
int main(int argc, char** argv) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ProjectDocument doc;
    if (decodeProject(bytes.data(), bytes.size(), &doc) != ProjectCodecStatus::Ok) return 2;
    // Path 1: decode -> encode directly (no scene).
    ProjectCodecStatus why = ProjectCodecStatus::Ok;
    std::vector<uint8_t> direct = encodeProjectV1(doc, &why);
    ProjectLoadReport report;
    loadProjectDocument(doc, constructionScene(), sculptSession(), constructionHistory(), &report);
    std::vector<uint8_t> viaScene = encodeProjectV1(captureProjectDocument(constructionScene(), report.kind), &why);
    std::printf("direct=%s viaScene=%s kind=%d\n", direct == bytes ? "identical" : "different",
                viaScene == bytes ? "identical" : "different", (int)report.kind);
    std::ofstream(argv[2], std::ios::binary).write((const char*)viaScene.data(), viaScene.size());
    return 0;
}
