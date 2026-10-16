//
// Created by PAWEL on 15/10/2026.
//

#include "Database.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
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

    bool Database::hasTable(const std::string& tableName) const { // Check if a table exists in the database
        return _data.contains(tableName); // Return true if the table name exists in the internal data map
    }

    void Database::_validateScheme(const Json::Object& scheme) { // Validate that the provided table schema conforms to the required column structure
        if (scheme.empty()) { // Check if the schema object has no column definitions
            throw std::invalid_argument("Table scheme cannot be empty"); // Throw exception when no columns are specified
        }

        static const std::set<std::string> allowedTypes = { // Set of supported column data type names
            "integer", "string", "boolean", "number", "float", "array", "object" // Allowed primitive and composite column types
        };
        static const std::set<std::string> booleanOptions = { // Set of known boolean constraint option keys
            "required", "primary_key", "auto_increment", "unique" // Optional boolean column modifiers
        };

        for (const auto& [columnName, columnDef] : scheme) { // Iterate over each column name and its definition in the schema
            if (columnName.empty()) { // Check if the column name key is an empty string
                throw std::invalid_argument("Column name cannot be empty"); // Throw exception if a column name is empty
            }

            if (!columnDef.isObject()) { // Ensure that the column definition is a Json::Object
                throw std::invalid_argument("Column '" + columnName + "' definition must be a JSON object"); // Throw exception if column value is not an object
            }

            const Json::Object& colObj = columnDef.asObject(); // Extract the underlying Json::Object map for the column definition
            auto typeIt = colObj.find("type"); // Look up the required "type" property in the column definition
            if (typeIt == colObj.end() || !typeIt->second.isString()) { // Verify that "type" exists and holds a string value
                throw std::invalid_argument("Column '" + columnName + "' must have a string 'type' property"); // Throw exception if "type" is missing or not a string
            }

            const std::string& typeName = typeIt->second.asString(); // Retrieve the string value of the "type" property
            if (!allowedTypes.contains(typeName)) { // Check if the specified type name is in the set of allowed types
                throw std::invalid_argument("Column '" + columnName + "' has unsupported type '" + typeName + "'"); // Throw exception for unsupported column type
            }

            for (const auto& [optKey, optVal] : colObj) { // Iterate over all properties in the column definition to validate options
                if (booleanOptions.contains(optKey) && !optVal.isBool()) { // Check if a boolean option key holds a non-boolean value
                    throw std::invalid_argument("Option '" + optKey + "' in column '" + columnName + "' must be a boolean"); // Throw exception if boolean constraint is not a bool
                }
            }
        }
    }

    bool Database::createTable(const std::string& tableName, const Json::Object& scheme) { // Create a new table with the given name and schema in the database
        if (tableName.empty()) { // Check if the provided table name is empty
            throw std::invalid_argument("Table name cannot be empty"); // Throw exception if table name is empty
        }

        _validateScheme(scheme); // Validate that the schema contains valid column objects with required "type" fields

        if (_data.contains(tableName)) { // Check if a table with the same name already exists in the database
            return false; // Return false to indicate table was not created because it already exists
        }

        Json::Object table; // Create an empty JSON object to represent the table structure
        table["scheme"] = scheme; // Assign the provided schema definition to the "scheme" property
        table["records"] = Json::Array{}; // Initialize an empty JSON array for the "records" property

        _data[tableName] = table; // Store the new table object in the database under the table name key
        _save(); // Persist the updated database state to disk

        return true; // Return true to indicate successful table creation
    }

    void Database::save() const { // Persist in-memory database content to disk
        _save(); // Serialize and write database data to the storage file
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

    void Database::_save() const { // Serialize and persist in-memory database data to storage file on disk
        std::ofstream file(_path, std::ios::trunc); // Open database file stream in truncation mode to overwrite content
        if (!file) { // Verify that the database file was opened successfully for writing
            throw std::runtime_error("Cannot open database file for writing: " + _path); // Throw an exception if writing fails
        }

        file << Json(_data).dump(4); // Write formatted JSON string with 4-space indentation to file
        file.close(); // Close the output file stream after writing
    }
} // HTTPServer
