#include "forgeshape_freeform.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

#include "forgeshape_freeform_subdivision.h"

namespace forgeshape {

const char* freeformStatusName(FreeformStatus status) {
    switch (status) {
        case FreeformStatus::Ok: return "Ok";
        case FreeformStatus::NonFinite: return "NonFinite";
        case FreeformStatus::OutOfRange: return "OutOfRange";
        case FreeformStatus::TooManyVertices: return "TooManyVertices";
        case FreeformStatus::TooManyEdges: return "TooManyEdges";
        case FreeformStatus::TooManyFaces: return "TooManyFaces";
        case FreeformStatus::IdInvalid: return "IdInvalid";
        case FreeformStatus::HighWaterInvalid: return "HighWaterInvalid";
        case FreeformStatus::UnknownVertex: return "UnknownVertex";
        case FreeformStatus::UnknownEdge: return "UnknownEdge";
        case FreeformStatus::UnknownFace: return "UnknownFace";
        case FreeformStatus::FaceNotQuad: return "FaceNotQuad";
        case FreeformStatus::FaceNotCanonical: return "FaceNotCanonical";
        case FreeformStatus::FaceDegenerate: return "FaceDegenerate";
        case FreeformStatus::EdgeNotCanonical: return "EdgeNotCanonical";
        case FreeformStatus::DuplicateEdge: return "DuplicateEdge";
        case FreeformStatus::DuplicateFace: return "DuplicateFace";
        case FreeformStatus::MissingEdge: return "MissingEdge";
        case FreeformStatus::EdgeUnused: return "EdgeUnused";
        case FreeformStatus::NonManifoldEdge: return "NonManifoldEdge";
        case FreeformStatus::InconsistentWinding: return "InconsistentWinding";
        case FreeformStatus::IsolatedVertex: return "IsolatedVertex";
        case FreeformStatus::BowTieVertex: return "BowTieVertex";
        case FreeformStatus::InvalidCrease: return "InvalidCrease";
        case FreeformStatus::InvalidSubdivisionLevel: return "InvalidSubdivisionLevel";
        case FreeformStatus::SubdivisionBudgetExceeded: return "SubdivisionBudgetExceeded";
        case FreeformStatus::InvalidSymmetry: return "InvalidSymmetry";
        case FreeformStatus::CageNotSymmetric: return "CageNotSymmetric";
        case FreeformStatus::SymmetryBroken: return "SymmetryBroken";
        case FreeformStatus::SymmetryRequiresMidpoint: return "SymmetryRequiresMidpoint";
        case FreeformStatus::EmptySelection: return "EmptySelection";
        case FreeformStatus::EdgeLoopSelfCrossing: return "EdgeLoopSelfCrossing";
        case FreeformStatus::InvalidRatio: return "InvalidRatio";
        case FreeformStatus::ExtrudeRegionPinched: return "ExtrudeRegionPinched";
        case FreeformStatus::InvalidDistance: return "InvalidDistance";
        case FreeformStatus::DeleteWouldEmpty: return "DeleteWouldEmpty";
        case FreeformStatus::DeleteWouldBreakManifold: return "DeleteWouldBreakManifold";
        case FreeformStatus::EmptyCage: return "EmptyCage";
        case FreeformStatus::TransformDegenerate: return "TransformDegenerate";
        case FreeformStatus::NotFreeformBody: return "NotFreeformBody";
        case FreeformStatus::NotEditing: return "NotEditing";
        case FreeformStatus::EditInProgress: return "EditInProgress";
        case FreeformStatus::BodyLocked: return "BodyLocked";
        case FreeformStatus::BodyHidden: return "BodyHidden";
    }
    return "unknown";
}

int freeformStatusCode(FreeformStatus status) { return static_cast<int>(status); }

const char* freeformElementName(FreeformElement element) {
    switch (element) {
        case FreeformElement::Vertex: return "Vertex";
        case FreeformElement::Edge: return "Edge";
        case FreeformElement::Face: return "Face";
    }
    return "unknown";
}

namespace {

bool sameBits(double a, double b) {
    uint64_t x = 0;
    uint64_t y = 0;
    std::memcpy(&x, &a, sizeof(x));
    std::memcpy(&y, &b, sizeof(y));
    return x == y;
}

bool samePosition(const DVec3& a, const DVec3& b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y) && sameBits(a.z, b.z);
}

template <typename Record, typename Id>
const Record* findById(const std::vector<Record>& table, Id id) {
    const auto it = std::lower_bound(table.begin(), table.end(), id,
                                     [](const Record& r, Id want) { return idOf(r.id) < idOf(want); });
    return it != table.end() && it->id == id ? &*it : nullptr;
}

template <typename Record, typename Id>
int32_t indexById(const std::vector<Record>& table, Id id) {
    const auto it = std::lower_bound(table.begin(), table.end(), id,
                                     [](const Record& r, Id want) { return idOf(r.id) < idOf(want); });
    return it != table.end() && it->id == id ? static_cast<int32_t>(it - table.begin()) : -1;
}

uint64_t pairKey(uint32_t a, uint32_t b) {
    const uint32_t lo = std::min(a, b);
    const uint32_t hi = std::max(a, b);
    return (static_cast<uint64_t>(lo) << 32) | hi;
}

DVec3 dsub(const DVec3& a, const DVec3& b) { return dvec3Sub(a, b); }
double dlength(const DVec3& v) { return std::sqrt(dvec3Dot(v, v)); }

// The loop rotated so its smallest id comes first, winding kept.
std::array<FreeformVertexId, 4> canonicalLoop(const std::array<FreeformVertexId, 4>& loop) {
    int first = 0;
    for (int k = 1; k < 4; ++k) {
        if (idOf(loop[k]) < idOf(loop[first])) {
            first = k;
        }
    }
    std::array<FreeformVertexId, 4> out{};
    for (int k = 0; k < 4; ++k) {
        out[k] = loop[(first + k) % 4];
    }
    return out;
}

// --- symmetry ---------------------------------------------------------------

// The non-identity elements of the reflection group the flags generate, as
// masks of negated axes, ascending.
std::vector<uint8_t> symmetryElements(uint8_t flags) {
    std::vector<uint8_t> out;
    for (uint8_t m = 1; m <= kFreeformSymmetryMask; ++m) {
        if ((m & ~flags) == 0) {
            out.push_back(m);
        }
    }
    return out;
}

DVec3 reflect(const DVec3& p, uint8_t mask) {
    DVec3 q = p;
    if (mask & kFreeformSymmetryX) q.x = -q.x;
    if (mask & kFreeformSymmetryY) q.y = -q.y;
    if (mask & kFreeformSymmetryZ) q.z = -q.z;
    // -0.0 is the same coordinate as 0.0: an on-plane vertex is its own image.
    q.x += 0.0;
    q.y += 0.0;
    q.z += 0.0;
    return q;
}

double cageExtent(const FreeformCage& cage) {
    double extent = 1.0;
    for (const FreeformVertex& v : cage.vertices) {
        extent = std::max(extent, std::max(std::fabs(v.position.x),
                                           std::max(std::fabs(v.position.y), std::fabs(v.position.z))));
    }
    return extent;
}

// partners[g][v] = the vertex index nearest reflect(p_v, element g) within
// `tolerance`, or -1. Bounded O(n log n + n k) by an x-sorted sweep.
std::vector<std::vector<int32_t>> symmetryPartners(const FreeformCage& cage,
                                                   const std::vector<uint8_t>& elements,
                                                   double tolerance) {
    const size_t n = cage.vertices.size();
    std::vector<uint32_t> byX(n);
    std::iota(byX.begin(), byX.end(), 0u);
    std::sort(byX.begin(), byX.end(), [&cage](uint32_t a, uint32_t b) {
        const double xa = cage.vertices[a].position.x;
        const double xb = cage.vertices[b].position.x;
        return xa < xb || (xa == xb && a < b);
    });
    std::vector<std::vector<int32_t>> partners(elements.size(), std::vector<int32_t>(n, -1));
    for (size_t g = 0; g < elements.size(); ++g) {
        for (size_t v = 0; v < n; ++v) {
            const DVec3 q = reflect(cage.vertices[v].position, elements[g]);
            auto lo = std::lower_bound(byX.begin(), byX.end(), q.x - tolerance,
                                       [&cage](uint32_t index, double x) {
                                           return cage.vertices[index].position.x < x;
                                       });
            double best = tolerance;
            int32_t found = -1;
            for (auto it = lo; it != byX.end() && cage.vertices[*it].position.x <= q.x + tolerance;
                 ++it) {
                const DVec3& p = cage.vertices[*it].position;
                const double d = std::max(std::fabs(p.x - q.x),
                                          std::max(std::fabs(p.y - q.y), std::fabs(p.z - q.z)));
                if (d <= best && (found < 0 || d < best || *it < static_cast<uint32_t>(found))) {
                    best = d;
                    found = static_cast<int32_t>(*it);
                }
            }
            partners[g][v] = found;
        }
    }
    return partners;
}

// Whether the faces map onto faces through `partners` (as vertex sets).
bool facesMapOntoFaces(const FreeformCage& cage, const std::vector<std::vector<int32_t>>& partners) {
    std::vector<std::array<uint32_t, 4>> sets;
    sets.reserve(cage.faces.size());
    for (const FreeformFace& face : cage.faces) {
        std::array<uint32_t, 4> s{};
        for (int k = 0; k < 4; ++k) s[k] = static_cast<uint32_t>(indexById(cage.vertices, face.loop[k]));
        std::sort(s.begin(), s.end());
        sets.push_back(s);
    }
    std::vector<std::array<uint32_t, 4>> sorted = sets;
    std::sort(sorted.begin(), sorted.end());
    for (const std::vector<int32_t>& map : partners) {
        for (const std::array<uint32_t, 4>& s : sets) {
            std::array<uint32_t, 4> image{};
            for (int k = 0; k < 4; ++k) {
                if (map[s[k]] < 0) return false;
                image[k] = static_cast<uint32_t>(map[s[k]]);
            }
            std::sort(image.begin(), image.end());
            if (!std::binary_search(sorted.begin(), sorted.end(), image)) return false;
        }
    }
    return true;
}

// Re-derives exact symmetry: every vertex orbit takes its positions from ONE
// source -- the smallest-id `preferred` member when any is (the vertices a tool
// moved are the authority), else the smallest id -- zeroed on the planes it
// stands on, and every other member is its exact reflection.
FreeformStatus snapSymmetry(FreeformCage* cage, const std::vector<std::vector<int32_t>>& partners,
                            const std::vector<uint8_t>& elements,
                            const std::vector<uint8_t>* preferred) {
    const size_t n = cage->vertices.size();
    if (elements.empty()) {
        return FreeformStatus::Ok;
    }
    for (const std::vector<int32_t>& map : partners) {
        for (int32_t p : map) {
            if (p < 0) return FreeformStatus::SymmetryBroken;
        }
    }
    std::vector<uint8_t> done(n, 0);
    for (size_t v = 0; v < n; ++v) {
        if (done[v]) continue;
        // The orbit, and its source.
        std::vector<uint32_t> orbit{static_cast<uint32_t>(v)};
        for (const std::vector<int32_t>& map : partners) orbit.push_back(static_cast<uint32_t>(map[v]));
        std::sort(orbit.begin(), orbit.end());
        orbit.erase(std::unique(orbit.begin(), orbit.end()), orbit.end());
        uint32_t source = orbit.front();
        if (preferred != nullptr) {
            for (uint32_t member : orbit) {
                if ((*preferred)[member]) {
                    source = member;
                    break;
                }
            }
        }
        DVec3 base = cage->vertices[source].position;
        // On a plane exactly when the source is its own image under that one
        // reflection.
        for (size_t g = 0; g < elements.size(); ++g) {
            const uint8_t m = elements[g];
            if ((m == kFreeformSymmetryX || m == kFreeformSymmetryY || m == kFreeformSymmetryZ)
                && partners[g][source] == static_cast<int32_t>(source)) {
                if (m == kFreeformSymmetryX) base.x = 0.0;
                if (m == kFreeformSymmetryY) base.y = 0.0;
                if (m == kFreeformSymmetryZ) base.z = 0.0;
            }
        }
        cage->vertices[source].position = base;
        done[source] = 1;
        for (size_t g = 0; g < elements.size(); ++g) {
            const uint32_t member = static_cast<uint32_t>(partners[g][source]);
            if (member == source) continue;
            cage->vertices[member].position = reflect(base, elements[g]);
            done[member] = 1;
        }
        for (uint32_t member : orbit) done[member] = 1;
    }
    return FreeformStatus::Ok;
}

double symmetryTolerance(const FreeformCage& cage) { return 1.0e-9 * cageExtent(cage); }

// After a tool changed topology: pair by position within the tolerance on the
// RESULT, then snap exactly. No partner, or faces that do not map, is a broken
// symmetry, refused by name.
FreeformStatus resnapSymmetry(FreeformCage* cage) {
    const std::vector<uint8_t> elements = symmetryElements(cage->symmetry);
    if (elements.empty()) return FreeformStatus::Ok;
    const auto partners = symmetryPartners(*cage, elements, symmetryTolerance(*cage));
    const FreeformStatus why = snapSymmetry(cage, partners, elements, nullptr);
    if (why != FreeformStatus::Ok) return why;
    return facesMapOntoFaces(*cage, partners) ? FreeformStatus::Ok : FreeformStatus::SymmetryBroken;
}

FreeformStatus validateSymmetryExact(const FreeformCage& cage) {
    const std::vector<uint8_t> elements = symmetryElements(cage.symmetry);
    if (elements.empty()) return FreeformStatus::Ok;
    const auto partners = symmetryPartners(cage, elements, 0.0);
    for (size_t g = 0; g < elements.size(); ++g) {
        for (size_t v = 0; v < cage.vertices.size(); ++v) {
            const int32_t p = partners[g][v];
            if (p < 0) return FreeformStatus::SymmetryBroken;
            const DVec3 image = reflect(cage.vertices[v].position, elements[g]);
            const DVec3 there = cage.vertices[static_cast<size_t>(p)].position;
            if (!(image.x == there.x && image.y == there.y && image.z == there.z)) {
                return FreeformStatus::SymmetryBroken;
            }
        }
    }
    return facesMapOntoFaces(cage, partners) ? FreeformStatus::Ok : FreeformStatus::SymmetryBroken;
}

}  // namespace

bool sameFreeformCage(const FreeformCage& a, const FreeformCage& b) {
    if (a.vertices.size() != b.vertices.size() || a.edges.size() != b.edges.size()
        || a.faces.size() != b.faces.size() || a.nextVertexId != b.nextVertexId
        || a.nextEdgeId != b.nextEdgeId || a.nextFaceId != b.nextFaceId
        || a.subdivisionLevel != b.subdivisionLevel || a.symmetry != b.symmetry) {
        return false;
    }
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        if (a.vertices[i].id != b.vertices[i].id
            || !samePosition(a.vertices[i].position, b.vertices[i].position)) {
            return false;
        }
    }
    for (size_t i = 0; i < a.edges.size(); ++i) {
        const FreeformEdge& x = a.edges[i];
        const FreeformEdge& y = b.edges[i];
        if (x.id != y.id || x.v0 != y.v0 || x.v1 != y.v1 || !sameBits(x.crease, y.crease)) {
            return false;
        }
    }
    for (size_t i = 0; i < a.faces.size(); ++i) {
        if (a.faces[i].id != b.faces[i].id || a.faces[i].loop != b.faces[i].loop) {
            return false;
        }
    }
    return true;
}

const FreeformVertex* findFreeformVertex(const FreeformCage& cage, FreeformVertexId id) {
    return findById(cage.vertices, id);
}

const FreeformEdge* findFreeformEdge(const FreeformCage& cage, FreeformEdgeId id) {
    return findById(cage.edges, id);
}

const FreeformFace* findFreeformFace(const FreeformCage& cage, FreeformFaceId id) {
    return findById(cage.faces, id);
}

const FreeformEdge* findFreeformEdgeBetween(const FreeformCage& cage, FreeformVertexId a,
                                            FreeformVertexId b) {
    const FreeformVertexId lo = idOf(a) < idOf(b) ? a : b;
    const FreeformVertexId hi = idOf(a) < idOf(b) ? b : a;
    for (const FreeformEdge& edge : cage.edges) {
        if (edge.v0 == lo && edge.v1 == hi) return &edge;
    }
    return nullptr;
}

bool freeformCageHasBoundary(const FreeformCage& cage) {
    FreeformTopology topology;
    if (buildFreeformTopology(cage, &topology) != FreeformStatus::Ok) return false;
    for (uint8_t count : topology.edgeFaceCount) {
        if (count == 1u) return true;
    }
    return false;
}

FreeformStatus buildFreeformTopology(const FreeformCage& cage, FreeformTopology* out) {
    FreeformTopology t;
    const size_t nv = cage.vertices.size();
    const size_t ne = cage.edges.size();
    const size_t nf = cage.faces.size();
    std::vector<std::pair<uint64_t, uint32_t>> keys;
    keys.reserve(ne);
    for (size_t e = 0; e < ne; ++e) {
        const int32_t a = indexById(cage.vertices, cage.edges[e].v0);
        const int32_t b = indexById(cage.vertices, cage.edges[e].v1);
        if (a < 0 || b < 0) return FreeformStatus::UnknownVertex;
        keys.emplace_back(pairKey(static_cast<uint32_t>(a), static_cast<uint32_t>(b)),
                          static_cast<uint32_t>(e));
    }
    std::sort(keys.begin(), keys.end());
    for (size_t i = 1; i < keys.size(); ++i) {
        if (keys[i].first == keys[i - 1].first) return FreeformStatus::DuplicateEdge;
    }
    t.faceVertices.resize(nf * 4u);
    t.faceEdges.resize(nf * 4u);
    t.edgeFaces.assign(ne * 2u, -1);
    t.edgeFaceCount.assign(ne, 0u);
    for (size_t f = 0; f < nf; ++f) {
        const FreeformFace& face = cage.faces[f];
        for (int k = 0; k < 4; ++k) {
            const int32_t v = indexById(cage.vertices, face.loop[k]);
            if (v < 0) return FreeformStatus::UnknownVertex;
            t.faceVertices[f * 4u + k] = static_cast<uint32_t>(v);
        }
        for (int a = 0; a < 4; ++a) {
            for (int b = a + 1; b < 4; ++b) {
                if (t.faceVertices[f * 4u + a] == t.faceVertices[f * 4u + b]) {
                    return FreeformStatus::FaceNotQuad;
                }
            }
        }
        for (int k = 0; k < 4; ++k) {
            const uint64_t key = pairKey(t.faceVertices[f * 4u + k], t.faceVertices[f * 4u + (k + 1) % 4]);
            const auto it = std::lower_bound(keys.begin(), keys.end(),
                                             std::make_pair(key, uint32_t{0}));
            if (it == keys.end() || it->first != key) return FreeformStatus::MissingEdge;
            const uint32_t e = it->second;
            t.faceEdges[f * 4u + k] = e;
            if (t.edgeFaceCount[e] >= 2u) return FreeformStatus::NonManifoldEdge;
            t.edgeFaces[e * 2u + t.edgeFaceCount[e]] = static_cast<int32_t>(f);
            ++t.edgeFaceCount[e];
        }
    }
    t.vertexEdges.assign(nv, {});
    t.vertexFaces.assign(nv, {});
    for (size_t e = 0; e < ne; ++e) {
        const uint32_t a = static_cast<uint32_t>(indexById(cage.vertices, cage.edges[e].v0));
        const uint32_t b = static_cast<uint32_t>(indexById(cage.vertices, cage.edges[e].v1));
        t.vertexEdges[a].push_back(static_cast<uint32_t>(e));
        t.vertexEdges[b].push_back(static_cast<uint32_t>(e));
    }
    for (size_t f = 0; f < nf; ++f) {
        for (int k = 0; k < 4; ++k) t.vertexFaces[t.faceVertices[f * 4u + k]].push_back(static_cast<uint32_t>(f));
    }
    if (out != nullptr) *out = std::move(t);
    return FreeformStatus::Ok;
}

FreeformStatus validateFreeformCage(const FreeformCage& cage) {
    if (cage.faces.empty()) return FreeformStatus::EmptyCage;
    if (cage.vertices.size() > kMaxFreeformVertices) return FreeformStatus::TooManyVertices;
    if (cage.edges.size() > kMaxFreeformEdges) return FreeformStatus::TooManyEdges;
    if (cage.faces.size() > kMaxFreeformFaces) return FreeformStatus::TooManyFaces;
    // Ids: non-zero, strictly ascending, below their marks.
    uint32_t last = 0;
    for (const FreeformVertex& v : cage.vertices) {
        if (idOf(v.id) == 0u || idOf(v.id) <= last) return FreeformStatus::IdInvalid;
        last = idOf(v.id);
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y)
            || !std::isfinite(v.position.z)) {
            return FreeformStatus::NonFinite;
        }
        if (std::fabs(v.position.x) > kMaxFreeformCoordinateMeters
            || std::fabs(v.position.y) > kMaxFreeformCoordinateMeters
            || std::fabs(v.position.z) > kMaxFreeformCoordinateMeters) {
            return FreeformStatus::OutOfRange;
        }
    }
    if (cage.nextVertexId <= last) return FreeformStatus::HighWaterInvalid;
    last = 0;
    for (const FreeformEdge& e : cage.edges) {
        if (idOf(e.id) == 0u || idOf(e.id) <= last) return FreeformStatus::IdInvalid;
        last = idOf(e.id);
        if (idOf(e.v0) >= idOf(e.v1)) return FreeformStatus::EdgeNotCanonical;
        if (!std::isfinite(e.crease)) return FreeformStatus::NonFinite;
        if (e.crease < 0.0 || e.crease > 1.0) return FreeformStatus::InvalidCrease;
    }
    if (cage.nextEdgeId <= last) return FreeformStatus::HighWaterInvalid;
    last = 0;
    for (const FreeformFace& f : cage.faces) {
        if (idOf(f.id) == 0u || idOf(f.id) <= last) return FreeformStatus::IdInvalid;
        last = idOf(f.id);
    }
    if (cage.nextFaceId <= last) return FreeformStatus::HighWaterInvalid;

    FreeformTopology t;
    const FreeformStatus structure = buildFreeformTopology(cage, &t);
    if (structure != FreeformStatus::Ok) return structure;
    for (const FreeformFace& f : cage.faces) {
        if (canonicalLoop(f.loop) != f.loop) return FreeformStatus::FaceNotCanonical;
    }
    // Duplicate faces: the same vertex set twice.
    {
        std::vector<std::array<uint32_t, 4>> sets(cage.faces.size());
        for (size_t f = 0; f < cage.faces.size(); ++f) {
            for (int k = 0; k < 4; ++k) sets[f][k] = t.faceVertices[f * 4u + k];
            std::sort(sets[f].begin(), sets[f].end());
        }
        std::sort(sets.begin(), sets.end());
        for (size_t i = 1; i < sets.size(); ++i) {
            if (sets[i] == sets[i - 1]) return FreeformStatus::DuplicateFace;
        }
    }
    // Every edge on one or two faces, and two faces traversing it opposite ways.
    for (size_t e = 0; e < cage.edges.size(); ++e) {
        if (t.edgeFaceCount[e] == 0u) return FreeformStatus::EdgeUnused;
        if (t.edgeFaceCount[e] == 2u) {
            int directions = 0;
            for (int side = 0; side < 2; ++side) {
                const uint32_t f = static_cast<uint32_t>(t.edgeFaces[e * 2u + side]);
                for (int k = 0; k < 4; ++k) {
                    if (t.faceEdges[f * 4u + k] == e) {
                        const uint32_t from = t.faceVertices[f * 4u + k];
                        const uint32_t v0 =
                                static_cast<uint32_t>(indexById(cage.vertices, cage.edges[e].v0));
                        directions += from == v0 ? 1 : -1;
                    }
                }
            }
            if (directions != 0) return FreeformStatus::InconsistentWinding;
        }
    }
    // Every vertex on a face, and its faces one fan.
    for (size_t v = 0; v < cage.vertices.size(); ++v) {
        const std::vector<uint32_t>& faces = t.vertexFaces[v];
        if (faces.empty()) return FreeformStatus::IsolatedVertex;
        std::vector<uint32_t> parent(faces.size());
        std::iota(parent.begin(), parent.end(), 0u);
        auto root = [&parent](uint32_t i) {
            while (parent[i] != i) i = parent[i] = parent[parent[i]];
            return i;
        };
        for (uint32_t e : t.vertexEdges[v]) {
            if (t.edgeFaceCount[e] != 2u) continue;
            const auto a = std::find(faces.begin(), faces.end(), static_cast<uint32_t>(t.edgeFaces[e * 2u]));
            const auto b = std::find(faces.begin(), faces.end(), static_cast<uint32_t>(t.edgeFaces[e * 2u + 1u]));
            if (a == faces.end() || b == faces.end()) continue;
            parent[root(static_cast<uint32_t>(a - faces.begin()))] = root(static_cast<uint32_t>(b - faces.begin()));
        }
        const uint32_t r0 = root(0);
        for (uint32_t i = 1; i < faces.size(); ++i) {
            if (root(i) != r0) return FreeformStatus::BowTieVertex;
        }
    }
    // No two corners of a face at one point.
    for (size_t f = 0; f < cage.faces.size(); ++f) {
        for (int a = 0; a < 4; ++a) {
            for (int b = a + 1; b < 4; ++b) {
                const DVec3 d = dsub(cage.vertices[t.faceVertices[f * 4u + a]].position,
                                     cage.vertices[t.faceVertices[f * 4u + b]].position);
                if (dlength(d) <= kFreeformCoincidenceMeters) return FreeformStatus::FaceDegenerate;
            }
        }
    }
    if (cage.subdivisionLevel > kMaxFreeformSubdivisionLevel) {
        return FreeformStatus::InvalidSubdivisionLevel;
    }
    const uint64_t derived = static_cast<uint64_t>(cage.faces.size())
                             << (2u * static_cast<uint32_t>(cage.subdivisionLevel));
    if (derived > kMaxFreeformDerivedQuads) return FreeformStatus::SubdivisionBudgetExceeded;
    if ((cage.symmetry & ~kFreeformSymmetryMask) != 0u) return FreeformStatus::InvalidSymmetry;
    return validateSymmetryExact(cage);
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------

FreeformCage assembleFreeformCage(const std::vector<DVec3>& positions,
                                  const std::vector<std::array<uint32_t, 4>>& quads,
                                  uint8_t subdivisionLevel) {
    FreeformCage cage;
    cage.subdivisionLevel = subdivisionLevel;
    for (size_t i = 0; i < positions.size(); ++i) {
        cage.vertices.push_back(FreeformVertex{FreeformVertexId{static_cast<uint32_t>(i + 1u)},
                                               positions[i]});
    }
    cage.nextVertexId = static_cast<uint32_t>(positions.size() + 1u);
    std::map<uint64_t, bool> seen;
    for (const std::array<uint32_t, 4>& quad : quads) {
        for (int k = 0; k < 4; ++k) {
            const uint32_t a = quad[k] + 1u;
            const uint32_t b = quad[(k + 1) % 4] + 1u;
            const uint64_t key = pairKey(a, b);
            if (seen.count(key)) continue;
            seen[key] = true;
            FreeformEdge edge;
            edge.id = FreeformEdgeId{cage.nextEdgeId++};
            edge.v0 = FreeformVertexId{std::min(a, b)};
            edge.v1 = FreeformVertexId{std::max(a, b)};
            cage.edges.push_back(edge);
        }
    }
    for (const std::array<uint32_t, 4>& quad : quads) {
        FreeformFace face;
        face.id = FreeformFaceId{cage.nextFaceId++};
        std::array<FreeformVertexId, 4> loop{};
        for (int k = 0; k < 4; ++k) loop[k] = FreeformVertexId{quad[k] + 1u};
        face.loop = canonicalLoop(loop);
        cage.faces.push_back(face);
    }
    return cage;
}

FreeformCage makeFreeformBox() {
    const double h = 0.5;
    const std::vector<DVec3> p = {
            {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
            {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h},
    };
    const std::vector<std::array<uint32_t, 4>> q = {
            {0, 3, 2, 1},  // -Z
            {4, 5, 6, 7},  // +Z
            {0, 1, 5, 4},  // -Y
            {3, 7, 6, 2},  // +Y
            {0, 4, 7, 3},  // -X
            {1, 2, 6, 5},  // +X
    };
    return assembleFreeformCage(p, q, 2);
}

FreeformCage makeFreeformPlane() {
    std::vector<DVec3> p;
    for (int i = 0; i <= 4; ++i) {
        for (int j = 0; j <= 4; ++j) {
            p.push_back(DVec3{-1.0 + 0.5 * i, 0.0, -1.0 + 0.5 * j});
        }
    }
    auto at = [](int i, int j) { return static_cast<uint32_t>(i * 5 + j); };
    std::vector<std::array<uint32_t, 4>> q;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // (x0, z0) -> (x0, z1) -> (x1, z1) -> (x1, z0): counter-clockwise
            // seen from +Y.
            q.push_back({at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i + 1, j)});
        }
    }
    return assembleFreeformCage(p, q, 2);
}

FreeformCage makeFreeformCylinder() {
    const double r = 0.5;
    const double h = 0.5;
    const double s = r * std::sqrt(0.5);
    // Eight ring points from +X toward +Z, built from first-quadrant values and
    // mirrored by sign so the ring is bit-exactly symmetric.
    const double ring[8][2] = {{r, 0.0}, {s, s}, {0.0, r}, {-s, s},
                               {-r, 0.0}, {-s, -s}, {0.0, -r}, {s, -s}};
    std::vector<DVec3> p;
    for (int k = 0; k < 8; ++k) p.push_back(DVec3{ring[k][0], -h, ring[k][1]});  // bottom 0..7
    for (int k = 0; k < 8; ++k) p.push_back(DVec3{ring[k][0], h, ring[k][1]});   // top 8..15
    p.push_back(DVec3{0.0, -h, 0.0});                                            // 16
    p.push_back(DVec3{0.0, h, 0.0});                                             // 17
    auto b = [](int k) { return static_cast<uint32_t>((k + 8) % 8); };
    auto t = [](int k) { return static_cast<uint32_t>(8 + (k + 8) % 8); };
    std::vector<std::array<uint32_t, 4>> q;
    for (int k = 0; k < 8; ++k) q.push_back({b(k), t(k), t(k + 1), b(k + 1)});
    for (int m = 0; m < 4; ++m) q.push_back({b(2 * m), b(2 * m + 1), b(2 * m + 2), 16u});
    for (int m = 0; m < 4; ++m) q.push_back({17u, t(2 * m + 2), t(2 * m + 1), t(2 * m)});
    return assembleFreeformCage(p, q, 2);
}

// ---------------------------------------------------------------------------
// Selections
// ---------------------------------------------------------------------------

FreeformStatus freeformSelectionVertices(const FreeformCage& cage, FreeformElement element,
                                         const std::vector<uint32_t>& ids,
                                         std::vector<FreeformVertexId>* out) {
    std::vector<FreeformVertexId> vertices;
    for (uint32_t id : ids) {
        switch (element) {
            case FreeformElement::Vertex:
                if (findFreeformVertex(cage, FreeformVertexId{id}) == nullptr) {
                    return FreeformStatus::UnknownVertex;
                }
                vertices.push_back(FreeformVertexId{id});
                break;
            case FreeformElement::Edge: {
                const FreeformEdge* edge = findFreeformEdge(cage, FreeformEdgeId{id});
                if (edge == nullptr) return FreeformStatus::UnknownEdge;
                vertices.push_back(edge->v0);
                vertices.push_back(edge->v1);
                break;
            }
            case FreeformElement::Face: {
                const FreeformFace* face = findFreeformFace(cage, FreeformFaceId{id});
                if (face == nullptr) return FreeformStatus::UnknownFace;
                for (FreeformVertexId v : face->loop) vertices.push_back(v);
                break;
            }
        }
    }
    std::sort(vertices.begin(), vertices.end(),
              [](FreeformVertexId a, FreeformVertexId b) { return idOf(a) < idOf(b); });
    vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
    if (out != nullptr) *out = std::move(vertices);
    return FreeformStatus::Ok;
}

bool freeformSelectionCentroid(const FreeformCage& cage, FreeformElement element,
                               const std::vector<uint32_t>& ids, DVec3* out) {
    std::vector<FreeformVertexId> vertices;
    if (freeformSelectionVertices(cage, element, ids, &vertices) != FreeformStatus::Ok
        || vertices.empty()) {
        return false;
    }
    DVec3 sum{0.0, 0.0, 0.0};
    for (FreeformVertexId id : vertices) sum = dvec3Add(sum, findFreeformVertex(cage, id)->position);
    if (out != nullptr) *out = dvec3Scale(sum, 1.0 / static_cast<double>(vertices.size()));
    return true;
}

bool freeformFaceNormal(const FreeformCage& cage, const FreeformFace& face, DVec3* out) {
    // Newell's method: exact for a planar quad, the best-fit normal otherwise.
    DVec3 n{0.0, 0.0, 0.0};
    for (int k = 0; k < 4; ++k) {
        const FreeformVertex* a = findFreeformVertex(cage, face.loop[k]);
        const FreeformVertex* b = findFreeformVertex(cage, face.loop[(k + 1) % 4]);
        if (a == nullptr || b == nullptr) return false;
        n.x += (a->position.y - b->position.y) * (a->position.z + b->position.z);
        n.y += (a->position.z - b->position.z) * (a->position.x + b->position.x);
        n.z += (a->position.x - b->position.x) * (a->position.y + b->position.y);
    }
    DVec3 unit;
    if (!dvec3Normalized(n, &unit)) return false;
    if (out != nullptr) *out = unit;
    return true;
}

std::vector<uint32_t> freeformSymmetricSelection(const FreeformCage& cage, FreeformElement element,
                                                 const std::vector<uint32_t>& ids) {
    std::vector<uint32_t> out = ids;
    const std::vector<uint8_t> elements = symmetryElements(cage.symmetry);
    if (!elements.empty()) {
        const auto partners = symmetryPartners(cage, elements, symmetryTolerance(cage));
        auto mapVertex = [&cage, &partners](size_t g, FreeformVertexId id, FreeformVertexId* image) {
            const int32_t v = indexById(cage.vertices, id);
            if (v < 0 || partners[g][static_cast<size_t>(v)] < 0) return false;
            *image = cage.vertices[static_cast<size_t>(partners[g][static_cast<size_t>(v)])].id;
            return true;
        };
        for (uint32_t id : ids) {
            for (size_t g = 0; g < elements.size(); ++g) {
                switch (element) {
                    case FreeformElement::Vertex: {
                        FreeformVertexId image;
                        if (mapVertex(g, FreeformVertexId{id}, &image)) out.push_back(idOf(image));
                        break;
                    }
                    case FreeformElement::Edge: {
                        const FreeformEdge* edge = findFreeformEdge(cage, FreeformEdgeId{id});
                        FreeformVertexId a;
                        FreeformVertexId b;
                        if (edge != nullptr && mapVertex(g, edge->v0, &a) && mapVertex(g, edge->v1, &b)) {
                            if (const FreeformEdge* image = findFreeformEdgeBetween(cage, a, b)) {
                                out.push_back(idOf(image->id));
                            }
                        }
                        break;
                    }
                    case FreeformElement::Face: {
                        const FreeformFace* face = findFreeformFace(cage, FreeformFaceId{id});
                        if (face == nullptr) break;
                        std::array<uint32_t, 4> image{};
                        bool ok = true;
                        for (int k = 0; k < 4 && ok; ++k) {
                            FreeformVertexId v;
                            ok = mapVertex(g, face->loop[k], &v);
                            image[k] = idOf(v);
                        }
                        if (!ok) break;
                        std::sort(image.begin(), image.end());
                        for (const FreeformFace& other : cage.faces) {
                            std::array<uint32_t, 4> set{};
                            for (int k = 0; k < 4; ++k) set[k] = idOf(other.loop[k]);
                            std::sort(set.begin(), set.end());
                            if (set == image) {
                                out.push_back(idOf(other.id));
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// ---------------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------------

namespace {

FreeformStatus finishTool(FreeformCage* cage) {
    const FreeformStatus why = validateFreeformCage(*cage);
    return why;
}

// Applies the moved positions, then lets the moved vertices drive their mirror
// partners exactly (pairs taken from the cage BEFORE the move, which is exactly
// symmetric, so the pairing is exact).
FreeformStatus moveWithSymmetry(const FreeformCage& before, FreeformCage* after,
                                const std::vector<uint8_t>& moved) {
    const std::vector<uint8_t> elements = symmetryElements(before.symmetry);
    if (elements.empty()) return FreeformStatus::Ok;
    const auto partners = symmetryPartners(before, elements, symmetryTolerance(before));
    return snapSymmetry(after, partners, elements, &moved);
}

bool distanceUsable(double distance) {
    return std::isfinite(distance) && std::fabs(distance) >= kMinFreeformDistanceMeters
           && std::fabs(distance) <= kMaxFreeformCoordinateMeters;
}

}  // namespace

FreeformStatus freeformTransform(const FreeformCage& cage, FreeformElement element,
                                 const std::vector<uint32_t>& ids, const FreeformAffine& affine,
                                 FreeformCage* out) {
    if (ids.empty()) return FreeformStatus::EmptySelection;
    for (double value : affine.linear) {
        if (!std::isfinite(value)) return FreeformStatus::NonFinite;
    }
    if (!dvec3Finite(affine.translation) || !dvec3Finite(affine.pivot)) return FreeformStatus::NonFinite;
    const double* m = affine.linear;
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7]) - m[1] * (m[3] * m[8] - m[5] * m[6])
                       + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (!(det > 1.0e-12)) return FreeformStatus::TransformDegenerate;
    std::vector<FreeformVertexId> vertices;
    const FreeformStatus selected = freeformSelectionVertices(cage, element, ids, &vertices);
    if (selected != FreeformStatus::Ok) return selected;
    FreeformCage next = cage;
    std::vector<uint8_t> moved(cage.vertices.size(), 0u);
    for (FreeformVertexId id : vertices) {
        const int32_t v = indexById(next.vertices, id);
        const DVec3 d = dsub(next.vertices[static_cast<size_t>(v)].position, affine.pivot);
        const DVec3 l{m[0] * d.x + m[1] * d.y + m[2] * d.z, m[3] * d.x + m[4] * d.y + m[5] * d.z,
                      m[6] * d.x + m[7] * d.y + m[8] * d.z};
        next.vertices[static_cast<size_t>(v)].position =
                dvec3Add(dvec3Add(affine.pivot, l), affine.translation);
        moved[static_cast<size_t>(v)] = 1u;
    }
    const FreeformStatus mirrored = moveWithSymmetry(cage, &next, moved);
    if (mirrored != FreeformStatus::Ok) return mirrored;
    const FreeformStatus why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformPushPull(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                double distance, FreeformCage* out) {
    if (faceIds.empty()) return FreeformStatus::EmptySelection;
    if (!distanceUsable(distance)) return FreeformStatus::InvalidDistance;
    // Each vertex moves along the normalized sum of its selected faces' normals.
    std::vector<DVec3> push(cage.vertices.size(), DVec3{0.0, 0.0, 0.0});
    std::vector<uint8_t> moved(cage.vertices.size(), 0u);
    for (uint32_t id : faceIds) {
        const FreeformFace* face = findFreeformFace(cage, FreeformFaceId{id});
        if (face == nullptr) return FreeformStatus::UnknownFace;
        DVec3 n;
        if (!freeformFaceNormal(cage, *face, &n)) return FreeformStatus::FaceDegenerate;
        for (FreeformVertexId v : face->loop) {
            const size_t index = static_cast<size_t>(indexById(cage.vertices, v));
            push[index] = dvec3Add(push[index], n);
            moved[index] = 1u;
        }
    }
    FreeformCage next = cage;
    for (size_t v = 0; v < next.vertices.size(); ++v) {
        if (!moved[v]) continue;
        DVec3 dir;
        if (!dvec3Normalized(push[v], &dir)) return FreeformStatus::FaceDegenerate;
        next.vertices[v].position = dvec3Add(next.vertices[v].position, dvec3Scale(dir, distance));
    }
    const FreeformStatus mirrored = moveWithSymmetry(cage, &next, moved);
    if (mirrored != FreeformStatus::Ok) return mirrored;
    const FreeformStatus why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformExtrudeFaces(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                    double distance, FreeformCage* out) {
    if (faceIds.empty()) return FreeformStatus::EmptySelection;
    if (!distanceUsable(distance)) return FreeformStatus::InvalidDistance;
    for (uint32_t id : faceIds) {
        if (findFreeformFace(cage, FreeformFaceId{id}) == nullptr) return FreeformStatus::UnknownFace;
    }
    const std::vector<uint32_t> selection =
            freeformSymmetricSelection(cage, FreeformElement::Face, faceIds);
    FreeformTopology t;
    FreeformStatus why = buildFreeformTopology(cage, &t);
    if (why != FreeformStatus::Ok) return why;
    std::vector<uint8_t> inRegion(cage.faces.size(), 0u);
    for (uint32_t id : selection) {
        inRegion[static_cast<size_t>(indexById(cage.faces, FreeformFaceId{id}))] = 1u;
    }
    // Region boundary edges, each with the direction its region face walks it.
    struct BoundaryEdge {
        uint32_t edge;
        uint32_t from;
        uint32_t to;
    };
    std::vector<BoundaryEdge> boundary;
    for (size_t f = 0; f < cage.faces.size(); ++f) {
        if (!inRegion[f]) continue;
        for (int k = 0; k < 4; ++k) {
            const uint32_t e = t.faceEdges[f * 4u + k];
            int inside = 0;
            for (uint8_t s = 0; s < t.edgeFaceCount[e]; ++s) {
                inside += inRegion[static_cast<size_t>(t.edgeFaces[e * 2u + s])] ? 1 : 0;
            }
            if (inside == 1) {
                boundary.push_back(BoundaryEdge{e, t.faceVertices[f * 4u + k],
                                                t.faceVertices[f * 4u + (k + 1) % 4]});
            }
        }
    }
    std::sort(boundary.begin(), boundary.end(),
              [&cage](const BoundaryEdge& a, const BoundaryEdge& b) {
                  return idOf(cage.edges[a.edge].id) < idOf(cage.edges[b.edge].id);
              });
    std::vector<int> boundaryDegree(cage.vertices.size(), 0);
    for (const BoundaryEdge& b : boundary) {
        ++boundaryDegree[b.from];
        ++boundaryDegree[b.to];
    }
    for (int degree : boundaryDegree) {
        if (degree != 0 && degree != 2) return FreeformStatus::ExtrudeRegionPinched;
    }
    // Per region vertex: the normalized sum of its region faces' normals.
    std::vector<DVec3> push(cage.vertices.size(), DVec3{0.0, 0.0, 0.0});
    std::vector<uint8_t> regionVertex(cage.vertices.size(), 0u);
    for (size_t f = 0; f < cage.faces.size(); ++f) {
        if (!inRegion[f]) continue;
        DVec3 n;
        if (!freeformFaceNormal(cage, cage.faces[f], &n)) return FreeformStatus::FaceDegenerate;
        for (int k = 0; k < 4; ++k) {
            const uint32_t v = t.faceVertices[f * 4u + k];
            push[v] = dvec3Add(push[v], n);
            regionVertex[v] = 1u;
        }
    }
    FreeformCage next = cage;
    if (next.vertices.size() + boundary.size() > kMaxFreeformVertices) {
        return FreeformStatus::TooManyVertices;
    }
    // New vertices: one per boundary vertex, in ascending id order.
    std::vector<int64_t> dup(cage.vertices.size(), -1);
    for (size_t v = 0; v < cage.vertices.size(); ++v) {
        if (!regionVertex[v]) continue;
        DVec3 dir;
        if (!dvec3Normalized(push[v], &dir)) return FreeformStatus::FaceDegenerate;
        const DVec3 moved = dvec3Add(cage.vertices[v].position, dvec3Scale(dir, distance));
        if (boundaryDegree[v] == 2) {
            const FreeformVertexId id{next.nextVertexId++};
            next.vertices.push_back(FreeformVertex{id, moved});
            dup[v] = static_cast<int64_t>(idOf(id));
        } else {
            next.vertices[v].position = moved;  // interior: the vertex itself moves
        }
    }
    auto mapped = [&cage, &dup](uint32_t v) {
        return dup[v] >= 0 ? FreeformVertexId{static_cast<uint32_t>(dup[v])} : cage.vertices[v].id;
    };
    // Region faces stand on the new vertices; their ids are kept.
    for (size_t f = 0; f < cage.faces.size(); ++f) {
        if (!inRegion[f]) continue;
        std::array<FreeformVertexId, 4> loop{};
        for (int k = 0; k < 4; ++k) loop[k] = mapped(t.faceVertices[f * 4u + k]);
        next.faces[f].loop = canonicalLoop(loop);
    }
    // Interior region edges follow their vertices; boundary edges stay.
    for (size_t e = 0; e < cage.edges.size(); ++e) {
        int inside = 0;
        for (uint8_t s = 0; s < t.edgeFaceCount[e]; ++s) {
            inside += inRegion[static_cast<size_t>(t.edgeFaces[e * 2u + s])] ? 1 : 0;
        }
        if (inside != t.edgeFaceCount[e] || inside == 0) continue;
        const uint32_t a = static_cast<uint32_t>(indexById(cage.vertices, cage.edges[e].v0));
        const uint32_t b = static_cast<uint32_t>(indexById(cage.vertices, cage.edges[e].v1));
        const FreeformVertexId ma = mapped(a);
        const FreeformVertexId mb = mapped(b);
        next.edges[e].v0 = idOf(ma) < idOf(mb) ? ma : mb;
        next.edges[e].v1 = idOf(ma) < idOf(mb) ? mb : ma;
    }
    if (next.edges.size() + 2u * boundary.size() > kMaxFreeformEdges) {
        return FreeformStatus::TooManyEdges;
    }
    if (next.faces.size() + boundary.size() > kMaxFreeformFaces) return FreeformStatus::TooManyFaces;
    // New edges: the verticals by vertex id, then the tops by old edge id.
    for (size_t v = 0; v < cage.vertices.size(); ++v) {
        if (dup[v] < 0) continue;
        next.edges.push_back(FreeformEdge{FreeformEdgeId{next.nextEdgeId++}, cage.vertices[v].id,
                                          FreeformVertexId{static_cast<uint32_t>(dup[v])}, 0.0});
    }
    for (const BoundaryEdge& b : boundary) {
        const FreeformVertexId ma = mapped(b.from);
        const FreeformVertexId mb = mapped(b.to);
        next.edges.push_back(FreeformEdge{FreeformEdgeId{next.nextEdgeId++},
                                          idOf(ma) < idOf(mb) ? ma : mb,
                                          idOf(ma) < idOf(mb) ? mb : ma, cage.edges[b.edge].crease});
    }
    // New side faces: (a, b, b', a') along each boundary edge as its region face
    // walks it, so the old edge is now walked the other way by the side wall.
    for (const BoundaryEdge& b : boundary) {
        const std::array<FreeformVertexId, 4> loop{cage.vertices[b.from].id, cage.vertices[b.to].id,
                                                   mapped(b.to), mapped(b.from)};
        next.faces.push_back(FreeformFace{FreeformFaceId{next.nextFaceId++}, canonicalLoop(loop)});
    }
    why = resnapSymmetry(&next);
    if (why != FreeformStatus::Ok) return why;
    why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

namespace {

struct RingStep {
    uint32_t edge;   // edge index
    uint32_t start;  // vertex index the ratio is measured from
};

struct RingFace {
    uint32_t face;
    int side;  // the side the ring enters by; it leaves by side + 2
};

FreeformStatus walkRing(const FreeformCage& cage, const FreeformTopology& t, uint32_t edge,
                        uint32_t start, std::vector<RingStep>* outSteps,
                        std::vector<RingFace>* outFaces, bool* outClosed) {
    std::vector<uint8_t> visited(cage.faces.size(), 0u);
    auto walk = [&](int32_t firstFace, std::vector<RingStep>* steps, std::vector<RingFace>* faces,
                    bool* closed) -> FreeformStatus {
        uint32_t cur = edge;
        uint32_t curStart = start;
        int32_t f = firstFace;
        *closed = false;
        while (f >= 0) {
            const uint32_t fu = static_cast<uint32_t>(f);
            if (visited[fu]) return FreeformStatus::EdgeLoopSelfCrossing;
            visited[fu] = 1u;
            int k = -1;
            for (int s = 0; s < 4; ++s) {
                if (t.faceEdges[fu * 4u + s] == cur) k = s;
            }
            if (k < 0) return FreeformStatus::MissingEdge;
            const uint32_t vk = t.faceVertices[fu * 4u + k];
            const uint32_t oppositeStart = curStart == vk ? t.faceVertices[fu * 4u + (k + 3) % 4]
                                                          : t.faceVertices[fu * 4u + (k + 2) % 4];
            const uint32_t opposite = t.faceEdges[fu * 4u + (k + 2) % 4];
            faces->push_back(RingFace{fu, k});
            if (opposite == edge) {
                *closed = true;
                return FreeformStatus::Ok;
            }
            steps->push_back(RingStep{opposite, oppositeStart});
            int32_t nextFace = -1;
            for (uint8_t s = 0; s < t.edgeFaceCount[opposite]; ++s) {
                if (t.edgeFaces[opposite * 2u + s] != f) nextFace = t.edgeFaces[opposite * 2u + s];
            }
            cur = opposite;
            curStart = oppositeStart;
            f = nextFace;
        }
        return FreeformStatus::Ok;
    };
    std::vector<RingStep> forward{RingStep{edge, start}};
    std::vector<RingFace> forwardFaces;
    bool closed = false;
    FreeformStatus why = walk(t.edgeFaces[edge * 2u], &forward, &forwardFaces, &closed);
    if (why != FreeformStatus::Ok) return why;
    std::vector<RingStep> backward;
    std::vector<RingFace> backwardFaces;
    if (!closed && t.edgeFaceCount[edge] == 2u) {
        bool unused = false;
        why = walk(t.edgeFaces[edge * 2u + 1u], &backward, &backwardFaces, &unused);
        if (why != FreeformStatus::Ok) return why;
    }
    std::vector<RingStep> steps(backward.rbegin(), backward.rend());
    steps.insert(steps.end(), forward.begin(), forward.end());
    std::vector<RingFace> faces(backwardFaces.rbegin(), backwardFaces.rend());
    faces.insert(faces.end(), forwardFaces.begin(), forwardFaces.end());
    // Every ring edge appears once; a repeat is a ring crossing itself.
    std::vector<uint32_t> edges;
    for (const RingStep& s : steps) edges.push_back(s.edge);
    std::sort(edges.begin(), edges.end());
    if (std::adjacent_find(edges.begin(), edges.end()) != edges.end()) {
        return FreeformStatus::EdgeLoopSelfCrossing;
    }
    *outSteps = std::move(steps);
    *outFaces = std::move(faces);
    *outClosed = closed;
    return FreeformStatus::Ok;
}

FreeformStatus insertLoopFrom(const FreeformCage& cage, FreeformEdgeId edgeId,
                              FreeformVertexId startId, double ratio, FreeformCage* out) {
    FreeformTopology t;
    FreeformStatus why = buildFreeformTopology(cage, &t);
    if (why != FreeformStatus::Ok) return why;
    const int32_t e = indexById(cage.edges, edgeId);
    const int32_t s = indexById(cage.vertices, startId);
    if (e < 0) return FreeformStatus::UnknownEdge;
    if (s < 0) return FreeformStatus::UnknownVertex;
    std::vector<RingStep> steps;
    std::vector<RingFace> faces;
    bool closed = false;
    why = walkRing(cage, t, static_cast<uint32_t>(e), static_cast<uint32_t>(s), &steps, &faces, &closed);
    if (why != FreeformStatus::Ok) return why;
    if (cage.vertices.size() + steps.size() > kMaxFreeformVertices) return FreeformStatus::TooManyVertices;
    if (cage.edges.size() + steps.size() + faces.size() > kMaxFreeformEdges) {
        return FreeformStatus::TooManyEdges;
    }
    if (cage.faces.size() + faces.size() > kMaxFreeformFaces) return FreeformStatus::TooManyFaces;
    FreeformCage next = cage;
    // New vertices, in ring order, at the ratio from each edge's ring start.
    std::vector<int64_t> splitVertex(cage.edges.size(), -1);
    for (const RingStep& step : steps) {
        const FreeformEdge& edge = cage.edges[step.edge];
        const uint32_t a = static_cast<uint32_t>(indexById(cage.vertices, edge.v0));
        const uint32_t b = static_cast<uint32_t>(indexById(cage.vertices, edge.v1));
        const uint32_t end = step.start == a ? b : a;
        const DVec3 p0 = cage.vertices[step.start].position;
        const DVec3 p1 = cage.vertices[end].position;
        const DVec3 p = dvec3Add(p0, dvec3Scale(dsub(p1, p0), ratio));
        const FreeformVertexId id{next.nextVertexId++};
        next.vertices.push_back(FreeformVertex{id, p});
        splitVertex[step.edge] = idOf(id);
    }
    // Each ring edge splits: the half on its v0 keeps the id, the other half is new.
    for (const RingStep& step : steps) {
        FreeformEdge& kept = next.edges[step.edge];
        const FreeformVertexId m{static_cast<uint32_t>(splitVertex[step.edge])};
        const FreeformVertexId far = kept.v1;
        kept.v1 = m;
        next.edges.push_back(FreeformEdge{FreeformEdgeId{next.nextEdgeId++}, far, m, kept.crease});
    }
    // Each crossed face splits along a new edge between its two split points.
    std::vector<FreeformFace> added;
    for (const RingFace& rf : faces) {
        const uint32_t f = rf.face;
        const int k = rf.side;
        const uint32_t A = t.faceVertices[f * 4u + k];
        const uint32_t B = t.faceVertices[f * 4u + (k + 1) % 4];
        const uint32_t C = t.faceVertices[f * 4u + (k + 2) % 4];
        const uint32_t D = t.faceVertices[f * 4u + (k + 3) % 4];
        const FreeformVertexId m1{static_cast<uint32_t>(splitVertex[t.faceEdges[f * 4u + k]])};
        const FreeformVertexId m2{static_cast<uint32_t>(splitVertex[t.faceEdges[f * 4u + (k + 2) % 4]])};
        next.edges.push_back(FreeformEdge{FreeformEdgeId{next.nextEdgeId++},
                                          idOf(m1) < idOf(m2) ? m1 : m2,
                                          idOf(m1) < idOf(m2) ? m2 : m1, 0.0});
        const std::array<FreeformVertexId, 4> first{cage.vertices[A].id, m1, m2, cage.vertices[D].id};
        const std::array<FreeformVertexId, 4> second{m1, cage.vertices[B].id, cage.vertices[C].id, m2};
        const FreeformVertexId origin = cage.faces[f].loop[0];
        const bool firstKeeps = origin == cage.vertices[A].id || origin == cage.vertices[D].id;
        next.faces[f].loop = canonicalLoop(firstKeeps ? first : second);
        added.push_back(FreeformFace{kNoFreeformFace, canonicalLoop(firstKeeps ? second : first)});
    }
    for (FreeformFace& face : added) {
        face.id = FreeformFaceId{next.nextFaceId++};
        next.faces.push_back(face);
    }
    // Edge records were appended in id order but split halves keep their slot:
    // the table stays strictly ascending by id.
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

}  // namespace

FreeformStatus freeformEdgeRing(const FreeformCage& cage, uint32_t edgeId,
                                std::vector<uint32_t>* outEdges, bool* outClosed) {
    FreeformTopology t;
    FreeformStatus why = buildFreeformTopology(cage, &t);
    if (why != FreeformStatus::Ok) return why;
    const int32_t e = indexById(cage.edges, FreeformEdgeId{edgeId});
    if (e < 0) return FreeformStatus::UnknownEdge;
    const uint32_t start =
            static_cast<uint32_t>(indexById(cage.vertices, cage.edges[static_cast<size_t>(e)].v0));
    std::vector<RingStep> steps;
    std::vector<RingFace> faces;
    bool closed = false;
    why = walkRing(cage, t, static_cast<uint32_t>(e), start, &steps, &faces, &closed);
    if (why != FreeformStatus::Ok) return why;
    if (outEdges != nullptr) {
        outEdges->clear();
        for (const RingStep& s : steps) outEdges->push_back(idOf(cage.edges[s.edge].id));
    }
    if (outClosed != nullptr) *outClosed = closed;
    return FreeformStatus::Ok;
}

FreeformStatus freeformInsertEdgeLoop(const FreeformCage& cage, uint32_t edgeId, double ratio,
                                      FreeformCage* out) {
    if (!std::isfinite(ratio) || ratio <= 0.0 || ratio >= 1.0) return FreeformStatus::InvalidRatio;
    const FreeformEdge* edge = findFreeformEdge(cage, FreeformEdgeId{edgeId});
    if (edge == nullptr) return FreeformStatus::UnknownEdge;
    // The rings to insert: the chosen one, and -- under symmetry -- each mirror
    // image on a ring of its own. A mirror image on the SAME ring would need a
    // second loop unless the ratio is the midpoint, which is refused by name
    // rather than inserted twice.
    struct Insertion {
        FreeformEdgeId edge;
        FreeformVertexId start;
    };
    std::vector<Insertion> insertions{Insertion{edge->id, edge->v0}};
    const std::vector<uint8_t> elements = symmetryElements(cage.symmetry);
    if (!elements.empty()) {
        const auto partners = symmetryPartners(cage, elements, symmetryTolerance(cage));
        std::vector<std::vector<uint32_t>> rings;
        std::vector<uint32_t> ring;
        bool closed = false;
        FreeformStatus why = freeformEdgeRing(cage, edgeId, &ring, &closed);
        if (why != FreeformStatus::Ok) return why;
        std::sort(ring.begin(), ring.end());
        rings.push_back(ring);
        for (size_t g = 0; g < elements.size(); ++g) {
            const int32_t a = partners[g][static_cast<size_t>(indexById(cage.vertices, edge->v0))];
            const int32_t b = partners[g][static_cast<size_t>(indexById(cage.vertices, edge->v1))];
            if (a < 0 || b < 0) return FreeformStatus::SymmetryBroken;
            const FreeformEdge* image = findFreeformEdgeBetween(
                    cage, cage.vertices[static_cast<size_t>(a)].id, cage.vertices[static_cast<size_t>(b)].id);
            if (image == nullptr) return FreeformStatus::SymmetryBroken;
            bool onKnownRing = false;
            for (size_t r = 0; r < rings.size(); ++r) {
                if (std::binary_search(rings[r].begin(), rings[r].end(), idOf(image->id))) {
                    onKnownRing = true;
                    if (r == 0 && ratio != 0.5) return FreeformStatus::SymmetryRequiresMidpoint;
                }
            }
            if (onKnownRing) continue;
            std::vector<uint32_t> other;
            why = freeformEdgeRing(cage, idOf(image->id), &other, &closed);
            if (why != FreeformStatus::Ok) return why;
            std::sort(other.begin(), other.end());
            rings.push_back(other);
            insertions.push_back(Insertion{image->id, cage.vertices[static_cast<size_t>(a)].id});
        }
    }
    FreeformCage next = cage;
    for (const Insertion& insertion : insertions) {
        FreeformCage step;
        const FreeformStatus why = insertLoopFrom(next, insertion.edge, insertion.start, ratio, &step);
        if (why != FreeformStatus::Ok) return why;
        next = std::move(step);
    }
    FreeformStatus why = resnapSymmetry(&next);
    if (why != FreeformStatus::Ok) return why;
    why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformSetCrease(const FreeformCage& cage, const std::vector<uint32_t>& edgeIds,
                                 double crease, FreeformCage* out) {
    if (edgeIds.empty()) return FreeformStatus::EmptySelection;
    if (!std::isfinite(crease)) return FreeformStatus::NonFinite;
    if (crease < 0.0 || crease > 1.0) return FreeformStatus::InvalidCrease;
    for (uint32_t id : edgeIds) {
        if (findFreeformEdge(cage, FreeformEdgeId{id}) == nullptr) return FreeformStatus::UnknownEdge;
    }
    FreeformCage next = cage;
    for (uint32_t id : freeformSymmetricSelection(cage, FreeformElement::Edge, edgeIds)) {
        next.edges[static_cast<size_t>(indexById(next.edges, FreeformEdgeId{id}))].crease = crease;
    }
    const FreeformStatus why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformDeleteFaces(const FreeformCage& cage, const std::vector<uint32_t>& faceIds,
                                   FreeformCage* out) {
    if (faceIds.empty()) return FreeformStatus::EmptySelection;
    for (uint32_t id : faceIds) {
        if (findFreeformFace(cage, FreeformFaceId{id}) == nullptr) return FreeformStatus::UnknownFace;
    }
    const std::vector<uint32_t> selection =
            freeformSymmetricSelection(cage, FreeformElement::Face, faceIds);
    FreeformCage next = cage;
    next.faces.erase(std::remove_if(next.faces.begin(), next.faces.end(),
                                    [&selection](const FreeformFace& face) {
                                        return std::binary_search(selection.begin(), selection.end(),
                                                                  idOf(face.id));
                                    }),
                     next.faces.end());
    if (next.faces.empty()) return FreeformStatus::DeleteWouldEmpty;
    std::vector<uint64_t> usedEdges;
    std::vector<uint32_t> usedVertices;
    for (const FreeformFace& face : next.faces) {
        for (int k = 0; k < 4; ++k) {
            usedEdges.push_back(pairKey(idOf(face.loop[k]), idOf(face.loop[(k + 1) % 4])));
            usedVertices.push_back(idOf(face.loop[k]));
        }
    }
    std::sort(usedEdges.begin(), usedEdges.end());
    std::sort(usedVertices.begin(), usedVertices.end());
    next.edges.erase(std::remove_if(next.edges.begin(), next.edges.end(),
                                    [&usedEdges](const FreeformEdge& edge) {
                                        return !std::binary_search(usedEdges.begin(), usedEdges.end(),
                                                                   pairKey(idOf(edge.v0), idOf(edge.v1)));
                                    }),
                     next.edges.end());
    next.vertices.erase(std::remove_if(next.vertices.begin(), next.vertices.end(),
                                       [&usedVertices](const FreeformVertex& vertex) {
                                           return !std::binary_search(usedVertices.begin(),
                                                                      usedVertices.end(),
                                                                      idOf(vertex.id));
                                       }),
                        next.vertices.end());
    const FreeformStatus why = finishTool(&next);
    if (why == FreeformStatus::BowTieVertex || why == FreeformStatus::NonManifoldEdge
        || why == FreeformStatus::IsolatedVertex) {
        return FreeformStatus::DeleteWouldBreakManifold;
    }
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformSetSubdivisionLevel(const FreeformCage& cage, int level, FreeformCage* out) {
    if (level < 0 || level > kMaxFreeformSubdivisionLevel) {
        return FreeformStatus::InvalidSubdivisionLevel;
    }
    FreeformCage next = cage;
    next.subdivisionLevel = static_cast<uint8_t>(level);
    const FreeformStatus why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

FreeformStatus freeformSetSymmetry(const FreeformCage& cage, uint8_t symmetry, FreeformCage* out) {
    if ((symmetry & ~kFreeformSymmetryMask) != 0u) return FreeformStatus::InvalidSymmetry;
    FreeformCage next = cage;
    next.symmetry = symmetry;
    if ((symmetry & ~cage.symmetry) != 0u) {
        // A plane turned ON: the cage must already be symmetric about the whole
        // group, within the tolerance, and is then made so exactly.
        const std::vector<uint8_t> elements = symmetryElements(symmetry);
        const auto partners = symmetryPartners(next, elements, symmetryTolerance(next));
        for (const std::vector<int32_t>& map : partners) {
            for (int32_t p : map) {
                if (p < 0) return FreeformStatus::CageNotSymmetric;
            }
        }
        if (!facesMapOntoFaces(next, partners)) return FreeformStatus::CageNotSymmetric;
        const FreeformStatus snapped = snapSymmetry(&next, partners, elements, nullptr);
        if (snapped != FreeformStatus::Ok) return FreeformStatus::CageNotSymmetric;
    }
    const FreeformStatus why = finishTool(&next);
    if (why != FreeformStatus::Ok) return why;
    if (out != nullptr) *out = std::move(next);
    return FreeformStatus::Ok;
}

// ---------------------------------------------------------------------------
// The body
// ---------------------------------------------------------------------------

FreeformStatus FreeformBody::applyCage(std::shared_ptr<const FreeformCage> next, bool* outChanged) {
    if (outChanged != nullptr) *outChanged = false;
    if (next == nullptr) return FreeformStatus::EmptyCage;
    if (next->nextVertexId < cage_->nextVertexId || next->nextEdgeId < cage_->nextEdgeId
        || next->nextFaceId < cage_->nextFaceId) {
        return FreeformStatus::HighWaterInvalid;
    }
    const FreeformStatus why = validateFreeformCage(*next);
    if (why != FreeformStatus::Ok) return why;
    if (sameFreeformCage(*next, *cage_)) return FreeformStatus::Ok;
    cage_ = std::move(next);
    ++updateCount_;
    if (outChanged != nullptr) *outChanged = true;
    return FreeformStatus::Ok;
}

void FreeformBody::restoreCage(std::shared_ptr<const FreeformCage> cage) {
    if (cage != nullptr) cage_ = std::move(cage);
}

FreeformStatus FreeformBody::derived(std::shared_ptr<const FreeformMesh>* out) const {
    if (out == nullptr) return FreeformStatus::EmptyCage;
    if (cache_ != nullptr && cacheCage_ == cage_) {
        *out = cache_;
        return FreeformStatus::Ok;
    }
    auto mesh = std::make_shared<FreeformMesh>();
    const FreeformStatus why = subdivideFreeformCage(*cage_, mesh.get());
    if (why != FreeformStatus::Ok) return why;
    cache_ = mesh;
    cacheCage_ = cage_;
    *out = cache_;
    return FreeformStatus::Ok;
}

}  // namespace forgeshape
