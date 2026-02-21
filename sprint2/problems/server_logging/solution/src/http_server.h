#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <memory>
#include <string>

#include "logger.h"
#include "sdk.h"

namespace http_server {

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using tcp = net::ip::tcp;
using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what);

class SessionBase : public std::enable_shared_from_this<SessionBase> {
 public:
  SessionBase(const SessionBase&) = delete;
  SessionBase& operator=(const SessionBase&) = delete;

  void Run();

 protected:
  explicit SessionBase(tcp::socket&& socket) : stream_(std::move(socket)) {}

  using HttpRequest = http::request<http::string_body>;

  virtual ~SessionBase() = default;

  template <typename Body, typename Fields>
  void Write(http::response<Body, Fields>&& response) {
    auto safe_response =
        std::make_shared<http::response<Body, Fields>>(std::move(response));

    http::async_write(
        stream_, *safe_response,
        [self = shared_from_this(), safe_response](
            beast::error_code ec, std::size_t bytes_written) {
          self->OnWrite(true, ec, bytes_written);
          BOOST_LOG_TRIVIAL(info)
              << logware::CreateLogMessage(
                     "response sent"sv,
                     logware::ResponseLogData<Body, Fields>(
                         self->GetRemoteIp(),
                         self->GetDurationFromTimeReceivedRequest_ms(
                             boost::posix_time::microsec_clock::local_time()),
                         *safe_response));
        });
  }

  void SetReceivedRequestTime(
      const boost::posix_time::ptime& received_request_moment) {
    received_request_moment_ = received_request_moment;
  }

  long GetDurationFromTimeReceivedRequest_ms(
      const boost::posix_time::ptime& to_moment) const {
    boost::posix_time::time_duration duration =
        to_moment - received_request_moment_;
    return duration.total_milliseconds();
  }

  const std::string& GetRemoteIp() {
    static std::string remote_ip;
    try {
      auto temp = stream_.socket().remote_endpoint().address().to_string();
      remote_ip = temp;
    } catch (...) {
    }
    return remote_ip;
  }

 private:
  beast::tcp_stream stream_;
  beast::flat_buffer buffer_;
  HttpRequest request_;
  boost::posix_time::ptime received_request_moment_;

  void Read();
  void OnRead(beast::error_code ec, std::size_t bytes_read);
  void OnWrite(bool close, beast::error_code ec, std::size_t bytes_written);
  void Close();
  virtual void HandleRequest(HttpRequest&& request) = 0;
};

template <typename RequestHandler>
class Session : public SessionBase {
 public:
  template <typename Handler>
  Session(tcp::socket&& socket, Handler&& request_handler)
      : SessionBase(std::move(socket)),
        request_handler_(std::forward<Handler>(request_handler)) {}

 private:
  RequestHandler request_handler_;

  void HandleRequest(HttpRequest&& request) override {
    SetReceivedRequestTime(boost::posix_time::microsec_clock::local_time());
    BOOST_LOG_TRIVIAL(info)
        << logware::CreateLogMessage("request received"sv,
                                      logware::RequestLogData(GetRemoteIp(),
                                                              request));
    request_handler_(
        std::move(request),
        [self = this->shared_from_this()](auto&& response) {
          self->Write(std::move(response));
        });
  }
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
 public:
  template <typename Handler>
  Listener(net::io_context& ioc, const tcp::endpoint& endpoint,
           Handler&& request_handler)
      : ioc_(ioc),
        acceptor_(net::make_strand(ioc)),
        request_handler_(std::forward<Handler>(request_handler)) {
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(net::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen(net::socket_base::max_listen_connections);
  }

  void Run() { DoAccept(); }

 private:
  net::io_context& ioc_;
  tcp::acceptor acceptor_;
  RequestHandler request_handler_;

  void DoAccept() {
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(&Listener::OnAccept,
                                  this->shared_from_this()));
  }

  void OnAccept(sys::error_code ec, tcp::socket socket) {
    if (ec) {
      ReportError(ec, "accept"sv);
      return;
    }

    AsyncRunSession(std::move(socket));
    DoAccept();
  }

  void AsyncRunSession(tcp::socket&& socket) {
    std::make_shared<Session<RequestHandler>>(std::move(socket),
                                              request_handler_)
        ->Run();
  }
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint,
               RequestHandler&& handler) {
  using MyListener = Listener<std::decay_t<RequestHandler>>;
  std::make_shared<MyListener>(ioc, endpoint,
                               std::forward<RequestHandler>(handler))
      ->Run();
}

}  // namespace http_server