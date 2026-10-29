#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include "forgeshape_freeform.h"
using namespace forgeshape;
static uint64_t bits(double d){uint64_t b;std::memcpy(&b,&d,8);return b;}
static const FreeformVertex* V(const FreeformCage& c,uint32_t id){for(auto&v:c.vertices) if(idOf(v.id)==id) return &v; return nullptr;}
static FreeformAffine tr(double x,double y,double z){FreeformAffine a{}; for(int i=0;i<9;i++)a.linear[i]=(i%4==0)?1:0; a.translation={x,y,z}; a.pivot={0,0,0}; return a;}
static FreeformAffine rotZ(double t,DVec3 piv){FreeformAffine a{}; double c=cos(t),s=sin(t); double m[9]={c,-s,0,s,c,0,0,0,1}; for(int i=0;i<9;i++)a.linear[i]=m[i]; a.translation={0,0,0}; a.pivot=piv; return a;}
// exact check: for every vertex under X, find a vertex with bit-identical (-x,y,z)
static void checkX(const char* tag,const FreeformCage& c){
  int bad=0,neg0=0;
  for(auto&v:c.vertices){ const DVec3 p=v.position; if(std::signbit(p.x)&&p.x==0)neg0++; if(std::signbit(p.y)&&p.y==0)neg0++; if(std::signbit(p.z)&&p.z==0)neg0++;
    bool f=false; for(auto&w:c.vertices){ if(bits(w.position.x)==bits(-p.x+0.0)&&bits(w.position.y)==bits(p.y)&&bits(w.position.z)==bits(p.z)) f=true;}
    if(!f)bad++;}
  printf("%s valid=%s vertices=%zu bitMismatch=%d negZero=%d\n",tag,freeformStatusName(validateFreeformCage(c)),c.vertices.size(),bad,neg0);
}
int main(){
  FreeformCage box=makeFreeformBox(), bx, mid, o;
  printf("set=%s\n",freeformStatusName(freeformSetSymmetry(box,kFreeformSymmetryX,&bx)));
  for(auto&v:bx.vertices) printf("v%u (%g,%g,%g)\n",idOf(v.id),v.position.x,v.position.y,v.position.z);
  // 1 both partners selected, +x translation
  auto s=freeformTransform(bx,FreeformElement::Vertex,{1,2},tr(0.1,0,0),&o);
  printf("both-selected +x: %s v1=(%g) v2=(%g)\n",freeformStatusName(s),V(o,1)->position.x,V(o,2)->position.x); checkX("both",o);
  // 2 rotation about off-plane pivot
  s=freeformTransform(bx,FreeformElement::Vertex,{2},rotZ(0.7,{0.3,0.2,0.1}),&o); printf("rot: %s\n",freeformStatusName(s)); checkX("rot",o);
  // 3 midpoint loop, rotate on-plane vertex 9 about off-plane pivot
  freeformInsertEdgeLoop(bx,idOf(bx.edges[0].id),0.5,&mid);
  FreeformCage m2; uint32_t e=0; for(auto&ed:bx.edges){ auto a=V(bx,idOf(ed.v0))->position,b=V(bx,idOf(ed.v1))->position; if(a.x!=b.x){e=idOf(ed.id);break;}}
  printf("loop=%s\n",freeformStatusName(freeformInsertEdgeLoop(bx,e,0.5,&m2)));
  for(auto&v:m2.vertices) if(v.position.x==0){ uint32_t id=idOf(v.id);
     s=freeformTransform(m2,FreeformElement::Vertex,{id},rotZ(1.1,{0.4,-0.3,0.2}),&o);
     printf("onplane v%u rot: %s x=%a signbit=%d\n",id,freeformStatusName(s),V(o,id)->position.x,(int)std::signbit(V(o,id)->position.x)); checkX("onplane-rot",o);
     s=freeformTransform(m2,FreeformElement::Vertex,{id},tr(-0.3,0,0),&o);
     printf("onplane v%u -x: %s x=%a signbit=%d\n",id,freeformStatusName(s),V(o,id)->position.x,(int)std::signbit(V(o,id)->position.x));
     break;}
  // 4 push/pull & extrude of +x face, and of a face straddling the plane
  for(auto&f:m2.faces){ s=freeformPushPull(m2,{idOf(f.id)},0.137,&o); if(s==FreeformStatus::Ok) checkX("pushpull",o); else printf("pp face %u %s\n",idOf(f.id),freeformStatusName(s));
                        s=freeformExtrudeFaces(m2,{idOf(f.id)},0.137,&o); if(s==FreeformStatus::Ok) checkX("extrude",o); else printf("ex face %u %s\n",idOf(f.id),freeformStatusName(s)); }
  // 5 vertex moved across plane onto it: v2 to x=0
  double x2=V(bx,2)->position.x; s=freeformTransform(bx,FreeformElement::Vertex,{2},tr(-x2,0,0),&o); printf("onto plane: %s\n",freeformStatusName(s)); if(s==FreeformStatus::Ok) checkX("ontoplane",o);
}
