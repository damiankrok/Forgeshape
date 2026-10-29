// Seat D diagnostic 3: Undo after an over-cap (NotRetained) stroke. Drives the
// same SculptHistory::record + SculptSession::undoStroke path a stroke close uses,
// with deltas built the way SculptStroke::buildDelta builds them. Scratch only.
#include <cstdio>
#include "forgeshape_sculpt.h"
#include "forgeshape_sculpt_history.h"
using namespace forgeshape;
int main() {
    const uint32_t side = 200;  // 40000 vertices: > 1 MiB / 28 B = 37449
    ConstructionMesh grid;
    for (uint32_t j = 0; j < side; ++j)
        for (uint32_t i = 0; i < side; ++i) {
            MeshVertex v{};
            v.position[0] = i * 0.01f; v.position[1] = j * 0.01f; v.position[2] = 0.0f;
            v.color[0] = v.color[1] = v.color[2] = 0.7f;
            grid.vertices.push_back(v);
        }
    for (uint32_t j = 0; j + 1 < side; ++j)
        for (uint32_t i = 0; i + 1 < side; ++i) {
            const uint32_t a = j * side + i, b = a + 1, c = a + side, d = c + 1;
            grid.indices.insert(grid.indices.end(), {a, b, d, a, d, c});
        }
    FrozenSculpt frozen;
    SculptSession session;
    session.bindTarget(&frozen);
    session.freezeToSculpt(grid, ObjectId{9}, nullptr);
    SculptMesh& mesh = frozen.mesh;
    const uint32_t n = mesh.vertexCount();
    auto pos = [&](uint32_t v) { return mesh.vertices()[v].position; };
    // Stroke 1: lifts vertex 0 and vertex 1 by +1 z. Retained.
    SculptStrokeDelta s1;
    for (uint32_t v : {0u, 1u}) {
        const Vec3 before{pos(v)[0], pos(v)[1], pos(v)[2]};
        const Vec3 after{before.x, before.y, before.z + 1.0f};
        s1.vertexIndices.push_back(v); s1.beforePositions.push_back(before); s1.afterPositions.push_back(after);
        mesh.setVertexPosition(v, after);
    }
    s1.beforeHasEdits = false; s1.afterHasEdits = true;
    mesh.advanceRevision();
    const auto o1 = frozen.history.record(s1);
    // Stroke 2: lifts EVERY vertex by +5 z (a big brush on a dense mesh). Not retained.
    SculptStrokeDelta s2;
    for (uint32_t v = 0; v < n; ++v) {
        const Vec3 before{pos(v)[0], pos(v)[1], pos(v)[2]};
        const Vec3 after{before.x, before.y, before.z + 5.0f};
        s2.vertexIndices.push_back(v); s2.beforePositions.push_back(before); s2.afterPositions.push_back(after);
        mesh.setVertexPosition(v, after);
    }
    s2.beforeHasEdits = true; s2.afterHasEdits = true;
    mesh.advanceRevision();
    const auto o2 = frozen.history.record(s2);
    std::printf("INFO record1=%s record2=%s undoDepth=%zu canUndo=%d\n",
                SculptHistory::recordOutcomeName(o1), SculptHistory::recordOutcomeName(o2),
                frozen.history.undoDepth(), (int)session.canUndoSculpt());
    std::printf("INFO before undo: z0=%.3f z1=%.3f z2=%.3f\n", pos(0)[2], pos(1)[2], pos(2)[2]);
    const auto u = session.undoStroke();
    std::printf("INFO undo=%s after undo: z0=%.3f z1=%.3f z2=%.3f hasEdits=%d\n",
                SculptSession::sculptHistoryStatusName(u), pos(0)[2], pos(1)[2], pos(2)[2],
                (int)mesh.hasEdits());
    // States on the branch: A (all 0), B (z0=z1=1, rest 0), C (z0=z1=6, rest 5).
    const bool onBranch = (pos(0)[2] == 0.0f && pos(2)[2] == 0.0f) || (pos(0)[2] == 1.0f && pos(2)[2] == 0.0f)
                          || (pos(0)[2] == 6.0f && pos(2)[2] == 5.0f);
    std::printf("RESULT undo_lands_on_a_state_that_existed=%d\n", (int)onBranch);
    return 0;
}
