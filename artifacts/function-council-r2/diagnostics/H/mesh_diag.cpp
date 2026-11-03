// FC2 seat H diagnostic: per-revision CPU cost of publish + render-mesh refresh + linear pick,
// at the Gate P1 stress tiers and the Freeform derived cap (131072 quads ~ 262144 tris).
#include <chrono>
#include <cstdio>
#include <algorithm>
#include "forgeshape_mesh.h"
#include "forgeshape_mesh_fixtures.h"
#include "forgeshape_render_mesh.h"
#include "forgeshape_picking.h"
using namespace forgeshape;
static double us(std::chrono::steady_clock::time_point t){return std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t).count();}
int main(){
  for (uint32_t target : {10000u, 50000u, 100000u, 131072u}) {
    FixtureMesh m = buildStressMesh(target);
    MeshStore store(42);
    double pubBest=1e18, refBestS=1e18, refBestF=1e18, pickMissBest=1e18;
    RenderMeshCache cache;
    for (int rep=0; rep<5; ++rep){
      auto t=std::chrono::steady_clock::now();
      store.publish(m.vertices.data(), (uint32_t)m.vertices.size(), m.indices.data(), (uint32_t)m.indices.size());
      pubBest=std::min(pubBest, us(t));
      RuntimeMeshPtr cur = store.current();
      t=std::chrono::steady_clock::now(); bool f=false; cache.refresh(*cur, SurfaceShading::Smooth, &f); refBestS=std::min(refBestS, us(t));
      t=std::chrono::steady_clock::now(); cache.refresh(*cur, SurfaceShading::Faceted, &f); refBestF=std::min(refBestF, us(t));
      Ray r; r.origin={1000.f,1000.f,1000.f}; r.direction={0.f,1.f,0.f}; // misses: every triangle tested
      t=std::chrono::steady_clock::now(); TriangleHit h=pickTriangleMesh(r, cur->triangleView(), true); pickMissBest=std::min(pickMissBest, us(t)); (void)h;
    }
    std::printf("target=%u vertices=%zu triangles=%zu publish_us=%.0f render_refresh_smooth_us=%.0f render_refresh_faceted_us=%.0f pick_full_scan_us=%.0f\n",
      target, m.vertices.size(), m.indices.size()/3, pubBest, refBestS, refBestF, pickMissBest);
  }
}
