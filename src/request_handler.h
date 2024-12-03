#pragma once

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/beast.hpp>
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include "http_server.h"
#include "application.h"

#include <chrono>
#include <fstream>
#include <variant>
#include <vector>
#include <optional>
#include <unordered_map>

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", boost::json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(custom_message, "CustomMessage", std::string)

namespace http_handler {

using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;
namespace keywords = boost::log::keywords;
namespace logging = boost::log;
namespace json = boost::json;
namespace net = boost::asio;
namespace sys = boost::system;
using tcp = net::ip::tcp;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;

using Response = std::variant<StringResponse, FileResponse>;
using Strand = net::strand<net::io_context::executor_type>;

template<class BaseRequestHandler>
class LoggingRequestHandler : public std::enable_shared_from_this<LoggingRequestHandler<BaseRequestHandler>> {
public:
    explicit LoggingRequestHandler(BaseRequestHandler& decorated)
    : decorated_{decorated}
    {
        InitBoostLogFilter();
    }

    void LogStartServer(const boost::asio::ip::address& address, const net::ip::port_type& port) {
        json::value entry{
            {"port"s, port},
            {"address"s, address.to_string()}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "server started"); 
    }

    void LogStopServer(int code) {
        json::value entry{
            {"code"s, code}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "server exited"); 
    }

    void LogStopServer(int code, const char* exception) {
        json::value entry{
            {"code"s, code},
            {"exception"s, *exception}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "server exited"); 
    }

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send, std::string& root_path, boost::asio::ip::tcp::endpoint& endpoint) {
        auto client_ip = endpoint.address().to_string();

        auto reqst = std::forward<decltype(req)>(req);
        LogRequest(reqst, client_ip);

        std::chrono::system_clock::time_point start_ts = std::chrono::system_clock::now();
        auto resp = decorated_(reqst, root_path);
        std::chrono::system_clock::time_point end_ts = std::chrono::system_clock::now();
        auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);

        if (std::holds_alternative<StringResponse>(resp)) {
            LogResponse(std::get<http::response<http::string_body>>(resp), ms_int.count(), client_ip);
            send(std::get<http::response<http::string_body>>(resp));
        } else if (std::holds_alternative<FileResponse>(resp)) {
            LogResponse(std::get<http::response<http::file_body>>(resp), ms_int.count(), client_ip);
            send(std::get<http::response<http::file_body>>(resp));
        }
    }

private:
    BaseRequestHandler& decorated_;

    static void LogFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
        // Момент времени приходится вручную конвертировать в строку.
        // Для получения истинного значения атрибута нужно добавить
        // разыменование. 
        auto ts = *rec[timestamp];
        json::value data{
            {"timestamp", to_iso_extended_string(ts)},
            {"data", rec[additional_data] ? *rec[additional_data] : json::value{}},
            {"message", rec[custom_message] ? *rec[custom_message] : std::string{}}
        };

        strm << json::serialize(data);
    }

    void InitBoostLogFilter() {
        logging::add_common_attributes();

        logging::add_console_log(
            std::clog,
            keywords::format = &LogFormatter, 
            keywords::auto_flush = true
        );
    }

    void LogRequest(const StringRequest& r, std::string& ip) {
        json::value entry{
            {"ip"s, ip},
            {"URI"s, r.target()},
            {"method"s, r.method_string()}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "request received"); 
    }

    template<typename T>
    void LogResponse(http::response<T>& r, int resp_time, std::string& ip) {
        json::value entry{
            {"ip"s, ip},
            {"response_time"s, resp_time},
            {"code"s, r.result_int()},
            {"content_type"s, r[http::field::content_type]}
        };

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, entry) << logging::add_value(custom_message, "response sent"); 
    }
};

//! ------------------------- Serialization Listener --------------------------------

struct SerializationObj {
    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & lost_objects;
        ar & tokens_dog;
    }
    
    std::unordered_map<std::string, std::unordered_map<int, LostObjectRepr>> lost_objects;
    std::unordered_map<std::string, std::unordered_map<std::string, DogRepr>> tokens_dog;
};

class SerializationListener : public ApplicationListener, public std::enable_shared_from_this<SerializationListener> {
public:
    SerializationListener()
    {}

    void SetSavePeriod(double interval) {
        save_interval_ = interval;
    }

    void SetPathToSaveFile(std::string path) {
        path_ = path;
    }

    void OnTick(double delta, model::Game* game) override {
        if (game->GetGameTime() - last_save_time_ >= save_interval_) {
            SaveState(game);
            last_save_time_ = game->GetGameTime();
        }
    }

    void SaveState(model::Game* game) {
        
        std::ofstream ofs(path_);
        if (ofs.is_open()) {
            try {
                SerializationObj s_object;

                auto maps = game->GetMaps();

                for (const auto& map : maps) {
                    auto map_id = *map.GetId();

                    for (const auto& [id, lost_obj] : map.GetLostObjects()) {
                        LostObjectRepr lor;
                        lor.pos = lost_obj.pos;
                        lor.type = lost_obj.loot->GetLootType();
                        s_object.lost_objects[map_id][id] = lor;
                    }
                }

                const auto players_with_tokens = const_cast<Players*>(game->GetPlayers())->GetPlayersWithTokens()->TokenAndPlayer();

                for (const auto& [token, player_ptr] : players_with_tokens) {
                    if (player_ptr) {
                        auto session = player_ptr->GetSession();
                        auto map = session->GetMap();
                        auto map_id = *(map->GetId());
                        auto dog = DogRepr{*(player_ptr->GetDog())};

                        s_object.tokens_dog[map_id][*(token)] = dog;
                    }
                }

                boost::archive::text_oarchive oa{ofs};
                oa << s_object;
                
            } catch(boost::archive::archive_exception& e) {
                std::cerr << "Exc: " << e.what() << '\n';
            }
            ofs.close();
        } else {
            std::cerr << "Can't open file." << std::endl;
        }
    }

    void LoadState(model::Game* game) {
        std::ifstream ifs(path_);
        if (ifs.is_open()) {
            SerializationObj s_object;

            boost::archive::text_iarchive ia{ifs};
            ia >> s_object;

            for (const auto& [map_id, tokens_dogs] : s_object.tokens_dog) {
                auto session = game->GetSession(map_id);
                auto map = const_cast<Map*>(session->GetMap());

                for (const auto& [token, dog_repr] : tokens_dogs) {
                    session->AddDog(dog_repr.Restore());
                    Player player{session, &session->GetDogsList().back()};
                    auto players = const_cast<Players*>(game->GetPlayers());
                    players->AddPlayer(player);
                    players->GetPlayersWithTokens()->AddPlayerWithToken(token, players->GetAllPlayers().back());
                }
            }

            for (const auto& [map_id, lost_object_reprs] : s_object.lost_objects) {
                auto session = game->GetSession(map_id);
                auto map = const_cast<Map*>(session->GetMap());
                for (const auto& [id, lost_object_repr] : lost_object_reprs) {
                    LostObject lo;
                    lo.pos = lost_object_repr.pos;
                    lo.loot = map->GetLootByType(lost_object_repr.type);
                    map->SetLostObject(id, lo);
                }    
            }
            ifs.close();
            
        } else {
            std::cerr << "Can't open file." << std::endl;
        }
    }
private:
    double last_save_time_ = .0;
    double save_interval_ = .0;
    std::filesystem::path path_;
};

//! -------------------------API handler --------------------------------

class ApiHandler {
public:
    explicit ApiHandler(Application* app, Strand& api_strand)
        : app_(app),
        api_strand_(api_strand)
    {
    }

    ApiHandler(const ApiHandler&) = delete;
    ApiHandler& operator=(const ApiHandler&) = delete;

    Response operator()(http::status status, std::vector<std::string>& target_uri, StringRequest& req) {
        return ProcessApiRequest(status, target_uri, req);      
    }
private:
    Application* app_;
    Strand& api_strand_;
    
    Response ProcessApiRequest(http::status status, std::vector<std::string>& target_uri, StringRequest& req,
                                                std::string_view content_type = "application/json");

    std::optional<std::vector<std::string>> GetPlayerTokenFromRequest(StringRequest& req);

    std::string MakeMapBody(std::vector<std::string>& target_uri);
    StringResponse GetMapResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    std::string MakeJoinBody(std::string&, std::string& map_id);
    StringResponse GetPlayerJoinResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    std::string MakePlayersBody(Player* player);
    StringResponse GetPlayersResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    std::string MakeStateBody(Player* player) const;
    StringResponse GetStateResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    StringResponse MakeUnauthorizedError(int version, bool keep_alive);

    StringResponse GetPlayerActionResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    StringResponse GetTickResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req);

    std::string MakeRecordsBody(int start_elem, int max_elem_count) const;
    StringResponse GetRecordsResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req, std::string params);

    template <typename Fn>
    StringResponse ExecuteAuthorized(Fn&& action, StringRequest& req) {
        if (auto token = GetPlayerTokenFromRequest(req)) {
            return action(Token{token.value()[1]});
        }
        return MakeUnauthorizedError(req.version(), req.keep_alive());
    }
};

//! -------------------------FILE handler --------------------------------

class FileHandler {
public:

    FileHandler& operator=(const FileHandler&) = delete;

    Response operator()(http::status status, std::string& root_path, std::string& target, StringRequest& req) {
        return ProcessNoneApiRequest(status, root_path, target, req);      
    }

private:

    const std::unordered_map<std::string_view, std::string> content_types_ = {
        {".htm", "text/html"},
        {".html", "text/html"},
        {".css", "text/css"},
        {".txt", "text/plain"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpe", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".tiff", "image/tiff"},
        {".tif", "image/tiff"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
        {"unknown", "application/octet-stream"}
    };

    std::string_view GetContentType(std::string_view file_ext);

    std::string UrlDecode(const std::string& target);

    bool IsSubPath(std::filesystem::path path, std::filesystem::path base);

    http::file_body::value_type MakeFileBody(std::filesystem::path& full_path);

    Response ProcessNoneApiRequest(http::status status, std::string& root_path, std::string& target, StringRequest& req);

};

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:

    explicit RequestHandler(model::Game& game, Strand api_strand, Application& app)
        : game_{game},
        app_(app),
        api_strand_{api_strand},
        api_handler_(ApiHandler{&app_, api_strand_})
    {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator>
    Response operator()(http::request<Body, http::basic_fields<Allocator>>& req, std::string& root_path) {
        // Обработать запрос request и отправить ответ, используя send
        auto resp = HandleRequest(req, root_path);
        return resp;      
    }

    void Update(int tick) {
        app_.SetTickAvailable();
        app_.Tick(tick);
    }

    void SetRandomize() {
        app_.SetRandomSpawnAvailable();
    }

private:
    model::Game& game_;
    FileHandler file_handler_ = FileHandler{};
    ApiHandler api_handler_;
    Strand api_strand_;
    Application app_;

    bool CheckRequestValid(Response& response, std::vector<std::string>& target_uri, StringRequest& req);

    std::vector<std::string> GetURIPath(std::string& target);

    Response HandleRequest(StringRequest& req, std::string& root_path) {
        using namespace std::literals;

        Response response;

        std::string target = std::string(req.target().data(), req.target().size());
        std::vector<std::string> target_uri = GetURIPath(target);

        auto check_result = CheckRequestValid(response, target_uri, req);

        if (check_result) {
            if (target_uri.size() < 2 || target_uri[0] != "api") {
                response = file_handler_(http::status::ok, root_path, target, req);
            } else {
                auto handle = [self = shared_from_this(), &response, &target_uri, &req] {
                    try {
                        response = self->api_handler_(http::status::ok, target_uri, req);
                    } catch (const std::exception& ex) {
                        std::cout << "Something went wrong: " << ex.what() << '\n';
                    }
                };
                net::dispatch(api_strand_, handle);
            }
        }

        return response;
    }
};

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = net::strand<net::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    // Функция handler будет вызываться внутри strand с интервалом period
    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} {
    }

    void Start() {
        net::dispatch(strand_, [self = shared_from_this()] {
            self->last_tick_ = Clock::now();
            self->ScheduleTick();
        });
    }

private:
    void ScheduleTick() {
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](sys::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(sys::error_code ec) {
        using namespace std::chrono;

        if (!ec) {
            auto this_tick = Clock::now();
            auto delta = duration_cast<milliseconds>(this_tick - last_tick_);
            last_tick_ = this_tick;
            try {
                handler_(delta);
            } catch (...) {
            }
            ScheduleTick();
        }
    }

    using Clock = std::chrono::steady_clock;

    Strand strand_;
    std::chrono::milliseconds period_;
    net::steady_timer timer_{strand_};
    Handler handler_;
    std::chrono::steady_clock::time_point last_tick_;
}; 


}  // namespace http_handler
