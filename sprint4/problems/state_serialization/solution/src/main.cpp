#include "sdk.h"
//
#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"
#include "application.h"
#include "program_options.h"

#include <boost/asio/io_context.hpp>
#include <thread>
#include <boost/asio/signal_set.hpp>
#include <filesystem>
#include <chrono>
#include <memory>

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
    
    try {
        // 1. Парсинг аргументов командной строки
        prog_opt::Args args = prog_opt::ParseCommandLine(argc, argv);
        
        // 2. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(args.config_file);

        // 3. Устанавливаем путь до статического контента.
        fs::path sc_root_path{args.www_root};

        // 4. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 5. Создание application
        auto application = std::make_shared<app::Application>(std::move(game)
                                                            , args.tick_period
                                                            , args.randomize_spawn_points
                                                            , ioc);

        // 6. Загрузка состояния и настройка автосохранения
        if (!args.state_file.empty()) {
            bool loaded = application->LoadGameState(args.state_file, ioc);
            if (!loaded && fs::exists(args.state_file)) {
                // Файл существует, но не удалось загрузить - критическая ошибка
                BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                                            logware::ExceptionLogData(EXIT_FAILURE, 
                                                "Failed to load existing state file", 
                                                args.state_file));
                return EXIT_FAILURE;
            }
            
            // Настраиваем автоматическое сохранение
            application->SetupStateSaving(args.state_file, args.save_state_period);
        }

        // 7. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([application, &ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("Received termination signal, saving state..."sv);
                
                // Сохраняем состояние перед выходом
                if (application) {
                    application->SaveGameState();
                }
                
                BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("server exited"sv,
                                                                        logware::ExitCodeLogData(0));
                ioc.stop();
            }
        });

        // 8. Создаём обработчик HTTP-запросов и связываем его с моделью игры, задаем путь до статического контента.
        http_handler::RequestHandler handler{application, sc_root_path};

        // 9. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });
        
        // Эта надпись сообщает тестам о том, что сервер запущен и готов обрабатывать запросы
        BOOST_LOG_TRIVIAL(info) << logware::CreateLogMessage("Server has started..."sv,
                                                                logware::ServerAddressLogData(address.to_string(), port));

        // 10. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        // 11. После остановки io_context сохраняем состояние ещё раз на всякий случай
        if (!args.state_file.empty() && application) {
            application->SaveGameState();
        }
        
    } catch (const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << logware::CreateLogMessage("error"sv,
                                        logware::ExceptionLogData(EXIT_FAILURE, "Server down"sv, ex.what()));
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}