// J1: can a HIDDEN (or LOCKED) CAD body be edited through the sketch session,
// and is its candidate ever in the snapshot the preview substitutes into?
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_body_commands.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_workplane.h"
#include "forgeshape_cad_body.h"
using namespace forgeshape;
static std::vector<uint8_t> readAll(const char* p){std::ifstream in(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
static bool inSnapshot(const ConstructionScene& s, ObjectId id){for(const auto& it: s.snapshot()) if(it.objectId==id) return true; return false;}
int main(int argc,char**argv){
  const char* dir=argv[1];
  for (int mode=0; mode<2; ++mode) { // 0 hidden, 1 locked
    std::string path=std::string(dir)+"/cad_rectangle_v1.forge";
    auto bytes=readAll(path.c_str());
    ProjectDocument doc; ProjectCodecStatus st=decodeProject(bytes.data(),bytes.size(),&doc);
    ConstructionScene scene((NoProjectTag())); ConstructionHistory history(scene); SculptSession sculpt;
    ProjectCodecStatus ls=loadProjectDocument(doc,scene,sculpt,history);
    ObjectId id=scene.activeBodyId();
    std::printf("decode=%s load=%s bodies=%zu id=%llu\n",projectCodecStatusName(st),projectCodecStatusName(ls),scene.bodyCount(),(unsigned long long)id);
    if(!scene.findBody(id)) return 1;
    BodyCommandStatus bs = mode==0 ? setSceneBodyVisible(id,false,scene,history) : setSceneBodyLocked(id,true,scene,history);
    const CadBodyState before = scene.findBody(id)->cadOrNull()->state();
    WorkplaneFrame wf=workplaneFrame(cadBaseSketch(before).plane);
    SketchFrame frame{Vec3{0,0,0},wf.uAxis,wf.vAxis,wf.normal};
    SketchSession sk;
    CadStatus b=sk.beginEdit(id,before,frame);
    CadStatus f=sk.finish();
    CadStatus e=sk.setExtrude(before.extrude.depth*2.0, before.extrude.direction);
    const CadCandidateEvaluation& ev=sk.evaluateCandidate();
    std::printf("[%s] decode=%s load=%s cmd=%s inSnapshot=%d beginEdit=%s finish=%s setExtrude=%s candidate=%s targetIsBody=%d\n",
      mode==0?"HIDDEN":"LOCKED",projectCodecStatusName(st),projectCodecStatusName(ls),bodyCommandStatusName(bs),
      inSnapshot(scene,id),cadStatusName(b),cadStatusName(f),cadStatusName(e),cadStatusName(ev.status),ev.targetBodyId==id);
    size_t depthBefore=history.undoDepth();
    CadStatus c=sk.commitEdit(scene,history);
    const CadBodyState& after=scene.findBody(id)->cadOrNull()->state();
    std::printf("[%s] commitEdit=%s depth %.4f -> %.4f undoSteps %zu -> %zu visible=%d locked=%d\n",
      mode==0?"HIDDEN":"LOCKED",cadStatusName(c),before.extrude.depth,after.extrude.depth,depthBefore,history.undoDepth(),
      scene.findBody(id)->visible(),scene.findBody(id)->locked());
  }
  // Freeform for contrast
  {
    std::string path=std::string(dir)+"/freeform_box_v1.forge";
    auto bytes=readAll(path.c_str()); ProjectDocument doc; decodeProject(bytes.data(),bytes.size(),&doc);
    ConstructionScene& scene=constructionScene(); ConstructionHistory& history=constructionHistory(); SculptSession sculpt;
    ProjectCodecStatus ls=loadProjectDocument(doc,scene,sculpt,history);
    ObjectId id=scene.activeBodyId();
    setSceneBodyVisible(id,false,scene,history);
    FreeformStatus fs=freeformEditSession().begin(id);
    setSceneBodyVisible(id,true,scene,history); setSceneBodyLocked(id,true,scene,history);
    FreeformStatus fl=freeformEditSession().begin(id);
    std::printf("[FREEFORM] load=%s beginHidden=%s beginLocked=%s\n",projectCodecStatusName(ls),freeformStatusName(fs),freeformStatusName(fl));
  }
  return 0;
}
