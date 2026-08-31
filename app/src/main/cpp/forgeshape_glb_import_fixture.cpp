#include "forgeshape_glb_import_fixture.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace forgeshape {
namespace {

constexpr uint32_t kRows = kNomadLikeFixtureRows;
constexpr uint32_t kColumns = kNomadLikeFixtureColumns;
constexpr uint32_t kPrimitives = kNomadLikeFixturePrimitives;
constexpr uint32_t kRowsPerPrimitive = (kRows - 1) / kPrimitives;  // 6
constexpr uint32_t kTrianglesPerPrimitive =
        kRowsPerPrimitive * (kColumns - 1) * 2;  // 540
constexpr uint32_t kIndicesPerPrimitive = kTrianglesPerPrimitive * 3;  // 1620

// Every coordinate is an integer over 1024, so a float holds it exactly and no
// rounding mode, compiler or libm can move a byte of the fixture.
constexpr float kUnit = 1.0f / 1024.0f;

// The height of the surface at one grid point, in 1/1024 metres.
//
// A deliberately lopsided integer polynomial: it is not symmetric in `row`, not
// symmetric in `column`, and not symmetric between the two. That is the point —
// a transposed node matrix, a dropped one or a mirrored bake all change the
// world bounds of this surface, so a test can catch each of them by number
// rather than by eye.
int32_t heightAt(int32_t row, int32_t column) {
    return (column * column * 3) / 8 - (row * row * 5) / 16 + (row * column * 7) / 8
            - row * 11 + column * 23;
}

void appendU32(std::vector<uint8_t>* out, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out->push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
    }
}

void appendU16(std::vector<uint8_t>* out, uint16_t value) {
    out->push_back(static_cast<uint8_t>(value & 0xFFu));
    out->push_back(static_cast<uint8_t>((value >> 8) & 0xFFu));
}

void appendF32(std::vector<uint8_t>* out, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(out, bits);
}

void padTo4(std::vector<uint8_t>* out, uint8_t filler) {
    while ((out->size() % 4u) != 0u) {
        out->push_back(filler);
    }
}

std::string number(float value) {
    // %.9g round-trips a float32 exactly, and every value it is given here is
    // an exact binary fraction, so the text is the same on every platform.
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", static_cast<double>(value));
    return text;
}

}  // namespace

std::vector<uint8_t> buildNomadLikeGlbFixture() {
    // -----------------------------------------------------------------------
    // The binary payload, one bufferView after another, each 4-byte aligned
    // -----------------------------------------------------------------------
    std::vector<uint8_t> bin;

    // POSITION — one accessor, shared by all seven primitives, exactly as a
    // low-poly character exported with several material regions states it.
    const uint32_t positionOffset = 0;
    float minPosition[3] = {0.0f, 0.0f, 0.0f};
    float maxPosition[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t row = 0; row < kRows; ++row) {
        for (uint32_t column = 0; column < kColumns; ++column) {
            const float position[3] = {
                static_cast<float>(static_cast<int32_t>(column) * 64 - 1440) * kUnit,
                static_cast<float>(heightAt(static_cast<int32_t>(row),
                                            static_cast<int32_t>(column))) * kUnit,
                static_cast<float>(static_cast<int32_t>(row) * 64 - 1344) * kUnit,
            };
            for (int c = 0; c < 3; ++c) {
                if (row == 0 && column == 0) {
                    minPosition[c] = position[c];
                    maxPosition[c] = position[c];
                } else {
                    if (position[c] < minPosition[c]) minPosition[c] = position[c];
                    if (position[c] > maxPosition[c]) maxPosition[c] = position[c];
                }
                appendF32(&bin, position[c]);
            }
        }
    }
    const uint32_t positionLength = static_cast<uint32_t>(bin.size()) - positionOffset;

    // COLOR_0 — normalized unsigned short VEC4. Validated by the reader and
    // then deliberately not decoded: the preview draws one flat neutral.
    const uint32_t color0Offset = static_cast<uint32_t>(bin.size());
    for (uint32_t row = 0; row < kRows; ++row) {
        for (uint32_t column = 0; column < kColumns; ++column) {
            appendU16(&bin, static_cast<uint16_t>((row * 1500u) % 65536u));
            appendU16(&bin, static_cast<uint16_t>((column * 1400u) % 65536u));
            appendU16(&bin, static_cast<uint16_t>((row * column * 31u) % 65536u));
            appendU16(&bin, 65535u);
        }
    }
    const uint32_t color0Length = static_cast<uint32_t>(bin.size()) - color0Offset;

    // COLOR_1 — normalized unsigned byte VEC4. Same treatment.
    const uint32_t color1Offset = static_cast<uint32_t>(bin.size());
    for (uint32_t row = 0; row < kRows; ++row) {
        for (uint32_t column = 0; column < kColumns; ++column) {
            bin.push_back(static_cast<uint8_t>((row * 6u) % 256u));
            bin.push_back(static_cast<uint8_t>((column * 5u) % 256u));
            bin.push_back(static_cast<uint8_t>(((row + column) * 3u) % 256u));
            bin.push_back(255u);
        }
    }
    const uint32_t color1Length = static_cast<uint32_t>(bin.size()) - color1Offset;
    padTo4(&bin, 0u);

    // TEXCOORD_0 — float VEC2, divided by a power of two so it too is exact.
    const uint32_t uvOffset = static_cast<uint32_t>(bin.size());
    for (uint32_t row = 0; row < kRows; ++row) {
        for (uint32_t column = 0; column < kColumns; ++column) {
            appendF32(&bin, static_cast<float>(column) / 64.0f);
            appendF32(&bin, static_cast<float>(row) / 64.0f);
        }
    }
    const uint32_t uvLength = static_cast<uint32_t>(bin.size()) - uvOffset;

    // Seven index accessors, one per primitive, each a horizontal band of the
    // same shared vertex grid. uint32 indices, as the owner's sample uses.
    uint32_t indexOffset[kPrimitives] = {0};
    uint32_t indexLength[kPrimitives] = {0};
    for (uint32_t primitive = 0; primitive < kPrimitives; ++primitive) {
        indexOffset[primitive] = static_cast<uint32_t>(bin.size());
        const uint32_t firstRow = primitive * kRowsPerPrimitive;
        for (uint32_t row = firstRow; row < firstRow + kRowsPerPrimitive; ++row) {
            for (uint32_t column = 0; column + 1 < kColumns; ++column) {
                const uint32_t v00 = row * kColumns + column;
                const uint32_t v01 = v00 + 1;
                const uint32_t v10 = v00 + kColumns;
                const uint32_t v11 = v10 + 1;
                appendU32(&bin, v00);
                appendU32(&bin, v10);
                appendU32(&bin, v11);
                appendU32(&bin, v00);
                appendU32(&bin, v11);
                appendU32(&bin, v01);
            }
        }
        indexLength[primitive] = static_cast<uint32_t>(bin.size()) - indexOffset[primitive];
    }
    padTo4(&bin, 0u);

    // -----------------------------------------------------------------------
    // The JSON document, written by hand so the bytes are fixed
    // -----------------------------------------------------------------------
    std::string json;
    json += "{\"asset\":{\"version\":\"2.0\",\"generator\":\"ForgeShape GLBIR1 Nomad-like "
            "fixture 1\"},";
    // `extras` at every level a real tool writes one. The reader must ignore
    // all of it: it is another tool's private data, never project truth.
    json += "\"extras\":{\"note\":\"synthetic structural fixture, not an owner asset\"},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";
    json += "\"nodes\":[{\"name\":\"NomadLikeFixture\",\"mesh\":0,\"matrix\":"
            // Column-major, per glTF. Non-orthogonal, non-uniform and
            // asymmetric, with a positive determinant of 1.859375, so a
            // transpose or a dropped matrix is visible in the world bounds and
            // no winding correction is expected.
            "[0.75,0.25,-0.5,0,0,1.5,0.25,0,0.5,-0.25,1.25,0,0.5,1.25,-0.75,1],"
            "\"extras\":{\"nomad\":{\"version\":11}}}],";
    json += "\"materials\":[{\"name\":\"fixture\",\"doubleSided\":true,\"alphaMode\":\"OPAQUE\","
            "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.5,0.3,1],"
            "\"metallicFactor\":0,\"roughnessFactor\":1}}],";

    json += "\"meshes\":[{\"name\":\"NomadLikeFixtureMesh\","
            "\"extras\":{\"nomad\":{\"regions\":7}},\"primitives\":[";
    for (uint32_t primitive = 0; primitive < kPrimitives; ++primitive) {
        if (primitive != 0) {
            json += ",";
        }
        char text[192];
        std::snprintf(text, sizeof(text),
                      "{\"attributes\":{\"POSITION\":0,\"COLOR_0\":1,\"COLOR_1\":2,"
                      "\"TEXCOORD_0\":3},\"indices\":%u,\"material\":0,\"mode\":4}",
                      4u + primitive);
        json += text;
    }
    json += "]}],";

    json += "\"accessors\":[";
    {
        char text[512];
        std::snprintf(text, sizeof(text),
                      "{\"bufferView\":0,\"componentType\":5126,\"count\":%u,\"type\":\"VEC3\","
                      "\"min\":[%s,%s,%s],\"max\":[%s,%s,%s]},",
                      kNomadLikeFixtureVertices, number(minPosition[0]).c_str(),
                      number(minPosition[1]).c_str(), number(minPosition[2]).c_str(),
                      number(maxPosition[0]).c_str(), number(maxPosition[1]).c_str(),
                      number(maxPosition[2]).c_str());
        json += text;
        std::snprintf(text, sizeof(text),
                      "{\"bufferView\":1,\"componentType\":5123,\"normalized\":true,"
                      "\"count\":%u,\"type\":\"VEC4\"},"
                      "{\"bufferView\":2,\"componentType\":5121,\"normalized\":true,"
                      "\"count\":%u,\"type\":\"VEC4\"},"
                      "{\"bufferView\":3,\"componentType\":5126,\"count\":%u,\"type\":\"VEC2\"}",
                      kNomadLikeFixtureVertices, kNomadLikeFixtureVertices,
                      kNomadLikeFixtureVertices);
        json += text;
        for (uint32_t primitive = 0; primitive < kPrimitives; ++primitive) {
            std::snprintf(text, sizeof(text),
                          ",{\"bufferView\":%u,\"componentType\":5125,\"count\":%u,"
                          "\"type\":\"SCALAR\"}",
                          4u + primitive, kIndicesPerPrimitive);
            json += text;
        }
    }
    json += "],";

    json += "\"bufferViews\":[";
    {
        // Four bufferViews in one call, each up to ~70 characters: the buffer
        // has to hold all of them or snprintf would truncate the document.
        char text[512];
        std::snprintf(text, sizeof(text),
                      "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
                      "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
                      "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
                      "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962}",
                      positionOffset, positionLength, color0Offset, color0Length, color1Offset,
                      color1Length, uvOffset, uvLength);
        json += text;
        for (uint32_t primitive = 0; primitive < kPrimitives; ++primitive) {
            std::snprintf(text, sizeof(text),
                          ",{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34963}",
                          indexOffset[primitive], indexLength[primitive]);
            json += text;
        }
    }
    json += "],";

    {
        char text[64];
        std::snprintf(text, sizeof(text), "\"buffers\":[{\"byteLength\":%zu}]}", bin.size());
        json += text;
    }

    // -----------------------------------------------------------------------
    // The container
    // -----------------------------------------------------------------------
    std::vector<uint8_t> jsonChunk(json.begin(), json.end());
    padTo4(&jsonChunk, static_cast<uint8_t>(' '));

    std::vector<uint8_t> glb;
    glb.reserve(12 + 8 + jsonChunk.size() + 8 + bin.size());
    appendU32(&glb, 0x46546C67u);  // "glTF"
    appendU32(&glb, 2u);
    appendU32(&glb, static_cast<uint32_t>(12 + 8 + jsonChunk.size() + 8 + bin.size()));
    appendU32(&glb, static_cast<uint32_t>(jsonChunk.size()));
    appendU32(&glb, 0x4E4F534Au);  // "JSON"
    glb.insert(glb.end(), jsonChunk.begin(), jsonChunk.end());
    appendU32(&glb, static_cast<uint32_t>(bin.size()));
    appendU32(&glb, 0x004E4942u);  // "BIN\0"
    glb.insert(glb.end(), bin.begin(), bin.end());
    return glb;
}

}  // namespace forgeshape
