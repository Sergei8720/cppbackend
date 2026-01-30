#include <boost/asio.hpp>
#include <iostream>
#include <thread>
#include "request_handler.h"
#include "json_loader.h"
#include "http_server.h"

namespace net = boost::asio;
using namespace std::literals;

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    
    try {
        auto game = json_loader::LoadGame(argv[1]);
        http_handler::RequestHandler handler{game};
        
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr unsigned short port = 8080;
        
        http_server::ServeHttp(ioc, {address, port},
            [&handler](http::request<http::string_body>&& req) {
                return handler.HandleRequest(std::move(req));
            });
        
        std::vector<std::thread> threads;
        threads.reserve(num_threads - 1);
        for (auto i = 0u; i < num_threads - 1; ++i) {
            threads.emplace_back([&ioc] { ioc.run(); });
        }
        ioc.run();
        
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}