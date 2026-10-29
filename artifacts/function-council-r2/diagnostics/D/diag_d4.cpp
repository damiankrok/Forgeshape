// Seat D diagnostic 4: a Sculpt-kind document whose active sculpted body is
// hidden -- does the load path enter Sculpt over a hidden target? Scratch only.
#include <cstdio>
#include "forgeshape_history.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
using namespace forgeshape;
int main() {
    ConstructionScene a;  // one default Construction body
    SceneObject& body = a.activeBody();
    ConstructionMesh seed;
    buildSculptSourceMesh(body, &seed);
    MeshValidation why = MeshValidation::Ok;
    const bool froze = body.frozenSculpt().mesh.freezeFrom(seed, body.objectId(), &why);
    body.setVisible(false);
    const ProjectDocument doc = captureProjectDocument(a, ProjectKind::Sculpt);
    ProjectCodecStatus encWhy = ProjectCodecStatus::Ok;
    const std::vector<uint8_t> bytes = encodeProjectV1(doc, &encWhy);
    ProjectDocument decoded;
    const ProjectCodecStatus dec = bytes.empty() ? encWhy : decodeProject(bytes.data(), bytes.size(), &decoded);
    ConstructionScene b(NoProjectTag{});
    ConstructionHistory history{b};
    SculptSession session;
    const ProjectCodecStatus load = dec == ProjectCodecStatus::Ok
        ? loadProjectDocument(decoded, b, session, history, nullptr) : dec;
    std::printf("INFO froze=%d encode=%s bytes=%zu decode=%s load=%s inSculpt=%d activeVisible=%d hasSculptMesh=%d\n",
                (int)froze, projectCodecStatusName(encWhy), bytes.size(), projectCodecStatusName(dec),
                projectCodecStatusName(load), (int)session.inSculptMode(),
                b.hasProject() ? (int)b.activeBody().visible() : -1, (int)session.hasSculptMesh());
    return 0;
}
