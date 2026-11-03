// J2: ASan+UBSan structural fuzz of decodeProject/loadProjectDocument.
// Overwrites a 4-byte window at EVERY byte offset of every section payload with
// count-like values, repairs the section CRC, decodes; an Ok decode is loaded
// over a live project and a refused load must leave that project unchanged.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;
static std::vector<uint8_t> readAll(const char* p){std::ifstream in(p,std::ios::binary);return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());}
static uint32_t rd32(const uint8_t* p){return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t rd64(const uint8_t* p){return (uint64_t)rd32(p)|((uint64_t)rd32(p+4)<<32);}
static void wr32(uint8_t* p,uint32_t v){for(int i=0;i<4;++i)p[i]=(v>>(8*i))&0xff;}
struct Sec{size_t hdr,payload;uint64_t len;};
static std::vector<Sec> secs(const std::vector<uint8_t>& b){std::vector<Sec> s; if(b.size()<28) return s; uint32_t n=rd32(&b[16]); size_t off=28;
 for(uint32_t i=0;i<n&&off+24<=b.size();++i){uint64_t len=rd64(&b[off+8]); if(off+24+len>b.size()) break; s.push_back({off,off+24,len}); off+=24+len;} return s;}
int main(int argc,char**argv){
  const char* base=argv[1];
  const uint32_t vals[]={0xFFFFFFFFu,0x7FFFFFFFu,0x00100000u,0x00010000u,0u};
  long runs=0,ok=0,loadOk=0,loadRefused=0,violations=0,badAlloc=0; double worstMs=0; std::string worst;
  auto baseBytes=readAll(base); ProjectDocument baseDoc; decodeProject(baseBytes.data(),baseBytes.size(),&baseDoc);
  for(int i=2;i<argc;++i){
    const auto bytes=readAll(argv[i]); const char* name=strrchr(argv[i],'/')+1;
    for(const Sec& s: secs(bytes)){
      for(size_t off=0; off+4<=s.len; ++off){
        for(uint32_t v: vals){
          std::vector<uint8_t> m=bytes; wr32(&m[s.payload+off],v);
          wr32(&m[s.hdr+16],crc32IsoHdlc(&m[s.payload],(size_t)s.len));
          ++runs; ProjectDocument d; ProjectCodecStatus st;
          auto t0=std::chrono::steady_clock::now();
          try { st=decodeProject(m.data(),m.size(),&d); } catch(const std::bad_alloc&){ ++badAlloc; std::printf("BAD_ALLOC decode %s off=%zu v=%08x\n",name,off,v); continue; }
          double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
          if(ms>worstMs){worstMs=ms; char buf[256]; snprintf(buf,sizeof buf,"%s sec@%zu off=%zu v=%08x status=%s",name,s.hdr,off,v,projectCodecStatusName(st)); worst=buf;}
          if(st!=ProjectCodecStatus::Ok) continue;
          ++ok;
          ConstructionScene scene((NoProjectTag())); ConstructionHistory h(scene); SculptSession ss;
          loadProjectDocument(baseDoc,scene,ss,h);
          const uint64_t fp=projectSemanticFingerprint(scene,ProjectKind::Construction); const size_t n=scene.bodyCount();
          ProjectCodecStatus ls;
          try { ls=loadProjectDocument(d,scene,ss,h);} catch(const std::bad_alloc&){ ++badAlloc; std::printf("BAD_ALLOC load %s off=%zu v=%08x\n",name,off,v); continue; }
          if(ls==ProjectCodecStatus::Ok) ++loadOk; else { ++loadRefused;
            if(projectSemanticFingerprint(scene,ProjectKind::Construction)!=fp||scene.bodyCount()!=n){++violations; std::printf("VIOLATION %s off=%zu v=%08x %s\n",name,off,v,projectCodecStatusName(ls));}
            else std::printf("LOAD_REFUSED_AFTER_OK_DECODE %s sec@%zu off=%zu v=%08x %s\n",name,s.hdr,off,v,projectCodecStatusName(ls)); }
        }
      }
    }
    std::fflush(stdout);
  }
  std::printf("J2_SUMMARY runs=%ld decodeOk=%ld loadOk=%ld loadRefused=%ld failClosedViolations=%ld badAlloc=%ld worstDecodeMs=%.2f (%s)\n",runs,ok,loadOk,loadRefused,violations,badAlloc,worstMs,worst.c_str());
}
