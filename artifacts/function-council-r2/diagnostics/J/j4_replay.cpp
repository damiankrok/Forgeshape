// J4: cross-representation command sequence -> after every step the project must
// (a) encode, (b) reopen in a fresh scene with the same semantic fingerprint;
// then Undo-all must walk back through exactly the recorded fingerprints and
// Redo-all forward again. Refusals must change nothing.
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "forgeshape_scene.h"
#include "forgeshape_history.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_body_mirror.h"
#include "forgeshape_freeform_session.h"
using namespace forgeshape;
static std::vector<uint8_t> readAll(const std::string& p){std::ifstream in(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
static ConstructionScene& S(){return constructionScene();}
static ConstructionHistory& H(){return constructionHistory();}
static uint64_t fp(){return projectSemanticFingerprint(S(),ProjectKind::Construction);}
static int fails=0;
static std::vector<ProjectDocument> docs;
static bool sameIgnoringAllocator(ProjectDocument a, ProjectDocument b){a.scene.nextObjectId=b.scene.nextObjectId=0; return sameProjectDocument(a,b);}
static ProjectDocument cap(){return captureProjectDocument(S(),ProjectKind::Construction);}
static void reopenCheck(const char* step){
  ProjectCodecStatus why=ProjectCodecStatus::Ok;
  auto bytes=encodeProjectV1(captureProjectDocument(S(),ProjectKind::Construction),&why);
  ProjectDocument d; ProjectCodecStatus ds=bytes.empty()?why:decodeProject(bytes.data(),bytes.size(),&d);
  ConstructionScene fresh((NoProjectTag())); ConstructionHistory fh(fresh); SculptSession ss;
  ProjectCodecStatus ls= ds==ProjectCodecStatus::Ok? loadProjectDocument(d,fresh,ss,fh):ds;
  uint64_t a=fp(), b= ls==ProjectCodecStatus::Ok? projectSemanticFingerprint(fresh,ProjectKind::Construction):0;
  bool ok=!bytes.empty()&&ls==ProjectCodecStatus::Ok&&a==b;
  if(!ok) ++fails;
  std::printf("  %-34s encode=%s(%zu B) reopen=%s fp=%016llx reopenedFp=%016llx %s\n",step,projectCodecStatusName(why),bytes.size(),projectCodecStatusName(ls),(unsigned long long)a,(unsigned long long)b,ok?"OK":"MISMATCH");
}
int main(int argc,char**argv){
  std::string dir=argv[1];
  auto bytes=readAll(dir+"/mixed_cad_face_v2.forge"); ProjectDocument doc; decodeProject(bytes.data(),bytes.size(),&doc);
  SculptSession ss; std::printf("load=%s bodies=%zu\n",projectCodecStatusName(loadProjectDocument(doc,S(),ss,H())),S().bodyCount());
  std::vector<uint64_t> trail{fp()}; docs.push_back(cap()); reopenCheck("loaded");
  // find producer (has dependents) and dependent (face supported)
  ObjectId producer=kNoObject, dependent=kNoObject, other=kNoObject;
  for(size_t i=0;i<S().bodyCount();++i){SceneObject& b=S().bodyAt(i); if(S().hasCadDependents(b.objectId())) producer=b.objectId(); else if(b.isFaceSupportedCad()) dependent=b.objectId(); else other=b.objectId();}
  std::printf("producer=%llu dependent=%llu other=%llu\n",(unsigned long long)producer,(unsigned long long)dependent,(unsigned long long)other);
  auto step=[&](const char* name,bool expectChange,int code,const char* codeName){
    uint64_t f=fp(); bool changed= f!=trail.back();
    if(changed!=expectChange){++fails; std::printf("  !! %s expected change=%d got %d\n",name,expectChange,changed);}
    std::printf("STEP %-30s -> %s changed=%d undoDepth=%zu\n",name,codeName,changed,H().undoDepth());
    if(changed){trail.push_back(f); docs.push_back(cap()); reopenCheck(name);}
  };
  // refusals first: must change nothing
  {DeleteBodyStatus s=deleteSceneBody(producer,S(),H()); step("delete producer (refuse)",false,(int)s,deleteBodyStatusName(s));}
  {BodyCommandStatus s=duplicateSceneBody(dependent,S(),H()); step("duplicate dependent (refuse)",false,(int)s,bodyCommandStatusName(s));}
  {BodyCommandStatus s=renameSceneBody(producer,"",S(),H()); step("rename to empty (refuse)",false,(int)s,bodyCommandStatusName(s));}
  {BodyCommandStatus s=mirrorSceneBody(producer,MirrorPlane::Xy,S(),H()); step("mirror CAD (refuse)",false,(int)s,bodyCommandStatusName(s));}
  // mutations
  {BodyCommandStatus s=renameSceneBody(producer,"Plate \xF0\x9F\x94\xA9 \xC3\xA9",S(),H()); step("rename producer (emoji)",true,(int)s,bodyCommandStatusName(s));}
  {BodyCommandStatus s=setSceneBodyLocked(dependent,true,S(),H()); step("lock dependent",true,(int)s,bodyCommandStatusName(s));}
  {BodyCommandStatus s=setSceneBodyVisible(producer,false,S(),H()); step("hide producer",true,(int)s,bodyCommandStatusName(s));}
  {DuplicateBodyReport r; BodyCommandStatus s=duplicateSceneBody(producer,S(),H(),&r); step("duplicate producer",true,(int)s,bodyCommandStatusName(s));}
  {ObjectId fid=kNoObject; FreeformStatus s=createFreeformBody(S(),H(),0,&fid); step("create freeform box",true,(int)s,freeformStatusName(s));
   FreeformStatus b=freeformEditSession().begin(fid); FreeformStatus sel=freeformEditSession().select({1});
   FreeformStatus ex=freeformEditSession().extrude(0.25);
   std::printf("   freeform begin=%s select=%s\n",freeformStatusName(b),freeformStatusName(sel));
   step("freeform extrude face 1",ex==FreeformStatus::Ok,(int)ex,freeformStatusName(ex));
   freeformEditSession().end();}
  {DeleteBodyStatus s=deleteSceneBody(dependent,S(),H()); step("delete dependent",true,(int)s,deleteBodyStatusName(s));}
  {DeleteBodyStatus s=deleteSceneBody(producer,S(),H()); step("delete producer (now free)",true,(int)s,deleteBodyStatusName(s));}
  size_t n=trail.size();
  std::printf("UNDO walk (%zu states)\n",n);
  for(size_t i=n-1;i>0;--i){bool u=H().undo(); uint64_t f=fp(); bool m=f==trail[i-1]; bool sem=sameIgnoringAllocator(cap(),docs[i-1]); if(!sem||!u) ++fails; std::printf("  undo -> state %zu fp=%s semantic(ignoring allocator)=%s nextObjectId=%llu\n",i-1,m?"MATCH":"DIFF",sem?"MATCH":"MISMATCH",(unsigned long long)S().nextObjectId()); if(m) reopenCheck("after undo");}
  std::printf("  extra undo=%d (expected 0)\n",(int)H().undo());
  for(size_t i=1;i<n;++i){bool r=H().redo(); uint64_t f=fp(); bool m=f==trail[i]; bool sem=sameIgnoringAllocator(cap(),docs[i]); if(!sem||!r) ++fails; std::printf("  redo -> state %zu fp=%s semantic=%s\n",i,m?"MATCH":"DIFF",sem?"MATCH":"MISMATCH");}
  std::printf("J4_SUMMARY states=%zu failures=%d nextObjectId=%llu\n",n,fails,(unsigned long long)S().nextObjectId());
}
