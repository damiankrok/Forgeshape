// Debug-only deterministic checks for derived render geometry: shading normals,
// the crease policy, the Smooth/Faceted contracts on all five primitives, the
// rebuild policy, the generated MatCap asset and the display settings.
//
// Not a test framework and not shipped behaviour: a fixed list of assertions the
// debug build runs once at startup, exactly like the camera, picking, mesh,
// Construction, transform, primitive, sphere, cone/capsule and sculpt suites.
// No third-party test library is used.
#pragma once

namespace forgeshape {

struct RenderMeshSelfTestResult {
    const char* name = "";
    bool passed = false;
};

// Runs every check, writing at most `max` results. Returns how many ran.
int runRenderMeshSelfTests(RenderMeshSelfTestResult* out, int max);

}  // namespace forgeshape
