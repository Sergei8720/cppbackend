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

// Ключи для JSON
inline const std::string kTimestamp = "timestamp";
inline const std::string kMessage = "message";
inline const std::string kData = "data";
inline const std::string kIp = "ip";
inline const std::string kUri = "URI";
inline const std::string kMethod = "method";
inline const std::string kResponseTime = "response_time";
inline const std::string kCode = "code";
inline const std::string kContentType = "content_type";
inline const std::string kPort = "port";
inline const std::string kAddress = "address";
inline const std::string kText = "text";
inline const std::string kWhere = "where";
inline const std::string kException = "exception";

// Структуры для логирования
struct RequestLogData {
  std::string ip;
  std::string uri;
  std::string method;
};

struct ResponseLogData {
  long response_time;
  int code;
  std::string content_type;
};

struct ServerStartLogData {
  uint32_t port;
  std::string address;
};

struct ServerExitLogData {
  int code;
  std::optional<std::string> exception;
};

struct ErrorLogData {
  int code;
  std::string text;
  std::string where;
};

// Функции для преобразования в JSON
void tag_invoke(json::value_from_tag, json::value& jv, const RequestLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ResponseLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ServerStartLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ServerExitLogData& data);
void tag_invoke(json::value_from_tag, json::value& jv, const ErrorLogData& data);

// Вспомогательная функция для создания лог-сообщения
template <typename T>
std::string CreateLogMessage(std::string_view message, T&& data) {
  json::object log_entry;
  log_entry[kTimestamp] = boost::posix_time::to_iso_extended_string(
      boost::posix_time::microsec_clock::local_time());
  log_entry[kMessage] = std::string(message);
  log_entry[kData] = json::value_from(std::forward<T>(data));
  return json::serialize(log_entry);
}

}  // namespace logware