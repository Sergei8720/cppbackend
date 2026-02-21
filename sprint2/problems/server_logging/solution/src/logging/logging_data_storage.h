#pragma once

#include <boost/json.hpp>
#include <optional>
#include <string>

namespace logware {

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
  int port = 0;
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

void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const RequestLogData& data);
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ResponseLogData& data);
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ServerStartLogData& data);
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ServerExitLogData& data);
void tag_invoke(boost::json::value_from_tag, boost::json::value& jv, const ErrorLogData& data);

}  // namespace logware