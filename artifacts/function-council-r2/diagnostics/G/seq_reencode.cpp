// seat G diagnostic: load fixture A, then fixture B, then encode; write B's re-encode.
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
static std::vector<uint8_t> rd(const char* p){std::ifstream in(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
static ProjectKind load(const std::vector<uint8_t>& b){ProjectDocument d; decodeProject(b.data(),b.size(),&d); ProjectLoadReport r; ProjectCodecStatus s=loadProjectDocument(d,constructionScene(),sculptSession(),constructionHistory(),&r); std::printf("load=%s kind=%d bodies=%d\n",projectCodecStatusName(s),(int)r.kind,r.bodies); return r.kind;}
int main(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) load(rd(argv[i]));
    std::vector<uint8_t> b = rd(argv[argc - 1]);
    ProjectKind k = load(b);
    ProjectCodecStatus why;
    std::vector<uint8_t> e = encodeProjectV1(captureProjectDocument(constructionScene(), k), &why);
    std::printf("final reencode=%s\n", e == b ? "identical" : "different");
    std::ofstream("seq.re.forge", std::ios::binary).write((const char*)e.data(), e.size());
}
