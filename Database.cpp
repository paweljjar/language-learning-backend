//
// Created by PAWEL on 15/10/2026.
//

#include "Database.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace HTTPServer {
    Database::Database(std::string path, bool createIfNotExists) : _path(std::move(path)), _createIfNotExists(createIfNotExists) {
        _load();
    }

    const std::string& Database::getPath() const {
        return _path;
    }

    size_t Database::collectionCount() const {
        return _data.size();
    }

    void Database::_load() {
        if (!std::filesystem::exists(_path)) {
            if (_createIfNotExists) {
                std::cout << "No database file at " << _path << ", creating new one" << std::endl;

                std::ofstream outfile(_path);

                if (!outfile) {
                    throw std::runtime_error("Failed to create database file at " + _path);
                }

                outfile.write("{}", 2);
                outfile.close();

                return;
            }
            throw std::runtime_error("Database file not found: " + _path);
        }

        std::ifstream file(_path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open database file: " + _path);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string content = buffer.str();

        if (content.empty()) return;

        Json parsed = Json::parse(content);
        if (!parsed.isObject()) {
            throw std::runtime_error("Database file is corrupted: expected a JSON object at top level");
        }

        _data = parsed.asObject();
        std::cout << "Loaded database from " << _path
                  << " (" << _data.size() << " collections)" << std::endl;
    }
} // HTTPServer