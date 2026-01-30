#pragma once

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <memory>

namespace http_server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

void ReportError(beast::error_code ec, std::string_view what);

using RequestHandler = std::function<http::message_generator(http::request<http::string_body>&&)>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket&& socket, const RequestHandler& handle_request);
    
    void Run();
    
private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    RequestHandler handle_request_;
    http::request<http::string_body> request_;
    http::message_generator response_;
    
    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);
    void HandleRequest();
    void Close();
};

class Listener : public std::enable_shared_from_this<Listener> {
public:
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, 
             const RequestHandler& handle_request);

    void Run();
    
private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler handle_request_;
    
    void DoAccept();
    void OnAccept(beast::error_code ec, tcp::socket socket);
};

void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, 
               const RequestHandler& handle_request);

}  // namespace http_server