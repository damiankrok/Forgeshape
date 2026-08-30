#include "forgeshape_project_bytes.h"

namespace forgeshape {

// Bit-reflected CRC-32/ISO-HDLC, computed one bit at a time.
//
// No 256-entry table and no lazily-built static: the whole point of this
// checksum is that a second implementation on another platform can reproduce it
// from the four constants alone, and a bitwise loop IS those constants. A
// project file is written once per Save and read once per Open, so the table's
// speed buys nothing measurable and costs a generated blob nobody can check by
// reading it.
uint32_t crc32IsoHdlc(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    if (data == nullptr) {
        // A zero-length payload is legitimate (an empty optional section), and
        // its CRC is the init value passed straight through xorout.
        return crc ^ 0xFFFFFFFFu;
    }
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace forgeshape
