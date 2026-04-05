#include "sdk.h"
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"
#include "saving_settings.h"
#include "state_serializer.h"
#include "database/connection_pool.h"
#include "database/database.h"
#include <boost/asio/io_context.hpp>
#include <thread>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>
#include <cstdlib>
#include <cstring>

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
    prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
    
    std::atomic<bool> final_save_done{false};
    
    try {
        model::Game game = json_loader::LoadGame(args.config_file);
        fs::path sc_root_path{args.www_root};
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        auto application = std::make_shared<app::Application>(std::move(game),
                                                              args.tick_period,
                                                              args.randomize_spawn_points,
                                                              ioc);

        const char* db_url = std::getenv("GAME_DB_URL");
        std::shared_ptr<database::ConnectionPool> db_pool;
        
        BOOST_LOG_TRIVIAL(info) << "Checking GAME_DB_URL environment variable...";
        
        if (db_url && std::strlen(db_url) > 0) {
            BOOST_LOG_TRIVIAL(info) << "GAME_DB_URL found: " << db_url;
            try {
                BOOST_LOG_TRIVIAL(info) << "Creating connection pool with size 2...";
                db_pool = std::make_shared<database::ConnectionPool>(
                    2,
                    [db_url]() -> std::shared_ptr<pqxx::connection> {
                        BOOST_LOG_TRIVIAL(debug) << "Creating new database connection to: " << db_url;
                        auto conn = std::make_shared<pqxx::connection>(db_url);
                        BOOST_LOG_TRIVIAL(debug) << "Connection created, is_open=" << conn->is_open();
                        return conn;
                    }
                );
                
                BOOST_LOG_TRIVIAL(info) << "Initializing database (creating tables)...";
                database::Database::Init(db_pool);
                
                application->SetConnectionPool(db_pool);
                BOOST_LOG_TRIVIAL(info) << "Database connected successfully to: " << db_url;
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(warning) << "Failed to connect to database: " << e.what();
                BOOST_LOG_TRIVIAL(warning) << "Running without database persistence";
            }
        } else {
            BOOST_LOG_TRIVIAL(info) << "GAME_DB_URL environment variable not set, running without database";
            BOOST_LOG_TRIVIAL(info) << "To enable database, set: export GAME_DB_URL=postgresql://user:pass@host:port/db";
        }

        std::shared_ptr<app::StateSerializer> state_serializer;
        if (!args.state_file.empty()) {
            auto save_period = args.save_state_period > 0 
                ? std::chrono::milliseconds(args.save_state_period)
                : std::chrono::milliseconds(0);
            
            state_serializer = std::make_shared<app::StateSerializer>(
                *application, 
                args.state_file, 
                save_period
            );
            
            application->SetStateSerializer(state_serializer);
            BOOST_LOG_TRIVIAL(info) << "StateSerializer set in Application";
            
            if (!state_serializer->LoadState(ioc)) {
                if (std::filesystem::exists(args.state_file)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to load game state from " << args.state_file;
                    return EXIT_FAILURE;
                }
                BOOST_LOG_TRIVIAL(info) << "Starting with clean state (no state file or empty)";
            }
            
            state_serializer->StartPeriodicSaving(ioc);
        }

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &state_serializer, &final_save_done](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec && !final_save_done.exchange(true)) {
                BOOST_LOG_TRIVIAL(info) << "Received signal " << signal_number << ", saving state...";
                if (state_serializer) {
                    try {
                        state_serializer->FinalSave();
                        BOOST_LOG_TRIVIAL(info) << "State saved successfully";
                    } catch (const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to save state: " << e.what();
                    }
                }
                ioc.stop();
            }
        });

        http_handler::RequestHandler handler{application, sc_root_path};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        BOOST_LOG_TRIVIAL(info) << "Server has started...";

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        if (state_serializer && !final_save_done.exchange(true)) {
            BOOST_LOG_TRIVIAL(info) << "Normal shutdown, saving final state...";
            state_serializer->FinalSave();
        }
        
        BOOST_LOG_TRIVIAL(info) << "Server stopped";
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Server error: " << ex.what();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}