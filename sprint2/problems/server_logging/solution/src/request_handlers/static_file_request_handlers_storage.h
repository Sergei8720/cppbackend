#pragma once

#include <boost/beast/http.hpp>
#include <filesystem>
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
  fs::path static_content{static_content_root};

  if (target.empty() || target == "/") {
    static_content = fs::weakly_canonical(static_content / kIndexFileName);
    return static_content;
  }

  std::string_view path_without_slash = target.substr(1);
  fs::path relative_path{path_without_slash};
  static_content = fs::weakly_canonical(static_content / relative_path);

  if (fs::is_directory(static_content)) {
    static_content = fs::weakly_canonical(static_content / kIndexFileName);
  }

  return static_content;
}

}

template <typename Request>
bool StaticContentFileNotFoundActivator(const Request& req,
                                         const fs::path& static_content_root) {
  fs::path full_path = GetFullStaticPath(static_content_root, req.target());
  return !fs::exists(full_path);
}

template <typename Request, typename Send>
void StaticContentFileNotFoundHandler(const Request& req,
                                      const fs::path& static_content_root,
                                      Send&& send) {
  StringResponse response(http::status::not_found, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Static content file not found.";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
}

template <typename Request>
bool LeaveStaticContentRootDirActivator(const Request& req,
                                         const fs::path& static_content_root) {
  fs::path static_content{static_content_root};
  std::string_view path_without_slash = req.target().substr(1);
  fs::path relative_path{path_without_slash};
  static_content = fs::weakly_canonical(static_content / relative_path);
  return !fs_utils::IsSubPath(static_content, static_content_root);
}

template <typename Request, typename Send>
void LeaveStaticContentRootDirHandler(const Request& req,
                                      const fs::path& static_content_root,
                                      Send&& send) {
  StringResponse response(http::status::bad_request, req.version());
  response.set(http::field::content_type, "text/plain");
  response.body() = "Try to leave static content root directory.";
  response.content_length(response.body().size());
  response.keep_alive(req.keep_alive());
  send(response);
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
  http::response<http::file_body> response;
  response.version(11);
  response.result(http::status::ok);

  fs::path full_path = GetFullStaticPath(static_content_root, req.target());

  auto extension_iterator = kExtensionToContentType.find(full_path.extension());
  if (extension_iterator != kExtensionToContentType.end()) {
    response.insert(http::field::content_type, extension_iterator->second);
  } else {
    response.insert(http::field::content_type, "application/octet-stream");
  }

  http::file_body::value_type file;
  sys::error_code ec;
  file.open(full_path.c_str(), beast::file_mode::read, ec);

  if (ec) {
    BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage(
        "error"sv, logware::ExceptionLogData(
                       0, "Failed to open static content file "sv, ec.what()));
  } else {
    response.body() = std::move(file);
  }

  response.prepare_payload();
  send(response);
}

}