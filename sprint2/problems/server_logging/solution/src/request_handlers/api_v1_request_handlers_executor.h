#pragma once

#include "request_handler_node.h"
#include "../api_v1_handlers/api_v1_activators.h"
#include "../api_v1_handlers/api_v1_handlers.h"

#include <vector>
#include <unordered_map>

namespace rh_storage {

namespace http = boost::beast::http;

template <typename Request, typename Send>
class ApiV1RequestHandlerExecutor {
public:
    using HandlerType = void(*)(const Request&, const model::Game&, Send&&);
    using ActivatorType = bool(*)(const Request&, const model::Game&);

    static ApiV1RequestHandlerExecutor& GetInstance() {
        static ApiV1RequestHandlerExecutor instance;
        return instance;
    }

    bool Execute(const Request& req, const model::Game& game, Send&& send) const {
        for (const auto& node : rh_storage_) {
            if (node.activator(req, game)) {
                auto it = node.handlers.find(req.method());
                if (it != node.handlers.end()) {
                    it->second(req, game, std::forward<Send>(send));
                    return true;
                }
            }
        }
        return false;
    }

private:
    ApiV1RequestHandlerExecutor() {
 
        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            BadRequestActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, BadRequestHandler}}
        });

        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            GetMapListActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, GetMapListHandler}}
        });

        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            MapNotFoundActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, MapNotFoundHandler}}
        });

        rh_storage_.emplace_back(RequestHandlerNode<ActivatorType, HandlerType>{
            GetMapByIdActivator,
            std::unordered_map<http::verb, HandlerType>{{http::verb::get, GetMapByIdHandler}}
        });
    }

    std::vector<RequestHandlerNode<ActivatorType, HandlerType>> rh_storage_;
};

} // namespace rh_storage
