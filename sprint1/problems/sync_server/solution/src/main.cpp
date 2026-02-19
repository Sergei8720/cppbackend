#ifdef _WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

using tcp = net::ip::tcp;
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

std::optional<StringRequest> ReadRequest(tcp::socket& socket, beast::flat_buffer& buffer) {
    beast::error_code ec;
    StringRequest req;
    
    http::read(socket, buffer, req, ec);

    if (ec == http::error::end_of_stream) {
        return std::nullopt;
    }
    
    if (ec) {
        throw std::runtime_error("Failed to read request: " + ec.message());
    }
    
    return req;
}

void DumpRequest(const StringRequest& req) {
    std::cout << req.method_string() << ' ' << req.target() << std::endl;
    
    for (const auto& header : req) {
        std::cout << "  " << header.name_string() << ": " << header.value() << std::endl;
    }
}

StringResponse MakeStringResponse(http::status status, 
                                  std::string_view body, 
                                  unsigned http_version,
                                  bool keep_alive) {
    StringResponse response(status, http_version);
    response.set(http::field::content_type, "text/html");
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(keep_alive);
    return response;
}

StringResponse HandleRequest(StringRequest&& req) {
    std::string body;
    
    if (req.method() == http::verb::get || req.method() == http::verb::head) {
        body = "Hello, ";
        
        if (req.target().size() > 1) {
            body.append(req.target().substr(1));
        } else {
            body.append("World");
        }
        
        StringResponse response = MakeStringResponse(http::status::ok, 
                                                    body, 
                                                    req.version(), 
                                                    req.keep_alive());
        
        if (req.method() == http::verb::head) {
            response.body().clear();
        }
        
        return response;
    }
    
    StringResponse response = MakeStringResponse(http::status::method_not_allowed,
                                                "Invalid method",
                                                req.version(),
                                                req.keep_alive());
    response.set(http::field::allow, "GET, HEAD");
    return response;
}

void HandleConnection(tcp::socket& socket) {
    try {
        beast::flat_buffer buffer;

        while (auto request = ReadRequest(socket, buffer)) {
            DumpRequest(*request);
            StringResponse response = HandleRequest(std::move(*request));
            http::write(socket, response);
            
            if (response.need_eof()) {
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main() {
    net::io_context ioc;
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr unsigned short port = 8080;

    tcp::acceptor acceptor(ioc, {address, port});
    std::cout << "Server has started..." << std::endl;
    
    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        std::cout << "Connection received" << std::endl;

        std::thread t([socket = std::move(socket)]() mutable {
            HandleConnection(socket);
        });
        
        t.detach();
    }
    
    return 0;
}
