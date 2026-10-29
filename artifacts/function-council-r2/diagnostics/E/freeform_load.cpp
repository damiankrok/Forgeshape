#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
#include "forgeshape_freeform_session.h"
using namespace forgeshape;
static ProjectDocument load(const char* p) {
    std::ifstream in(p, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ProjectDocument d; std::printf("decode %s = %s\n", p, projectCodecStatusName(decodeProject(b.data(), b.size(), &d)));
    return d;
}
int main(int, char** argv) {
    ConstructionScene scene(NoProjectTag{}); SculptSession s; ConstructionHistory h(scene);
    FreeformEditSession ff(scene, h);
    ProjectDocument a = load(argv[1]), b = load(argv[2]);
    std::printf("load A=%s active=%llu\n", projectCodecStatusName(loadProjectDocument(a, scene, s, h)), (unsigned long long)scene.activeBodyId());
    std::printf("begin=%s\n", freeformStatusName(ff.begin(scene.activeBodyId())));
    const FreeformCage& ca = scene.activeBody().freeformOrNull()->cage();
    std::vector<uint32_t> pick{idOf(ca.faces.back().id)};
    std::printf("select face %u => %s\n", pick[0], freeformStatusName(ff.select(pick)));
    // The product's Open path: loadProjectDocument under the lock, nothing about the freeform session.
    std::printf("load B=%s active=%llu\n", projectCodecStatusName(loadProjectDocument(b, scene, s, h)), (unsigned long long)scene.activeBodyId());
    ff.reconcile();
    std::printf("after load+reconcile: session active=%d body=%llu selection_size=%zu", ff.active() ? 1 : 0, (unsigned long long)ff.bodyId(), ff.selection().size());
    for (uint32_t id : ff.selection()) std::printf(" id=%u", id);
    std::printf("\n");
    return 0;
}
