// Adversarial repro of D-01 through the REAL stroke path (beginStroke/updateStroke/endStroke).
#include <cstdio>
#include <cmath>
#include "forgeshape_sculpt.h"
#include "forgeshape_sculpt_history.h"
#include "forgeshape_camera.h"
using namespace forgeshape;
static const int W=1080,H=2400;
CameraSnapshot cam(){ CameraSnapshot s{}; Vec3 eye{0,6,0}; s.eye=eye; s.target=Vec3{0,0,0};
 s.distance=6; s.projection=ProjectionMode::Perspective; s.orthoHalfHeightMeters=6*std::tan(kFovYRadians*0.5f);
 s.view=mat4LookAt(eye,s.target,Vec3{0,0,1}); s.proj=mat4Perspective(kFovYRadians,(float)W/H,kNearPlane,kFarPlane); return s;}
int main(){
  const uint32_t side=230; ConstructionMesh g;
  for(uint32_t j=0;j<side;++j)for(uint32_t i=0;i<side;++i){MeshVertex v{};
    v.position[0]=-0.6f+1.2f*i/(side-1); v.position[1]=0; v.position[2]=-0.6f+1.2f*j/(side-1);
    v.color[0]=v.color[1]=v.color[2]=0.7f; g.vertices.push_back(v);}
  for(uint32_t j=0;j+1<side;++j)for(uint32_t i=0;i+1<side;++i){uint32_t a=j*side+i,b=a+1,c=a+side,d=c+1;
    g.indices.insert(g.indices.end(),{a,d,b,a,c,d});}
  FrozenSculpt fz; SculptSession s; s.bindTarget(&fz);
  if(!s.freezeToSculpt(g,ObjectId{7},nullptr)){std::puts("freeze failed");return 1;}
  SculptMesh& m=fz.mesh; Mat4 I=mat4Identity(); CameraSnapshot c=cam();
  auto y=[&](uint32_t v){return m.vertices()[v].position[1];};
  const uint32_t centre=(side/2)*side+side/2, corner=0;
  // Stroke 1: small Inflate at centre.
  s.setTool(SculptTool::Inflate); s.setStrength(1.0f); s.setRadiusPixels(20);
  bool b1=s.beginStroke(c,W*0.5f,H*0.5f,W,H,I,I); for(int k=0;k<5;++k) s.updateStroke(W*0.5f+3*(k+1),H*0.5f+3*(k+1)); s.endStroke();
  std::printf("stroke1 begin=%d undoDepth=%zu notRetained=%zu hasEdits=%d yC=%.5f yCorner=%.5f\n",b1,fz.history.undoDepth(),(size_t)fz.history.notRetainedStrokes(),m.hasEdits(),y(centre),y(corner));
  float yC1=y(centre);
  // Stroke 2: huge Inflate covering whole grid.
  s.setRadiusPixels(600);
  bool b2=s.beginStroke(c,W*0.5f,H*0.5f,W,H,I,I); for(int k=0;k<5;++k) s.updateStroke(W*0.5f+3*(k+1),H*0.5f+3*(k+1)); s.endStroke();
  int moved=0; for(uint32_t v=0;v<m.vertexCount();++v) if(std::fabs(y(v))>1e-7f) ++moved;
  std::printf("stroke2 begin=%d moved=%d undoDepth=%zu notRetained=%zu hasEdits=%d yC=%.5f yCorner=%.5f\n",b2,moved,fz.history.undoDepth(),(size_t)fz.history.notRetainedStrokes(),m.hasEdits(),y(centre),y(corner));
  float yCorner2=y(corner);
  auto st=s.undoStroke();
  int still=0; for(uint32_t v=0;v<m.vertexCount();++v) if(std::fabs(y(v))>1e-7f) ++still;
  std::printf("undo status=%d hasEdits=%d yC=%.5f (pre-stroke1=0, after-stroke1=%.5f) yCorner=%.5f stillDisplaced=%d canUndo=%d\n",(int)st,m.hasEdits(),y(centre),yC1,y(corner),still,fz.history.canUndo());
  std::printf("RESULT mixed_state=%d hasEdits_false_while_displaced=%d\n", (std::fabs(y(centre))<1e-7f && std::fabs(y(corner)-yCorner2)<1e-7f && yCorner2!=0), (!m.hasEdits() && still>0));
}
