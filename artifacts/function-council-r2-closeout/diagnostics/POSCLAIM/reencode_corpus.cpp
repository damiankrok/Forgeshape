// POSCLAIM closeout diagnostic (audit only): decode every committed .forge
// fixture with THIS tree's codec, and for each Ok one re-encode it with
// encodeProjectV1 and compare the bytes to the committed file.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
int main(int argc, char** argv) {
    using namespace forgeshape;
    int ok = 0, identical = 0, differ = 0, refused = 0;
    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
        const char* name = argv[i];
        for (const char* p = argv[i]; *p; ++p) if (*p == '/') name = p + 1;
        ProjectDocument doc;
        const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &doc);
        if (why != ProjectCodecStatus::Ok) {
            ++refused;
            std::printf("%s REFUSED %s\n", name, projectCodecStatusName(why));
            continue;
        }
        ++ok;
        ProjectCodecStatus encWhy = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> again = encodeProjectV1(doc, &encWhy);
        const bool same = encWhy == ProjectCodecStatus::Ok && again == bytes;
        if (same) ++identical; else ++differ;
        std::printf("%s Ok reencode=%s encode_status=%s in=%zu out=%zu\n", name,
                    same ? "IDENTICAL" : "DIFFERENT", projectCodecStatusName(encWhy),
                    bytes.size(), again.size());
    }
    std::printf("SUMMARY files=%d ok=%d refused=%d reencode_identical=%d reencode_different=%d\n",
                argc - 1, ok, refused, identical, differ);
    return differ ? 1 : 0;
}
