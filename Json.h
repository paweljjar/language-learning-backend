//
// Created by PAWEL on 15/10/2026.
//

#ifndef LANGUAGE_LEARNING_BACKEND_JSON_H
#define LANGUAGE_LEARNING_BACKEND_JSON_H
#include <cstddef>
#include <map>
#include <vector>

namespace HTTPServer {
    class Json {
        public:
            using Null   = std::nullptr_t;
            using Array  = std::vector<Json>;
            using Object = std::map<std::string, Json>;

            // Constructors (must stay in header because they are templates or inline)
            Json() : _value(nullptr) {}
            Json(std::nullptr_t) : _value(nullptr) {}
            Json(bool b) : _value(b) {}
            template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
            Json(T n) : _value(static_cast<double>(n)) {}
            Json(const char* s) : _value(std::string(s)) {}
            Json(std::string s) : _value(std::move(s)) {}
            Json(Array a) : _value(std::move(a)) {}
            Json(Object o) : _value(std::move(o)) {}

            // Public API
            std::string dump(int indent = -1) const;
            static Json parse(const std::string& text);

            bool isNull()   const;
            bool isBool()   const;
            bool isNumber() const;
            bool isString() const;
            bool isArray()  const;
            bool isObject() const;

            bool asBool() const;
            double asNumber() const;
            const std::string& asString() const;
            const Array& asArray() const;
            const Object& asObject() const;

            const Json& operator[](const std::string& key) const;
            const Json& operator[](size_t i) const;

        private:
            std::variant<Null, bool, double, std::string, Array, Object> _value;

            // Internal Helpers
            static void _newline(std::string& out, int indent, int depth);
            static std::string _numberToString(double n);
            static void _escape(const std::string& s, std::string& out);
            void _dump(std::string& out, int indent, int depth) const;

            // Parser is a helper class, usually kept in the header or a private internal header
            class Parser {
                public:
                    explicit Parser(const std::string& s);
                    Json parseDocument();
                private:
                    static constexpr int MAX_DEPTH = 512;
                    const std::string& _s;
                    size_t _pos = 0;

                    [[noreturn]] void _fail(const std::string& msg) const;
                    static bool _isDigit(char c);
                    void _skipWs();
                    void _consume(char expected);
                    void _expectWord(const char* word);
                    Json _parseValue(int depth);
                    Json _parseObject(int depth);
                    Json _parseArray(int depth);
                    unsigned _parseHex4();
                    static void _appendUtf8(std::string& out, unsigned cp);
                    std::string _parseString();
                    void _requireDigits();
                    Json _parseNumber();
            };
    };
} // HTTPServer

#endif //LANGUAGE_LEARNING_BACKEND_JSON_H
