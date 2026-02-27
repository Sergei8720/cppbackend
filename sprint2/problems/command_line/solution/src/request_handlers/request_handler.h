#pragma once
#include "http_server.h"
#include "application.h"
#include "api_v1_request_handlers_executor.h"
#include "static_file_request_handlers_executor.h"

#include <filesystem>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
using StringResponse = http::response<http::string_body>;
namespace fs = std::filesystem;

class RequestHandler {
public:
    explicit RequestHandler(app::Application& application, fs::path static_content_root_path)
        : application_{application}, static_content_root_path_{std::move(static_content_root_path)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        using RequestType = http::request<Body, http::basic_fields<Allocator>>;
        
        if (rh_storage::ApiV1RequestHandlerExecutor<RequestType, Send>
            ::GetInstance().Execute(req, application_, std::forward<Send>(send))) {
            return;
        }
        
        if (rh_storage::StaticFileRequestHandlerExecutor<RequestType, Send>
            ::GetInstance().Execute(req, static_content_root_path_, std::forward<Send>(send))) {
            return;
        }
        
        rh_storage::PageNotFoundHandler(req, application_, std::forward<Send>(send));
    }

private:
    app::Application& application_;
    fs::path static_content_root_path_;
};

}