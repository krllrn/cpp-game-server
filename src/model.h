#pragma once

#include "players.h"
#include "loot_generator.h"
#include "postgres.h"

#include <cmath>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace model {

using Maps = std::deque<Map>;
using Sessions = std::deque<GameSession>;

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    // квадрат расстояния до точки
    double sq_distance;

    // доля пройденного отрезка
    double proj_ratio;
};

// Движемся из точки a в точку b и пытаемся подобрать точку c.
// Эта функция реализована в уроке.
CollectionResult TryCollectPoint(DogPosition a, DogPosition b, LostObjectPosition c);

} //namespace collision_detector

class Game {
public:
    void AddMap(Map map);

    const Maps& GetMaps() const noexcept;

    const Map* FindMap(const Map::Id& id) const noexcept;

    std::pair<Player*, Token> AddPlayerToSession(const std::string& username, const std::string& map_id, double start_x, double start_y, Road* road);

    const Players* GetPlayers() const;

    void SetDefaultDogSpeed(double speed);

    void UpdateGameState(int interval);

    void SetLootGenerator(double period, double probability);

    Sessions* GetSessions();

    double GetGameTime();

    GameSession* GetSession(const std::string& map_id);

    void SetDogRetirementTime(double sec);

    double GetDogRetirementTime();

    std::unordered_map<std::string, std::pair<int, int>> RetirePlayers();


private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    Maps maps_;
    Sessions sessions_;
    Players players_;
    MapIdToIndex map_id_to_index_;
    double default_dog_speed_ = .0;
    int default_bag_capacity_ = 0;
    int update_interval_ = 0;
    double game_time_ = .0;
    double dog_retirement_time_ = .0;
    std::optional<loot_gen::LootGenerator> loot_generator_ = std::nullopt;


    GameSession* FindSessionFromMapId(const std::string& map_id);

    void UpdateLostObjects(const GameSession* session, int interval);

    void CollectLostObjects(const GameSession& session);

};

}  // namespace model
