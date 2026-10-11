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
            {"message", "Hello, World!"}
        };

        res.body = response.dump(4);
        res.status = HTTP_OK;
        res.contentType = "text/json";
    });

    server.get("/users", [](const ServerRequest& req, ServerResponse& res) {
        Json response = Json::Array{
            Json::Object{{"id", 1}, {"name", "Alice"}},
            Json::Object{{"id", 2}, {"name", "Bob"}}
        };

        res.body = response.dump(4);
        res.status = HTTP_OK;
        res.contentType = "text/json";
    });

    server.start();

    return 0;
}