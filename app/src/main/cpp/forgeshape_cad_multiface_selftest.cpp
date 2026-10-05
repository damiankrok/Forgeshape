#include "forgeshape_cad_multiface_selftest.h"

#include <algorithm>
#include <chrono>
#include <cstring>
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
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
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
// Helpers over a finished grid session
// ---------------------------------------------------------------------------

// Every cell of a cols x rows grid in reading order (row by row from the lower
// left), as face indices of the session; empty when any cell does not resolve.
std::vector<size_t> gridCells(const SketchSession& sketch, int cols, int rows) {
    std::vector<size_t> cells;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const size_t face = cellFace(sketch, cols, rows, col, row);
            if (face >= sketch.planarFaceCount()) return {};
            cells.push_back(face);
        }
    }
    return cells;
}

// The Ready tap a finger makes: the cell's interior point, projected through
// the session's camera, resolved by `toggleRegionAt` at that pixel.
bool tapFace(Driver& d, size_t face) {
    SketchPoint p;
    float x = 0.0f;
    float y = 0.0f;
    return d.sketch.planarFaceInfo(face, &p, nullptr)
           && d.sketch.sketchToScreen(d.camera.snapshot(), p, Driver::kW, Driver::kH, &x, &y)
           && d.sketch.toggleRegionAt(d.camera.snapshot(), x, y, Driver::kW, Driver::kH)
           && d.sketch.lastTapOutcome() == SketchTapOutcome::Resolved;
}

bool sameRefs(const std::vector<PlanarFaceRef>& a, const std::vector<PlanarFaceRef>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!samePlanarFaceRef(a[i], b[i])) return false;
    }
    return true;
}

// Strictly ascending and distinct: the one canonical stored form.
bool canonical(const std::vector<PlanarFaceRef>& refs) {
    for (size_t i = 1; i < refs.size(); ++i) {
        if (comparePlanarFaceRef(refs[i - 1], refs[i]) >= 0) return false;
    }
    return true;
}

bool clearAll(SketchSession& sketch) {
    for (size_t i = 0; i < sketch.planarFaceCount(); ++i) {
        if (sketch.planarFaceSelected(i) && sketch.togglePlanarFace(i) != CadStatus::Ok) return false;
    }
    return sketch.selectedAreaCount() == 0u;
}

// A non-empty selection is never "choose one" (`MF-14`), wherever it is asked.
bool neverChooseOne(SketchSession& sketch) {
    if (sketch.selectedAreaCount() == 0u) return true;
    return sketch.evaluateCandidate().status != CadStatus::AmbiguousProfile
           && sketch.evaluateCandidate().status != CadStatus::ProfileNotFound;
}

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

double micros(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - since)
            .count();
}

// --- byte helpers for the codec checks --------------------------------------

size_t cadPayloadAt(const std::vector<uint8_t>& bytes) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            return offset + kForgeSectionHeaderBytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

uint16_t cadVersionOf(const std::vector<uint8_t>& bytes) {
    const size_t payload = cadPayloadAt(bytes);
    if (payload < kForgeSectionHeaderBytes) return 0;
    uint16_t version = 0;
    std::memcpy(&version, &bytes[payload - kForgeSectionHeaderBytes + 4], 2);
    return version;
}

// The bytes with `value` written at CADB payload offset `at` and the section's
// CRC repaired, so only the decoder's own count rule can refuse them.
std::vector<uint8_t> patchedCadU32(std::vector<uint8_t> bytes, size_t at, uint32_t value) {
    const size_t payload = cadPayloadAt(bytes);
    if (payload == 0 || payload + at + 4u > bytes.size()) return {};
    std::memcpy(&bytes[payload + at], &value, 4);
    const size_t header = payload - kForgeSectionHeaderBytes;
    uint64_t payloadBytes = 0;
    std::memcpy(&payloadBytes, &bytes[header + 8], 8);
    const uint32_t crc = crc32IsoHdlc(&bytes[payload], static_cast<size_t>(payloadBytes));
    std::memcpy(&bytes[header + 16], &crc, 4);
    return bytes;
}

ProjectCodecStatus decodeStatus(const std::vector<uint8_t>& bytes, ProjectDocument* out = nullptr) {
    ProjectDocument scratch;
    return decodeProject(bytes.data(), bytes.size(), out != nullptr ? out : &scratch);
}

// ---------------------------------------------------------------------------
// MF-01: the bound is derived, and it is not the region cap
// ---------------------------------------------------------------------------

void testBound(Checks& r) {
    // The densest arrangement the caps admit: 64 lines by 64 lines is exactly
    // the contact cap, and 63 x 63 faces must sit inside the derived bound.
    CadSketch lines;
    for (int k = 0; k < 64; ++k) {
        SketchLine v;
        v.start = SketchPoint{k * 0.25, -1.0};
        v.end = SketchPoint{k * 0.25, 64 * 0.25};
        add(&lines, v);
        SketchLine h;
        h.start = SketchPoint{-1.0, k * 0.25};
        h.end = SketchPoint{64 * 0.25, k * 0.25};
        add(&lines, h);
    }
    const SketchArrangement dense = deriveSketchArrangement(lines);
    r.check("MF_01_the_planar_face_bound_is_derived_from_the_arrangement_caps_not_the_region_cap",
            kMaxPlanarFaceSelection == kMaxArrangementFaces
                    && kMaxArrangementFaces == kMaxArrangementFragments
                    && kMaxArrangementFragments
                               == kMaxArrangementSourceEdges + 2u * kMaxArrangementContacts
                    && kMaxArrangementFaces == 9216u && kMaxProfileRegions == 16u
                    && kMaxPlanarFaceSelection != kMaxProfileRegions
                    && dense.status == ArrangementStatus::Ok && dense.faces.size() == 63u * 63u
                    && dense.stats.fragments <= kMaxArrangementFragments
                    && dense.faces.size() <= kMaxArrangementFaces);
}

// ---------------------------------------------------------------------------
// MF-02..06: selection past the old cap
// ---------------------------------------------------------------------------

void testSelection(Checks& r) {
    // 6 x 4: the owner's "about two dozen cells", crossing the old cap.
    {
        Driver d;
        const bool ready = d.finishOn(gridSketch(6, 4))
                           && d.sketch.selectionKind() == CadSelectionKind::PlanarFaces;
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 6, 4) : std::vector<size_t>{};
        bool each = cells.size() == 24u;
        bool honest = each;
        for (size_t k = 0; each && k < 18u; ++k) {
            // Real taps from the 15th on, so the 17th and 18th cross the old
            // cap through the Ready tap path and its diagnostic outcome.
            each = (k < 14u ? d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok
                            : tapFace(d, cells[k]))
                   && d.sketch.selectedAreaCount() == k + 1u && d.sketch.planarFaceSelected(cells[k])
                   && d.sketch.lastStatus() == CadStatus::Ok;
            honest = honest && neverChooseOne(d.sketch);
        }
        r.check("MF_02_the_17th_and_18th_faces_select_by_a_real_tap_with_no_selection_cap",
                each && honest && d.sketch.lastTapOutcome() == SketchTapOutcome::Resolved
                        && canonical(d.sketch.extrude().planarFaces)
                        && d.sketch.evaluateCandidate().status == CadStatus::Ok);
        d.sketch.cancel();
    }
    // 8 x 6: 48 faces.
    Driver d;
    const bool ready = d.finishOn(gridSketch(8, 6));
    const std::vector<size_t> cells = ready ? gridCells(d.sketch, 8, 6) : std::vector<size_t>{};
    bool thirtyTwo = cells.size() == 48u;
    for (size_t k = 0; thirtyTwo && k < 32u; ++k) {
        thirtyTwo = d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok
                    && d.sketch.selectedAreaCount() == k + 1u;
    }
    r.check("MF_03_thirty_two_faces_select_and_extrude",
            thirtyTwo && canonical(d.sketch.extrude().planarFaces)
                    && d.sketch.evaluateCandidate().status == CadStatus::Ok
                    && neverChooseOne(d.sketch));
    const std::vector<PlanarFaceRef> first32 = d.sketch.extrude().planarFaces;

    // MF-06 on those 32: remove the faces tapped 15th, 16th, 17th and 32nd
    // (positions 14, 15, 16, 31), then put them back in another order.
    bool removed = thirtyTwo;
    for (size_t k : {14u, 15u, 16u, 31u}) {
        removed = removed && d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok
                  && !d.sketch.planarFaceSelected(cells[k]);
    }
    removed = removed && d.sketch.selectedAreaCount() == 28u && neverChooseOne(d.sketch);
    bool readded = removed;
    for (size_t k : {31u, 16u, 14u, 15u}) {
        readded = readded && d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok;
    }
    r.check("MF_06_removing_and_readding_faces_around_the_old_cap_restores_the_exact_set",
            readded && sameRefs(d.sketch.extrude().planarFaces, first32));

    bool all = thirtyTwo;
    for (size_t k = 32; all && k < cells.size(); ++k) {
        all = d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok;
    }
    const CadCandidateEvaluation& every = d.sketch.evaluateCandidate();
    r.check("MF_04_every_face_of_a_48_face_arrangement_selects_and_the_union_is_one_slab",
            all && d.sketch.selectedAreaCount() == 48u && every.status == CadStatus::Ok
                    && every.mesh != nullptr && every.mesh->components == 1u
                    && near(every.mesh->volume, 48.0 * d.sketch.extrude().depth, 1e-6));
    const std::vector<PlanarFaceRef> allRefs = d.sketch.extrude().planarFaces;
    d.sketch.cancel();

    // MF-05: the same 48 faces tapped in reverse and in a fixed shuffle end in
    // the same canonical selection; random toggle runs end in their parity.
    const auto selectInOrder = [](const std::vector<size_t>& order, std::vector<PlanarFaceRef>* out) {
        Driver o;
        if (!o.finishOn(gridSketch(8, 6))) return false;
        const std::vector<size_t> c = gridCells(o.sketch, 8, 6);
        if (c.size() != 48u) return false;
        for (size_t k : order) {
            if (o.sketch.togglePlanarFace(c[k]) != CadStatus::Ok) return false;
        }
        *out = o.sketch.extrude().planarFaces;
        o.sketch.cancel();
        return true;
    };
    std::vector<size_t> reverse(48);
    std::vector<size_t> shuffled(48);
    for (size_t k = 0; k < 48u; ++k) {
        reverse[k] = 47u - k;
        shuffled[k] = (k * 29u + 7u) % 48u;  // 29 is coprime to 48: a permutation
    }
    std::vector<PlanarFaceRef> fromReverse;
    std::vector<PlanarFaceRef> fromShuffle;
    const bool orders = selectInOrder(reverse, &fromReverse) && selectInOrder(shuffled, &fromShuffle);
    // Random runs of 120 toggles: the result is the set toggled an odd number
    // of times, whatever the order and however far past 16 it goes.
    uint32_t seed = 0x2545F491u;
    int parityMismatches = 0;
    int largest = 0;
    {
        Driver o;
        const bool ok = o.finishOn(gridSketch(8, 6));
        const std::vector<size_t> c = ok ? gridCells(o.sketch, 8, 6) : std::vector<size_t>{};
        for (int run = 0; c.size() == 48u && run < 6; ++run) {
            std::vector<int> parity(48, 0);
            for (int t = 0; t < 120; ++t) {
                seed = seed * 1664525u + 1013904223u;
                const size_t k = (seed >> 8) % 48u;
                if (o.sketch.togglePlanarFace(c[k]) != CadStatus::Ok) ++parityMismatches;
                parity[k] ^= 1;
                largest = std::max(largest, static_cast<int>(o.sketch.selectedAreaCount()));
            }
            for (size_t k = 0; k < 48u; ++k) {
                if (o.sketch.planarFaceSelected(c[k]) != (parity[k] != 0)) ++parityMismatches;
            }
            if (!canonical(o.sketch.extrude().planarFaces)) ++parityMismatches;
            clearAll(o.sketch);
        }
        if (c.size() != 48u) parityMismatches = -1;
        o.sketch.cancel();
    }
    r.check("MF_05_order_and_repetition_do_not_matter_past_the_old_cap",
            orders && sameRefs(fromReverse, allRefs) && sameRefs(fromShuffle, allRefs)
                    && parityMismatches == 0 && largest > 16);
}

// ---------------------------------------------------------------------------
// MF-07..12: union, extrusion, commit and persistence past the old cap
// ---------------------------------------------------------------------------

// The cells of a mixed selection on an 8 x 8 grid: rows 0-1 whole (16 cells,
// one edge-connected block), row 2 empty (the block is DISJOINT from the
// rest), rows 3-7 a checkerboard (20 cells, each meeting its neighbours only
// at a corner). 36 faces, 1 + 20 = 21 components.
std::vector<size_t> mixedSelection(const std::vector<size_t>& cells) {
    std::vector<size_t> chosen;
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            if (row < 2 || (row >= 3 && (row + col) % 2 == 0)) chosen.push_back(cells[row * 8 + col]);
        }
    }
    return chosen;
}

void testExtrusion(Checks& r, std::string* perf) {
    // --- MF-07 / MF-09: a contiguous 5 x 4 block of a 6 x 4 grid ----------
    {
        ConstructionScene scene;  // a live project: the default Box
        ConstructionHistory history(scene);
        Driver d;
        const bool ready = d.finishOn(gridSketch(6, 4));
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 6, 4) : std::vector<size_t>{};
        std::vector<size_t> block;
        for (int row = 0; cells.size() == 24u && row < 4; ++row) {
            for (int col = 0; col < 5; ++col) block.push_back(cells[row * 6 + col]);
        }
        bool chosen = block.size() == 20u;
        for (size_t f : block) chosen = chosen && d.sketch.togglePlanarFace(f) == CadStatus::Ok;
        std::vector<PlanarProfileComponent> components;
        const CadStatus merged = mergePlanarFaceSelection(d.sketch.arrangement(), block, &components);
        // The union's outline walks the block's perimeter: 2 * (5 + 4) unit
        // fragments, every internal shared side cancelled, no hole.
        r.check("MF_07_a_contiguous_20_face_selection_unions_to_one_component_with_internal_edges_cancelled",
                chosen && merged == CadStatus::Ok && components.size() == 1u
                        && components[0].holes.empty()
                        && components[0].outer.fragments.size() == 18u
                        && near(components[0].area, 20.0, 1e-9));
        const CadCandidateEvaluation& candidate = d.sketch.evaluateCandidate();
        const bool preview = candidate.status == CadStatus::Ok && candidate.mesh != nullptr
                             && candidate.mesh->components == 1u
                             && near(candidate.mesh->volume, 20.0 * d.sketch.extrude().depth, 1e-6);
        const size_t bodiesBefore = scene.bodyCount();
        ObjectId id = kNoObject;
        const CadStatus committed = chosen ? d.sketch.commit(scene, history, &id) : CadStatus::NotSketching;
        const SceneObject* body = scene.findBody(id);
        r.check("MF_09_new_body_previews_and_commits_a_contiguous_20_face_selection_as_one_undo",
                preview && committed == CadStatus::Ok && body != nullptr && body->isCad()
                        && scene.bodyCount() == bodiesBefore + 1u && history.undoDepth() == 1u
                        && body->cadOrNull()->state().extrude.planarFaces.size() == 20u);
    }

    // --- MF-08 / MF-10 / MF-11 / MF-12: a mixed 36-face selection ---------
    ConstructionScene scene;
    ConstructionHistory history(scene);
    Driver d;
    const bool ready = d.finishOn(gridSketch(8, 8));
    const std::vector<size_t> cells = ready ? gridCells(d.sketch, 8, 8) : std::vector<size_t>{};
    const std::vector<size_t> mixed = cells.size() == 64u ? mixedSelection(cells) : std::vector<size_t>{};
    bool chosen = mixed.size() == 36u;
    for (size_t f : mixed) chosen = chosen && d.sketch.togglePlanarFace(f) == CadStatus::Ok;
    std::vector<std::vector<size_t>> groups;
    const ArrangementStatus partitioned =
            partitionSelectedPlanarFacesBySharedBoundary(d.sketch.arrangement(), mixed, &groups);
    std::vector<PlanarProfileComponent> first;
    std::vector<PlanarProfileComponent> second;
    std::vector<size_t> reversed(mixed.rbegin(), mixed.rend());
    const CadStatus m1 = mergePlanarFaceSelection(d.sketch.arrangement(), mixed, &first);
    const CadStatus m2 = mergePlanarFaceSelection(d.sketch.arrangement(), reversed, &second);
    bool deterministic = m1 == CadStatus::Ok && m2 == CadStatus::Ok && first.size() == second.size();
    double area = 0.0;
    size_t sixteenCellBlocks = 0;
    for (size_t i = 0; deterministic && i < first.size(); ++i) {
        deterministic = first[i].outer.polygon.size() == second[i].outer.polygon.size()
                        && first[i].area == second[i].area && first[i].holes.empty();
        area += first[i].area;
        if (near(first[i].area, 16.0, 1e-9)) ++sixteenCellBlocks;
    }
    r.check("MF_08_mixed_edge_disjoint_and_point_touch_groups_derive_21_deterministic_components",
            chosen && partitioned == ArrangementStatus::Ok && groups.size() == 21u
                    && first.size() == 21u && deterministic && sixteenCellBlocks == 1u
                    && near(area, 36.0, 1e-9));

    const auto evalStart = std::chrono::steady_clock::now();
    const CadCandidateEvaluation& candidate = d.sketch.evaluateCandidate();
    const double evalUs = micros(evalStart);
    const double depth = d.sketch.extrude().depth;
    const bool preview = candidate.status == CadStatus::Ok && candidate.mesh != nullptr
                         && candidate.mesh->components == 21u
                         && near(candidate.mesh->volume, 36.0 * depth, 1e-6);
    const uint32_t previewComponents = candidate.mesh != nullptr ? candidate.mesh->components : 0u;
    const std::vector<PlanarFaceRef> stored = d.sketch.extrude().planarFaces;
    ObjectId id = kNoObject;
    const CadStatus committed = chosen ? d.sketch.commit(scene, history, &id) : CadStatus::NotSketching;
    const SceneObject* body = scene.findBody(id);
    CadBodyMesh committedMesh;
    const bool regenerated = body != nullptr && body->cadOrNull() != nullptr
                             && regenerateCadBody(body->cadOrNull()->state(), &committedMesh)
                                        == CadStatus::Ok;
    r.check("MF_10_new_body_previews_and_commits_a_36_face_multi_component_selection",
            preview && committed == CadStatus::Ok && regenerated && committedMesh.components == 21u
                    && near(committedMesh.volume, 36.0 * depth, 1e-6)
                    && sameRefs(body->cadOrNull()->state().extrude.planarFaces, stored));

    // MF-11: save, decode, re-encode, reload.
    const ProjectDocument document = captureProjectDocument(scene, ProjectKind::Construction);
    ProjectCodecStatus why = ProjectCodecStatus::Ok;
    const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
    ProjectDocument back;
    const ProjectCodecStatus decoded = decodeStatus(bytes, &back);
    const ProjectCadBody* backBody = nullptr;
    for (const ProjectCadBody& cad : back.cad.bodies) {
        if (cad.objectId == id) backBody = &cad;
    }
    ConstructionScene reloaded{NoProjectTag{}};
    ConstructionHistory reloadedHistory(reloaded);
    SculptSession sculpt;
    ProjectLoadReport report;
    const ProjectCodecStatus loaded =
            decoded == ProjectCodecStatus::Ok
                    ? loadProjectDocument(back, reloaded, sculpt, reloadedHistory, &report)
                    : decoded;
    const SceneObject* reloadedBody = reloaded.findBody(id);
    CadBodyMesh reloadedMesh;
    const bool reloadedOk = reloadedBody != nullptr && reloadedBody->cadOrNull() != nullptr
                            && regenerateCadBody(reloadedBody->cadOrNull()->state(), &reloadedMesh)
                                       == CadStatus::Ok
                            && reloadedMesh.components == 21u
                            && near(reloadedMesh.volume, committedMesh.volume, 1e-9);
    r.check("MF_11_a_36_face_selection_round_trips_exactly_through_cadb_v6_and_reloads",
            why == ProjectCodecStatus::Ok && !bytes.empty() && cadVersionOf(bytes) == kCadSectionVersionV6
                    && decoded == ProjectCodecStatus::Ok && backBody != nullptr
                    && sameRefs(backBody->state.extrude.planarFaces, stored)
                    && encodeProjectV1(back) == bytes && loaded == ProjectCodecStatus::Ok && reloadedOk);

    // MF-12: the count field is the existing v6 u32. Find it -- the selection
    // kind byte (PlanarFaces, file code 2) followed by the u32 36 -- and hold the decoder
    // to its new bound: 36 is accepted (above), the bound + 1 is an impossible
    // count, and a count at the bound with no bytes behind it is truncated.
    const size_t payload = cadPayloadAt(bytes);
    size_t countAt = 0;
    for (size_t at = 1; payload != 0 && payload + at + 4u <= bytes.size(); ++at) {
        uint32_t value = 0;
        std::memcpy(&value, &bytes[payload + at], 4);
        if (value == 36u && bytes[payload + at - 1u] == 2u) {  // PlanarFaces' file code
            countAt = at;
            break;
        }
    }
    r.check("MF_12_the_v6_count_field_is_unchanged_and_the_decoder_holds_it_to_the_derived_bound",
            countAt != 0
                    && decodeStatus(patchedCadU32(bytes, countAt, kMaxPlanarFaceSelection + 1u))
                               == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patchedCadU32(bytes, countAt, kMaxPlanarFaceSelection))
                               == ProjectCodecStatus::Truncated
                    && decodeStatus(patchedCadU32(bytes, countAt, 0u))
                               == ProjectCodecStatus::ImpossibleCount
                    && decodeStatus(patchedCadU32(bytes, countAt, 36u)) == ProjectCodecStatus::Ok);

    char line[160];
    std::snprintf(line, sizeof(line), " multiface_mixed36_eval_us=%.0f components=%u bytes=%zu",
                  evalUs, previewComponents, bytes.size());
    *perf += line;
}

// ---------------------------------------------------------------------------
// MF-13: the loop-region cap is unchanged
// ---------------------------------------------------------------------------

void testLoopRegionsUnchanged(Checks& r) {
    // Seventeen disjoint circles cross nothing: LoopRegions, as always.
    CadSketch circles;
    for (int k = 0; k < 17; ++k) {
        SketchCircle c;
        c.center = SketchPoint{-8.0 + k, 0.0};
        c.radius = 0.3;
        add(&circles, c);
    }
    Driver d;
    const bool ready = d.finishOn(circles) && d.sketch.selectionKind() == CadSelectionKind::LoopRegions
                       && d.sketch.regions().regions.size() == 17u && d.sketch.planarFaceCount() == 0u;
    bool sixteen = ready;
    for (size_t k = 0; sixteen && k < 16u; ++k) {
        sixteen = d.sketch.toggleRegion(d.sketch.regions().regions[k].outerAnchorId) == CadStatus::Ok;
    }
    const CadStatus seventeenth =
            ready ? d.sketch.toggleRegion(d.sketch.regions().regions[16].outerAnchorId)
                  : CadStatus::Ok;
    r.check("MF_13_loop_regions_keep_the_sixteen_region_cap_and_refuse_the_17th_by_name",
            sixteen && seventeenth == CadStatus::TooManyRegions
                    && d.sketch.selectedAreaCount() == 16u && kMaxProfileRegions == 16u
                    && d.sketch.evaluateCandidate().status == CadStatus::Ok);
    d.sketch.cancel();
}

// ---------------------------------------------------------------------------
// MF-14 / MF-15: the status names the CURRENT state, on every commit path
// ---------------------------------------------------------------------------

void testStatus(Checks& r) {
    SculptSession sculpt;
    // The owner's sketch, first project: nothing chosen is "choose one";
    // three faces chosen commit and create the project.
    {
        ConstructionScene empty{NoProjectTag{}};
        ConstructionHistory history(empty);
        Driver d;
        const bool ready = d.finishOn(ownerStyleSketch())
                           && d.sketch.selectionKind() == CadSelectionKind::PlanarFaces
                           && d.sketch.planarFaceCount() >= 5u;
        const CadStatus unchosen = ready ? commitFirstCadProject(d.sketch, empty, sculpt, history)
                                         : CadStatus::NotSketching;
        bool picked = ready && unchosen == CadStatus::AmbiguousProfile && !empty.hasProject();
        for (size_t i = 0; picked && i < 3u; ++i) {
            picked = d.sketch.togglePlanarFace(i) == CadStatus::Ok;
        }
        const bool honest = neverChooseOne(d.sketch);
        FirstProjectReport report;
        const CadStatus committed =
                picked ? commitFirstCadProject(d.sketch, empty, sculpt, history, &report)
                       : CadStatus::NotSketching;
        const SceneObject* body = empty.findBody(report.bodyId);
        r.check("MF_14_a_non_empty_face_selection_is_never_choose_one_and_the_first_project_commits_it",
                picked && honest && committed == CadStatus::Ok && empty.hasProject()
                        && body != nullptr && body->isCad()
                        && body->cadOrNull()->state().extrude.selection == CadSelectionKind::PlanarFaces
                        && body->cadOrNull()->state().extrude.planarFaces.size() == 3u
                        && history.undoDepth() == 0u);
    }

    // A first project of 19 faces on a 6 x 6 grid whose union PINCHES -- the
    // seven cells (row, col) (1,0) (1,1) (2,0) (2,2) (3,0) (3,1) (3,2) ring
    // the unchosen (2,1) and meet at one corner -- plus the whole of columns
    // 4 and 5. The commit refuses with the candidate's own reason, never a
    // selection message; dropping (2,2) fixes it and the commit succeeds.
    {
        ConstructionScene empty{NoProjectTag{}};
        ConstructionHistory history(empty);
        Driver d;
        const bool ready = d.finishOn(gridSketch(6, 6));
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 6, 6) : std::vector<size_t>{};
        const auto cell = [&](int row, int col) { return cells[row * 6 + col]; };
        bool chosen = cells.size() == 36u;
        if (chosen) {
            for (const auto& rc : {std::pair<int, int>{1, 0}, {1, 1}, {2, 0}, {2, 2}, {3, 0}, {3, 1},
                                   {3, 2}}) {
                chosen = chosen && d.sketch.togglePlanarFace(cell(rc.first, rc.second)) == CadStatus::Ok;
            }
            for (int row = 0; row < 6; ++row) {
                for (int col = 4; col < 6; ++col) {
                    chosen = chosen && d.sketch.togglePlanarFace(cell(row, col)) == CadStatus::Ok;
                }
            }
        }
        const CadStatus candidate = d.sketch.evaluateCandidate().status;
        const CadStatus refused =
                chosen ? commitFirstCadProject(d.sketch, empty, sculpt, history) : CadStatus::Ok;
        const bool refusedFresh = chosen && d.sketch.selectedAreaCount() == 19u
                                  && candidate == CadStatus::PlanarFacesTouchAtPoint
                                  && refused == candidate && d.sketch.lastStatus() == candidate
                                  && !empty.hasProject() && d.sketch.selectedAreaCount() == 19u;
        const bool fixed = refusedFresh && d.sketch.togglePlanarFace(cell(2, 2)) == CadStatus::Ok
                           && d.sketch.lastStatus() == CadStatus::Ok
                           && d.sketch.evaluateCandidate().status == CadStatus::Ok;
        const CadStatus committed =
                fixed ? commitFirstCadProject(d.sketch, empty, sculpt, history) : CadStatus::NotSketching;
        r.check("MF_15_a_refused_commit_names_the_current_candidate_reason_and_a_fix_commits",
                refusedFresh && fixed && committed == CadStatus::Ok && empty.hasProject());
    }

    // The same freshness through the ordinary commit of a live project.
    {
        ConstructionScene scene;
        ConstructionHistory history(scene);
        Driver d;
        const bool ready = d.finishOn(gridSketch(6, 6));
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 6, 6) : std::vector<size_t>{};
        const auto cell = [&](int row, int col) { return cells[row * 6 + col]; };
        bool chosen = cells.size() == 36u;
        if (chosen) {
            for (const auto& rc : {std::pair<int, int>{1, 0}, {1, 1}, {2, 0}, {2, 2}, {3, 0}, {3, 1},
                                   {3, 2}}) {
                chosen = chosen && d.sketch.togglePlanarFace(cell(rc.first, rc.second)) == CadStatus::Ok;
            }
            for (int row = 0; row < 6; ++row) {
                for (int col = 4; col < 6; ++col) {
                    chosen = chosen && d.sketch.togglePlanarFace(cell(row, col)) == CadStatus::Ok;
                }
            }
        }
        const size_t bodiesBefore = scene.bodyCount();
        ObjectId id = kNoObject;
        const CadStatus refused = chosen ? d.sketch.commit(scene, history, &id) : CadStatus::Ok;
        const bool refusedFresh = refused == CadStatus::PlanarFacesTouchAtPoint
                                  && d.sketch.lastStatus() == refused && id == kNoObject
                                  && scene.bodyCount() == bodiesBefore && history.undoDepth() == 0u;
        const bool fixed = refusedFresh && d.sketch.togglePlanarFace(cell(2, 2)) == CadStatus::Ok
                           && d.sketch.commit(scene, history, &id) == CadStatus::Ok && id != kNoObject;
        r.check("MF_15B_the_live_project_commit_names_the_same_current_reason_and_a_fix_commits",
                refusedFresh && fixed && scene.bodyCount() == bodiesBefore + 1u);
    }
}

// The device class's own sequence (`CadMultiFaceOwnerTest` DEV-MF-05), on its
// own 4-column x 6-row grid, pinned here first so the device asserts a fact
// the host already proved: seventeen cells whose union pinches at one corner,
// then sixteen once (row 2, col 2) is dropped.
void testDevicePattern(Checks& r) {
    Driver d;
    const bool ready = d.finishOn(gridSketch(4, 6));
    const std::vector<size_t> cells = ready ? gridCells(d.sketch, 4, 6) : std::vector<size_t>{};
    bool chosen = cells.size() == 24u;
    for (const auto& rc : {std::pair<int, int>{1, 0}, {1, 1}, {2, 0}, {2, 2}, {3, 0}, {3, 1}, {3, 2},
                           {0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3}, {5, 3}, {5, 0}, {5, 1}, {5, 2},
                           {0, 0}}) {
        chosen = chosen && d.sketch.togglePlanarFace(cells[rc.first * 4 + rc.second]) == CadStatus::Ok;
    }
    const CadStatus pinched = d.sketch.evaluateCandidate().status;
    const bool dropped = chosen && d.sketch.togglePlanarFace(cells[2 * 4 + 2]) == CadStatus::Ok;
    const CadCandidateEvaluation& fixed = d.sketch.evaluateCandidate();
    r.check("MF_15C_the_device_pattern_pinches_at_17_cells_and_extrudes_at_16",
            chosen && pinched == CadStatus::PlanarFacesTouchAtPoint && dropped
                    && d.sketch.selectedAreaCount() == 16u && fixed.status == CadStatus::Ok
                    && fixed.mesh != nullptr && fixed.mesh->components == 1u);
    d.sketch.cancel();
}

// ---------------------------------------------------------------------------
// Timings: bounded, single-shot, never a loop at startup
// ---------------------------------------------------------------------------

void measure(Checks& r, std::string* perf) {
    char line[320];
    // 32 faces, each followed by the candidate evaluation the UI asks for on
    // every tap: the per-tap cost a user feels.
    double tap32Max = 0.0;
    double tap32Total = 0.0;
    {
        Driver d;
        const bool ready = d.finishOn(gridSketch(8, 6));
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 8, 6) : std::vector<size_t>{};
        for (size_t k = 0; cells.size() == 48u && k < 32u; ++k) {
            const auto t0 = std::chrono::steady_clock::now();
            d.sketch.togglePlanarFace(cells[k]);
            d.sketch.evaluateCandidate();
            const double us = micros(t0);
            tap32Total += us;
            tap32Max = std::max(tap32Max, us);
        }
        d.sketch.cancel();
    }
    // 128 of a 12 x 12 grid's 144 faces: the toggles, then one evaluation.
    double toggle128 = 0.0;
    double eval128 = 0.0;
    bool ok128 = false;
    {
        Driver d;
        const bool ready = d.finishOn(gridSketch(12, 12));
        const std::vector<size_t> cells = ready ? gridCells(d.sketch, 12, 12) : std::vector<size_t>{};
        if (cells.size() == 144u) {
            const auto t0 = std::chrono::steady_clock::now();
            bool each = true;
            for (size_t k = 0; k < 128u; ++k) each = each && d.sketch.togglePlanarFace(cells[k]) == CadStatus::Ok;
            toggle128 = micros(t0);
            const auto t1 = std::chrono::steady_clock::now();
            ok128 = each && d.sketch.evaluateCandidate().status == CadStatus::Ok
                    && d.sketch.selectedAreaCount() == 128u;
            eval128 = micros(t1);
        }
        d.sketch.cancel();
    }
    r.check("MF_PERF_01_128_faces_of_a_144_face_grid_select_and_extrude", ok128);
    // The near-bound arrangement: a 63 x 63 grid (one rectangle and 124 side-
    // to-side lines: 3844 crossings + 248 T-junctions + 4 corners = exactly
    // the 4096-contact cap). Every face selected, then one evaluation.
    double finish3969 = 0.0;
    double toggleAll = 0.0;
    double evalAll = 0.0;
    size_t faces = 0;
    bool okAll = false;
    {
        Driver d;
        const auto t0 = std::chrono::steady_clock::now();
        const bool ready = d.finishOn(gridSketch(63, 63));
        finish3969 = micros(t0);
        faces = ready ? d.sketch.planarFaceCount() : 0u;
        if (faces == 63u * 63u) {
            const auto t1 = std::chrono::steady_clock::now();
            bool each = true;
            for (size_t k = 0; k < faces; ++k) each = each && d.sketch.togglePlanarFace(k) == CadStatus::Ok;
            toggleAll = micros(t1);
            const auto t2 = std::chrono::steady_clock::now();
            const CadCandidateEvaluation& every = d.sketch.evaluateCandidate();
            evalAll = micros(t2);
            okAll = each && every.status == CadStatus::Ok && every.mesh != nullptr
                    && every.mesh->components == 1u && d.sketch.selectedAreaCount() == faces;
        }
        d.sketch.cancel();
    }
    r.check("MF_PERF_02_every_face_of_the_near_bound_3969_face_arrangement_selects_and_extrudes",
            okAll && faces == 3969u);
    std::snprintf(line, sizeof(line),
                  " multiface_tap32_us=%.0f/%.0f multiface_toggle128_us=%.0f multiface_eval128_us=%.0f"
                  " multiface_all_faces=%zu finish_us=%.0f toggle_all_us=%.0f eval_all_us=%.0f",
                  tap32Total / 32.0, tap32Max, toggle128, eval128, faces, finish3969, toggleAll,
                  evalAll);
    *perf += line;
}

}  // namespace

void runCadMultiFaceSelfTests(std::vector<ArrangementSelfTestCheck>* out, std::string* performance) {
    Checks r{out};
    std::string perf;
    testBound(r);
    testSelection(r);
    testExtrusion(r, &perf);
    testLoopRegionsUnchanged(r);
    testStatus(r);
    testDevicePattern(r);
    measure(r, &perf);
    if (performance != nullptr) *performance = perf.empty() ? perf : perf.substr(1);
}

}  // namespace forgeshape
