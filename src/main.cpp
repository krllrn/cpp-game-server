#ifdef WIN32
#include <sdkddkver.h>
#endif

// Этот заголовочный файл надо подключить в одном и только одном .cpp-файле программы
//#include <boost/json/src.hpp>

#include <boost/program_options.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;

using pqxx::operator"" _zv;

namespace {
// Запускает функцию fn на n потоках, включая текущий
template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    // Запускаем n-1 рабочих потоков, выполняющих функцию fn
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

struct Args {
    int tick = -1;
    std::string config;
    std::string static_files;
    bool random_spawn = false;
    std::string state_file;
    int save_state_period = -1;
}; 

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;
    po::options_description desc{"Allowed options"s};

    Args args;
    desc.add_options()
        // Добавляем опцию --help и её короткую версию -h
        ("help,h", "produce help message")
        ("tick-period,t", po::value(&args.tick)->value_name("milliseconds"s), "set tick period")
        ("config-file,c", po::value(&args.config)->value_name("file"s), "set config file path")
        ("www-root,w", po::value(&args.static_files)->value_name("dir"s), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions")
        ("state-file", po::value(&args.state_file)->value_name("dir"s), "set state file")
        ("save-state-period", po::value(&args.save_state_period)->value_name("milliseconds"s), "set period to autosave to state file");

    // variables_map хранит значения опций после разбора
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.contains("help"s)) {
        // Если был указан параметр --help, то выводим справку и возвращаем nullopt
        std::cout << desc;
        return std::nullopt;
    }

    // Проверяем наличие опций
    if (!vm.contains("config-file"s)) {
        throw std::runtime_error("Config files have not been specified"s);
    }
    if (!vm.contains("www-root"s)) {
        throw std::runtime_error("Static files have not been specified"s);
    }
    if (vm.contains("randomize-spawn-points")) {
        args.random_spawn = true;
    }
    if (vm.contains("save-state-period") && !vm.contains("state-file"s)) {
        args.save_state_period = -1;
    }

    // С опциями программы всё в порядке, возвращаем структуру args
    return args;
}

}  // namespace

int main(int argc, const char* argv[]) {
    const char* db_url = std::getenv("GAME_DB_URL");
    if (!db_url) {
        throw std::runtime_error("GAME_DB_URL env is not specified");
    }

    auto args = ParseCommandLine(argc, argv);
    
    // Загружаем карту из файла и построить модель игры
    model::Game game;
    json_loader::LoadGame(game, std::filesystem::path(args->config));

    // Инициализируем io_context
    const unsigned num_threads = std::thread::hardware_concurrency();
    net::io_context ioc(num_threads);

    // Создаём обработчик HTTP-запросов, связываем его с моделью игры
    // strand для выполнения запросов к API
    auto api_strand = net::make_strand(ioc);

    ConnectionPool conn_pool{num_threads, 
    [db_url] {
        auto conn = std::make_shared<pqxx::connection>(db_url);
        pqxx::work work{*conn};
        work.exec(R"(
        CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
    )"_zv);
        work.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id UUID  DEFAULT uuid_generate_v1() CONSTRAINT retired_players_id_constraint PRIMARY KEY,
            name varchar(100) NOT NULL,
            score INTEGER NOT NULL,
            play_time_ms INTEGER NOT NULL
        );
    )"_zv);
        work.exec(R"(
        CREATE INDEX IF NOT EXISTS score_play_time_name ON retired_players (score DESC, play_time_ms, name);
    )"_zv);
        work.commit();
        return conn;
    }};
    
    Application app{&game, &conn_pool};
    std::shared_ptr<http_handler::SerializationListener> srl_listener = nullptr;
    if (!args->state_file.empty()) {
        srl_listener = std::make_shared<http_handler::SerializationListener>();
        srl_listener->SetPathToSaveFile(args->state_file);
        if (std::filesystem::exists(args->state_file) && !std::filesystem::is_empty(args->state_file)) {
            srl_listener->LoadState(&game);
        }
        
        if (args->save_state_period != -1.0) {
            srl_listener->SetSavePeriod(args->save_state_period);
        } else {
            if (args->tick != -1) {
                srl_listener->SetSavePeriod(args->tick);
            }
        }
        app.SetApplicationListener(srl_listener->shared_from_this());
    } else {
        app.SetApplicationListener(nullptr);
    }
    auto handler = std::make_shared<http_handler::RequestHandler>(game, api_strand, app);
    auto logging_handler = std::make_shared<http_handler::LoggingRequestHandler<http_handler::RequestHandler>>(*handler);

    if (args->random_spawn) {
        handler->SetRandomize();
    }

    if (args->tick != -1) {
        std::chrono::duration<int, std::milli> chrono_milliseconds{ args->tick };
        auto ticker = std::make_shared<http_handler::Ticker>(api_strand, std::chrono::duration_cast<std::chrono::milliseconds>(chrono_milliseconds),
            [self = handler->shared_from_this()](std::chrono::milliseconds delta) { 
                self->Update(delta.count()); 
            }
        );
        ticker->Start();
    }

    try {
        // Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc, &logging_handler](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        std::string root_path = args->static_files;

        logging_handler->LogStartServer(address, port);
        http_server::ServeHttp(ioc, {address, port}, [self = logging_handler->shared_from_this(), &root_path](auto&& req, auto&& send, 
                                                                                                                boost::asio::ip::tcp::endpoint& endpoint) 
        {
            (*self)(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send), root_path, endpoint);
        });
        // Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
        
        app.SaveState();
    } catch (const std::exception& ex) {
        logging_handler->LogStopServer(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }
    logging_handler->LogStopServer(0);
}
