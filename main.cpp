#include <string>
#include <map>
#include <functional>
#include <iostream>

#include "Database.h"
#include "Json.h"
#include "Server.h"

using namespace HTTPServer;

int main() {
    Server server(8000, "Backend"); // Create a server instance on port 8000
    Database db("database.json", true); // Create a database instance with the specified file and enabled creation if it doesn't exist
    Server frontend(8080, "Frontend"); // Create a server instance on port 8080

    server.get("/", [&server](const ServerRequest&, ServerResponse& res) { // Define a GET route for the root path
        server.sendJson(res, HTTP_OK, Json::Object{ // Send a JSON response with status 200 OK
            {"message", "Hello World!"}
        });
    });

    std::thread serverThread = server.startAsync(); // Start the server to listen for incoming requests
    std::thread frontendThread = frontend.startAsync(); // Start the frontend server to listen for incoming requests

    serverThread.join();
    frontendThread.join();

    return 0;
}
