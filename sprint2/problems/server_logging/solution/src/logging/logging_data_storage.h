#pragma once

#include <boost/beast/http.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace logware {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using HttpRequest = http::request<http::string_body>;
using namespace std::literals;

// Структуры для логирования
struct RequestLogData {
  std::string ip;
  std::string uri;
  std::string method;
};

struct ResponseLogData {
  long response_time = 0;
  int code = 0;
  std::string content_type;
};

struct ServerStartLogData {
  uint32_t port = 0;
  std::string address;
};

struct ServerExitLogData {
  int code = 0;
  std::optional<std::string> exception;
};

struct ErrorLogData {
  int code = 0;
  std::string text;
  std::string where;
};

// Функции для преобразования в JSON
void tag_invoke(json::value_from_tag, json::value& jv, const RequestLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ResponseLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ServerStartLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ServerExitLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ErrorLogData& data);

}  // namespace logware