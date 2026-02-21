#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "logging_data_storage.h"

namespace logware {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

void InitLogger();

template <typename T>
std::string CreateLogMessage(std::string_view message, const T& data) {
  return logging_data_storage::CreateLogMessage(message, data);
}

}  // namespace logware