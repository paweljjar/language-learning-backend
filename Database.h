//
// Created by PAWEL on 15/10/2026.
//

#ifndef LANGUAGE_LEARNING_BACKEND_DATABASE_H
#define LANGUAGE_LEARNING_BACKEND_DATABASE_H
#include <string>

#include "Json.h"

namespace HTTPServer {
    class Database {
        public:
            explicit Database(std::string path, bool createIfNotExists = false);

            Database(const Database&) = delete; // Prevent copy construction
            Database& operator=(const Database&) = delete; // Prevent copy assignment

            const std::string& getPath() const;
            size_t collectionCount() const;

        private:
            std::string _path;
            Json::Object _data;
            bool _createIfNotExists;

            void _load();
    };
} // HTTPServer

#endif //LANGUAGE_LEARNING_BACKEND_DATABASE_H
