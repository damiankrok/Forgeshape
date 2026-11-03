#include <cstdio>
#include <vector>
#include "forgeshape_surface_selftest.h"
#include "forgeshape_freeform_selftest.h"
using namespace forgeshape;
int main(){ std::vector<SurfaceSelfTestResult> s(8192); int n=runSurfaceSelfTests(s.data(),8192); for(int i=0;i<n;i++) std::printf("SURF %s\n", s[i].name);
 std::vector<FreeformSelfTestResult> f(8192); n=runFreeformSelfTests(f.data(),8192); for(int i=0;i<n;i++) std::printf("FF %s\n", f[i].name);}
