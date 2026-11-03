#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;
int main(int, char** argv) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // locate CADB section
    size_t off = 28; uint32_t n = b[16] | (b[17]<<8);
    size_t hdr = 0, pay = 0; uint64_t len = 0;
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t l = 0; for (int k = 7; k >= 0; --k) l = (l << 8) | b[off + 8 + k];
        if (b[off]=='C'&&b[off+1]=='A'&&b[off+2]=='D'&&b[off+3]=='B') { hdr = off; pay = off + 24; len = l; }
        off += 24 + l;
    }
    ProjectDocument d0; std::printf("original decode=%s\n", projectCodecStatusName(decodeProject(b.data(), b.size(), &d0)));
    const auto& s0 = d0.cad.bodies[1].state;
    std::printf("original body2 faceSupport kind=%d entity=%u localIndex=%u lineage=%llx\n", (int)cadBaseSketch(s0).faceSupport.face.kind, cadBaseSketch(s0).faceSupport.face.edgeEntityId, cadBaseSketch(s0).faceSupport.face.edgeLocalIndex, (unsigned long long)cadBaseSketch(s0).faceSupport.lineageToken);
    std::vector<uint8_t> m = b;
    m[pay + 101] = 0x08;  // high bytes of faceEdgeLocalIndex of body 2 (offset computed from §7c layout)
    uint32_t c = crc32IsoHdlc(&m[pay], (size_t)len);
    for (int k = 0; k < 4; ++k) m[hdr + 16 + k] = (c >> (8*k)) & 0xff;
    ProjectDocument d1; ProjectCodecStatus w = decodeProject(m.data(), m.size(), &d1);
    std::printf("mutated decode=%s\n", projectCodecStatusName(w));
    if (w != ProjectCodecStatus::Ok) return 0;
    const auto& s1 = d1.cad.bodies[1].state;
    std::printf("mutated body2 faceSupport entity=%u localIndex=%u (0x%08x)\n", cadBaseSketch(s1).faceSupport.face.edgeEntityId, cadBaseSketch(s1).faceSupport.face.edgeLocalIndex, cadBaseSketch(s1).faceSupport.face.edgeLocalIndex);
    ConstructionScene a(NoProjectTag{}), bb(NoProjectTag{}); SculptSession sa, sb; ConstructionHistory ha(a), hb(bb);
    std::printf("load original=%s mutated=%s\n", projectCodecStatusName(loadProjectDocument(d0, a, sa, ha)), projectCodecStatusName(loadProjectDocument(d1, bb, sb, hb)));
    std::printf("fingerprint original=%016llx mutated=%016llx\n", (unsigned long long)projectSemanticFingerprint(a, d0.kind), (unsigned long long)projectSemanticFingerprint(bb, d1.kind));
    ProjectCodecStatus e; auto ra = encodeProjectV1(captureProjectDocument(a, d0.kind), &e); auto rb = encodeProjectV1(captureProjectDocument(bb, d1.kind), &e);
    std::printf("resave original==file:%d mutated==mutatedfile:%d resaves_equal:%d\n", ra == b, rb == m, ra == rb);
    std::printf("sameProjectDocument(d0,d1)=%d\n", sameProjectDocument(d0, d1));
    return 0;
}
