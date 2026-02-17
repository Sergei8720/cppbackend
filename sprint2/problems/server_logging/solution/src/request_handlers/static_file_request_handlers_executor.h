#pragma once
#include "static_file_request_handlers_storage.h"
#include "request_handler_node.h"

#include <vector>

namespace rh_storage {

template<typename Request, typename Send>
class StaticFileRequestHandlerExecutor {
 public:
  using ActivatorType = bool(*)(const Request&, const std::filesystem::path&);
  using HandlerType = void(*)(const Request&, const std::filesystem::path&, Send&&);

  StaticFileRequestHandlerExecutor(const StaticFileRequestHandlerExecutor&) = delete;
  StaticFileRequestHandlerExecutor& operator=(const StaticFileRequestHandlerExecutor&) = delete;
  StaticFileRequestHandlerExecutor(StaticFileRequestHandlerExecutor&&) = delete;
  StaticFileRequestHandlerExecutor& operator=(StaticFileRequestHandlerExecutor&&) = delete;

  static StaticFileRequestHandlerExecutor& GetInstance() {
    static StaticFileRequestHandlerExecutor instance;
    return instance;
  }

  bool Execute(const Request& req, const std::filesystem::path& static_content_root, Send&& send) {
    for (const auto& handler_node : storage_) {
      if (handler_node.GetActivator()(req, static_content_root)) {
        handler_node.GetHandler(req, fault_handler_)(req, static_content_root, std::move(send));
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<RequestHandlerNode<ActivatorType, HandlerType>> storage_ = {
    RequestHandlerNode<ActivatorType, HandlerType>(
      StaticContentFileNotFoundActivator<Request>,
      {{http::verb::get, StaticContentFileNotFoundHandler<Request, Send>}}),
    RequestHandlerNode<ActivatorType, HandlerType>(
      LeaveStaticContentRootDirActivator<Request>,
      {{http::verb::get, LeaveStaticContentRootDirHandler<Request, Send>}}),
    RequestHandlerNode<ActivatorType, HandlerType>(
      GetStaticContentFileActivator<Request>,
      {{http::verb::get, GetStaticContentFileHandler<Request, Send>}})
  };
  
  HandlerType fault_handler_ = StaticContentFileNotFoundHandler<Request, Send>;

  StaticFileRequestHandlerExecutor() = default;
};

}  // namespace rh_storage