#include "sdk.h"

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
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    // 0. Инициализация логгера.
    logware::InitLogger();
    prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
    
    std::atomic<bool> final_save_done{false};
    
    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args.config_file);

        // 2. Устанавливаем путь до статического контента.
        fs::path sc_root_path{args.www_root};

        // 3. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 4. Создание application
        auto application = std::make_shared<app::Application>(std::move(game),
                                                              args.tick_period,
                                                              args.randomize_spawn_points,
                                                              ioc);

        // 5. Инициализация настроек сохранения
        std::shared_ptr<app::StateSerializer> state_serializer;  // ← ИЗМЕНЕНО: unique_ptr -> shared_ptr
        if (!args.state_file.empty()) {
            auto save_period = args.save_state_period > 0 
                ? std::chrono::milliseconds(args.save_state_period)
                : std::chrono::milliseconds(0);
            
            state_serializer = std::make_shared<app::StateSerializer>(  // ← ИЗМЕНЕНО: make_unique -> make_shared
                *application, 
                args.state_file, 
                save_period
            );
            
            // Устанавливаем StateSerializer в Application
            application->SetStateSerializer(state_serializer);  // ← ИЗМЕНЕНО: убрали .get()
            BOOST_LOG_TRIVIAL(info) << "StateSerializer set in Application";
            
            // Восстанавливаем состояние
            if (!state_serializer->LoadState(ioc)) {
                // Если файл существует, но произошла ошибка - завершаем работу
                if (std::filesystem::exists(args.state_file)) {
                    BOOST_LOG_TRIVIAL(error) << "Failed to load game state from " << args.state_file;
                    return EXIT_FAILURE;
                }
                // Если файла нет - начинаем с чистого листа
                BOOST_LOG_TRIVIAL(info) << "Starting with clean state (no state file or empty)";
            }
            
            // Запускаем периодическое сохранение
            state_serializer->StartPeriodicSaving(ioc);
        }

        // 6. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
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

        // 7. Создаём обработчик HTTP-запросов и связываем его с моделью игры, задаем путь до статического контента.
        http_handler::RequestHandler handler{application, sc_root_path};

        // 8. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        BOOST_LOG_TRIVIAL(info) << "Server has started...";

        // 9. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        // 10. Сохраняем состояние при нормальном завершении (если не было сохранено по сигналу)
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