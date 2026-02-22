#include "http_server.h"
#include "logger.h"

#include <iostream>

namespace http_server {

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view where) {
    // Логируем ошибку через Boost.Log
    boost::json::value error_data{
        {"code", ec.value()},
        {"text", ec.message()},
        {"where", std::string(where)}
    };
    BOOST_LOG_TRIVIAL(error) << boost::log::add_value(logging::additional_data, error_data)
                             << "error";
    
    // Оставляем вывод в cerr для обратной совместимости
    std::cerr << where << " : " << ec.what() << std::endl;
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() { 
    request_ = {};
    stream_.expires_after(30s);
    
    http::async_read(stream_, buffer_, request_,
                     beast::bind_front_handler(&SessionBase::OnRead,
                         GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, std::size_t bytes_read) {
    using namespace std::literals;
    
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    
    if (ec) {
        return ReportError(ec, "read");
    }
    
    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec,
                          std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, "write");
    }
    
    if (close) {
        return Close();
    }
    
    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace http_server