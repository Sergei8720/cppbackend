#pragma once
#include "filesystem_utils.h" 
#include "logger.h"

#include <vector>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <string>
#include <fstream>

namespace rh_storage{

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;
using StringResponse = http::response<http::string_body>;
using namespace std::literals;

const std::unordered_map<std::string, std::string> EXTENSION_FILE_TO_CONTENT_TYPE = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".css", "text/css"},
    {".txt", "text/plain"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpe", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".ico", "image/vnd.microsoft.icon"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},
    {".svg", "image/svg+xml"},
    {".svgz", "image/svg+xml"},
    {".mp3", "audio/mpeg"}
};

const std::string INDEX_FILE_NAME{"index.html"};

template <typename Request>
fs::path GetStaticFilePath(const Request& req, const fs::path& static_content_root) {
    fs::path static_content = static_content_root;
    
    if (req.target().empty() || req.target() == "/") {
        static_content = fs::weakly_canonical(static_content_root / INDEX_FILE_NAME);
    } else {
        std::string_view pathStr = req.target().substr(1);
        static_content = fs::weakly_canonical(static_content_root / pathStr);
        
        if (fs::is_directory(static_content)) {
            static_content = fs::weakly_canonical(static_content / INDEX_FILE_NAME);
        }
    }
    
    return static_content;
}

template <typename Request>
bool StaticContentFileNotFoundActivator(
        const Request& req,
        const fs::path& static_content_root) {
    fs::path static_content = GetStaticFilePath(req, static_content_root);
    return !fs::exists(static_content);
}

template <typename Request, typename Send>
void StaticContentFileNotFoundHandler(
        const Request& req,
        const fs::path& static_content_root,
        Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "text/plain");
    response.body() = "File not found";
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool LeaveStaticContentRootDirActivator(
        const Request& req,
        const fs::path& static_content_root) {
    fs::path static_content = GetStaticFilePath(req, static_content_root);
    return !fs_utils::IsSubPath(static_content, static_content_root);
}

template <typename Request, typename Send>
void LeaveStaticContentRootDirHandler(
        const Request& req,
        const fs::path& static_content_root,
        Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, "text/plain");
    response.body() = "Invalid file path";
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool GetStaticContentFileActivator(
        const Request& req,
        const fs::path& static_content_root) {
    return true;
}

template <typename Request, typename Send>
void GetStaticContentFileHandler(
        const Request& req,
        const fs::path& static_content_root,
        Send&& send) {
    fs::path file_path = GetStaticFilePath(req, static_content_root);
    
    http::response<http::file_body> response(http::status::ok, req.version());
    
    auto ext = file_path.extension().string();
    auto content_type_it = EXTENSION_FILE_TO_CONTENT_TYPE.find(ext);
    if (content_type_it != EXTENSION_FILE_TO_CONTENT_TYPE.end()) {
        response.set(http::field::content_type, content_type_it->second);
    } else {
        response.set(http::field::content_type, "application/octet-stream");
    }
    
    http::file_body::value_type file;
    sys::error_code ec;
    file.open(file_path.c_str(), beast::file_mode::read, ec);
    
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(0, "Failed to open file", ec.what()));
        
        StringResponse error_response(http::status::internal_server_error, req.version());
        error_response.set(http::field::content_type, "text/plain");
        error_response.body() = "Internal server error";
        error_response.content_length(error_response.body().size());
        error_response.keep_alive(req.keep_alive());
        send(error_response);
        return;
    }
    
    response.body() = std::move(file);
    response.prepare_payload();
    response.keep_alive(req.keep_alive());
    send(response);
}

}