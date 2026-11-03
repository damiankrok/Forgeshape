// OWNER-like reproducer (scratch, never shipped).
#include <cstdio>
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include "forgeshape_camera.h"
#include "forgeshape_cad_body.h"
#include "forgeshape_input.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_session.h"
using namespace forgeshape;

struct Driver {
    static constexpr int kW = 1000, kH = 1000;
    SketchSession sketch; CameraController camera;
    bool begin() {
        if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
        return true;
    }
    bool at(TouchAction a, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(a, a == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(), kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.13, 0.27}, kW, kH, &x0, &y0);
        sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.61, 0.83}, kW, kH, &x1, &y1);
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int s = 1; s <= 4; ++s) { const float t = s / 4.0f; ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t); }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return false;
        return sketch.replaceEntity(sketch.sketch().entities.back().id(), std::move(exact)) == CadStatus::Ok;
    }
};

static void addAll(std::vector<std::pair<SketchTool, SketchEntity::Payload>>* e) {
    SketchRectangle outer; outer.center = {0, 0}; outer.width = 6; outer.height = 4;
    e->push_back({SketchTool::Rectangle, outer});
    for (auto c : {std::make_pair(SketchPoint{-1.2, 0.0}, 1.0), std::make_pair(SketchPoint{0.0, 0.0}, 1.0),
                   std::make_pair(SketchPoint{1.2, 0.0}, 1.0), std::make_pair(SketchPoint{0.0, 0.9}, 0.8)}) {
        SketchCircle k; k.center = c.first; k.radius = c.second; e->push_back({SketchTool::Circle, k});
    }
    SketchSpline sp; sp.points = {{-3.0, -1.0}, {-1.5, 1.2}, {0.4, -1.3}, {1.8, 1.1}, {3.0, 0.6}};
    e->push_back({SketchTool::Line, sp});
    SketchRectangle extra; extra.center = {1.6, -1.2}; extra.width = 2.0; extra.height = 1.2;
    e->push_back({SketchTool::Rectangle, extra});
}

static const char* st(CadStatus s) { return cadStatusName(s); }

int main() {
    Driver d;
    if (!d.begin()) { std::printf("begin failed\n"); return 1; }
    std::vector<std::pair<SketchTool, SketchEntity::Payload>> ents; addAll(&ents);
    for (auto& e : ents) if (!d.place(e.first, e.second)) { std::printf("place failed\n"); return 1; }
    const CadStatus fin = d.sketch.finish();
    const SketchArrangement& a = d.sketch.arrangement();
    std::printf("finish=%s kind=%d arrangement=%s faces=%zu nodes=%zu fragments=%zu\n", st(fin),
                (int)d.sketch.selectionKind(), arrangementStatusName(a.status), a.faces.size(), a.nodes.size(), a.fragments.size());
    const size_t F = a.faces.size();
    // nodes and fragments per face
    std::vector<std::set<uint32_t>> nodesOf(F), fragsOf(F);
    double total = 0;
    for (size_t i = 0; i < F; ++i) {
        std::vector<const FragmentCycle*> b{&a.faces[i].ref.outer};
        for (auto& h : a.faces[i].ref.holes) b.push_back(&h);
        for (auto* c : b) for (auto& fr : *c) {
            for (uint32_t k = 0; k < a.fragments.size(); ++k) {
                const FragmentRef& g = a.fragments[k].ref;
                if (g.sourceEntityId == fr.sourceEntityId && g.sourceEdgeLocalIndex == fr.sourceEdgeLocalIndex
                    && compareArrangementCut(g.startCut, fr.startCut) == 0 && compareArrangementCut(g.endCut, fr.endCut) == 0) {
                    fragsOf[i].insert(k); nodesOf[i].insert(a.fragments[k].startNode); nodesOf[i].insert(a.fragments[k].endNode);
                }
            }
        }
        SketchPoint p; double area = 0; d.sketch.planarFaceInfo(i, &p, &area);
        total += a.faces[i].area;
        std::printf("FACE %2zu area=%.6f interior=(%.4f,%.4f) outerFragments=%zu holes=%zu nodes=%zu\n", i, a.faces[i].area, p.u, p.v,
                    a.faces[i].ref.outer.size(), a.faces[i].ref.holes.size(), nodesOf[i].size());
    }
    std::printf("TOTAL_AREA %.6f (rectangle 24)\n", total);
    // point-only contacts
    int pointPairs = 0;
    for (size_t i = 0; i < F; ++i) for (size_t j = i + 1; j < F; ++j) {
        bool shareFrag = false; for (auto k : fragsOf[i]) if (fragsOf[j].count(k)) shareFrag = true;
        int shareNode = 0; for (auto n : nodesOf[i]) if (nodesOf[j].count(n)) ++shareNode;
        if (!shareFrag && shareNode > 0) { ++pointPairs; std::printf("POINT_CONTACT %zu-%zu nodes=%d\n", i, j, shareNode); }
    }
    std::printf("POINT_CONTACT_PAIRS %d\n", pointPairs);
    // all-but selections
    auto evalSel = [&](const std::vector<size_t>& sel, bool verbose) {
        std::vector<PlanarProfileComponent> comps;
        const ArrangementStatus m = mergePlanarFaces(a, sel, &comps);
        std::vector<std::vector<size_t>> groups;
        partitionSelectedPlanarFacesBySharedBoundary(a, sel, &groups);
        if (verbose) {
            std::printf("  selected=%zu groups=%zu merge=%s components=%zu\n", sel.size(), groups.size(), arrangementStatusName(m), comps.size());
            for (size_t gi = 0; gi < groups.size(); ++gi) {
                // boundary half-edges' origin node counts in this group
                std::map<uint32_t, int> out;
                std::set<size_t> gs(groups[gi].begin(), groups[gi].end());
                for (size_t f : groups[gi]) for (auto k : fragsOf[f]) {
                    int owners = 0; for (size_t h : groups[gi]) if (fragsOf[h].count(k)) ++owners;
                    if (owners == 1) { out[a.fragments[k].startNode]++; out[a.fragments[k].endNode]++; }
                }
                std::printf("   group %zu faces={", gi); for (size_t f : groups[gi]) std::printf("%zu ", f); std::printf("} repeatedNodes={");
                for (auto& kv : out) if (kv.second > 2) std::printf("%u@(%.4f,%.4f)x%d ", kv.first, a.nodes[kv.first].u, a.nodes[kv.first].v, kv.second / 2);
                std::printf("}\n");
            }
        }
        return m;
    };
    std::vector<size_t> all; for (size_t i = 0; i < F; ++i) all.push_back(i);
    std::printf("ALL: "); evalSel(all, true);
    int pinches = 0;
    std::vector<std::vector<size_t>> found;
    for (size_t i = 0; i < F; ++i) for (size_t j = i + 1; j < F; ++j) {
        std::vector<size_t> sel; for (size_t k = 0; k < F; ++k) if (k != i && k != j) sel.push_back(k);
        std::vector<PlanarProfileComponent> comps;
        if (mergePlanarFaces(a, sel, &comps) == ArrangementStatus::PinchedSelection) { ++pinches; if (found.size() < 3) found.push_back(sel);
            std::printf("PINCH all-but {%zu,%zu} (%zu selected)\n", i, j, sel.size()); }
    }
    std::printf("ALL_BUT_TWO_PINCHES %d\n", pinches);
    for (auto& sel : found) {
        std::printf("REPRO selection:"); for (auto f : sel) std::printf(" %zu", f); std::printf("\n");
        evalSel(sel, true);
        // drive the session
        for (size_t i = 0; i < F; ++i) if (d.sketch.planarFaceSelected(i)) d.sketch.togglePlanarFace(i);
        bool ok = true; for (auto f : sel) ok &= d.sketch.togglePlanarFace(f) == CadStatus::Ok;
        const CadCandidateEvaluation& ev = d.sketch.evaluateCandidate();
        std::printf("  session toggles ok=%d selectedAreaCount=%zu candidate=%s operation=NewBody lastStatus=%s\n", ok,
                    d.sketch.selectedAreaCount(), st(ev.status), st(d.sketch.lastStatus()));
        // refs
        for (auto f : sel) {
            std::printf("   ref face %zu outer:", f);
            for (auto& fr : a.faces[f].ref.outer) std::printf(" [e%u.%u%s]", fr.sourceEntityId, fr.sourceEdgeLocalIndex, fr.reversed ? "r" : "");
            std::printf(" holes=%zu\n", a.faces[f].ref.holes.size());
        }
    }
    return 0;
}
