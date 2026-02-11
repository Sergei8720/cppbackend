#pragma once

#include "static_file_request_handlers.h"
#include "static_file_handler_helpers.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;

inline bool StaticContentFileNotFoundActivator(const http::request<http::string_body>& req,
                                               const std::filesystem::path& static_content_path) {
    namespace fs = std::filesystem;

    auto target = req.target();
    if (target.starts_with("/static/")) {
        fs::path relative_path = std::string(target.substr(8));
        fs::path full_path = static_content_path / relative_path;
        full_path = fs::weakly_canonical(full_path);
        
        return !fs::exists(full_path) || !fs::is_regular_file(full_path);
    }
    return false;
}

template <typename Send>
void StaticContentFileNotFoundHandler(const http::request<http::string_body>& req,
                                      const std::filesystem::path& /*static_content_path*/,
                                      Send&& send) {
    std::string body = "File not found";
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = body;
    res.prepare_payload();
    send(std::move(res));
}

inline bool LeaveStaticContentRootDirActivator(const http::request<http::string_body>& req,
                                               const std::filesystem::path& static_content_path) {
    namespace fs = std::filesystem;

    auto target = req.target();
    if (target.starts_with("/static/")) {
        fs::path relative_path = std::string(target.substr(8));
        fs::path full_path = static_content_path / relative_path;
        full_path = fs::weakly_canonical(full_path);
        
        return !full_path.string().starts_with(static_content_path.string());
    }
    return false;
}

template <typename Send>
void LeaveStaticContentRootDirHandler(const http::request<http::string_body>& req,
                                      const std::filesystem::path& /*static_content_path*/,
                                      Send&& send) {
    std::string body = "Forbidden";
    http::response<http::string_body> res{http::status::forbidden, req.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = body;
    res.prepare_payload();
    send(std::move(res));
}

inline bool GetStaticContentFileActivator(const http::request<http::string_body>& req,
                                          const std::filesystem::path& static_content_path) {
    namespace fs = std::filesystem;

    auto target = req.target();
    if (target.starts_with("/static/")) {
        fs::path relative_path = std::string(target.substr(8));
        fs::path full_path = static_content_path / relative_path;
        full_path = fs::weakly_canonical(full_path);
        
        return fs::exists(full_path) && fs::is_regular_file(full_path) &&
               full_path.string().starts_with(static_content_path.string());
    }
    return false;
}

template <typename Send>
void GetStaticContentFileHandler(const http::request<http::string_body>& req,
                                 const std::filesystem::path& static_content_path,
                                 Send&& send) {
    namespace fs = std::filesystem;
    namespace beast = boost::beast;
    namespace http = beast::http;

    auto target = req.target();
    fs::path relative_path = std::string(target.substr(8));
    fs::path full_path = static_content_path / relative_path;
    full_path = fs::weakly_canonical(full_path);

    http::response<http::file_body> res;
    res.version(req.version());
    res.result(http::status::ok);
    
    http::file_body::value_type file;
    beast::error_code ec;
    
    // ИСПРАВЛЕНО: используем c_str() для преобразования string в const char*
    file.open(full_path.string().c_str(), beast::file_mode::read, ec);
    
    if (ec) {
        http::response<http::string_body> error_res{http::status::internal_server_error, req.version()};
        error_res.set(http::field::content_type, "text/plain");
        error_res.body() = "Failed to open file";
        error_res.prepare_payload();
        send(std::move(error_res));
        return;
    }
    
    res.body() = std::move(file);
    res.prepare_payload();
    
    auto mime_type = http_handler::GetMimeType(full_path.extension().string());
    res.set(http::field::content_type, mime_type);
    
    send(std::move(res));
}

} // namespace rh_storage
