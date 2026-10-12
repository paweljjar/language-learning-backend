#ifndef SERVER_H
#define SERVER_H

#include <sstream>
#include <string>
#include <map>
#include <functional>
#include <netinet/in.h>
#include <iostream>
#include <unistd.h>

namespace HTTPServer {
    #define HTTP_INTERNAL_SERVER_ERROR 500
    #define HTTP_OK 200
    #define HTTP_NOT_FOUND 404

    struct ServerRequest {
        std::string method;
        std::string path;
        std::map<std::string, std::string> query;

        [[nodiscard]] bool hasQuery(const std::string& key) const;
        [[nodiscard]] std::string getQuery(const std::string& key, const std::string& fallback = "") const;
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
            int getPort() const;

        private:
            int _port;
            int _server_fd;
            sockaddr_in _address;
            std::map<std::string, RequestHandler> _getRoutes;

            // Move private method signatures here
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
