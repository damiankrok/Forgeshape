#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_cad_body.h"
#include "forgeshape_project_document.h"
int main(int argc, char** argv) {
    using namespace forgeshape;
    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
        ProjectDocument doc;
        const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &doc);
        const char* name = argv[i];
        for (const char* p = argv[i]; *p != '\0'; ++p) {
            if (*p == '/') name = p + 1;
        }
        if (why != ProjectCodecStatus::Ok) {
            std::printf("%s %s\n", name, projectCodecStatusName(why));
            continue;
        }
        int regenerated = 0;
        for (const auto& body : doc.cad.bodies) {
            CadBodyMesh mesh;
            if (regenerateCadBody(body.state, &mesh) == CadStatus::Ok) ++regenerated;
        }
        std::printf("%s Ok cad_bodies=%d regenerated=%d\n", name,
                    static_cast<int>(doc.cad.bodies.size()), regenerated);
    }
    return 0;
}
