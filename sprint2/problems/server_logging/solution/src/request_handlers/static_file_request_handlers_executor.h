#pragma once

#include <functional>
#include <vector>

#include "request_handler_node.h"
#include "static_file_request_handlers_storage.h"

namespace rh_storage {

template <typename Request, typename Send>
class StaticFileRequestHandlerExecutor {
 public:
  using ActivatorType = bool (*)(const Request&, const std::filesystem::path&);
  using HandlerType =
      void (*)(const Request&, const std::filesystem::path&, Send&&);

  StaticFileRequestHandlerExecutor(const StaticFileRequestHandlerExecutor&) =
      delete;
  StaticFileRequestHandlerExecutor& operator=(
      const StaticFileRequestHandlerExecutor&) = delete;
  StaticFileRequestHandlerExecutor(StaticFileRequestHandlerExecutor&&) = delete;
  StaticFileRequestHandlerExecutor& operator=(
      StaticFileRequestHandlerExecutor&&) = delete;

  static StaticFileRequestHandlerExecutor& GetInstance() {
    static StaticFileRequestHandlerExecutor instance;
    return instance;
  }

  bool Execute(const Request& req,
               const std::filesystem::path& static_content_root,
               Send&& send) {
    for (const auto& node : handler_storage_) {
      if (node.GetActivator()(req, static_content_root)) {
        node.GetHandler(req, fault_handler_)(req, static_content_root,
                                             std::move(send));
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<RequestHandlerNode<ActivatorType, HandlerType>> handler_storage_ =
      {RequestHandlerNode<ActivatorType, HandlerType>(
           StaticContentFileNotFoundActivator,
           {{http::verb::get, StaticContentFileNotFoundHandler}}),
       RequestHandlerNode<ActivatorType, HandlerType>(
           LeaveStaticContentRootDirActivator,
           {{http::verb::get, LeaveStaticContentRootDirHandler}}),
       RequestHandlerNode<ActivatorType, HandlerType>(
           GetStaticContentFileActivator,
           {{http::verb::get, GetStaticContentFileHandler}})};

  HandlerType fault_handler_ = StaticContentFileNotFoundHandler;

  StaticFileRequestHandlerExecutor() = default;
};

}