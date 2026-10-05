#include "forgeshape_cad_multiface_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_camera.h"
#include "forgeshape_history.h"
#include "forgeshape_input.h"
#include "forgeshape_project_bootstrap.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_arrangement.h"
#include "forgeshape_sketch_region.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {

namespace {

struct Checks {
    std::vector<ArrangementSelfTestCheck>* out;
    void check(const char* name, bool ok) { out->push_back(ArrangementSelfTestCheck{name, ok}); }
};

// A sketch session over the XY plane, driven through the gesture path the
// product uses: each entity is drawn by a real drag and then TYPED to its exact
// geometry, so the case is exact and never depends on snapping.
struct Driver {
    static constexpr int kW = 1000;
    static constexpr int kH = 1000;
    SketchSession sketch;
    CameraController camera;

    bool begin() {
        if (sketch.begin(Workplane::XY) != CadStatus::Ok) return false;
        camera.setViewport(kW, kH);
        const SketchFrame& f = sketch.frame();
        camera.frameSketchView(f.origin, f.u, f.v, f.n);
        return true;
    }
    bool at(TouchAction action, float x, float y) {
        TouchPointer p{7, x, y};
        return sketch.onTouch(action, action == TouchAction::Up ? 7 : -1, &p, 1, camera.snapshot(),
                              kW, kH);
    }
    bool place(SketchTool tool, SketchEntity::Payload exact) {
        sketch.setTool(tool);
        float x0, y0, x1, y1;
        if (!sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.13, 0.27}, kW, kH, &x0, &y0)
            || !sketch.sketchToScreen(camera.snapshot(), SketchPoint{0.61, 0.83}, kW, kH, &x1, &y1)) {
            return false;
        }
        const size_t before = sketch.sketch().entities.size();
        bool ok = at(TouchAction::Down, x0, y0);
        for (int step = 1; step <= 4; ++step) {
            const float t = step / 4.0f;
            ok &= at(TouchAction::Move, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t);
        }
        ok &= at(TouchAction::Up, x1, y1);
        if (!ok || sketch.sketch().entities.size() != before + 1u) return false;
        return sketch.replaceEntity(sketch.sketch().entities.back().id(), std::move(exact))
               == CadStatus::Ok;
    }
    // Every entity of `source` through `place`, then Finish.
    bool finishOn(const CadSketch& source) {
        if (!begin()) return false;
        for (const SketchEntity& entity : source.entities) {
            SketchTool tool = SketchTool::Line;
            if (entity.rectangle() != nullptr) tool = SketchTool::Rectangle;
            if (entity.circle() != nullptr) tool = SketchTool::Circle;
            if (!place(tool, entity.payload())) return false;
        }
        return sketch.finish() == CadStatus::Ok;
    }
};

SketchEntityId add(CadSketch* sketch, SketchEntity::Payload payload) {
    SketchEntityId id = kNoSketchEntity;
    addSketchEntity(sketch, std::move(payload), &id);
    return id;
}

// A `cols` x `rows` grid of unit cells: one rectangle centred on the origin
// and every interior grid line running exactly from side to side, so each
// line ends in two T-junctions and crosses every line of the other family.
// Exactly cols * rows bounded atomic faces, no overlap, no sliver.
CadSketch gridSketch(int cols, int rows) {
    CadSketch s;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = cols;
    r.height = rows;
    add(&s, r);
    const double u0 = -0.5 * cols;
    const double v0 = -0.5 * rows;
    for (int k = 1; k < cols; ++k) {
        SketchLine l;
        l.start = SketchPoint{u0 + k, v0};
        l.end = SketchPoint{u0 + k, -v0};
        add(&s, l);
    }
    for (int k = 1; k < rows; ++k) {
        SketchLine l;
        l.start = SketchPoint{u0, v0 + k};
        l.end = SketchPoint{-u0, v0 + k};
        add(&s, l);
    }
    return s;
}

// The owner's kind of sketch: a 4 x 3 rectangle, circles of radius 0.8 at
// (-0.8, 0) and (0, 0) overlapping each other, and one of radius 0.6 at
// (2, 0) across the right side. Four closed loops whose curves cross.
CadSketch ownerStyleSketch() {
    CadSketch s;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = 4.0;
    r.height = 3.0;
    add(&s, r);
    for (const auto& c : {std::pair<SketchPoint, double>{{-0.8, 0.0}, 0.8},
                          std::pair<SketchPoint, double>{{0.0, 0.0}, 0.8},
                          std::pair<SketchPoint, double>{{2.0, 0.0}, 0.6}}) {
        SketchCircle circle;
        circle.center = c.first;
        circle.radius = c.second;
        add(&s, circle);
    }
    return s;
}

// The atomic face of the session whose interior holds `p`, or the face count.
size_t faceAt(const SketchSession& sketch, const SketchPoint& p) {
    const SketchArrangement& a = sketch.arrangement();
    for (size_t i = 0; i < a.faces.size(); ++i) {
        std::vector<PlanarProfileComponent> shape;
        if (mergePlanarFaceSelection(a, {i}, &shape) != CadStatus::Ok || shape.size() != 1u) continue;
        bool inside = sketchPointStrictlyInside(p, shape[0].outer.polygon);
        for (const PlanarProfileLoop& hole : shape[0].holes) {
            inside = inside && !sketchPointStrictlyInside(p, hole.polygon);
        }
        if (inside) return i;
    }
    return a.faces.size();
}

// The face of grid cell (column c, row r), counted from the lower left.
size_t cellFace(const SketchSession& sketch, int cols, int rows, int c, int r) {
    return faceAt(sketch, SketchPoint{-0.5 * cols + c + 0.5, -0.5 * rows + r + 0.5});
}

// ---------------------------------------------------------------------------
// BEFORE: what the owner's build does (`MF-01`), recorded as it is.
// ---------------------------------------------------------------------------

void testBefore(Checks& r, std::string* report) {
    constexpr int kCols = 6;
    constexpr int kRows = 4;
    r.check("MF_B01_the_planar_face_cap_is_the_sixteen_region_cap",
            kMaxPlanarFaceSelection == kMaxProfileRegions && kMaxProfileRegions == 16u);

    Driver d;
    const bool ready = d.finishOn(gridSketch(kCols, kRows))
                       && d.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                       && d.sketch.planarFaceCount() == static_cast<size_t>(kCols * kRows);
    // Reading order: row by row from the lower left.
    std::vector<size_t> cells;
    for (int row = 0; ready && row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) cells.push_back(cellFace(d.sketch, kCols, kRows, col, row));
    }
    bool first16 = ready && cells.size() == 24u;
    for (size_t k = 0; first16 && k < 16u; ++k) {
        first16 = d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok
                  && d.sketch.selectedAreaCount() == k + 1u;
    }
    const CadStatus seventeenth = first16 ? d.sketch.togglePlanarFace(cells[16]) : CadStatus::Ok;
    const CadStatus lastAfter = d.sketch.lastStatus();
    const size_t countAfter = d.sketch.selectedAreaCount();
    const CadCandidateEvaluation& candidate = d.sketch.evaluateCandidate();
    r.check("MF_B02_faces_1_to_16_select_and_the_17th_is_refused_too_many_regions",
            first16 && seventeenth == CadStatus::TooManyRegions
                    && lastAfter == CadStatus::TooManyRegions && countAfter == 16u
                    && !d.sketch.planarFaceSelected(cells[16]));
    r.check("MF_B03_the_sixteen_face_candidate_stays_ok_after_the_refusal",
            candidate.status == CadStatus::Ok && candidate.mesh != nullptr);

    // The first project (`APP-H1`): the same selection, committed from the
    // CAD bootstrap over an empty scene.
    ConstructionScene empty{NoProjectTag{}};
    ConstructionHistory emptyHistory(empty);
    SculptSession sculpt;
    Driver boot;
    const bool bootReady = boot.finishOn(gridSketch(kCols, kRows))
                           && boot.sketch.togglePlanarFace(cellFace(boot.sketch, kCols, kRows, 0, 0))
                                      == CadStatus::Ok
                           && boot.sketch.togglePlanarFace(cellFace(boot.sketch, kCols, kRows, 1, 0))
                                      == CadStatus::Ok;
    const CadStatus bootCandidate = boot.sketch.evaluateCandidate().status;
    const CadStatus bootCommit =
            bootReady ? commitFirstCadProject(boot.sketch, empty, sculpt, emptyHistory)
                      : CadStatus::NotSketching;
    // The grid closes ONE loop (the rectangle), so the bootstrap's loop-count
    // test names it ProfileNotFound; the owner's sketch closes several.
    r.check("MF_B04_the_first_project_commit_refuses_a_grid_face_selection_as_profile_not_found",
            bootReady && bootCandidate == CadStatus::Ok
                    && bootCommit == CadStatus::ProfileNotFound
                    && boot.sketch.lastStatus() == CadStatus::ProfileNotFound
                    && boot.sketch.selectedAreaCount() == 2u && !empty.hasProject());

    // The owner's shape of sketch: a rectangle, two overlapping circles and a
    // third across the rectangle's side -- several closed loops, crossing.
    ConstructionScene ownerEmpty{NoProjectTag{}};
    ConstructionHistory ownerHistory(ownerEmpty);
    Driver owner;
    const bool ownerReady = owner.finishOn(ownerStyleSketch())
                            && owner.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                            && owner.sketch.planarFaceCount() >= 5u;
    bool ownerPicked = ownerReady;
    for (size_t i = 0; ownerPicked && i < 3u; ++i) {
        ownerPicked = owner.sketch.togglePlanarFace(i) == CadStatus::Ok;
    }
    const CadStatus ownerCandidate = owner.sketch.evaluateCandidate().status;
    const CadStatus ownerCommit =
            ownerPicked ? commitFirstCadProject(owner.sketch, ownerEmpty, sculpt, ownerHistory)
                        : CadStatus::NotSketching;
    r.check("MF_B04B_the_owner_sketch_first_commit_says_several_profiles_with_three_faces_chosen",
            ownerPicked && ownerCandidate == CadStatus::Ok
                    && ownerCommit == CadStatus::AmbiguousProfile
                    && owner.sketch.lastStatus() == CadStatus::AmbiguousProfile
                    && owner.sketch.selectedAreaCount() == 3u && !ownerEmpty.hasProject());

    // Inside a live project the same selection commits: the refusal is the
    // bootstrap's alone.
    ConstructionScene live;
    ConstructionHistory liveHistory(live);
    Driver inProject;
    ObjectId created = kNoObject;
    const bool liveReady =
            inProject.finishOn(gridSketch(kCols, kRows))
            && inProject.sketch.togglePlanarFace(cellFace(inProject.sketch, kCols, kRows, 0, 0))
                       == CadStatus::Ok
            && inProject.sketch.togglePlanarFace(cellFace(inProject.sketch, kCols, kRows, 1, 0))
                       == CadStatus::Ok;
    const CadStatus liveCommit =
            liveReady ? inProject.sketch.commit(live, liveHistory, &created) : CadStatus::NotSketching;
    r.check("MF_B05_the_same_selection_commits_inside_a_live_project",
            liveCommit == CadStatus::Ok && created != kNoObject);

    char line[256];
    std::snprintf(line, sizeof(line),
                  "multiface_before=cap:%u,17th:%s,count:%zu,candidate:%s,first_project_grid:%s,"
                  "first_project_owner:%s",
                  kMaxPlanarFaceSelection, cadStatusName(seventeenth), countAfter,
                  cadStatusName(candidate.status), cadStatusName(bootCommit),
                  cadStatusName(ownerCommit));
    *report = line;
    boot.sketch.cancel();
    owner.sketch.cancel();
}

}  // namespace

void runCadMultiFaceSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    Checks r{out};
    std::string before;
    testBefore(r, &before);
    if (performance != nullptr) *performance = before;
}

}  // namespace forgeshape
