#pragma once

#include <boost/beast/http.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "filesystem_utils.h"
#include "logger.h"

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;

using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using namespace std::literals;

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
    {".mp3", "audio/mpeg"}};

const std::string kIndexFileName = "index.html";

namespace {

fs::path GetFullStaticPath(const fs::path& static_content_root,
                           std::string_view target) {
  fs::path static_content = static_content_root;

  if (target.empty() || target == "/") {
    static_content /= kIndexFileName;
    return fs::weakly_canonical(static_content);
  }

  std::string path_str(target);
  if (path_str[0] == '/') {
    path_str = path_str.substr(1);
  }
  
  fs::path relative_path(path_str);
  static_content /= relative_path;
  static_content = fs::weakly_canonical(static_content);

  if (fs::is_directory(static_content)) {
    static_content /= kIndexFileName;
    static_content = fs::weakly_canonical(static_content);
  }

  return static_content;
}

}  // namespace

template <typename Request>
bool StaticContentFileNotFoundActivator(const Request& req,
                                         const fs::path& static_content_root) {
  fs::path full_path = GetFullStaticPath(static_content_root, req.target());
  return !fs::exists(full_path) || !fs::is_regular_file(full_path);
}

template <typename Request, typename Send>
void StaticContentFileNotFoundHandler(const Request& req,
                                      const fs::path& static_content_root,
                                      Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "File not found";
  response.prepare_payload();
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool LeaveStaticContentRootDirActivator(const Request& req,
                                         const fs::path& static_content_root) {
  fs::path static_content = static_content_root;
  std::string path_str(req.target());
  if (!path_str.empty() && path_str[0] == '/') {
    path_str = path_str.substr(1);
  }
  
  fs::path relative_path(path_str);
  static_content /= relative_path;
  static_content = fs::weakly_canonical(static_content);
  
  return !fs_utils::IsSubPath(static_content, static_content_root);
}

template <typename Request, typename Send>
void LeaveStaticContentRootDirHandler(const Request& req,
                                      const fs::path& static_content_root,
                                      Send&& send) {
  StringResponse response(http::status::bad_request, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Bad request";
  response.prepare_payload();
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool GetStaticContentFileActivator(const Request& req,
                                   const fs::path& static_content_root) {
  return true;
}

template <typename Request, typename Send>
void GetStaticContentFileHandler(const Request& req,
                                 const fs::path& static_content_root,
                                 Send&& send) {
  fs::path full_path = GetFullStaticPath(static_content_root, req.target());

  FileResponse response;
  response.version(req.version());
  response.result(http::status::ok);

  std::string ext = full_path.extension().string();
  auto extension_iterator = kExtensionToContentType.find(ext);
  if (extension_iterator != kExtensionToContentType.end()) {
    response.set(http::field::content_type, extension_iterator->second);
  } else {
    response.set(http::field::content_type, "application/octet-stream");
  }

  http::file_body::value_type file;
  sys::error_code ec;
  file.open(full_path.string().c_str(), beast::file_mode::read, ec);

  if (ec) {
    logware::ErrorLogData error_data;
    error_data.code = ec.value();
    error_data.text = ec.message();
    error_data.where = "open_file";
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error", error_data);
    
    StringResponse error_response(http::status::internal_server_error, req.version());
    error_response.set(http::field::content_type, "text/plain");
    error_response.body() = "Internal server error";
    error_response.prepare_payload();
    error_response.keep_alive(req.keep_alive());
    send(std::move(error_response));
    return;
  }

  response.body() = std::move(file);
  response.prepare_payload();
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

}  // namespace rh_storage