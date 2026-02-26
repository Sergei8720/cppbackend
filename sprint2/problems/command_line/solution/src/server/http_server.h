#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <filesystem>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

namespace {

template <typename Fn>
void RunWorkers(size_t n, const Fn& fn) {
    n = std::max(static_cast<size_t>(1), n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}

int main(int argc, const char* argv[]) {
    logware::InitLogger();
    
    try {
        prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
        
        model::Game game = json_loader::LoadGame(args.config_file);
        fs::path sc_root_path{args.www_root};
        
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        app::Application application(std::move(game), args.tick_period, args.randomize_spawn_points, ioc);
        
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server exited"sv,
                    logware::ExitCodeLogData{0});
                ioc.stop();
            }
        });
        
        http_handler::RequestHandler handler{application, sc_root_path};
        
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server started"sv,
            logware::ServerAddressLogData(address.to_string(), port));
        
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
            logware::ExceptionLogData(EXIT_FAILURE, "Server error", ex.what()));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}