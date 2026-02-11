#pragma once

#include "request_handler_node.h"
#include "static_file_request_handlers_storage.h"

#include <vector>
#include <unordered_map>
#include <filesystem>

namespace rh_storage {

namespace http = boost::beast::http;

template <typename Request, typename Send>
class StaticFileRequestHandlerExecutor {
public:
    using HandlerType = void(*)(const Request&, const std::filesystem::path&, Send&&);
    using ActivatorType = bool(*)(const Request&, const std::filesystem::path&);

    static StaticFileRequestHandlerExecutor& GetInstance() {
        static StaticFileRequestHandlerExecutor instance;
        return instance;
    }

    bool Execute(const Request& req, const std::filesystem::path& static_content_path, Send&& send) const {
        for (const auto& node : rh_storage_) {
            if (node.activator(req, static_content_path)) {
                auto it = node.handlers.find(req.method());
                if (it != node.handlers.end()) {
                    it->second(req, static_content_path, std::forward<Send>(send));
                    return true;
                }
            }
        }
        return false;
    }

private:
    StaticFileRequestHandlerExecutor() {
        // ИСПРАВЛЕНО: явно создаем объект RequestHandlerNode
        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            StaticContentFileNotFoundActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, StaticContentFileNotFoundHandler<Send>}}
        });

        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            LeaveStaticContentRootDirActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, LeaveStaticContentRootDirHandler<Send>}}
        });

        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            GetStaticContentFileActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, GetStaticContentFileHandler<Send>}}
        });
    }

    std::vector<RequestHandlerNode<ActivatorType, HandlerType>> rh_storage_;
};

} // namespace rh_storage
