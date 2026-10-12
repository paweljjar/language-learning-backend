#include <string>
#include <map>
#include <functional>
#include "HttpServer.h"

using namespace HTTPServer;

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

    server.start();

    return 0;
} 