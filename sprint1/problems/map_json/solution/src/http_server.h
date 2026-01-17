#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase {
public:
    SessionBase(const SessionBase&) = delete;
    SessionBase& operator=(const SessionBase&) = delete;
    
    void Run();

protected:
    explicit SessionBase(tcp::socket&& socket);
    virtual ~SessionBase() = default;

    using HttpRequest = http::request<http::string_body>;

    template <typename Body, typename Fields>
    void Write(http::response<Body, Fields>&& response);

private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    HttpRequest request_;

    void Read();
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);
    void Close();

    virtual void HandleRequest(HttpRequest&& request) = 0;
    virtual std::shared_ptr<SessionBase> GetSharedThis() = 0;
};

template <typename RequestHandler>
class Session : public SessionBase, public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    template <typename Handler>
    Session(tcp::socket&& socket, Handler&& request_handler);

private:
    RequestHandler request_handler_;

    std::shared_ptr<SessionBase> GetSharedThis() override;
    void HandleRequest(HttpRequest&& request) override;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& request_handler);

    void Run();

private:
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;

    void DoAccept();
    void OnAccept(boost::system::error_code ec, tcp::socket socket);
    void AsyncRunSession(tcp::socket&& socket);
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler);

}  // namespace http_server