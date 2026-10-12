#ifndef SERVER_H
#define SERVER_H

#include <sstream>
#include <string>
#include <map>
#include <functional>
#include <netinet/in.h>

namespace HTTPServer {
    class Json;
    class Server;

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
            void sendJson(ServerResponse& res, int status, const Json& json, int indent = -1);

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
            int _readRequest(int client_fd, ServerRequest& request);
            void _setError(ServerResponse& res, int status);
            void _dispatch(const ServerRequest& request, ServerResponse& response);
    };

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
}


#endif
