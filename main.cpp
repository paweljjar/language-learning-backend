#include <string>
#include <map>
#include <functional>
#include "HttpServer.h"

using namespace HTTPServer;

int main() {
    Server server(8080);

    server.get("/", [&server](const ServerRequest& req, ServerResponse& res) {
        Json response = Json::Object{
            {"message", "Hello, World!"}
        };

        res.body = response.dump(4);
        res.status = HTTP_OK;
        res.contentType = "text/json";
    });

    server.post("/users", [&server](const ServerRequest& req, ServerResponse& res) {
        Json body;
        try {
            body = Json::parse(req.body);
        } catch (const std::exception& e) {
            server.sendJson(res, HTTP_BAD_REQUEST, Json::Object{{"error", e.what()}});
            return;
        }

        if (!body.isObject() || !body.asObject().count("name") || !body["name"].isString()) {
            server.sendJson(res, HTTP_BAD_REQUEST, Json::Object{{"error", "'name' (string) is required"}});
            return;
        }

        server.sendJson(res, HTTP_CREATED, Json::Object{
            {"id", 3},
            {"name", body["name"].asString()}
        }, 4);
    });

    server.start();

    return 0;
}