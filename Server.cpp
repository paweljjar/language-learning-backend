//
// Created by PAWEL on 15/10/2026.
//

#include "Server.h"

#include <iostream>
#include <ranges>
#include <sstream>
#include <unistd.h>

#include "Json.h"

namespace HTTPServer {
    bool ServerRequest::hasQuery(const std::string &key) const {
        return query.contains(key);
    }

    std::string ServerRequest::getQuery(const std::string &key, const std::string &fallback) const {
        auto it = query.find(key);
        return it != query.end() ? it->second : fallback;
    }

    std::string ServerRequest::getHeader(const std::string& key, const std::string& fallback) const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : fallback;
    }

    //region public methods
    Server::Server(const int port) : _port(port), _server_fd(-1), _address{} {
        if (port < 0 || port > 65535) {
            throw std::invalid_argument("Port number must be between 0 and 65535");
        }
    }

    void Server::start() {
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

    void Server::get(const std::string& path, RequestHandler handler)  { _addRoute("GET",  path, std::move(handler)); }
    void Server::post(const std::string& path, RequestHandler handler) { _addRoute("POST", path, std::move(handler)); }

    int Server::getPort() const {
        return _port;
    }

    void Server::sendJson(ServerResponse& res, int status, const Json& json, int indent) {
        res.status = status;
        res.contentType = "application/json";
        res.body = json.dump(indent);
    }
    //endregion public methods

    //region private methods
    std::string Server::_toLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    std::string Server::_trim(const std::string& s) {
        size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos) return "";
        return s.substr(b, s.find_last_not_of(" \t") - b + 1);
    }

    void Server::_addRoute(const std::string& method, const std::string& path, RequestHandler handler) {
        _routes[method][path] = std::move(handler);
        std::cout << "Registered " << method << " route: " << path << std::endl;
    }

    bool Server::_createSocket() {
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

    bool Server::_bindSocket() {
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

    bool Server::_listenPort() const {
        std::cout << "Listening on socket..." << std::endl;
        if (listen(this->_server_fd, 10) == -1) {
            std::cerr << "Failed to listen on socket" << std::endl;
            return false;
        }

        std::cout << "Socket is now listening!" << std::endl;
        return true;
    }

    std::string Server::_getStatusMessage(int status) {
        switch (status) {
            case HTTP_OK:
                return "OK";
            case HTTP_NOT_FOUND:
                return "Not Found";
            case HTTP_INTERNAL_SERVER_ERROR:
                return "Internal Server Error";
            case 201: return "Created";
            case 400: return "Bad Request";
            case 405: return "Method Not Allowed";
            case 413: return "Payload Too Large";
            case 431: return "Request Header Fields Too Large";
            case 501: return "Not Implemented";
            default:
                return "Unknown Status";
        }
    }

    std::string Server::_buildResponse(const ServerResponse& response) {
        std::ostringstream out;
        out << "HTTP/1.1 " << response.status << " " << _getStatusMessage(response.status) << "\r\n"
                    << "Content-Type: " << response.contentType << "\r\n"
                    << "Content-Length: " << response.body.size() << "\r\n"
                    << "Connection: close\r\n\r\n"
                    << response.body;
        return out.str();
    }

    bool Server::_parseRequest(const std::string& head, ServerRequest& request) {
        size_t lineEnd = head.find("\r\n");
        std::istringstream line(head.substr(0, lineEnd));
        std::string target, version;

        if (!(line >> request.method >> target >> version)) {
            std::cerr << "Failed to parse request line" << std::endl;
            return false;
        }

        size_t q = target.find('?');
        request.path = target.substr(0, q);
        if (q != std::string::npos) {
            _parseQuery(target.substr(q + 1), request.query);
        }

        // Headers: "Name: value" lines
        if (lineEnd == std::string::npos) return true;
        size_t pos = lineEnd + 2;
        while (pos < head.size()) {
            size_t end = head.find("\r\n", pos);
            if (end == std::string::npos) end = head.size();
            std::string headerLine = head.substr(pos, end - pos);
            pos = end + 2;

            size_t colon = headerLine.find(':');
            if (colon == std::string::npos) return false; // malformed header
            request.headers[_toLower(headerLine.substr(0, colon))] = _trim(headerLine.substr(colon + 1));
        }
        return true;
    }

    int Server::_hexValue(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    std::string Server::_urlDecode(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '+') {
                out += ' ';
            } else if (s[i] == '%' && i + 2 < s.size() && _hexValue(s[i + 1]) >= 0 && _hexValue(s[i + 2]) >= 0) {
                out += static_cast<char>(_hexValue(s[i + 1]) * 16 + _hexValue(s[i + 2]));
                i += 2;
            } else {
                out += s[i];
            }
        }
        return out;
    }

    void Server::_parseQuery(const std::string& queryString, std::map<std::string, std::string>& out) {
        std::istringstream stream(queryString);
        std::string pair;
        while (std::getline(stream, pair, '&')) {
            if (pair.empty()) continue;
            size_t eq = pair.find('=');
            std::string key   = eq == std::string::npos ? pair : pair.substr(0, eq);
            std::string value = eq == std::string::npos ? ""   : pair.substr(eq + 1);
            out[_urlDecode(key)] = _urlDecode(value);
        }
    }

    void Server::_handleClient(int client_fd) {
        ServerRequest request;
        ServerResponse response;

        int status = _readRequest(client_fd, request);
        if (status < 0) { close(client_fd); return; }

        if (status != 0) _setError(response, status);
        else             _dispatch(request, response);

        std::string out = _buildResponse(response);
        write(client_fd, out.c_str(), out.size());
        close(client_fd);
    }

    void Server::_acceptLoop() {
        while (true) {
            sockaddr_in client{};
            socklen_t len = sizeof(client);
            const int client_fd = accept(this->_server_fd, reinterpret_cast<sockaddr*>(&client), &len);
            if (client_fd == -1) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }
            _handleClient(client_fd);
        }
    }

    int Server::_readRequest(int client_fd, ServerRequest &request) {
        std::string raw;
        char buffer[4096];

        size_t headEnd;
        while ((headEnd = raw.find("\r\n\r\n")) == std::string::npos) {
            if (raw.size() > MAX_HEADER_SIZE) return 431; // headers too large
            ssize_t n = read(client_fd, buffer, sizeof(buffer));
            if (n <= 0) return -1;
            raw.append(buffer, static_cast<size_t>(n));
        }

        if (!_parseRequest(raw.substr(0, headEnd), request)) return HTTP_BAD_REQUEST;
        std::string body = raw.substr(headEnd + 4); // whatever of the body already arrived

        if (request.headers.contains("transfer-encoding")) return 501; // chunked not supported

        size_t contentLength = 0;
        auto it = request.headers.find("content-length");
        if (it != request.headers.end()) {
            const std::string& value = it->second;
            if (value.empty() || !std::isdigit(static_cast<unsigned char>(value[0]))) return HTTP_BAD_REQUEST;
            try {
                size_t used = 0;
                contentLength = std::stoull(value, &used);
                if (used != value.size()) return HTTP_BAD_REQUEST;
            } catch (const std::exception&) {
                return HTTP_BAD_REQUEST;
            }
            if (contentLength > MAX_BODY_SIZE) return 413;
        }

        auto expect = request.headers.find("expect");
        if (body.size() < contentLength && expect != request.headers.end() &&
            _toLower(expect->second) == "100-continue") {
            std::string cont = "HTTP/1.1 100 Continue\r\n\r\n";
            write(client_fd, cont.c_str(), cont.size());
        }

        while (body.size() < contentLength) {
            ssize_t n = read(client_fd, buffer, sizeof(buffer));
            if (n <= 0) return -1;
            body.append(buffer, static_cast<size_t>(n));
        }
        body.resize(contentLength);

        request.body = std::move(body);
        return 0;
    }

    void Server::_setError(ServerResponse& res, int status) {
        sendJson(res, status, Json::Object{{"error", _getStatusMessage(status)}});
    }

    void Server::_dispatch(const ServerRequest& request, ServerResponse& response) {
        auto methodIt = _routes.find(request.method);
        if (methodIt != _routes.end()) {
            auto it = methodIt->second.find(request.path);
            if (it != methodIt->second.end()) {
                try {
                    it->second(request, response);
                } catch (const std::exception& e) {
                    std::cerr << "Handler threw: " << e.what() << std::endl;
                    _setError(response, HTTP_INTERNAL_SERVER_ERROR);
                }
                return;
            }
        }

        // Path exists, but not for this method -> 405 instead of 404
        for (const auto &val: _routes | std::views::values) {
            if (val.contains(request.path)) {
                _setError(response, 405);
                return;
            }
        }
        _setError(response, HTTP_NOT_FOUND);
    }
    //endregion private methods
} // HTTPServer