// A small, bounded, read-only JSON parser.
//
// WHY THIS EXISTS, AND WHAT IT DELIBERATELY IS NOT
// -----------------------------------------------
// The GLB exporter writes its JSON by hand, one key at a time, in a fixed
// order. That is right for a writer — it is what makes the output
// deterministic — and it is useless for a reader, which has to accept whatever
// order and spacing a conforming file happens to use.
//
// So the importer needs a parser, and it must be a parser that shares nothing
// with the writer. This file is it: it knows what JSON is and nothing about
// glTF, meshes, transforms or ForgeShape. `forgeshape_gltf_import.cpp` gives
// the values meaning; this file only gets them out of the bytes.
//
// It is deliberately NOT a general-purpose JSON library. It is read-only, it
// builds a value tree and hands it back, and every limit below is a fixed cap
// rather than a policy a caller can raise:
//
//   * bounded nesting depth, so a hostile file cannot recurse the stack away;
//   * bounded token and container counts;
//   * no comments, no trailing commas, no NaN/Infinity literals -- all of
//     which some JSON supersets accept and none of which are JSON, and a
//     `nan` accepted here is a `nan` that reaches geometry;
//   * numbers are parsed as double and nothing else. There is no integer type:
//     JSON has one number type, and pretending otherwise here would move the
//     integer question into the wrong file.
//
// Platform-neutral C++17: no Android, no JNI, no Vulkan, no filesystem.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace forgeshape {

// How deep a document may nest before it is refused. Fifteen is far past
// anything glTF produces (a document is object -> array -> object -> array of
// numbers, four deep) and far short of anything that could threaten the stack.
constexpr int kMaxJsonDepth = 15;

// A ceiling on the values one document may contain, so parsing cannot allocate
// without bound from a small input. glTF documents this project reads are a few
// thousand values; a million is a refusal, not a limit anyone will meet.
constexpr size_t kMaxJsonValues = 1000000;

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

enum class JsonStatus {
    Ok,
    Empty,
    UnexpectedCharacter,
    UnterminatedString,
    InvalidEscape,
    InvalidNumber,
    DepthExceeded,
    TooManyValues,
    TrailingContent,
    UnexpectedEnd,
};

const char* jsonStatusName(JsonStatus status);

// One parsed value.
//
// Children are held by INDEX into the document's flat value array rather than
// by pointer, so the tree can grow while it is being built without any
// reference into it dangling. A caller never sees an index: `JsonDocument`'s
// accessors below hand back references.
struct JsonValue {
    JsonType type = JsonType::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;                 // String values, and object member keys
    std::vector<size_t> children;     // Array elements, or Object member values
    std::vector<std::string> keys;    // Object member names, parallel to children
};

// A parsed document. Value 0 is the root, when `status` is Ok.
class JsonDocument {
public:
    // Parses UTF-8 JSON text. On anything but Ok the document is left empty,
    // because a half-parsed tree is a tree a caller can read wrong values out
    // of without noticing.
    JsonStatus parse(const char* text, size_t length);

    JsonStatus status() const { return status_; }
    bool ok() const { return status_ == JsonStatus::Ok && !values_.empty(); }

    const JsonValue& root() const { return values_[0]; }
    const JsonValue& at(size_t index) const { return values_[index]; }
    size_t valueCount() const { return values_.size(); }

    // The member of `object` named `key`, or nullptr. Returns nullptr for a
    // non-object, so a caller never has to check the type first.
    const JsonValue* member(const JsonValue& object, const char* key) const;

    // Element `index` of `array`, or nullptr when the value is not an array or
    // the index is past its end. Bounds are the parser's job, not the caller's.
    const JsonValue* element(const JsonValue& array, size_t index) const;

    // Convenience readers that fail to a default rather than throwing. Each
    // states its own type requirement, so asking an array for a number is a
    // missing value rather than a wrong one.
    bool numberMember(const JsonValue& object, const char* key, double* out) const;
    bool stringMember(const JsonValue& object, const char* key, std::string* out) const;
    bool boolMember(const JsonValue& object, const char* key, bool* out) const;

private:
    JsonStatus status_ = JsonStatus::Empty;
    std::vector<JsonValue> values_;
};

}  // namespace forgeshape
