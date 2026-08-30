#include "forgeshape_project_selftest.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "forgeshape_history.h"
#include "forgeshape_project_bytes.h"
#include "forgeshape_project_document.h"
#include "forgeshape_project_state.h"
#include "forgeshape_scene.h"
#include "forgeshape_sculpt.h"

namespace forgeshape {
namespace {

struct Recorder {
    ProjectSelfTestResult* out;
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

// ---------------------------------------------------------------------------
// SHA-256, for the golden-corpus check only
// ---------------------------------------------------------------------------
//
// Deliberately here rather than in the production codec: the product never
// hashes a project file, and a digest in the shipped format layer would invite
// someone to make it part of the format. What it IS for is drift detection --
// DATA_PACKAGE_SPEC.md records these digests for the committed fixtures, so a
// change to the encoder that nobody meant to make shows up as a mismatch rather
// than as a corpus that silently no longer describes the format.
struct Sha256 {
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint8_t block[64] = {};
    size_t blockLen = 0;
    uint64_t totalBits = 0;

    static uint32_t rotr(uint32_t value, int bits) {
        return (value >> bits) | (value << (32 - bits));
    }

    void compress() {
        static const uint32_t k[64] = {
                0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
                0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
                0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
                0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
                0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
                0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
                0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
                0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
                0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24)
                   | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
                   | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
                   | static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t t1 = hh + s1 + ch + k[i] + w[i];
            const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t t2 = s0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void update(const uint8_t* data, size_t size) {
        totalBits += static_cast<uint64_t>(size) * 8ull;
        for (size_t i = 0; i < size; ++i) {
            block[blockLen++] = data[i];
            if (blockLen == 64) {
                compress();
                blockLen = 0;
            }
        }
    }

    std::string hex() {
        const uint64_t bits = totalBits;
        const uint8_t one = 0x80u;
        update(&one, 1);
        totalBits = bits;
        while (blockLen != 56) {
            const uint8_t zero = 0u;
            update(&zero, 1);
            totalBits = bits;
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            block[blockLen++] = static_cast<uint8_t>((bits >> shift) & 0xFFu);
        }
        compress();
        char text[65];
        for (int i = 0; i < 8; ++i) {
            std::snprintf(text + i * 8, 9, "%08x", h[i]);
        }
        return std::string(text, 64);
    }
};

std::string sha256Hex(const std::vector<uint8_t>& bytes) {
    Sha256 sha;
    if (!bytes.empty()) {
        sha.update(bytes.data(), bytes.size());
    }
    return sha.hex();
}

// ---------------------------------------------------------------------------
// The two canonical documents
// ---------------------------------------------------------------------------
//
// Every number here is an exact binary fraction, so the committed fixture is
// reproducible by an independent implementation with no rounding argument -- see
// scripts/build-forge-corpus.ps1, which writes the same bytes from the
// specification alone and is the cross-implementation half of the portability
// proof.

TransformValues placement(double px, double py, double pz, double rx, double ry, double rz,
                          double sx, double sy, double sz) {
    TransformValues values;
    values.positionX = px;
    values.positionY = py;
    values.positionZ = pz;
    values.rotationX = rx;
    values.rotationY = ry;
    values.rotationZ = rz;
    values.scaleX = sx;
    values.scaleY = sy;
    values.scaleZ = sz;
    return values;
}

// All six remembered parameter sets for body `i` (1-based), every one of them
// different from every other body's, so a roundtrip that dropped the five
// INACTIVE sets — or swapped two bodies' — cannot pass by coincidence.
ConstructionObjectState canonicalShape(int i, PrimitiveKind kind) {
    const double n = static_cast<double>(i);
    ConstructionObjectState shape;
    shape.kind = kind;
    shape.box = BoxDimensionsMeters{2.0 + 0.5 * n, 1.0 + 0.25 * n, 0.5 + 0.125 * n};
    shape.cylinder = CylinderDimensionsMeters{1.5 + 0.25 * n, 3.0 + 0.5 * n};
    shape.sphere = SphereDimensionsMeters{2.25 + 0.25 * n};
    shape.cone = ConeDimensionsMeters{1.75 + 0.25 * n, 2.5 + 0.5 * n};
    shape.capsule = CapsuleDimensionsMeters{1.0 + 0.125 * n, 4.0 + 0.25 * n};
    shape.plane = PlaneDimensionsMeters{3.0 + 0.5 * n, 1.5 + 0.25 * n};
    return shape;
}

// Six bodies, one per primitive kind, each with a non-default placement that
// includes an uncanonicalized 370 degrees and a non-uniform scale.
ProjectDocument canonicalConstructionDocument() {
    static const PrimitiveKind kKinds[6] = {PrimitiveKind::Box,     PrimitiveKind::Cylinder,
                                            PrimitiveKind::Sphere,  PrimitiveKind::Cone,
                                            PrimitiveKind::Capsule, PrimitiveKind::Plane};
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.hasConstruction = true;
    document.scene.nextObjectId = 7;
    document.scene.activeObjectId = 4;
    for (int i = 1; i <= 6; ++i) {
        const double n = static_cast<double>(i);
        ProjectBodyPlacement body;
        body.objectId = static_cast<ObjectId>(i);
        body.transform = placement(0.5 * n, -0.25 * n, 1.25 * n,
                                   370.0, -45.5, 12.25 * n,
                                   1.0 + 0.25 * n, 2.0, 0.5);
        document.scene.bodies.push_back(body);

        ProjectConstructionBody construction;
        construction.objectId = static_cast<ObjectId>(i);
        construction.shape = canonicalShape(i, kKinds[i - 1]);
        construction.features.push_back(ProjectFeatureRecord{});
        document.construction.bodies.push_back(construction);
    }
    return document;
}

// The legacy mixed state: two bodies, both with a Construction Source, the
// second also carrying an edited Frozen Sculpt Mesh, reopening in Sculpt.
ProjectDocument canonicalSculptDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Sculpt;
    document.hasConstruction = true;
    document.hasSculpt = true;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;

    ConstructionObjectState shared;
    shared.box = BoxDimensionsMeters{2.0, 1.0, 0.5};
    shared.cylinder = CylinderDimensionsMeters{1.0, 2.0};
    shared.sphere = SphereDimensionsMeters{1.5};
    shared.cone = ConeDimensionsMeters{1.0, 2.0};
    shared.capsule = CapsuleDimensionsMeters{1.0, 2.0};
    shared.plane = PlaneDimensionsMeters{2.0, 2.0};

    ProjectBodyPlacement first;
    first.objectId = 1;
    first.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(first);

    ProjectBodyPlacement second;
    second.objectId = 2;
    second.transform = placement(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(second);

    ProjectConstructionBody firstSource;
    firstSource.objectId = 1;
    firstSource.shape = shared;
    firstSource.shape.kind = PrimitiveKind::Box;
    firstSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(firstSource);

    ProjectConstructionBody secondSource;
    secondSource.objectId = 2;
    secondSource.shape = shared;
    secondSource.shape.kind = PrimitiveKind::Sphere;
    secondSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(secondSource);

    // A closed tetrahedron whose vertices are deliberately NOT the sphere the
    // Construction Source describes: this is what a sculpted mesh IS -- geometry
    // that can no longer be recreated from the source, which is the whole reason
    // it is stored rather than regenerated.
    ProjectSculptBody sculpt;
    sculpt.objectId = 2;
    sculpt.renderBothSides = false;
    sculpt.sourceStale = true;
    // Saved having been sculpted, so the destructive-reset guard has something
    // to lose if the flag does not come back.
    sculpt.hasEdits = true;
    sculpt.positions = {0.0f,  0.0f,  0.0f,
                        1.5f,  0.0f,  0.0f,
                        0.0f,  1.25f, 0.0f,
                        0.25f, 0.5f,  1.75f};
    sculpt.indices = {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3};
    document.sculpt.bodies.push_back(sculpt);
    return document;
}

uint32_t readU32(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    for (int i = 3; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<size_t>(i)];
    }
    return value;
}

uint64_t readU64(const std::vector<uint8_t>& bytes, size_t offset) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<size_t>(i)];
    }
    return value;
}

uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>(bytes[offset] | (static_cast<uint16_t>(bytes[offset + 1]) << 8));
}

// Appends a section the v1 reader has never heard of, with a valid length and a
// correct CRC, so the only thing that decides its fate is its required bit.
std::vector<uint8_t> withExtraSection(const std::vector<uint8_t>& file, bool required) {
    std::vector<uint8_t> out = file;
    const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::vector<uint8_t> section;
    {
        ByteWriter writer(section);
        writer.u8('X');
        writer.u8('T');
        writer.u8('R');
        writer.u8('A');
        writer.u16(1);
        writer.u16(required ? kSectionFlagRequired : 0u);
        writer.u64(sizeof(payload));
        writer.u32(crc32IsoHdlc(payload, sizeof(payload)));
        writer.u32(0u);
        writer.bytes(payload, sizeof(payload));
    }
    out.insert(out.end(), section.begin(), section.end());
    // sectionCount at offset 16, fileBytes at offset 20 -- both have to keep
    // describing the file, or the decoder refuses it for the wrong reason.
    const uint32_t sectionCount = readU32(out, 16) + 1u;
    for (int i = 0; i < 4; ++i) {
        out[16 + static_cast<size_t>(i)] = static_cast<uint8_t>((sectionCount >> (i * 8)) & 0xFFu);
    }
    const uint64_t fileBytes = out.size();
    for (int i = 0; i < 8; ++i) {
        out[20 + static_cast<size_t>(i)] = static_cast<uint8_t>((fileBytes >> (i * 8)) & 0xFFu);
    }
    return out;
}

// Walks the section table and reports where a section's PAYLOAD begins, or
// zero when the file does not carry that section.
//
// Used only to build a deliberately broken file for a corruption case. Doing it
// by walking the table rather than by scanning for a byte pattern is what makes
// the case say what it means: the patched field is the one the specification
// names, not the first four bytes that happened to look like it.
size_t findSectionPayload(const std::vector<uint8_t>& file, const char tag[4],
                          uint64_t* outPayloadBytes, size_t* outHeaderStart) {
    size_t offset = kForgeHeaderBytes;
    const uint32_t sectionCount = readU32(file, 16);
    for (uint32_t i = 0; i < sectionCount; ++i) {
        if (offset + kForgeSectionHeaderBytes > file.size()) {
            return 0;
        }
        const uint64_t payloadBytes = readU64(file, offset + 8);
        const size_t payloadStart = offset + kForgeSectionHeaderBytes;
        if (std::memcmp(file.data() + offset, tag, 4) == 0) {
            if (outPayloadBytes != nullptr) *outPayloadBytes = payloadBytes;
            if (outHeaderStart != nullptr) *outHeaderStart = offset;
            return payloadStart;
        }
        offset = payloadStart + static_cast<size_t>(payloadBytes);
    }
    return 0;
}

// Re-stamps a section's CRC after its payload has been deliberately edited, so
// that the case under test is the one the edit is about rather than the
// checksum catching it first.
void restampSectionCrc(std::vector<uint8_t>& file, size_t headerStart, size_t payloadStart,
                       uint64_t payloadBytes) {
    const uint32_t crc =
            crc32IsoHdlc(file.data() + payloadStart, static_cast<size_t>(payloadBytes));
    for (int i = 0; i < 4; ++i) {
        file[headerStart + 16 + static_cast<size_t>(i)] =
                static_cast<uint8_t>((crc >> (i * 8)) & 0xFFu);
    }
}

// A live project to load INTO, built the way the product builds one, so that
// "the pre-load project is unchanged" is a claim about a real scene rather than
// about an empty one.
struct LiveFixture {
    ConstructionScene scene;
    ConstructionHistory history{scene};
    SculptSession session;

    LiveFixture() {
        session.bindTarget(&scene.activeBody().frozenSculpt());
        publishConstructionObject(scene.activeBody().construction(),
                                  scene.activeBody().meshStore());
        {
            ScopedConstructionEdit edit(history);
            applyPrimitive(scene.activeBody().construction(), scene.activeBody().meshStore(),
                           PrimitiveSpec::forBox(3.0, 2.0, 1.0));
        }
        {
            ScopedConstructionEdit edit(history);
            SceneObject& added = scene.addBody();
            applyPrimitive(added.construction(), added.meshStore(),
                           PrimitiveSpec::forCylinder(1.25, 2.5));
        }
        session.bindTarget(&scene.activeBody().frozenSculpt());
    }

    ProjectCodecStatus load(const std::vector<uint8_t>& bytes, ProjectLoadReport* report) {
        ProjectDocument document;
        const ProjectCodecStatus status = decodeProject(bytes.data(), bytes.size(), &document);
        if (status != ProjectCodecStatus::Ok) {
            return status;
        }
        return loadProjectDocument(document, scene, session, history, report);
    }
};

std::string g_constructionSha;
std::string g_sculptSha;

}  // namespace

const char* canonicalConstructionFixtureSha256() { return g_constructionSha.c_str(); }
const char* canonicalSculptFixtureSha256() { return g_sculptSha.c_str(); }

int runProjectSelfTests(ProjectSelfTestResult* out, int maxOut) {
    Recorder r{out, maxOut};

    const ProjectDocument constructionDoc = canonicalConstructionDocument();
    const ProjectDocument sculptDoc = canonicalSculptDocument();

    ProjectCodecStatus why = ProjectCodecStatus::Ok;
    const std::vector<uint8_t> constructionBytes = encodeProjectV1(constructionDoc, &why);
    r.check("FSR1A_02_canonical_construction_document_encodes",
            why == ProjectCodecStatus::Ok && !constructionBytes.empty());
    const std::vector<uint8_t> sculptBytes = encodeProjectV1(sculptDoc, &why);
    r.check("FSR1A_02_canonical_sculpt_document_encodes",
            why == ProjectCodecStatus::Ok && !sculptBytes.empty());
    if (constructionBytes.empty() || sculptBytes.empty()) {
        return r.n;  // nothing below can say anything useful
    }

    g_constructionSha = sha256Hex(constructionBytes);
    g_sculptSha = sha256Hex(sculptBytes);
    {
        // A known-answer test for the digest itself, so a golden-corpus failure
        // can only mean the ENCODER moved. Without it, a bug in this file's
        // SHA-256 would look exactly like format drift.
        const std::vector<uint8_t> abc = {'a', 'b', 'c'};
        r.check("FSR1A_12_sha256_known_answer",
                sha256Hex(abc)
                        == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }

    // -----------------------------------------------------------------------
    // FSR1A-01: the exact envelope, and every multibyte field little-endian
    // -----------------------------------------------------------------------
    {
        const std::vector<uint8_t>& f = constructionBytes;
        r.check("FSR1A_01_magic_is_FORGESH1", std::memcmp(f.data(), "FORGESH1", 8) == 0);
        r.check("FSR1A_01_major_is_1_little_endian", f[8] == 1 && f[9] == 0);
        r.check("FSR1A_01_minor_is_0_little_endian", f[10] == 0 && f[11] == 0);
        r.check("FSR1A_01_header_is_28_bytes", readU16(f, 12) == 28 && f[12] == 28 && f[13] == 0);
        r.check("FSR1A_01_project_kind_is_construction", f[14] == 1);
        r.check("FSR1A_01_header_flags_say_has_cons_only", f[15] == kHeaderFlagHasConstruction);
        r.check("FSR1A_01_section_count_is_two_little_endian",
                f[16] == 2 && f[17] == 0 && f[18] == 0 && f[19] == 0);
        r.check("FSR1A_01_file_bytes_matches_actual_size", readU64(f, 20) == f.size());

        // Section 0 is SCNE, required, checksummed over its payload alone.
        const size_t s0 = kForgeHeaderBytes;
        r.check("FSR1A_01_first_section_is_scne", std::memcmp(f.data() + s0, "SCNE", 4) == 0);
        r.check("FSR1A_01_scne_section_version_is_1", readU16(f, s0 + 4) == 1);
        r.check("FSR1A_01_scne_is_required", readU16(f, s0 + 6) == kSectionFlagRequired);
        const uint64_t scnePayload = readU64(f, s0 + 8);
        // 4 (count) + 8 (nextObjectId) + 8 (activeObjectId) + 6 bodies x 80.
        r.check("FSR1A_01_scne_payload_is_the_format_arithmetic", scnePayload == 4 + 8 + 8 + 6 * 80);
        r.check("FSR1A_01_scne_crc_covers_the_payload",
                crc32IsoHdlc(f.data() + s0 + kForgeSectionHeaderBytes,
                             static_cast<size_t>(scnePayload))
                        == readU32(f, s0 + 16));
        r.check("FSR1A_01_scne_reserved_is_zero", readU32(f, s0 + 20) == 0);

        const size_t s1 = s0 + kForgeSectionHeaderBytes + static_cast<size_t>(scnePayload);
        r.check("FSR1A_01_second_section_is_cons", std::memcmp(f.data() + s1, "CONS", 4) == 0);
        const uint64_t consPayload = readU64(f, s1 + 8);
        // 4 (count) + 6 bodies x (8 id + 1 kind + 96 parameters + 4 featureCount + 5 feature).
        r.check("FSR1A_01_cons_payload_is_the_format_arithmetic",
                consPayload == 4 + 6 * (8 + 1 + 96 + 4 + 5));
        r.check("FSR1A_01_sections_exactly_fill_the_file",
                s1 + kForgeSectionHeaderBytes + consPayload == f.size());

        // CRC-32/ISO-HDLC, pinned against the published check value so a second
        // implementation cannot accidentally agree with a different CRC-32.
        r.check("FSR1A_01_crc32_check_value",
                crc32IsoHdlc(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926u);
    }

    // -----------------------------------------------------------------------
    // FSR1A-02: deterministic writer
    // -----------------------------------------------------------------------
    {
        const std::vector<uint8_t> again = encodeProjectV1(constructionDoc);
        r.check("FSR1A_02_same_document_encodes_byte_identically", again == constructionBytes);

        ProjectDocument decoded;
        const ProjectCodecStatus status =
                decodeProject(constructionBytes.data(), constructionBytes.size(), &decoded);
        const std::vector<uint8_t> reencoded = encodeProjectV1(decoded);
        r.check("FSR1A_02_encode_decode_encode_is_byte_identical",
                status == ProjectCodecStatus::Ok && reencoded == constructionBytes);

        const std::vector<uint8_t> sculptAgain = encodeProjectV1(sculptDoc);
        r.check("FSR1A_02_sculpt_document_encodes_byte_identically",
                sculptAgain == sculptBytes);
    }

    // -----------------------------------------------------------------------
    // FSR1A-03: multi-body Construction roundtrip
    // -----------------------------------------------------------------------
    {
        ProjectDocument decoded;
        const ProjectCodecStatus status =
                decodeProject(constructionBytes.data(), constructionBytes.size(), &decoded);
        r.check("FSR1A_03_canonical_construction_decodes", status == ProjectCodecStatus::Ok);
        r.check("FSR1A_03_roundtrip_is_bit_identical",
                sameProjectDocument(decoded, constructionDoc));
        r.check("FSR1A_03_scene_order_and_ids_survive",
                decoded.scene.bodies.size() == 6 && decoded.scene.bodies[0].objectId == 1
                        && decoded.scene.bodies[5].objectId == 6);
        r.check("FSR1A_03_active_body_survives", decoded.scene.activeObjectId == 4);
        r.check("FSR1A_03_all_six_kinds_survive",
                decoded.construction.bodies[0].shape.kind == PrimitiveKind::Box
                        && decoded.construction.bodies[1].shape.kind == PrimitiveKind::Cylinder
                        && decoded.construction.bodies[2].shape.kind == PrimitiveKind::Sphere
                        && decoded.construction.bodies[3].shape.kind == PrimitiveKind::Cone
                        && decoded.construction.bodies[4].shape.kind == PrimitiveKind::Capsule
                        && decoded.construction.bodies[5].shape.kind == PrimitiveKind::Plane);
        // The five INACTIVE parameter sets of a body that is not a box.
        const ConstructionObjectState& plane = decoded.construction.bodies[5].shape;
        r.check("FSR1A_03_remembered_inactive_parameters_survive",
                plane.box.width == 5.0 && plane.cylinder.diameter == 3.0
                        && plane.sphere.diameter == 3.75 && plane.cone.height == 5.5
                        && plane.capsule.totalHeight == 5.5 && plane.plane.width == 6.0);
        r.check("FSR1A_03_370_degrees_is_not_canonicalized",
                decoded.scene.bodies[0].transform.rotationX == 370.0);
        r.check("FSR1A_03_non_uniform_scale_survives",
                decoded.scene.bodies[0].transform.scaleX == 1.25
                        && decoded.scene.bodies[0].transform.scaleY == 2.0
                        && decoded.scene.bodies[0].transform.scaleZ == 0.5);
        r.check("FSR1A_03_allocator_high_water_mark_survives", decoded.scene.nextObjectId == 7);
        r.check("FSR1A_03_feature_identity_is_object_id_plus_local_feature_id",
                decoded.construction.bodies[3].objectId == 4
                        && decoded.construction.bodies[3].features.size() == 1
                        && decoded.construction.bodies[3].features[0].localFeatureId == 1
                        && decoded.construction.bodies[3].features[0].kindCode
                                   == kFeatureKindPrimitiveSource);

        // The id allocator after a load cannot collide with anything loaded.
        LiveFixture live;
        ProjectLoadReport report;
        const ProjectCodecStatus loaded = live.load(constructionBytes, &report);
        r.check("FSR1A_03_construction_document_loads", loaded == ProjectCodecStatus::Ok);
        r.check("FSR1A_03_loaded_scene_has_every_body_in_order",
                live.scene.bodyCount() == 6 && live.scene.bodyAt(0).objectId() == 1
                        && live.scene.bodyAt(5).objectId() == 6);
        r.check("FSR1A_03_loaded_active_body_is_the_file_s",
                live.scene.activeBodyId() == 4);
        const ObjectId minted = live.scene.addBody().objectId();
        r.check("FSR1A_03_next_minted_id_cannot_collide",
                minted >= 7 && live.scene.findBody(minted) != nullptr
                        && live.scene.bodyCount() == 7);
    }

    // -----------------------------------------------------------------------
    // FSR1A-04: derived Construction geometry is regenerated, never stored
    // -----------------------------------------------------------------------
    {
        LiveFixture live;
        ProjectLoadReport report;
        const ProjectCodecStatus loaded = live.load(constructionBytes, &report);
        bool everyBodyHasGeometry = (loaded == ProjectCodecStatus::Ok);
        uint64_t totalVertices = 0;
        for (size_t i = 0; i < live.scene.bodyCount(); ++i) {
            const RuntimeMeshPtr mesh = live.scene.bodyAt(i).meshStore().current();
            if (!mesh || mesh->vertexCount() == 0 || mesh->indexCount() == 0) {
                everyBodyHasGeometry = false;
                break;
            }
            totalVertices += mesh->vertexCount();
        }
        r.check("FSR1A_04_every_loaded_body_has_regenerated_geometry", everyBodyHasGeometry);
        r.check("FSR1A_04_regenerated_geometry_is_far_larger_than_the_file",
                totalVertices * sizeof(MeshVertex) > constructionBytes.size() * 4);
        // Arithmetic, not a guess: the whole file is the header plus two
        // sections whose payload lengths are fixed by the body count. There is
        // physically no room in it for a vertex, an index or a revision.
        r.check("FSR1A_04_file_size_is_exactly_the_semantic_arithmetic",
                constructionBytes.size()
                        == static_cast<size_t>(kForgeHeaderBytes)
                                   + (kForgeSectionHeaderBytes + 4 + 8 + 8 + 6 * 80)
                                   + (kForgeSectionHeaderBytes + 4 + 6 * (8 + 1 + 96 + 4 + 5)));
    }

    // -----------------------------------------------------------------------
    // FSR1A-05 / FSR1A-06: sculpt and legacy mixed state
    // -----------------------------------------------------------------------
    {
        ProjectDocument decoded;
        const ProjectCodecStatus status =
                decodeProject(sculptBytes.data(), sculptBytes.size(), &decoded);
        r.check("FSR1A_05_sculpt_document_decodes", status == ProjectCodecStatus::Ok);
        r.check("FSR1A_05_sculpt_roundtrip_is_bit_identical",
                sameProjectDocument(decoded, sculptDoc));
        r.check("FSR1A_05_sculpt_flags_survive",
                decoded.sculpt.bodies.size() == 1 && decoded.sculpt.bodies[0].objectId == 2
                        && !decoded.sculpt.bodies[0].renderBothSides
                        && decoded.sculpt.bodies[0].sourceStale
                        && decoded.sculpt.bodies[0].hasEdits);
        r.check("FSR1A_06_construction_companion_is_kept_beside_the_sculpt_branch",
                decoded.hasConstruction && decoded.construction.bodies.size() == 2
                        && decoded.construction.bodies[1].shape.kind == PrimitiveKind::Sphere);

        LiveFixture live;
        ProjectLoadReport report;
        const ProjectCodecStatus loaded = live.load(sculptBytes, &report);
        r.check("FSR1A_05_sculpt_document_loads", loaded == ProjectCodecStatus::Ok);
        const SceneObject* sculpted = live.scene.findBody(2);
        const bool frozen = sculpted != nullptr && sculpted->frozenSculpt().mesh.frozen();
        r.check("FSR1A_05_frozen_sculpt_mesh_is_rebuilt",
                frozen && sculpted->frozenSculpt().mesh.vertexCount() == 4
                        && sculpted->frozenSculpt().mesh.indexCount() == 12);
        bool positionsExact = frozen;
        if (frozen) {
            const std::vector<MeshVertex>& vertices = sculpted->frozenSculpt().mesh.vertices();
            for (uint32_t v = 0; v < 4 && positionsExact; ++v) {
                for (int c = 0; c < 3; ++c) {
                    uint32_t left = 0;
                    uint32_t right = 0;
                    const float expected =
                            sculptDoc.sculpt.bodies[0].positions[static_cast<size_t>(v) * 3 + c];
                    const float actual = vertices[v].position[c];
                    std::memcpy(&left, &expected, 4);
                    std::memcpy(&right, &actual, 4);
                    if (left != right) {
                        positionsExact = false;
                        break;
                    }
                }
            }
        }
        r.check("FSR1A_05_sculpt_vertex_float32_bits_survive_the_roundtrip", positionsExact);
        r.check("FSR1A_05_sculpt_index_topology_survives",
                frozen && sculpted->frozenSculpt().mesh.indices()
                                  == sculptDoc.sculpt.bodies[0].indices);
        r.check("FSR1A_05_source_stale_state_survives",
                sculpted != nullptr && sculpted->frozenSculpt().sourceStale);
        // The destructive Reset-Sculpt-from-Shape guard asks this. A reopened
        // project that reported an unedited mesh would let that reset discard
        // every stroke in the file without a word of warning.
        r.check("FSR1A_05_a_reopened_sculpt_mesh_still_reports_user_edits",
                frozen && sculpted->frozenSculpt().mesh.hasEdits());
        r.check("FSR1A_05_derived_adjacency_and_normals_are_rebuilt",
                frozen && sculpted->frozenSculpt().mesh.topology().built()
                        && sculpted->frozenSculpt().mesh.vertexNormals().size() == 4);
        r.check("FSR1A_06_reopen_mode_is_sculpt", live.session.inSculptMode());
        r.check("FSR1A_06_active_body_is_the_sculpted_one", live.scene.activeBodyId() == 2);
        const RuntimeMeshPtr active = live.scene.activeBody().meshStore().current();
        r.check("FSR1A_06_active_representation_is_the_sculpt_mesh",
                active && active->vertexCount() == 4 && active->indexCount() == 12);
        const SceneObject* companion = live.scene.findBody(1);
        r.check("FSR1A_06_the_other_body_keeps_its_construction_representation",
                companion != nullptr
                        && companion->construction().kind() == PrimitiveKind::Box
                        && !companion->frozenSculpt().mesh.frozen()
                        && companion->meshStore().current() != nullptr);
        r.check("FSR1A_06_sculpting_did_not_write_the_construction_source",
                sculpted != nullptr && sculpted->construction().kind() == PrimitiveKind::Sphere
                        && sculpted->construction().sphere().diameterMeters() == 1.5);

        // The same file opened as a Construction project keeps the sculpt data
        // and comes back in Construction mode: neither branch overwrites the
        // other, and the header's ProjectKind is the only thing that decides.
        ProjectDocument asConstruction = sculptDoc;
        asConstruction.kind = ProjectKind::Construction;
        const std::vector<uint8_t> mixedBytes = encodeProjectV1(asConstruction);
        LiveFixture mixedLive;
        ProjectLoadReport mixedReport;
        const ProjectCodecStatus mixedLoaded = mixedLive.load(mixedBytes, &mixedReport);
        const SceneObject* mixedBody = mixedLive.scene.findBody(2);
        r.check("FSR1A_06_mixed_construction_project_keeps_the_sculpt_branch",
                mixedLoaded == ProjectCodecStatus::Ok && mixedBody != nullptr
                        && mixedBody->frozenSculpt().mesh.frozen()
                        && mixedBody->frozenSculpt().sourceStale);
        r.check("FSR1A_06_mixed_construction_project_reopens_in_construction",
                !mixedLive.session.inSculptMode());
    }

    // -----------------------------------------------------------------------
    // FSR1A-07: a bad CRC is refused, and the live project does not move
    // -----------------------------------------------------------------------
    {
        std::vector<uint8_t> corrupt = constructionBytes;
        // One bit inside the SCNE payload: the length, the count and every
        // section header stay exactly right, so ONLY the checksum can catch it.
        corrupt[kForgeHeaderBytes + kForgeSectionHeaderBytes + 5] ^= 0x01u;
        ProjectDocument decoded;
        r.check("FSR1A_07_bad_crc_is_refused",
                decodeProject(corrupt.data(), corrupt.size(), &decoded)
                        == ProjectCodecStatus::ChecksumMismatch);

        LiveFixture live;
        const ObjectId activeBefore = live.scene.activeBodyId();
        const size_t bodiesBefore = live.scene.bodyCount();
        const ConstructionObjectState shapeBefore =
                live.scene.activeBody().construction().captureState();
        const size_t undoBefore = live.history.undoDepth();
        const bool sculptBefore = live.session.inSculptMode();
        ProjectLoadReport report;
        const ProjectCodecStatus status = live.load(corrupt, &report);
        r.check("FSR1A_07_a_failed_load_reports_the_reason",
                status == ProjectCodecStatus::ChecksumMismatch);
        r.check("FSR1A_07_a_failed_load_leaves_the_scene_untouched",
                live.scene.bodyCount() == bodiesBefore
                        && live.scene.activeBodyId() == activeBefore);
        r.check("FSR1A_07_a_failed_load_leaves_the_active_body_untouched",
                sameConstructionShape(live.scene.activeBody().construction().captureState(),
                                      shapeBefore)
                        && sameConstructionPlacement(
                                   live.scene.activeBody().construction().captureState(),
                                   shapeBefore));
        r.check("FSR1A_07_a_failed_load_leaves_the_mode_untouched",
                live.session.inSculptMode() == sculptBefore);
        r.check("FSR1A_13_a_failed_load_preserves_session_history",
                live.history.undoDepth() == undoBefore && undoBefore == 2);
    }

    // -----------------------------------------------------------------------
    // FSR1A-08: truncation, impossible lengths and overflow
    // -----------------------------------------------------------------------
    {
        ProjectDocument decoded;
        std::vector<uint8_t> shorter(constructionBytes.begin(), constructionBytes.end() - 40);
        r.check("FSR1A_08_a_truncated_file_is_refused",
                decodeProject(shorter.data(), shorter.size(), &decoded)
                        == ProjectCodecStatus::Truncated);
        r.check("FSR1A_08_an_empty_file_is_refused",
                decodeProject(nullptr, 0, &decoded) == ProjectCodecStatus::Truncated);

        // A file whose header claims more bytes than it has. Caught before any
        // section header is even read.
        std::vector<uint8_t> lying = constructionBytes;
        lying[20] = static_cast<uint8_t>(lying[20] + 1u);
        r.check("FSR1A_08_an_inconsistent_file_length_is_refused",
                decodeProject(lying.data(), lying.size(), &decoded)
                        == ProjectCodecStatus::Truncated);

        // A section length that runs past the end of the file.
        std::vector<uint8_t> longSection = constructionBytes;
        const size_t lengthField = kForgeHeaderBytes + 8;
        longSection[lengthField + 4] = 0x10u;  // payloadBytes now ~68 GB
        r.check("FSR1A_08_a_section_length_past_the_end_is_refused",
                decodeProject(longSection.data(), longSection.size(), &decoded)
                        == ProjectCodecStatus::Truncated);

        // An impossible body count, refused BEFORE anything is allocated for it.
        // The CRC is re-stamped afterwards on purpose: this case is about the
        // COUNT, and a checksum refusal would prove nothing about the bound.
        std::vector<uint8_t> hugeCount = constructionBytes;
        {
            uint64_t payloadBytes = 0;
            size_t headerStart = 0;
            const size_t payloadStart =
                    findSectionPayload(hugeCount, kSectionTagScene, &payloadBytes, &headerStart);
            for (int i = 0; i < 4; ++i) {
                hugeCount[payloadStart + static_cast<size_t>(i)] = 0xFFu;
            }
            restampSectionCrc(hugeCount, headerStart, payloadStart, payloadBytes);
        }
        r.check("FSR1A_08_an_impossible_body_count_is_refused_before_allocation",
                decodeProject(hugeCount.data(), hugeCount.size(), &decoded)
                        == ProjectCodecStatus::ImpossibleCount);

        // A sculpt vertex count large enough that multiplying it by the 12
        // bytes each vertex occupies would wrap a 32-bit computation. The
        // decoder widens to 64 bits before it multiplies and compares against
        // what the section window actually still holds, so this is refused
        // rather than turned into a small allocation and a wild read.
        std::vector<uint8_t> hugeVertices = sculptBytes;
        {
            uint64_t payloadBytes = 0;
            size_t headerStart = 0;
            const size_t payloadStart =
                    findSectionPayload(hugeVertices, kSectionTagSculpt, &payloadBytes,
                                       &headerStart);
            // entryCount (4) + objectId (8) + flags (1) reaches the entry's
            // vertexCount field, exactly as DATA_PACKAGE_SPEC.md lays it out.
            const size_t vertexCountField = payloadStart + 4 + 8 + 1;
            hugeVertices[vertexCountField + 0] = 0xFFu;
            hugeVertices[vertexCountField + 1] = 0xFFu;
            hugeVertices[vertexCountField + 2] = 0xFFu;
            hugeVertices[vertexCountField + 3] = 0x3Fu;  // ~1.07e9 vertices
            restampSectionCrc(hugeVertices, headerStart, payloadStart, payloadBytes);
        }
        r.check("FSR1A_08_a_vertex_count_that_would_overflow_is_refused",
                decodeProject(hugeVertices.data(), hugeVertices.size(), &decoded)
                        == ProjectCodecStatus::ImpossibleCount);
    }

    // -----------------------------------------------------------------------
    // FSR1A-09: version compatibility
    // -----------------------------------------------------------------------
    {
        ProjectDocument decoded;
        std::vector<uint8_t> newerMajor = constructionBytes;
        newerMajor[8] = 2;
        r.check("FSR1A_09_a_newer_major_is_refused_explicitly",
                decodeProject(newerMajor.data(), newerMajor.size(), &decoded)
                        == ProjectCodecStatus::UnsupportedMajor);

        std::vector<uint8_t> newerMinor = constructionBytes;
        newerMinor[10] = 7;
        r.check("FSR1A_09_a_newer_minor_of_the_same_major_is_accepted",
                decodeProject(newerMinor.data(), newerMinor.size(), &decoded)
                        == ProjectCodecStatus::Ok);

        // ...but only while every REQUIRED section version is understood.
        std::vector<uint8_t> newerRequiredSection = newerMinor;
        newerRequiredSection[kForgeHeaderBytes + 4] = 2;  // SCNE sectionVersion
        r.check("FSR1A_09_a_newer_required_section_version_is_refused",
                decodeProject(newerRequiredSection.data(), newerRequiredSection.size(), &decoded)
                        == ProjectCodecStatus::UnsupportedSectionVersion);

        std::vector<uint8_t> badHeaderBytes = constructionBytes;
        badHeaderBytes[12] = 32;
        r.check("FSR1A_09_a_header_of_the_wrong_size_is_refused",
                decodeProject(badHeaderBytes.data(), badHeaderBytes.size(), &decoded)
                        == ProjectCodecStatus::BadHeader);

        std::vector<uint8_t> notForge = constructionBytes;
        notForge[0] = 'G';
        r.check("FSR1A_09_a_file_that_is_not_a_forge_file_is_refused",
                decodeProject(notForge.data(), notForge.size(), &decoded)
                        == ProjectCodecStatus::NotForgeFile);
    }

    // -----------------------------------------------------------------------
    // FSR1A-10: unknown sections, and duplicates
    // -----------------------------------------------------------------------
    {
        ProjectDocument decoded;
        uint32_t skipped = 0;
        const std::vector<uint8_t> optional = withExtraSection(constructionBytes, false);
        const ProjectCodecStatus optionalStatus =
                decodeProject(optional.data(), optional.size(), &decoded, &skipped);
        r.check("FSR1A_10_an_unknown_optional_section_is_skipped",
                optionalStatus == ProjectCodecStatus::Ok && skipped == 1);
        r.check("FSR1A_10_skipping_an_optional_section_changes_nothing_else",
                sameProjectDocument(decoded, constructionDoc));

        const std::vector<uint8_t> required = withExtraSection(constructionBytes, true);
        r.check("FSR1A_10_an_unknown_required_section_is_refused",
                decodeProject(required.data(), required.size(), &decoded)
                        == ProjectCodecStatus::UnknownRequiredSection);

        // A second SCNE, valid in every respect except that there can only be
        // one. Built by appending a copy of the real one.
        std::vector<uint8_t> duplicate = constructionBytes;
        const uint64_t scnePayload = readU64(constructionBytes, kForgeHeaderBytes + 8);
        const size_t scneTotal = kForgeSectionHeaderBytes + static_cast<size_t>(scnePayload);
        duplicate.insert(duplicate.end(), constructionBytes.begin() + kForgeHeaderBytes,
                         constructionBytes.begin() + kForgeHeaderBytes
                                 + static_cast<std::ptrdiff_t>(scneTotal));
        for (int i = 0; i < 4; ++i) {
            duplicate[16 + static_cast<size_t>(i)] =
                    static_cast<uint8_t>((3u >> (i * 8)) & 0xFFu);
        }
        const uint64_t duplicateBytes = duplicate.size();
        for (int i = 0; i < 8; ++i) {
            duplicate[20 + static_cast<size_t>(i)] =
                    static_cast<uint8_t>((duplicateBytes >> (i * 8)) & 0xFFu);
        }
        r.check("FSR1A_10_a_duplicate_required_singleton_section_is_refused",
                decodeProject(duplicate.data(), duplicate.size(), &decoded)
                        == ProjectCodecStatus::DuplicateSection);

        std::vector<uint8_t> badFlags = constructionBytes;
        badFlags[kForgeHeaderBytes + 6] = 0x04u;  // a reserved section flag bit
        r.check("FSR1A_10_a_reserved_section_flag_bit_is_refused",
                decodeProject(badFlags.data(), badFlags.size(), &decoded)
                        == ProjectCodecStatus::BadSectionHeader);

        std::vector<uint8_t> badReserved = constructionBytes;
        badReserved[kForgeHeaderBytes + 20] = 0x01u;
        r.check("FSR1A_10_a_non_zero_section_reserved_word_is_refused",
                decodeProject(badReserved.data(), badReserved.size(), &decoded)
                        == ProjectCodecStatus::BadSectionHeader);

        std::vector<uint8_t> badHeaderFlags = constructionBytes;
        badHeaderFlags[15] = 0x80u;
        r.check("FSR1A_10_a_reserved_header_flag_bit_is_refused",
                decodeProject(badHeaderFlags.data(), badHeaderFlags.size(), &decoded)
                        == ProjectCodecStatus::BadHeader);

        std::vector<uint8_t> lyingFlags = constructionBytes;
        lyingFlags[15] = kHeaderFlagHasConstruction | kHeaderFlagHasSculpt;
        r.check("FSR1A_10_a_header_flag_that_no_section_backs_is_refused",
                decodeProject(lyingFlags.data(), lyingFlags.size(), &decoded)
                        == ProjectCodecStatus::BadHeader);

        std::vector<uint8_t> badKind = constructionBytes;
        badKind[14] = 9;
        r.check("FSR1A_10_an_unknown_project_kind_is_refused",
                decodeProject(badKind.data(), badKind.size(), &decoded)
                        == ProjectCodecStatus::BadHeader);

        // A duplicate is a duplicate whatever version it claims. Without this,
        // a second CONS at a version the reader does not understand would be
        // skipped and the singleton rule would have a hole in it.
        std::vector<uint8_t> duplicateNewer = duplicate;
        // The second SCNE begins exactly where the original file ended, so its
        // sectionVersion is four bytes past that.
        duplicateNewer[constructionBytes.size() + 4] = 9;
        r.check("FSR1A_10_a_duplicate_section_at_another_version_is_still_a_duplicate",
                decodeProject(duplicateNewer.data(), duplicateNewer.size(), &decoded)
                        == ProjectCodecStatus::DuplicateSection);

        // A Sculpt project's CONS companion is OPTIONAL, so a version this
        // build cannot read is skipped rather than refused — and the project
        // still opens, still in Sculpt, on the right body. That is the whole
        // point of the companion being optional.
        std::vector<uint8_t> newerCompanion = sculptBytes;
        {
            uint64_t payloadBytes = 0;
            size_t headerStart = 0;
            findSectionPayload(newerCompanion, kSectionTagConstruction, &payloadBytes,
                               &headerStart);
            newerCompanion[headerStart + 4] = 9;  // CONS sectionVersion; not covered by the CRC
        }
        uint32_t companionSkipped = 0;
        ProjectDocument withoutCompanion;
        const ProjectCodecStatus companionStatus =
                decodeProject(newerCompanion.data(), newerCompanion.size(), &withoutCompanion,
                              &companionSkipped);
        r.check("FSR1A_09_an_optional_companion_at_a_newer_version_is_skipped_not_refused",
                companionStatus == ProjectCodecStatus::Ok && companionSkipped == 1
                        && !withoutCompanion.hasConstruction && withoutCompanion.hasSculpt
                        && withoutCompanion.kind == ProjectKind::Sculpt);

        // ...but THIS runtime cannot build a body with no Construction Source,
        // and says so instead of inventing a default Box for a body whose real
        // shape the file described. The live project is untouched by the
        // refusal, exactly like every other one.
        LiveFixture live;
        const size_t bodiesBefore = live.scene.bodyCount();
        const size_t undoBefore = live.history.undoDepth();
        r.check("FSR1A_09_a_document_this_build_cannot_evaluate_is_refused_at_the_load",
                loadProjectDocument(withoutCompanion, live.scene, live.session, live.history)
                                == ProjectCodecStatus::MissingRequiredSection
                        && live.scene.bodyCount() == bodiesBefore
                        && live.history.undoDepth() == undoBefore);
    }

    // -----------------------------------------------------------------------
    // FSR1A-11: invalid semantics, refused by the domain's own contracts
    // -----------------------------------------------------------------------
    {
        ProjectDocument bad = constructionDoc;
        bad.construction.bodies[0].shape.box.width = -1.0;
        r.check("FSR1A_11_a_negative_dimension_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
        r.check("FSR1A_11_an_invalid_document_encodes_nothing",
                encodeProjectV1(bad).empty());

        bad = constructionDoc;
        // A capsule whose two hemispherical ends alone are already taller than
        // its total height: every value is a length, and the RELATION is broken.
        bad.construction.bodies[0].shape.capsule = CapsuleDimensionsMeters{2.0, 1.0};
        r.check("FSR1A_11_a_broken_capsule_relation_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.bodies[0].transform.scaleY = 0.0;
        r.check("FSR1A_11_a_zero_scale_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.bodies[0].transform.scaleY = -1.0;
        r.check("FSR1A_11_a_mirroring_negative_scale_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.bodies[2].transform.positionX = std::numeric_limits<double>::quiet_NaN();
        r.check("FSR1A_11_a_non_finite_position_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.bodies[3].objectId = 1;
        r.check("FSR1A_11_a_duplicate_object_id_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.bodies[0].objectId = kNoObject;
        r.check("FSR1A_11_the_reserved_no_object_id_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.nextObjectId = 6;
        r.check("FSR1A_11_an_allocator_that_could_mint_a_collision_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = constructionDoc;
        bad.scene.activeObjectId = 99;
        r.check("FSR1A_11_an_active_body_no_section_carries_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::UnresolvedReference);

        bad = constructionDoc;
        bad.construction.bodies[2].objectId = 42;
        r.check("FSR1A_11_a_cons_body_scne_does_not_carry_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::UnresolvedReference);

        bad = constructionDoc;
        bad.construction.bodies[2].features.clear();
        r.check("FSR1A_11_a_body_with_no_primitive_source_feature_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = sculptDoc;
        bad.sculpt.bodies[0].indices[0] = 4;  // only four vertices exist
        r.check("FSR1A_11_a_sculpt_index_out_of_range_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);

        bad = sculptDoc;
        bad.sculpt.bodies[0].indices.pop_back();
        r.check("FSR1A_11_an_index_count_that_is_not_triangles_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::ImpossibleCount);

        bad = sculptDoc;
        bad.sculpt.bodies[0].objectId = 7;
        r.check("FSR1A_11_a_scul_body_scne_does_not_carry_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::UnresolvedReference);

        bad = sculptDoc;
        bad.scene.activeObjectId = 1;  // a Sculpt project whose active body has no mesh
        r.check("FSR1A_11_a_sculpt_project_whose_active_body_is_not_sculpted_is_refused",
                validateProjectDocument(bad) == ProjectCodecStatus::UnresolvedReference);
    }

    // -----------------------------------------------------------------------
    // FSR1A-12: the golden corpus, and the absence of a fabricated predecessor
    // -----------------------------------------------------------------------
    {
        // These two digests are what scripts/build-forge-corpus.ps1 wrote into
        // testdata/forge/v1/ and what DATA_PACKAGE_SPEC.md records. A change to
        // the encoder that nobody meant to make fails HERE, with the new value
        // printed beside it, rather than silently invalidating the corpus.
        r.check("FSR1A_12_canonical_construction_fixture_matches_the_committed_digest",
                g_constructionSha
                        == "8830e7fbd8dcb803535d5c4f91e410dfca4c7a0cecc1202c9d2b1aa8553b9aaf");
        r.check("FSR1A_12_canonical_sculpt_fixture_matches_the_committed_digest",
                g_sculptSha
                        == "112b109731a43bf57a0f77b34794e7ce2529e056d9b18f061cd3c891f51a2784");
        // The dispatch seam exists and answers for exactly one version. There
        // has never been a production format before v1, so there is nothing to
        // migrate FROM and no v0 branch is claimed.
        r.check("FSR1A_12_the_only_supported_major_is_1", kForgeVersionMajor == 1);
        ProjectDocument decoded;
        r.check("FSR1A_12_the_corpus_files_stay_loadable",
                decodeProject(constructionBytes.data(), constructionBytes.size(), &decoded)
                                == ProjectCodecStatus::Ok
                        && decodeProject(sculptBytes.data(), sculptBytes.size(), &decoded)
                                   == ProjectCodecStatus::Ok);
    }

    // -----------------------------------------------------------------------
    // FSR1A-13: a successful load starts fresh session history
    // -----------------------------------------------------------------------
    {
        LiveFixture live;
        r.check("FSR1A_13_the_fixture_starts_with_recorded_history",
                live.history.undoDepth() == 2 && live.history.canUndo());
        live.history.undo();
        r.check("FSR1A_13_the_fixture_has_something_to_redo", live.history.canRedo());
        ProjectLoadReport report;
        const ProjectCodecStatus status = live.load(constructionBytes, &report);
        r.check("FSR1A_13_a_successful_load_clears_both_history_stacks",
                status == ProjectCodecStatus::Ok && live.history.undoDepth() == 0
                        && live.history.redoDepth() == 0 && !live.history.canUndo()
                        && !live.history.canRedo());
        // And the fresh history works over the LOADED scene, not the old one.
        {
            ScopedConstructionEdit edit(live.history);
            applyPrimitive(live.scene.activeBody().construction(),
                           live.scene.activeBody().meshStore(),
                           PrimitiveSpec::forSphere(4.5));
        }
        r.check("FSR1A_13_a_post_load_edit_records_exactly_one_step",
                live.history.undoDepth() == 1);
        live.history.undo();
        r.check("FSR1A_13_a_post_load_undo_returns_the_loaded_value",
                live.scene.activeBody().construction().kind() == PrimitiveKind::Cone);

        // A load is refused outright while an edit is open, and refusing it
        // changes nothing.
        LiveFixture busy;
        busy.history.beginEdit();
        const size_t bodiesBefore = busy.scene.bodyCount();
        ProjectDocument document;
        decodeProject(constructionBytes.data(), constructionBytes.size(), &document);
        const ProjectCodecStatus refused =
                loadProjectDocument(document, busy.scene, busy.session, busy.history);
        r.check("FSR1A_13_a_load_during_an_open_edit_is_refused",
                refused == ProjectCodecStatus::RefusedEditInProgress
                        && busy.scene.bodyCount() == bodiesBefore);
        busy.history.commitEdit();
    }

    // -----------------------------------------------------------------------
    // FSR1A-14 / FSR1A-15: the file is the format, not this machine
    // -----------------------------------------------------------------------
    {
        // Not a struct image. Every record's on-disk size is the arithmetic the
        // FORMAT specifies, and it is independent of sizeof(TransformValues),
        // sizeof(ConstructionObjectState), the enum ABI, the pointer width and
        // any padding this compiler chose.
        const uint64_t scnePayload = readU64(constructionBytes, kForgeHeaderBytes + 8);
        // Not a struct image. The section is exactly the arithmetic the FORMAT
        // specifies, and the fields inside it are at the offsets the format
        // names -- neither of which is derived from any C++ type: sizeof and
        // alignof of the types that hold these values do not appear in the
        // encoder at all, and the same file is produced whatever this compiler
        // chose for padding.
        r.check("FSR1A_14_the_scene_record_is_the_format_arithmetic",
                scnePayload == 4 + 8 + 8 + 6 * 80);
        // The CONS record is the case that would give a struct image away: a
        // body is 8 + 1 + 96 + 4 + 5 bytes, which no aligned C++ layout of an
        // ObjectId, an enum and twelve doubles can produce -- the enum and the
        // feature-kind byte would both be padded out.
        const uint64_t consPayload =
                readU64(constructionBytes,
                        kForgeHeaderBytes + kForgeSectionHeaderBytes
                                + static_cast<size_t>(scnePayload) + 8);
        r.check("FSR1A_14_the_construction_record_has_no_host_padding",
                consPayload == 4 + 6 * 114
                        && consPayload
                                   != 4 + 6 * (8 + sizeof(ConstructionObjectState) + 4 + 8));
        r.check("FSR1A_15_primitive_codes_are_file_owned_not_the_enum_abi",
                primitiveFileCode(PrimitiveKind::Box) == 1
                        && primitiveFileCode(PrimitiveKind::Plane) == 6
                        && static_cast<int>(PrimitiveKind::Box) == 0);
        PrimitiveKind decodedKind = PrimitiveKind::Plane;
        r.check("FSR1A_15_an_unknown_primitive_code_is_refused",
                !primitiveKindFromFileCode(0, &decodedKind)
                        && !primitiveKindFromFileCode(7, &decodedKind)
                        && primitiveKindFromFileCode(3, &decodedKind)
                        && decodedKind == PrimitiveKind::Sphere);
        // Every scalar is written as its own fixed-width little-endian field.
        // Reading the first body's position X back out by hand -- from the byte
        // offset the specification names, with no host type involved -- must
        // give exactly the value that went in.
        const size_t firstBody = kForgeHeaderBytes + kForgeSectionHeaderBytes + 4 + 8 + 8;
        const uint64_t bits = readU64(constructionBytes, firstBody + 8);
        double positionX = 0.0;
        std::memcpy(&positionX, &bits, sizeof(positionX));
        r.check("FSR1A_15_a_binary64_field_reads_back_from_its_specified_offset",
                readU64(constructionBytes, firstBody) == 1u && positionX == 0.5);
        r.check("FSR1A_15_the_file_carries_no_host_pointer_width",
                sizeof(kForgeHeaderBytes) == 2 && kForgeHeaderBytes == 28
                        && kForgeSectionHeaderBytes == 24);
    }

    return r.n;
}

}  // namespace forgeshape
