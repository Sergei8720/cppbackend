#pragma once
#include "api_v1_request_handlers_storage.h"
#include "request_handler_node.h"

#include <vector>

namespace rh_storage {

template<typename Request, typename Send>
class ApiV1RequestHandlerExecutor {
 public:
  using ActivatorType = bool(*)(const Request&, const model::Game&);
  using HandlerType = void(*)(const Request&, const model::Game&, Send&&);

  ApiV1RequestHandlerExecutor(const ApiV1RequestHandlerExecutor&) = delete;
  ApiV1RequestHandlerExecutor& operator=(const ApiV1RequestHandlerExecutor&) = delete;
  ApiV1RequestHandlerExecutor(ApiV1RequestHandlerExecutor&&) = delete;
  ApiV1RequestHandlerExecutor& operator=(ApiV1RequestHandlerExecutor&&) = delete;

  static ApiV1RequestHandlerExecutor& GetInstance() {
    static ApiV1RequestHandlerExecutor instance;
    return instance;
  }

  bool Execute(const Request& req, const model::Game& game, Send&& send) const {
    for (const auto& handler_node : storage_) {
      if (handler_node.GetActivator()(req, game)) {
        handler_node.GetHandler(req, fault_handler_)(req, game, std::move(send));
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<RequestHandlerNode<ActivatorType, HandlerType>> storage_ = {
    RequestHandlerNode<ActivatorType, HandlerType>(
      BadRequestActivator<Request>,
      {{http::verb::get, BadRequestHandler<Request, Send>}}),
    RequestHandlerNode<ActivatorType, HandlerType>(
      GetMapListActivator<Request>,
      {{http::verb::get, GetMapListHandler<Request, Send>}}),
    RequestHandlerNode<ActivatorType, HandlerType>(
      MapNotFoundActivator<Request>,
      {{http::verb::get, MapNotFoundHandler<Request, Send>}}),
    RequestHandlerNode<ActivatorType, HandlerType>(
      GetMapByIdActivator<Request>,
      {{http::verb::get, GetMapByIdHandler<Request, Send>}})
  };
  
  const HandlerType fault_handler_ = BadRequestHandler<Request, Send>;

  ApiV1RequestHandlerExecutor() = default;
};

}  // namespace rh_storage