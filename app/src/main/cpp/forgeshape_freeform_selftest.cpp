#include "forgeshape_freeform_selftest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "forgeshape_body_commands.h"
#include "forgeshape_body_delete.h"
#include "forgeshape_freeform.h"
#include "forgeshape_freeform_session.h"
#include "forgeshape_freeform_subdivision.h"
#include "forgeshape_gizmo.h"
#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_freeform.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"

namespace forgeshape {
namespace {

struct Checks {
    FreeformSelfTestResult* out;
    int maxOut;
    int count = 0;
    void check(const char* name, bool ok) {
        if (count < maxOut) {
            out[count] = FreeformSelfTestResult{name, ok};
        }
        ++count;
    }
};

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

DVec3 at(const FreeformCage& cage, uint32_t vertexId) {
    const FreeformVertex* v = findFreeformVertex(cage, FreeformVertexId{vertexId});
    return v != nullptr ? v->position : DVec3{std::nan(""), std::nan(""), std::nan("")};
}

bool sameBits(double a, double b) { return std::memcmp(&a, &b, sizeof(double)) == 0; }

bool exactly(const DVec3& a, const DVec3& b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
}

bool near(const DVec3& a, const DVec3& b, double tolerance) {
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance
           && std::fabs(a.z - b.z) <= tolerance;
}

// Positions bit for bit, ids and topology aside.
bool samePositions(const FreeformCage& a, const FreeformCage& b) {
    if (a.vertices.size() != b.vertices.size()) return false;
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].id != b.vertices[i].id || !exactly(a.vertices[i].position, b.vertices[i].position)) {
            return false;
        }
    }
    return true;
}

// Ids, loops and edges equal: the topology and its identity, positions aside.
bool sameTopology(const FreeformCage& a, const FreeformCage& b) {
    if (a.edges.size() != b.edges.size() || a.faces.size() != b.faces.size()
        || a.vertices.size() != b.vertices.size()) {
        return false;
    }
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].id != b.vertices[i].id) return false;
    }
    for (size_t i = 0; i < a.edges.size(); ++i) {
        if (a.edges[i].id != b.edges[i].id || a.edges[i].v0 != b.edges[i].v0 || a.edges[i].v1 != b.edges[i].v1) {
            return false;
        }
    }
    for (size_t i = 0; i < a.faces.size(); ++i) {
        if (a.faces[i].id != b.faces[i].id || a.faces[i].loop != b.faces[i].loop) return false;
    }
    return a.nextVertexId == b.nextVertexId && a.nextEdgeId == b.nextEdgeId && a.nextFaceId == b.nextFaceId;
}

bool idsSequential(const FreeformCage& cage) {
    for (size_t i = 0; i < cage.vertices.size(); ++i) {
        if (idOf(cage.vertices[i].id) != i + 1u) return false;
    }
    for (size_t i = 0; i < cage.edges.size(); ++i) {
        if (idOf(cage.edges[i].id) != i + 1u) return false;
    }
    for (size_t i = 0; i < cage.faces.size(); ++i) {
        if (idOf(cage.faces[i].id) != i + 1u) return false;
    }
    return cage.nextVertexId == cage.vertices.size() + 1u && cage.nextEdgeId == cage.edges.size() + 1u
           && cage.nextFaceId == cage.faces.size() + 1u;
}

DVec3 faceCentroid(const FreeformCage& cage, const FreeformFace& face) {
    DVec3 sum{0.0, 0.0, 0.0};
    for (FreeformVertexId v : face.loop) sum = dvec3Add(sum, at(cage, idOf(v)));
    return dvec3Scale(sum, 0.25);
}

// Every face's outward normal points away from the origin: true of every
// creation form, which is centred on it and convex.
bool facesPointOutward(const FreeformCage& cage) {
    for (const FreeformFace& face : cage.faces) {
        DVec3 n;
        if (!freeformFaceNormal(cage, face, &n)) return false;
        if (!(dvec3Dot(n, faceCentroid(cage, face)) > 0.0)) return false;
    }
    return true;
}

bool canonicalFaces(const FreeformCage& cage) {
    for (const FreeformFace& face : cage.faces) {
        for (int k = 1; k < 4; ++k) {
            if (idOf(face.loop[k]) <= idOf(face.loop[0])) return false;
        }
    }
    return true;
}

int boundaryEdgeCount(const FreeformCage& cage) {
    FreeformTopology t;
    if (buildFreeformTopology(cage, &t) != FreeformStatus::Ok) return -1;
    int count = 0;
    for (uint8_t faces : t.edgeFaceCount) count += faces == 1u ? 1 : 0;
    return count;
}

// A flat nx x nz grid of quads on XZ facing +Y, spanning [-1, 1]^2.
FreeformCage gridCage(int nx, int nz, uint8_t level) {
    std::vector<DVec3> p;
    for (int i = 0; i <= nx; ++i) {
        for (int j = 0; j <= nz; ++j) {
            p.push_back(DVec3{-1.0 + 2.0 * i / nx, 0.0, -1.0 + 2.0 * j / nz});
        }
    }
    auto index = [nz](int i, int j) { return static_cast<uint32_t>(i * (nz + 1) + j); };
    std::vector<std::array<uint32_t, 4>> q;
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < nz; ++j) {
            q.push_back({index(i, j), index(i, j + 1), index(i + 1, j + 1), index(i + 1, j)});
        }
    }
    return assembleFreeformCage(p, q, level);
}

// The box's own ids (makeFreeformBox): vertices 1..8 are (-,-,-) (+,-,-)
// (+,+,-) (-,+,-) (-,-,+) (+,-,+) (+,+,+) (-,+,+); faces 1..6 are -Z +Z -Y +Y
// -X +X; edge 4 joins vertices 1 and 2 (an X-direction edge).
constexpr uint32_t kBoxFaceNegZ = 1;
constexpr uint32_t kBoxFacePosZ = 2;
constexpr uint32_t kBoxFacePosY = 4;
constexpr uint32_t kBoxFacePosX = 6;
constexpr uint32_t kBoxEdgeX = 4;

FreeformAffine translation(double x, double y, double z) {
    FreeformAffine affine;
    affine.translation = DVec3{x, y, z};
    return affine;
}

// A cage whose every edge ring crosses some face twice: a three-quad "twisted"
// torus (each quad's four corners on just three abstract vertices, glued with
// a half twist) refined twice by Catmull-Clark's TOPOLOGY, so it is an honest
// manifold, orientable, all-quad cage of 48 vertices, 96 edges and 48 faces.
// The ring through edge 1 re-enters a face it has already crossed, so no single
// loop exists there and an insertion must be refused rather than split one face
// twice. Positions are distinct points on a unit sphere (a golden-angle spiral):
// only the topology matters, but no face may be degenerate.
FreeformCage twistedRingCage() {
    static const uint32_t kQuads[48][4] = {
            {0, 12, 36, 15},  {3, 13, 36, 12},  {9, 14, 36, 13},  {4, 15, 36, 14},
            {1, 16, 37, 18},  {6, 17, 37, 16},  {9, 13, 37, 17},  {3, 18, 37, 13},
            {2, 19, 38, 21},  {7, 20, 38, 19},  {9, 17, 38, 20},  {6, 21, 38, 17},
            {1, 22, 39, 23},  {4, 14, 39, 22},  {9, 20, 39, 14},  {7, 23, 39, 20},
            {0, 15, 40, 26},  {4, 24, 40, 15},  {10, 25, 40, 24}, {5, 26, 40, 25},
            {1, 27, 41, 22},  {8, 28, 41, 27},  {10, 24, 41, 28}, {4, 22, 41, 24},
            {1, 23, 42, 30},  {7, 29, 42, 23},  {10, 28, 42, 29}, {8, 30, 42, 28},
            {2, 31, 43, 19},  {5, 25, 43, 31},  {10, 29, 43, 25}, {7, 19, 43, 29},
            {0, 26, 44, 12},  {5, 32, 44, 26},  {11, 33, 44, 32}, {3, 12, 44, 33},
            {2, 21, 45, 31},  {6, 34, 45, 21},  {11, 32, 45, 34}, {5, 31, 45, 32},
            {1, 30, 46, 16},  {8, 35, 46, 30},  {11, 34, 46, 35}, {6, 16, 46, 34},
            {1, 18, 47, 27},  {3, 33, 47, 18},  {11, 35, 47, 33}, {8, 27, 47, 35},
    };
    std::vector<DVec3> p;
    for (int i = 0; i < 48; ++i) {
        const double t = i * 2.399963;
        const double z = 1.0 - 2.0 * (i + 0.5) / 48.0;
        const double r = std::sqrt(1.0 - z * z);
        p.push_back(DVec3{r * std::cos(t), r * std::sin(t), z});
    }
    std::vector<std::array<uint32_t, 4>> q;
    for (const auto& quad : kQuads) q.push_back({quad[0], quad[1], quad[2], quad[3]});
    return assembleFreeformCage(p, q, 0);
}

// ---------------------------------------------------------------------------
// Scene, history and file helpers
// ---------------------------------------------------------------------------

struct Rig {
    ConstructionScene scene{NoProjectTag{}};
    ConstructionHistory history{scene};
    ObjectId id = kNoObject;

    explicit Rig(FreeformCage cage) {
        history.beginSessionInitialization();
        SceneObject* body = scene.addFreeformBody(std::move(cage));
        if (body != nullptr) {
            id = body->objectId();
            publishSceneObject(*body);
        }
        history.endSessionInitialization();
    }
    SceneObject* body() { return scene.findBody(id); }
    FreeformBody* freeform() {
        SceneObject* b = body();
        return b != nullptr ? b->freeformOrNull() : nullptr;
    }
    const FreeformCage& cage() { return freeform()->cage(); }
    // One user gesture: one Construction edit around every sample it applied.
    FreeformStatus gesture(const std::vector<FreeformCage>& samples) {
        ScopedConstructionEdit edit(history);
        FreeformStatus last = FreeformStatus::Ok;
        for (const FreeformCage& sample : samples) {
            last = freeform()->applyCage(std::make_shared<const FreeformCage>(sample));
            if (last != FreeformStatus::Ok) return last;
            publishSceneObject(*body());
        }
        return last;
    }
    std::vector<uint8_t> bytes() const {
        return encodeProjectV1(captureProjectDocument(scene, ProjectKind::Construction));
    }
    uint64_t fingerprint() const { return projectSemanticFingerprint(scene, ProjectKind::Construction); }
};

ProjectDocument documentFor(const FreeformCage& cage) {
    ConstructionScene scene((NoProjectTag()));
    SceneObject* object = scene.addFreeformBody(cage);
    if (object == nullptr) return ProjectDocument{};
    publishSceneObject(*object);
    return captureProjectDocument(scene, ProjectKind::Construction);
}

// The offset of a section's 24-byte header, or 0 when absent.
size_t sectionOffset(const std::vector<uint8_t>& bytes, const char tag[4]) {
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t length = 0;
        for (int i = 7; i >= 0; --i) length = (length << 8) | bytes[offset + 8u + static_cast<size_t>(i)];
        if (std::memcmp(&bytes[offset], tag, 4) == 0) return offset;
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(length);
    }
    return 0;
}

uint64_t sectionLength(const std::vector<uint8_t>& bytes, size_t offset) {
    uint64_t length = 0;
    for (int i = 7; i >= 0; --i) length = (length << 8) | bytes[offset + 8u + static_cast<size_t>(i)];
    return length;
}

void putU32(std::vector<uint8_t>* bytes, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) (*bytes)[at + static_cast<size_t>(i)] = static_cast<uint8_t>(value >> (8 * i));
}

void putU16(std::vector<uint8_t>* bytes, size_t at, uint16_t value) {
    (*bytes)[at] = static_cast<uint8_t>(value);
    (*bytes)[at + 1u] = static_cast<uint8_t>(value >> 8);
}

// Re-seals the FRFM section's CRC after an in-place payload patch, so the
// decoder reaches the payload rule under test rather than refusing the CRC.
void resealFreeform(std::vector<uint8_t>* bytes) {
    const size_t offset = sectionOffset(*bytes, kSectionTagFreeform);
    const uint64_t length = sectionLength(*bytes, offset);
    const size_t payload = offset + kForgeSectionHeaderBytes;
    putU32(bytes, offset + 16u, crc32IsoHdlc(bytes->data() + payload, static_cast<size_t>(length)));
}

ProjectCodecStatus decodeStatus(const std::vector<uint8_t>& bytes) {
    ProjectDocument decoded;
    return decodeProject(bytes.data(), bytes.size(), &decoded);
}

// ---------------------------------------------------------------------------
// The corpus documents (DATA_PACKAGE_SPEC.md §7j; scripts/build-forge-corpus.ps1)
// ---------------------------------------------------------------------------

// freeform_crease_symmetry_v1: the box, symmetric about X, its +X face
// extruded 0.25 (and so its -X face too), the four edges of its +Y face creased
// 0.75, at level 3.
FreeformCage creaseSymmetryCage() {
    FreeformCage cage = makeFreeformBox();
    FreeformCage next;
    freeformSetSymmetry(cage, kFreeformSymmetryX, &next);
    cage = next;
    freeformExtrudeFaces(cage, {kBoxFacePosX}, 0.25, &next);
    cage = next;
    std::vector<uint32_t> topEdges;
    const FreeformFace* top = findFreeformFace(cage, FreeformFaceId{kBoxFacePosY});
    for (int k = 0; k < 4; ++k) {
        topEdges.push_back(idOf(findFreeformEdgeBetween(cage, top->loop[k], top->loop[(k + 1) % 4])->id));
    }
    freeformSetCrease(cage, topEdges, 0.75, &next);
    cage = next;
    freeformSetSubdivisionLevel(cage, 3, &next);
    return next;
}

// freeform_bad_topology_v1: the box with face 1 wound the wrong way round --
// every one of its edges is then walked the same way by two faces.
FreeformCage badTopologyCage() {
    FreeformCage cage = makeFreeformBox();
    std::array<FreeformVertexId, 4>& loop = cage.faces[0].loop;
    std::swap(loop[1], loop[3]);
    return cage;
}

std::string g_fixtureDigests;
std::string g_performance;

// ---------------------------------------------------------------------------
// FF-01 .. FF-03: the creation forms
// ---------------------------------------------------------------------------

void testCreation(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    r.check("FF_01_box_cage_is_valid_with_stable_sequential_ids",
            validateFreeformCage(box) == FreeformStatus::Ok && box.vertices.size() == 8u
                    && box.edges.size() == 12u && box.faces.size() == 6u && idsSequential(box)
                    && box.subdivisionLevel == 2u && box.symmetry == 0u);
    r.check("FF_01_box_faces_are_canonical_outward_and_closed",
            canonicalFaces(box) && facesPointOutward(box) && !freeformCageHasBoundary(box));
    const FreeformEdge* edgeX = findFreeformEdge(box, FreeformEdgeId{kBoxEdgeX});
    r.check("FF_01_box_is_deterministic_and_its_ids_are_a_function_of_its_shape",
            sameFreeformCage(box, makeFreeformBox()) && edgeX != nullptr && idOf(edgeX->v0) == 1u
                    && idOf(edgeX->v1) == 2u
                    && exactly(at(box, 7), DVec3{0.5, 0.5, 0.5}));

    const FreeformCage plane = makeFreeformPlane();
    bool flatUp = true;
    for (const FreeformFace& face : plane.faces) {
        DVec3 n;
        flatUp = flatUp && freeformFaceNormal(plane, face, &n) && n.y == 1.0;
    }
    for (const FreeformVertex& v : plane.vertices) flatUp = flatUp && v.position.y == 0.0;
    r.check("FF_02_plane_cage_is_valid_open_and_faces_up",
            validateFreeformCage(plane) == FreeformStatus::Ok && plane.vertices.size() == 25u
                    && plane.edges.size() == 40u && plane.faces.size() == 16u && idsSequential(plane)
                    && freeformCageHasBoundary(plane) && boundaryEdgeCount(plane) == 16 && flatUp
                    && canonicalFaces(plane));

    const FreeformCage cylinder = makeFreeformCylinder();
    FreeformCage symmetric;
    r.check("FF_03_cylinder_cage_is_valid_closed_and_outward",
            validateFreeformCage(cylinder) == FreeformStatus::Ok && cylinder.vertices.size() == 18u
                    && cylinder.edges.size() == 32u && cylinder.faces.size() == 16u
                    && idsSequential(cylinder) && !freeformCageHasBoundary(cylinder)
                    && facesPointOutward(cylinder) && canonicalFaces(cylinder));
    r.check("FF_03_every_creation_form_is_exactly_symmetric_about_all_three_planes",
            freeformSetSymmetry(cylinder, kFreeformSymmetryMask, &symmetric) == FreeformStatus::Ok
                    && samePositions(symmetric, cylinder)
                    && freeformSetSymmetry(box, kFreeformSymmetryMask, &symmetric) == FreeformStatus::Ok
                    && samePositions(symmetric, box)
                    && freeformSetSymmetry(plane, kFreeformSymmetryMask, &symmetric) == FreeformStatus::Ok
                    && samePositions(symmetric, plane));
}

// ---------------------------------------------------------------------------
// FF-04 .. FF-07: Catmull-Clark
// ---------------------------------------------------------------------------

bool hasPosition(const FreeformMesh& mesh, const DVec3& p, double tolerance) {
    for (const DVec3& q : mesh.positions) {
        if (near(p, q, tolerance)) return true;
    }
    return false;
}

void testSubdivision(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    FreeformMesh a;
    FreeformMesh b;
    const bool built = subdivideFreeformCageAt(box, 1, &a) == FreeformStatus::Ok
                       && subdivideFreeformCageAt(box, 1, &b) == FreeformStatus::Ok;
    std::vector<int> perFace(7, 0);
    for (FreeformFaceId face : a.quadFace) {
        if (idOf(face) >= 1u && idOf(face) <= 6u) ++perFace[idOf(face)];
    }
    bool fourEach = true;
    for (int f = 1; f <= 6; ++f) fourEach = fourEach && perFace[f] == 4;
    r.check("FF_04_level_1_has_the_catmull_clark_counts_and_is_deterministic",
            built && a.positions.size() == 26u && a.quads.size() == 96u && a.level == 1u && fourEach
                    && freeformMeshDigest(a) == freeformMeshDigest(b) && a.positions.size() == b.positions.size()
                    && a.render.indices.size() == 144u && a.triangleFace.size() == 48u
                    && !a.render.renderBothSides);
    // The corner's vertex point: (Q + 2R + (n - 3) V) / n with n = 3, Q the mean
    // of its three face points and R of its three edge midpoints -- 5/18 on
    // each axis. An edge point: (v0 + v1 + F0 + F1) / 4.
    const double corner = 5.0 / 18.0;
    r.check("FF_04_level_1_vertex_and_edge_points_follow_the_rules",
            hasPosition(a, DVec3{corner, corner, corner}, 1e-15)
                    && hasPosition(a, DVec3{-corner, -corner, -corner}, 1e-15)
                    && hasPosition(a, DVec3{0.0, -0.375, -0.375}, 1e-15)
                    && hasPosition(a, DVec3{0.0, 0.0, 0.5}, 0.0));
    FreeformMesh zero;
    r.check("FF_04_level_0_is_the_cage_itself",
            subdivideFreeformCageAt(box, 0, &zero) == FreeformStatus::Ok && zero.positions.size() == 8u
                    && zero.quads.size() == 24u && exactly(zero.positions[6], at(box, 7)));

    FreeformMesh four;
    FreeformMesh fourAgain;
    bool inside = subdivideFreeformCageAt(box, 4, &four) == FreeformStatus::Ok
                  && subdivideFreeformCageAt(box, 4, &fourAgain) == FreeformStatus::Ok;
    for (const DVec3& p : four.positions) {
        inside = inside && dvec3Finite(p) && std::fabs(p.x) < 0.5 && std::fabs(p.y) < 0.5
                 && std::fabs(p.z) < 0.5;
    }
    r.check("FF_05_level_4_is_bounded_inside_its_hull_and_deterministic",
            inside && four.quads.size() == 6u * 256u * 4u
                    && freeformMeshDigest(four) == freeformMeshDigest(fourAgain)
                    && four.quads.size() / 4u <= kMaxFreeformDerivedQuads);
    // The derived budget is a rule on the cage: 512 faces reach exactly the
    // budget at level 4; 2116 faces are allowed level 2 and refused level 3.
    const FreeformCage exact = gridCage(16, 32, 4);
    const FreeformCage large = gridCage(46, 46, 2);
    FreeformCage refused = large;
    FreeformMesh atBudget;
    r.check("FF_05_the_derived_quad_budget_is_enforced_on_the_cage_by_name",
            validateFreeformCage(exact) == FreeformStatus::Ok
                    && subdivideFreeformCage(exact, &atBudget) == FreeformStatus::Ok
                    && atBudget.quads.size() / 4u == kMaxFreeformDerivedQuads
                    && validateFreeformCage(large) == FreeformStatus::Ok
                    && freeformSetSubdivisionLevel(large, 3, &refused)
                               == FreeformStatus::SubdivisionBudgetExceeded
                    && sameFreeformCage(refused, large)
                    && freeformSetSubdivisionLevel(box, 5, nullptr) == FreeformStatus::InvalidSubdivisionLevel
                    && freeformSetSubdivisionLevel(box, -1, nullptr) == FreeformStatus::InvalidSubdivisionLevel
                    && subdivideFreeformCageAt(box, 5, &zero) == FreeformStatus::InvalidSubdivisionLevel);

    // FF-06: boundaries. A flat open sheet stays flat, its corners hold, and its
    // border stays on the border -- whatever an interior vertex does.
    FreeformCage plane = makeFreeformPlane();
    FreeformMesh sheet;
    bool flat = subdivideFreeformCageAt(plane, 2, &sheet) == FreeformStatus::Ok;
    int onEdge = 0;
    for (const DVec3& p : sheet.positions) {
        flat = flat && p.y == 0.0 && std::fabs(p.x) <= 1.0 && std::fabs(p.z) <= 1.0;
        onEdge += p.z == -1.0 ? 1 : 0;
    }
    r.check("FF_06_an_open_cage_subdivides_with_fixed_corners_and_a_border_that_stays",
            flat && onEdge == 17 && hasPosition(sheet, DVec3{-1.0, 0.0, -1.0}, 0.0)
                    && hasPosition(sheet, DVec3{1.0, 0.0, 1.0}, 0.0) && sheet.render.renderBothSides);
    FreeformCage lifted;
    FreeformMesh liftedSheet;
    bool borderHeld = freeformTransform(plane, FreeformElement::Vertex, {13}, translation(0.0, 0.5, 0.0), &lifted)
                              == FreeformStatus::Ok
                      && subdivideFreeformCageAt(lifted, 2, &liftedSheet) == FreeformStatus::Ok;
    bool interiorMoved = false;
    for (const DVec3& p : liftedSheet.positions) {
        if (std::fabs(p.x) == 1.0 || std::fabs(p.z) == 1.0) borderHeld = borderHeld && p.y == 0.0;
        interiorMoved = interiorMoved || p.y > 0.0;
    }
    r.check("FF_06_the_border_curve_depends_only_on_border_vertices",
            borderHeld && interiorMoved);

    // FF-07: creases change the derived surface and never the control truth.
    std::vector<uint32_t> allEdges;
    for (const FreeformEdge& e : box.edges) allEdges.push_back(idOf(e.id));
    FreeformCage sharp;
    FreeformCage half;
    FreeformMesh smoothMesh;
    FreeformMesh sharpMesh;
    FreeformMesh halfMesh;
    bool ok = freeformSetCrease(box, allEdges, 1.0, &sharp) == FreeformStatus::Ok
              && freeformSetCrease(box, allEdges, 0.5, &half) == FreeformStatus::Ok
              && subdivideFreeformCageAt(box, 2, &smoothMesh) == FreeformStatus::Ok
              && subdivideFreeformCageAt(sharp, 2, &sharpMesh) == FreeformStatus::Ok
              && subdivideFreeformCageAt(half, 1, &halfMesh) == FreeformStatus::Ok;
    bool onCube = ok;
    for (const DVec3& p : sharpMesh.positions) {
        const double m = std::max(std::fabs(p.x), std::max(std::fabs(p.y), std::fabs(p.z)));
        onCube = onCube && std::fabs(m - 0.5) <= 1e-12;
    }
    bool shrunk = ok;
    for (const DVec3& p : smoothMesh.positions) {
        shrunk = shrunk && std::max(std::fabs(p.x), std::max(std::fabs(p.y), std::fabs(p.z))) < 0.5;
    }
    double halfCorner = 0.0;
    for (const DVec3& p : halfMesh.positions) {
        if (p.x > 0.0 && p.x == p.y && p.y == p.z) halfCorner = p.x;
    }
    r.check("FF_07_a_full_crease_keeps_the_box_corners_and_faces_exactly",
            onCube && hasPosition(sharpMesh, DVec3{0.5, 0.5, 0.5}, 0.0)
                    && hasPosition(sharpMesh, DVec3{-0.5, -0.5, -0.5}, 0.0) && shrunk);
    r.check("FF_07_crease_weight_is_continuous_between_smooth_and_sharp",
            halfCorner > 5.0 / 18.0 && halfCorner < 0.5
                    && freeformMeshDigest(sharpMesh) != freeformMeshDigest(smoothMesh));
    r.check("FF_07_a_crease_moves_no_control_vertex_and_no_id",
            samePositions(sharp, box) && sameTopology(sharp, box) && !sameFreeformCage(sharp, box)
                    && sharp.edges[0].crease == 1.0);
    r.check("FF_07_an_out_of_range_crease_is_refused_by_name",
            freeformSetCrease(box, {1}, 1.5, nullptr) == FreeformStatus::InvalidCrease
                    && freeformSetCrease(box, {1}, -0.1, nullptr) == FreeformStatus::InvalidCrease
                    && freeformSetCrease(box, {1}, std::nan(""), nullptr) == FreeformStatus::NonFinite
                    && freeformSetCrease(box, {99}, 0.5, nullptr) == FreeformStatus::UnknownEdge
                    && freeformSetCrease(box, {}, 0.5, nullptr) == FreeformStatus::EmptySelection);
}

// ---------------------------------------------------------------------------
// FF-08 .. FF-13: the tools
// ---------------------------------------------------------------------------

bool othersUnchanged(const FreeformCage& before, const FreeformCage& after, const std::vector<uint32_t>& moved) {
    if (before.vertices.size() != after.vertices.size()) return false;
    for (size_t i = 0; i < before.vertices.size(); ++i) {
        const uint32_t id = idOf(before.vertices[i].id);
        if (std::find(moved.begin(), moved.end(), id) != moved.end()) continue;
        if (!exactly(before.vertices[i].position, after.vertices[i].position)) return false;
    }
    return true;
}

// The box's own eight vertices, untouched by a tool that only adds.
bool firstEightHeld(const FreeformCage& box, const FreeformCage& after) {
    for (uint32_t v = 1; v <= 8; ++v) {
        if (!exactly(at(box, v), at(after, v))) return false;
    }
    return true;
}

void testTools(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    const double h = 0.5;

    // FF-08: a vertex move is exactly the translation, on that vertex alone.
    FreeformCage moved;
    r.check("FF_08_vertex_move_writes_exactly_the_translation_to_that_vertex",
            freeformTransform(box, FreeformElement::Vertex, {7}, translation(0.1, 0.2, 0.3), &moved)
                            == FreeformStatus::Ok
                    && exactly(at(moved, 7), DVec3{h + 0.1, h + 0.2, h + 0.3})
                    && othersUnchanged(box, moved, {7}) && sameTopology(moved, box));
    FreeformCage pair;
    r.check("FF_08_a_multi_selection_moves_together",
            freeformTransform(box, FreeformElement::Vertex, {1, 2}, translation(0.0, -0.25, 0.0), &pair)
                            == FreeformStatus::Ok
                    && exactly(at(pair, 1), DVec3{-h, -h - 0.25, -h})
                    && exactly(at(pair, 2), DVec3{h, -h - 0.25, -h}) && othersUnchanged(box, pair, {1, 2}));
    FreeformAffine flat;
    flat.linear[8] = 0.0;
    FreeformCage untouched = box;
    r.check("FF_08_a_bad_move_is_refused_by_name_and_writes_nothing",
            freeformTransform(box, FreeformElement::Vertex, {99}, translation(0.1, 0, 0), &untouched)
                            == FreeformStatus::UnknownVertex
                    && freeformTransform(box, FreeformElement::Vertex, {}, translation(0.1, 0, 0), &untouched)
                               == FreeformStatus::EmptySelection
                    && freeformTransform(box, FreeformElement::Vertex, {1},
                                         translation(std::nan(""), 0, 0), &untouched)
                               == FreeformStatus::NonFinite
                    && freeformTransform(box, FreeformElement::Vertex, {1}, flat, &untouched)
                               == FreeformStatus::TransformDegenerate
                    && sameFreeformCage(untouched, box));

    // FF-09: an edge rotates and a face scales about the selection's centroid.
    std::vector<FreeformVertexId> edgeVertices;
    std::vector<FreeformVertexId> faceVertices;
    freeformSelectionVertices(box, FreeformElement::Edge, {kBoxEdgeX}, &edgeVertices);
    freeformSelectionVertices(box, FreeformElement::Face, {kBoxFacePosZ}, &faceVertices);
    DVec3 faceCentre;
    DVec3 edgeCentre;
    freeformSelectionCentroid(box, FreeformElement::Face, {kBoxFacePosZ}, &faceCentre);
    freeformSelectionCentroid(box, FreeformElement::Edge, {kBoxEdgeX}, &edgeCentre);
    FreeformAffine scale;
    for (int i = 0; i < 9; ++i) scale.linear[i] = (i % 4 == 0) ? 2.0 : 0.0;
    scale.pivot = faceCentre;
    FreeformCage scaled;
    r.check("FF_09_face_scale_about_its_centroid",
            edgeVertices.size() == 2u && idOf(edgeVertices[0]) == 1u && idOf(edgeVertices[1]) == 2u
                    && faceVertices.size() == 4u && exactly(faceCentre, DVec3{0.0, 0.0, h})
                    && freeformTransform(box, FreeformElement::Face, {kBoxFacePosZ}, scale, &scaled)
                               == FreeformStatus::Ok
                    && exactly(at(scaled, 5), DVec3{-1.0, -1.0, h}) && exactly(at(scaled, 7), DVec3{1.0, 1.0, h})
                    && othersUnchanged(box, scaled, {5, 6, 7, 8}) && sameTopology(scaled, box));
    FreeformAffine turn;  // 90 degrees about +Z, exact
    const double rz[9] = {0.0, -1.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    std::copy(rz, rz + 9, turn.linear);
    turn.pivot = edgeCentre;
    FreeformCage turned;
    r.check("FF_09_edge_rotate_about_its_centroid",
            exactly(edgeCentre, DVec3{0.0, -h, -h})
                    && freeformTransform(box, FreeformElement::Edge, {kBoxEdgeX}, turn, &turned) == FreeformStatus::Ok
                    && exactly(at(turned, 1), DVec3{0.0, -1.0, -h}) && exactly(at(turned, 2), DVec3{0.0, 0.0, -h})
                    && othersUnchanged(box, turned, {1, 2}));

    // FF-10: push/pull moves the face exactly the typed distance along its
    // normal.
    FreeformCage pushed;
    FreeformCage pulled;
    DVec3 normal;
    const bool pushOk = freeformPushPull(box, {kBoxFacePosY}, 0.25, &pushed) == FreeformStatus::Ok;
    r.check("FF_10_push_pull_moves_a_face_exactly_along_its_normal",
            pushOk && at(pushed, 3).y == 0.75 && at(pushed, 4).y == 0.75 && at(pushed, 7).y == 0.75
                    && at(pushed, 8).y == 0.75 && exactly(at(pushed, 3), DVec3{h, 0.75, -h})
                    && othersUnchanged(box, pushed, {3, 4, 7, 8}) && sameTopology(pushed, box)
                    && freeformFaceNormal(pushed, *findFreeformFace(pushed, FreeformFaceId{kBoxFacePosY}), &normal)
                    && exactly(normal, DVec3{0.0, 1.0, 0.0})
                    && freeformPushPull(box, {kBoxFacePosY}, -0.25, &pulled) == FreeformStatus::Ok
                    && at(pulled, 7).y == 0.25);
    FreeformCage both;
    const double diagonal = 0.25 / std::sqrt(2.0);
    r.check("FF_10_adjacent_faces_push_their_shared_vertices_along_the_mean_normal",
            freeformPushPull(box, {kBoxFacePosY, kBoxFacePosX}, 0.25, &both) == FreeformStatus::Ok
                    && near(at(both, 7), DVec3{h + diagonal, h + diagonal, h}, 1e-15)
                    && exactly(at(both, 4), DVec3{-h, 0.75, -h}));
    r.check("FF_10_an_unusable_distance_is_refused_by_name",
            freeformPushPull(box, {kBoxFacePosY}, 0.0, nullptr) == FreeformStatus::InvalidDistance
                    && freeformPushPull(box, {kBoxFacePosY}, 1e-7, nullptr) == FreeformStatus::InvalidDistance
                    && freeformPushPull(box, {kBoxFacePosY}, std::nan(""), nullptr) == FreeformStatus::InvalidDistance
                    && freeformPushPull(box, {kBoxFacePosY}, 2e5, nullptr) == FreeformStatus::InvalidDistance
                    && freeformPushPull(box, {42}, 0.1, nullptr) == FreeformStatus::UnknownFace);

    // FF-11: extrusion keeps the region's face ids and mints new ones for the
    // walls, deterministically.
    FreeformCage extruded;
    FreeformCage again;
    const bool extrudeOk = freeformExtrudeFaces(box, {kBoxFacePosZ}, 0.5, &extruded) == FreeformStatus::Ok
                           && freeformExtrudeFaces(box, {kBoxFacePosZ}, 0.5, &again) == FreeformStatus::Ok;
    const FreeformFace* cap = extrudeOk ? findFreeformFace(extruded, FreeformFaceId{kBoxFacePosZ}) : nullptr;
    bool capOnNew = cap != nullptr;
    for (int k = 0; cap != nullptr && k < 4; ++k) {
        capOnNew = capOnNew && idOf(cap->loop[k]) >= 9u && at(extruded, idOf(cap->loop[k])).z == 1.0;
    }
    bool oldHeld = extrudeOk;
    for (uint32_t v = 1; v <= 8; ++v) oldHeld = oldHeld && exactly(at(extruded, v), at(box, v));
    r.check("FF_11_extrude_keeps_the_face_id_and_mints_ordered_new_topology",
            extrudeOk && extruded.vertices.size() == 12u && extruded.edges.size() == 20u
                    && extruded.faces.size() == 10u && capOnNew && oldHeld
                    && extruded.nextVertexId == 13u && extruded.nextEdgeId == 21u && extruded.nextFaceId == 11u
                    && idOf(extruded.faces.back().id) == 10u && !freeformCageHasBoundary(extruded)
                    && validateFreeformCage(extruded) == FreeformStatus::Ok && sameFreeformCage(extruded, again));
    FreeformCage region;
    r.check("FF_11_a_two_face_region_extrudes_as_one_with_one_wall_per_boundary_edge",
            freeformExtrudeFaces(box, {kBoxFacePosZ, kBoxFacePosX}, 0.25, &region) == FreeformStatus::Ok
                    && region.vertices.size() == 14u && region.edges.size() == 24u
                    && region.faces.size() == 12u && validateFreeformCage(region) == FreeformStatus::Ok);
    r.check("FF_11_a_region_pinched_at_a_vertex_is_refused_by_name",
            freeformExtrudeFaces(makeFreeformPlane(), {1, 6}, 0.25, nullptr)
                            == FreeformStatus::ExtrudeRegionPinched
                    && freeformExtrudeFaces(box, {77}, 0.25, nullptr) == FreeformStatus::UnknownFace);

    // FF-12: an edge loop across the whole quad ring, at the typed ratio from
    // each ring edge's own start, with no T-junction.
    std::vector<uint32_t> ring;
    bool closed = false;
    FreeformCage looped;
    const bool ringOk = freeformEdgeRing(box, kBoxEdgeX, &ring, &closed) == FreeformStatus::Ok;
    const bool loopOk = freeformInsertEdgeLoop(box, kBoxEdgeX, 0.25, &looped) == FreeformStatus::Ok;
    bool onPlane = loopOk;
    for (uint32_t v = 9; loopOk && v <= 12; ++v) onPlane = onPlane && at(looped, v).x == -0.25;
    r.check("FF_12_insert_loop_splits_the_whole_ring_at_the_ratio",
            ringOk && closed && ring.size() == 4u && loopOk && looped.vertices.size() == 12u
                    && looped.edges.size() == 20u && looped.faces.size() == 10u && onPlane
                    && !freeformCageHasBoundary(looped) && validateFreeformCage(looped) == FreeformStatus::Ok
                    && firstEightHeld(box, looped));
    FreeformCage openLoop;
    std::vector<uint32_t> openRing;
    bool openClosed = true;
    r.check("FF_12_an_open_ring_runs_border_to_border",
            freeformEdgeRing(makeFreeformPlane(), 1, &openRing, &openClosed) == FreeformStatus::Ok && !openClosed
                    && openRing.size() == 5u
                    && freeformInsertEdgeLoop(makeFreeformPlane(), 1, 0.5, &openLoop) == FreeformStatus::Ok
                    && openLoop.vertices.size() == 30u && openLoop.faces.size() == 20u
                    && validateFreeformCage(openLoop) == FreeformStatus::Ok);

    // FF-13: refusals. A ring that re-enters a face it crossed has no single
    // loop; a ratio outside (0, 1) is not a position on the edge.
    const FreeformCage twisted = twistedRingCage();
    FreeformCage sentinel = box;
    r.check("FF_13_a_self_crossing_ring_is_refused_and_writes_nothing",
            validateFreeformCage(twisted) == FreeformStatus::Ok && twisted.faces.size() == 48u
                    && freeformEdgeRing(twisted, 1, nullptr, nullptr) == FreeformStatus::EdgeLoopSelfCrossing
                    && freeformInsertEdgeLoop(twisted, 1, 0.5, &sentinel) == FreeformStatus::EdgeLoopSelfCrossing
                    && sameFreeformCage(sentinel, box));
    r.check("FF_13_a_ratio_outside_the_edge_is_refused_by_name",
            freeformInsertEdgeLoop(box, kBoxEdgeX, 0.0, nullptr) == FreeformStatus::InvalidRatio
                    && freeformInsertEdgeLoop(box, kBoxEdgeX, 1.0, nullptr) == FreeformStatus::InvalidRatio
                    && freeformInsertEdgeLoop(box, kBoxEdgeX, -0.5, nullptr) == FreeformStatus::InvalidRatio
                    && freeformInsertEdgeLoop(box, kBoxEdgeX, std::nan(""), nullptr) == FreeformStatus::InvalidRatio
                    && freeformInsertEdgeLoop(box, 999, 0.5, nullptr) == FreeformStatus::UnknownEdge);
    FreeformCage boxX;
    freeformSetSymmetry(box, kFreeformSymmetryX, &boxX);
    r.check("FF_13_a_ring_that_mirrors_onto_itself_needs_the_midpoint",
            freeformInsertEdgeLoop(boxX, kBoxEdgeX, 0.25, nullptr) == FreeformStatus::SymmetryRequiresMidpoint);
}

// ---------------------------------------------------------------------------
// FF-14, FF-15: symmetry
// ---------------------------------------------------------------------------

bool mirroredX(const DVec3& a, const DVec3& b) { return a.x == -b.x && sameBits(a.y, b.y) && sameBits(a.z, b.z); }

void testSymmetry(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    const double h = 0.5;
    FreeformCage boxX;
    FreeformCage movedX;
    const bool on = freeformSetSymmetry(box, kFreeformSymmetryX, &boxX) == FreeformStatus::Ok;
    r.check("FF_14_symmetry_x_moves_the_mirror_partner_exactly",
            on && boxX.symmetry == kFreeformSymmetryX
                    && freeformTransform(boxX, FreeformElement::Vertex, {2}, translation(0.1, 0.05, 0.0), &movedX)
                               == FreeformStatus::Ok
                    && exactly(at(movedX, 2), DVec3{h + 0.1, -h + 0.05, -h})
                    && mirroredX(at(movedX, 1), at(movedX, 2)) && othersUnchanged(box, movedX, {1, 2})
                    && validateFreeformCage(movedX) == FreeformStatus::Ok);
    const std::vector<uint32_t> partners = freeformSymmetricSelection(boxX, FreeformElement::Vertex, {2});
    r.check("FF_14_the_symmetric_selection_names_both_sides",
            partners == std::vector<uint32_t>{1, 2}
                    && freeformSymmetricSelection(boxX, FreeformElement::Face, {kBoxFacePosX})
                               == std::vector<uint32_t>{5, 6});

    // A midpoint loop lands ON the plane, exactly, with no duplicate; a vertex
    // there stays there whatever the move asks.
    FreeformCage mid;
    FreeformCage slid;
    const bool midOk = freeformInsertEdgeLoop(boxX, kBoxEdgeX, 0.5, &mid) == FreeformStatus::Ok;
    bool centred = midOk && mid.vertices.size() == 12u;
    for (uint32_t v = 9; midOk && v <= 12; ++v) centred = centred && sameBits(at(mid, v).x, 0.0);
    r.check("FF_14_a_vertex_on_the_plane_stays_on_it_and_is_never_duplicated",
            centred
                    && freeformTransform(mid, FreeformElement::Vertex, {9}, translation(0.2, 0.1, 0.0), &slid)
                               == FreeformStatus::Ok
                    && sameBits(at(slid, 9).x, 0.0) && at(slid, 9).y == at(mid, 9).y + 0.1
                    && slid.vertices.size() == 12u && othersUnchanged(mid, slid, {9}));
    // A loop on a ring with its own mirror ring inserts both, exactly mirrored.
    FreeformCage twoRings;
    const bool twoOk = freeformInsertEdgeLoop(mid, kBoxEdgeX, 0.25, &twoRings) == FreeformStatus::Ok;
    bool mirroredRings = twoOk && twoRings.vertices.size() == 20u && twoRings.faces.size() == 18u;
    for (uint32_t v = 13; twoOk && v <= 20; ++v) {
        mirroredRings = mirroredRings && std::fabs(std::fabs(at(twoRings, v).x) - 0.375) == 0.0;
    }
    r.check("FF_14_a_loop_whose_mirror_is_another_ring_inserts_both",
            mirroredRings && validateFreeformCage(twoRings) == FreeformStatus::Ok);
    FreeformCage lopsided;
    FreeformCage extrudedX;
    freeformTransform(box, FreeformElement::Vertex, {2}, translation(0.1, 0.0, 0.0), &lopsided);
    r.check("FF_14_symmetry_on_an_asymmetric_cage_is_refused_and_tools_mirror_regions",
            freeformSetSymmetry(lopsided, kFreeformSymmetryX, nullptr) == FreeformStatus::CageNotSymmetric
                    && freeformExtrudeFaces(boxX, {kBoxFacePosX}, 0.25, &extrudedX) == FreeformStatus::Ok
                    && extrudedX.faces.size() == 14u && validateFreeformCage(extrudedX) == FreeformStatus::Ok);

    // FF-15: all three planes at once.
    FreeformCage boxXYZ;
    FreeformCage grown;
    bool corners = freeformSetSymmetry(box, kFreeformSymmetryMask, &boxXYZ) == FreeformStatus::Ok
                   && freeformTransform(boxXYZ, FreeformElement::Vertex, {7}, translation(0.1, 0.1, 0.1), &grown)
                              == FreeformStatus::Ok;
    for (uint32_t v = 1; corners && v <= 8; ++v) {
        const DVec3 p = at(grown, v);
        corners = corners && std::fabs(p.x) == h + 0.1 && std::fabs(p.y) == h + 0.1 && std::fabs(p.z) == h + 0.1;
    }
    r.check("FF_15_combined_xyz_symmetry_moves_all_eight_corners_exactly", corners);
    FreeformCage cylinder;
    FreeformCage raised;
    const bool cylOk = freeformSetSymmetry(makeFreeformCylinder(), kFreeformSymmetryMask, &cylinder)
                               == FreeformStatus::Ok
                       && freeformTransform(cylinder, FreeformElement::Vertex, {18}, translation(0.3, 0.2, 0.1), &raised)
                                  == FreeformStatus::Ok;
    r.check("FF_15_a_vertex_on_two_planes_keeps_both_and_mirrors_through_the_third",
            cylOk && sameBits(at(raised, 18).x, 0.0) && sameBits(at(raised, 18).z, 0.0)
                    && at(raised, 18).y == 0.5 + 0.2 && at(raised, 17).y == -(0.5 + 0.2)
                    && validateFreeformCage(raised) == FreeformStatus::Ok);
    FreeformCage pushedX;
    freeformPushPull(box, {kBoxFacePosX}, 0.25, &pushedX);
    r.check("FF_15_a_plane_is_allowed_only_where_the_cage_is_symmetric",
            freeformSetSymmetry(pushedX, kFreeformSymmetryY | kFreeformSymmetryZ, nullptr) == FreeformStatus::Ok
                    && freeformSetSymmetry(pushedX, kFreeformSymmetryMask, nullptr) == FreeformStatus::CageNotSymmetric
                    && freeformSetSymmetry(box, 0x08u, nullptr) == FreeformStatus::InvalidSymmetry
                    && freeformSymmetricSelection(boxXYZ, FreeformElement::Face, {kBoxFacePosZ})
                               == std::vector<uint32_t>{kBoxFaceNegZ, kBoxFacePosZ});
}

// ---------------------------------------------------------------------------
// FF-16, FF-17: deletion and topology refusals
// ---------------------------------------------------------------------------

void testTopology(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    FreeformCage open;
    FreeformMesh openMesh;
    r.check("FF_16_deleting_a_face_leaves_a_valid_open_cage",
            freeformDeleteFaces(box, {kBoxFacePosY}, &open) == FreeformStatus::Ok && open.faces.size() == 5u
                    && open.edges.size() == 12u && open.vertices.size() == 8u && freeformCageHasBoundary(open)
                    && boundaryEdgeCount(open) == 4 && open.nextFaceId == 7u
                    && validateFreeformCage(open) == FreeformStatus::Ok
                    && subdivideFreeformCage(open, &openMesh) == FreeformStatus::Ok
                    && openMesh.render.renderBothSides);
    FreeformCage holed;
    r.check("FF_16_a_hole_in_a_sheet_is_a_second_border",
            freeformDeleteFaces(makeFreeformPlane(), {6, 7, 10, 11}, &holed) == FreeformStatus::Ok
                    && holed.faces.size() == 12u && boundaryEdgeCount(holed) == 24
                    && holed.vertices.size() == 24u && validateFreeformCage(holed) == FreeformStatus::Ok);
    std::vector<uint32_t> everything;
    for (uint32_t f = 1; f <= 16; ++f) everything.push_back(f);
    r.check("FF_16_deleting_every_face_is_refused",
            freeformDeleteFaces(makeFreeformPlane(), everything, nullptr) == FreeformStatus::DeleteWouldEmpty
                    && freeformDeleteFaces(box, {}, nullptr) == FreeformStatus::EmptySelection
                    && freeformDeleteFaces(box, {9}, nullptr) == FreeformStatus::UnknownFace);

    // FF-17: what R1 cannot carry is refused by name, from a tool and from a
    // hand-built cage alike.
    FreeformCage sentinel = box;
    r.check("FF_17_a_delete_that_leaves_a_bow_tie_is_refused",
            freeformDeleteFaces(makeFreeformPlane(), {2, 5}, &sentinel) == FreeformStatus::DeleteWouldBreakManifold
                    && sameFreeformCage(sentinel, box));
    const std::vector<DVec3> fin = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0},
                                    {0, 0, 1}, {1, 0, 1}, {0, -1, 0}, {1, -1, 0}};
    const FreeformCage threeFins = assembleFreeformCage(fin, {{0, 1, 3, 2}, {1, 0, 4, 5}, {0, 1, 7, 6}}, 0);
    const std::vector<DVec3> bow = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                    {-1, 0, 0}, {-1, -1, 0}, {0, -1, 0}};
    const FreeformCage bowTie = assembleFreeformCage(bow, {{0, 1, 2, 3}, {0, 4, 5, 6}}, 0);
    FreeformCage duplicateEdge = box;
    duplicateEdge.edges.push_back(FreeformEdge{FreeformEdgeId{13}, FreeformVertexId{1}, FreeformVertexId{2}, 0.0});
    duplicateEdge.nextEdgeId = 14;
    FreeformCage isolated = box;
    isolated.vertices.push_back(FreeformVertex{FreeformVertexId{9}, DVec3{2.0, 2.0, 2.0}});
    isolated.nextVertexId = 10;
    FreeformCage rotated = box;
    std::rotate(rotated.faces[1].loop.begin(), rotated.faces[1].loop.begin() + 1, rotated.faces[1].loop.end());
    FreeformCage collapsed = box;
    collapsed.vertices[1].position = collapsed.vertices[0].position;
    FreeformCage lowMark = box;
    lowMark.nextVertexId = 8;
    r.check("FF_17_hand_built_non_manifold_and_broken_cages_are_refused_by_name",
            validateFreeformCage(threeFins) == FreeformStatus::NonManifoldEdge
                    && validateFreeformCage(badTopologyCage()) == FreeformStatus::InconsistentWinding
                    && validateFreeformCage(bowTie) == FreeformStatus::BowTieVertex
                    && validateFreeformCage(duplicateEdge) == FreeformStatus::DuplicateEdge
                    && validateFreeformCage(isolated) == FreeformStatus::IsolatedVertex
                    && validateFreeformCage(rotated) == FreeformStatus::FaceNotCanonical
                    && validateFreeformCage(collapsed) == FreeformStatus::FaceDegenerate
                    && validateFreeformCage(lowMark) == FreeformStatus::HighWaterInvalid
                    && validateFreeformCage(FreeformCage{}) == FreeformStatus::EmptyCage);
    ConstructionScene scene((NoProjectTag()));
    FreeformStatus why = FreeformStatus::Ok;
    r.check("FF_17_the_scene_refuses_an_invalid_cage_before_minting_an_id",
            scene.addFreeformBody(threeFins, &why) == nullptr && why == FreeformStatus::NonManifoldEdge
                    && scene.bodyCount() == 0u);
}


// ---------------------------------------------------------------------------
// The edit session: semantic picking, the gizmo on the cage, typed tools
// ---------------------------------------------------------------------------

CameraSnapshot lookAt(const Vec3& eye, const Vec3& target, int width, int height) {
    CameraSnapshot camera{};
    camera.view = mat4LookAt(eye, target, Vec3{0.0f, 1.0f, 0.0f});
    camera.proj = mat4Perspective(1.0471975512f, static_cast<float>(width) / height, 0.05f, 200.0f);
    camera.eye = eye;
    camera.target = target;
    camera.projection = ProjectionMode::Perspective;
    camera.orthoHalfHeightMeters = 1.0f;
    return camera;
}

bool screenOf(const CameraSnapshot& camera, const Vec3& world, int w, int h, float* x, float* y) {
    return projectWorldToScreen(camera, world, w, h, x, y);
}

FreeformTouchResult touch(FreeformEditSession& session, TouchAction action, float x, float y,
                          const CameraSnapshot& camera, int w, int h, int32_t id = 7) {
    TouchPointer p{};
    p.id = id;
    p.x = x;
    p.y = y;
    return session.onTouch(action, id, &p, 1, camera, w, h);
}

void testSession(Checks& r) {
    const int w = 1080;
    const int h = 2000;
    const CameraSnapshot camera = lookAt(Vec3{3.0f, 2.5f, 4.0f}, Vec3{0.0f, 0.0f, 0.0f}, w, h);
    Rig rig(makeFreeformBox());
    FreeformEditSession session(rig.scene, rig.history);
    const bool opened = session.begin(rig.id) == FreeformStatus::Ok && session.active()
                        && session.element() == FreeformElement::Face && session.selection().empty();

    // Picking resolves to ids, through the projected cage and the surface's
    // per-triangle control face.
    float x = 0.0f, y = 0.0f;
    bool picked = opened && session.setElement(FreeformElement::Vertex) == FreeformStatus::Ok
                  && screenOf(camera, Vec3{0.5f, 0.5f, 0.5f}, w, h, &x, &y);
    picked = picked && touch(session, TouchAction::Down, x + 3.0f, y - 2.0f, camera, w, h).consumed
             && touch(session, TouchAction::Up, x + 3.0f, y - 2.0f, camera, w, h).tapResolved
             && session.selection() == std::vector<uint32_t>{7};
    float ex = 0.0f, ey = 0.0f, fx = 0.0f, fy = 0.0f;
    bool edgeOk = session.setElement(FreeformElement::Edge) == FreeformStatus::Ok && session.selection().empty()
                  && screenOf(camera, Vec3{0.0f, 0.5f, 0.5f}, w, h, &ex, &ey);
    session.tap(camera, ex, ey, w, h);
    edgeOk = edgeOk && session.selection() == std::vector<uint32_t>{7};
    bool faceOk = session.setElement(FreeformElement::Face) == FreeformStatus::Ok
                  && screenOf(camera, Vec3{0.1f, 0.05f, 0.5f}, w, h, &fx, &fy);
    session.tap(camera, fx, fy, w, h);
    faceOk = faceOk && session.selection() == std::vector<uint32_t>{kBoxFacePosZ};
    session.tap(camera, 5.0f, 5.0f, w, h);
    const bool missClears = session.selection().empty();
    session.setElement(FreeformElement::Vertex);
    session.setMultiSelect(true);
    float x8 = 0.0f, y8 = 0.0f;
    screenOf(camera, Vec3{-0.5f, 0.5f, 0.5f}, w, h, &x8, &y8);
    session.tap(camera, x, y, w, h);
    session.tap(camera, x8, y8, w, h);
    const bool both = session.selection() == std::vector<uint32_t>{7, 8};
    session.tap(camera, x, y, w, h);
    const bool toggled = session.selection() == std::vector<uint32_t>{8};
    r.check("FF_SESSION_taps_pick_cage_elements_by_id_and_multi_select_toggles",
            picked && edgeOk && faceOk && missClears && both && toggled);

    // A travelling finger is navigation, never a tap.
    session.setMultiSelect(false);
    session.select({8});
    // Away from the gizmo standing on vertex 8, whose X shaft points at 7.
    const bool armed = touch(session, TouchAction::Down, 150.0f, 1850.0f, camera, w, h).consumed
                       && session.tapArmed();
    const FreeformTouchResult travelled = touch(session, TouchAction::Move, 350.0f, 1850.0f, camera, w, h);
    const FreeformTouchResult lifted = touch(session, TouchAction::Up, 350.0f, 1850.0f, camera, w, h);
    r.check("FF_SESSION_a_finger_past_the_slop_navigates_and_selects_nothing",
            armed && !travelled.consumed && !session.tapArmed() && !lifted.tapResolved
                    && session.selection() == std::vector<uint32_t>{8});

    // The gizmo on the cage: one captured drag is one Undo and moves the
    // selection along the handle.
    session.select({7});
    const GizmoSnapshot state = session.gizmoSnapshot(camera, w, h);
    Vec3 grab{};
    float gx = 0.0f, gy = 0.0f, tx = 0.0f, ty = 0.0f;
    const bool ready = state.visible && near(dvec3FromVec3(state.pivot), DVec3{0.5, 0.5, 0.5}, 1e-6)
                       && gizmoHandleGrabPoint(state, GizmoHandle::AxisX, &grab)
                       && screenOf(camera, grab, w, h, &gx, &gy)
                       && screenOf(camera, vec3Add(grab, Vec3{0.25f, 0.0f, 0.0f}), w, h, &tx, &ty);
    const size_t depth = rig.history.undoDepth();
    const FreeformTouchResult down = touch(session, TouchAction::Down, gx, gy, camera, w, h);
    bool movedAll = down.dragBegan && session.capturing();
    for (int i = 1; i <= 10; ++i) {
        const float k = i / 10.0f;
        touch(session, TouchAction::Move, gx + (tx - gx) * k, gy + (ty - gy) * k, camera, w, h);
    }
    const FreeformTouchResult up = touch(session, TouchAction::Up, tx, ty, camera, w, h);
    const DVec3 dragged = at(rig.cage(), 7);
    r.check("FF_08_a_gizmo_drag_on_a_cage_vertex_moves_it_along_the_handle_as_one_step",
            ready && movedAll && up.dragCommitted && rig.history.undoDepth() == depth + 1u
                    && std::fabs(dragged.x - 0.75) < 1e-3 && dragged.y == 0.5 && dragged.z == 0.5
                    && exactly(at(rig.cage(), 1), DVec3{-0.5, -0.5, -0.5}));
    const bool undone = rig.history.undo() && exactly(at(rig.cage(), 7), DVec3{0.5, 0.5, 0.5});
    rig.history.redo();
    // A second finger cancels: the cage goes back and nothing is recorded.
    const size_t before = rig.history.undoDepth();
    const DVec3 held = at(rig.cage(), 7);
    const GizmoSnapshot again = session.gizmoSnapshot(camera, w, h);
    gizmoHandleGrabPoint(again, GizmoHandle::AxisX, &grab);
    screenOf(camera, grab, w, h, &gx, &gy);
    touch(session, TouchAction::Down, gx, gy, camera, w, h);
    touch(session, TouchAction::Move, gx + 80.0f, gy, camera, w, h);
    TouchPointer two[2] = {};
    two[0].id = 7;
    two[0].x = gx + 80.0f;
    two[0].y = gy;
    two[1].id = 8;
    two[1].x = 100.0f;
    two[1].y = 100.0f;
    const FreeformTouchResult second = session.onTouch(TouchAction::PointerDown, 8, two, 2, camera, w, h);
    r.check("FF_08_undo_restores_the_cage_and_a_second_finger_cancels_a_drag",
            undone && second.dragCancelled && !session.capturing() && rig.history.undoDepth() == before
                    && exactly(at(rig.cage(), 7), held));

    // Typed tools, each one transaction; a locked body refuses the session.
    session.setElement(FreeformElement::Face);
    session.select({kBoxFacePosY});
    const size_t d0 = rig.history.undoDepth();
    const bool pushed = session.pushPull(0.25) == FreeformStatus::Ok && rig.history.undoDepth() == d0 + 1u;
    const bool extruded = session.extrude(0.5) == FreeformStatus::Ok && rig.cage().faces.size() == 10u
                          && session.selection() == std::vector<uint32_t>{kBoxFacePosY}
                          && rig.history.undoDepth() == d0 + 2u;
    session.setElement(FreeformElement::Edge);
    session.select({kBoxEdgeX});
    const bool looped = session.insertLoop(0.5) == FreeformStatus::Ok && rig.history.undoDepth() == d0 + 3u;
    const bool creased = session.setCrease(1.0) == FreeformStatus::Ok && rig.history.undoDepth() == d0 + 4u;
    const bool levelled = session.setLevel(3) == FreeformStatus::Ok && rig.cage().subdivisionLevel == 3u;
    const bool wrongKind = session.pushPull(0.1) == FreeformStatus::EmptySelection;
    const bool refusedRecordsNothing = session.setLevel(9) == FreeformStatus::InvalidSubdivisionLevel
                                       && rig.history.undoDepth() == d0 + 5u;
    r.check("FF_SESSION_typed_tools_are_one_undo_each_and_refusals_record_nothing",
            pushed && extruded && looped && creased && levelled && wrongKind && refusedRecordsNothing);
    rig.body()->setLocked(true);
    session.reconcile();
    const bool closed = !session.active();
    r.check("FF_SESSION_a_locked_or_hidden_body_cannot_be_edited",
            closed && session.begin(rig.id) == FreeformStatus::BodyLocked
                    && (rig.body()->setLocked(false), rig.body()->setVisible(false),
                        session.begin(rig.id) == FreeformStatus::BodyHidden));
    rig.body()->setVisible(true);

    // The overlay: every edge, crosses in Vertex mode, the selection lit.
    const FreeformCage box = makeFreeformBox();
    const SketchOverlayPtr overlay =
            buildFreeformCageOverlay(box, mat4Identity(), FreeformElement::Vertex, {7}, 0.01f, 99);
    const SketchOverlayPtr faces =
            buildFreeformCageOverlay(box, mat4Identity(), FreeformElement::Face, {kBoxFacePosZ}, 0.01f, 100);
    r.check("FF_SESSION_the_cage_overlay_draws_edges_vertices_and_the_selection",
            overlay != nullptr && overlay->revision == 99u && overlay->ranges.size() == 2u
                    && overlay->vertices.size() == 24u + 48u && overlay->ranges[1].vertexCount == 6u
                    && faces != nullptr && faces->ranges[1].vertexCount == 8u + 4u);
}

// ---------------------------------------------------------------------------
// FF-18: the Construction history owns cage edits
// ---------------------------------------------------------------------------

void testHistory(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    Rig rig(box);
    FreeformCage pushed;
    freeformPushPull(box, {kBoxFacePosY}, 0.25, &pushed);
    const bool first = rig.gesture({pushed}) == FreeformStatus::Ok && rig.history.undoDepth() == 1u;
    // One drag: five samples, each composed from the gesture's START cage, as
    // the gizmo does -- one step.
    std::vector<FreeformCage> drag;
    for (int i = 1; i <= 5; ++i) {
        FreeformCage sample;
        freeformTransform(pushed, FreeformElement::Vertex, {7}, translation(0.02 * i, 0.0, 0.0), &sample);
        drag.push_back(sample);
    }
    const bool second = rig.gesture(drag) == FreeformStatus::Ok && rig.history.undoDepth() == 2u;
    const std::shared_ptr<const FreeformCage> afterDrag = rig.freeform()->cagePointer();
    r.check("FF_18_one_gesture_is_one_construction_step",
            first && second && sameFreeformCage(rig.cage(), drag.back()));
    const bool undo1 = rig.history.undo();
    const bool atPush = sameFreeformCage(rig.cage(), pushed);
    const bool undo2 = rig.history.undo();
    const bool atBox = sameFreeformCage(rig.cage(), box);
    const bool redo1 = rig.history.redo();
    const bool redo2 = rig.history.redo();
    r.check("FF_18_undo_and_redo_restore_the_exact_cage_ids_and_marks",
            undo1 && atPush && undo2 && atBox && redo1 && redo2 && sameFreeformCage(rig.cage(), drag.back())
                    && rig.freeform()->cagePointer() == afterDrag && rig.history.redoDepth() == 0u);
    const size_t depth = rig.history.undoDepth();
    rig.gesture({drag.back()});
    r.check("FF_18_an_edit_that_changes_nothing_records_nothing", rig.history.undoDepth() == depth);
    // Delete and Duplicate are representation-neutral: Undo brings the same
    // id back with its cage; a duplicate is a fresh id carrying a copy.
    const ObjectId original = rig.id;
    const bool duplicated = duplicateSceneBody(original, rig.scene, rig.history) == BodyCommandStatus::Ok;
    const ObjectId copyId = rig.scene.activeBodyId();
    const SceneObject* copy = rig.scene.findBody(copyId);
    const bool copied = duplicated && copyId != original && copy != nullptr && copy->freeformOrNull() != nullptr
                        && sameFreeformCage(copy->freeformOrNull()->cage(), rig.cage());
    const bool deleted = deleteSceneBody(original, rig.scene, rig.history) == DeleteBodyStatus::Ok;
    const bool gone = rig.scene.findBody(original) == nullptr;
    const bool restored = rig.history.undo() && rig.scene.findBody(original) != nullptr
                          && sameFreeformCage(rig.scene.findBody(original)->freeformOrNull()->cage(), drag.back());
    r.check("FF_18_duplicate_and_delete_carry_the_cage_and_undo_restores_the_same_id",
            copied && deleted && gone && restored);
    r.check("FF_18_a_cage_edit_never_touches_sculpt_state",
            !rig.body()->frozenSculpt().mesh.frozen() && rig.body()->frozenSculpt().history.undoDepth() == 0u);
}

// ---------------------------------------------------------------------------
// FF-19, FF-20: the FRFM section
// ---------------------------------------------------------------------------

void testFormat(Checks& r) {
    const FreeformCage authored = creaseSymmetryCage();
    // A mixed project: a Construction body beside the Freeform one.
    ConstructionScene scene((NoProjectTag()));
    ConstructionHistory history(scene);
    history.beginSessionInitialization();
    publishSceneObject(scene.addBody());
    SceneObject* body = scene.addFreeformBody(authored);
    if (body != nullptr) publishSceneObject(*body);
    history.endSessionInitialization();
    const ObjectId id = body != nullptr ? body->objectId() : kNoObject;
    const ProjectDocument document = captureProjectDocument(scene, ProjectKind::Construction);
    const std::vector<uint8_t> bytes = encodeProjectV1(document);
    ProjectDocument decoded;
    const bool decodedOk = decodeProject(bytes.data(), bytes.size(), &decoded) == ProjectCodecStatus::Ok;
    ConstructionScene reopened{NoProjectTag{}};
    ConstructionHistory reopenedHistory(reopened);
    SculptSession sculpt;
    const bool loaded = decodedOk
                        && loadProjectDocument(decoded, reopened, sculpt, reopenedHistory) == ProjectCodecStatus::Ok;
    const SceneObject* back = loaded ? reopened.findBody(id) : nullptr;
    const bool sameCage = back != nullptr && back->freeformOrNull() != nullptr
                          && sameFreeformCage(back->freeformOrNull()->cage(), authored);
    std::shared_ptr<const FreeformMesh> before;
    std::shared_ptr<const FreeformMesh> after;
    const bool derivedSame = body != nullptr && sameCage
                             && body->freeformOrNull()->derived(&before) == FreeformStatus::Ok
                             && back->freeformOrNull()->derived(&after) == FreeformStatus::Ok
                             && freeformMeshDigest(*before) == freeformMeshDigest(*after);
    r.check("FF_19_save_and_reopen_restore_the_exact_cage_ids_marks_and_surface",
            decodedOk && sameProjectDocument(decoded, document) && loaded && sameCage && derivedSame
                    && reopened.bodyCount() == 2u && reopenedHistory.undoDepth() == 0u);
    r.check("FF_19_reopened_bytes_and_fingerprint_are_identical",
            loaded && encodeProjectV1(captureProjectDocument(reopened, ProjectKind::Construction)) == bytes
                    && projectSemanticFingerprint(reopened, ProjectKind::Construction)
                               == projectSemanticFingerprint(scene, ProjectKind::Construction));
    const size_t frfm = sectionOffset(bytes, kSectionTagFreeform);
    r.check("FF_19_the_file_announces_freeform_in_a_required_frfm_v1_section",
            bytes.size() > 16u && (bytes[15] & kHeaderFlagHasFreeform) != 0u && frfm != 0u
                    && bytes[frfm + 4u] == 1u && bytes[frfm + 5u] == 0u && (bytes[frfm + 6u] & 1u) != 0u);

    // FF-20: the payload is the cage and only the cage.
    const uint64_t expected = 4u + 36u + 28u * authored.vertices.size() + 20u * authored.edges.size()
                              + 20u * authored.faces.size();
    FreeformCage levelled = authored;
    levelled.subdivisionLevel = 1;
    const std::vector<uint8_t> lower = encodeProjectV1(documentFor(levelled));
    const std::vector<uint8_t> higher = encodeProjectV1(documentFor(authored));
    size_t differing = 0;
    for (size_t i = 0; i < lower.size() && lower.size() == higher.size(); ++i) differing += lower[i] != higher[i];
    r.check("FF_20_the_payload_is_the_cage_alone_and_a_level_change_is_one_byte",
            frfm != 0u && sectionLength(bytes, frfm) == expected && lower.size() == higher.size()
                    && differing >= 1u && differing <= 5u);
    // A project without Freeform writes neither the flag nor the section.
    ConstructionScene legacy((NoProjectTag()));
    publishSceneObject(legacy.addBody());
    const std::vector<uint8_t> legacyBytes =
            encodeProjectV1(captureProjectDocument(legacy, ProjectKind::Construction));
    r.check("FF_20_a_project_without_freeform_writes_no_flag_and_no_section",
            legacyBytes.size() > 16u && (legacyBytes[15] & kHeaderFlagHasFreeform) == 0u
                    && sectionOffset(legacyBytes, kSectionTagFreeform) == 0u);

    // Bounds and refusals, each with the bad value in place and a valid CRC.
    const std::vector<uint8_t> boxBytes = encodeProjectV1(documentFor(makeFreeformBox()));
    const size_t at = sectionOffset(boxBytes, kSectionTagFreeform) + kForgeSectionHeaderBytes;
    // Payload offsets: bodyCount 0, objectId 4, level 12, symmetry 13,
    // reserved 14, marks 16..27, counts 28 (vertices) 32 (edges) 36 (faces),
    // then records from 40.
    auto patched = [&](size_t offset, uint32_t value, int width) {
        std::vector<uint8_t> copy = boxBytes;
        if (width == 4) putU32(&copy, at + offset, value);
        else if (width == 2) putU16(&copy, at + offset, static_cast<uint16_t>(value));
        else copy[at + offset] = static_cast<uint8_t>(value);
        resealFreeform(&copy);
        return decodeStatus(copy);
    };
    r.check("FF_20_counts_are_bounded_before_anything_is_allocated",
            patched(0, 0, 4) == ProjectCodecStatus::ImpossibleCount
                    && patched(28, 0, 4) == ProjectCodecStatus::ImpossibleCount
                    && patched(28, kMaxFreeformVertices + 1u, 4) == ProjectCodecStatus::ImpossibleCount
                    && patched(32, kMaxFreeformEdges + 1u, 4) == ProjectCodecStatus::ImpossibleCount
                    && patched(36, kMaxFreeformFaces, 4) == ProjectCodecStatus::Truncated
                    && patched(28, 9, 4) == ProjectCodecStatus::Truncated
                    && patched(0, 0xFFFFFFFFu, 4) == ProjectCodecStatus::ImpossibleCount);
    // The first face record's second vertex: records start at 40, vertices are
    // 28 bytes and edges 20, and a face is its id then four vertex ids.
    const size_t firstFace = 40u + 8u * 28u + 12u * 20u;
    r.check("FF_20_a_malformed_cage_or_reserved_field_is_refused_by_name",
            patched(14, 1, 2) == ProjectCodecStatus::BadPayload
                    && patched(firstFace + 8u, 42, 4) == ProjectCodecStatus::InvalidSemanticValue
                    && patched(12, 9, 1) == ProjectCodecStatus::InvalidSemanticValue
                    && patched(13, 0x08, 1) == ProjectCodecStatus::InvalidSemanticValue
                    && patched(4, 77, 4) == ProjectCodecStatus::UnresolvedReference
                    && decodeStatus(boxBytes) == ProjectCodecStatus::Ok);
    std::vector<uint8_t> unflagged = boxBytes;
    unflagged[15] = static_cast<uint8_t>(unflagged[15] & ~kHeaderFlagHasFreeform);
    std::vector<uint8_t> future = boxBytes;
    putU16(&future, sectionOffset(future, kSectionTagFreeform) + 4u, 2u);
    r.check("FF_20_a_section_the_header_does_not_announce_or_cannot_read_is_refused",
            decodeStatus(unflagged) != ProjectCodecStatus::Ok
                    && decodeStatus(future) == ProjectCodecStatus::UnsupportedSectionVersion);

    // The three corpus documents, as this build encodes them.
    ProjectDocument bad = documentFor(makeFreeformBox());
    if (!bad.freeform.bodies.empty()) bad.freeform.bodies[0].cage = badTopologyCage();
    const std::vector<uint8_t> badBytes = encodeProjectV1Unchecked(bad);
    const std::vector<uint8_t> creaseBytes = encodeProjectV1(documentFor(authored));
    r.check("FF_20_the_corpus_documents_encode_and_decode_to_their_verdicts",
            decodeStatus(boxBytes) == ProjectCodecStatus::Ok && decodeStatus(creaseBytes) == ProjectCodecStatus::Ok
                    && decodeStatus(badBytes) == ProjectCodecStatus::InvalidSemanticValue);
    // The digests scripts/build-forge-corpus.ps1 writes from DATA_PACKAGE_SPEC.md
    // §7j and testdata/forge/v1/ commits: the independent encoder and this one
    // must agree byte for byte.
    r.check("FF_20_the_frfm_corpus_matches_the_independent_builder",
            projectFixtureSha256Hex(boxBytes)
                            == "2f38b285c5718a5f53cbac8e9dcac9749ae474a3ae2423dd4e7dca97157faf0e"
                    && projectFixtureSha256Hex(creaseBytes)
                               == "b218fda7d3eaea4170827a8fece576e032969681c710b1cf9d17521f199e2029"
                    && projectFixtureSha256Hex(badBytes)
                               == "e896be40f36453d588ea22d4a0dacc6289222484d5d95db377b521d27f86c562");
    g_fixtureDigests = "freeform_box_v1=" + projectFixtureSha256Hex(boxBytes)
                       + " freeform_crease_symmetry_v1=" + projectFixtureSha256Hex(creaseBytes)
                       + " freeform_bad_topology_v1=" + projectFixtureSha256Hex(badBytes);
}

// ---------------------------------------------------------------------------
// FF-21, FF-22
// ---------------------------------------------------------------------------

void testDigestAndSculpt(Checks& r) {
    const FreeformCage box = makeFreeformBox();
    uint64_t digests[3] = {0, 0, 0};
    for (uint64_t& digest : digests) {
        FreeformMesh mesh;
        subdivideFreeformCageAt(box, 3, &mesh);
        digest = freeformMeshDigest(mesh);
    }
    FreeformCage explicitSmooth;
    freeformSetCrease(box, {1, 2, 3}, 0.0, &explicitSmooth);
    FreeformMesh smooth;
    subdivideFreeformCageAt(explicitSmooth, 3, &smooth);
    FreeformCage nudged = box;
    nudged.vertices[6].position.x = std::nextafter(nudged.vertices[6].position.x, 1.0);
    FreeformMesh nudgedMesh;
    subdivideFreeformCageAt(nudged, 3, &nudgedMesh);
    r.check("FF_21_the_mesh_digest_is_deterministic_and_sees_one_ulp",
            digests[0] == digests[1] && digests[1] == digests[2] && freeformMeshDigest(smooth) == digests[0]
                    && freeformMeshDigest(nudgedMesh) != digests[0]);
    FreeformBody body(ObjectId{1}, std::make_shared<const FreeformCage>(box));
    std::shared_ptr<const FreeformMesh> first;
    std::shared_ptr<const FreeformMesh> second;
    std::shared_ptr<const FreeformMesh> third;
    bool changed = false;
    const bool cached = body.derived(&first) == FreeformStatus::Ok && body.derived(&second) == FreeformStatus::Ok
                        && first == second;
    const bool replaced = body.applyCage(std::make_shared<const FreeformCage>(nudged), &changed)
                                  == FreeformStatus::Ok
                          && changed && body.derived(&third) == FreeformStatus::Ok && third != first;
    FreeformCage lower = box;
    lower.nextFaceId = 3;
    r.check("FF_21_the_body_caches_its_surface_by_cage_and_refuses_lower_marks",
            cached && replaced
                    && body.applyCage(std::make_shared<const FreeformCage>(lower)) == FreeformStatus::HighWaterInvalid);

    Rig rig(box);
    ConstructionMesh source;
    const SceneObject* object = rig.body();
    r.check("FF_22_freeform_is_its_own_representation_and_never_a_sculpt_source",
            object != nullptr && object->isFreeform()
                    && object->representation() == BodyRepresentation::Freeform
                    && std::strcmp(bodyRepresentationName(object->representation()), "Freeform") == 0
                    && object->constructionOrNull() == nullptr && object->cadOrNull() == nullptr
                    && object->importedOrNull() == nullptr && !buildSculptSourceMesh(*object, &source));
    // A file claiming a sculpt mesh for a Freeform body is refused.
    ProjectDocument document = captureProjectDocument(rig.scene, ProjectKind::Construction);
    document.hasSculpt = true;
    ProjectSculptBody sculpted;
    sculpted.objectId = rig.id;
    sculpted.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    sculpted.indices = {0, 1, 2};
    document.sculpt.bodies.push_back(sculpted);
    r.check("FF_22_a_scul_entry_over_a_freeform_body_is_refused",
            validateProjectDocument(document) == ProjectCodecStatus::UnresolvedReference);
}

// ---------------------------------------------------------------------------
// Performance (bounded; measured once, never per frame)
// ---------------------------------------------------------------------------

double medianMicros(int runs, const std::function<void()>& work) {
    std::vector<double> samples;
    for (int i = 0; i < runs; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        work();
        samples.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2u];
}

FreeformCage cageFromMesh(const FreeformMesh& mesh) {
    std::vector<std::array<uint32_t, 4>> quads;
    for (size_t q = 0; q < mesh.quads.size() / 4u; ++q) {
        quads.push_back({mesh.quads[q * 4u], mesh.quads[q * 4u + 1u], mesh.quads[q * 4u + 2u], mesh.quads[q * 4u + 3u]});
    }
    return assembleFreeformCage(mesh.positions, quads, 2);
}

void measure(Checks& r) {
    FreeformMesh boxLevel2;
    subdivideFreeformCageAt(makeFreeformBox(), 2, &boxLevel2);
    const FreeformCage closed96 = cageFromMesh(boxLevel2);  // 96 faces
    const FreeformCage open500 = gridCage(20, 25, 2);        // 500 faces
    char line[512];
    std::string report;
    bool bounded = validateFreeformCage(closed96) == FreeformStatus::Ok && closed96.faces.size() == 96u
                   && validateFreeformCage(open500) == FreeformStatus::Ok && open500.faces.size() == 500u;
    const struct {
        const char* name;
        const FreeformCage* cage;
    } cases[] = {{"closed96", &closed96}, {"open500", &open500}};
    for (const auto& c : cases) {
        for (uint32_t level = 1; level <= 4; ++level) {
            FreeformMesh mesh;
            const double us = medianMicros(1, [&]() { subdivideFreeformCageAt(*c.cage, level, &mesh); });
            bounded = bounded && mesh.quads.size() / 4u == c.cage->faces.size() << (2u * level);
            std::snprintf(line, sizeof(line), "%s_l%u_us=%.0f ", c.name, level, us);
            report += line;
        }
        // One drag sample: transform from the gesture's start, validate, and
        // derive the surface at the cage's level -- what a publication costs
        // before the GPU upload.
        const uint32_t vertex = idOf(c.cage->vertices[c.cage->vertices.size() / 2u].id);
        const double us = medianMicros(1, [&]() {
            FreeformCage next;
            FreeformMesh mesh;
            freeformTransform(*c.cage, FreeformElement::Vertex, {vertex}, translation(0.01, 0.02, 0.0), &next);
            subdivideFreeformCage(next, &mesh);
        });
        std::snprintf(line, sizeof(line), "%s_drag_us=%.0f ", c.name, us);
        report += line;
    }
    if (!report.empty()) report.pop_back();
    g_performance = report;
    r.check("FF_PERF_subdivision_and_drag_timings_are_measured_on_bounded_cages", bounded);
}

}  // namespace

int runFreeformSelfTests(FreeformSelfTestResult* out, int maxOut) {
    Checks r{out, maxOut};
    testCreation(r);
    testSubdivision(r);
    testTools(r);
    testSymmetry(r);
    testTopology(r);
    testHistory(r);
    testSession(r);
    testFormat(r);
    testDigestAndSculpt(r);
    measure(r);
    return std::min(r.count, maxOut);
}

const char* freeformPerformanceReport() { return g_performance.c_str(); }

const char* freeformFixtureDigests() { return g_fixtureDigests.c_str(); }

}  // namespace forgeshape
