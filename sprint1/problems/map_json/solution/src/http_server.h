#pragma once

#include <memory>
#include <functional>

#include "sdk.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

void ReportError(beast::error_code ec, std::string_view what);

using RequestHandler = std::function<void(http::request<http::string_body>&&,
                                          std::function<void(http::response<http::string_body>&&)>)>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket&& socket, RequestHandler request_handler);
    
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    
    void Run();

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> request_;
    RequestHandler request_handler_;

    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);
    void Close();
};

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler request_handler);
    
    void Run();

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;

    void DoAccept();
    void OnAccept(sys::error_code ec, tcp::socket socket);
};

void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler);

}  // namespace http_server 