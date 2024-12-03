#include "json_loader.h"
#include "json_loader.h"

#include <fstream>
#include <iostream>

namespace json_loader {

namespace json = boost::json;

namespace str_literals {
    static const std::string ID = "id";
    static const std::string NAME = "name";
    static const std::string ROADS = "roads";
    static const std::string BUILDINGS = "buildings";
    static const std::string OFFICES = "offices";
    static const std::string ROADS_X0 = "x0";
    static const std::string ROADS_Y0 = "y0";
    static const std::string ROADS_X1 = "x1";
    static const std::string ROADS_Y1 = "y1";
    static const std::string X = "x";
    static const std::string OFFSET_X = "offsetX";
    static const std::string Y = "y";
    static const std::string OFFSET_Y = "offsetY";
    static const std::string W = "w";
    static const std::string H = "h";
}

namespace {

void FillMapWithRoads(const boost::json::value& map_j, Map& map) {
    for (const auto& road : map_j.at(str_literals::ROADS).as_array()) {
        if (road.as_object().if_contains(str_literals::ROADS_X1)) {
            map.AddRoad(Road(Road::HORIZONTAL, 
                        Point(road.at(str_literals::ROADS_X0).as_int64(), road.at(str_literals::ROADS_Y0).as_int64()), 
                        road.at(str_literals::ROADS_X1).as_int64()));
        }
        if (road.as_object().if_contains(str_literals::ROADS_Y1)) {
            map.AddRoad(Road(Road::VERTICAL, 
                        Point(road.at(str_literals::ROADS_X0).as_int64(), road.at(str_literals::ROADS_Y0).as_int64()), 
                        road.at(str_literals::ROADS_Y1).as_int64()));
        }
    }
}

void FillMapWithBuildings(const boost::json::value& map_j, Map& map) {
    for (const auto& building : map_j.at(str_literals::BUILDINGS).as_array()) {
        Rectangle bounds;
        Size size;
        size.width = building.at(str_literals::W).as_int64();
        size.height = building.at(str_literals::H).as_int64();
        bounds.position = Point(building.at(str_literals::X).as_int64(), building.at(str_literals::Y).as_int64());
        bounds.size = size;

        map.AddBuilding(Building(bounds));
    }
}

void FillMapWithOffices(const boost::json::value& map_j, Map& map) {
    for (const auto& office : map_j.at(str_literals::OFFICES).as_array()) {
        Office::Id id(office.at(str_literals::ID).as_string().c_str());
        Point position;
        position.x = office.at(str_literals::X).as_int64();
        position.y = office.at(str_literals::Y).as_int64();

        Offset offset(office.at(str_literals::OFFSET_X).as_int64(), office.at(str_literals::OFFSET_Y).as_int64());

        map.AddOffice(Office(id, position, offset));
    }
}

json::array GetRoads(const Map& map) {
    json::array roads;
    for (const auto& road : map.GetRoads()) {
        json::object road_j;
        if (road.IsHorizontal()) {
            road_j[str_literals::ROADS_X0] = road.GetStart().x;
            road_j[str_literals::ROADS_Y0] = road.GetStart().y;
            road_j[str_literals::ROADS_X1] = road.GetEnd().x;
        }
        if (road.IsVertical()) {
            road_j[str_literals::ROADS_X0] = road.GetStart().x;
            road_j[str_literals::ROADS_Y0] = road.GetStart().y;
            road_j[str_literals::ROADS_Y1] = road.GetEnd().y;
        }
        roads.emplace_back(road_j);
    }
    return roads;
}

json::array GetBuildings(const Map& map) {
    json::array buildings;
    for (const auto& building : map.GetBuildings()) {
        json::object building_j;
        building_j[str_literals::X] = building.GetBounds().position.x;
        building_j[str_literals::Y] = building.GetBounds().position.y;
        building_j[str_literals::W] = building.GetBounds().size.width;
        building_j[str_literals::H] = building.GetBounds().size.height;

        buildings.emplace_back(building_j);
    }

    return buildings;
}

json::array GetOffices(const Map& map) {
    json::array offices;
    for (const auto& office : map.GetOffices()) {
        json::object office_j;
        office_j[str_literals::ID] = *office.GetId();
        office_j[str_literals::X] = office.GetPosition().x;
        office_j[str_literals::Y] = office.GetPosition().y;
        office_j[str_literals::OFFSET_X] = office.GetOffset().dx;
        office_j[str_literals::OFFSET_Y] = office.GetOffset().dy;

        offices.emplace_back(office_j);
    }

    return offices;
}

void SetLootGenerator(const json::object loot_generator, model::Game* game) {
    game->SetLootGenerator(loot_generator.at("period").as_double(), loot_generator.at("probability").as_double());
}

} //namespace


void LoadGame(model::Game& game, const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    // Распарсить строку как JSON, используя boost::json::parse
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file.");
    }

    std::ostringstream str;
    str << ifs.rdbuf();
    auto config = json::parse(str.str());

    auto default_dog_speed_param = config.as_object().if_contains("defaultDogSpeed");
    auto default_dog_speed = default_dog_speed_param ? default_dog_speed_param->as_double() : 1.0;

    auto default_bag_capacity_param = config.as_object().if_contains("defaultBagCapacity");
    auto default_bag_capacity = default_bag_capacity_param ? default_bag_capacity_param->as_int64() : 3;

    auto default_dog_retirement_param = config.as_object().if_contains("dogRetirementTime");
    auto default_dog_retirement = default_dog_retirement_param ? default_dog_retirement_param->as_double() : 60.0;
    game.SetDogRetirementTime(default_dog_retirement);

    json::object loot_generator = config.at("lootGeneratorConfig").as_object();
    SetLootGenerator(loot_generator, &game);

    auto maps = config.at("maps");

    for (const auto& map_j : maps.as_array()) {
        auto dog_speed_on_map_param = map_j.as_object().if_contains("dogSpeed");
        auto dog_speed_on_map = dog_speed_on_map_param ? dog_speed_on_map_param->as_double() : default_dog_speed;

        auto bag_capacity_on_map_param = map_j.as_object().if_contains("bagCapacity");
        auto bag_capacity_on_map = bag_capacity_on_map_param ? bag_capacity_on_map_param->as_int64() : default_bag_capacity;
        
        Map map(Map::Id(map_j.at(str_literals::ID).as_string().c_str()), 
                        map_j.at(str_literals::NAME).as_string().c_str(),
                        dog_speed_on_map, bag_capacity_on_map);

        json::array loot_types = map_j.at("lootTypes").as_array();

        for (const auto& l : loot_types) {
            map.AddLootTypes(Loot{l.as_object()});
        }

        FillMapWithRoads(map_j, map);
        FillMapWithBuildings(map_j, map);
        FillMapWithOffices(map_j, map);

        game.AddMap(map);
    }
    game.SetDefaultDogSpeed(default_dog_speed);
}

std::string GetSerializedMaps(const model::Maps& maps)
{
    json::array arr;
    for (const auto& map : maps) {
        json::object obj;
        obj[str_literals::ID] = *map.GetId();
        obj[str_literals::NAME] = map.GetName();
        arr.emplace_back(obj);
    }
    return json::serialize(arr);
}


std::string GetSerializedMap(const Map& map)
{
    json::object obj;
    obj[str_literals::ID] = *map.GetId();
    obj[str_literals::NAME] = map.GetName();
    json::array l;
    for (auto& loot : map.GetLootTypes()) {
        l.emplace_back(loot.GetLootObject());
    }
    obj["lootTypes"] = l;
    obj[str_literals::ROADS] = GetRoads(map);
    obj[str_literals::BUILDINGS] = GetBuildings(map);
    obj[str_literals::OFFICES] = GetOffices(map);


    return json::serialize(obj);
}

std::string GetSerialezedJoinBody(const std::string& auth_token, const std::uint64_t id) {
    json::object obj;
    obj["authToken"] = auth_token;
    obj["playerId"] = id;

    return json::serialize(obj);
}

std::string GetLogRequest(std::string& ip, std::string& uri, std::string& method) {
    json::array entry;
    json::object message;
    message["message"] = "request received";
    entry.emplace_back(message);

    json::object data;
    data["ip"] = ip;
    data["URI"] = uri;
    data["method"] = method;
    entry.emplace_back(data);

    return json::serialize(entry);
}

std::string GetLogResponse(int response_time, int code, std::string content_type/* = "null"*/) {
    json::array entry;
    json::object message;
    message["message"] = "response sent";
    entry.emplace_back(message);
    
    json::object data;
    data["response_time"] = response_time;
    data["code"] = code;
    data["content_type"] = content_type;
    entry.emplace_back(data);

    return json::serialize(entry);
}

}  // namespace json_loader