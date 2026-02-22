#ifndef LOGGER_H_
#define LOGGER_H_

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/attribute_value.hpp>
#include <boost/log/attributes/attribute_value_set.hpp>
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/json.hpp>

#include <iostream>

namespace logging {

// Ключ для атрибута, который будет содержать дополнительные JSON-данные
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)

// Форматтер для логов в JSON
inline void JsonFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
    namespace logging = boost::log;
    namespace json = boost::json;

    // Основной объект JSON
    json::object log_record;

    // Добавляем временную метку
    auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (ts) {
        std::string timestamp = to_iso_extended_string(ts.get());
        // Добавляем микросекунды, если их нет
        if (timestamp.find('.') == std::string::npos) {
            timestamp += ".000000";
        }
        log_record["timestamp"] = timestamp;
    } else {
        log_record["timestamp"] = nullptr;
    }

    // Добавляем сообщение
    auto severity = logging::extract<logging::trivial::severity_level>("Severity", rec);
    if (severity) {
        log_record["message"] = logging::trivial::to_string(severity.get());
    } else {
        log_record["message"] = nullptr;
    }

    // Добавляем дополнительные данные
    auto data = logging::extract<json::value>(additional_data.get_name(), rec);
    if (data && !data.get().is_null()) {
        log_record["data"] = data.get();
    } else {
        log_record["data"] = json::object();
    }

    // Сериализуем JSON в поток
    strm << json::serialize(log_record);
}

// Инициализация логгера
inline void Init() {
    namespace logging = boost::log;

    logging::add_common_attributes();

    // Настройка логирования в консоль (stdout)
    auto console_sink = logging::add_console_log(
        std::cout,
        logging::keywords::format = &JsonFormatter
    );

    // Устанавливаем фильтр, чтобы пропускать только сообщения с severity >= info
    console_sink->set_filter(
        logging::trivial::severity >= logging::trivial::info
    );

    console_sink->locked_backend()->auto_flush(true);
}

}  // namespace logging

#endif  // LOGGER_H_