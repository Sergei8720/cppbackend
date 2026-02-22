#ifndef STATIC_FILE_RESPONSE_STORAGE_H_
#define STATIC_FILE_RESPONSE_STORAGE_H_

#include <boost/beast/http.hpp>
#include <filesystem>
#include <unordered_map>
#include <string>

#include "filesystem_utils.h"
#include "utils/url_utils.h"

namespace response_storage {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;

const std::unordered_map<std::string, std::string> kExtensionToContentType = {
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

const std::string kIndexFileName = "index.html";

fs::path GetStaticFilePath(const fs::path& static_content_root,
                           std::string_view target);

std::string GetContentType(const fs::path& file_path);

template <typename Body, typename Allocator>
bool UseStaticContentFileNotFoundActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  fs::path file_path = GetStaticFilePath(static_content_root, req.target());
  return !fs::exists(file_path);
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeStaticContentFileNotFoundResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  http::response<http::string_body> response(
      http::status::not_found, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "File not found";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
bool UseLeaveStaticContentRootDirActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  fs::path file_path = GetStaticFilePath(static_content_root, req.target());
  return !fs_utils::IsSubPath(file_path, static_content_root);
}

template <typename Body, typename Allocator>
http::response<http::string_body> MakeLeaveStaticContentRootDirResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  http::response<http::string_body> response(
      http::status::bad_request, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Invalid path";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  return response;
}

template <typename Body, typename Allocator>
bool UseGetStaticContentFileActivator(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  return true;
}

template <typename Body, typename Allocator>
http::response<http::file_body> MakeGetStaticContentFileResponse(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    const fs::path& static_content_root) {
  http::response<http::file_body> response;
  response.version(11);
  response.result(http::status::ok);

  fs::path file_path = GetStaticFilePath(static_content_root, req.target());
  
  response.set(http::field::content_type, GetContentType(file_path));
  
  http::file_body::value_type file;
  sys::error_code ec;
  
  file.open(file_path.c_str(), beast::file_mode::read, ec);
  if (ec) {
    response.result(http::status::internal_server_error);
    response.body() = {};
  } else {
    response.body() = std::move(file);
  }
  
  response.prepare_payload();
  return response;
}

}  // namespace response_storage

#endif  // STATIC_FILE_RESPONSE_STORAGE_H_