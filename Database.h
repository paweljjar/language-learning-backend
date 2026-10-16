//
// Created by PAWEL on 15/10/2026.
//

#ifndef LANGUAGE_LEARNING_BACKEND_DATABASE_H
#define LANGUAGE_LEARNING_BACKEND_DATABASE_H
#include <functional>
#include <string>

#include "Json.h"

namespace HTTPServer {
    class Database { // Simple file-backed JSON database that persists collections as key-value mappings
        public:
            explicit Database(std::string path, bool createIfNotExists = false); // Constructor that initializes database file path and loads or initializes the JSON file

            Database(const Database&) = delete; // Prevent copy construction to avoid concurrent access issues
            Database& operator=(const Database&) = delete; // Prevent copy assignment to avoid concurrent access issues

            const std::string& getPath() const; // Return a const reference to the database file path
            size_t collectionCount() const; // Return the number of collections (top-level keys) currently stored in the database
            bool hasTable(const std::string& tableName) const; // Check if a table with the given name exists in the database
            bool createTable(const std::string& tableName, const Json::Object& scheme); // Create a new table with the given name and validated column schema in the database
            Json::Object insert(const std::string& tableName, const Json::Object& record); // Validate and insert a new record into the specified table, returning the stored record
            Json::Array select(const std::string& tableName, const Json::Object& where = {}) const; // Query and return records from the specified table matching the given key-value filter criteria
            Json::Array select(const std::string& tableName, const std::function<bool(const Json::Object&)>& where) const; // Query and return records from the specified table matching the given predicate callback
            Json::Array update(const std::string& tableName, const Json::Object& updates, const Json::Object& where = {}); // Update records matching the key-value filter criteria and return the updated records
            Json::Array update(const std::string& tableName, const Json::Object& updates, const std::function<bool(const Json::Object&)>& where); // Update records matching the predicate callback and return the updated records
            Json::Array remove(const std::string& tableName, const Json::Object& where = {}); // Delete records matching the key-value filter criteria and return the removed records
            Json::Array remove(const std::string& tableName, const std::function<bool(const Json::Object&)>& where); // Delete records matching the predicate callback and return the removed records
            void save() const; // Persist in-memory database content to disk

        private:
            std::string _path; // Filesystem path to the JSON database storage file
            Json::Object _data; // In-memory JSON object storing database collections and documents
            bool _createIfNotExists; // Flag indicating whether to automatically create a new empty database file if none exists

            static void _validateScheme(const Json::Object& scheme); // Validate that the table schema contains valid column definitions with required type fields
            static bool _matchesType(const Json& value, const std::string& typeName); // Check whether a Json value matches the expected schema type name
            void _load(); // Read, parse, and validate JSON data from the storage file on disk
            void _save() const; // Serialize and persist in-memory database data to storage file on disk
    };
} // HTTPServer

#endif //LANGUAGE_LEARNING_BACKEND_DATABASE_H
