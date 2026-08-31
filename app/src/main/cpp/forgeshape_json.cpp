#include "forgeshape_json.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace forgeshape {
namespace {

// The parser's cursor. Every read is bounds-checked against `end` rather than
// relying on a terminator, because the input is a chunk of a larger file and is
// not guaranteed to be NUL-terminated.
struct Cursor {
    const char* p = nullptr;
    const char* end = nullptr;

    bool done() const { return p >= end; }
    char peek() const { return p < end ? *p : '\0'; }
    void skipWhitespace() {
        // Exactly the four JSON whitespace characters. Not isspace(), which is
        // locale-dependent and accepts vertical tab and form feed.
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
            ++p;
        }
    }
};

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

const char* jsonStatusName(JsonStatus status) {
    switch (status) {
        case JsonStatus::Ok: return "Ok";
        case JsonStatus::Empty: return "Empty";
        case JsonStatus::UnexpectedCharacter: return "UnexpectedCharacter";
        case JsonStatus::UnterminatedString: return "UnterminatedString";
        case JsonStatus::InvalidEscape: return "InvalidEscape";
        case JsonStatus::InvalidNumber: return "InvalidNumber";
        case JsonStatus::DepthExceeded: return "DepthExceeded";
        case JsonStatus::TooManyValues: return "TooManyValues";
        case JsonStatus::TrailingContent: return "TrailingContent";
        case JsonStatus::UnexpectedEnd: return "UnexpectedEnd";
    }
    return "unknown";
}

namespace {

// One recursive-descent parse. `values` grows as the tree is discovered and
// `outIndex` receives the index of the value parsed by this call.
//
// The recursion is bounded by `depth` against kMaxJsonDepth before it descends,
// not after, so the limit is a limit on the stack this function can use rather
// than on the tree it has already built.
JsonStatus parseValue(Cursor& c, std::vector<JsonValue>& values, int depth, size_t* outIndex);

JsonStatus parseString(Cursor& c, std::string* out) {
    if (c.peek() != '"') {
        return JsonStatus::UnexpectedCharacter;
    }
    ++c.p;
    out->clear();
    while (true) {
        if (c.done()) {
            return JsonStatus::UnterminatedString;
        }
        const char ch = *c.p++;
        if (ch == '"') {
            return JsonStatus::Ok;
        }
        if (ch != '\\') {
            out->push_back(ch);
            continue;
        }
        if (c.done()) {
            return JsonStatus::UnterminatedString;
        }
        const char escape = *c.p++;
        switch (escape) {
            case '"': out->push_back('"'); break;
            case '\\': out->push_back('\\'); break;
            case '/': out->push_back('/'); break;
            case 'b': out->push_back('\b'); break;
            case 'f': out->push_back('\f'); break;
            case 'n': out->push_back('\n'); break;
            case 'r': out->push_back('\r'); break;
            case 't': out->push_back('\t'); break;
            case 'u': {
                // A \uXXXX escape is decoded to UTF-8. Surrogate pairs are
                // joined; a lone surrogate is passed through as the replacement
                // character rather than refused, because a stray surrogate in a
                // node NAME must not stop a file's geometry from being read.
                if (c.end - c.p < 4) {
                    return JsonStatus::InvalidEscape;
                }
                uint32_t code = 0;
                for (int i = 0; i < 4; ++i) {
                    const char h = *c.p++;
                    code <<= 4;
                    if (h >= '0' && h <= '9') {
                        code |= static_cast<uint32_t>(h - '0');
                    } else if (h >= 'a' && h <= 'f') {
                        code |= static_cast<uint32_t>(h - 'a' + 10);
                    } else if (h >= 'A' && h <= 'F') {
                        code |= static_cast<uint32_t>(h - 'A' + 10);
                    } else {
                        return JsonStatus::InvalidEscape;
                    }
                }
                if (code >= 0xD800 && code <= 0xDBFF && c.end - c.p >= 6 && c.p[0] == '\\'
                    && c.p[1] == 'u') {
                    uint32_t low = 0;
                    bool lowOk = true;
                    for (int i = 0; i < 4; ++i) {
                        const char h = c.p[2 + i];
                        low <<= 4;
                        if (h >= '0' && h <= '9') {
                            low |= static_cast<uint32_t>(h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            low |= static_cast<uint32_t>(h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            low |= static_cast<uint32_t>(h - 'A' + 10);
                        } else {
                            lowOk = false;
                            break;
                        }
                    }
                    if (lowOk && low >= 0xDC00 && low <= 0xDFFF) {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        c.p += 6;
                    }
                }
                if (code >= 0xD800 && code <= 0xDFFF) {
                    code = 0xFFFD;
                }
                if (code < 0x80) {
                    out->push_back(static_cast<char>(code));
                } else if (code < 0x800) {
                    out->push_back(static_cast<char>(0xC0 | (code >> 6)));
                    out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else if (code < 0x10000) {
                    out->push_back(static_cast<char>(0xE0 | (code >> 12)));
                    out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
                } else {
                    out->push_back(static_cast<char>(0xF0 | (code >> 18)));
                    out->push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                    out->push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    out->push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                break;
            }
            default:
                return JsonStatus::InvalidEscape;
        }
    }
}

// JSON's number grammar, checked before `strtod` sees the text.
//
// strtod alone would accept "nan", "inf", "0x10" and a leading "+", none of
// which is JSON — and the first two would put a non-finite value straight into
// geometry, which is exactly the failure this parser exists to prevent.
JsonStatus parseNumber(Cursor& c, double* out) {
    const char* start = c.p;
    if (c.peek() == '-') {
        ++c.p;
    }
    if (c.done() || !isDigit(c.peek())) {
        return JsonStatus::InvalidNumber;
    }
    if (c.peek() == '0') {
        ++c.p;  // a leading zero may not be followed by more digits
        if (!c.done() && isDigit(c.peek())) {
            return JsonStatus::InvalidNumber;
        }
    } else {
        while (!c.done() && isDigit(c.peek())) {
            ++c.p;
        }
    }
    if (!c.done() && c.peek() == '.') {
        ++c.p;
        if (c.done() || !isDigit(c.peek())) {
            return JsonStatus::InvalidNumber;
        }
        while (!c.done() && isDigit(c.peek())) {
            ++c.p;
        }
    }
    if (!c.done() && (c.peek() == 'e' || c.peek() == 'E')) {
        ++c.p;
        if (!c.done() && (c.peek() == '+' || c.peek() == '-')) {
            ++c.p;
        }
        if (c.done() || !isDigit(c.peek())) {
            return JsonStatus::InvalidNumber;
        }
        while (!c.done() && isDigit(c.peek())) {
            ++c.p;
        }
    }
    const std::string text(start, static_cast<size_t>(c.p - start));
    char* stop = nullptr;
    const double value = std::strtod(text.c_str(), &stop);
    if (stop != text.c_str() + text.size()) {
        return JsonStatus::InvalidNumber;
    }
    // A finite decimal can still overflow to infinity (1e400). Refused here so
    // no consumer has to wonder whether its own finiteness check came first.
    if (!std::isfinite(value)) {
        return JsonStatus::InvalidNumber;
    }
    *out = value;
    return JsonStatus::Ok;
}

bool matchLiteral(Cursor& c, const char* literal) {
    const size_t n = std::strlen(literal);
    if (static_cast<size_t>(c.end - c.p) < n) {
        return false;
    }
    if (std::memcmp(c.p, literal, n) != 0) {
        return false;
    }
    c.p += n;
    return true;
}

JsonStatus parseValue(Cursor& c, std::vector<JsonValue>& values, int depth, size_t* outIndex) {
    if (depth > kMaxJsonDepth) {
        return JsonStatus::DepthExceeded;
    }
    if (values.size() >= kMaxJsonValues) {
        return JsonStatus::TooManyValues;
    }
    c.skipWhitespace();
    if (c.done()) {
        return JsonStatus::UnexpectedEnd;
    }

    const size_t self = values.size();
    values.emplace_back();
    *outIndex = self;

    const char ch = c.peek();
    if (ch == '{') {
        ++c.p;
        values[self].type = JsonType::Object;
        c.skipWhitespace();
        if (c.peek() == '}') {
            ++c.p;
            return JsonStatus::Ok;
        }
        while (true) {
            c.skipWhitespace();
            std::string key;
            const JsonStatus keyStatus = parseString(c, &key);
            if (keyStatus != JsonStatus::Ok) {
                return keyStatus;
            }
            c.skipWhitespace();
            if (c.peek() != ':') {
                return JsonStatus::UnexpectedCharacter;
            }
            ++c.p;
            size_t child = 0;
            const JsonStatus valueStatus = parseValue(c, values, depth + 1, &child);
            if (valueStatus != JsonStatus::Ok) {
                return valueStatus;
            }
            values[self].keys.push_back(key);
            values[self].children.push_back(child);
            c.skipWhitespace();
            if (c.peek() == ',') {
                ++c.p;
                continue;
            }
            if (c.peek() == '}') {
                ++c.p;
                return JsonStatus::Ok;
            }
            return c.done() ? JsonStatus::UnexpectedEnd : JsonStatus::UnexpectedCharacter;
        }
    }
    if (ch == '[') {
        ++c.p;
        values[self].type = JsonType::Array;
        c.skipWhitespace();
        if (c.peek() == ']') {
            ++c.p;
            return JsonStatus::Ok;
        }
        while (true) {
            size_t child = 0;
            const JsonStatus elementStatus = parseValue(c, values, depth + 1, &child);
            if (elementStatus != JsonStatus::Ok) {
                return elementStatus;
            }
            values[self].children.push_back(child);
            c.skipWhitespace();
            if (c.peek() == ',') {
                ++c.p;
                continue;
            }
            if (c.peek() == ']') {
                ++c.p;
                return JsonStatus::Ok;
            }
            return c.done() ? JsonStatus::UnexpectedEnd : JsonStatus::UnexpectedCharacter;
        }
    }
    if (ch == '"') {
        values[self].type = JsonType::String;
        return parseString(c, &values[self].text);
    }
    if (ch == '-' || isDigit(ch)) {
        values[self].type = JsonType::Number;
        return parseNumber(c, &values[self].number);
    }
    if (matchLiteral(c, "true")) {
        values[self].type = JsonType::Bool;
        values[self].boolean = true;
        return JsonStatus::Ok;
    }
    if (matchLiteral(c, "false")) {
        values[self].type = JsonType::Bool;
        values[self].boolean = false;
        return JsonStatus::Ok;
    }
    if (matchLiteral(c, "null")) {
        values[self].type = JsonType::Null;
        return JsonStatus::Ok;
    }
    return JsonStatus::UnexpectedCharacter;
}

}  // namespace

JsonStatus JsonDocument::parse(const char* text, size_t length) {
    values_.clear();
    status_ = JsonStatus::Empty;
    if (text == nullptr || length == 0) {
        return status_;
    }
    Cursor c{text, text + length};
    size_t rootIndex = 0;
    JsonStatus status = parseValue(c, values_, 0, &rootIndex);
    if (status == JsonStatus::Ok) {
        c.skipWhitespace();
        // GLB pads its JSON chunk with spaces, which skipWhitespace consumes.
        // Anything else after the root is a second document, not padding.
        if (!c.done()) {
            status = JsonStatus::TrailingContent;
        }
    }
    if (status != JsonStatus::Ok) {
        // Nothing partial is kept: a half-built tree is a tree a caller can
        // read plausible-looking wrong values out of.
        values_.clear();
    }
    status_ = status;
    return status_;
}

const JsonValue* JsonDocument::member(const JsonValue& object, const char* key) const {
    if (object.type != JsonType::Object) {
        return nullptr;
    }
    for (size_t i = 0; i < object.keys.size(); ++i) {
        if (object.keys[i] == key) {
            return &values_[object.children[i]];
        }
    }
    return nullptr;
}

const JsonValue* JsonDocument::element(const JsonValue& array, size_t index) const {
    if (array.type != JsonType::Array || index >= array.children.size()) {
        return nullptr;
    }
    return &values_[array.children[index]];
}

bool JsonDocument::numberMember(const JsonValue& object, const char* key, double* out) const {
    const JsonValue* value = member(object, key);
    if (value == nullptr || value->type != JsonType::Number) {
        return false;
    }
    *out = value->number;
    return true;
}

bool JsonDocument::stringMember(const JsonValue& object, const char* key,
                                std::string* out) const {
    const JsonValue* value = member(object, key);
    if (value == nullptr || value->type != JsonType::String) {
        return false;
    }
    *out = value->text;
    return true;
}

bool JsonDocument::boolMember(const JsonValue& object, const char* key, bool* out) const {
    const JsonValue* value = member(object, key);
    if (value == nullptr || value->type != JsonType::Bool) {
        return false;
    }
    *out = value->boolean;
    return true;
}

}  // namespace forgeshape
