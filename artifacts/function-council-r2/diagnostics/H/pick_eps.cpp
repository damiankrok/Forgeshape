// FC2 seat H: absolute det epsilon in intersectRayTriangle -> smallest pickable triangle.
#include <cstdio>
#include <initializer_list>
#include "forgeshape_picking.h"
using namespace forgeshape;
int main(){
  for (float leg : {1e-3f, 3e-4f, 1.5e-4f, 1.0e-4f, 9e-5f, 5e-5f, 1e-5f}) {
    Vec3 a{0,0,0}, b{leg,0,0}, c{0,leg,0};
    Ray r; r.origin = {leg/3, leg/3, 1.0f}; r.direction = {0,0,-1}; // centroid, normal incidence, front face (+z normal)
    float t=0; bool hit = intersectRayTriangle(r, a, b, c, true, &t);
    // 60 deg incidence
    Ray g; g.origin={leg/3 - 0.8660254f, leg/3, 0.5f}; g.direction={0.8660254f,0,-0.5f};
    float t2=0; bool hit2 = intersectRayTriangle(g, a, b, c, true, &t2);
    std::printf("leg_m=%.1e twice_area=%.2e hit_normal=%d hit_60deg=%d\n", leg, (double)leg*leg, hit, hit2);
  }
}
