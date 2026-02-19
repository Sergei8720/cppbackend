#include "logger.h"

namespace logware {

void StringFormatter(logging::record_view const& view, logging::formatting_ostream& stream) {
  stream << view[expr::smessage];
}

void InitLogger() {
  boost::log::add_console_log(
    std::cout,
    boost::log::keywords::auto_flush = true,
    boost::log::keywords::format = &StringFormatter);
}

}  // namespace logware