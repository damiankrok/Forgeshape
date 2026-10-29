// J5: non-finite / absurd values pushed through the domain entry points the JNI
// forwards doubles to unchanged. Each must be refused by name with the project
// (fingerprint) and the history depth unchanged.
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_surface_authoring.h"
#include "forgeshape_surface.h"
#include "forgeshape_sketch_session.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_workplane.h"
using namespace forgeshape;
static std::vector<uint8_t> readAll(const std::string& p){std::ifstream in(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
static bool load(const std::string& p, ConstructionScene& s, ConstructionHistory& h){auto b=readAll(p);ProjectDocument d; if(decodeProject(b.data(),b.size(),&d)!=ProjectCodecStatus::Ok) return false; SculptSession ss; return loadProjectDocument(d,s,ss,h)==ProjectCodecStatus::Ok;}
static int bad=0, total=0;
static void verdict(const char* what,double v,const char* status,bool refused,bool unchanged){++total; bool ok=refused&&unchanged; if(!ok)++bad; std::printf("%-28s v=%-12g -> %-26s refused=%d unchanged=%d %s\n",what,v,status,refused,unchanged,ok?"OK":"VIOLATION");}
int main(int argc,char**argv){
  std::string dir=argv[1];
  const double vals[]={std::nan(""),INFINITY,-INFINITY,1e308,-1e308};
  // Freeform (process-scoped session)
  {ConstructionScene& s=constructionScene(); ConstructionHistory& h=constructionHistory(); load(dir+"/freeform_box_v1.forge",s,h);
   FreeformEditSession& f=freeformEditSession(); f.begin(s.activeBodyId()); f.select({1});
   for(double v:vals){
     uint64_t fp=projectSemanticFingerprint(s,ProjectKind::Construction); size_t d=h.undoDepth();
     FreeformStatus a=f.pushPull(v); verdict("freeform pushPull",v,freeformStatusName(a),a!=FreeformStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
     FreeformStatus b=f.extrude(v); verdict("freeform extrude",v,freeformStatusName(b),b!=FreeformStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
     FreeformStatus c=f.setCrease(v); verdict("freeform setCrease",v,freeformStatusName(c),c!=FreeformStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
   }
   for(int lvl: {-1,5,1000}){uint64_t fp=projectSemanticFingerprint(s,ProjectKind::Construction); FreeformStatus c=f.setLevel(lvl); verdict("freeform setLevel",lvl,freeformStatusName(c),c!=FreeformStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction));}
   f.end();}
  // Surface
  {ConstructionScene s((NoProjectTag())); ConstructionHistory h(s); load(dir+"/surface_patch_extrude_v1.forge",s,h); ObjectId id=s.activeBodyId();
   for(uint32_t fid=1; fid<=4; ++fid) for(double v:vals){
     uint64_t fp=projectSemanticFingerprint(s,ProjectKind::Construction); size_t d=h.undoDepth();
     SurfaceStatus a=surfaceApplyValue(s,h,id,SurfaceValueTarget::Feature,fid,v); verdict((std::string("surface value feature ")+std::to_string(fid)).c_str(),v,surfaceStatusName(a),a!=SurfaceStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
     SurfaceStatus b=surfaceApplyValue(s,h,id,SurfaceValueTarget::Sketch,fid,v); verdict((std::string("surface value sketch ")+std::to_string(fid)).c_str(),v,surfaceStatusName(b),b!=SurfaceStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
     SurfaceStatus c=surfaceThicken(s,h,id,SurfaceFeatureId{fid},v); verdict((std::string("surface thicken ")+std::to_string(fid)).c_str(),v,surfaceStatusName(c),c!=SurfaceStatus::Ok,fp==projectSemanticFingerprint(s,ProjectKind::Construction)&&d==h.undoDepth());
   }}
  // CAD sketch session staged values (nothing is truth until commit; check refusal + staged depth intact)
  {ConstructionScene s((NoProjectTag())); ConstructionHistory h(s); load(dir+"/cad_rectangle_v1.forge",s,h); ObjectId id=s.activeBodyId();
   const CadBodyState st=s.findBody(id)->cadOrNull()->state(); WorkplaneFrame wf=workplaneFrame(cadBaseSketch(st).plane);
   SketchSession k; k.beginEdit(id,st,SketchFrame{Vec3{0,0,0},wf.uAxis,wf.vAxis,wf.normal}); k.finish();
   for(double v:vals){ double before=k.extrude().depth; CadStatus a=k.setExtrude(v,ExtrudeDirection::AlongNormal); verdict("cad setExtrude",v,cadStatusName(a),a!=CadStatus::Ok,before==k.extrude().depth);
     CadStatus b=k.setExtrudeSide(true,v); verdict("cad setExtrudeSide",v,cadStatusName(b),b!=CadStatus::Ok,before==k.extrude().depth);
     CadStatus c=k.setRevolveAngle(v); verdict("cad setRevolveAngle (no revolve)",v,cadStatusName(c),c!=CadStatus::Ok,true);}
   uint64_t fp=projectSemanticFingerprint(s,ProjectKind::Construction); CadStatus c=k.commitEdit(s,h);
   std::printf("cad commit after refusals=%s fpUnchanged=%d (staged edit carried no value)\n",cadStatusName(c),fp==projectSemanticFingerprint(s,ProjectKind::Construction));}
  std::printf("J5_SUMMARY probes=%d violations=%d\n",total,bad);
}
