// Explicit little-endian byte plumbing for the portable `.forge` project file.
//
// Platform-neutral C++17. No Android, no JNI, no Vulkan, no renderer type, and
// deliberately no domain type either: this file knows integers, IEEE-754
// scalars, bounds and a checksum, and nothing about what they mean.
//
// Why this exists at all
// ----------------------
// A `.forge` file is transferable between compatible ForgeShape installations,
// so every field on disk has ONE encoding that is a property of the FORMAT and
// never of the machine that wrote it. Nothing here writes a struct image, an
// enum's ABI value, a pointer, a `size_t` or a padding byte: every value is
// widened to a named fixed-width type and emitted byte by byte, least
// significant first. A big-endian host, a 32-bit host and a host with different
// struct packing all produce and consume the same bytes.
//
// Reading is bounds-checked by construction. `ByteReader` never advances past
// its own limit, and every read reports failure rather than trusting a length
// that came out of the file — which is the whole of how a truncated or hostile
// file is refused before anything is allocated.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace forgeshape {

// CRC-32/ISO-HDLC ("CRC-32"), the checksum every section header carries:
// poly 0x04C11DB7, reflected implementation 0xEDB88320, init 0xFFFFFFFF,
// refin/refout true, xorout 0xFFFFFFFF. Chosen because it is completely
// specified by those constants — no table file, no library and no ambiguity
// about which of the many "CRC-32"s a second implementation must match.
uint32_t crc32IsoHdlc(const uint8_t* data, size_t size);

// Appends fixed-width little-endian values to a byte vector.
//
// Deliberately has no seek, no overwrite and no alignment: an encoder writes a
// payload strictly forward, which is what makes the deterministic-writer rule
// something the type system helps with rather than something a comment asks for.
class ByteWriter {
public:
    explicit ByteWriter(std::vector<uint8_t>& out) : out_(out) {}

    void u8(uint8_t value) { out_.push_back(value); }

    void u16(uint16_t value) {
        u8(static_cast<uint8_t>(value & 0xFFu));
        u8(static_cast<uint8_t>((value >> 8) & 0xFFu));
    }

    void u32(uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            u8(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }
    }

    void u64(uint64_t value) {
        for (int shift = 0; shift < 64; shift += 8) {
            u8(static_cast<uint8_t>((value >> shift) & 0xFFu));
        }
    }

    // The IEEE-754 BIT PATTERN, not a decimal rendering: a sculpted vertex must
    // come back exactly, and a text form would quietly round it.
    void f32(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

    void f64(double value) {
        uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void bytes(const void* data, size_t size) {
        const uint8_t* src = static_cast<const uint8_t*>(data);
        out_.insert(out_.end(), src, src + size);
    }

    size_t size() const { return out_.size(); }

private:
    std::vector<uint8_t>& out_;
};

// Reads fixed-width little-endian values out of a bounded window.
//
// Every accessor returns false and consumes nothing when the window has too few
// bytes left, and once a read has failed the reader stays failed — so a decoder
// may read a whole record and test once at the end rather than testing after
// every field, and cannot act on a value it never actually read.
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool ok() const { return ok_; }
    size_t offset() const { return offset_; }
    size_t remaining() const { return ok_ ? (size_ - offset_) : 0; }
    bool atEnd() const { return ok_ && offset_ == size_; }

    // A window over the next `size` bytes, without copying them. Used for a
    // section payload, so a section physically cannot read past its own length
    // even if its inner decoder is wrong.
    bool window(size_t size, ByteReader* out) {
        if (!have(size) || out == nullptr) return fail();
        *out = ByteReader(data_ + offset_, size);
        offset_ += size;
        return true;
    }

    bool skip(size_t size) {
        if (!have(size)) return fail();
        offset_ += size;
        return true;
    }

    bool u8(uint8_t* out) {
        if (!have(1)) return fail();
        *out = data_[offset_++];
        return true;
    }

    bool u16(uint16_t* out) {
        uint8_t b[2];
        if (!raw(b, 2)) return false;
        *out = static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
        return true;
    }

    bool u32(uint32_t* out) {
        uint8_t b[4];
        if (!raw(b, 4)) return false;
        uint32_t value = 0;
        for (int i = 3; i >= 0; --i) {
            value = (value << 8) | b[i];
        }
        *out = value;
        return true;
    }

    bool u64(uint64_t* out) {
        uint8_t b[8];
        if (!raw(b, 8)) return false;
        uint64_t value = 0;
        for (int i = 7; i >= 0; --i) {
            value = (value << 8) | b[i];
        }
        *out = value;
        return true;
    }

    bool f32(float* out) {
        uint32_t bits = 0;
        if (!u32(&bits)) return false;
        std::memcpy(out, &bits, sizeof(*out));
        return true;
    }

    bool f64(double* out) {
        uint64_t bits = 0;
        if (!u64(&bits)) return false;
        std::memcpy(out, &bits, sizeof(*out));
        return true;
    }

    bool raw(void* out, size_t size) {
        if (!have(size)) return fail();
        std::memcpy(out, data_ + offset_, size);
        offset_ += size;
        return true;
    }

    const uint8_t* cursor() const { return data_ + offset_; }

private:
    bool have(size_t size) const { return ok_ && size <= size_ - offset_; }
    bool fail() {
        ok_ = false;
        return false;
    }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t offset_ = 0;
    bool ok_ = true;
};

}  // namespace forgeshape
