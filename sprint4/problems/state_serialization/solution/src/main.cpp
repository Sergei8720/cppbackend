#include "sdk.h"
//
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"
#include "saving_settings.h"
#include "state_serializer.h"

#include <boost/asio/io_context.hpp>
#include <thread>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;

namespace {

// Запускает функцию fn на n потоках, включая текущий
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

}  // namespace

int main(int argc, const char* argv[]) {
    logware::InitLogger();
    
    BOOST_LOG_TRIVIAL(info) << "========================================";
    BOOST_LOG_TRIVIAL(info) << "Game server starting...";
    BOOST_LOG_TRIVIAL(info) << "========================================";
    
    prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
    
    BOOST_LOG_TRIVIAL(info) << "Command line arguments:";
    BOOST_LOG_TRIVIAL(info) << "  --config-file: " << args.config_file;
    BOOST_LOG_TRIVIAL(info) << "  --www-root: " << args.www_root;
    BOOST_LOG_TRIVIAL(info) << "  --tick-period: " << args.tick_period;
    BOOST_LOG_TRIVIAL(info) << "  --randomize-spawn-points: " << args.randomize_spawn_points;
    BOOST_LOG_TRIVIAL(info) << "  --state-file: " << (args.state_file.empty() ? "none" : args.state_file);
    BOOST_LOG_TRIVIAL(info) << "  --save-state-period: " << args.save_state_period;
    
    std::atomic<bool> final_save_done{false};
    
    try {
        BOOST_LOG_TRIVIAL(info) << "Loading game config from " << args.config_file;
        model::Game game = json_loader::LoadGame(args.config_file);
        BOOST_LOG_TRIVIAL(info) << "Game config loaded successfully";

        fs::path sc_root_path{args.www_root};

        const unsigned num_threads = std::thread::hardware_concurrency();
        BOOST_LOG_TRIVIAL(info) << "Using " << num_threads << " threads";
        net::io_context ioc(num_threads);

        auto application = std::make_shared<app::Application>(std::move(game),
                                                              args.tick_period,
                                                              args.randomize_spawn_points,
                                                              ioc);
        BOOST_LOG_TRIVIAL(info) << "Application created";

        // Настройка сохранения состояния
        std::unique_ptr<app::StateSerializer> state_serializer;
        if (!args.state_file.empty()) {
            auto save_period = args.save_state_period > 0 
                ? std::chrono::milliseconds(args.save_state_period)
                : std::chrono::milliseconds(0);
            
            BOOST_LOG_TRIVIAL(info) << "State serialization enabled: state_file=" << args.state_file
                                   << ", save_period=" << save_period.count() << "ms";
            
            state_serializer = std::make_unique<app::StateSerializer>(
                *application, 
                args.state_file, 
                save_period
            );
            
            // Восстанавливаем состояние
            bool load_success = state_serializer->LoadState(ioc);
            if (!load_success) {
                if (std::filesystem::exists(args.state_file)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to load game state from " << args.state_file;
                    return EXIT_FAILURE;
                }
                BOOST_LOG_TRIVIAL(info) << "Starting with clean state (no state file or empty)";
            }
            
            // Запускаем периодическое сохранение
            if (save_period.count() > 0) {
                state_serializer->StartPeriodicSaving(ioc);
                BOOST_LOG_TRIVIAL(info) << "Periodic saving started with period " << save_period.count() << "ms";
            } else {
                BOOST_LOG_TRIVIAL(info) << "Periodic saving disabled (period=0)";
            }
        } else {
            BOOST_LOG_TRIVIAL(info) << "State serialization disabled (no state-file specified)";
        }

        // Обработчик сигналов
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &state_serializer, &final_save_done](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec && !final_save_done.exchange(true)) {
                BOOST_LOG_TRIVIAL(info) << "Received signal " << signal_number << ", saving state...";
                if (state_serializer) {
                    try {
                        state_serializer->FinalSave();
                        BOOST_LOG_TRIVIAL(info) << "State saved successfully on signal";
                    } catch (const std::exception& e) {
                        BOOST_LOG_TRIVIAL(error) << "Failed to save state on signal: " << e.what();
                    }
                }
                ioc.stop();
            }
        });
        BOOST_LOG_TRIVIAL(info) << "Signal handlers registered";

        // HTTP обработчик
        http_handler::RequestHandler handler{application, sc_root_path};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        BOOST_LOG_TRIVIAL(info) << "Starting HTTP server on " << address << ":" << port;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        BOOST_LOG_TRIVIAL(info) << "Server has started...";
        BOOST_LOG_TRIVIAL(info) << "========================================";

        // Запуск обработки
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        // Финальное сохранение
        if (state_serializer && !final_save_done.exchange(true)) {
            BOOST_LOG_TRIVIAL(info) << "Normal shutdown, saving final state...";
            state_serializer->FinalSave();
        }
        
        BOOST_LOG_TRIVIAL(info) << "========================================";
        BOOST_LOG_TRIVIAL(info) << "Server stopped";
        BOOST_LOG_TRIVIAL(info) << "========================================";
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Server error: " << ex.what();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}