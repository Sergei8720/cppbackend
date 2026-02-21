#include "logging_data_storage.h"

namespace logware {

void tag_invoke(json::value_from_tag, json::value& jv, const RequestLogData& data) {
  json::object obj;
  obj[kIp] = data.ip;
  obj[kUri] = data.uri;
  obj[kMethod] = data.method;
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ResponseLogData& data) {
  json::object obj;
  obj[kResponseTime] = data.response_time;
  obj[kCode] = data.code;
  if (data.content_type.empty()) {
    obj[kContentType] = nullptr;
  } else {
    obj[kContentType] = data.content_type;
  }
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ServerStartLogData& data) {
  json::object obj;
  obj[kPort] = data.port;
  obj[kAddress] = data.address;
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ServerExitLogData& data) {
  json::object obj;
  obj[kCode] = data.code;
  if (data.exception.has_value()) {
    obj[kException] = data.exception.value();
  }
  jv = std::move(obj);
}

void tag_invoke(json::value_from_tag, json::value& jv, const ErrorLogData& data) {
  json::object obj;
  obj[kCode] = data.code;
  obj[kText] = data.text;
  obj[kWhere] = data.where;
  jv = std::move(obj);
}

}  // namespace logware