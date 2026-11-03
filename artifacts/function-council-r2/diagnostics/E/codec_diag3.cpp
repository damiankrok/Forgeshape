// FUNCTION-COUNCIL-R2 seat E diagnostic. Never part of the build.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"
#include "forgeshape_history.h"
using namespace forgeshape;

static std::vector<uint8_t> readAll(const char* p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
static uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t rd64(const uint8_t* p) { return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32); }
static void wr32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = (v >> (8 * i)) & 0xff; }

struct Section { size_t hdr; size_t payload; uint64_t len; };
static std::vector<Section> sections(const std::vector<uint8_t>& b) {
    std::vector<Section> s;
    if (b.size() < 28) return s;
    uint32_t n = rd32(&b[16]);
    size_t off = 28;
    for (uint32_t i = 0; i < n && off + 24 <= b.size(); ++i) {
        uint64_t len = rd64(&b[off + 8]);
        if (off + 24 + len > b.size()) break;
        s.push_back({off, off + 24, len});
        off += 24 + len;
    }
    return s;
}
static void fixCrc(std::vector<uint8_t>& b, const Section& s) {
    wr32(&b[s.hdr + 16], crc32IsoHdlc(s.len ? &b[s.payload] : nullptr, (size_t)s.len));
}

int main(int argc, char** argv) {
    int roundTripSame = 0, roundTripDiff = 0, loadRecapSame = 0, loadRecapDiff = 0, okFixtures = 0;
    std::map<std::string, int> fuzzStatus;
    long fuzzRuns = 0, fuzzDecodedOk = 0, fuzzLoadedOk = 0, fuzzLoadRefused = 0;
    long failClosedViolations = 0, fpCollisions = 0, fpCompared = 0;
    std::mt19937 rng(20261029u);
    for (int i = 1; i < argc; ++i) {
        const std::vector<uint8_t> bytes = readAll(argv[i]);
        const char* name = std::strrchr(argv[i], '/') ? std::strrchr(argv[i], '/') + 1 : argv[i];
        ProjectDocument doc;
        uint32_t skipped = 0;
        ProjectCodecStatus why = decodeProject(bytes.data(), bytes.size(), &doc, &skipped);
        if (why == ProjectCodecStatus::Ok) {
            ++okFixtures;
            ProjectCodecStatus ew;
            std::vector<uint8_t> again = encodeProjectV1(doc, &ew);
            const bool same = again == bytes;
            same ? ++roundTripSame : ++roundTripDiff;
            // Load into a fresh scene, capture, encode.
            ConstructionScene scene(NoProjectTag{});
            SculptSession session;
            ConstructionHistory history(scene);
            ProjectLoadReport rep;
            ProjectCodecStatus lw = loadProjectDocument(doc, scene, session, history, &rep);
            bool recapSame = false;
            if (lw == ProjectCodecStatus::Ok) {
                ProjectDocument cap = captureProjectDocument(scene, doc.kind);
                std::vector<uint8_t> re = encodeProjectV1(cap, &ew);
                recapSame = re == bytes;
            }
            recapSame ? ++loadRecapSame : ++loadRecapDiff;
            std::printf("FIXTURE %s decode=Ok skipped=%u reencode_identical=%d load=%s load_capture_encode_identical=%d\n",
                        name, skipped, same ? 1 : 0, projectCodecStatusName(lw), recapSame ? 1 : 0);
        } else {
            std::printf("FIXTURE %s decode=%s\n", name, projectCodecStatusName(why));
        }
        // Fuzz: byte mutations inside one section payload WITH the CRC repaired, so
        // the semantic reader (not just the checksum) is exercised.
        std::vector<Section> secs = sections(bytes);
        if (secs.empty()) continue;
        for (int k = 0; k < 400; ++k) {
            std::vector<uint8_t> m = bytes;
            const Section& s = secs[rng() % secs.size()];
            if (s.len == 0) continue;
            int flips = 1 + (rng() % 3);
            for (int f = 0; f < flips; ++f) {
                size_t at = s.payload + (rng() % s.len);
                switch (rng() % 4) {
                    case 0: m[at] ^= (uint8_t)(1u << (rng() % 8)); break;
                    case 1: m[at] = 0xff; break;
                    case 2: m[at] = 0x00; break;
                    default: m[at] = (uint8_t)rng(); break;
                }
            }
            fixCrc(m, s);
            ++fuzzRuns;
            ProjectDocument fd;
            ProjectCodecStatus fw = decodeProject(m.data(), m.size(), &fd);
            fuzzStatus[projectCodecStatusName(fw)]++;
            if (fw != ProjectCodecStatus::Ok) continue;
            ++fuzzDecodedOk;
            // Load over a LIVE project built from the original fixture, and check
            // fail-closed: a refused load must leave the fingerprint unchanged.
            ConstructionScene scene(NoProjectTag{});
            SculptSession session;
            ConstructionHistory history(scene);
            if (why == ProjectCodecStatus::Ok) {
                loadProjectDocument(doc, scene, session, history);
            }
            const bool had = scene.hasProject();
            const uint64_t before = had ? projectSemanticFingerprint(scene, ProjectKind::Construction) : 0;
            const size_t bodiesBefore = scene.bodyCount();
            ProjectCodecStatus lw = loadProjectDocument(fd, scene, session, history);
            if (lw != ProjectCodecStatus::Ok) {
                ++fuzzLoadRefused;
                const uint64_t after = had ? projectSemanticFingerprint(scene, ProjectKind::Construction) : 0;
                if (after != before || scene.bodyCount() != bodiesBefore) ++failClosedViolations;
                continue;
            }
            ++fuzzLoadedOk;
            // Fingerprint gap probe: two loaded documents whose canonical bytes
            // differ but whose fingerprints are equal.
            ProjectCodecStatus ew;
            ProjectDocument capM = captureProjectDocument(scene, fd.kind);
            std::vector<uint8_t> encM = encodeProjectV1(capM, &ew);
            if (why == ProjectCodecStatus::Ok) {
                ConstructionScene s0(NoProjectTag{});
                SculptSession ss0;
                ConstructionHistory h0(s0);
                if (loadProjectDocument(doc, s0, ss0, h0) == ProjectCodecStatus::Ok) {
                    ProjectDocument cap0 = captureProjectDocument(s0, doc.kind);
                    std::vector<uint8_t> enc0 = encodeProjectV1(cap0, &ew);
                    ++fpCompared;
                    const bool fpEq = projectSemanticFingerprint(s0, doc.kind) == projectSemanticFingerprint(scene, fd.kind);
                    if (fpEq && enc0 != encM) {
                        ++fpCollisions;
                        // Classify: does the difference lie only in SCUL?
                        bool sculOnly = !cap0.sculpt.bodies.empty();
                        ProjectDocument a = cap0, b = capM;
                        a.sculpt = ProjectSculptRecord{}; b.sculpt = ProjectSculptRecord{};
                        a.hasSculpt = b.hasSculpt = false;
                        std::vector<uint8_t> ea = encodeProjectV1Unchecked(a), eb = encodeProjectV1Unchecked(b);
                        size_t d = 0; while (d < enc0.size() && d < encM.size() && (enc0[d] == encM[d] || [&]{ for (const Section& sx : sections(enc0)) if (d >= sx.hdr && d < sx.payload) return true; return false; }())) ++d;
                        std::string tag = "?"; size_t rel = 0;
                        for (const Section& sx : sections(enc0)) if (d >= sx.hdr && d < sx.payload + sx.len) { tag.assign((const char*)&enc0[sx.hdr], 4); rel = d - sx.payload; }
                        std::printf("FP_EQUAL_BYTES_DIFFER %s sculpt_only=%d first_diff=%zu section=%s payload_off=%zu old=%02x new=%02x sizes=%zu/%zu\n", name, (sculOnly && ea == eb) ? 1 : 0, d, tag.c_str(), rel, d<enc0.size()?enc0[d]:0, d<encM.size()?encM[d]:0, enc0.size(), encM.size());
                    }
                }
            }
        }
        // Truncations at every 7th byte length.
        for (size_t len = 0; len < bytes.size(); len += 7) {
            ProjectDocument td;
            ProjectCodecStatus tw = decodeProject(bytes.data(), len, &td);
            ++fuzzRuns;
            fuzzStatus[std::string("trunc:") + projectCodecStatusName(tw)]++;
        }
    }
    std::printf("SUMMARY ok_fixtures=%d reencode_identical=%d reencode_diff=%d load_capture_identical=%d load_capture_diff=%d\n",
                okFixtures, roundTripSame, roundTripDiff, loadRecapSame, loadRecapDiff);
    std::printf("FUZZ runs=%ld decoded_ok=%ld loaded_ok=%ld load_refused=%ld fail_closed_violations=%ld fp_compared=%ld fp_equal_bytes_differ=%ld\n",
                fuzzRuns, fuzzDecodedOk, fuzzLoadedOk, fuzzLoadRefused, failClosedViolations, fpCompared, fpCollisions);
    for (auto& kv : fuzzStatus) std::printf("FUZZ_STATUS %s %d\n", kv.first.c_str(), kv.second);
    return 0;
}
