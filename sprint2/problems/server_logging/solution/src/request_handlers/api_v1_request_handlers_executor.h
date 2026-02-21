#pragma once

#include <functional>
#include <vector>

#include "api_v1_request_handlers_storage.h"
#include "request_handler_node.h"

namespace rh_storage {

template <typename Request, typename Send>
class ApiV1RequestHandlerExecutor {
 public:
  using ActivatorType = bool (*)(const Request&, const model::Game&);
  using HandlerType = void (*)(const Request&, const model::Game&, Send&&);

  ApiV1RequestHandlerExecutor(const ApiV1RequestHandlerExecutor&) = delete;
  ApiV1RequestHandlerExecutor& operator=(const ApiV1RequestHandlerExecutor&) =
      delete;
  ApiV1RequestHandlerExecutor(ApiV1RequestHandlerExecutor&&) = delete;
  ApiV1RequestHandlerExecutor& operator=(ApiV1RequestHandlerExecutor&&) =
      delete;

  static ApiV1RequestHandlerExecutor& GetInstance() {
    static ApiV1RequestHandlerExecutor instance;
    return instance;
  }

  bool Execute(const Request& req, const model::Game& game, Send&& send) {
    for (const auto& node : handler_storage_) {
      if (node.GetActivator()(req, game)) {
        node.GetHandler(req, fault_handler_)(req, game, std::move(send));
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<RequestHandlerNode<ActivatorType, HandlerType>> handler_storage_ =
      {RequestHandlerNode<ActivatorType, HandlerType>(
           BadRequestActivator, {{http::verb::get, BadRequestHandler}}),
       RequestHandlerNode<ActivatorType, HandlerType>(
           GetMapListActivator, {{http::verb::get, GetMapListHandler}}),
       RequestHandlerNode<ActivatorType, HandlerType>(
           MapNotFoundActivator, {{http::verb::get, MapNotFoundHandler}}),
       RequestHandlerNode<ActivatorType, HandlerType>(
           GetMapByIdActivator, {{http::verb::get, GetMapByIdHandler}})};

  HandlerType fault_handler_ = BadRequestHandler;

  ApiV1RequestHandlerExecutor() = default;
};

}