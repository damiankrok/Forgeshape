// CR-04 (FUNCTION-COUNCIL-R2-CLOSEOUT-R1): Sculpt Undo / Redo / navigator jump
// after a stroke too large to retain (SculptHistory::RecordOutcome::NotRetained).
//
// Audit-only host diagnostic, never built into the product. Every stroke is a
// REAL brush stroke driven through SculptSession::beginStroke/updateStroke/
// endStroke (the product path that builds the delta with SculptStroke::
// buildDelta and records it with SculptHistory::record) — no hand-built delta.
//
// The checks state the CONTRACT, not today's behaviour:
//   K1 the oversized stroke APPLIES (geometry != state before it)
//   K2 and is said by name (notRetainedStrokes() advanced; nothing was pushed)
//   K3 after Undo the mesh equals SOME state that existed on the branch
//      (S0..Sn, S_big); a refused Undo that leaves S_big standing also passes
//   K4 hasEdits is truthful: true exactly when the mesh differs from S0
//   K5 Redo after that Undo also lands on a state that existed
//   K6 a navigator jump to row 0 lands on a state that existed
//   K7 hasEdits after the jump is truthful
// A check that PASSES under today's code is reported as PASS; nothing is
// expected to fail by construction.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "forgeshape_camera.h"
#include "forgeshape_construction.h"
#include "forgeshape_math.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_sculpt_history.h"

using namespace forgeshape;

namespace {
constexpr int kW = 1080;
constexpr int kH = 2400;

CameraSnapshot cameraAbove() {  // copied from forgeshape_sculpt_selftest.cpp cameraAt()
    const Vec3 eye{0.0f, 6.0f, 0.0f};
    CameraSnapshot s{};
    s.eye = eye;
    s.target = Vec3{0.0f, 0.0f, 0.0f};
    s.distance = 6.0f;
    s.projection = ProjectionMode::Perspective;
    s.orthoHalfHeightMeters = s.distance * std::tan(kFovYRadians * 0.5f);
    s.view = mat4LookAt(eye, s.target, Vec3{0.0f, 0.0f, 1.0f});
    s.proj = mat4Perspective(kFovYRadians, float(kW) / float(kH), kNearPlane, kFarPlane);
    return s;
}

// A dense +Y-facing grid in the XZ plane: side*side vertices, spacing 0.01 m.
ConstructionMesh denseGrid(uint32_t side) {
    ConstructionMesh m;
    const float half = 0.5f * 0.01f * float(side - 1);
    for (uint32_t j = 0; j < side; ++j)
        for (uint32_t i = 0; i < side; ++i) {
            MeshVertex v{};
            v.position[0] = -half + 0.01f * float(i);
            v.position[1] = 0.0f;
            v.position[2] = -half + 0.01f * float(j);
            v.color[0] = v.color[1] = v.color[2] = 0.7f;
            m.vertices.push_back(v);
        }
    for (uint32_t j = 0; j + 1 < side; ++j)
        for (uint32_t i = 0; i + 1 < side; ++i) {
            const uint32_t a = j * side + i, b = a + 1, c = a + side, d = c + 1;
            m.indices.insert(m.indices.end(), {a, d, b, a, c, d});  // +Y normal
        }
    return m;
}

std::vector<Vec3> snap(const SculptMesh& mesh) {
    std::vector<Vec3> out;
    for (uint32_t v = 0; v < mesh.vertexCount(); ++v) out.push_back(mesh.vertexPosition(v));
    return out;
}

struct Diff { double maxAbs; size_t count; };
Diff diff(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    Diff d{0.0, 0};
    for (size_t i = 0; i < a.size(); ++i) {
        const double e = std::fmax(std::fabs(double(a[i].x) - b[i].x),
                         std::fmax(std::fabs(double(a[i].y) - b[i].y), std::fabs(double(a[i].z) - b[i].z)));
        if (e > 0.0) ++d.count;
        if (e > d.maxAbs) d.maxAbs = e;
    }
    return d;
}

bool stroke(SculptSession& s, const CameraSnapshot& cam, float x, float y, float radiusPx) {
    s.setRadiusPixels(radiusPx);
    const Mat4 id = mat4Identity();
    if (!s.beginStroke(cam, x, y, kW, kH, id, id)) return false;
    bool moved = false;
    for (int i = 0; i < 4; ++i)
        if (s.updateStroke(x + 6.0f * float(i + 1), y)) moved = true;
    s.endStroke();
    return moved;
}

int gFail = 0;
void check(const char* name, bool ok) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++gFail;
}

// Index of the existing state the mesh equals exactly (max abs diff 0), or -1.
int matchExisting(const std::vector<Vec3>& now, const std::vector<std::vector<Vec3>>& states,
                  const char* label) {
    int match = -1;
    double best = 1e30;
    for (size_t k = 0; k < states.size(); ++k) {
        const Diff d = diff(now, states[k]);
        std::printf("  %s vs %s: maxAbs=%.6f differingVertices=%zu\n", label,
                    k + 1 == states.size() ? "S_big(or dup of Sn in control A)" : ("S" + std::to_string(k)).c_str(), d.maxAbs, d.count);
        if (d.maxAbs < best) best = d.maxAbs;
        if (d.maxAbs == 0.0 && match < 0) match = int(k);
    }
    std::printf("  %s nearest existing state maxAbs=%.6f match=%d\n", label, best, match);
    return match;
}

// One scenario. `normalStrokes` small strokes, then (optionally) one big one.
void scenario(const char* title, uint32_t normalStrokes, bool bigStroke) {
    std::printf("=== SCENARIO %s (normal=%u big=%d)\n", title, normalStrokes, int(bigStroke));
    const CameraSnapshot cam = cameraAbove();
    FrozenSculpt frozen;
    SculptSession s;
    s.bindTarget(&frozen);
    const bool froze = s.freezeToSculpt(denseGrid(250), ObjectId{9}, nullptr);
    s.setTool(SculptTool::Clay);
    s.setStrength(1.0f);
    std::printf("INFO froze=%d vertices=%u hitsCentre=%d mode_sculpt=%d\n", int(froze), s.mesh().vertexCount(),
                int(s.hitsSculptMesh(cam, kW * 0.5f, kH * 0.5f, kW, kH, mat4Identity())), int(s.inSculptMode()));

    std::vector<std::vector<Vec3>> states;  // S0..Sn, then S_big (if any)
    states.push_back(snap(s.mesh()));
    for (uint32_t k = 0; k < normalStrokes; ++k) {
        const float x = kW * 0.5f - 300.0f + 150.0f * float(k);
        const float y = kH * 0.5f - 200.0f + 100.0f * float(k);
        const bool moved = stroke(s, cam, x, y, 24.0f);
        const SculptStrokeDelta& top = s.history().undoTop();
        std::printf("INFO stroke%u moved=%d undoDepth=%zu topVertices=%zu topPayload=%zu notRetained=%llu\n", k + 1,
                    int(moved), s.history().undoDepth(), top.vertexIndices.size(), top.payloadBytes(),
                    (unsigned long long)s.history().notRetainedStrokes());
        states.push_back(snap(s.mesh()));
    }
    const size_t depthBefore = s.history().undoDepth();
    const uint64_t nrBefore = s.history().notRetainedStrokes();
    if (bigStroke) {
        const bool moved = stroke(s, cam, kW * 0.5f, kH * 0.5f, kMaxBrushRadiusPixels);
        states.push_back(snap(s.mesh()));
        const Diff applied = diff(states.back(), states[states.size() - 2]);
        std::printf("INFO big stroke moved=%d verticesChanged=%zu payloadIfRetained~=%zu cap=%zu undoDepth=%zu "
                    "notRetained=%llu hasEdits=%d\n",
                    int(moved), applied.count, applied.count * 28 + 128, kMaxSculptHistoryEntryBytes,
                    s.history().undoDepth(), (unsigned long long)s.history().notRetainedStrokes(),
                    int(s.mesh().hasEdits()));
        check("K1_oversized_stroke_applies", applied.count > 0);
        check("K2_and_is_named_not_retained_and_not_pushed",
              s.history().notRetainedStrokes() == nrBefore + 1 && s.history().undoDepth() == depthBefore);
    } else {
        states.push_back(states.back());  // S_big slot = current state, keeps indexing uniform
    }
    const std::vector<Vec3>& s0 = states.front();

    const SculptHistoryCursor c0 = s.history().cursor();
    std::printf("INFO navigator before undo: cursor=%zu stateCount=%zu\n", c0.cursor, c0.stateCount);

    const auto u = s.undoStroke();
    const std::vector<Vec3> afterUndo = snap(s.mesh());
    std::printf("INFO undo=%s hasEdits=%d\n", SculptSession::sculptHistoryStatusName(u), int(s.mesh().hasEdits()));
    const int m3 = matchExisting(afterUndo, states, "afterUndo");
    check("K3_undo_lands_on_a_state_that_existed", m3 >= 0);
    const bool editedTruth = diff(afterUndo, s0).count > 0;
    std::printf("INFO afterUndo differsFromS0=%d hasEdits=%d\n", int(editedTruth), int(s.mesh().hasEdits()));
    check("K4_hasEdits_truthful_after_undo", s.mesh().hasEdits() == editedTruth);

    const auto rd = s.redoStroke();
    const std::vector<Vec3> afterRedo = snap(s.mesh());
    std::printf("INFO redo=%s hasEdits=%d\n", SculptSession::sculptHistoryStatusName(rd), int(s.mesh().hasEdits()));
    const int m5 = matchExisting(afterRedo, states, "afterRedo");
    check("K5_redo_lands_on_a_state_that_existed", m5 >= 0);

    const auto j = s.jumpToHistoryCursor(0);
    const std::vector<Vec3> afterJump = snap(s.mesh());
    std::printf("INFO jump0=%s hasEdits=%d\n", SculptSession::sculptHistoryStatusName(j), int(s.mesh().hasEdits()));
    const int m6 = matchExisting(afterJump, states, "afterJump0");
    check("K6_navigator_row0_is_a_state_that_existed", m6 >= 0);
    check("K7_hasEdits_truthful_after_jump", s.mesh().hasEdits() == (diff(afterJump, s0).count > 0));
}
}  // namespace

int main() {
    std::printf("INFO kMaxSculptHistoryEntryBytes=%zu entries=%zu total=%zu\n", kMaxSculptHistoryEntryBytes,
                kMaxSculptHistoryEntries, kMaxSculptHistoryBytes);
    // Control A: no oversized stroke. Every check must PASS.
    scenario("CONTROL_A_no_big_stroke", 3, false);
    // Control B: the oversized stroke on an EMPTY undo stack (SCUNDO_23's shape).
    scenario("CONTROL_B_big_stroke_on_empty_stack", 0, true);
    // Subject: N=3 retained strokes, then the oversized stroke, then Undo.
    scenario("SUBJECT_big_stroke_after_3_retained", 3, true);
    // Subject 2: N=1, so the one retained entry's before-side is the fresh
    // Freeze (beforeHasEdits=false) -- the shape of council-R2 D-01 / diag_d3.
    scenario("SUBJECT_big_stroke_after_1_retained", 1, true);
    std::printf("RESULT failures=%d\n", gFail);
    return gFail == 0 ? 0 : 1;
}
