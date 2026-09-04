#include "forgeshape_project_selftest.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "forgeshape_cad_face.h"
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

// The one Imported Mesh every imported golden fixture carries.
//
// Four vertices, two submeshes with DIFFERENT `doubleSided` answers, and every
// number an exact binary fraction — so the independent PowerShell encoder has
// no rounding argument to make and the two implementations agree byte for byte
// or not at all.
ProjectImportedBody canonicalImportedBody(ObjectId objectId) {
    ProjectImportedBody body;
    body.objectId = objectId;
    body.name = "head_low";
    body.positions = {0.0f, 0.0f,  0.0f,
                      1.5f, 0.0f,  0.0f,
                      0.0f, 2.25f, 0.0f,
                      0.0f, 0.0f,  3.5f};
    body.normals = {0.0f, 0.0f, 1.0f,
                    0.0f, 1.0f, 0.0f,
                    1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, -1.0f};
    body.indices = {0, 1, 2, 0, 2, 3};
    body.batches = {ImportedMeshBatch{0, 3, false}, ImportedMeshBatch{3, 3, true}};
    return body;
}

// The placement an imported golden fixture's object sits at: the translation
// its source node stated, and nothing else. Rotation and scale are the identity
// because an import bakes the node's linear part into the geometry.
TransformValues canonicalImportedPlacement() {
    return placement(1.5, -0.25, 4.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
}

// The six remembered parameter sets both mixed fixtures give every Construction
// Body. Shared so the fixtures differ in the thing under test — which branches
// are present — rather than in incidental numbers.
ConstructionObjectState canonicalSharedShape(PrimitiveKind kind) {
    ConstructionObjectState shape;
    shape.kind = kind;
    shape.box = BoxDimensionsMeters{2.0, 1.0, 0.5};
    shape.cylinder = CylinderDimensionsMeters{1.0, 2.0};
    shape.sphere = SphereDimensionsMeters{1.5};
    shape.cone = ConeDimensionsMeters{1.0, 2.0};
    shape.capsule = CapsuleDimensionsMeters{1.0, 2.0};
    shape.plane = PlaneDimensionsMeters{2.0, 2.0};
    return shape;
}

// IMPORTED-ONLY: one body, no Construction branch at all.
//
// The fixture that proves an imported object needs no Construction Source
// standing in for it — the file carries SCNE and IMPT and nothing else.
ProjectDocument canonicalImportedOnlyDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;

    ProjectBodyPlacement body;
    body.objectId = 1;
    body.transform = canonicalImportedPlacement();
    document.scene.bodies.push_back(body);

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(1));
    return document;
}

// CONSTRUCTION + IMPORTED: a sparse CONS beside an IMPT.
//
// This is the fixture that pins the generalized CONS rule — one entry per body
// that HAS a Construction Source, in scene order, rather than one per body.
ProjectDocument canonicalConstructionImportedDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;

    ProjectBodyPlacement first;
    first.objectId = 1;
    first.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(first);

    ProjectBodyPlacement second;
    second.objectId = 2;
    second.transform = canonicalImportedPlacement();
    document.scene.bodies.push_back(second);

    document.hasConstruction = true;
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalSharedShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(2));
    return document;
}

// ALL THREE BRANCHES: Construction, Sculpt and Imported in one document.
//
// Three bodies — a plain Construction Body, a Construction Body carrying an
// edited Frozen Sculpt Mesh, and an Imported Mesh — reopening in Sculpt on the
// sculpted one. The scene interleaves the representations rather than grouping
// them, so a reader that assumed a contiguous block of either would fail here.
ProjectDocument canonicalMixedImportedDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Sculpt;
    document.scene.nextObjectId = 4;
    document.scene.activeObjectId = 2;

    ProjectBodyPlacement first;
    first.objectId = 1;
    first.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(first);

    ProjectBodyPlacement second;
    second.objectId = 2;
    second.transform = placement(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(second);

    ProjectBodyPlacement third;
    third.objectId = 3;
    third.transform = canonicalImportedPlacement();
    document.scene.bodies.push_back(third);

    document.hasConstruction = true;
    ProjectConstructionBody firstSource;
    firstSource.objectId = 1;
    firstSource.shape = canonicalSharedShape(PrimitiveKind::Box);
    firstSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(firstSource);

    ProjectConstructionBody secondSource;
    secondSource.objectId = 2;
    secondSource.shape = canonicalSharedShape(PrimitiveKind::Sphere);
    secondSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(secondSource);

    document.hasSculpt = true;
    ProjectSculptBody sculpt;
    sculpt.objectId = 2;
    sculpt.renderBothSides = false;
    sculpt.sourceStale = true;
    sculpt.hasEdits = true;
    sculpt.positions = {0.0f,  0.0f,  0.0f,
                        1.5f,  0.0f,  0.0f,
                        0.0f,  1.25f, 0.0f,
                        0.25f, 0.5f,  1.75f};
    sculpt.indices = {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3};
    document.sculpt.bodies.push_back(sculpt);

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(3));
    return document;
}

// The Frozen Sculpt Mesh both `IMPORT-01B` fixtures put on an IMPORTED body.
//
// A tetrahedron, edited, with every number an exact binary fraction — so the
// independent PowerShell encoder has no rounding argument to make. It is
// deliberately NOT the imported geometry it was frozen from: a sculpt mesh is
// its own positions and its own topology, and a fixture where the two matched
// could not tell a decoder that confused them apart.
ProjectSculptBody canonicalImportedSculptBody(ObjectId objectId) {
    ProjectSculptBody sculpt;
    sculpt.objectId = objectId;
    sculpt.renderBothSides = true;  // the imported source had a two-sided submesh
    // An Imported Mesh is immutable for the life of its body, so nothing can
    // make one stale. The fixture states the false a capture would write.
    sculpt.sourceStale = false;
    sculpt.hasEdits = true;
    sculpt.positions = {0.25f, 0.0f,  0.0f,
                        1.75f, 0.0f,  0.0f,
                        0.0f,  2.5f,  0.0f,
                        0.5f,  0.75f, 3.25f};
    sculpt.indices = {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3};
    return sculpt;
}

// IMPORTED + SCULPT: one body, geometry from a file, sculpted, reopening in
// Sculpt.
//
// The `IMPORT-01B` fixture, and the one that proves the generalized SCUL rule:
// a sculpt entry's body may have `CONS` or `IMPT` as its source. There is no
// `CONS` section here at all, so a reader that still required one — every build
// before `IMPORT-01B` did — refuses this file rather than opening half of it.
ProjectDocument canonicalImportedSculptDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Sculpt;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;

    ProjectBodyPlacement body;
    body.objectId = 1;
    body.transform = canonicalImportedPlacement();
    document.scene.bodies.push_back(body);

    document.hasSculpt = true;
    document.sculpt.bodies.push_back(canonicalImportedSculptBody(1));

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(1));
    return document;
}

// ALL FOUR COMBINATIONS in one document, which is the whole of the B1 matrix:
// a plain Construction Body, a Construction Body with a sculpt mesh, a plain
// Imported Mesh, and an Imported Mesh with a sculpt mesh.
//
// The representations interleave and the SCUL entries sit over bodies of BOTH
// source kinds, so a reader that assumed a contiguous block of either, or that
// keyed a sculpt entry to a Construction body, fails here.
ProjectDocument canonicalMixedImportedSculptDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Sculpt;
    document.scene.nextObjectId = 5;
    document.scene.activeObjectId = 4;

    ProjectBodyPlacement first;
    first.objectId = 1;
    first.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(first);

    ProjectBodyPlacement second;
    second.objectId = 2;
    second.transform = placement(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(second);

    ProjectBodyPlacement third;
    third.objectId = 3;
    third.transform = canonicalImportedPlacement();
    document.scene.bodies.push_back(third);

    ProjectBodyPlacement fourth;
    fourth.objectId = 4;
    fourth.transform = placement(-2.5, 1.25, 0.5, 0.0, 45.0, 0.0, 1.0, 2.0, 1.0);
    document.scene.bodies.push_back(fourth);

    document.hasConstruction = true;
    ProjectConstructionBody firstSource;
    firstSource.objectId = 1;
    firstSource.shape = canonicalSharedShape(PrimitiveKind::Box);
    firstSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(firstSource);

    ProjectConstructionBody secondSource;
    secondSource.objectId = 2;
    secondSource.shape = canonicalSharedShape(PrimitiveKind::Sphere);
    secondSource.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(secondSource);

    document.hasSculpt = true;
    ProjectSculptBody constructionSculpt;
    constructionSculpt.objectId = 2;
    constructionSculpt.renderBothSides = false;
    constructionSculpt.sourceStale = true;
    constructionSculpt.hasEdits = true;
    constructionSculpt.positions = {0.0f,  0.0f,  0.0f,
                                    1.5f,  0.0f,  0.0f,
                                    0.0f,  1.25f, 0.0f,
                                    0.25f, 0.5f,  1.75f};
    constructionSculpt.indices = {0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3};
    document.sculpt.bodies.push_back(constructionSculpt);
    document.sculpt.bodies.push_back(canonicalImportedSculptBody(4));

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(3));
    document.imported.bodies.push_back(canonicalImportedBody(4));
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
// ---------------------------------------------------------------------------
// The Imported Mesh fixture
// ---------------------------------------------------------------------------
//
// Deliberately tiny and deliberately ASYMMETRIC in every dimension the format
// carries: four vertices, two submeshes with DIFFERENT `doubleSided` answers,
// and a placement no default could produce. A fixture whose two batches agreed,
// or whose geometry was symmetric, would pass a roundtrip that had lost the very
// thing being asserted.
constexpr const char* kImportedFixtureName = "head_low";

TransformValues importedFixturePlacement() {
    TransformValues values;
    values.positionX = 1.5;
    values.positionY = -0.25;
    values.positionZ = 4.0;
    return values;
}

struct ImportedFixture {
    // Four vertices, no two alike, so a dropped or reordered one shows.
    std::vector<float> positions{0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 3.0f};
    // Unit directions, which is what the domain requires of a stored normal.
    std::vector<float> normals{0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
                               1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f};
    std::vector<uint32_t> indices{0, 1, 2, 0, 2, 3};
    // Two submeshes, and only the second is double-sided: one answer for the
    // whole mesh could not tell them apart.
    std::vector<ImportedMeshBatch> batches{ImportedMeshBatch{0, 3, false},
                                           ImportedMeshBatch{3, 3, true}};

    ConstructionScene scene;

    ImportedFixture() {
        // The scene's first Body is a Construction Box, which this fixture is
        // not about. It is detached rather than left in place so the document
        // under test is genuinely imported-ONLY — the case that proves an
        // imported body needs no Construction Source beside it.
        SceneObject* added = scene.addImportedBody(mesh(), kImportedFixtureName);
        added->transform().setValues(importedFixturePlacement());
        publishSceneObject(*added);
        scene.detachBody(scene.bodyAt(0).objectId());
        scene.setActiveBody(scene.bodyAt(0).objectId());
    }

    ImportedMesh mesh() const {
        return ImportedMesh::build(positions, normals, indices, batches);
    }
};

// Recomputes the CRC of the LAST section in a file, after its payload was
// edited by hand.
//
// A test that flipped a payload byte without this would only ever prove the
// checksum works. Recomputing it is what lets the case underneath assert the
// rule it is actually about.
void rewriteLastSectionCrc(std::vector<uint8_t>* file) {
    size_t at = kForgeHeaderBytes;
    size_t lastHeader = 0;
    size_t lastPayload = 0;
    while (at + kForgeSectionHeaderBytes <= file->size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &(*file)[at + 8], sizeof(payloadBytes));
        lastHeader = at;
        lastPayload = static_cast<size_t>(payloadBytes);
        at += kForgeSectionHeaderBytes + lastPayload;
    }
    const uint32_t crc =
            crc32IsoHdlc(&(*file)[lastHeader + kForgeSectionHeaderBytes], lastPayload);
    std::memcpy(&(*file)[lastHeader + 16], &crc, sizeof(crc));
}

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

// ---------------------------------------------------------------------------
// The four `CAD-R0-A1A2` fixtures
// ---------------------------------------------------------------------------
//
// Every coordinate, size and depth is an exact binary fraction, so the
// independent PowerShell encoder has no rounding argument to make.

// CAD RECTANGLE: one body, a rectangle on the XZ plane extruded along +Y,
// at a placement that includes an uncanonicalized 370 degrees and a
// non-uniform scale. No CONS at all: the fixture that proves a CAD Body needs
// no Construction Source standing in for it.
ProjectDocument canonicalCadRectangleDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;

    ProjectBodyPlacement body;
    body.objectId = 1;
    body.transform = placement(0.5, -0.25, 1.25, 370.0, -45.5, 12.25, 1.25, 2.0, 0.5);
    document.scene.bodies.push_back(body);

    document.hasCad = true;
    ProjectCadBody cad;
    cad.objectId = 1;
    cad.state.sketch.plane = Workplane::XZ;
    SketchRectangle rectangle;
    rectangle.center = SketchPoint{0.5, 0.25};
    rectangle.width = 2.0;
    rectangle.height = 1.0;
    cad.state.sketch.entities.emplace_back(1, rectangle);
    cad.state.sketch.nextEntityId = 2;
    cad.state.extrude.profileEntityId = 1;
    cad.state.extrude.depth = 1.5;
    cad.state.extrude.direction = ExtrudeDirection::AlongNormal;
    document.cad.bodies.push_back(cad);
    return document;
}

// CAD CIRCLE: one body, a circle on the YZ plane extruded AGAINST its normal.
ProjectDocument canonicalCadCircleDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 2;
    document.scene.activeObjectId = 1;

    ProjectBodyPlacement body;
    body.objectId = 1;
    body.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(body);

    document.hasCad = true;
    ProjectCadBody cad;
    cad.objectId = 1;
    cad.state.sketch.plane = Workplane::YZ;
    SketchCircle circle;
    circle.center = SketchPoint{-0.5, 0.5};
    circle.radius = 0.75;
    cad.state.sketch.entities.emplace_back(1, circle);
    cad.state.sketch.nextEntityId = 2;
    cad.state.extrude.profileEntityId = 1;
    cad.state.extrude.depth = 0.5;
    cad.state.extrude.direction = ExtrudeDirection::AgainstNormal;
    document.cad.bodies.push_back(cad);
    return document;
}

// MIXED CAD: a Construction Body beside two CAD Bodies -- a closed polyline
// profile with an unrelated open line in the same sketch, and a loop of three
// lines -- so a SPARSE CONS sits next to a CADB and the CADB carries every
// entity kind and both profile-closing rules.
ProjectDocument canonicalMixedCadDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 4;
    document.scene.activeObjectId = 2;

    ProjectBodyPlacement first;
    first.objectId = 1;
    first.transform = placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(first);
    ProjectBodyPlacement second;
    second.objectId = 2;
    second.transform = placement(1.5, 0.5, -2.0, 370.0, 0.0, 90.0, 1.0, 1.0, 1.0);
    document.scene.bodies.push_back(second);
    ProjectBodyPlacement third;
    third.objectId = 3;
    third.transform = placement(-2.5, 1.25, 0.5, 0.0, 45.0, 0.0, 1.0, 2.0, 1.0);
    document.scene.bodies.push_back(third);

    document.hasConstruction = true;
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalSharedShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);

    document.hasCad = true;
    {
        ProjectCadBody cad;
        cad.objectId = 2;
        cad.state.sketch.plane = Workplane::XY;
        SketchPolyline polyline;
        polyline.vertices = {SketchPoint{0.0, 0.0}, SketchPoint{2.0, 0.0}, SketchPoint{2.0, 1.0},
                             SketchPoint{1.0, 2.0}, SketchPoint{0.0, 1.0}};
        polyline.closed = true;
        cad.state.sketch.entities.emplace_back(1, polyline);
        cad.state.sketch.entities.emplace_back(
                2, SketchLine{SketchPoint{3.0, 3.0}, SketchPoint{4.0, 4.5}});
        cad.state.sketch.nextEntityId = 3;
        cad.state.extrude.profileEntityId = 1;
        cad.state.extrude.depth = 2.0;
        cad.state.extrude.direction = ExtrudeDirection::AlongNormal;
        document.cad.bodies.push_back(cad);
    }
    {
        ProjectCadBody cad;
        cad.objectId = 3;
        cad.state.sketch.plane = Workplane::XZ;
        cad.state.sketch.entities.emplace_back(
                1, SketchLine{SketchPoint{0.0, 0.0}, SketchPoint{2.0, 0.0}});
        cad.state.sketch.entities.emplace_back(
                2, SketchLine{SketchPoint{2.0, 0.0}, SketchPoint{0.0, 2.0}});
        cad.state.sketch.entities.emplace_back(
                3, SketchLine{SketchPoint{0.0, 2.0}, SketchPoint{0.0, 0.0}});
        cad.state.sketch.nextEntityId = 4;
        cad.state.extrude.profileEntityId = 1;
        cad.state.extrude.depth = 0.25;
        cad.state.extrude.direction = ExtrudeDirection::AlongNormal;
        document.cad.bodies.push_back(cad);
    }
    return document;
}

// ---------------------------------------------------------------------------
// The six `CAD-A3` CADB v2 fixtures
// ---------------------------------------------------------------------------
//
// Every coordinate, size and depth is an exact binary fraction. Each dependent
// body's lineage token is the producer's own topology signature, computed here
// by the domain's `cadTopologySignature` and, in the PowerShell builder, by an
// independent reimplementation of the rule DATA_PACKAGE_SPEC.md §7c states --
// which is what makes the token a FORMAT field rather than an accident of this
// build. A face-supported body's SCNE placement is the unused identity.

ProjectCadBody canonicalCadRectangleBody(ObjectId objectId, Workplane plane, double centreU,
                                         double centreV, double width, double height,
                                         double depth, ExtrudeDirection direction) {
    ProjectCadBody cad;
    cad.objectId = objectId;
    cad.state.sketch.plane = plane;
    SketchRectangle rectangle;
    rectangle.center = SketchPoint{centreU, centreV};
    rectangle.width = width;
    rectangle.height = height;
    cad.state.sketch.entities.emplace_back(1, rectangle);
    cad.state.sketch.nextEntityId = 2;
    cad.state.extrude.profileEntityId = 1;
    cad.state.extrude.depth = depth;
    cad.state.extrude.direction = direction;
    return cad;
}

// Supports `dependent` on `producer`'s face `token`, with the lineage the
// producer's current topology carries.
void supportOn(ProjectCadBody* dependent, const ProjectCadBody& producer,
               const CadFaceToken& token) {
    dependent->state.sketch.plane = Workplane::XY;
    dependent->state.sketch.hasFaceSupport = true;
    dependent->state.sketch.faceSupport.producerObjectId = producer.objectId;
    dependent->state.sketch.faceSupport.producerLocalFeatureId = kCadFeatureId;
    dependent->state.sketch.faceSupport.face = token;
    dependent->state.sketch.faceSupport.lineageToken = cadTopologySignature(producer.state);
}

ProjectBodyPlacement placementOf(ObjectId objectId, const TransformValues& values) {
    ProjectBodyPlacement body;
    body.objectId = objectId;
    body.transform = values;
    return body;
}

// CAD FACE SKETCH CAP: a producer rectangle on XY, translated, and a dependent
// rectangle supported by its far cap.
ProjectDocument canonicalCadFaceSketchCapDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(
            placementOf(1, placement(1.5, 0.5, -2.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)));
    document.scene.bodies.push_back(placementOf(2, TransformValues{}));
    document.hasCad = true;
    const ProjectCadBody producer = canonicalCadRectangleBody(
            1, Workplane::XY, 0.0, 0.0, 2.0, 2.0, 2.0, ExtrudeDirection::AlongNormal);
    ProjectCadBody dependent = canonicalCadRectangleBody(
            2, Workplane::XY, 0.25, -0.25, 1.0, 1.0, 0.5, ExtrudeDirection::AlongNormal);
    supportOn(&dependent, producer, CadFaceToken{CadFaceKind::CapFar, 0, 0});
    document.cad.bodies.push_back(producer);
    document.cad.bodies.push_back(dependent);
    return document;
}

// CAD FACE SKETCH SIDE: the same producer, and a dependent supported by the
// producer's second profile edge (edge entity 1, local index 1) and extruded
// AGAINST the face normal.
ProjectDocument canonicalCadFaceSketchSideDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 3;
    document.scene.activeObjectId = 2;
    document.scene.bodies.push_back(
            placementOf(1, placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)));
    document.scene.bodies.push_back(placementOf(2, TransformValues{}));
    document.hasCad = true;
    const ProjectCadBody producer = canonicalCadRectangleBody(
            1, Workplane::XY, 0.0, 0.0, 2.0, 2.0, 2.0, ExtrudeDirection::AlongNormal);
    ProjectCadBody dependent = canonicalCadRectangleBody(
            2, Workplane::XY, 0.0, 0.0, 0.5, 0.5, 0.25, ExtrudeDirection::AgainstNormal);
    supportOn(&dependent, producer, CadFaceToken{CadFaceKind::Side, 1, 1});
    document.cad.bodies.push_back(producer);
    document.cad.bodies.push_back(dependent);
    return document;
}

// CAD FACE CHAIN: A (world XZ) <- B (on A's far cap) <- C (a circle on B's far
// cap). Three levels, so a reader has to resolve a chain rather than one hop.
ProjectDocument canonicalCadFaceChainDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 4;
    document.scene.activeObjectId = 3;
    document.scene.bodies.push_back(
            placementOf(1, placement(0.0, 0.0, 0.0, 0.0, 45.0, 0.0, 1.0, 1.0, 1.0)));
    document.scene.bodies.push_back(placementOf(2, TransformValues{}));
    document.scene.bodies.push_back(placementOf(3, TransformValues{}));
    document.hasCad = true;
    const ProjectCadBody a = canonicalCadRectangleBody(
            1, Workplane::XZ, 0.0, 0.0, 3.0, 3.0, 1.0, ExtrudeDirection::AlongNormal);
    ProjectCadBody b = canonicalCadRectangleBody(
            2, Workplane::XY, 0.0, 0.0, 1.5, 1.5, 0.5, ExtrudeDirection::AlongNormal);
    supportOn(&b, a, CadFaceToken{CadFaceKind::CapFar, 0, 0});
    ProjectCadBody c;
    c.objectId = 3;
    SketchCircle circle;
    circle.center = SketchPoint{0.0, 0.0};
    circle.radius = 0.5;
    c.state.sketch.entities.emplace_back(1, circle);
    c.state.sketch.nextEntityId = 2;
    c.state.extrude.profileEntityId = 1;
    c.state.extrude.depth = 0.25;
    c.state.extrude.direction = ExtrudeDirection::AlongNormal;
    supportOn(&c, b, CadFaceToken{CadFaceKind::CapFar, 0, 0});
    document.cad.bodies.push_back(a);
    document.cad.bodies.push_back(b);
    document.cad.bodies.push_back(c);
    return document;
}

// MIXED CAD FACE: a Construction Box, an Imported Mesh, a CAD producer and a
// dependent on its far cap -- every representation and the face support in
// one file, with SCNE, CONS, IMPT and a v2 CADB side by side.
ProjectDocument canonicalMixedCadFaceDocument() {
    ProjectDocument document;
    document.kind = ProjectKind::Construction;
    document.scene.nextObjectId = 5;
    document.scene.activeObjectId = 4;
    document.scene.bodies.push_back(
            placementOf(1, placement(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)));
    document.scene.bodies.push_back(placementOf(2, canonicalImportedPlacement()));
    document.scene.bodies.push_back(
            placementOf(3, placement(-2.5, 1.25, 0.5, 0.0, 0.0, 90.0, 1.0, 1.0, 1.0)));
    document.scene.bodies.push_back(placementOf(4, TransformValues{}));

    document.hasConstruction = true;
    ProjectConstructionBody source;
    source.objectId = 1;
    source.shape = canonicalSharedShape(PrimitiveKind::Box);
    source.features.push_back(ProjectFeatureRecord{});
    document.construction.bodies.push_back(source);

    document.hasImported = true;
    document.imported.bodies.push_back(canonicalImportedBody(2));

    document.hasCad = true;
    const ProjectCadBody producer = canonicalCadRectangleBody(
            3, Workplane::XY, 0.0, 0.0, 2.0, 2.0, 1.0, ExtrudeDirection::AlongNormal);
    ProjectCadBody dependent = canonicalCadRectangleBody(
            4, Workplane::XY, 0.0, 0.0, 1.0, 0.5, 0.5, ExtrudeDirection::AlongNormal);
    supportOn(&dependent, producer, CadFaceToken{CadFaceKind::CapFar, 0, 0});
    document.cad.bodies.push_back(producer);
    document.cad.bodies.push_back(dependent);
    return document;
}

// Finds the CADB section's payload in an encoded file, by walking the section
// headers rather than by a hard-coded offset, so the two corrupt v2 fixtures
// below can be derived from a valid one whatever precedes the section.
// Returns the payload's start offset, writing its length, or 0 when absent.
size_t findCadPayload(const std::vector<uint8_t>& file, size_t* outLength) {
    size_t at = kForgeHeaderBytes;
    while (at + kForgeSectionHeaderBytes <= file.size()) {
        uint64_t length = 0;
        for (int i = 0; i < 8; ++i) {
            length |= static_cast<uint64_t>(file[at + 8 + i]) << (8 * i);
        }
        const bool cad = file[at] == 'C' && file[at + 1] == 'A' && file[at + 2] == 'D'
                         && file[at + 3] == 'B';
        if (cad) {
            *outLength = static_cast<size_t>(length);
            return at + kForgeSectionHeaderBytes;
        }
        at += kForgeSectionHeaderBytes + static_cast<size_t>(length);
    }
    *outLength = 0;
    return 0;
}

void writeU32At(std::vector<uint8_t>* bytes, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        (*bytes)[at + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFFu);
    }
}

void writeU64At(std::vector<uint8_t>* bytes, size_t at, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        (*bytes)[at + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFFu);
    }
}

// Recomputes the CADB section's payload CRC after a patch, so every length and
// checksum is right and only the semantic check can refuse the file.
void refreshCadCrc(std::vector<uint8_t>* bytes, size_t payloadAt, size_t payloadLength) {
    const uint32_t crc = crc32IsoHdlc(bytes->data() + payloadAt, payloadLength);
    writeU32At(bytes, payloadAt - kForgeSectionHeaderBytes + 16, crc);
}

// The byte offset of the dependent's face block inside a v2 CADB payload whose
// first body is a world-plane rectangle: bodyCount(4) + body 1 (objectId 8,
// plane 1, supportKind 1, nextEntityId 4, profile 4, direction 1, depth 8,
// entityCount 4, one rectangle entity 4 + 1 + 32) = 72; then body 2's
// objectId(8), plane(1), supportKind(1) = 10 -> the block starts at 82:
// producerObjectId u64 @82, featureId u32 @90, faceKind u8 @94,
// edgeEntityId u32 @95, edgeLocalIndex u32 @99, lineageToken u64 @103.
constexpr size_t kV2SecondBodyFaceBlock = 82;

// CAD BAD FACE REF: the side fixture with the dependent's edgeLocalIndex set
// to 7 -- a rectangle has sides 0..3 -- so the reference names no face. The
// lineage still matches, the producer still exists; only the face is wrong.
std::vector<uint8_t> canonicalCadBadFaceRefBytes(const std::vector<uint8_t>& side) {
    std::vector<uint8_t> bytes = side;
    size_t length = 0;
    const size_t payload = findCadPayload(bytes, &length);
    writeU32At(&bytes, payload + kV2SecondBodyFaceBlock + 17, 7u);
    refreshCadCrc(&bytes, payload, length);
    return bytes;
}

// CAD DEPENDENCY CYCLE: the chain fixture with B re-pointed at C -- B on C's
// far cap and C on B's -- while A stays a world body. B's lineage is patched
// to C's topology signature so ONLY the cycle can refuse it.
std::vector<uint8_t> canonicalCadDependencyCycleBytes(const std::vector<uint8_t>& chain) {
    std::vector<uint8_t> bytes = chain;
    size_t length = 0;
    const size_t payload = findCadPayload(bytes, &length);
    const ProjectDocument document = canonicalCadFaceChainDocument();
    writeU64At(&bytes, payload + kV2SecondBodyFaceBlock, 3u);
    writeU64At(&bytes, payload + kV2SecondBodyFaceBlock + 21,
               cadTopologySignature(document.cad.bodies[2].state));
    refreshCadCrc(&bytes, payload, length);
    return bytes;
}

// CAD BAD PLANE: the rectangle fixture with its workplane code set to 9 and
// the CADB payload's CRC recomputed, so every length and checksum is right
// and only the semantic check can refuse it. Derived from the encoded bytes
// rather than from a document, because the encoder refuses to write it.
std::vector<uint8_t> canonicalCadBadPlaneBytes(const std::vector<uint8_t>& rectangle) {
    std::vector<uint8_t> bytes = rectangle;
    size_t offset = kForgeHeaderBytes;
    while (offset + kForgeSectionHeaderBytes <= bytes.size()) {
        uint64_t payloadBytes = 0;
        std::memcpy(&payloadBytes, &bytes[offset + 8], 8);
        if (std::memcmp(&bytes[offset], kSectionTagCad, 4) == 0) {
            const size_t payload = offset + kForgeSectionHeaderBytes;
            bytes[payload + 12] = 9;  // bodyCount(4) + objectId(8), then the plane
            const uint32_t crc = crc32IsoHdlc(&bytes[payload], static_cast<size_t>(payloadBytes));
            std::memcpy(&bytes[offset + 16], &crc, 4);
            break;
        }
        offset += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
    }
    return bytes;
}

std::string g_constructionSha;
std::string g_sculptSha;
std::string g_importedOnlySha;
std::string g_constructionImportedSha;
std::string g_mixedImportedSha;
std::string g_importedSculptSha;
std::string g_mixedImportedSculptSha;
std::string g_cadRectangleSha;
std::string g_cadCircleSha;
std::string g_mixedCadSha;
std::string g_cadBadPlaneSha;
std::string g_cadFaceCapSha;
std::string g_cadFaceSideSha;
std::string g_cadFaceChainSha;
std::string g_cadBadFaceRefSha;
std::string g_cadDependencyCycleSha;
std::string g_mixedCadFaceSha;

}  // namespace

// The one hash implementation the corpus-pinning suites share; see the header.
ConstructionObjectState canonicalCorpusShape(PrimitiveKind kind) {
    return canonicalSharedShape(kind);
}

std::string projectFixtureSha256Hex(const std::vector<uint8_t>& bytes) {
    return sha256Hex(bytes);
}

const char* canonicalConstructionFixtureSha256() { return g_constructionSha.c_str(); }
const char* canonicalSculptFixtureSha256() { return g_sculptSha.c_str(); }
const char* canonicalImportedOnlyFixtureSha256() { return g_importedOnlySha.c_str(); }
const char* canonicalConstructionImportedFixtureSha256() {
    return g_constructionImportedSha.c_str();
}
const char* canonicalMixedImportedFixtureSha256() { return g_mixedImportedSha.c_str(); }
const char* canonicalImportedSculptFixtureSha256() { return g_importedSculptSha.c_str(); }
const char* canonicalCadRectangleFixtureSha256() { return g_cadRectangleSha.c_str(); }
const char* canonicalCadCircleFixtureSha256() { return g_cadCircleSha.c_str(); }
const char* canonicalMixedCadFixtureSha256() { return g_mixedCadSha.c_str(); }
const char* canonicalCadBadPlaneFixtureSha256() { return g_cadBadPlaneSha.c_str(); }
const char* canonicalCadFaceSketchCapFixtureSha256() { return g_cadFaceCapSha.c_str(); }
const char* canonicalCadFaceSketchSideFixtureSha256() { return g_cadFaceSideSha.c_str(); }
const char* canonicalCadFaceChainFixtureSha256() { return g_cadFaceChainSha.c_str(); }
const char* canonicalCadBadFaceRefFixtureSha256() { return g_cadBadFaceRefSha.c_str(); }
const char* canonicalCadDependencyCycleFixtureSha256() {
    return g_cadDependencyCycleSha.c_str();
}
const char* canonicalMixedCadFaceFixtureSha256() { return g_mixedCadFaceSha.c_str(); }
const char* canonicalMixedImportedSculptFixtureSha256() {
    return g_mixedImportedSculptSha.c_str();
}

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

    // The three `IMPORT-01A` fixtures, encoded here so their digests are
    // reported beside the other two whether their case passes or fails.
    const std::vector<uint8_t> importedOnlyBytes =
            encodeProjectV1(canonicalImportedOnlyDocument(), &why);
    r.check("IMP01A_19_canonical_imported_only_document_encodes",
            why == ProjectCodecStatus::Ok && !importedOnlyBytes.empty());
    const std::vector<uint8_t> constructionImportedBytes =
            encodeProjectV1(canonicalConstructionImportedDocument(), &why);
    r.check("IMP01A_19_canonical_construction_imported_document_encodes",
            why == ProjectCodecStatus::Ok && !constructionImportedBytes.empty());
    const std::vector<uint8_t> mixedImportedBytes =
            encodeProjectV1(canonicalMixedImportedDocument(), &why);
    r.check("IMP01A_19_canonical_mixed_imported_document_encodes",
            why == ProjectCodecStatus::Ok && !mixedImportedBytes.empty());
    g_importedOnlySha = sha256Hex(importedOnlyBytes);
    g_constructionImportedSha = sha256Hex(constructionImportedBytes);
    g_mixedImportedSha = sha256Hex(mixedImportedBytes);
    {
        // Every one of them decodes back to the document it came from, and
        // re-encodes to the same bytes: the deterministic-writer rule, applied
        // to the branch that carries geometry.
        ProjectDocument back;
        r.check("IMP01A_19_the_imported_fixtures_roundtrip_bit_for_bit",
                decodeProject(importedOnlyBytes.data(), importedOnlyBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalImportedOnlyDocument(), back)
                        && encodeProjectV1(back) == importedOnlyBytes);
        r.check("IMP01A_19_the_construction_imported_fixture_roundtrips_bit_for_bit",
                decodeProject(constructionImportedBytes.data(),
                              constructionImportedBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalConstructionImportedDocument(), back)
                        && encodeProjectV1(back) == constructionImportedBytes);
        r.check("IMP01A_19_the_mixed_imported_fixture_roundtrips_bit_for_bit",
                decodeProject(mixedImportedBytes.data(), mixedImportedBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalMixedImportedDocument(), back)
                        && encodeProjectV1(back) == mixedImportedBytes);
    }
    // The two `IMPORT-01B` fixtures, on exactly the same terms: encoded here so
    // their digests are reported whether their case passes or fails.
    const std::vector<uint8_t> importedSculptBytes =
            encodeProjectV1(canonicalImportedSculptDocument(), &why);
    r.check("IMP01B_11_canonical_imported_sculpt_document_encodes",
            why == ProjectCodecStatus::Ok && !importedSculptBytes.empty());
    const std::vector<uint8_t> mixedImportedSculptBytes =
            encodeProjectV1(canonicalMixedImportedSculptDocument(), &why);
    r.check("IMP01B_12_canonical_mixed_imported_sculpt_document_encodes",
            why == ProjectCodecStatus::Ok && !mixedImportedSculptBytes.empty());
    g_importedSculptSha = sha256Hex(importedSculptBytes);
    g_mixedImportedSculptSha = sha256Hex(mixedImportedSculptBytes);
    {
        ProjectDocument back;
        r.check("IMP01B_11_the_imported_sculpt_fixture_roundtrips_bit_for_bit",
                decodeProject(importedSculptBytes.data(), importedSculptBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalImportedSculptDocument(), back)
                        && encodeProjectV1(back) == importedSculptBytes);
        r.check("IMP01B_12_the_mixed_imported_sculpt_fixture_roundtrips_bit_for_bit",
                decodeProject(mixedImportedSculptBytes.data(), mixedImportedSculptBytes.size(),
                              &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalMixedImportedSculptDocument(), back)
                        && encodeProjectV1(back) == mixedImportedSculptBytes);
    }
    // The four `CAD-R0-A1A2` fixtures, on the same terms.
    const std::vector<uint8_t> cadRectangleBytes =
            encodeProjectV1(canonicalCadRectangleDocument(), &why);
    r.check("CADR0_33_canonical_cad_rectangle_document_encodes",
            why == ProjectCodecStatus::Ok && !cadRectangleBytes.empty());
    const std::vector<uint8_t> cadCircleBytes = encodeProjectV1(canonicalCadCircleDocument(), &why);
    r.check("CADR0_34_canonical_cad_circle_document_encodes",
            why == ProjectCodecStatus::Ok && !cadCircleBytes.empty());
    const std::vector<uint8_t> mixedCadBytes = encodeProjectV1(canonicalMixedCadDocument(), &why);
    r.check("CADR0_33_canonical_mixed_cad_document_encodes",
            why == ProjectCodecStatus::Ok && !mixedCadBytes.empty());
    const std::vector<uint8_t> cadBadPlaneBytes = canonicalCadBadPlaneBytes(cadRectangleBytes);
    g_cadRectangleSha = sha256Hex(cadRectangleBytes);
    g_cadCircleSha = sha256Hex(cadCircleBytes);
    g_mixedCadSha = sha256Hex(mixedCadBytes);
    g_cadBadPlaneSha = sha256Hex(cadBadPlaneBytes);
    {
        ProjectDocument back;
        r.check("CADR0_33_the_cad_rectangle_fixture_roundtrips_bit_for_bit",
                decodeProject(cadRectangleBytes.data(), cadRectangleBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalCadRectangleDocument(), back)
                        && encodeProjectV1(back) == cadRectangleBytes);
        r.check("CADR0_34_the_cad_circle_fixture_roundtrips_bit_for_bit",
                decodeProject(cadCircleBytes.data(), cadCircleBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalCadCircleDocument(), back)
                        && encodeProjectV1(back) == cadCircleBytes);
        r.check("CADR0_33_the_mixed_cad_fixture_roundtrips_bit_for_bit",
                decodeProject(mixedCadBytes.data(), mixedCadBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalMixedCadDocument(), back)
                        && encodeProjectV1(back) == mixedCadBytes);
        // The corrupt fixture differs from the valid one in exactly the plane
        // byte and the CRC, and is refused by the SEMANTIC check -- proof that
        // every length and checksum was right and the meaning alone stopped it.
        r.check("CADR0_36_the_cad_bad_plane_fixture_is_refused_semantically",
                cadBadPlaneBytes.size() == cadRectangleBytes.size()
                        && cadBadPlaneBytes != cadRectangleBytes
                        && decodeProject(cadBadPlaneBytes.data(), cadBadPlaneBytes.size(), &back)
                                   == ProjectCodecStatus::InvalidSemanticValue);
    }

    // The six `CAD-A3` CADB v2 fixtures, on the same terms: encoded here so
    // their digests are pinned beside the v1 corpus, round-tripped bit for bit,
    // and the two corrupt ones refused by the production decoder for exactly
    // the reason the fixture plants.
    const std::vector<uint8_t> cadFaceCapBytes =
            encodeProjectV1(canonicalCadFaceSketchCapDocument(), &why);
    r.check("CADA3_46_canonical_cad_face_cap_document_encodes",
            why == ProjectCodecStatus::Ok && !cadFaceCapBytes.empty());
    const std::vector<uint8_t> cadFaceSideBytes =
            encodeProjectV1(canonicalCadFaceSketchSideDocument(), &why);
    r.check("CADA3_46_canonical_cad_face_side_document_encodes",
            why == ProjectCodecStatus::Ok && !cadFaceSideBytes.empty());
    const std::vector<uint8_t> cadFaceChainBytes =
            encodeProjectV1(canonicalCadFaceChainDocument(), &why);
    r.check("CADA3_46_canonical_cad_face_chain_document_encodes",
            why == ProjectCodecStatus::Ok && !cadFaceChainBytes.empty());
    const std::vector<uint8_t> mixedCadFaceBytes =
            encodeProjectV1(canonicalMixedCadFaceDocument(), &why);
    r.check("CADA3_46_canonical_mixed_cad_face_document_encodes",
            why == ProjectCodecStatus::Ok && !mixedCadFaceBytes.empty());
    const std::vector<uint8_t> cadBadFaceRefBytes = canonicalCadBadFaceRefBytes(cadFaceSideBytes);
    const std::vector<uint8_t> cadDependencyCycleBytes =
            canonicalCadDependencyCycleBytes(cadFaceChainBytes);
    g_cadFaceCapSha = sha256Hex(cadFaceCapBytes);
    g_cadFaceSideSha = sha256Hex(cadFaceSideBytes);
    g_cadFaceChainSha = sha256Hex(cadFaceChainBytes);
    g_mixedCadFaceSha = sha256Hex(mixedCadFaceBytes);
    g_cadBadFaceRefSha = sha256Hex(cadBadFaceRefBytes);
    g_cadDependencyCycleSha = sha256Hex(cadDependencyCycleBytes);
    {
        // The CADB section's version word, read off the file: a face-supported
        // project writes v2, and the v1 corpus is untouched at v1.
        const auto cadSectionVersion = [](const std::vector<uint8_t>& file) -> int {
            size_t length = 0;
            const size_t payload = findCadPayload(file, &length);
            if (payload == 0) return -1;
            const size_t header = payload - kForgeSectionHeaderBytes;
            return static_cast<int>(file[header + 4]) | (static_cast<int>(file[header + 5]) << 8);
        };
        r.check("CADA3_46_face_supported_fixtures_carry_cadb_v2_and_v1_fixtures_stay_v1",
                cadSectionVersion(cadFaceCapBytes) == 2 && cadSectionVersion(cadFaceSideBytes) == 2
                        && cadSectionVersion(cadFaceChainBytes) == 2
                        && cadSectionVersion(mixedCadFaceBytes) == 2
                        && cadSectionVersion(cadRectangleBytes) == 1
                        && cadSectionVersion(mixedCadBytes) == 1);
        ProjectDocument back;
        r.check("CADA3_46_the_cad_face_cap_fixture_roundtrips_bit_for_bit",
                decodeProject(cadFaceCapBytes.data(), cadFaceCapBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalCadFaceSketchCapDocument(), back)
                        && back.cad.bodies[1].state.sketch.hasFaceSupport
                        && back.cad.bodies[1].state.sketch.faceSupport.face.kind
                                   == CadFaceKind::CapFar
                        && encodeProjectV1(back) == cadFaceCapBytes);
        r.check("CADA3_47_the_cad_face_side_fixture_roundtrips_bit_for_bit",
                decodeProject(cadFaceSideBytes.data(), cadFaceSideBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalCadFaceSketchSideDocument(), back)
                        && back.cad.bodies[1].state.sketch.faceSupport.face.kind
                                   == CadFaceKind::Side
                        && back.cad.bodies[1].state.sketch.faceSupport.face.edgeLocalIndex == 1
                        && encodeProjectV1(back) == cadFaceSideBytes);
        r.check("CADA3_48_the_cad_face_chain_fixture_roundtrips_bit_for_bit",
                decodeProject(cadFaceChainBytes.data(), cadFaceChainBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalCadFaceChainDocument(), back)
                        && back.cad.bodies.size() == 3
                        && back.cad.bodies[2].state.sketch.faceSupport.producerObjectId == 2
                        && encodeProjectV1(back) == cadFaceChainBytes);
        r.check("CADA3_49_the_mixed_cad_face_fixture_roundtrips_bit_for_bit",
                decodeProject(mixedCadFaceBytes.data(), mixedCadFaceBytes.size(), &back)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(canonicalMixedCadFaceDocument(), back)
                        && back.hasConstruction && back.hasImported && back.hasCad
                        && encodeProjectV1(back) == mixedCadFaceBytes);
        // The corrupt fixtures differ from their valid parents only in the
        // planted field and the CRC that vouches for it, so the refusal can
        // only be the semantic one the fixture is about.
        r.check("CADA3_50_the_cad_bad_face_ref_fixture_is_refused_semantically",
                cadBadFaceRefBytes.size() == cadFaceSideBytes.size()
                        && cadBadFaceRefBytes != cadFaceSideBytes
                        && decodeProject(cadBadFaceRefBytes.data(), cadBadFaceRefBytes.size(),
                                         &back)
                                   == ProjectCodecStatus::InvalidSemanticValue);
        r.check("CADA3_51_the_cad_dependency_cycle_fixture_is_refused_as_unresolvable",
                cadDependencyCycleBytes.size() == cadFaceChainBytes.size()
                        && cadDependencyCycleBytes != cadFaceChainBytes
                        && decodeProject(cadDependencyCycleBytes.data(),
                                         cadDependencyCycleBytes.size(), &back)
                                   == ProjectCodecStatus::UnresolvedReference);
        // Both corrupt files pass every structural check -- lengths, CRCs,
        // counts -- and are refused ONLY once the document is validated. The
        // structural half is proved by the CRC being right: a wrong CRC would
        // refuse earlier, with a different status.
    }
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
        // Placement is the BODY's since IMPORT-01A, so it is captured beside the
        // shape rather than inside it.
        const TransformValues placementBefore = live.scene.activeBody().transform().values();
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
                                   live.scene.activeBody().transform().values(),
                                   placementBefore));
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
    // IMP01B-11/12: the four valid source/sculpt combinations, and the invalid
    // -----------------------------------------------------------------------
    //
    // `SCNE+CONS`, `SCNE+CONS+SCUL`, `SCNE+IMPT` and `SCNE+IMPT+SCUL` are the
    // whole matrix. What stays refused is a body claimed by BOTH source
    // branches, which is two answers to what the object IS.
    {
        r.check("IMP01B_11_an_imported_body_may_carry_a_sculpt_mesh",
                validateProjectDocument(canonicalImportedSculptDocument())
                        == ProjectCodecStatus::Ok);
        r.check("IMP01B_12_one_document_may_hold_all_four_combinations",
                validateProjectDocument(canonicalMixedImportedSculptDocument())
                        == ProjectCodecStatus::Ok);
        r.check("IMP01B_11_the_three_pre_existing_combinations_are_still_valid",
                validateProjectDocument(canonicalConstructionDocument())
                                == ProjectCodecStatus::Ok
                        && validateProjectDocument(canonicalSculptDocument())
                                == ProjectCodecStatus::Ok
                        && validateProjectDocument(canonicalImportedOnlyDocument())
                                == ProjectCodecStatus::Ok);

        // The exactly-one-of rule is untouched: it is about CONS and IMPT, and
        // allowing SCUL over IMPT did not weaken it.
        ProjectDocument both = canonicalImportedSculptDocument();
        both.hasConstruction = true;
        ProjectConstructionBody claim;
        claim.objectId = 1;
        claim.shape = canonicalSharedShape(PrimitiveKind::Box);
        claim.features.push_back(ProjectFeatureRecord{});
        both.construction.bodies.push_back(claim);
        r.check("IMP01B_11_a_body_claimed_by_both_cons_and_impt_is_still_refused",
                validateProjectDocument(both) == ProjectCodecStatus::UnresolvedReference);

        // And a SCUL entry for a body SCNE does not carry is still refused,
        // whichever branch the rest of the document uses.
        ProjectDocument dangling = canonicalImportedSculptDocument();
        dangling.sculpt.bodies[0].objectId = 7;
        r.check("IMP01B_11_a_scul_body_scne_does_not_carry_is_still_refused",
                validateProjectDocument(dangling) == ProjectCodecStatus::UnresolvedReference);

        // A Sculpt project's active body must still have a sculpt mesh, and
        // that rule never cared which source the body has.
        ProjectDocument wrongActive = canonicalMixedImportedSculptDocument();
        wrongActive.scene.activeObjectId = 3;  // the imported body with no sculpt mesh
        r.check("IMP01B_11_a_sculpt_project_whose_active_body_has_no_mesh_is_still_refused",
                validateProjectDocument(wrongActive) == ProjectCodecStatus::UnresolvedReference);
    }

    // IMP01B-11/12: and the same documents survive the LIVE round trip —
    // captured from a scene, encoded, decoded and loaded back into one.
    {
        ProjectDocument decoded;
        r.check("IMP01B_11_the_imported_sculpt_document_decodes",
                decodeProject(importedSculptBytes.data(), importedSculptBytes.size(), &decoded)
                        == ProjectCodecStatus::Ok);
        r.check("IMP01B_11_and_this_build_can_evaluate_it",
                runtimeCanEvaluateProject(decoded));

        ConstructionScene scene;
        SculptSession session;
        ConstructionHistory history(scene);
        ProjectLoadReport report;
        r.check("IMP01B_11_an_imported_sculpt_project_loads",
                loadProjectDocument(decoded, scene, session, history, &report)
                        == ProjectCodecStatus::Ok);
        r.check("IMP01B_11_as_one_imported_body_with_one_sculpt_mesh",
                report.bodies == 1 && report.importedBodies == 1 && report.sculptMeshes == 1);
        const SceneObject& body = scene.activeBody();
        r.check("IMP01B_11_the_body_is_still_an_imported_mesh",
                body.isImported() && body.constructionOrNull() == nullptr);
        r.check("IMP01B_11_and_its_imported_geometry_came_back",
                body.importedOrNull() != nullptr
                    && body.importedOrNull()->positions()
                            == canonicalImportedBody(1).positions
                    && body.importedOrNull()->normals() == canonicalImportedBody(1).normals
                    && body.importedOrNull()->indices() == canonicalImportedBody(1).indices);
        r.check("IMP01B_11_and_its_name", body.name() == "head_low");
        const ProjectSculptBody expectedSculpt = canonicalImportedSculptBody(1);
        bool sculptPositionsMatch =
                body.frozenSculpt().mesh.vertexCount() * 3u == expectedSculpt.positions.size();
        for (uint32_t v = 0; sculptPositionsMatch && v < body.frozenSculpt().mesh.vertexCount();
             ++v) {
            const Vec3 p = body.frozenSculpt().mesh.vertexPosition(v);
            if (p.x != expectedSculpt.positions[v * 3u]
                || p.y != expectedSculpt.positions[v * 3u + 1u]
                || p.z != expectedSculpt.positions[v * 3u + 2u]) {
                sculptPositionsMatch = false;
            }
        }
        r.check("IMP01B_11_and_its_sculpt_mesh_came_back_bit_for_bit",
                body.frozenSculpt().mesh.frozen() && sculptPositionsMatch
                    && body.frozenSculpt().mesh.indices() == expectedSculpt.indices);
        r.check("IMP01B_11_the_restored_sculpt_mesh_keeps_its_sidedness",
                body.frozenSculpt().mesh.renderBothSides() == expectedSculpt.renderBothSides);
        r.check("IMP01B_09_and_the_reset_guard_still_knows_there_are_edits",
                body.frozenSculpt().mesh.hasEdits());
        r.check("IMP01B_08_so_resume_sculpt_is_available", session.hasSculptMesh());
        // A Sculpt project reopens showing the sculpt mesh, not the source.
        r.check("IMP01B_11_and_the_sculpt_mesh_is_the_published_representation",
                body.meshStore().current() != nullptr
                    && body.meshStore().current()->vertexCount()
                            == body.frozenSculpt().mesh.vertexCount());

        // IMP01B-11: capturing the live project back produces the same document.
        const ProjectDocument recaptured = captureProjectDocument(scene, ProjectKind::Sculpt);
        r.check("IMP01B_11_capturing_it_again_produces_the_same_document",
                sameProjectDocument(recaptured, canonicalImportedSculptDocument()));
        r.check("IMP01B_11_and_the_same_bytes",
                encodeProjectV1(recaptured) == importedSculptBytes);
    }

    {
        // IMP01B-12: the four-combination document, loaded whole.
        ProjectDocument decoded;
        r.check("IMP01B_12_the_mixed_document_decodes",
                decodeProject(mixedImportedSculptBytes.data(), mixedImportedSculptBytes.size(),
                              &decoded)
                        == ProjectCodecStatus::Ok);
        ConstructionScene scene;
        SculptSession session;
        ConstructionHistory history(scene);
        ProjectLoadReport report;
        r.check("IMP01B_12_a_mixed_source_and_sculpt_project_loads",
                loadProjectDocument(decoded, scene, session, history, &report)
                        == ProjectCodecStatus::Ok);
        r.check("IMP01B_12_with_every_body_and_every_sculpt_mesh",
                report.bodies == 4 && report.importedBodies == 2 && report.sculptMeshes == 2);
        r.check("IMP01B_12_a_plain_construction_body_has_no_sculpt_mesh",
                scene.bodyAt(0).hasConstructionSource()
                    && !scene.bodyAt(0).frozenSculpt().mesh.frozen());
        r.check("IMP01B_12_a_construction_body_with_one_has_it",
                scene.bodyAt(1).hasConstructionSource()
                    && scene.bodyAt(1).frozenSculpt().mesh.frozen());
        r.check("IMP01B_12_a_plain_imported_body_has_none",
                scene.bodyAt(2).isImported()
                    && !scene.bodyAt(2).frozenSculpt().mesh.frozen());
        r.check("IMP01B_12_and_an_imported_body_with_one_has_it",
                scene.bodyAt(3).isImported()
                    && scene.bodyAt(3).frozenSculpt().mesh.frozen()
                    && scene.bodyAt(3).importedOrNull() != nullptr);
        r.check("IMP01B_12_recapturing_the_mixed_project_produces_the_same_bytes",
                encodeProjectV1(captureProjectDocument(scene, ProjectKind::Sculpt))
                        == mixedImportedSculptBytes);
    }

    // IMP01B-11: the fingerprint tells an imported body's sculpt work apart.
    //
    // A checkpoint that could not see a stroke on an imported body would stop
    // protecting exactly the work this stage adds.
    {
        ProjectDocument decoded;
        decodeProject(importedSculptBytes.data(), importedSculptBytes.size(), &decoded);
        ConstructionScene scene;
        SculptSession session;
        ConstructionHistory history(scene);
        loadProjectDocument(decoded, scene, session, history);
        const uint64_t before = projectSemanticFingerprint(scene, ProjectKind::Sculpt);
        SceneObject& body = scene.activeBody();
        const Vec3 moved = vec3Add(body.frozenSculpt().mesh.vertexPosition(0),
                                   Vec3{0.5f, 0.0f, 0.0f});
        body.frozenSculpt().mesh.setVertexPosition(0, moved);
        body.frozenSculpt().mesh.advanceRevision();
        r.check("IMP01B_11_the_fingerprint_sees_a_stroke_on_an_imported_body",
                projectSemanticFingerprint(scene, ProjectKind::Sculpt) != before);
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
        // The three `IMPORT-01A` fixtures, on exactly the same terms. They are
        // what proves the imported branch is a SPECIFICATION and not merely
        // whatever this encoder happens to emit: the PowerShell builder writes
        // them from DATA_PACKAGE_SPEC.md and shares no line with the codec.
        r.check("IMP01A_19_imported_only_fixture_matches_the_committed_digest",
                g_importedOnlySha
                        == "0f42be318da9faf4aa780e152b8550171085a267882d5e6e69cc9ef29a1539a8");
        r.check("IMP01A_19_construction_imported_fixture_matches_the_committed_digest",
                g_constructionImportedSha
                        == "539e10e7a9e388ab1ca72867b78c1e461d3bbe0ef5876d87fa54bc6bd6ae7a51");
        r.check("IMP01A_19_mixed_imported_fixture_matches_the_committed_digest",
                g_mixedImportedSha
                        == "3fdc82a099da69b93552d7c84c56002ed8ae24ba7086ddbc6a69a9bbf671f1fb");
        // The two `IMPORT-01B` fixtures, which pin the generalized SCUL rule:
        // an imported body carrying a Frozen Sculpt Mesh, and one document
        // holding all four valid source/sculpt combinations.
        r.check("IMP01B_11_imported_sculpt_fixture_matches_the_committed_digest",
                g_importedSculptSha
                        == "b82430cf6dbb82fddf075722d7ae335460f687d2a06cde09db43817323729f76");
        r.check("IMP01B_12_mixed_imported_sculpt_fixture_matches_the_committed_digest",
                g_mixedImportedSculptSha
                        == "ab709ecea27ec29f21b6fbef126e8cdc15dc5c733d9b751bd1c8832907f27a2b");
        // The four `CAD-R0-A1A2` fixtures, which pin the `CADB` section.
        r.check("CADR0_33_cad_rectangle_fixture_matches_the_committed_digest",
                g_cadRectangleSha
                        == "e2fd79c4070d1168ae883e064200438244c09a41bcf1682f9a0596b549d1b26b");
        r.check("CADR0_34_cad_circle_fixture_matches_the_committed_digest",
                g_cadCircleSha
                        == "886b1538a1a20113316b7badcc7c6aaca6b17ccb54d3dcfed042ff41866e4559");
        r.check("CADR0_33_mixed_cad_fixture_matches_the_committed_digest",
                g_mixedCadSha
                        == "94f014cdc01fe8beaa14301ef2a99c0805a7e13afe1bca0126b29026882c93db");
        r.check("CADR0_36_cad_bad_plane_fixture_matches_the_committed_digest",
                g_cadBadPlaneSha
                        == "60476603b6c1b1e1cf6b785e863c953ee6aa63573c1a2453bd7db0935909aba9");
        // The six `CAD-A3` fixtures, which pin CADB v2: the face support, the
        // lineage token as a FORMAT field the PowerShell builder reimplements
        // from DATA_PACKAGE_SPEC.md, and the two refusals.
        r.check("CADA3_46_cad_face_sketch_cap_fixture_matches_the_committed_digest",
                g_cadFaceCapSha
                        == "4e4bdacc20954b063ef38d81a0c2c1c4bb4faff97255aea1e11d161bec560a86");
        r.check("CADA3_47_cad_face_sketch_side_fixture_matches_the_committed_digest",
                g_cadFaceSideSha
                        == "9afa0ae2036cc1c03ee2e49f42fe45c19e79a8f050c9c87a2202205e10ed5a3d");
        r.check("CADA3_48_cad_face_chain_fixture_matches_the_committed_digest",
                g_cadFaceChainSha
                        == "24ad47b5b5d600a47860ccafc3a644fa22e3d0bde994c43d969567cb299580b7");
        r.check("CADA3_49_mixed_cad_face_fixture_matches_the_committed_digest",
                g_mixedCadFaceSha
                        == "4b20f3c8ea05876850043dff28591f19855efa4c0566bebd43df11f1c0ff1529");
        r.check("CADA3_50_cad_bad_face_ref_fixture_matches_the_committed_digest",
                g_cadBadFaceRefSha
                        == "2f728f27393ff19b9dfa327be8b6598f26eb595dfe9a8c399a0db5e8cd50f564");
        r.check("CADA3_51_cad_dependency_cycle_fixture_matches_the_committed_digest",
                g_cadDependencyCycleSha
                        == "88072d355efa54f95e6d68c10d15c81a9cfebc0e2ed1f231255ebe5a42b117f0");
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

    // -----------------------------------------------------------------------
    // FSR1B-01 / FSR1B-02: what autosave asks before it writes anything
    // -----------------------------------------------------------------------
    //
    // The fingerprint is the whole reason a checkpoint per edit is affordable
    // and a checkpoint per frame never happens. If it were wrong in the
    // permissive direction the product would write constantly; wrong in the
    // other direction it would silently stop protecting work. Both are covered.
    {
        LiveFixture live;
        const uint64_t initial = projectSemanticFingerprint(live.scene,
                                                            ProjectKind::Construction);
        r.check("FSR1B_01_the_fingerprint_is_stable_when_nothing_changes",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) == initial);

        // The reopen mode is part of the document, so it is part of the answer.
        r.check("FSR1B_01_the_reopen_mode_changes_the_fingerprint",
                projectSemanticFingerprint(live.scene, ProjectKind::Sculpt) != initial);

        // An ordinary Construction edit.
        {
            ScopedConstructionEdit edit(live.history);
            applyPrimitive(live.scene.activeBody().construction(),
                           live.scene.activeBody().meshStore(),
                           PrimitiveSpec::forSphere(2.75));
        }
        const uint64_t afterShape =
                projectSemanticFingerprint(live.scene, ProjectKind::Construction);
        r.check("FSR1B_01_a_shape_edit_changes_the_fingerprint", afterShape != initial);

        // A REJECTED edit changes nothing, so it must cost no checkpoint. This
        // is the "no write storm" rule at its source: a fingerprint that moved
        // on every attempted edit would checkpoint on every refused one too.
        {
            ScopedConstructionEdit edit(live.history);
            applyPrimitive(live.scene.activeBody().construction(),
                           live.scene.activeBody().meshStore(),
                           PrimitiveSpec::forSphere(-1.0));
        }
        r.check("FSR1B_02_a_refused_edit_leaves_the_fingerprint_alone",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) == afterShape);

        // An identical re-apply is Unchanged and must also cost nothing.
        {
            ScopedConstructionEdit edit(live.history);
            applyPrimitive(live.scene.activeBody().construction(),
                           live.scene.activeBody().meshStore(),
                           PrimitiveSpec::forSphere(2.75));
        }
        r.check("FSR1B_02_an_identical_re_apply_leaves_the_fingerprint_alone",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) == afterShape);

        // Placement is document truth too, and publishes no mesh revision — so
        // anything watching revisions instead of values would miss it entirely.
        {
            ScopedConstructionEdit edit(live.history);
            TransformValues moved = live.scene.activeBody().transform().values();
            moved.positionX = 3.25;
            applyTransformValues(live.scene.activeBody().transform(), moved);
        }
        const uint64_t afterMove =
                projectSemanticFingerprint(live.scene, ProjectKind::Construction);
        r.check("FSR1B_01_a_placement_edit_changes_the_fingerprint", afterMove != afterShape);

        // THE case that makes counters the wrong answer. `restoreState`, which
        // is the path an undo takes, deliberately does not advance updateCount:
        // an undo returns the object to a state it has already counted. A
        // counter-based fingerprint would call the undone project unchanged and
        // quietly stop protecting it.
        live.history.undo();
        r.check("FSR1B_01_an_undo_changes_the_fingerprint_that_counters_would_miss",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) != afterMove);
        r.check("FSR1B_01_an_undo_returns_the_fingerprint_to_the_earlier_state",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) == afterShape);

        // Creation, selection and the id allocator are all document facts.
        const uint64_t beforeAdd =
                projectSemanticFingerprint(live.scene, ProjectKind::Construction);
        live.scene.addBody();
        r.check("FSR1B_01_adding_a_body_changes_the_fingerprint",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) != beforeAdd);
        const uint64_t withTwo =
                projectSemanticFingerprint(live.scene, ProjectKind::Construction);
        live.scene.setActiveBody(live.scene.bodyAt(0).objectId());
        r.check("FSR1B_01_selecting_another_body_changes_the_fingerprint",
                projectSemanticFingerprint(live.scene, ProjectKind::Construction) != withTwo);
    }

    // FSR1B-13: nothing device-local can reach the document, by construction.
    {
        // The fingerprint reads the SCENE and the mode and nothing else — there
        // is no Context, no Uri, no path, no window and no GPU handle in the
        // signature, and the document DTOs carry none either. Asserted here as
        // the arithmetic it is: a document's encoded size is decided entirely by
        // its body count and its sculpt data, so no hidden device-local field
        // can be riding along.
        LiveFixture live;
        const ProjectDocument document =
                captureProjectDocument(live.scene, ProjectKind::Construction);
        const std::vector<uint8_t> encoded = encodeProjectV1(document);
        const size_t bodies = document.scene.bodies.size();
        const size_t predicted = static_cast<size_t>(kForgeHeaderBytes)
                                 + (kForgeSectionHeaderBytes + 4 + 8 + 8 + bodies * 80)
                                 + (kForgeSectionHeaderBytes + 4 + bodies * 114);
        r.check("FSR1B_13_a_captured_document_is_exactly_its_semantic_arithmetic",
                !encoded.empty() && encoded.size() == predicted);
        r.check("FSR1B_13_a_captured_document_carries_no_sculpt_branch_without_a_sculpt_mesh",
                !document.hasSculpt);
    }

    // -----------------------------------------------------------------------
    // IMP01A-16..20: the Imported Mesh branch of the `.forge` contract
    // -----------------------------------------------------------------------
    //
    // An Imported Mesh is the first representation whose GEOMETRY is project
    // truth: a Construction Body's mesh is regenerated from its parameters on
    // every load, and an imported object has no parameters and no source file
    // to go back to. These cases hold the three properties that follow from
    // that — the arrays come back bit for bit, a body is described by exactly
    // one branch, and a reader that could not rebuild one refuses the file
    // rather than opening it with objects missing.

    // IMP01A-16: an imported-only project is self-contained.
    {
        ImportedFixture fixture;
        const ProjectDocument document =
                captureProjectDocument(fixture.scene, ProjectKind::Construction);
        r.check("IMP01A_16_an_imported_body_writes_the_imported_branch", document.hasImported);
        r.check("IMP01A_16_and_no_construction_branch_is_invented_for_it",
                !document.hasConstruction && document.imported.bodies.size() == 1);

        ProjectCodecStatus why = ProjectCodecStatus::Ok;
        const std::vector<uint8_t> encoded = encodeProjectV1(document, &why);
        r.check("IMP01A_16_an_imported_only_project_encodes", !encoded.empty()
                && why == ProjectCodecStatus::Ok);

        ProjectDocument decoded;
        const ProjectCodecStatus status =
                decodeProject(encoded.data(), encoded.size(), &decoded);
        r.check("IMP01A_16_and_decodes", status == ProjectCodecStatus::Ok);
        r.check("IMP01A_16_bit_for_bit_including_normals_and_batches",
                sameProjectDocument(document, decoded));
        r.check("IMP01A_16_the_encoder_is_deterministic",
                encodeProjectV1(decoded) == encoded);
        r.check("IMP01A_16_the_name_survives_the_file",
                decoded.hasImported && decoded.imported.bodies.size() == 1
                        && decoded.imported.bodies[0].name == kImportedFixtureName);

        // And the LOAD rebuilds a real imported body, geometry and all.
        LiveFixture live;
        ProjectLoadReport report;
        r.check("IMP01A_16_an_imported_only_project_loads",
                live.load(encoded, &report) == ProjectCodecStatus::Ok);
        r.check("IMP01A_16_it_comes_back_as_one_imported_body",
                report.bodies == 1 && report.importedBodies == 1);
        const SceneObject& reloaded = live.scene.bodyAt(0);
        const ImportedMesh* mesh = reloaded.importedOrNull();
        r.check("IMP01A_16_with_no_construction_source",
                reloaded.isImported() && reloaded.constructionOrNull() == nullptr);
        r.check("IMP01A_16_and_the_same_geometry",
                mesh != nullptr && mesh->positions() == fixture.positions
                        && mesh->normals() == fixture.normals
                        && mesh->indices() == fixture.indices);
        r.check("IMP01A_16_the_per_submesh_double_sided_answer_survives",
                mesh != nullptr && mesh->batchCount() == 2 && !mesh->batches()[0].doubleSided
                        && mesh->batches()[1].doubleSided);
        r.check("IMP01A_16_and_the_placement_survives",
                sameConstructionPlacement(reloaded.transform().values(),
                                          importedFixturePlacement()));
        r.check("IMP01A_16_a_reloaded_body_is_drawable",
                reloaded.meshStore().currentRevision() != kNoMeshRevision);
        // Re-captured from the LIVE scene, so this proves the whole round trip
        // rather than only that the decoder inverts the encoder.
        r.check("IMP01A_16_re_encoding_the_loaded_project_is_byte_identical",
                encodeProjectV1(captureProjectDocument(live.scene, ProjectKind::Construction))
                        == encoded);
    }

    // IMP01A-17/18: mixed scenes. Construction + Imported, then all three.
    {
        LiveFixture live;  // two Construction Bodies
        ImportedFixture imported;
        SceneObject* added = live.scene.addImportedBody(imported.mesh(), kImportedFixtureName);
        publishSceneObject(*added);
        const ObjectId importedId = added->objectId();

        const ProjectDocument mixed =
                captureProjectDocument(live.scene, ProjectKind::Construction);
        r.check("IMP01A_17_a_mixed_project_writes_both_branches",
                mixed.hasConstruction && mixed.hasImported);
        r.check("IMP01A_17_construction_carries_only_the_bodies_that_have_a_source",
                mixed.construction.bodies.size() == 2 && mixed.imported.bodies.size() == 1
                        && mixed.scene.bodies.size() == 3);
        const std::vector<uint8_t> mixedBytes = encodeProjectV1(mixed);
        ProjectDocument mixedBack;
        r.check("IMP01A_17_a_mixed_project_roundtrips",
                !mixedBytes.empty()
                        && decodeProject(mixedBytes.data(), mixedBytes.size(), &mixedBack)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(mixed, mixedBack));

        LiveFixture target;
        ProjectLoadReport report;
        r.check("IMP01A_17_a_mixed_project_loads_atomically",
                target.load(mixedBytes, &report) == ProjectCodecStatus::Ok);
        r.check("IMP01A_17_with_both_representations_rebuilt",
                report.bodies == 3 && report.importedBodies == 1);
        const SceneObject* back = target.scene.findBody(importedId);
        r.check("IMP01A_17_the_imported_body_keeps_its_identity_and_order",
                back != nullptr && back->isImported()
                        && target.scene.indexOfBody(importedId) == 2);
        r.check("IMP01A_17_and_the_construction_bodies_keep_theirs",
                target.scene.bodyAt(0).hasConstructionSource()
                        && target.scene.bodyAt(1).hasConstructionSource());

        // IMP01A-18: add a Frozen Sculpt Mesh to one Construction Body and the
        // file carries all three branches at once.
        SceneObject& sculpted = live.scene.bodyAt(1);
        MeshValidation meshWhy = MeshValidation::Ok;
        const bool froze = sculpted.frozenSculpt().mesh.freezeFrom(
                sculpted.construction().generateMesh(), sculpted.objectId(), &meshWhy);
        r.check("IMP01A_18_the_fixture_freezes_a_sculpt_mesh", froze);
        const ProjectDocument all =
                captureProjectDocument(live.scene, ProjectKind::Construction);
        r.check("IMP01A_18_all_three_branches_are_written",
                all.hasConstruction && all.hasSculpt && all.hasImported);
        const std::vector<uint8_t> allBytes = encodeProjectV1(all);
        ProjectDocument allBack;
        r.check("IMP01A_18_all_three_roundtrip",
                !allBytes.empty()
                        && decodeProject(allBytes.data(), allBytes.size(), &allBack)
                                == ProjectCodecStatus::Ok
                        && sameProjectDocument(all, allBack));
        LiveFixture allTarget;
        ProjectLoadReport allReport;
        r.check("IMP01A_18_and_load_together",
                allTarget.load(allBytes, &allReport) == ProjectCodecStatus::Ok
                        && allReport.bodies == 3 && allReport.sculptMeshes == 1
                        && allReport.importedBodies == 1);
    }

    // IMP01A-19: a project with no imported body is byte-for-byte what it was.
    {
        // The legacy digests in DATA_PACKAGE_SPEC.md are checked above; this
        // states the rule that makes them still true. The imported branch costs
        // a project that has none exactly nothing: no section, no header flag,
        // and therefore not one byte.
        LiveFixture live;
        const ProjectDocument document =
                captureProjectDocument(live.scene, ProjectKind::Construction);
        const std::vector<uint8_t> encoded = encodeProjectV1(document);
        const size_t bodies = document.scene.bodies.size();
        const size_t predicted = static_cast<size_t>(kForgeHeaderBytes)
                                 + (kForgeSectionHeaderBytes + 4 + 8 + 8 + bodies * 80)
                                 + (kForgeSectionHeaderBytes + 4 + bodies * 114);
        r.check("IMP01A_19_a_project_with_no_imported_body_is_the_same_arithmetic",
                !document.hasImported && encoded.size() == predicted);
        r.check("IMP01A_19_and_its_header_flags_carry_no_imported_bit",
                encoded.size() > 15 && (encoded[15] & kHeaderFlagHasImported) == 0u);
    }

    // IMP01A-20: required Imported truth is never silently dropped, and every
    // way of describing it wrongly is refused.
    {
        ImportedFixture fixture;
        const ProjectDocument document =
                captureProjectDocument(fixture.scene, ProjectKind::Construction);
        const std::vector<uint8_t> encoded = encodeProjectV1(document);

        // The compatibility gate, stated as bytes: the header says the file
        // carries an imported branch, and the section is REQUIRED. A reader
        // from before `IMPORT-01A` refuses the first on its reserved-bit check
        // and the second on its unknown-required-section rule, so it cannot
        // open the project with the imported objects quietly missing.
        r.check("IMP01A_20_the_header_states_the_imported_branch",
                encoded.size() > 15 && (encoded[15] & kHeaderFlagHasImported) != 0u);
        bool importedSectionIsRequired = false;
        for (size_t at = kForgeHeaderBytes; at + kForgeSectionHeaderBytes <= encoded.size();) {
            const bool isImported = std::memcmp(&encoded[at], kSectionTagImported, 4) == 0;
            uint64_t payloadBytes = 0;
            std::memcpy(&payloadBytes, &encoded[at + 8], sizeof(payloadBytes));
            if (isImported) {
                importedSectionIsRequired = (encoded[at + 6] & kSectionFlagRequired) != 0u;
                break;
            }
            at += kForgeSectionHeaderBytes + static_cast<size_t>(payloadBytes);
        }
        r.check("IMP01A_20_the_imported_section_carries_the_required_bit",
                importedSectionIsRequired);

        // A name the importer could not have produced.
        {
            ProjectDocument bad = document;
            bad.imported.bodies[0].name = " untrimmed";
            r.check("IMP01A_20_an_uncanonical_name_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
            bad.imported.bodies[0].name.clear();
            r.check("IMP01A_20_an_empty_name_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
            bad.imported.bodies[0].name = std::string("head\x01low");
            r.check("IMP01A_20_a_control_character_in_a_name_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
        }
        // Geometry the live model refuses, through the DOMAIN's own validator.
        {
            ProjectDocument bad = document;
            bad.imported.bodies[0].indices[0] = bad.imported.bodies[0].vertexCount();
            r.check("IMP01A_20_an_index_outside_the_vertex_array_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
        }
        {
            ProjectDocument bad = document;
            bad.imported.bodies[0].normals[0] = 4.0f;  // no longer a direction
            r.check("IMP01A_20_a_normal_that_is_not_a_unit_direction_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::InvalidSemanticValue);
        }
        {
            ProjectDocument bad = document;
            bad.imported.bodies[0].batches[0].indexCount += 3;  // a gap, then an overrun
            r.check("IMP01A_20_batches_that_do_not_tile_the_indices_are_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::BadPayload);
        }
        {
            ProjectDocument bad = document;
            bad.imported.bodies[0].normals.pop_back();
            r.check("IMP01A_20_one_normal_short_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::BadPayload);
        }
        // A body claimed by both branches: two answers to what the object IS.
        {
            ProjectDocument bad = document;
            ProjectConstructionBody stowaway;
            stowaway.objectId = bad.scene.bodies[0].objectId;
            stowaway.shape = ConstructionObjectState{};
            stowaway.features.push_back(ProjectFeatureRecord{});
            bad.construction.bodies.push_back(stowaway);
            bad.hasConstruction = true;
            r.check("IMP01A_20_a_body_in_both_branches_is_refused",
                    validateProjectDocument(bad) == ProjectCodecStatus::UnresolvedReference);
        }
        // A Frozen Sculpt Mesh for an imported body was refused until
        // `IMPORT-01B` and is VALID now: an imported object can be sculpted,
        // and its sculpt mesh is a second representation of the same body, not
        // a second answer to what the body IS. The case is kept, inverted, so
        // the rule change is visible in the suite that used to state the old
        // one rather than merely absent from it.
        {
            ProjectDocument nowValid = document;
            ProjectSculptBody sculpt;
            sculpt.objectId = nowValid.scene.bodies[0].objectId;
            sculpt.positions = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
            sculpt.indices = {0, 1, 2};
            nowValid.sculpt.bodies.push_back(sculpt);
            nowValid.hasSculpt = true;
            r.check("IMP01B_11_a_sculpt_mesh_for_an_imported_body_is_accepted",
                    validateProjectDocument(nowValid) == ProjectCodecStatus::Ok);
        }
        // A truncated file fails closed and applies nothing, exactly as every
        // other corrupt document does.
        {
            std::vector<uint8_t> truncated(encoded.begin(), encoded.end() - 24);
            ProjectDocument out;
            r.check("IMP01A_20_a_truncated_imported_file_is_refused",
                    decodeProject(truncated.data(), truncated.size(), &out)
                            != ProjectCodecStatus::Ok);
        }
        // One flipped byte inside the imported payload is caught by the CRC.
        {
            std::vector<uint8_t> corrupt = encoded;
            corrupt[corrupt.size() - 8] ^= 0x01u;
            ProjectDocument out;
            r.check("IMP01A_20_a_flipped_bit_in_the_imported_payload_is_caught",
                    decodeProject(corrupt.data(), corrupt.size(), &out)
                            == ProjectCodecStatus::ChecksumMismatch);
        }
        // A reserved batch-flag bit is refused rather than ignored.
        {
            std::vector<uint8_t> tweaked = encoded;
            // The final byte of an IMPT entry is its last batch's flags byte.
            tweaked[tweaked.size() - 1] = 0x02u;
            // The CRC has to agree, or this would only prove the checksum works.
            rewriteLastSectionCrc(&tweaked);
            ProjectDocument out;
            r.check("IMP01A_20_a_reserved_batch_flag_bit_is_refused",
                    decodeProject(tweaked.data(), tweaked.size(), &out)
                            == ProjectCodecStatus::BadPayload);
        }
        // And a refused load leaves the live project untouched, which is the
        // whole reason the decode happens into temporary state.
        {
            LiveFixture live;
            const std::vector<uint8_t> before =
                    encodeProjectV1(captureProjectDocument(live.scene, ProjectKind::Construction));
            std::vector<uint8_t> truncated(encoded.begin(), encoded.end() - 24);
            r.check("IMP01A_20_a_refused_imported_load_changes_nothing",
                    live.load(truncated, nullptr) != ProjectCodecStatus::Ok
                            && encodeProjectV1(captureProjectDocument(
                                       live.scene, ProjectKind::Construction))
                                    == before);
        }
    }

    return r.n;
}

}  // namespace forgeshape
