//
// Created by PAWEL on 15/10/2026.
//

#ifndef LANGUAGE_LEARNING_BACKEND_JSON_H
#define LANGUAGE_LEARNING_BACKEND_JSON_H
#include <cstddef>
#include <map>
#include <vector>

namespace HTTPServer {
    class Json { // Lightweight DOM-style JSON value class supporting null, boolean, number, string, array, and object types
        public:
            using Null   = std::nullptr_t; // Type alias representing the JSON null primitive
            using Array  = std::vector<Json>; // Type alias representing a JSON array as a sequence of Json values
            using Object = std::map<std::string, Json>; // Type alias representing a JSON object as an ordered key-value map

            // Constructors (must stay in header because they are templates or inline)
            Json() : _value(nullptr) {} // Default constructor initializing the Json instance to null
            Json(std::nullptr_t) : _value(nullptr) {} // Explicit nullptr constructor initializing the Json instance to null
            Json(bool b) : _value(b) {} // Boolean constructor initializing the Json instance with a boolean value
            template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
            Json(T n) : _value(static_cast<double>(n)) {} // Numeric constructor converting any arithmetic type (except bool) to double
            Json(const char* s) : _value(std::string(s)) {} // C-string constructor initializing the Json instance with a string value
            Json(std::string s) : _value(std::move(s)) {} // String constructor moving an existing std::string into the Json value
            Json(Array a) : _value(std::move(a)) {} // Array constructor moving an existing vector of Json values into the instance
            Json(Object o) : _value(std::move(o)) {} // Object constructor moving an existing key-value map of Json values into the instance

            // Public API
            std::string dump(int indent = -1) const; // Serialize the JSON value into a string, with optional indentation (-1 for compact, >=0 for pretty-print)
            static Json parse(const std::string& text); // Parse a JSON-formatted string into a corresponding Json object hierarchy

            bool isNull()   const; // Check if the underlying variant currently holds a null value
            bool isBool()   const; // Check if the underlying variant currently holds a boolean value
            bool isNumber() const; // Check if the underlying variant currently holds a numeric (double) value
            bool isString() const; // Check if the underlying variant currently holds a string value
            bool isArray()  const; // Check if the underlying variant currently holds an array value
            bool isObject() const; // Check if the underlying variant currently holds an object value

            bool asBool() const; // Extract the underlying boolean value (throws std::bad_variant_access if not boolean)
            double asNumber() const; // Extract the underlying numeric double value (throws std::bad_variant_access if not number)
            const std::string& asString() const; // Return a const reference to the underlying string (throws std::bad_variant_access if not string)
            const Array& asArray() const; // Return a const reference to the underlying Array (throws std::bad_variant_access if not array)
            const Object& asObject() const; // Return a const reference to the underlying Object map (throws std::bad_variant_access if not object)

            const Json& operator[](const std::string& key) const; // Access an object member by key using const map lookup (throws if not object or key missing)
            const Json& operator[](size_t i) const; // Access an array element by numeric index (throws if not array or index out of range)

        private:
            std::variant<Null, bool, double, std::string, Array, Object> _value; // Internal variant holding the current value across the supported JSON data types

            // Internal Helpers
            static void _newline(std::string& out, int indent, int depth); // Append newline and indentation spaces corresponding to current nesting depth if pretty-printing
            static std::string _numberToString(double n); // Convert a double-precision floating-point number into an accurate and clean JSON string representation
            static void _escape(const std::string& s, std::string& out); // Escape special characters and control codes in a string according to JSON specification
            void _dump(std::string& out, int indent, int depth) const; // Recursive serialization helper to format values with specified indentation and depth

            // Parser is a helper class, usually kept in the header or a private internal header
            class Parser { // Recursive descent JSON parser converting JSON text into Json values
                public:
                    explicit Parser(const std::string& s); // Construct a parser instance for the given input JSON string
                    Json parseDocument(); // Parse the complete JSON document and ensure no invalid trailing characters exist
                private:
                    static constexpr int MAX_DEPTH = 512; // Maximum recursion nesting depth allowed to guard against stack overflow attacks
                    const std::string& _s; // Reference to the input JSON string being parsed
                    size_t _pos = 0; // Current character cursor index within the input string

                    [[noreturn]] void _fail(const std::string& msg) const; // Throw a formatted runtime_error exception with current cursor position and error details
                    static bool _isDigit(char c); // Helper to check if a character is an ASCII decimal digit ('0'-'9')
                    void _skipWs(); // Advance cursor past ASCII whitespace characters (spaces, tabs, newlines, carriage returns)
                    void _consume(char expected); // Skip whitespace and consume the expected character, or throw an error if it does not match
                    void _expectWord(const char* word); // Match and consume an exact keyword literal (e.g. "true", "false", "null") at the current position
                    Json _parseValue(int depth); // Parse any valid JSON value (object, array, string, number, boolean, null) at given recursion depth
                    Json _parseObject(int depth); // Parse a JSON object enclosed in '{' and '}' into an Object map
                    Json _parseArray(int depth); // Parse a JSON array enclosed in '[' and ']' into an Array vector
                    unsigned _parseHex4(); // Parse exactly four hexadecimal digits following a '\\u' escape into an unsigned integer
                    static void _appendUtf8(std::string& out, unsigned cp); // Encode a Unicode code point as UTF-8 bytes and append them to the output string
                    std::string _parseString(); // Parse a quoted JSON string with escape sequence handling
                    void _requireDigits(); // Ensure at least one digit is present and consume all contiguous digits
                    Json _parseNumber(); // Parse a JSON numerical literal into a double-precision Json value
            };
    };
} // HTTPServer

#endif //LANGUAGE_LEARNING_BACKEND_JSON_H
