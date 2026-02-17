#pragma once
#include "filesystem_utils.h"
#include "logger.h"

#include <vector>
#include <boost/beast/http.hpp>
#include <unordered_map>
#include <string>
#include <iostream>

namespace rh_storage {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;

using StringResponse = http::response<http::string_body>;

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

namespace {

fs::path BuildStaticContentPath(const fs::path& static_content_root, std::string_view target) {
  fs::path result = static_content_root;
  
  if (target.empty() || target == "/") {
    result /= kIndexFileName;
  } else {
    std::string_view path_without_slash = target.substr(1);
    result /= fs::path(std::string(path_without_slash));
    
    if (fs::is_directory(result)) {
      result /= kIndexFileName;
    }
  }
  
  return fs::weakly_canonical(result);
}

}  // namespace

template <typename Request>
bool StaticContentFileNotFoundActivator(const Request& req, const fs::path& static_content_root) {
  fs::path content_path = BuildStaticContentPath(static_content_root, req.target());
  return !fs::exists(content_path);
}

template <typename Request, typename Send>
void StaticContentFileNotFoundHandler(const Request& req, const fs::path& static_content_root, Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Static content file not found.";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
}

template <typename Request>
bool LeaveStaticContentRootDirActivator(const Request& req, const fs::path& static_content_root) {
  fs::path content_path = BuildStaticContentPath(static_content_root, req.target());
  return !fs_utils::IsSubPath(content_path, static_content_root);
}

template <typename Request, typename Send>
void LeaveStaticContentRootDirHandler(const Request& req, const fs::path& static_content_root, Send&& send) {
  StringResponse response(http::status::bad_request, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Attempt to leave static content root directory.";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(std::move(response));
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
  
  fs::path content_path = BuildStaticContentPath(static_content_root, req.target());
  
  auto extension = content_path.extension().string();
  auto content_type_it = kExtensionToContentType.find(extension);
  
  if (content_type_it != kExtensionToContentType.end()) {
    response.insert(http::field::content_type, content_type_it->second);
  } else {
    response.insert(http::field::content_type, "application/octet-stream");
  }
  
  http::file_body::value_type file;
  sys::error_code ec;
  
  if (file.open(content_path.c_str(), beast::file_mode::read, ec), ec) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
      "error",
      logware::ExceptionLogData(0, "Failed to open static content file", ec.what()));
  } else {
    response.body() = std::move(file);
  }
  
  response.prepare_payload();
  send(std::move(response));
}

}  // namespace rh_storage