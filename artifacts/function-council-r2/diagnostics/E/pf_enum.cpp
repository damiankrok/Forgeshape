#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_cad_body.h"
using namespace forgeshape;
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        ProjectDocument d; if (decodeProject(b.data(), b.size(), &d) != ProjectCodecStatus::Ok) continue;
        for (auto& body : d.cad.bodies)
            std::printf("%s body=%llu planar=%d later=%zu sketches=%zu revolve=%d\n", argv[i], (unsigned long long)body.objectId, cadBodyStateUsesPlanarFaces(body.state), body.state.laterFeatures.size(), body.state.sketches.size(), cadBodyStateUsesRevolve(body.state));
    }
}
