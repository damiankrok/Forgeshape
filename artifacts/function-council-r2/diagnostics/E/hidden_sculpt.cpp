#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;
int main(int, char** argv) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ProjectDocument d; std::printf("decode=%s kind=%s\n", projectCodecStatusName(decodeProject(b.data(), b.size(), &d)), projectKindName(d.kind));
    for (auto& p : d.scene.bodies) if (p.objectId == d.scene.activeObjectId) p.visible = false;
    ProjectCodecStatus e; std::vector<uint8_t> bytes = encodeProjectV1(d, &e);
    std::printf("encode hidden-active sculpt document: %s bytes=%zu\n", projectCodecStatusName(e), bytes.size());
    ProjectDocument back; std::printf("decode=%s\n", projectCodecStatusName(decodeProject(bytes.data(), bytes.size(), &back)));
    ConstructionScene scene(NoProjectTag{}); SculptSession s; ConstructionHistory h(scene);
    std::printf("load=%s\n", projectCodecStatusName(loadProjectDocument(back, scene, s, h)));
    std::printf("inSculptMode=%d activeVisible=%d\n", s.inSculptMode() ? 1 : 0, scene.activeBody().visible() ? 1 : 0);
}
