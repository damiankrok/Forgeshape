#include "forgeshape_sketch_region.h"

#include <algorithm>
#include <cmath>

namespace forgeshape {
namespace {

int orientation(const SketchPoint& a, const SketchPoint& b, const SketchPoint& c) {
    const double v = (b.u - a.u) * (c.v - a.v) - (b.v - a.v) * (c.u - a.u);
    if (v > 0.0) return 1;
    if (v < 0.0) return -1;
    return 0;
}

bool onSegment(const SketchPoint& p, const SketchPoint& a, const SketchPoint& b) {
    return std::min(a.u, b.u) <= p.u && p.u <= std::max(a.u, b.u) && std::min(a.v, b.v) <= p.v
           && p.v <= std::max(a.v, b.v);
}

// Whether any edge of one loop touches or crosses any edge of the other.
// Bounded: both loops are at most kMaxProfileVertices long.
bool loopsTouch(const std::vector<SketchPoint>& a, const std::vector<SketchPoint>& b) {
    // A cheap bounding-box reject first: most loop pairs in a sketch are apart.
    double aMinU = a[0].u, aMaxU = a[0].u, aMinV = a[0].v, aMaxV = a[0].v;
    for (const SketchPoint& p : a) {
        aMinU = std::min(aMinU, p.u); aMaxU = std::max(aMaxU, p.u);
        aMinV = std::min(aMinV, p.v); aMaxV = std::max(aMaxV, p.v);
    }
    double bMinU = b[0].u, bMaxU = b[0].u, bMinV = b[0].v, bMaxV = b[0].v;
    for (const SketchPoint& p : b) {
        bMinU = std::min(bMinU, p.u); bMaxU = std::max(bMaxU, p.u);
        bMinV = std::min(bMinV, p.v); bMaxV = std::max(bMaxV, p.v);
    }
    if (aMaxU < bMinU || bMaxU < aMinU || aMaxV < bMinV || bMaxV < aMinV) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const SketchPoint& a0 = a[i];
        const SketchPoint& a1 = a[(i + 1u) % a.size()];
        for (size_t j = 0; j < b.size(); ++j) {
            if (sketchSegmentsIntersect(a0, a1, b[j], b[(j + 1u) % b.size()])) {
                return true;
            }
        }
    }
    return false;
}

double signedAreaTwice(const std::vector<SketchPoint>& polygon) {
    return polygonSignedAreaTwice(polygon);
}

// The area centroid of a polygon, counter-clockwise or not; `twiceArea` out.
SketchPoint polygonCentroid(const std::vector<SketchPoint>& polygon, double* twiceArea) {
    double a = 0.0;
    double cu = 0.0;
    double cv = 0.0;
    const size_t n = polygon.size();
    for (size_t i = 0; i < n; ++i) {
        const SketchPoint& p = polygon[i];
        const SketchPoint& q = polygon[(i + 1u) % n];
        const double cross = p.u * q.v - q.u * p.v;
        a += cross;
        cu += (p.u + q.u) * cross;
        cv += (p.v + q.v) * cross;
    }
    *twiceArea = a;
    if (std::fabs(a) < 1e-300) {
        return polygon.empty() ? SketchPoint{} : polygon[0];
    }
    return SketchPoint{cu / (3.0 * a), cv / (3.0 * a)};
}

// The crossings of the horizontal line v = level with every edge of `loops`,
// sorted: consecutive pairs bound the inside by the even-odd rule. Edges are
// half-open in v so a vertex exactly on the line is counted once.
std::vector<double> crossingsAt(const std::vector<std::vector<SketchPoint>>& loops, double level) {
    std::vector<double> xs;
    for (const std::vector<SketchPoint>& loop : loops) {
        const size_t n = loop.size();
        for (size_t i = 0; i < n; ++i) {
            const SketchPoint& a = loop[i];
            const SketchPoint& b = loop[(i + 1u) % n];
            if ((a.v > level) != (b.v > level)) {
                xs.push_back(a.u + (level - a.v) * (b.u - a.u) / (b.v - a.v));
            }
        }
    }
    std::sort(xs.begin(), xs.end());
    return xs;
}

// A point strictly inside the region: the middle of the widest inside span on
// a few horizontal lines through the outer loop's vertical extent. The first
// candidate that is strictly inside the outer loop and strictly outside every
// hole wins; deterministic, never a centroid (which a centred hole swallows).
SketchPoint regionInteriorPoint(const std::vector<std::vector<SketchPoint>>& loops) {
    const std::vector<SketchPoint>& outer = loops[0];
    double minV = outer[0].v;
    double maxV = outer[0].v;
    for (const SketchPoint& p : outer) {
        minV = std::min(minV, p.v);
        maxV = std::max(maxV, p.v);
    }
    static const double kFractions[] = {0.5, 0.25, 0.75, 0.375, 0.625, 0.125, 0.875};
    for (double f : kFractions) {
        const double level = minV + (maxV - minV) * f;
        const std::vector<double> xs = crossingsAt(loops, level);
        double bestWidth = 0.0;
        SketchPoint best{};
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            const double width = xs[i + 1] - xs[i];
            if (width > bestWidth) {
                bestWidth = width;
                best = SketchPoint{(xs[i] + xs[i + 1]) * 0.5, level};
            }
        }
        if (bestWidth <= 0.0) {
            continue;
        }
        bool ok = sketchPointStrictlyInside(best, outer);
        for (size_t h = 1; ok && h < loops.size(); ++h) {
            ok = !sketchPointStrictlyInside(best, loops[h]);
        }
        if (ok) {
            return best;
        }
    }
    // A region too thin to find by scan still names a point: its first vertex.
    return outer[0];
}

bool ascendingUnique(const std::vector<SketchEntityId>& ids) {
    for (size_t i = 1; i < ids.size(); ++i) {
        if (!(ids[i - 1] < ids[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool sameProfileRegionRef(const ProfileRegionRef& a, const ProfileRegionRef& b) {
    return a.outerAnchorId == b.outerAnchorId && a.holeAnchorIds == b.holeAnchorIds;
}

bool sketchPointStrictlyInside(const SketchPoint& p, const std::vector<SketchPoint>& polygon) {
    const size_t n = polygon.size();
    if (n < 3u) {
        return false;
    }
    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const SketchPoint& a = polygon[i];
        const SketchPoint& b = polygon[j];
        if (orientation(a, b, p) == 0 && onSegment(p, a, b)) {
            return false;
        }
        const bool crosses = ((a.v > p.v) != (b.v > p.v))
                             && (p.u < (b.u - a.u) * (p.v - a.v) / (b.v - a.v) + a.u);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

SketchRegionExtraction extractSketchRegions(const CadSketch& sketch) {
    SketchRegionExtraction out;
    out.loops = extractClosedProfiles(sketch);
    const std::vector<ClosedProfile>& loops = out.loops.profiles;
    const size_t n = loops.size();
    out.conflict.assign(n * n, 0u);
    out.parent.assign(n, -1);

    // Pairwise: touch/cross, then clean containment. O(n^2) loop pairs, each
    // O(edges^2) only when their boxes overlap -- bounded by the sketch caps.
    out.contains.assign(n * n, 0u);
    std::vector<uint8_t>& contains = out.contains;
    for (size_t a = 0; a < n; ++a) {
        for (size_t b = a + 1; b < n; ++b) {
            if (loopsTouch(loops[a].polygon, loops[b].polygon)) {
                out.conflict[a * n + b] = 1u;
                out.conflict[b * n + a] = 1u;
                continue;
            }
            // With no edge contact, one vertex decides containment for the
            // whole loop.
            if (sketchPointStrictlyInside(loops[b].polygon[0], loops[a].polygon)) {
                contains[a * n + b] = 1u;
            } else if (sketchPointStrictlyInside(loops[a].polygon[0], loops[b].polygon)) {
                contains[b * n + a] = 1u;
            }
        }
    }
    // The parent is the SMALLEST clean container (ties by anchor, which the
    // ascending loop order already gives).
    std::vector<uint32_t> depth(n, 0u);
    for (size_t b = 0; b < n; ++b) {
        int32_t best = -1;
        for (size_t a = 0; a < n; ++a) {
            if (contains[a * n + b] == 0u) {
                continue;
            }
            ++depth[b];
            if (best < 0 || loops[a].area < loops[static_cast<size_t>(best)].area) {
                best = static_cast<int32_t>(a);
            }
        }
        out.parent[b] = best;
    }

    out.regions.resize(n);
    for (size_t l = 0; l < n; ++l) {
        SketchRegion& region = out.regions[l];
        region.outerAnchorId = loops[l].anchorEntityId;
        region.outerLoop = static_cast<uint32_t>(l);
        region.depth = depth[l];
        double area = loops[l].area;
        for (size_t c = 0; c < n; ++c) {
            if (out.parent[c] == static_cast<int32_t>(l)) {
                region.holeLoops.push_back(static_cast<uint32_t>(c));
                region.holeAnchorIds.push_back(loops[c].anchorEntityId);
                area -= loops[c].area;
            }
        }
        region.area = area;
        // Holes that touch or cross each other leave the region undefined
        // without an arrangement this build does not compute.
        for (size_t i = 0; i < region.holeLoops.size() && region.status == CadStatus::Ok; ++i) {
            for (size_t j = i + 1; j < region.holeLoops.size(); ++j) {
                if (out.loopsConflict(region.holeLoops[i], region.holeLoops[j])) {
                    region.status = CadStatus::OverlappingHoles;
                    break;
                }
            }
        }
        if (region.holeLoops.size() > kMaxRegionHoles) {
            region.status = CadStatus::TooManyRegions;
        }
        const std::vector<std::vector<SketchPoint>> polys = sketchRegionLoops(out, region);
        region.interiorPoint = regionInteriorPoint(polys);
        double twiceOuter = 0.0;
        const SketchPoint co = polygonCentroid(polys[0], &twiceOuter);
        double su = co.u * twiceOuter;
        double sv = co.v * twiceOuter;
        double total = twiceOuter;
        for (size_t h = 1; h < polys.size(); ++h) {
            double twiceHole = 0.0;
            const SketchPoint ch = polygonCentroid(polys[h], &twiceHole);
            su -= ch.u * twiceHole;
            sv -= ch.v * twiceHole;
            total -= twiceHole;
        }
        region.centroid = std::fabs(total) > 1e-300 ? SketchPoint{su / total, sv / total} : co;
    }
    return out;
}

const SketchRegion* findSketchRegion(const SketchRegionExtraction& extraction,
                                     SketchEntityId outerAnchorId) {
    for (const SketchRegion& region : extraction.regions) {
        if (region.outerAnchorId == outerAnchorId) {
            return &region;
        }
    }
    return nullptr;
}

ProfileRegionRef sketchRegionRef(const SketchRegion& region) {
    return ProfileRegionRef{region.outerAnchorId, region.holeAnchorIds};
}

CadStatus validateRegionSelection(const SketchRegionExtraction& extraction,
                                  const std::vector<ProfileRegionRef>& selection) {
    if (selection.empty()) {
        return extraction.regions.size() > 1u ? CadStatus::AmbiguousProfile
                                              : CadStatus::ProfileNotFound;
    }
    if (selection.size() > kMaxProfileRegions) {
        return CadStatus::TooManyRegions;
    }
    std::vector<const SketchRegion*> chosen;
    chosen.reserve(selection.size());
    for (size_t i = 0; i < selection.size(); ++i) {
        const ProfileRegionRef& ref = selection[i];
        if (i > 0 && !(selection[i - 1].outerAnchorId < ref.outerAnchorId)) {
            return CadStatus::ProfileRegionMismatch;
        }
        if (ref.holeAnchorIds.size() > kMaxRegionHoles) {
            return CadStatus::TooManyRegions;
        }
        if (!ascendingUnique(ref.holeAnchorIds)) {
            return CadStatus::ProfileRegionMismatch;
        }
        const SketchRegion* region = findSketchRegion(extraction, ref.outerAnchorId);
        if (region == nullptr) {
            return CadStatus::ProfileNotFound;
        }
        if (region->holeAnchorIds != ref.holeAnchorIds) {
            return CadStatus::ProfileRegionMismatch;
        }
        if (region->status != CadStatus::Ok) {
            return region->status;
        }
        chosen.push_back(region);
    }
    // Two chosen regions must be disjoint and must not touch: their outer loops
    // may not touch or cross, and when one outer loop lies inside the other it
    // must lie inside one of that region's HOLES -- strictly inside, because a
    // region and its own hole share the hole's loop as a boundary.
    auto insideAHole = [&extraction](const SketchRegion& container, uint32_t loop) {
        for (uint32_t h : container.holeLoops) {
            if (h != loop && extraction.loopContains(h, loop)) {
                return true;
            }
        }
        return false;
    };
    for (size_t i = 0; i < chosen.size(); ++i) {
        for (size_t j = i + 1; j < chosen.size(); ++j) {
            const uint32_t a = chosen[i]->outerLoop;
            const uint32_t b = chosen[j]->outerLoop;
            if (extraction.loopsConflict(a, b)) {
                return CadStatus::OverlappingRegions;
            }
            if (extraction.loopContains(a, b) && !insideAHole(*chosen[i], b)) {
                return CadStatus::OverlappingRegions;
            }
            if (extraction.loopContains(b, a) && !insideAHole(*chosen[j], a)) {
                return CadStatus::OverlappingRegions;
            }
            // A hole of one that touches the other's outer loop overlaps too.
            for (uint32_t h : chosen[i]->holeLoops) {
                if (extraction.loopsConflict(h, b)) {
                    return CadStatus::OverlappingRegions;
                }
            }
            for (uint32_t h : chosen[j]->holeLoops) {
                if (extraction.loopsConflict(h, a)) {
                    return CadStatus::OverlappingRegions;
                }
            }
        }
    }
    return CadStatus::Ok;
}

bool sketchRegionAt(const SketchRegionExtraction& extraction, const SketchPoint& point,
                    SketchEntityId* outOuterAnchorId) {
    const std::vector<ClosedProfile>& loops = extraction.loops.profiles;
    int32_t best = -1;
    for (size_t l = 0; l < loops.size(); ++l) {
        if (!sketchPointStrictlyInside(point, loops[l].polygon)) {
            continue;
        }
        if (best < 0 || loops[l].area < loops[static_cast<size_t>(best)].area) {
            best = static_cast<int32_t>(l);
        }
    }
    if (best < 0) {
        return false;
    }
    if (outOuterAnchorId != nullptr) {
        *outOuterAnchorId = loops[static_cast<size_t>(best)].anchorEntityId;
    }
    return true;
}

std::vector<ProfileRegionRef> toggleRegionSelection(const std::vector<ProfileRegionRef>& selection,
                                                    const SketchRegion& region) {
    std::vector<ProfileRegionRef> next;
    bool removed = false;
    for (const ProfileRegionRef& ref : selection) {
        if (ref.outerAnchorId == region.outerAnchorId) {
            removed = true;
            continue;
        }
        next.push_back(ref);
    }
    if (!removed) {
        next.push_back(sketchRegionRef(region));
        std::sort(next.begin(), next.end(), [](const ProfileRegionRef& a, const ProfileRegionRef& b) {
            return a.outerAnchorId < b.outerAnchorId;
        });
    }
    return next;
}

std::vector<std::vector<SketchPoint>> sketchRegionLoops(const SketchRegionExtraction& extraction,
                                                        const SketchRegion& region) {
    std::vector<std::vector<SketchPoint>> out;
    const std::vector<ClosedProfile>& loops = extraction.loops.profiles;
    if (region.outerLoop >= loops.size()) {
        return out;
    }
    out.push_back(loops[region.outerLoop].polygon);
    for (uint32_t h : region.holeLoops) {
        if (h < loops.size()) {
            out.push_back(loops[h].polygon);
        }
    }
    return out;
}

std::vector<SketchPoint> sketchRegionHatch(const SketchRegionExtraction& extraction,
                                           const SketchRegion& region, double spacing) {
    std::vector<SketchPoint> segments;
    const std::vector<std::vector<SketchPoint>> loops = sketchRegionLoops(extraction, region);
    if (loops.empty() || !(spacing > 0.0) || !std::isfinite(spacing)) {
        return segments;
    }
    double minV = loops[0][0].v;
    double maxV = loops[0][0].v;
    for (const SketchPoint& p : loops[0]) {
        minV = std::min(minV, p.v);
        maxV = std::max(maxV, p.v);
    }
    // Widen the spacing rather than exceed the line bound: a hatch is a hint,
    // never a cost that grows with how far the user zoomed in.
    double step = spacing;
    if ((maxV - minV) / step > static_cast<double>(kMaxRegionHatchLines)) {
        step = (maxV - minV) / static_cast<double>(kMaxRegionHatchLines);
    }
    // Lines on a fixed lattice (multiples of the step), so the hatch does not
    // swim as the region changes shape.
    const double first = std::ceil(minV / step) * step;
    uint32_t lines = 0;
    for (double level = first; level < maxV && lines < kMaxRegionHatchLines; level += step, ++lines) {
        if (level <= minV) {
            continue;
        }
        const std::vector<double> xs = crossingsAt(loops, level);
        for (size_t i = 0; i + 1 < xs.size(); i += 2) {
            segments.push_back(SketchPoint{xs[i], level});
            segments.push_back(SketchPoint{xs[i + 1], level});
        }
    }
    return segments;
}

}  // namespace forgeshape
