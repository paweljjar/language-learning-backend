//
// Created by PAWEL on 15/10/2026.
//

#ifndef LANGUAGE_LEARNING_BACKEND_SERVER_H
#define LANGUAGE_LEARNING_BACKEND_SERVER_H
#include <map>
#include <string>
#include <functional>
#include <netinet/in.h>

#include "Json.h"

namespace HTTPServer {
#define HTTP_INTERNAL_SERVER_ERROR 500
#define HTTP_OK 200
#define HTTP_CREATED 201
#define HTTP_BAD_REQUEST 400
#define HTTP_NOT_FOUND 404

    struct ServerRequest {
        std::string method;
        std::string path;
        std::map<std::string, std::string> query;
        std::map<std::string, std::string> headers;
        std::string body;

        [[nodiscard]] bool hasQuery(const std::string& key) const;
        [[nodiscard]] std::string getQuery(const std::string& key, const std::string& fallback = "") const;
        [[nodiscard]] std::string getHeader(const std::string& key, const std::string& fallback = "") const;
    };

    struct ServerResponse {
        int status = HTTP_OK;
        std::string body;
        std::string contentType = "text/plain";
    };

    using RequestHandler = std::function<void(const ServerRequest&, ServerResponse&)>;

    class Server {
        public:
            explicit Server(int port);
            void start();
            void get(const std::string& path, RequestHandler handler);
            void post(const std::string& path, RequestHandler handler);
            int getPort() const;

            static void sendJson(ServerResponse& res, int status, const Json& json, int indent = -1);

        private:
            int _port;
            int _server_fd;
            sockaddr_in _address;
            std::map<std::string, std::map<std::string, RequestHandler>> _routes; // method -> path -> handler

            static constexpr int MAX_HEADER_SIZE = 16 * 1024;
            static constexpr int MAX_BODY_SIZE = 1024 * 1024; // 1 MB

            static std::string _toLower(std::string s);
            static std::string _trim(const std::string& s);
            void _addRoute(const std::string& method, const std::string& path, RequestHandler handler);
            bool _createSocket();
            bool _bindSocket();
            bool _listenPort() const;
            static std::string _getStatusMessage(int status);
            static std::string _buildResponse(const ServerResponse& response) ;
            static bool _parseRequest(const std::string& raw, ServerRequest& request);
            static int _hexValue(char c);
            static std::string _urlDecode(const std::string& s);
            static void _parseQuery(const std::string& queryString, std::map<std::string, std::string>& out);
            void _handleClient(int client_fd);
            void _acceptLoop();

            static int _readRequest(int client_fd, ServerRequest& request);

            static void _setError(ServerResponse& res, int status);
            void _dispatch(const ServerRequest& request, ServerResponse& response);
    };
}

#endif //LANGUAGE_LEARNING_BACKEND_SERVER_H
