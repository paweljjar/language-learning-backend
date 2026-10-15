//
// Created by PAWEL on 15/10/2026.
//

#include "Json.h"

namespace HTTPServer {
    std::string Json::dump(int indent) const {
        std::string out;
        _dump(out, indent, 0);
        return out;
    }

    Json Json::parse(const std::string& text) {
        return Parser(text).parseDocument();
    }

    bool Json::isNull()   const { return std::holds_alternative<Null>(_value); }
    bool Json::isBool()   const { return std::holds_alternative<bool>(_value); }
    bool Json::isNumber() const { return std::holds_alternative<double>(_value); }
    bool Json::isString() const { return std::holds_alternative<std::string>(_value); }
    bool Json::isArray()  const { return std::holds_alternative<Array>(_value); }
    bool Json::isObject() const { return std::holds_alternative<Object>(_value); }

    bool Json::asBool() const                { return std::get<bool>(_value); }
    double Json::asNumber() const            { return std::get<double>(_value); }
    const std::string& Json::asString() const { return std::get<std::string>(_value); }
    const Json::Array& Json::asArray() const   { return std::get<Array>(_value); }
    const Json::Object& Json::asObject() const { return std::get<Object>(_value); }

    const Json& Json::operator[](const std::string& key) const { return asObject().at(key); }
    const Json& Json::operator[](size_t i) const               { return asArray().at(i); }

    void Json::_newline(std::string& out, int indent, int depth) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent) * depth, ' ');
    }

    std::string Json::_numberToString(double n) {
        if (!std::isfinite(n)) return "null";
        if (n == std::floor(n) && std::fabs(n) < 1e15)
            return std::to_string(static_cast<long long>(n));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.15g", n);
        if (std::strtod(buf, nullptr) != n)
            std::snprintf(buf, sizeof(buf), "%.17g", n);
        return buf;
    }

    void Json::_escape(const std::string& s, std::string& out) {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (c < 0x20) {
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        out += '"';
    }

    void Json::_dump(std::string& out, int indent, int depth) const {
        if (std::holds_alternative<Null>(_value)) {
            out += "null";
        } else if (auto* b = std::get_if<bool>(&_value)) {
            out += *b ? "true" : "false";
        } else if (auto* n = std::get_if<double>(&_value)) {
            out += _numberToString(*n);
        } else if (auto* s = std::get_if<std::string>(&_value)) {
            _escape(*s, out);
        } else if (auto* a = std::get_if<Array>(&_value)) {
            if (a->empty()) { out += "[]"; return; }
            out += '[';
            bool first = true;
            for (const auto& item : *a) {
                if (!first) out += ',';
                first = false;
                _newline(out, indent, depth + 1);
                item._dump(out, indent, depth + 1);
            }
            _newline(out, indent, depth);
            out += ']';
        } else if (auto* o = std::get_if<Object>(&_value)) {
            if (o->empty()) { out += "{}"; return; }
            out += '{';
            bool first = true;
            for (const auto& [key, value] : *o) {
                if (!first) out += ',';
                first = false;
                _newline(out, indent, depth + 1);
                _escape(key, out);
                out += indent >= 0 ? ": " : ":";
                value._dump(out, indent, depth + 1);
            }
            _newline(out, indent, depth);
            out += '}';
        }
    }

    Json::Parser::Parser(const std::string& s) : _s(s) {}

    Json Json::Parser::parseDocument() {
        Json v = _parseValue(0);
        _skipWs();
        if (_pos != _s.size()) _fail("unexpected trailing characters");
        return v;
    }

    void Json::Parser::_fail(const std::string& msg) const {
        throw std::runtime_error("JSON parse error at position " + std::to_string(_pos) + ": " + msg);
    }

    bool Json::Parser::_isDigit(char c) { return c >= '0' && c <= '9'; }

    void Json::Parser::_skipWs() {
        while (_pos < _s.size() && (_s[_pos] == ' ' || _s[_pos] == '\t' || _s[_pos] == '\n' || _s[_pos] == '\r'))
            ++_pos;
    }

    void Json::Parser::_consume(char expected) {
        _skipWs();
        if (_pos >= _s.size() || _s[_pos] != expected)
            _fail(std::string("expected '") + expected + "'");
        ++_pos;
    }

    void Json::Parser::_expectWord(const char* word) {
        const size_t len = std::strlen(word);
        if (_s.compare(_pos, len, word) != 0) _fail("invalid literal");
        _pos += len;
    }

    Json Json::Parser::_parseValue(int depth) {
        if (depth > MAX_DEPTH) _fail("nesting too deep");
        _skipWs();
        if (_pos >= _s.size()) _fail("unexpected end of input");
        switch (char c = _s[_pos]) {
            case '{': return _parseObject(depth);
            case '[': return _parseArray(depth);
            case '"': return Json(_parseString());
            case 't': _expectWord("true");  return Json(true);
            case 'f': _expectWord("false"); return Json(false);
            case 'n': _expectWord("null");  return Json(nullptr);
            default:
                if (c == '-' || _isDigit(c)) return _parseNumber();
                _fail("unexpected character");
        }
    }

    Json Json::Parser::_parseObject(int depth) {
        ++_pos;
        Object obj;
        _skipWs();
        if (_pos < _s.size() && _s[_pos] == '}') { ++_pos; return Json(std::move(obj)); }
        while (true) {
            _skipWs();
            if (_pos >= _s.size() || _s[_pos] != '"') _fail("expected string key");
            std::string key = _parseString();
            _consume(':');
            obj[std::move(key)] = _parseValue(depth + 1);
            _skipWs();
            if (_pos < _s.size() && _s[_pos] == ',') { ++_pos; continue; }
            _consume('}');
            break;
        }
        return Json(std::move(obj));
    }

    Json Json::Parser::_parseArray(int depth) {
        ++_pos;
        Array arr;
        _skipWs();
        if (_pos < _s.size() && _s[_pos] == ']') { ++_pos; return Json(std::move(arr)); }
        while (true) {
            arr.push_back(_parseValue(depth + 1));
            _skipWs();
            if (_pos < _s.size() && _s[_pos] == ',') { ++_pos; continue; }
            _consume(']');
            break;
        }
        return Json(std::move(arr));
    }

    unsigned Json::Parser::_parseHex4() {
        if (_pos + 4 > _s.size()) _fail("incomplete \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = _s[_pos++];
            v <<= 4;
            if      (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else _fail("invalid hex digit in \\u escape");
        }
        return v;
    }

    void Json::Parser::_appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::string Json::Parser::_parseString() {
        ++_pos;
        std::string out;
        while (true) {
            if (_pos >= _s.size()) _fail("unterminated string");
            unsigned char c = static_cast<unsigned char>(_s[_pos++]);
            if (c == '"') return out;
            if (c < 0x20) _fail("unescaped control character in string");
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (_pos >= _s.size()) _fail("unterminated escape");
            switch (_s[_pos++]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'b':  out += '\b'; break;
                case 'f':  out += '\f'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    unsigned cp = _parseHex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (_s.compare(_pos, 2, "\\u") != 0) _fail("lone high surrogate");
                        _pos += 2;
                        unsigned lo = _parseHex4();
                        if (lo < 0xDC00 || lo > 0xDFFF) _fail("invalid low surrogate");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        _fail("lone low surrogate");
                    }
                    _appendUtf8(out, cp);
                    break;
                }
                default: _fail("invalid escape sequence");
            }
        }
    }

    void Json::Parser::_requireDigits() {
        if (_pos >= _s.size() || !_isDigit(_s[_pos])) _fail("expected digit");
        while (_pos < _s.size() && _isDigit(_s[_pos])) ++_pos;
    }

    Json Json::Parser::_parseNumber() {
        size_t start = _pos;
        if (_s[_pos] == '-') ++_pos;
        if (_pos < _s.size() && _s[_pos] == '0') ++_pos;
        else _requireDigits();
        if (_pos < _s.size() && _s[_pos] == '.') { ++_pos; _requireDigits(); }
        if (_pos < _s.size() && (_s[_pos] == 'e' || _s[_pos] == 'E')) {
            ++_pos;
            if (_pos < _s.size() && (_s[_pos] == '+' || _s[_pos] == '-')) ++_pos;
            _requireDigits();
        }
        return Json(std::strtod(_s.substr(start, _pos - start).c_str(), nullptr));
    }
} // HTTPServer