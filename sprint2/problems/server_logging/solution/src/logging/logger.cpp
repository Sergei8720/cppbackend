#include "logger.h"

#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/support/date_time.hpp>
#include <iostream>

namespace logware {

void InitLogger() {
  logging::add_common_attributes();

  auto console_sink = logging::add_console_log(
      std::clog,
      keywords::format = (
          expr::stream << expr::smessage
      ),
      keywords::auto_flush = true
  );
}

}  // namespace logware