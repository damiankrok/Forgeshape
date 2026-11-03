// FC2 B-02 adversarial repro: export translation vs drawn (snapshot) model for a face-supported CAD body.
#include <cstdio>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_gltf_export.h"
#include "forgeshape_cad_face.h"
using namespace forgeshape;
static CadBodyState rectBody(double w,double h,double d){CadBodyState s;cadBaseSketch(s).plane=Workplane::XY;SketchRectangle r;r.center=SketchPoint{0,0};r.width=w;r.height=h;addSketchEntity(&cadBaseSketch(s),r);s.extrude.profileEntityId=1;s.extrude.depth=d;s.extrude.direction=ExtrudeDirection::AlongNormal;return s;}
int main(){
  ConstructionScene scene{NoProjectTag{}};
  CadStatus why; CadBodyState ps=rectBody(2,1,3);
  SceneObject* p=scene.addCadBody(ps,&why); publishSceneObject(*p);
  // rotate+move the producer too, so the face frame and producer placement both matter
  CadBodyState c=rectBody(0.5,0.5,0.5);
  std::vector<CadFace> faces; enumerateCadFaces(ps,&faces);
  CadFaceToken tok{}; for(auto&f:faces) if(f.token.kind==CadFaceKind::Side){tok=f.token;break;}
  cadBaseSketch(c).hasFaceSupport=true; auto&fs=cadBaseSketch(c).faceSupport;
  fs.producerObjectId=p->objectId(); fs.producerLocalFeatureId=kCadFeatureId; fs.face=tok; fs.lineageToken=cadTopologySignature(ps);
  SceneObject* d=scene.addCadBody(c,&why); if(!d){std::printf("dep refused %d\n",(int)why);return 1;} publishSceneObject(*d);
  std::printf("faceSupported=%d\n",(int)d->isFaceSupportedCad());
  Mat4 w; bool ok=scene.resolveWorldModel(d->objectId(),&w);
  std::printf("resolved ok=%d T=(%.4f,%.4f,%.4f) col0=(%.3f,%.3f,%.3f) col2=(%.3f,%.3f,%.3f)\n",ok,w.m[12],w.m[13],w.m[14],w.m[0],w.m[1],w.m[2],w.m[8],w.m[9],w.m[10]);
  for(auto& it: scene.snapshot()) std::printf("snapshot id=%llu T=(%.4f,%.4f,%.4f)\n",(unsigned long long)it.objectId,it.model.m[12],it.model.m[13],it.model.m[14]);
  GlbExportScene ex; auto st=captureGlbExportScene(scene,ProjectKind::Construction,&ex);
  std::printf("export=%s\n",glbExportStatusName(st));
  for(auto&b:ex.bodies){ float mn[3]={1e9,1e9,1e9},mx[3]={-1e9,-1e9,-1e9}; for(auto&v:b.render.vertices) for(int k=0;k<3;k++){mn[k]=std::min(mn[k],v.position[k]);mx[k]=std::max(mx[k],v.position[k]);}
    std::printf("export id=%llu T=(%.4f,%.4f,%.4f) localbbox=[%.3f,%.3f,%.3f]-[%.3f,%.3f,%.3f] local col0=(%.3f,%.3f,%.3f)\n",(unsigned long long)b.objectId,b.translation[0],b.translation[1],b.translation[2],mn[0],mn[1],mn[2],mx[0],mx[1],mx[2],b.local.m[0],b.local.m[1],b.local.m[2]);}
  auto glb=exportSceneAsGlb(scene,ProjectKind::Construction); std::printf("glb bytes=%zu\n",glb.size());
}
