//
// Created by PAWEL on 15/10/2026.
//

#include "Database.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace HTTPServer {
    Database::Database(std::string path, bool createIfNotExists) : _path(std::move(path)), _createIfNotExists(createIfNotExists) { // Construct Database with the given file path and auto-creation flag, then load data
        _load(); // Load and parse database content from disk into memory
    }

    const std::string& Database::getPath() const { // Return the path of the database file
        return _path; // Return the internal filesystem path string
    }

    size_t Database::collectionCount() const { // Return the total count of top-level collections in the database
        return _data.size(); // Return the number of entries in the in-memory JSON object map
    }

    void Database::_load() { // Load and parse JSON content from the database file on disk, creating a new file if configured
        if (!std::filesystem::exists(_path)) { // Check if the database file does not exist on disk
            if (_createIfNotExists) { // Check if auto-creation of a missing database file is enabled
                std::cout << "No database file at " << _path << ", creating new one" << std::endl; // Log an informational message indicating that a new database file is being created

                std::ofstream outfile(_path); // Open an output file stream to create the new database file

                if (!outfile) { // Verify if the file stream was opened successfully
                    throw std::runtime_error("Failed to create database file at " + _path); // Throw an exception if file creation fails
                }

                outfile.write("{}", 2); // Write an empty JSON object literal ("{}") as initial database content
                outfile.close(); // Close the output file stream after writing

                return; // Return early since the new empty database is now initialized
            }
            throw std::runtime_error("Database file not found: " + _path); // Throw an exception if the file does not exist and creation is not allowed
        }

        std::ifstream file(_path, std::ios::binary); // Open an input file stream in binary mode to read the database file
        if (!file) { // Verify that the database file was opened successfully
            throw std::runtime_error("Cannot open database file: " + _path); // Throw an exception if the database file cannot be opened
        }

        std::stringstream buffer; // String stream buffer to read entire file content
        buffer << file.rdbuf(); // Read all bytes from the file stream buffer into the string stream
        const std::string content = buffer.str(); // Extract the accumulated content as a standard string

        if (content.empty()) return; // If file content is empty, return early without parsing

        Json parsed = Json::parse(content); // Parse the file content string using the JSON parser
        if (!parsed.isObject()) { // Validate that the parsed JSON root structure is a JSON object
            throw std::runtime_error("Database file is corrupted: expected a JSON object at top level"); // Throw an exception if root JSON value is not an object
        }

        _data = parsed.asObject(); // Store the parsed JSON object map in the internal database data container
        std::cout << "Loaded database from " << _path // Log successful database loading with path
                  << " (" << _data.size() << " collections)" << std::endl; // Output the total count of collections loaded from the database
    }
} // HTTPServer
