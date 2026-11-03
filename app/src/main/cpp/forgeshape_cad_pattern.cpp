#include "forgeshape_cad_pattern.h"

#include <algorithm>
#include <cmath>

#include "forgeshape_cad_feature.h"
#include "forgeshape_workplane.h"

namespace forgeshape {

namespace {

constexpr double kPi = 3.14159265358979323846;

void unitAxis(CadBodyAxis axis, double sign, double out[3]) {
    out[0] = out[1] = out[2] = 0.0;
    out[cadBodyAxisIndex(axis)] = sign;
}

bool finite3(const double v[3]) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

bool inSpacing(double d) {
    return std::isfinite(d) && d >= kMinPatternSpacing && d <= kMaxPatternSpacing;
}

// The fields a kind does not use must be exactly their defaults: one pattern,
// one encoding.
bool secondDirectionEmpty(const CadFeaturePattern& p) {
    return p.count2 == 1u && p.axis2 == CadBodyAxis::Y && !p.reversed2 && p.spacing2 == 0.0;
}

bool centerZero(const CadFeaturePattern& p) {
    return p.center[0] == 0.0 && p.center[1] == 0.0 && p.center[2] == 0.0;
}

CadStatus validateOwnValues(const CadFeaturePattern& p) {
    switch (p.kind) {
        case CadPatternKind::Linear: {
            if (p.count < kMinPatternCount || p.count > kMaxPatternInstances
                || !inSpacing(p.spacing) || p.angleDegrees != 0.0 || !centerZero(p)
                || p.offset != 0.0) {
                return CadStatus::PatternInvalid;
            }
            if (p.count2 == 1u) {
                if (!secondDirectionEmpty(p)) return CadStatus::PatternInvalid;
            } else if (p.count2 < kMinPatternCount || p.count2 > kMaxPatternInstances
                       || !inSpacing(p.spacing2) || p.axis2 == p.axis) {
                return CadStatus::PatternInvalid;
            }
            if (p.count * p.count2 > kMaxPatternInstances) {
                return CadStatus::PatternInvalid;
            }
            return CadStatus::Ok;
        }
        case CadPatternKind::Circular: {
            if (p.count < kMinPatternCount || p.count > kMaxPatternInstances
                || !std::isfinite(p.angleDegrees) || p.angleDegrees <= 0.0
                || p.angleDegrees > 360.0 || !finite3(p.center)
                || std::fabs(p.center[0]) > kMaxPatternOffset
                || std::fabs(p.center[1]) > kMaxPatternOffset
                || std::fabs(p.center[2]) > kMaxPatternOffset || p.spacing != 0.0
                || !secondDirectionEmpty(p) || p.offset != 0.0) {
                return CadStatus::PatternInvalid;
            }
            return CadStatus::Ok;
        }
        case CadPatternKind::Mirror: {
            if (p.count != 2u || p.reversed || p.spacing != 0.0 || !secondDirectionEmpty(p)
                || p.angleDegrees != 0.0 || !centerZero(p) || !std::isfinite(p.offset)
                || std::fabs(p.offset) > kMaxPatternOffset) {
                return CadStatus::PatternInvalid;
            }
            return CadStatus::Ok;
        }
    }
    return CadStatus::PatternInvalid;
}

// The 1/2/5 x 10^k value at or above `v`.
double niceAtLeast(double v) {
    if (!(v > 0.0) || !std::isfinite(v)) return 0.0;
    const double decade = std::pow(10.0, std::floor(std::log10(v)));
    for (double m : {1.0, 2.0, 5.0, 10.0}) {
        if (m * decade >= v * (1.0 - 1e-9)) return m * decade;
    }
    return 10.0 * decade;
}

bool featureBounds(const CadBodyState& state, uint32_t featureId, double lo[3], double hi[3]) {
    CadFeatureGeometry g;
    CadSolid solid;
    if (buildCadFeatureGeometry(state, featureId, &g) != CadStatus::Ok
        || appendCadFeatureSolid(g, 0u, &solid) != CadStatus::Ok || solid.positions.empty()) {
        return false;
    }
    for (int a = 0; a < 3; ++a) {
        lo[a] = INFINITY;
        hi[a] = -INFINITY;
    }
    for (size_t i = 0; i + 2 < solid.positions.size(); i += 3) {
        for (int a = 0; a < 3; ++a) {
            lo[a] = std::min(lo[a], solid.positions[i + a]);
            hi[a] = std::max(hi[a], solid.positions[i + a]);
        }
    }
    return true;
}

// The shared shape of the three acts (the edge finishes' own, restated).
template <typename Change>
CadStatus applyPatternAct(ConstructionScene& scene, ConstructionHistory& history, ObjectId bodyId,
                          Change change) {
    if (history.editInProgress()) {
        return CadStatus::RefusedEditInProgress;
    }
    SceneObject* object = scene.findBody(bodyId);
    CadBody* body = object != nullptr ? object->cadOrNull() : nullptr;
    if (body == nullptr) {
        return CadStatus::NotCadBody;
    }
    CadBodyState next = body->state();
    const CadStatus built = change(&next);
    if (built != CadStatus::Ok) {
        return built;
    }
    const CadStatus valid = validateCadBodyState(next);
    if (valid != CadStatus::Ok) {
        return valid;
    }
    CadStatus why = CadStatus::Ok;
    {
        ScopedConstructionEdit edit(history);
        why = body->applyState(next);
        if (why == CadStatus::Ok) {
            publishSceneObject(*object);
        }
    }
    return why;
}

}  // namespace

std::vector<CadPatternTransform> cadPatternInstanceTransforms(const CadFeaturePattern& pattern) {
    std::vector<CadPatternTransform> out;
    if (validateOwnValues(pattern) != CadStatus::Ok) {
        return out;
    }
    switch (pattern.kind) {
        case CadPatternKind::Linear: {
            double d1[3];
            double d2[3];
            unitAxis(pattern.axis, pattern.reversed ? -1.0 : 1.0, d1);
            unitAxis(pattern.axis2, pattern.reversed2 ? -1.0 : 1.0, d2);
            for (uint32_t j = 0; j < pattern.count2; ++j) {
                for (uint32_t i = 0; i < pattern.count; ++i) {
                    if (i == 0u && j == 0u) continue;
                    CadPatternTransform t;
                    for (int a = 0; a < 3; ++a) {
                        t.t[a] = d1[a] * pattern.spacing * i + d2[a] * pattern.spacing2 * j;
                    }
                    out.push_back(t);
                }
            }
            break;
        }
        case CadPatternKind::Circular: {
            // A full turn spaces the instances evenly; less spans first to last.
            const bool full = pattern.angleDegrees >= 360.0;
            const double step = full ? 360.0 / pattern.count
                                     : pattern.angleDegrees / (pattern.count - 1u);
            const double sense = pattern.reversed ? -1.0 : 1.0;
            const int k = cadBodyAxisIndex(pattern.axis);
            const int a1 = (k + 1) % 3;
            const int a2 = (k + 2) % 3;
            for (uint32_t i = 1; i < pattern.count; ++i) {
                const double theta = sense * step * i * kPi / 180.0;
                const double c = std::cos(theta);
                const double s = std::sin(theta);
                CadPatternTransform t;
                // Right-handed rotation about +axis: (a1, a2) turn by theta.
                double r[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
                r[k][k] = 1.0;
                r[a1][a1] = c;
                r[a1][a2] = -s;
                r[a2][a1] = s;
                r[a2][a2] = c;
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) t.r[row * 3 + col] = r[row][col];
                }
                // About the axis through `center`: p' = R (p - c) + c.
                for (int row = 0; row < 3; ++row) {
                    double rc = 0.0;
                    for (int col = 0; col < 3; ++col) rc += r[row][col] * pattern.center[col];
                    t.t[row] = pattern.center[row] - rc;
                }
                out.push_back(t);
            }
            break;
        }
        case CadPatternKind::Mirror: {
            const int k = cadBodyAxisIndex(pattern.axis);
            CadPatternTransform t;
            t.r[k * 3 + k] = -1.0;
            t.t[k] = 2.0 * pattern.offset;
            t.reflects = true;
            out.push_back(t);
            break;
        }
    }
    return out;
}

void transformCadSolid(const CadPatternTransform& transform, CadSolid* solid) {
    if (solid == nullptr) return;
    for (size_t i = 0; i + 2 < solid->positions.size(); i += 3) {
        const double p[3] = {solid->positions[i], solid->positions[i + 1],
                             solid->positions[i + 2]};
        for (int row = 0; row < 3; ++row) {
            solid->positions[i + row] = transform.r[row * 3] * p[0]
                                        + transform.r[row * 3 + 1] * p[1]
                                        + transform.r[row * 3 + 2] * p[2] + transform.t[row];
        }
    }
    if (transform.reflects) {
        // A reflection turns every triangle inside out; swapping two corners
        // turns it back, so the moved solid still winds outward.
        for (size_t t = 0; t + 2 < solid->indices.size(); t += 3) {
            std::swap(solid->indices[t + 1], solid->indices[t + 2]);
        }
    }
}

CadStatus validateCadFeaturePatterns(const CadBodyState& state, uint32_t* outFailedId) {
    auto fail = [outFailedId](CadStatus why, uint32_t id) {
        if (outFailedId != nullptr) *outFailedId = id;
        return why;
    };
    if (state.patterns.empty()) {
        return CadStatus::Ok;
    }
    if (state.patterns.size() > kMaxCadFeaturePatterns) {
        return fail(CadStatus::TooManyPatterns, state.patterns.back().patternId);
    }
    uint32_t previous = kCadFeatureId;
    for (const CadFeaturePattern& p : state.patterns) {
        // Ids: ascending, in the shared id space, below the high-water mark,
        // and never a feature's or a finish's.
        if (p.patternId <= previous || p.patternId >= state.nextFeatureId) {
            return fail(CadStatus::PatternInvalid, p.patternId);
        }
        previous = p.patternId;
        for (const CadFeature& f : state.laterFeatures) {
            if (f.featureId == p.patternId) return fail(CadStatus::PatternInvalid, p.patternId);
        }
        for (const CadEdgeFinish& f : state.edgeFinishes) {
            if (f.finishId == p.patternId) return fail(CadStatus::PatternInvalid, p.patternId);
        }
        const CadStatus own = validateOwnValues(p);
        if (own != CadStatus::Ok) {
            return fail(own, p.patternId);
        }
        if (p.featureIds.empty() || p.featureIds.size() > kMaxFeaturesPerPattern) {
            return fail(CadStatus::PatternInvalid, p.patternId);
        }
        uint32_t last = 0;
        for (uint32_t id : p.featureIds) {
            if (id <= last) return fail(CadStatus::PatternInvalid, p.patternId);
            last = id;
            if (id == kCadFeatureId || state.baseKind == CadFeatureKind::Revolve) {
                return fail(CadStatus::PatternTargetUnsupported, p.patternId);
            }
            bool found = false;
            for (const CadFeature& f : state.laterFeatures) {
                found = found || (f.featureId == id && id < p.patternId);
                if (f.featureId == id && f.kind == CadFeatureKind::Revolve) {
                    return fail(CadStatus::PatternTargetUnsupported, p.patternId);
                }
            }
            if (!found) return fail(CadStatus::PatternFeatureNotFound, p.patternId);
        }
    }
    return CadStatus::Ok;
}

double suggestCadPatternSpacing(const CadBodyState& state, const std::vector<uint32_t>& featureIds,
                                CadBodyAxis axis) {
    if (featureIds.empty()) return 0.0;
    double lo[3];
    double hi[3];
    if (!featureBounds(state, featureIds.front(), lo, hi)) return 0.0;
    const int a = cadBodyAxisIndex(axis);
    const double extent = hi[a] - lo[a];
    // Along an axis the feature is flat on (its own extrusion direction seen
    // edge-on), fall back to its largest extent.
    double size = extent;
    if (size < kMinPatternSpacing) {
        for (int k = 0; k < 3; ++k) size = std::max(size, hi[k] - lo[k]);
    }
    return std::min(niceAtLeast(size * 1.25), kMaxPatternSpacing);
}

bool suggestCadPatternCircularAxis(const CadBodyState& state, CadBodyAxis* outAxis,
                                   double outCenter[3]) {
    if (outAxis == nullptr || outCenter == nullptr) return false;
    double lo[3];
    double hi[3];
    if (!featureBounds(state, kCadFeatureId, lo, hi)) return false;
    // The base sketch's normal in body-local space: a face-supported base
    // authors on its own XY, a world-plane one on its workplane.
    const CadSketch& sketch = cadBaseSketch(state);
    const Vec3 n = sketch.hasFaceSupport ? Vec3{0.0f, 0.0f, 1.0f}
                                         : workplaneFrame(sketch.plane).normal;
    int best = 0;
    const float comps[3] = {std::fabs(n.x), std::fabs(n.y), std::fabs(n.z)};
    for (int k = 1; k < 3; ++k) {
        if (comps[k] > comps[best]) best = k;
    }
    cadBodyAxisFromIndex(best, outAxis);
    for (int k = 0; k < 3; ++k) outCenter[k] = 0.5 * (lo[k] + hi[k]);
    outCenter[best] = 0.0;
    return true;
}

bool cadBodyLocalCenter(const CadBodyState& state, double out[3]) {
    double lo[3];
    double hi[3];
    if (out == nullptr || !featureBounds(state, kCadFeatureId, lo, hi)) return false;
    for (int k = 0; k < 3; ++k) out[k] = 0.5 * (lo[k] + hi[k]);
    return true;
}

CadBodyAxis suggestCadPatternLinearAxis(const CadBodyState& state, uint32_t featureId) {
    CadFeatureGeometry g;
    if (buildCadFeatureGeometry(state, featureId, &g) != CadStatus::Ok) {
        return CadBodyAxis::X;
    }
    const double n[3] = {std::fabs(g.placement.n.x), std::fabs(g.placement.n.y),
                         std::fabs(g.placement.n.z)};
    int normal = 0;
    for (int k = 1; k < 3; ++k) {
        if (n[k] > n[normal]) normal = k;
    }
    CadBodyAxis axis = CadBodyAxis::X;
    cadBodyAxisFromIndex(normal == 0 ? 1 : 0, &axis);
    return axis;
}

void suggestCadPatternLinearRun(const CadBodyState& state, uint32_t featureId, CadBodyAxis axis,
                                double spacing, uint32_t maxCount, uint32_t* outCount,
                                bool* outReversed) {
    uint32_t count = kMinPatternCount;
    bool reversed = false;
    double baseLo[3];
    double baseHi[3];
    double lo[3];
    double hi[3];
    const int a = cadBodyAxisIndex(axis);
    if (spacing > 0.0 && featureBounds(state, kCadFeatureId, baseLo, baseHi)
        && featureBounds(state, featureId, lo, hi)) {
        // Copies whose whole footprint stays inside the part's span along the
        // axis: never a boss hanging off the edge or a pocket in thin air.
        const double roomPlus = baseHi[a] - hi[a];
        const double roomMinus = lo[a] - baseLo[a];
        const double room = std::max(roomPlus, roomMinus);
        reversed = roomMinus > roomPlus;
        const double fit = std::floor(room / spacing + 1e-9) + 1.0;
        count = static_cast<uint32_t>(std::max(static_cast<double>(kMinPatternCount),
                                               std::min(static_cast<double>(maxCount), fit)));
    }
    if (outCount != nullptr) *outCount = count;
    if (outReversed != nullptr) *outReversed = reversed;
}

CadStatus addCadFeaturePatternToBody(ConstructionScene& scene, ConstructionHistory& history,
                                     ObjectId bodyId, CadFeaturePattern pattern,
                                     uint32_t* outPatternId) {
    uint32_t minted = 0;
    const CadStatus why = applyPatternAct(scene, history, bodyId, [&](CadBodyState* next) {
        if (next->patterns.size() >= kMaxCadFeaturePatterns) {
            return CadStatus::TooManyPatterns;
        }
        minted = appendCadFeaturePattern(next, std::move(pattern));
        return minted != 0 ? CadStatus::Ok : CadStatus::PatternInvalid;
    });
    if (why == CadStatus::Ok && outPatternId != nullptr) {
        *outPatternId = minted;
    }
    return why;
}

CadStatus editCadFeaturePatternOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                      ObjectId bodyId, uint32_t patternId,
                                      CadFeaturePattern pattern) {
    return applyPatternAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (CadFeaturePattern& existing : next->patterns) {
            if (existing.patternId == patternId) {
                pattern.patternId = patternId;
                existing = std::move(pattern);
                return CadStatus::Ok;
            }
        }
        return CadStatus::PatternFeatureNotFound;
    });
}

CadStatus deleteCadFeaturePatternOfBody(ConstructionScene& scene, ConstructionHistory& history,
                                        ObjectId bodyId, uint32_t patternId) {
    return applyPatternAct(scene, history, bodyId, [&](CadBodyState* next) {
        for (size_t i = 0; i < next->patterns.size(); ++i) {
            if (next->patterns[i].patternId == patternId) {
                next->patterns.erase(next->patterns.begin() + static_cast<long>(i));
                return CadStatus::Ok;
            }
        }
        return CadStatus::PatternFeatureNotFound;
    });
}

}  // namespace forgeshape
