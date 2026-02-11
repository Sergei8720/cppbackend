#pragma once
#include "filesystem_utils.h"
#include "logger.h"
#include <vector>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <string>
#include <fstream>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;
using StringResponse = http::response<http::string_body>;
using namespace std::literals;

const std::unordered_map<std::string, std::string> ExtensionFileToContentType = {
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

const std::string IndexFileName = "index.html";

template <typename Request>
bool StaticContentFileNotFoundActivator(const Request& req, const fs::path& static_content_root) {
    fs::path static_content = static_content_root;
    
    std::string target(req.target());
    if (target.empty() || target == "/") {
        static_content = fs::weakly_canonical(static_content_root / IndexFileName);
    } else {
        std::string path_str;
        if (target[0] == '/') {
            path_str = target.substr(1);
        } else {
            path_str = target;
        }
        fs::path rel_path(path_str);
        static_content = fs::weakly_canonical(static_content_root / rel_path);
        
        if (fs::is_directory(static_content)) {
            static_content = fs::weakly_canonical(static_content / IndexFileName);
        }
    }
    
    return !fs::exists(static_content);
}

template <typename Request, typename Send>
void StaticContentFileNotFoundHandler(const Request& req, const fs::path& static_content_root, Send&& send) {
    StringResponse response(http::status::not_found, req.version());
    response.set(http::field::content_type, "text/plain");
    response.body() = "Static content file not found.";
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool LeaveStaticContentRootDirActivator(const Request& req, const fs::path& static_content_root) {
    std::string target(req.target());
    if (target.empty() || target == "/") {
        return false;
    }
    
    std::string path_str;
    if (target[0] == '/') {
        path_str = target.substr(1);
    } else {
        path_str = target;
    }
    
    fs::path rel_path(path_str);
    fs::path static_content = fs::weakly_canonical(static_content_root / rel_path);
    
    return !fs_utils::IsSubPath(static_content, static_content_root);
}

template <typename Request, typename Send>
void LeaveStaticContentRootDirHandler(const Request& req, const fs::path& static_content_root, Send&& send) {
    StringResponse response(http::status::bad_request, req.version());
    response.set(http::field::content_type, "text/plain");
    response.body() = "Try to leave static content root directory.";
    response.content_length(response.body().size());
    response.keep_alive(req.keep_alive());
    send(response);
}

template <typename Request>
bool GetStaticContentFileActivator(const Request& req, const fs::path& static_content_root) {
    return true;
}

template <typename Request, typename Send>
void GetStaticContentFileHandler(const Request& req, const fs::path& static_content_root, Send&& send) {
    http::response<http::file_body> response;
    response.version(11);
    response.result(http::status::ok);

    fs::path static_content = static_content_root;
    
    std::string target(req.target());
    if (target.empty() || target == "/") {
        static_content = fs::weakly_canonical(static_content_root / IndexFileName);
    } else {
        std::string path_str;
        if (target[0] == '/') {
            path_str = target.substr(1);
        } else {
            path_str = target;
        }
        fs::path rel_path(path_str);
        static_content = fs::weakly_canonical(static_content_root / rel_path);
    }
    
    std::string extension = static_content.extension().string();
    auto it = ExtensionFileToContentType.find(extension);
    if (it != ExtensionFileToContentType.end()) {
        response.insert(http::field::content_type, it->second);
    } else {
        response.insert(http::field::content_type, "application/octet-stream");
    }
    
    http::file_body::value_type file;
    sys::error_code ec;
    
    file.open(static_content.string(), beast::file_mode::read, ec);
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error",
            logware::ExceptionLogData(ec.value(), "Failed to open static content file", ec.message()));
        
        StringResponse error_response(http::status::internal_server_error, req.version());
        error_response.set(http::field::content_type, "text/plain");
        error_response.body() = "Failed to open file.";
        error_response.content_length(error_response.body().size());
        error_response.keep_alive(req.keep_alive());
        send(error_response);
        return;
    }
    
    response.body() = std::move(file);
    response.prepare_payload();
    send(response);
}

}