#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <functional>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <variant>
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <vector>

#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_OK 200
#define HTTP_NOT_FOUND 404

namespace {
    struct ServerRequest {
        std::string method;
        std::string path;
    };

    struct ServerResponse {
        int status = HTTP_OK;
        std::string body;
        std::string contentType = "text/plain";
    };

    using RequestHandler = std::function<void(const ServerRequest&, ServerResponse&)>;

    class Json {
        public:
            using Null   = std::nullptr_t;
            using Array  = std::vector<Json>;
            using Object = std::map<std::string, Json>;

            Json() : _value(nullptr) {}
            Json(std::nullptr_t) : _value(nullptr) {}
            Json(bool b) : _value(b) {}
            template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>>
            Json(T n) : _value(static_cast<double>(n)) {}
            Json(const char* s) : _value(std::string(s)) {}
            Json(std::string s) : _value(std::move(s)) {}
            Json(Array a) : _value(std::move(a)) {}
            Json(Object o) : _value(std::move(o)) {}

            std::string dump(int indent = -1) const {
                std::string out;
                _dump(out, indent, 0);
                return out;
            }

            static Json parse(const std::string& text) {
                return Parser(text).parseDocument();
            }

            bool isNull()   const { return std::holds_alternative<Null>(_value); }
            bool isBool()   const { return std::holds_alternative<bool>(_value); }
            bool isNumber() const { return std::holds_alternative<double>(_value); }
            bool isString() const { return std::holds_alternative<std::string>(_value); }
            bool isArray()  const { return std::holds_alternative<Array>(_value); }
            bool isObject() const { return std::holds_alternative<Object>(_value); }

            bool asBool() const                { return std::get<bool>(_value); }
            double asNumber() const            { return std::get<double>(_value); }
            const std::string& asString() const { return std::get<std::string>(_value); }
            const Array& asArray() const       { return std::get<Array>(_value); }
            const Object& asObject() const     { return std::get<Object>(_value); }

            const Json& operator[](const std::string& key) const { return asObject().at(key); }
            const Json& operator[](size_t i) const               { return asArray().at(i); }

        private:
            std::variant<Null, bool, double, std::string, Array, Object> _value;

            static void _newline(std::string& out, int indent, int depth) {
                if (indent < 0) return;
                out += '\n';
                out.append(static_cast<size_t>(indent) * depth, ' ');
            }

            static std::string _numberToString(double n) {
                if (!std::isfinite(n)) return "null";
                if (n == std::floor(n) && std::fabs(n) < 1e15)
                    return std::to_string(static_cast<long long>(n));

                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.15g", n);
                if (std::strtod(buf, nullptr) != n)
                    std::snprintf(buf, sizeof(buf), "%.17g", n);
                return buf;
            }

            static void _escape(const std::string& s, std::string& out) {
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

            void _dump(std::string& out, int indent, int depth) const {
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

            class Parser {
                public:
                    explicit Parser(const std::string& s) : _s(s) {}

                    Json parseDocument() {
                        Json v = _parseValue(0);
                        _skipWs();
                        if (_pos != _s.size()) _fail("unexpected trailing characters");
                        return v;
                    }
                private:
                    static constexpr int MAX_DEPTH = 512; // protects against stack overflow on "[[[[[[..."

                    const std::string& _s;
                    size_t _pos = 0;

                    [[noreturn]] void _fail(const std::string& msg) const {
                        throw std::runtime_error("JSON parse error at position " + std::to_string(_pos) + ": " + msg);
                    }

                    static bool _isDigit(char c) { return c >= '0' && c <= '9'; }

                    void _skipWs() {
                        while (_pos < _s.size() &&
                               (_s[_pos] == ' ' || _s[_pos] == '\t' || _s[_pos] == '\n' || _s[_pos] == '\r'))
                            ++_pos;
                    }

                    // Skips whitespace, then requires exactly this character
                    void _consume(char expected) {
                        _skipWs();
                        if (_pos >= _s.size() || _s[_pos] != expected)
                            _fail(std::string("expected '") + expected + "'");
                        ++_pos;
                    }

                    void _expectWord(const char* word) {
                        size_t len = std::char_traits<char>::length(word);
                        if (_s.compare(_pos, len, word) != 0) _fail("invalid literal");
                        _pos += len;
                    }

                    Json _parseValue(int depth) {
                        if (depth > MAX_DEPTH) _fail("nesting too deep");
                        _skipWs();
                        if (_pos >= _s.size()) _fail("unexpected end of input");

                        char c = _s[_pos];
                        switch (c) {
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

                    Json _parseObject(int depth) {
                        ++_pos; // skip '{'
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

                    Json _parseArray(int depth) {
                        ++_pos; // skip '['
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

                    unsigned _parseHex4() {
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

                    static void _appendUtf8(std::string& out, unsigned cp) {
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

                    std::string _parseString() {
                        ++_pos; // skip opening quote
                        std::string out;

                        while (true) {
                            if (_pos >= _s.size()) _fail("unterminated string");
                            unsigned char c = static_cast<unsigned char>(_s[_pos++]);

                            if (c == '"') return out;
                            if (c < 0x20) _fail("unescaped control character in string");
                            if (c != '\\') { out += static_cast<char>(c); continue; }

                            if (_pos >= _s.size()) _fail("unterminated escape");
                            char e = _s[_pos++];
                            switch (e) {
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
                                    if (cp >= 0xD800 && cp <= 0xDBFF) {          // high surrogate -> needs a low one
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

                    void _requireDigits() {
                        if (_pos >= _s.size() || !_isDigit(_s[_pos])) _fail("expected digit");
                        while (_pos < _s.size() && _isDigit(_s[_pos])) ++_pos;
                    }

                    // Grammar: -? (0 | [1-9][0-9]*) (. [0-9]+)? ([eE] [+-]? [0-9]+)?
                    Json _parseNumber() {
                        size_t start = _pos;
                        if (_s[_pos] == '-') ++_pos;

                        if (_pos < _s.size() && _s[_pos] == '0') ++_pos;   // no leading zeros like 012
                        else _requireDigits();

                        if (_pos < _s.size() && _s[_pos] == '.') { ++_pos; _requireDigits(); }

                        if (_pos < _s.size() && (_s[_pos] == 'e' || _s[_pos] == 'E')) {
                            ++_pos;
                            if (_pos < _s.size() && (_s[_pos] == '+' || _s[_pos] == '-')) ++_pos;
                            _requireDigits();
                        }

                        return Json(std::strtod(_s.substr(start, _pos - start).c_str(), nullptr));
                    }
                };
    };

    class Server {
        private:
            int _port;
            int _server_fd;
            sockaddr_in _address;
            std::map<std::string, RequestHandler> _getRoutes;

            bool _createSocket() {
                std::cout << "Creating socket..." << std::endl;
                this->_server_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (this->_server_fd == -1) {
                    std::cerr << "Failed to create socket" << std::endl;
                    return false;
                }

                int opt = 1;
                setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

                std::cout << "Socket created!" << std::endl;
                return true;
            }

            bool _bindSocket() {
                std::cout << "Binding socket..." << std::endl;
                this->_address.sin_family = AF_INET; // IPv4
                this->_address.sin_addr.s_addr = INADDR_ANY; // Bind to all available interfaces
                this->_address.sin_port = htons(this->_port); // Convert port number to network byte order

                // Bind the socket to the specified address and port
                if (bind(this->_server_fd, reinterpret_cast<sockaddr*>(&this->_address), sizeof(this->_address)) == -1) {
                    std::cerr << "Failed to bind socket" << std::endl;
                    return false;
                }

                std::cout << "Socket bound!" << std::endl;
                return true;
            }

            bool _listenPort() {
                std::cout << "Listening on socket..." << std::endl;
                if (listen(this->_server_fd, 10) == -1) {
                    std::cerr << "Failed to listen on socket" << std::endl;
                    return false;
                }

                std::cout << "Socket is now listening!" << std::endl;
                return true;
            }

            std::string _getStatusMessage(int status) {
                switch (status) {
                    case HTTP_OK:
                        return "OK";
                    case HTTP_NOT_FOUND:
                        return "Not Found";
                    case HTTP_INTERNAL_SERVER_ERROR:
                        return "Internal Server Error";
                    default:
                        return "Unknown Status";
                }
            }

            std::string _buildResponse(const ServerResponse& response) {
                std::ostringstream out;
                out << "HTTP/1.1 " << response.status << " " << this->_getStatusMessage(response.status) << "\r\n"
                            << "Content-Type: " << response.contentType << "\r\n"
                            << "Content-Length: " << response.body.size() << "\r\n"
                            << "Connection: close\r\n\r\n"
                            << response.body;
                return out.str();
            }

            bool _parseRequest(const std::string& raw, ServerRequest& request) {
                std::istringstream line(raw.substr(0, raw.find("\r\n")));
                std::string target, version;

                if (!(line >> request.method >> target >> version)) {
                    std::cerr << "Failed to parse request line" << std::endl;
                    return false;
                }

                request.path = target.substr(0, target.find('?'));

                return true;
            }

            void _handleClient(int client_fd) {
                char buffer[4096];
                ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
                if (n <= 0) { close(client_fd); return; }
                buffer[n] = '\0';

                ServerRequest request;
                ServerResponse response;

                if (!_parseRequest(buffer, request)) {
                    response.status = 400;
                    response.body = "Bad Request";
                } else if (request.method != "GET") {
                    response.status = 405;
                    response.body = "Method Not Allowed";
                } else {
                    auto it = _getRoutes.find(request.path);
                    if (it == _getRoutes.end()) {
                        response.status = 404;
                        response.body = "Not Found";
                    } else {
                        it->second(request, response); // call the registered handler
                    }
                }

                std::string out = _buildResponse(response);
                write(client_fd, out.c_str(), out.size());
                close(client_fd);
            }

            void _acceptLoop() {
                while (true) {
                    sockaddr_in client{};
                    socklen_t len = sizeof(client);
                    int client_fd = accept(this->_server_fd, reinterpret_cast<sockaddr*>(&client), &len);
                    if (client_fd == -1) {
                        std::cerr << "Failed to accept connection" << std::endl;
                        continue;
                    }
                    _handleClient(client_fd);
                }
            }
        public:
            explicit Server(const int port) : _port(port), _server_fd(0), _address() {
                if (port < 0 || port > 65535) {
                    throw std::invalid_argument("Port number must be between 0 and 65535");
                }
            }

            void start() {
                if (!this->_createSocket()) {
                    std::cerr << "Error creating socket" << std::endl;
                    return;
                }
                if (!this->_bindSocket()) {
                    std::cerr << "Error binding socket" << std::endl;
                    return;
                }
                if (!this->_listenPort()) {
                    std::cerr << "Error listening on socket" << std::endl;
                    return;
                }
                std::cout << "Server started on port " << this->getPort() << std::endl;
                _acceptLoop();
            }

            void get(const std::string& path, RequestHandler handler) {
                _getRoutes[path] = std::move(handler);
                std::cout << "Registered GET route: " << path << std::endl;
            }

            int getPort() {
                return _port;
            }
    };
}

int main() {
    Server server(8080);

    server.get("/", [](const ServerRequest& req, ServerResponse& res) {
        Json response = Json::Object{
            {"message", "Hello, World!"},
        };

        res.body = response.dump(4);
        res.status = HTTP_OK;
        res.contentType = "text/json";
    });

    server.start();

    return 0;
}