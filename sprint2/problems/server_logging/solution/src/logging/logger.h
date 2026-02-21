#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/json.hpp>
#include <string>

namespace logware {

void InitLogger();

template <typename T>
std::string CreateLogMessage(const std::string& message, const T& data) {
  boost::json::object log_entry;
  log_entry["timestamp"] = boost::posix_time::to_iso_extended_string(
      boost::posix_time::microsec_clock::local_time());
  log_entry["message"] = message;
  log_entry["data"] = boost::json::value_from(data);
  return boost::json::serialize(log_entry);
}

}  // namespace logware