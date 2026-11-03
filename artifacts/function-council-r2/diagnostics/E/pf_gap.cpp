#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;
static void tryDoc(const char* label, ProjectDocument d) {
    CadBodyMesh mesh;
    const CadStatus regen = regenerateCadBody(d.cad.bodies[0].state, &mesh);
    const ProjectCodecStatus v = validateProjectDocument(d);
    std::vector<uint8_t> bytes = encodeProjectV1Unchecked(d);
    ProjectDocument back; const ProjectCodecStatus dec = decodeProject(bytes.data(), bytes.size(), &back);
    ConstructionScene scene(NoProjectTag{}); SculptSession s; ConstructionHistory h(scene);
    const bool canEval = dec == ProjectCodecStatus::Ok && runtimeCanEvaluateProject(back);
    const ProjectCodecStatus ld = dec == ProjectCodecStatus::Ok ? loadProjectDocument(back, scene, s, h) : dec;
    std::printf("%s: regenerateCadBody=%s validateProjectDocument=%s decode(bytes)=%s runtimeCanEvaluate=%d (=> validateProject JNI says %s) loadProjectDocument=%s bodies_after=%zu\n",
                label, cadStatusName(regen), projectCodecStatusName(v), projectCodecStatusName(dec), canEval ? 1 : 0,
                (dec == ProjectCodecStatus::Ok && canEval) ? "PROJECT_OK" : "refused", projectCodecStatusName(ld), scene.bodyCount());
}
int main(int, char** argv) {
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ProjectDocument d; decodeProject(b.data(), b.size(), &d);
    tryDoc("original", d);
    ProjectDocument flip = d;
    CadFeature& f = flip.cad.bodies[0].state.laterFeatures[0];
    std::printf("later op=%s depth=%g\n", cadFeatureOperationName(f.operation), f.extrude.depth);
    f.operation = f.operation == CadFeatureOperation::Add ? CadFeatureOperation::Cut : CadFeatureOperation::Add;
    tryDoc("operation_flipped", flip);
    ProjectDocument dir = d;
    CadFeature& g = dir.cad.bodies[0].state.laterFeatures[0];
    g.extrude.direction = g.extrude.direction == ExtrudeDirection::AlongNormal ? ExtrudeDirection::AgainstNormal : ExtrudeDirection::AlongNormal;
    tryDoc("direction_flipped", dir);
    ProjectDocument both = flip;
    CadFeature& k = both.cad.bodies[0].state.laterFeatures[0];
    k.extrude.direction = k.extrude.direction == ExtrudeDirection::AlongNormal ? ExtrudeDirection::AgainstNormal : ExtrudeDirection::AlongNormal;
    tryDoc("operation_and_direction_flipped", both);
    return 0;
}
