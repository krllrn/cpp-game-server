#pragma once

#include "model.h"

#include <boost/json.hpp>
#include <filesystem>

namespace json_loader {

namespace json = boost::json;

void LoadGame(model::Game& game, const std::filesystem::path& json_path);

std::string GetSerializedMaps(const model::Maps& maps);

std::string GetSerializedMap(const Map& map);

std::string GetSerialezedJoinBody(const std::string& auth_token, const std::uint64_t id);

std::string GetLogRequest(std::string& ip, std::string& uri, std::string& method);

std::string GetLogResponse(int response_time, int code, std::string content_type = "null");

}  // namespace json_loader
