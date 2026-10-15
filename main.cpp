#include <string>
#include <map>
#include <functional>
#include "HttpServer.h"

using namespace HTTPServer;

int main() {
    Server server(8080);
    Database db("database.json", true);

    server.get("/", [&server](const ServerRequest&, ServerResponse& res) {
        server.sendJson(res, HTTP_OK, Json::Object{
            {"message", "Hello World!"}
        });
    });

    server.start();

    return 0;
}