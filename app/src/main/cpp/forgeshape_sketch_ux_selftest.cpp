#include "forgeshape_sketch_ux_selftest.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "forgeshape_cad_body.h"
#include "forgeshape_cad_face.h"
#include "forgeshape_history.h"
#include "forgeshape_math.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_selftest.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sketch.h"
#include "forgeshape_sketch_session.h"

namespace forgeshape {
namespace {

struct Recorder {
    SketchUxSelfTestResult* out;
    int max;
    int n = 0;
    void check(const char* name, bool ok) {
        if (n < max) {
            out[n].name = name;
            out[n].passed = ok;
            ++n;
        }
    }
};

std::string g_performance = "not measured";

bool near2(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }
bool nearPoint(const SketchPoint& p, double u, double v, double tol = 1e-9) {
    return near2(p.u, u, tol) && near2(p.v, v, tol);
}
bool nearVec(const Vec3& a, float x, float y, float z, float tol = 1e-5f) {
    return std::fabs(a.x - x) <= tol && std::fabs(a.y - y) <= tol && std::fabs(a.z - z) <= tol;
}

// A right-handed orthonormal check: u x v == n, all three unit.
bool rightHanded(const SketchFrame& f) {
    const Vec3 cross = vec3Cross(f.u, f.v);
    return nearVec(cross, f.n.x, f.n.y, f.n.z, 1e-4f)
           && std::fabs(vec3Dot(f.u, f.u) - 1.0f) <= 1e-4f
           && std::fabs(vec3Dot(f.v, f.v) - 1.0f) <= 1e-4f
           && std::fabs(vec3Dot(f.n, f.n) - 1.0f) <= 1e-4f;
}

SketchEntity arcEntity(SketchEntityId id, SketchPoint start, SketchPoint mid, SketchPoint end) {
    SketchArc arc;
    arc.start = start;
    arc.mid = mid;
    arc.end = end;
    return SketchEntity(id, arc);
}

SketchEntity splineEntity(SketchEntityId id, std::vector<SketchPoint> points) {
    SketchSpline spline;
    spline.points = std::move(points);
    return SketchEntity(id, spline);
}

// The canonical semicircle used throughout: centre (0,0), radius 1, from
// (1,0) through (0,1) to (-1,0). Counter-clockwise, sweep +pi.
SketchArc unitSemicircle() {
    SketchArc arc;
    arc.start = SketchPoint{1.0, 0.0};
    arc.mid = SketchPoint{0.0, 1.0};
    arc.end = SketchPoint{-1.0, 0.0};
    return arc;
}

// A closed profile made of one semicircular arc and the straight line that
// joins its two ends: a "D" shape. Exactly the mixed line/curve chain the
// stage has to extrude.
CadBodyState arcAndLineBody(double depth = 1.0) {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    addSketchEntity(&state.sketch, unitSemicircle());
    addSketchEntity(&state.sketch, SketchLine{SketchPoint{-1.0, 0.0}, SketchPoint{1.0, 0.0}});
    state.extrude.profileEntityId = 1;  // the chain's smallest member id
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// A closed profile made of one spline and one line joining its ends.
//
// Every coordinate is an EXACT binary fraction, because this body is also a
// corpus fixture: 0.75 is representable to the last bit and 0.8 is not, and a
// value the two encoders could round differently is a byte-parity argument
// waiting to happen.
CadBodyState splineAndLineBody() {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    SketchSpline spline;
    spline.points = {SketchPoint{-1.0, 0.0}, SketchPoint{-0.5, 0.75}, SketchPoint{0.5, 0.75},
                     SketchPoint{1.0, 0.0}};
    addSketchEntity(&state.sketch, spline);
    addSketchEntity(&state.sketch, SketchLine{SketchPoint{1.0, 0.0}, SketchPoint{-1.0, 0.0}});
    state.extrude.profileEntityId = 1;
    state.extrude.depth = 0.5;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// A plain rectangle body, for the edit-session and dependency cases.
CadBodyState rectBody(double w, double h, double depth) {
    CadBodyState state;
    state.sketch.plane = Workplane::XY;
    SketchRectangle r;
    r.center = SketchPoint{0.0, 0.0};
    r.width = w;
    r.height = h;
    addSketchEntity(&state.sketch, r);
    state.extrude.profileEntityId = 1;
    state.extrude.depth = depth;
    state.extrude.direction = ExtrudeDirection::AlongNormal;
    return state;
}

// -------------------------------------------------------------------------
// The curve domain (`CADUXR1-25..31`)
// -------------------------------------------------------------------------

void testArcDomain(Recorder& r) {
    // The derived circle is exact for a semicircle on the unit circle.
    {
        SketchPoint centre;
        double radius = 0.0;
        double startAngle = 0.0;
        double sweep = 0.0;
        const CadStatus why = arcGeometry(unitSemicircle(), &centre, &radius, &startAngle, &sweep);
        r.check("CADUXR1_25_arc_geometry_is_the_circle_through_its_three_points",
                why == CadStatus::Ok && nearPoint(centre, 0.0, 0.0, 1e-12)
                        && near2(radius, 1.0, 1e-12) && near2(startAngle, 0.0, 1e-12)
                        && near2(sweep, 3.14159265358979323846, 1e-12));
    }
    // The other way round the circle: the middle point alone decides, and the
    // sweep comes back negative rather than as the same arc's mirror.
    {
        SketchArc arc;
        arc.start = SketchPoint{1.0, 0.0};
        arc.mid = SketchPoint{0.0, -1.0};
        arc.end = SketchPoint{-1.0, 0.0};
        double sweep = 0.0;
        const CadStatus why = arcGeometry(arc, nullptr, nullptr, nullptr, &sweep);
        r.check("CADUXR1_25_the_middle_point_decides_which_arc_was_drawn",
                why == CadStatus::Ok && sweep < 0.0
                        && near2(sweep, -3.14159265358979323846, 1e-12));
    }
    // Collinear and coincident are refused BY NAME and never straightened.
    {
        const SketchEntity flat = arcEntity(1, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 0.0},
                                            SketchPoint{2.0, 0.0});
        const SketchEntity doubled = arcEntity(1, SketchPoint{0.0, 0.0}, SketchPoint{0.0, 0.0},
                                               SketchPoint{2.0, 1.0});
        r.check("CADUXR1_25_a_collinear_or_degenerate_arc_is_refused_by_name",
                validateSketchEntity(flat) == CadStatus::InvalidArc
                        && validateSketchEntity(doubled) == CadStatus::InvalidArc);
    }
    // A far-from-origin collinear triple is refused too: the test is scaled by
    // the triangle's own extent, not an absolute epsilon.
    {
        const SketchEntity far = arcEntity(1, SketchPoint{1000.0, 1000.0},
                                           SketchPoint{1001.0, 1000.0},
                                           SketchPoint{1002.0, 1000.0});
        r.check("CADUXR1_25_collinearity_is_scaled_not_absolute",
                validateSketchEntity(far) == CadStatus::InvalidArc);
    }
    // The tessellation: exact endpoints, on the circle, bounded, deterministic.
    {
        const SketchEntity arc(1, unitSemicircle());
        std::vector<SketchPoint> a;
        std::vector<SketchPoint> b;
        const bool ok = tessellateSketchCurve(arc, &a) == CadStatus::Ok
                        && tessellateSketchCurve(arc, &b) == CadStatus::Ok;
        bool identical = ok && a.size() == b.size();
        for (size_t i = 0; identical && i < a.size(); ++i) {
            identical = a[i].u == b[i].u && a[i].v == b[i].v;
        }
        bool onCircle = ok;
        for (size_t i = 0; onCircle && i < a.size(); ++i) {
            onCircle = near2(std::sqrt(a[i].u * a[i].u + a[i].v * a[i].v), 1.0, 1e-12);
        }
        r.check("CADUXR1_31_arc_tessellation_is_exact_bounded_and_deterministic",
                ok && identical && onCircle && a.size() >= kMinArcSegments + 1
                        && a.size() <= kMaxArcSegments + 1
                        && nearPoint(a.front(), 1.0, 0.0, 0.0)
                        && nearPoint(a.back(), -1.0, 0.0, 0.0));
    }
    // Half the sweep, about half the segments: the density is the circle's.
    {
        SketchArc quarter;
        quarter.start = SketchPoint{1.0, 0.0};
        quarter.mid = SketchPoint{0.70710678118654752, 0.70710678118654752};
        quarter.end = SketchPoint{0.0, 1.0};
        std::vector<SketchPoint> q;
        std::vector<SketchPoint> h;
        tessellateSketchCurve(SketchEntity(1, quarter), &q);
        tessellateSketchCurve(SketchEntity(1, unitSemicircle()), &h);
        r.check("CADUXR1_31_arc_segment_count_follows_the_swept_angle",
                q.size() < h.size() && q.size() >= kMinArcSegments + 1);
    }
    // The endpoints a chain joins on are the AUTHORED values, bit for bit.
    {
        const SketchEntity arc(1, unitSemicircle());
        SketchPoint start;
        SketchPoint end;
        r.check("CADUXR1_27_an_arc_chains_on_its_authored_endpoints",
                sketchEntityEndpoints(arc, &start, &end) && start.u == 1.0 && start.v == 0.0
                        && end.u == -1.0 && end.v == 0.0 && sketchEntityIsCurved(arc));
    }
}

void testSplineDomain(Recorder& r) {
    // Too few points, a repeated point and a closed run are all refused.
    {
        const SketchEntity one = splineEntity(1, {SketchPoint{0.0, 0.0}});
        const SketchEntity dup = splineEntity(
                1, {SketchPoint{0.0, 0.0}, SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}});
        const SketchEntity closed = splineEntity(
                1, {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}, SketchPoint{0.0, 0.0}});
        r.check("CADUXR1_28_a_spline_needs_two_distinct_points_and_two_ends",
                validateSketchEntity(one) == CadStatus::InvalidSpline
                        && validateSketchEntity(dup) == CadStatus::InvalidSpline
                        && validateSketchEntity(closed) == CadStatus::InvalidSpline);
    }
    // The point cap is a bound, not a suggestion.
    {
        std::vector<SketchPoint> many;
        for (uint32_t i = 0; i <= kMaxSplinePoints; ++i) {
            many.push_back(SketchPoint{static_cast<double>(i), 0.5 * static_cast<double>(i % 3)});
        }
        r.check("CADUXR1_28_a_spline_past_its_point_cap_is_refused",
                validateSketchEntity(splineEntity(1, many)) == CadStatus::TooManyEntities);
    }
    // The curve PASSES THROUGH every authored point, which is the whole reason
    // for choosing an interpolating spline: moving a point moves the curve
    // there. Deterministic and bounded with it.
    {
        const std::vector<SketchPoint> pts = {SketchPoint{0.0, 0.0}, SketchPoint{1.0, 2.0},
                                              SketchPoint{2.0, -1.0}, SketchPoint{3.0, 0.5}};
        const SketchEntity spline = splineEntity(1, pts);
        std::vector<SketchPoint> a;
        std::vector<SketchPoint> b;
        const bool ok = tessellateSketchCurve(spline, &a) == CadStatus::Ok
                        && tessellateSketchCurve(spline, &b) == CadStatus::Ok;
        bool identical = ok && a.size() == b.size();
        for (size_t i = 0; identical && i < a.size(); ++i) {
            identical = a[i].u == b[i].u && a[i].v == b[i].v;
        }
        bool interpolates = ok;
        for (size_t i = 0; interpolates && i < pts.size(); ++i) {
            const size_t at = i * kSplineSegmentsPerSpan;
            interpolates = at < a.size() && a[at].u == pts[i].u && a[at].v == pts[i].v;
        }
        r.check("CADUXR1_29_spline_tessellation_interpolates_and_is_deterministic",
                ok && identical && interpolates
                        && a.size() == (pts.size() - 1) * kSplineSegmentsPerSpan + 1);
    }
    {
        const SketchEntity spline = splineEntity(
                1, {SketchPoint{-1.0, 0.0}, SketchPoint{0.0, 1.0}, SketchPoint{1.0, 0.0}});
        SketchPoint start;
        SketchPoint end;
        r.check("CADUXR1_30_a_spline_chains_on_its_first_and_last_authored_points",
                sketchEntityEndpoints(spline, &start, &end) && start.u == -1.0 && end.u == 1.0
                        && sketchEntityIsCurved(spline));
    }
}

void testCurveProfiles(Recorder& r) {
    // An arc and a line closing one profile, and it extrudes.
    {
        const CadBodyState state = arcAndLineBody();
        const ProfileExtraction extraction = extractClosedProfiles(state.sketch);
        const ClosedProfile* profile = extraction.profiles.empty()
                                           ? nullptr
                                           : &extraction.profiles.front();
        ConstructionMesh mesh;
        const CadStatus generated = generateCadMesh(state, &mesh);
        // Half a unit circle: pi/2, and the closing chord contributes nothing.
        const bool area = profile != nullptr
                          && std::fabs(profile->area - 1.5707963267948966) < 0.02;
        r.check("CADUXR1_27_an_arc_and_a_line_close_one_profile_and_extrude",
                extraction.profiles.size() == 1 && profile != nullptr && area
                        && profile->memberEntityIds.size() == 2 && generated == CadStatus::Ok
                        && mesh.vertices.size() > 0);
    }
    // A spline and a line likewise.
    {
        const CadBodyState state = splineAndLineBody();
        const ProfileExtraction extraction = extractClosedProfiles(state.sketch);
        ConstructionMesh mesh;
        r.check("CADUXR1_30_a_spline_and_a_line_close_one_profile_and_extrude",
                extraction.profiles.size() == 1
                        && extraction.profiles.front().memberEntityIds.size() == 2
                        && generateCadMesh(state, &mesh) == CadStatus::Ok);
    }
    // An arc whose free end meets nothing is an OPEN profile, not a repaired
    // one -- exactly what a lone line already is.
    {
        CadSketch sketch;
        addSketchEntity(&sketch, unitSemicircle());
        const ProfileExtraction extraction = extractClosedProfiles(sketch);
        r.check("CADUXR1_27_a_lone_arc_is_an_open_profile_not_a_closed_one",
                extraction.profiles.empty() && extraction.rejections.size() == 1
                        && extraction.rejections.front().why == CadStatus::OpenProfile);
    }
    // The mixed profile's SIDE faces: the line's is planar and eligible to
    // carry a sketch; every arc facet is reported so a tap resolves, and none
    // of them is eligible.
    {
        const CadBodyState state = arcAndLineBody();
        std::vector<CadFace> faces;
        const CadStatus why = enumerateCadFaces(state, &faces);
        int eligibleSides = 0;
        int curvedSides = 0;
        for (const CadFace& face : faces) {
            if (face.token.kind != CadFaceKind::Side) continue;
            if (face.eligible) {
                ++eligibleSides;
            } else {
                ++curvedSides;
            }
        }
        bool capsEligible = faces.size() >= 2 && faces[0].eligible && faces[1].eligible;
        r.check("CADUXR1_27_a_curved_side_is_never_eligible_and_a_line_side_is",
                why == CadStatus::Ok && capsEligible && eligibleSides == 1 && curvedSides > 1);
    }
    // Determinism at the level that matters: the same authored state produces
    // the same polygon, twice, value for value.
    {
        const CadBodyState state = arcAndLineBody();
        const ProfileExtraction a = extractClosedProfiles(state.sketch);
        const ProfileExtraction b = extractClosedProfiles(state.sketch);
        bool same = a.profiles.size() == 1 && b.profiles.size() == 1
                    && a.profiles[0].polygon.size() == b.profiles[0].polygon.size();
        for (size_t i = 0; same && i < a.profiles[0].polygon.size(); ++i) {
            same = a.profiles[0].polygon[i].u == b.profiles[0].polygon[i].u
                   && a.profiles[0].polygon[i].v == b.profiles[0].polygon[i].v;
        }
        r.check("CADUXR1_31_a_curve_profile_is_bit_identical_between_runs", same);
    }
    // The bound holds: a chain long enough to exceed kMaxProfileVertices is
    // refused rather than triangulated.
    {
        CadSketch sketch;
        // Splines at the point cap, chained end to end. Each contributes
        // (points - 1) * segments edges, so a few of them pass the ceiling.
        double x = 0.0;
        for (int i = 0; i < 8; ++i) {
            std::vector<SketchPoint> pts;
            for (uint32_t k = 0; k < kMaxSplinePoints; ++k) {
                pts.push_back(SketchPoint{x + static_cast<double>(k) * 0.01,
                                          (k % 2 == 0) ? 0.0 : 0.005});
            }
            x = pts.back().u;
            addSketchEntity(&sketch, splineEntity(1, pts).spline() != nullptr
                                             ? SketchEntity::Payload(SketchSpline{pts})
                                             : SketchEntity::Payload(SketchSpline{pts}));
        }
        // Close the run with one line back to the start.
        addSketchEntity(&sketch, SketchLine{SketchPoint{x, 0.0}, SketchPoint{0.0, 0.0}});
        const ProfileExtraction extraction = extractClosedProfiles(sketch);
        bool boundedRefusal = extraction.profiles.empty();
        for (const ClosedProfile& profile : extraction.profiles) {
            boundedRefusal = boundedRefusal || profile.polygon.size() <= kMaxProfileVertices;
        }
        r.check("CADUXR1_31_a_curve_chain_past_the_polygon_bound_is_refused_not_grown",
                boundedRefusal && !extraction.rejections.empty());
    }
}

// -------------------------------------------------------------------------
// The exact line-length edit (`CADUXR1-17..24`)
// -------------------------------------------------------------------------

void testLineDimension(Recorder& r) {
    SketchSession session;
    session.begin(Workplane::XY);
    SketchEntityId lineId = kNoSketchEntity;
    addSketchEntity(const_cast<CadSketch*>(&session.sketch()),
                    SketchLine{SketchPoint{1.0, 1.0}, SketchPoint{4.0, 5.0}}, &lineId);
    session.select(lineId);

    // A 3-4-5 triangle: the reported length is exactly 5.
    {
        Meters length = 0.0;
        r.check("CADUXR1_18_a_selected_line_reports_its_exact_length",
                session.selectedLineLength(&length) && near2(length, 5.0, 1e-12));
    }
    // The edit: P0 fixed, direction preserved, P1 exactly `length` away.
    {
        const CadStatus why = session.applyLineLength(lineId, 10.0);
        const SketchLine* line = findSketchEntity(session.sketch(), lineId)->line();
        const double du = line->end.u - line->start.u;
        const double dv = line->end.v - line->start.v;
        r.check("CADUXR1_20_21_22_a_typed_length_keeps_P0_and_the_direction",
                why == CadStatus::Ok && nearPoint(line->start, 1.0, 1.0, 0.0)
                        && near2(std::sqrt(du * du + dv * dv), 10.0, 1e-12)
                        // 3-4-5 doubled twice over: the direction is unchanged.
                        && near2(du / dv, 3.0 / 4.0, 1e-12) && du > 0.0 && dv > 0.0);
    }
    // Every invalid length is refused and mutates nothing.
    {
        const SketchLine before = *findSketchEntity(session.sketch(), lineId)->line();
        const CadStatus zero = session.applyLineLength(lineId, 0.0);
        const CadStatus negative = session.applyLineLength(lineId, -3.0);
        const CadStatus nan = session.applyLineLength(lineId,
                                                      std::numeric_limits<double>::quiet_NaN());
        const CadStatus infinite = session.applyLineLength(
                lineId, std::numeric_limits<double>::infinity());
        const CadStatus huge = session.applyLineLength(lineId, kMaxSketchCoordinateMeters * 10.0);
        const SketchLine after = *findSketchEntity(session.sketch(), lineId)->line();
        r.check("CADUXR1_23_an_invalid_typed_length_is_refused_and_changes_nothing",
                zero != CadStatus::Ok && negative != CadStatus::Ok && nan == CadStatus::NonFinite
                        && infinite == CadStatus::NonFinite && huge != CadStatus::Ok
                        && after.start.u == before.start.u && after.start.v == before.start.v
                        && after.end.u == before.end.u && after.end.v == before.end.v);
    }
    // Only a straight Line has a length to show.
    {
        SketchSession other;
        other.begin(Workplane::XY);
        SketchEntityId circleId = kNoSketchEntity;
        SketchCircle circle;
        circle.center = SketchPoint{0.0, 0.0};
        circle.radius = 1.0;
        addSketchEntity(const_cast<CadSketch*>(&other.sketch()), circle, &circleId);
        other.select(circleId);
        Meters length = 0.0;
        r.check("CADUXR1_17_only_a_straight_line_carries_a_dimension",
                !other.selectedLineLength(&length)
                        && other.applyLineLength(circleId, 2.0) == CadStatus::UnknownEntity);
    }
    // The label anchor stands OFF the stroke, by the annotation offset.
    {
        SketchPoint anchor;
        const double unit = 0.01;
        const bool ok = session.selectedLineDimensionAnchor(unit, &anchor);
        const SketchLine* line = findSketchEntity(session.sketch(), lineId)->line();
        const SketchPoint mid{(line->start.u + line->end.u) * 0.5,
                              (line->start.v + line->end.v) * 0.5};
        const double away = std::sqrt((anchor.u - mid.u) * (anchor.u - mid.u)
                                      + (anchor.v - mid.v) * (anchor.v - mid.v));
        r.check("CADUXR1_17_the_dimension_label_stands_clear_of_the_stroke",
                ok && near2(away, kSketchDimensionOffsetUnits * unit, 1e-9));
    }
    // A length edit that opens a chain is allowed as an EDIT STATE, and the
    // profile is then refused by name rather than repaired.
    {
        SketchSession chain;
        chain.begin(Workplane::XY);
        CadSketch* sketch = const_cast<CadSketch*>(&chain.sketch());
        SketchEntityId first = kNoSketchEntity;
        addSketchEntity(sketch, SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{2.0, 0.0}}, &first);
        addSketchEntity(sketch, SketchLine{SketchPoint{2.0, 0.0}, SketchPoint{1.0, 2.0}});
        addSketchEntity(sketch, SketchLine{SketchPoint{1.0, 2.0}, SketchPoint{0.0, 0.0}});
        const bool closedBefore = extractClosedProfiles(chain.sketch()).profiles.size() == 1;
        const CadStatus edited = chain.applyLineLength(first, 1.0);
        const CadStatus finished = chain.finish();
        r.check("CADUXR1_24_a_length_edit_may_open_a_profile_and_extrude_then_refuses",
                closedBefore && edited == CadStatus::Ok && finished == CadStatus::OpenProfile
                        && chain.state() == SketchSessionState::Editing);
    }
}

// -------------------------------------------------------------------------
// The orientation navigator (`CADUXR1-08..16`)
// -------------------------------------------------------------------------

void testOrientationNavigator(Recorder& r) {
    SketchSession session;
    session.begin(Workplane::XY);

    // The default: XY seen along +Z, the right way up, and the view frame is
    // the authoring frame untouched.
    {
        const SketchFrame view = session.viewFrame();
        r.check("CADUXR1_06_08_a_new_sketch_is_XY_seen_along_plus_Z",
                session.plane() == Workplane::XY && !session.viewFlipped()
                        && session.viewQuarterTurns() == 0 && nearVec(view.n, 0.0f, 0.0f, 1.0f)
                        && nearVec(view.u, 1.0f, 0.0f, 0.0f) && nearVec(view.v, 0.0f, 1.0f, 0.0f)
                        && rightHanded(view));
    }
    // Flipping looks at the back of the plane. The normal reverses, the basis
    // stays right-handed, and NOTHING is mirrored.
    {
        session.setViewFlipped(true);
        const SketchFrame view = session.viewFrame();
        r.check("CADUXR1_08_flipping_the_normal_stays_right_handed",
                session.viewFlipped() && nearVec(view.n, 0.0f, 0.0f, -1.0f)
                        && nearVec(view.u, 1.0f, 0.0f, 0.0f)
                        && nearVec(view.v, 0.0f, -1.0f, 0.0f) && rightHanded(view));
        session.setViewFlipped(false);
    }
    // A quarter turn: the VIEW turns clockwise, so what was at screen-up ends
    // up at screen-left and the drawing appears to turn counter-clockwise.
    // Exact, and still right-handed.
    {
        session.rotateView(1);
        const SketchFrame plus = session.viewFrame();
        const bool plusOk = session.viewQuarterTurns() == 1
                            && nearVec(plus.u, 0.0f, -1.0f, 0.0f)
                            && nearVec(plus.v, 1.0f, 0.0f, 0.0f) && rightHanded(plus);
        session.rotateView(-2);
        const SketchFrame minus = session.viewFrame();
        const bool minusOk = session.viewQuarterTurns() == 3
                             && nearVec(minus.u, 0.0f, 1.0f, 0.0f)
                             && nearVec(minus.v, -1.0f, 0.0f, 0.0f) && rightHanded(minus);
        r.check("CADUXR1_11_12_plus_and_minus_ninety_are_exact_quarter_turns",
                plusOk && minusOk);
    }
    // Four quarter turns is the identity, and a rotation never touches one
    // authored coordinate.
    {
        SketchSession rolled;
        rolled.begin(Workplane::XY);
        CadSketch* sketch = const_cast<CadSketch*>(&rolled.sketch());
        addSketchEntity(sketch, SketchLine{SketchPoint{1.0, 2.0}, SketchPoint{3.0, 4.0}});
        const CadSketch before = rolled.sketch();
        for (int i = 0; i < 4; ++i) {
            rolled.rotateView(1);
        }
        const SketchFrame view = rolled.viewFrame();
        r.check("CADUXR1_13_a_view_rotation_mutates_no_authored_coordinate",
                rolled.viewQuarterTurns() == 0 && nearVec(view.u, 1.0f, 0.0f, 0.0f)
                        && sameCadSketch(before, rolled.sketch()));
    }
    // Support switching is allowed while the sketch is empty, and it re-bases
    // the authoring frame to the new plane.
    {
        SketchSession empty;
        empty.begin(Workplane::XY);
        const CadStatus toXZ = empty.setSupportPlane(Workplane::XZ);
        const SketchFrame frame = empty.frame();
        const WorkplaneFrame wanted = workplaneFrame(Workplane::XZ);
        r.check("CADUXR1_09_14_support_switching_is_allowed_while_the_sketch_is_empty",
                toXZ == CadStatus::Ok && empty.plane() == Workplane::XZ
                        && nearVec(frame.n, wanted.normal.x, wanted.normal.y, wanted.normal.z)
                        && nearVec(frame.u, wanted.uAxis.x, wanted.uAxis.y, wanted.uAxis.z));
        const CadStatus toYZ = empty.setSupportPlane(Workplane::YZ);
        const WorkplaneFrame yz = workplaneFrame(Workplane::YZ);
        r.check("CADUXR1_10_every_principal_plane_is_reachable_from_the_navigator",
                toYZ == CadStatus::Ok && empty.plane() == Workplane::YZ
                        && nearVec(empty.frame().n, yz.normal.x, yz.normal.y, yz.normal.z));
    }
    // Once there is geometry, the plane is frozen: the authored numbers mean
    // one plane and are never quietly reinterpreted on another.
    {
        SketchSession drawn;
        drawn.begin(Workplane::XY);
        addSketchEntity(const_cast<CadSketch*>(&drawn.sketch()),
                        SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{1.0, 1.0}});
        const CadSketch before = drawn.sketch();
        const CadStatus why = drawn.setSupportPlane(Workplane::XZ);
        r.check("CADUXR1_15_support_switching_after_geometry_is_refused_by_name",
                why == CadStatus::SketchNotEmpty && drawn.plane() == Workplane::XY
                        && sameCadSketch(before, drawn.sketch()));
    }
    // A FACE-supported sketch stays attached to its TopoRef: the navigator can
    // turn the view, but it cannot re-point the support at a world plane.
    {
        SketchSession faceSketch;
        TopoRef ref;
        ref.producerObjectId = 7;
        ref.face.kind = CadFaceKind::CapFar;
        ref.lineageToken = 42;
        SketchFrame worldFrame;
        worldFrame.origin = Vec3{0.0f, 0.0f, 2.0f};
        worldFrame.u = Vec3{1.0f, 0.0f, 0.0f};
        worldFrame.v = Vec3{0.0f, 1.0f, 0.0f};
        worldFrame.n = Vec3{0.0f, 0.0f, 1.0f};
        const CadStatus began = faceSketch.beginOnFace(worldFrame, ref);
        const CadStatus refused = faceSketch.setSupportPlane(Workplane::XZ);
        faceSketch.rotateView(1);
        r.check("CADUXR1_16_a_face_sketch_keeps_its_TopoRef_and_still_rotates",
                began == CadStatus::Ok && refused == CadStatus::InvalidWorkplane
                        && faceSketch.sketch().hasFaceSupport
                        && faceSketch.sketch().faceSupport.producerObjectId == 7
                        && faceSketch.viewQuarterTurns() == 1
                        && rightHanded(faceSketch.viewFrame()));
    }
}

// -------------------------------------------------------------------------
// Edit Sketch (`CADUXR1-32..36`)
// -------------------------------------------------------------------------

SketchFrame planeFrameOf(Workplane plane) {
    const WorkplaneFrame wf = workplaneFrame(plane);
    return SketchFrame{Vec3{0.0f, 0.0f, 0.0f}, wf.uAxis, wf.vAxis, wf.normal};
}

void testEditSketch(Recorder& r) {
    // A one-body project whose only body is a CAD rectangle.
    ConstructionScene scene((NoProjectTag()));
    ConstructionHistory history(scene);
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(rectBody(2.0, 1.0, 1.0), &why);
    const bool built = object != nullptr && why == CadStatus::Ok;
    const ObjectId bodyId = built ? object->objectId() : kNoObject;
    if (built) {
        publishSceneObject(*object);
    }

    // Opening the edit stages a COPY. The body is untouched throughout.
    {
        SketchSession session;
        const CadStatus began = session.beginEdit(bodyId, object->cadOrNull()->state(),
                                                  planeFrameOf(Workplane::XY));
        const bool staged = began == CadStatus::Ok && session.editingExistingBody()
                            && session.editingBodyId() == bodyId
                            && session.sketch().entities.size() == 1;
        // Change the staged rectangle. The BODY still says 2.0 x 1.0.
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 5.0;
        wider.height = 1.0;
        const CadStatus replaced = session.replaceEntity(1, wider);
        const double bodyWidth = object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_32_edit_sketch_stages_a_copy_and_leaves_the_body_alone",
                built && staged && replaced == CadStatus::Ok && near2(bodyWidth, 2.0)
                        && history.undoDepth() == 0);
        // Cancel: zero durable mutation, no history step.
        session.cancel();
        r.check("CADUXR1_33_edit_sketch_cancel_is_exact_non_mutation",
                !session.active() && !session.editingExistingBody()
                        && near2(object->cadOrNull()->sketch().entities[0].rectangle()->width, 2.0)
                        && history.undoDepth() == 0 && history.redoDepth() == 0);
    }

    // Finish: exactly one history step, and Undo/Redo restore the whole sketch.
    {
        SketchSession session;
        session.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 5.0;
        wider.height = 1.0;
        session.replaceEntity(1, wider);
        const CadStatus finished = session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        const double afterWidth = object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_34_finish_edit_is_exactly_one_project_history_step",
                finished == CadStatus::Ok && committed == CadStatus::Ok && near2(afterWidth, 5.0)
                        && history.undoDepth() == 1 && history.redoDepth() == 0
                        && !session.active());
        const bool undone = history.undo();
        const double undoneWidth =
                scene.findBody(bodyId)->cadOrNull()->sketch().entities[0].rectangle()->width;
        const bool redone = history.redo();
        const double redoneWidth =
                scene.findBody(bodyId)->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_35_undo_and_redo_of_a_sketch_edit_are_exact",
                undone && near2(undoneWidth, 2.0) && redone && near2(redoneWidth, 5.0));
        history.undo();  // back to the 2.0-wide body for the cases below
    }

    // A body standing on the producer's far cap follows a SUPPORTED edit --
    // one that changes sizes but not the set of faces.
    {
        CadBodyState child = rectBody(0.5, 0.5, 0.25);
        child.sketch.hasFaceSupport = true;
        child.sketch.faceSupport.producerObjectId = bodyId;
        child.sketch.faceSupport.face.kind = CadFaceKind::CapFar;
        child.sketch.faceSupport.lineageToken =
                cadTopologySignature(object->cadOrNull()->state());
        CadStatus childWhy = CadStatus::Ok;
        SceneObject* childObject = scene.addCadBody(child, &childWhy);
        const bool added = childObject != nullptr && childWhy == CadStatus::Ok;
        if (added) {
            publishSceneObject(*childObject);
        }

        SketchSession session;
        session.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        SketchRectangle wider;
        wider.center = SketchPoint{0.0, 0.0};
        wider.width = 3.0;
        wider.height = 1.0;
        session.replaceEntity(1, wider);
        session.finish();
        session.selectProfile(1);
        const CadStatus committed = session.commitEdit(scene, history);
        Mat4 childModel;
        const bool resolves = added && scene.resolveWorldModel(childObject->objectId(),
                                                               &childModel);
        r.check("CADUXR1_36_a_dependent_survives_a_supported_sketch_edit",
                added && committed == CadStatus::Ok && resolves);

        // An edit that would take the referenced face away is REFUSED by name
        // and changes nothing, exactly as deleting such a producer is.
        SketchSession breaking;
        breaking.beginEdit(bodyId, object->cadOrNull()->state(), planeFrameOf(Workplane::XY));
        CadSketch* staged = const_cast<CadSketch*>(&breaking.sketch());
        removeSketchEntity(staged, 1);
        SketchCircle circle;
        circle.center = SketchPoint{0.0, 0.0};
        circle.radius = 1.0;
        SketchEntityId circleId = kNoSketchEntity;
        addSketchEntity(staged, circle, &circleId);
        const CadStatus finished = breaking.finish();
        breaking.selectProfile(circleId);
        const CadStatus refused = breaking.commitEdit(scene, history);
        const double stillWide =
                object->cadOrNull()->sketch().entities[0].rectangle()->width;
        r.check("CADUXR1_36_an_edit_that_would_strip_a_supported_face_is_refused",
                finished == CadStatus::Ok && refused == CadStatus::DependentFaceLost
                        && near2(stillWide, 3.0));
    }
}

// -------------------------------------------------------------------------
// The CADB v3 data contract (`CADUXR1-37, 38`)
// -------------------------------------------------------------------------

ProjectDocument documentFor(const CadBodyState& state) {
    ConstructionScene scene((NoProjectTag()));
    CadStatus why = CadStatus::Ok;
    SceneObject* object = scene.addCadBody(state, &why);
    if (object == nullptr) {
        return ProjectDocument{};
    }
    publishSceneObject(*object);
    return captureProjectDocument(scene, ProjectKind::Construction);
}

// The CADB section's declared version, read straight out of the bytes.
bool cadSectionVersion(const std::vector<uint8_t>& bytes, uint16_t* out) {
    for (size_t i = 0; i + 8 <= bytes.size(); ++i) {
        if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D' && bytes[i + 3] == 'B') {
            *out = static_cast<uint16_t>(bytes[i + 4] | (bytes[i + 5] << 8));
            return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------------
// The independent corpus (`CADUXR1-38`)
// -------------------------------------------------------------------------
//
// These six documents are the C++ side of the six `CADB` v3 fixtures
// `scripts/build-forge-corpus.ps1` writes from `DATA_PACKAGE_SPEC.md` §7d. The
// PowerShell builder shares no line with the codec, so a digest agreeing here
// is the format being a SPECIFICATION rather than whatever this encoder happens
// to emit. A change nobody meant to make fails HERE, with the new value beside
// it, rather than silently invalidating the corpus.

TransformValues placementAt(double tx, double ty, double tz) {
    TransformValues t;
    t.positionX = tx;
    t.positionY = ty;
    t.positionZ = tz;
    return t;
}

ProjectBodyPlacement sceneBodyAt(ObjectId id, const TransformValues& transform) {
    ProjectBodyPlacement body;
    body.objectId = id;
    body.transform = transform;
    return body;
}

// The canonical semicircle plus the chord that closes it: a "D" profile that is
// one chain of two entities, one curved and one straight.
ProjectCadBody arcProfileBody(ObjectId id, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = arcAndLineBody(depth);
    return body;
}

ProjectCadBody splineProfileBody(ObjectId id, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = splineAndLineBody();
    body.state.extrude.depth = depth;
    return body;
}

ProjectCadBody rectangleProfileBody(ObjectId id, double w, double h, double depth) {
    ProjectCadBody body;
    body.objectId = id;
    body.state = rectBody(w, h, depth);
    return body;
}

ProjectDocument cadArcProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(sceneBodyAt(1, placementAt(0.5, 0.0, 0.0)));
    document.hasCad = true;
    document.cad.bodies.push_back(arcProfileBody(1, 1.5));
    return document;
}

ProjectDocument cadSplineProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;
    document.scene.bodies.push_back(sceneBodyAt(1, TransformValues{}));
    document.hasCad = true;
    document.cad.bodies.push_back(splineProfileBody(1, 0.75));
    return document;
}

// Every entity kind the format has, in one v3 section, with a Construction
// Body beside them so CONS stays sparse.
ProjectDocument cadMixedCurveProfileDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 5;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(sceneBodyAt(1, TransformValues{}));
    document.scene.bodies.push_back(sceneBodyAt(2, placementAt(2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(sceneBodyAt(3, placementAt(-2.0, 0.0, 0.0)));
    document.scene.bodies.push_back(sceneBodyAt(4, placementAt(0.0, 3.0, 0.0)));
    document.hasConstruction = true;
    // The same canonical parameter set every other corpus fixture's
    // Construction Body carries, so this one differs from them in exactly the
    // thing it is here to pin: the v3 `CADB` beside a sparse `CONS`.
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalCorpusShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);
    document.hasCad = true;
    document.cad.bodies.push_back(arcProfileBody(2, 1.0));
    document.cad.bodies.push_back(splineProfileBody(3, 0.5));
    document.cad.bodies.push_back(rectangleProfileBody(4, 1.5, 1.5, 1.0));
    return document;
}

// v3 carrying a v2 support block, which is the whole point of a version being a
// superset of the one below it: a CURVE profile standing on a producer's cap.
ProjectDocument cadFaceCurveDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(sceneBodyAt(1, placementAt(0.0, 0.0, 1.0)));
    document.scene.bodies.push_back(sceneBodyAt(2, TransformValues{}));
    document.hasCad = true;
    const ProjectCadBody producer = rectangleProfileBody(1, 4.0, 4.0, 1.0);
    ProjectCadBody dependent = arcProfileBody(2, 0.5);
    dependent.state.sketch.hasFaceSupport = true;
    dependent.state.sketch.faceSupport.producerObjectId = producer.objectId;
    dependent.state.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    dependent.state.sketch.faceSupport.face = CadFaceToken{CadFaceKind::CapFar, 0, 0};
    dependent.state.sketch.faceSupport.lineageToken = cadTopologySignature(producer.state);
    document.cad.bodies.push_back(producer);
    document.cad.bodies.push_back(dependent);
    return document;
}

// The two CORRUPT v3 fixtures: an arc whose three points are collinear, and a
// spline whose two ends meet. Every length, count and CRC in both is correct,
// so only the SEMANTIC check can refuse them -- which is exactly what makes
// them worth having.
//
// They cannot come out of `encodeProjectV1`, which validates first and would
// refuse the document. So, exactly as the `CAD-R0` bad-plane and `CAD-A3`
// bad-reference fixtures already do here, the valid file is encoded and the bad
// values are written over their own FIXED-WIDTH fields: every length, count and
// offset is unchanged by construction, and only the payload CRC is redone. The
// PowerShell builder reaches the same bytes by constructing them with the bad
// value in place, which is the point -- two routes, one file.

// The byte offset of the first entity's own values inside a v3 `CADB` payload
// carrying ONE body whose first entity is the one being corrupted:
//
//   bodyCount u32 | objectId u64 | plane u8 | supportKind u8 | nextEntityId u32
//   | profileEntityId u32 | direction u8 | depth f64 | entityCount u32
//   | entityId u32 | kind u8
constexpr size_t kV3FirstEntityValueOffset = 4 + 8 + 1 + 1 + 4 + 4 + 1 + 8 + 4 + 4 + 1;

// Overwrites `count` doubles at `valueIndex` within the first entity's values,
// then recomputes the CAD section's CRC. Returns the patched file.
//
// `prefixBytes` is what stands between the kind code and the first double for
// this entity kind: 0 for an Arc, whose six doubles follow immediately, and 4
// for a Spline, whose points follow its `pointCount`.
std::vector<uint8_t> patchFirstCadEntityValues(const std::vector<uint8_t>& file,
                                               size_t prefixBytes, size_t valueIndex,
                                               const double* values, size_t count) {
    std::vector<uint8_t> bytes = file;
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            const size_t payload = offset + kForgeSectionHeaderBytes;
            const size_t at = payload + kV3FirstEntityValueOffset + prefixBytes
                              + valueIndex * sizeof(double);
            if (at + count * sizeof(double) > bytes.size()) {
                return std::vector<uint8_t>();  // refused rather than trusted
            }
            std::memcpy(&bytes[at], values, count * sizeof(double));
            const uint32_t crc = crc32IsoHdlc(&bytes[payload], static_cast<size_t>(payloadBytes));
            std::memcpy(&bytes[offset + 16], &crc, 4);
            return bytes;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return std::vector<uint8_t>();
}

// The valid counterparts the two corrupt files are patched from: the same
// documents with a depth of 1.0 and the identity placement, so the only
// difference from the fixture is the values that are then overwritten.
ProjectDocument cadBadArcBaseDocument() {
    ProjectDocument document = cadArcProfileDocument();
    document.scene.bodies[0].transform = TransformValues{};
    document.cad.bodies[0].state.extrude.depth = 1.0;
    return document;
}

ProjectDocument cadBadSplineBaseDocument() {
    ProjectDocument document = cadSplineProfileDocument();
    document.cad.bodies[0].state.extrude.depth = 1.0;
    return document;
}

void testDataContract(Recorder& r) {
    // A project of straight geometry stays at v1: no existing file moves.
    {
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(rectBody(2.0, 1.0, 1.0)));
        uint16_t version = 0;
        r.check("CADUXR1_37_a_curveless_cad_project_still_writes_CADB_v1",
                !bytes.empty() && cadSectionVersion(bytes, &version)
                        && version == kCadSectionVersion);
    }
    // A body carrying an arc forces v3, and only that body's presence does.
    {
        const std::vector<uint8_t> bytes = encodeProjectV1(documentFor(arcAndLineBody()));
        uint16_t version = 0;
        r.check("CADUXR1_38_a_body_with_a_curve_writes_CADB_v3",
                !bytes.empty() && cadSectionVersion(bytes, &version)
                        && version == kCadSectionVersionV3);
    }
    // The round trip is bit-exact for both curve kinds, and re-encoding the
    // decoded document reproduces the same bytes: a deterministic writer.
    {
        for (int kind = 0; kind < 2; ++kind) {
            const CadBodyState state = kind == 0 ? arcAndLineBody() : splineAndLineBody();
            const ProjectDocument original = documentFor(state);
            const std::vector<uint8_t> bytes = encodeProjectV1(original);
            ProjectDocument decoded;
            const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
            const std::vector<uint8_t> again = encodeProjectV1(decoded);
            r.check(kind == 0 ? "CADUXR1_26_an_arc_round_trips_bit_exactly"
                              : "CADUXR1_29_a_spline_round_trips_bit_exactly",
                    why == ProjectCodecStatus::Ok && sameProjectDocument(original, decoded)
                            && again == bytes);
        }
    }
    // A curve code inside a section that declared itself v1 or v2 is a
    // malformed file, refused rather than read.
    {
        std::vector<uint8_t> bytes = encodeProjectV1(documentFor(arcAndLineBody()));
        bool patched = false;
        for (size_t i = 0; i + 8 <= bytes.size(); ++i) {
            if (bytes[i] == 'C' && bytes[i + 1] == 'A' && bytes[i + 2] == 'D'
                && bytes[i + 3] == 'B') {
                bytes[i + 4] = static_cast<uint8_t>(kCadSectionVersionV2);
                patched = true;
                break;
            }
        }
        ProjectDocument decoded;
        const ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &decoded);
        // The CRC guards the section too, so the refusal may name either the
        // checksum or the value; what matters is that it is REFUSED and that
        // nothing was written.
        r.check("CADUXR1_37_a_curve_inside_a_v2_section_is_refused",
                patched && why != ProjectCodecStatus::Ok && !decoded.hasCad);
    }
    // A file whose sketch closes no profile is refused, curve or not.
    {
        CadBodyState open = arcAndLineBody();
        open.sketch.entities.pop_back();  // drop the closing line
        ProjectDocument document = documentFor(rectBody(1.0, 1.0, 1.0));
        if (!document.cad.bodies.empty()) {
            document.cad.bodies[0].state = open;
        }
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> bytes = encodeProjectV1(document, &why);
        r.check("CADUXR1_38_a_curve_sketch_that_closes_nothing_is_refused",
                bytes.empty() && why == ProjectCodecStatus::InvalidSemanticValue);
    }

    // ---------------------------------------------------------------------
    // The independent corpus: this encoder against the PowerShell one
    // ---------------------------------------------------------------------
    //
    // The digests below are what `scripts/build-forge-corpus.ps1` wrote into
    // `testdata/forge/v1/` and what `DATA_PACKAGE_SPEC.md` §7d records. The two
    // implementations share no line: one is the production codec, the other is
    // written from the spec text. Agreement is the format being a
    // SPECIFICATION; a change nobody meant to make fails here, with the new
    // value beside it, rather than silently invalidating the corpus.
    {
        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> arcBytes = encodeProjectV1(cadArcProfileDocument(), &why);
        r.check("CADUXR1_38_cad_arc_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(arcBytes)
                                   == "580a47dcfa305ddec34c2ed7831ba88b7a05659008e7944285ca2a70fc"
                                      "02b8cd");
        const std::vector<uint8_t> splineBytes = encodeProjectV1(cadSplineProfileDocument(), &why);
        r.check("CADUXR1_38_cad_spline_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(splineBytes)
                                   == "e3ff4f7be2529a30f040bd3d699b46df9792473b88755494144c15697b"
                                      "19261a");
        const std::vector<uint8_t> mixedBytes =
                encodeProjectV1(cadMixedCurveProfileDocument(), &why);
        r.check("CADUXR1_38_cad_mixed_curve_profile_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(mixedBytes)
                                   == "3e2fa16f05e353f1745a36e165aedadbb5d5378db0c039167293aececa"
                                      "d7078d");
        const std::vector<uint8_t> faceCurveBytes = encodeProjectV1(cadFaceCurveDocument(), &why);
        r.check("CADUXR1_38_cad_face_curve_fixture_matches_the_committed_digest",
                why == ProjectCodecStatus::Ok
                        && projectFixtureSha256Hex(faceCurveBytes)
                                   == "0a8218f0aa86cfb7cdcf7781c72864e76e9065e2f7a788d5b2cf4e6fc1"
                                      "250ad0");

        // The two corrupt files, patched from their valid counterparts.
        const double collinear[6] = {-1.0, 0.0, 0.0, 0.0, 1.0, 0.0};
        const std::vector<uint8_t> badArcBytes = patchFirstCadEntityValues(
                encodeProjectV1(cadBadArcBaseDocument(), &why), 0, 0, collinear, 6);
        r.check("CADUXR1_38_cad_bad_arc_fixture_matches_the_committed_digest",
                !badArcBytes.empty()
                        && projectFixtureSha256Hex(badArcBytes)
                                   == "628d74fdbf3cf9082ef4869f9b948acef81c6a14517703b707896602e7"
                                      "c4b418");
        const double meetingEnd[2] = {-1.0, 0.0};
        const std::vector<uint8_t> badSplineBytes = patchFirstCadEntityValues(
                encodeProjectV1(cadBadSplineBaseDocument(), &why), 4, 6, meetingEnd, 2);
        r.check("CADUXR1_38_cad_bad_spline_fixture_matches_the_committed_digest",
                !badSplineBytes.empty()
                        && projectFixtureSha256Hex(badSplineBytes)
                                   == "f5437366d894032e97b2e49d3027726fa2bc739bda747f05f3b1eaf8c9"
                                      "ed714d");

        // And both are REFUSED by the ordinary decoder, by name: the whole
        // point of a corrupt fixture is that only the semantic check catches
        // it, so a reader that opened one would have a real defect.
        ProjectDocument decoded;
        const ProjectCodecStatus arcWhy =
                decodeProject(badArcBytes.data(), badArcBytes.size(), &decoded);
        ProjectDocument decodedSpline;
        const ProjectCodecStatus splineWhy =
                decodeProject(badSplineBytes.data(), badSplineBytes.size(), &decodedSpline);
        r.check("CADUXR1_38_the_corrupt_curve_fixtures_fail_closed_by_name",
                arcWhy == ProjectCodecStatus::InvalidSemanticValue
                        && splineWhy == ProjectCodecStatus::InvalidSemanticValue
                        && !decoded.hasCad && !decodedSpline.hasCad);
    }
}

void measurePerformance() {
    using Clock = std::chrono::steady_clock;
    const auto micros = [](Clock::time_point a, Clock::time_point b) {
        return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count();
    };
    const SketchEntity arc(1, unitSemicircle());
    std::vector<SketchPoint> points;
    auto t0 = Clock::now();
    for (int i = 0; i < 200; ++i) {
        tessellateSketchCurve(arc, &points);
    }
    auto t1 = Clock::now();
    std::vector<SketchPoint> splinePoints = {SketchPoint{-1.0, 0.0}, SketchPoint{-0.5, 0.8},
                                             SketchPoint{0.5, 0.8}, SketchPoint{1.0, 0.0}};
    const SketchEntity spline(1, SketchSpline{splinePoints});
    for (int i = 0; i < 200; ++i) {
        tessellateSketchCurve(spline, &points);
    }
    auto t2 = Clock::now();
    const CadBodyState mixed = arcAndLineBody();
    for (int i = 0; i < 50; ++i) {
        extractClosedProfiles(mixed.sketch);
    }
    auto t3 = Clock::now();
    ConstructionMesh mesh;
    for (int i = 0; i < 50; ++i) {
        generateCadMesh(mixed, &mesh);
    }
    auto t4 = Clock::now();
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "arc_tess=%lldus/200 spline_tess=%lldus/200 curve_profile=%lldus/50 "
                  "curve_regen=%lldus/50",
                  static_cast<long long>(micros(t0, t1)), static_cast<long long>(micros(t1, t2)),
                  static_cast<long long>(micros(t2, t3)), static_cast<long long>(micros(t3, t4)));
    g_performance = buffer;
}

}  // namespace

int runSketchUxSelfTests(SketchUxSelfTestResult* out, int maxOut) {
    if (out == nullptr || maxOut <= 0) {
        return 0;
    }
    Recorder r{out, maxOut};
    testArcDomain(r);
    testSplineDomain(r);
    testCurveProfiles(r);
    testLineDimension(r);
    testOrientationNavigator(r);
    testEditSketch(r);
    testDataContract(r);
    measurePerformance();
    return r.n;
}

const char* sketchUxPerformanceReport() { return g_performance.c_str(); }

}  // namespace forgeshape
