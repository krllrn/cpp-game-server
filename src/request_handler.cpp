
#include "request_handler.h"

#include <algorithm>
#include <filesystem>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;


namespace {
    std::string ConstructError(std::string code, std::string message)
    {
        boost::json::object error;
        error["code"] = code;
        error["message"] = message;
        return boost::json::serialize(error);
    }

    std::vector<std::string> Split(const std::string &s, char delim) {
        std::vector<std::string> result;
        std::stringstream ss(s);
        std::string item;

        while (getline(ss, item, delim)) {
            result.push_back(item);
        }

        return result;
    }

    StringResponse ProcessApiError(http::status status, u_int version, std::string_view body, 
                                bool keep_alive, std::string_view allow = "GET, HEAD"s, 
                                                        std::string_view content_type = "application/json") {
        StringResponse response(status, version);

        if (status == http::status::method_not_allowed) {
            response.set(http::field::allow, allow);
        }

        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.set(http::field::cache_control, "no-cache");
        response.keep_alive(keep_alive);

        return response;
    }

    StringResponse ProcessFileError(http::status status, std::string_view body, StringRequest &req, 
                                                        std::string_view content_type = "application/json")
    {
        StringResponse response(status, req.version());
        if (status == http::status::method_not_allowed && body == "Invalid method") {
            response.set(http::field::allow, "GET, HEAD"s);
        }
        response.set(http::field::content_type, content_type);
        response.body() = body;
        response.content_length(body.size());
        response.keep_alive(req.keep_alive());

        return response;
    }

    std::string FormatDouble(double value) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << value;
        return ss.str();
    }

    StringResponse MakeBadRequestError(int version, bool keep_alive) {
        return ProcessApiError(http::status::bad_request, version, ConstructError("badRequest", "Bad request"), keep_alive);
    }

    StringResponse MakeInvalidMethodError(int version, bool keep_alive, std::string_view allow = "GET, HEAD"s) {
        std::string message = "Only "s.assign(allow) + " method is expected"s;
        return ProcessApiError(http::status::method_not_allowed, version, ConstructError("invalidMethod", std::move(message)), keep_alive, allow);
    }

    StringResponse MakeUnknownTokenError(int version, bool keep_alive) {
        return ProcessApiError(http::status::unauthorized, version, 
                                    ConstructError("unknownToken", "Player token has not been found"), keep_alive);;
    }

    StringResponse MakeInvalidArgumentError(int version, bool keep_alive, std::string message) {
        return ProcessApiError(http::status::bad_request, version, ConstructError("invalidArgument", std::move(message)), keep_alive);
    }

    bool IsDirectionValid(const std::string_view dir) {
        return dir.length() == 1 && (dir[0] == Direction::NORTH || 
                                    dir[0] == Direction::SOUTH || 
                                    dir[0] == Direction::WEST || 
                                    dir[0] == Direction::EAST);
    }

} //namespace

//! -------------------------Request handler --------------------------------

std::vector<std::string> RequestHandler::GetURIPath(std::string& target) {
    std::vector<std::string> target_uri;
    boost::split(target_uri, target, boost::is_any_of("/"));
    auto isEmptyOrBlank = [](const std::string &s) {
        return s.find_first_not_of(" \t") == std::string::npos;
    };
    if (target_uri.size() > 0) {
        target_uri.erase(std::remove_if(target_uri.begin(), target_uri.end(), isEmptyOrBlank), target_uri.end());
    }

    return target_uri;
}

bool IsGetOrHeadRequest(const boost::beast::http::verb& method) {
    return method == http::verb::get || method == http::verb::head;
}

bool RequestHandler::CheckRequestValid(Response& response, std::vector<std::string>& target_uri, StringRequest& req) {
    bool result = true;
    boost::beast::http::verb method = req.method();
    if (target_uri[0] == "api") {
        if (target_uri[1] != "v1" && IsGetOrHeadRequest(method)) {
            response = MakeBadRequestError(req.version(), req.keep_alive());
            result = false;
        }
        if (target_uri[1] == "v1") {
            if (target_uri[2] == "maps") {
                if (!IsGetOrHeadRequest(method)) {
                    response = MakeInvalidMethodError(req.version(), req.keep_alive());
                    result = false;
                }
                if (target_uri.size() == 4 && 
                        app_.FindMapById(Map::Id(static_cast<std::string>(target_uri[3]))) == nullptr) {
                    response = ProcessApiError(http::status::not_found, req.version(), ConstructError("mapNotFound", "Map not found"), req.keep_alive());
                    result = false;
                }
            } 
            if (target_uri[2] == "game" && target_uri[3] == "join" && req.method() != http::verb::post) {
                response = MakeInvalidMethodError(req.version(), req.keep_alive(), "POST"s);
                result = false;
            }
        }

    } 

    return result;
}

//! -------------------------API handler --------------------------------

StringResponse ApiHandler::MakeUnauthorizedError(int version, bool keep_alive) {
    return ProcessApiError(http::status::unauthorized, version, ConstructError("invalidToken", "Authorization header is missing"), keep_alive);
}

std::optional<std::vector<std::string>> ApiHandler::GetPlayerTokenFromRequest(StringRequest& req) {
    auto it_field = req.find("Authorization");
    if (it_field == req.end()) {
        return std::nullopt; 
    }
    auto auth_header = static_cast<std::string>(it_field->value());
    auto header_values = Split(auth_header, ' ');
    if (header_values.size() != 2 || header_values[0] != "Bearer" || header_values[1].size() != 32) {
        return std::nullopt;
    }

    return header_values;
}

std::string ApiHandler::MakeMapBody(std::vector<std::string>& target_uri) {
    std::string body;
    if (target_uri.size() == 3) {
        body = json_loader::GetSerializedMaps(app_->GetAllMaps());
    } else if (target_uri.size() == 4) {
        auto id = target_uri[3];
        auto map = app_->FindMapById(Map::Id(static_cast<std::string>(id)));

        body = json_loader::GetSerializedMap(*map);
    }

    return body;
}

StringResponse ApiHandler::GetMapResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    auto body = MakeMapBody(target_uri);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(req.keep_alive());
    response.set(http::field::allow, "GET, HEAD"s);

    return response;
}

std::string ApiHandler::MakeJoinBody(std::string& username, std::string& map_id) {
    auto player_and_token = app_->JoinPlayer(username, map_id);
    auto id = const_cast<Dog*>(player_and_token.first->GetDog())->GetId();
    std::string body = json_loader::GetSerialezedJoinBody(*player_and_token.second, id);

    return body;
}

StringResponse ApiHandler::GetPlayerJoinResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    json::value req_body;
    std::string username;
    std::string map_id;

    try {
        req_body = json::parse(req.body());

        username = static_cast<std::string>(req_body.at("userName").as_string());
        if (username.empty()) {
            return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Invalid argument"s);
        }

        map_id = static_cast<std::string>(req_body.at("mapId").as_string());
        auto map = app_->FindMapById(Map::Id(map_id));
        if (map == nullptr) {
            return ProcessApiError(http::status::not_found, req.version(), ConstructError("mapNotFound", "Map not found"), req.keep_alive());
        } 
    } catch(const std::exception&) {
        return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Join game request parse error"s);
    }                                                    
    auto body = MakeJoinBody(username, map_id);
    response.body() = body;
    response.content_length(body.size());
    response.keep_alive(req.keep_alive());
    response.set(http::field::cache_control, "no-cache");

    return response;
}

std::string ApiHandler::MakePlayersBody(Player* player) {
    json::object players_list;
    auto dogs = const_cast<GameSession*>(player->GetSession())->GetListIdWithName();
    for (auto& [id, name] : dogs) {
        json::object player_name;
        player_name["name"] = name;
        players_list[std::to_string(id)] = player_name;
    }

    std::string body = json::serialize(players_list);

    return body;
}

StringResponse ApiHandler::GetPlayersResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    if (!IsGetOrHeadRequest(req.method())) {
        return MakeInvalidMethodError(req.version(), req.keep_alive());
    }

    return ExecuteAuthorized([&req, &response, this](const Token& token){
        auto player = app_->FindPlayerByToken(token);
        if (player == nullptr) {
            return MakeUnknownTokenError(req.version(), req.keep_alive());
        }
        auto body = MakePlayersBody(player);
        response.body() = body;
        response.result(http::status::ok);
        response.content_length(body.size());
        response.keep_alive(req.keep_alive());
        response.set(http::field::cache_control, "no-cache");

        return response;
    }, req);
}

std::string ApiHandler::MakeStateBody(Player* player) const {
    json::object result;
    json::object players;
    
    auto session = const_cast<GameSession*>(player->GetSession());
    auto dogs = const_cast<GameSession*>(player->GetSession())->GetDogs();
    for (const auto& [id, dog] : *dogs) {
        if (!dog->IsNeedToRetire()) {
                    json::object state;
        auto speed = dog->GetSpeed();
        auto pos = dog->GetPosition();
        state["pos"] = json::array{pos.x, pos.y};
        state["speed"] = json::array{speed.x, speed.y};
        std::string dir_to_string;
        dir_to_string += static_cast<char>(dog->GetDirection());
        state["dir"] = dir_to_string;

        json::array bag;
        if (dog->GetBagSize() > 0) {
            for (const auto& [id, type_id] : dog->GetBag()) {
                json::object obj;
                obj["id"] = id;
                obj["type"] = type_id;
                bag.emplace_back(obj);
            }
        }

        state["bag"] = bag;
        state["score"] = dog->GetScore();
        players[std::to_string(id)] = state;
        }
    }

    json::object lost_objects;
    for (const auto& [id, lost_object] : const_cast<Map*>(session->GetMap())->GetLostObjects()) {
        json::object lost;
        lost["type"] = lost_object.loot->GetLootType();
        lost["pos"] = json::array{lost_object.pos.x, lost_object.pos.y};
        lost_objects[std::to_string(id)] = lost;
    }

    result["players"] = players;
    result ["lostObjects"] = lost_objects;
    std::string body = json::serialize(result);

    return body;
}

StringResponse ApiHandler::GetStateResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    if (!IsGetOrHeadRequest(req.method())) {
        return MakeInvalidMethodError(req.version(), req.keep_alive());
    }

    return ExecuteAuthorized([&req, &response, this](const Token& token){
        auto player = app_->FindPlayerByToken(token);
        if (player == nullptr) {
            return MakeUnknownTokenError(req.version(), req.keep_alive());
        }
        auto body = MakeStateBody(player);
        response.body() = body;
        response.result(http::status::ok);
        response.content_length(body.size());
        response.keep_alive(req.keep_alive());

        return response;
    }, req);
}

StringResponse ApiHandler::GetPlayerActionResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    if (req.method() != http::verb::post) {
        return MakeInvalidMethodError(req.version(), req.keep_alive(), "POST"s);
    }

    if (req.at("Content-Type") != "application/json") {
        return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Invalid content type"s);
    }

    return ExecuteAuthorized([&req, &response, this](const Token& token){
        auto player = app_->FindPlayerByToken(token);
        if (player == nullptr) {
            return MakeUnknownTokenError(req.version(), req.keep_alive());
        }
        auto player_move = json::parse(req.body());
        if (!player_move.as_object().if_contains("move") || 
                (!player_move.at("move").as_string().empty() && !IsDirectionValid(player_move.at("move").as_string()))) {
            return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Failed to parse action"s);
        }
        app_->MakePlayerAction(player, static_cast<std::string>(player_move.at("move").as_string()));
        std::string body = "{}"s;
        response.body() = body;
        response.result(http::status::ok);
        response.content_length(body.size());
        response.keep_alive(req.keep_alive());

        return response;
    }, req);
}

StringResponse ApiHandler::GetTickResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req) {
    if (app_->IsCommandTickSet()) {
        return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Invalid endpoint"s);
    }
    if (req.method() != http::verb::post) {
        return MakeInvalidMethodError(req.version(), req.keep_alive(), "POST"s);
    }

    if (req.at("Content-Type") != "application/json") {
        return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Invalid content type"s);
    }
    json::value req_body;
    int delta;
    try {
        req_body = json::parse(req.body());
        delta = req_body.at("timeDelta").as_int64();
    } catch (const std::exception&) {
        return MakeInvalidArgumentError(req.version(), req.keep_alive(), "Failed to parse tick request JSON"s);
    }
    app_->Tick(delta);
    std::string body = "{}"s;
    response.body() = body;
    response.result(http::status::ok);
    response.content_length(body.size());
    response.keep_alive(req.keep_alive());

    return response;
}

std::string ApiHandler::MakeRecordsBody(int start_elem, int max_elem_count) const {
    auto records_list = app_->GetRecords(start_elem, max_elem_count);

    json::array result;
    for (const auto& [name, score_play_time] : records_list) {
        json::object obj;
        obj["name"] = name;
        obj["score"] = score_play_time.first;
        obj["playTime"] = score_play_time.second / 1000.0;
        result.emplace_back(obj);
    }
    return json::serialize(result);
}

StringResponse ApiHandler::GetRecordsResponse(StringResponse& response, std::vector<std::string>& target_uri, StringRequest& req, std::string params) {
    if (!IsGetOrHeadRequest(req.method())) {
        return MakeInvalidMethodError(req.version(), req.keep_alive());
    }

    int start_elem = 0;
    int max_elem_count = 100;
    if (!params.empty()) {
        size_t first = params.find_first_of('=');
        start_elem = std::stoi(params.substr(first + 1, params.find('&') - first - 1));
        size_t second = params.find_first_of('=', first + 1);
        max_elem_count = std::stoi(params.substr(second + 1, params.length() - second));
        if (max_elem_count > 100) {
            return MakeBadRequestError(req.version(), req.keep_alive());
        }

    }

    auto body = MakeRecordsBody(start_elem, max_elem_count);
    response.body() = body;
    response.result(http::status::ok);
    response.content_length(body.size());
    response.keep_alive(req.keep_alive());

    return response;
}


Response ApiHandler::ProcessApiRequest(http::status status, std::vector<std::string>& target_uri, StringRequest& req,
                                                std::string_view content_type) {
    auto last_target_elem = target_uri[target_uri.size() - 1];
    std::string last_target_elem_wo_params;
    std::string params;
    if (target_uri.size() == 4 && last_target_elem.find_first_of('?') != std::string::npos) {
        std::size_t q_pos = last_target_elem.find_first_of('?');

        params = last_target_elem.substr(q_pos + 1, last_target_elem.length() - q_pos);
        last_target_elem_wo_params = last_target_elem.substr(0, q_pos);
    } else {
        last_target_elem_wo_params = last_target_elem;
    }

    auto version = req.version();
    StringResponse response;
    response.result(status);
    response.set(http::field::content_type, content_type);
    if (target_uri[2] == "game") {
        if (target_uri[3] == "join") {
            response = GetPlayerJoinResponse(response, target_uri, req);
        } else if (last_target_elem_wo_params == "players") {
            response = GetPlayersResponse(response, target_uri, req);
        } else if (last_target_elem_wo_params == "state") {
            response = GetStateResponse(response, target_uri, req);
        } else if (target_uri[3] == "player" && last_target_elem_wo_params == "action") {
            response = GetPlayerActionResponse(response, target_uri, req);
        } else if (last_target_elem_wo_params == "tick") {
            response = GetTickResponse(response, target_uri, req);
        } else if (last_target_elem_wo_params == "records") {
            response = GetRecordsResponse(response, target_uri, req, params);
        } 
    } else {
        response = GetMapResponse(response, target_uri, req);
    }
    response.set(http::field::cache_control, "no-cache");
    return response;
}

//! -------------------------File handler --------------------------------

std::string_view FileHandler::GetContentType(std::string_view file_ext)
{
    if (content_types_.count(file_ext) == 0) {
        return content_types_.at("unknown");
    }
    
    return content_types_.at(file_ext);
}

std::string FileHandler::UrlDecode(const std::string& target) {
    std::istringstream input(target);
    std::ostringstream output;

    char hex;
    int value;

    while (input >> std::noskipws >> hex) {
        if (hex == '%') {
            if (input >> std::hex >> value) {
                output << static_cast<char>(value);
            }
        } else if (hex == '+') {
            output << ' ';
        }
        else {
            output << hex;
        }
    }

    return output.str();
}

// Возвращает true, если каталог p содержится внутри base_path.
bool FileHandler::IsSubPath(std::filesystem::path path, std::filesystem::path base) {
    // Приводим оба пути к каноничному виду (без . и ..)
    path = std::filesystem::weakly_canonical(path);
    base = std::filesystem::weakly_canonical(base);

    // Проверяем, что все компоненты base содержатся внутри path
    for (auto b = base.begin(), p = path.begin(); b != base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

http::file_body::value_type FileHandler::MakeFileBody(std::filesystem::path& full_path) {
    http::file_body::value_type file;
    sys::error_code ec;
    file.open(full_path.c_str(), beast::file_mode::read, ec);
    
    return file;
}

Response FileHandler::ProcessNoneApiRequest(http::status status, std::string& root_path, std::string& target, StringRequest& req) {
    FileResponse response(status, req.version());

    std::string s = UrlDecode(target);

    auto full_path = std::filesystem::path(root_path + target);
    auto full_path_canon = std::filesystem::weakly_canonical(full_path);

    if (!std::filesystem::exists(full_path_canon.parent_path()) || 
        !IsSubPath(std::filesystem::path(full_path), std::filesystem::path(root_path))) {
        return ProcessApiError(http::status::bad_request, req.version(), "Bad File Request"s, req.keep_alive(), {}, "text/plain");
    }

    if (!std::filesystem::exists(full_path_canon)) {
        return ProcessApiError(http::status::not_found, req.version(), "File not found"s, req.keep_alive(), {}, "text/plain");;
    }
    
    response.keep_alive(req.keep_alive());

    if (full_path.has_filename()) {
        auto file_ext = std::filesystem::path(full_path.filename()).extension().string();
        std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower);
        std::string_view content_type = GetContentType(file_ext);
        response.set(http::field::content_type, content_type);
        auto file = std::move(MakeFileBody(full_path_canon));
        response.body() = std::move(file);
    } else {
        auto index = std::filesystem::weakly_canonical(std::filesystem::path(full_path.string() + "/index.html"));
        response.set(http::field::content_type, "text/html");
        response.body() = std::move(MakeFileBody(index));
    }

    // Метод prepare_payload заполняет заголовки Content-Length и Transfer-Encoding
    // в зависимости от свойств тела сообщения
    response.prepare_payload();

    return response;
}

} // namespace http_handler
