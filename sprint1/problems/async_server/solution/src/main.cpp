#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "http_server.h"

namespace {

namespace net = boost::asio;
using namespace std::literals;
namespace sys = boost::system;
namespace http = boost::beast::http;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

class ResponseBuilder {
public:
    static StringResponse CreateGetResponse(http::status status, std::string_view body, 
                                           unsigned http_version, bool keep_alive,
                                           std::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        SetCommonHeaders(response, content_type, keep_alive);
        response.body() = body;
        response.content_length(body.size());
        return response;
    }

    static StringResponse CreateHeadResponse(http::status status, size_t body_size, 
                                            unsigned http_version, bool keep_alive,
                                            std::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        SetCommonHeaders(response, content_type, keep_alive);
        response.content_length(body_size);
        return response;
    }

    static StringResponse CreateErrorResponse(http::status status, unsigned http_version, 
                                             bool keep_alive,
                                             std::string_view content_type = ContentType::TEXT_HTML) {
        StringResponse response(status, http_version);
        SetCommonHeaders(response, content_type, keep_alive);
        response.set(http::field::allow, "GET, HEAD");
        response.body() = "Invalid method"sv;
        response.content_length(response.body().size());
        return response;
    }

private:
    static void SetCommonHeaders(StringResponse& response, std::string_view content_type,
                                bool keep_alive) {
        response.set(http::field::content_type, content_type);
        response.keep_alive(keep_alive);
    }
};

std::string CreateGreetingMessage(std::string_view target) {
    std::stringstream text;
    text << "Hello, " << target.substr(1);
    return text.str();
}

StringResponse HandleRequest(StringRequest&& req) {
    const std::string greeting = CreateGreetingMessage(req.target());
    
    switch (req.method()) {
        case http::verb::get: {
            return ResponseBuilder::CreateGetResponse(
                http::status::ok, greeting, req.version(), req.keep_alive());
        }
        case http::verb::head: {
            return ResponseBuilder::CreateHeadResponse(
                http::status::ok, greeting.size(), req.version(), req.keep_alive());
        }
        default: {
            return ResponseBuilder::CreateErrorResponse(
                http::status::method_not_allowed, req.version(), req.keep_alive());
        }
    }
}

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    
    while (--n) {
        workers.emplace_back(fn);
    }
    
    fn();
}

}  // namespace

int main() {
    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    
    signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
        if (!ec) {
            std::cout << "Signal " << signal_number << " received" << std::endl;
            ioc.stop();
        }
    });

    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    
    http_server::ServeHttp(ioc, {address, port}, [](auto&& req, auto&& sender) {
        sender(HandleRequest(std::forward<decltype(req)>(req)));
    });

    std::cout << "Server has started..." << std::endl;

    RunWorkers(num_threads, [&ioc] {
        ioc.run();
    });
    
    std::cout << "Shutting down" << std::endl;
    return 0;
}
