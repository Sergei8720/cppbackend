#include "logger.h"

#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

namespace logware {

namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

void InitLogger() {
  boost::log::add_common_attributes();
  
  boost::log::add_console_log(
      std::cout,
      keywords::format = expr::stream << expr::smessage,
      keywords::auto_flush = true
  );
}

}  // namespace logware