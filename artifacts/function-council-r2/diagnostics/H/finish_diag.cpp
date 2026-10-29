// FC2 seat H: split the near-cap Finish cost: entity placement (touch path) vs SketchSession::finish().
#include "forgeshape_cad_multiface_selftest.cpp"
#include <cstdio>
using namespace forgeshape;
int main() {
  for (int n : {24, 40, 63}) {
    Driver d; d.begin();
    CadSketch src = gridSketch(n, n);
    auto t0 = std::chrono::steady_clock::now(); double placeMax = 0;
    for (const SketchEntity& e : src.entities) {
      SketchTool tool = e.rectangle() ? SketchTool::Rectangle : SketchTool::Line;
      auto ti = std::chrono::steady_clock::now();
      if (!d.place(tool, e.payload())) { std::printf("place failed\n"); return 1; }
      placeMax = std::max(placeMax, micros(ti));
    }
    double place = micros(t0);
    auto t1 = std::chrono::steady_clock::now();
    CadStatus s = d.sketch.finish();
    double fin = micros(t1);
    std::printf("grid=%dx%d entities=%zu place_total_us=%.0f place_max_one_entity_us=%.0f finish_only_us=%.0f status=%s faces=%zu\n",
      n, n, src.entities.size(), place, placeMax, fin, cadStatusName(s), d.sketch.planarFaceCount());
    d.sketch.cancel();
  }
}
