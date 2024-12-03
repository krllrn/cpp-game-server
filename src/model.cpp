#include "model.h"
#include "model.h"

#include <algorithm>
#include <stdexcept>

namespace model {
using namespace std::literals;

namespace collision_detector {

CollectionResult TryCollectPoint(DogPosition a, DogPosition b, LostObjectPosition c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пoскольку при сборе придётся учитывать перемещение даже на небольшое
    // расстояние.
    if (b.x != a.x || b.y != a.y) {
        const double u_x = c.x - a.x;
        const double u_y = c.y - a.y;
        const double v_x = b.x - a.x;
        const double v_y = b.y - a.y;
        const double u_dot_v = u_x * v_x + u_y * v_y;
        const double u_len2 = u_x * u_x + u_y * u_y;
        const double v_len2 = v_x * v_x + v_y * v_y;
        const double proj_ratio = u_dot_v / v_len2;
        const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

        return CollectionResult(sq_distance, proj_ratio);
    }
}

} //namespace collision_detector

//! ------------------------- Game --------------------------------
const Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

std::pair<Player*, Token> Game::AddPlayerToSession(const std::string& username, const std::string& map_id, double start_x, double start_y, Road* road_to_move) {
    auto session = GetSession(map_id);
    Dog dog{session->GetDogsCount(), username};
    
    dog.SetPosition(DogPosition{start_x, start_y});
    dog.SetRoadToMove(road_to_move);
    session->AddDog(std::move(dog));
    auto last = std::prev(const_cast<GameSession*>(session)->GetDogs()->end());
    return players_.Add(*last->second, *session);
}

const Players* Game::GetPlayers() const {
    return &players_;
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
    
}

GameSession* Game::FindSessionFromMapId(const std::string& map_id) {
    if (sessions_.size() == 0) {
        return nullptr;
    }

    auto session = std::find_if(sessions_.begin(), sessions_.end(), [&map_id](const GameSession s){
        return *(s.GetMap()->GetId()) == map_id;
    });

    if (session == sessions_.end()) {
        return nullptr;
    }
    return &*session;        
}

GameSession* Game::GetSession(const std::string& map_id) {
    auto session = FindSessionFromMapId(map_id);
    if (session == nullptr) {
        auto map = FindMap(Map::Id(map_id));
        sessions_.emplace_back(GameSession{map});
        session = &sessions_.back();
    }
    return session;    
}

void Game::SetDogRetirementTime(double sec)
{
    dog_retirement_time_ = sec * 1000;
}

double Game::GetDogRetirementTime()
{
    return dog_retirement_time_;
}

void Game::SetDefaultDogSpeed(double speed) {
    default_dog_speed_ = std::move(speed);
}

void Game::UpdateLostObjects(const GameSession* session, int interval) {
    auto map = session->GetMap();
    std::chrono::duration<int, std::milli> chrono_milliseconds{ interval };
    int need_to_generate = loot_generator_->Generate(std::chrono::duration_cast<std::chrono::milliseconds>(chrono_milliseconds), 
                                                    map->GetLostObjectsCount(), session->GetDogsCount());
    if (need_to_generate > 0) {
        auto roads = map->GetRoads();
        for (auto i = 0; i < need_to_generate; ++i) {
            int r_road = sdk::GetRandomInt(0, roads.size() - 1);
            auto start = roads[r_road].GetStart();
            auto end = roads[r_road].GetEnd();
            
            auto [min_x, max_x] = sdk::GetMinMax(start.x, end.x);
            auto [min_y, max_y] = sdk::GetMinMax(start.y, end.y);

            auto loot_type = sdk::GetRandomInt(0, map->GetLootTypes().size() - 1);
            double loot_x = sdk::GetRandomDouble(min_x, max_x);
            double loot_y = sdk::GetRandomDouble(min_y, max_y);
            const_cast<Map*>(map)->AddLootOnMap(loot_x, loot_y, const_cast<Map*>(map)->GetLootTypeByPos(loot_type));
        }
    }
}

std::vector<collision_detector::CollectionResult> MoveThroughOffice(const Map* map, DogPosition pos_before_move, DogPosition pos_after_move) {
    std::vector<collision_detector::CollectionResult> offices_on_way;
    auto offices = map->GetOffices();
    for (const auto& office : offices) {
        collision_detector::CollectionResult move_through_office = 
                                    collision_detector::TryCollectPoint(pos_before_move, pos_after_move, 
                                                                        LostObjectPosition{office.GetPosition().x - office.GetOffset().dx, 
                                                                                            office.GetPosition().y - office.GetOffset().dy});
        if (move_through_office.IsCollected((0.5 / 2) + (0.6 / 2))) {
            offices_on_way.push_back(move_through_office);
        }
    }
    return offices_on_way;
}

void Game::CollectLostObjects(const GameSession& session) {
    auto dogs = const_cast<GameSession&>(session).GetDogs();
    for (auto& [dog_id, dog] : *dogs) {
        auto pos_before_move = dog->GetPreviousPosition();
        auto pos_after_move = dog->GetPosition();
        if (pos_before_move.x == pos_after_move.x && pos_before_move.y == pos_after_move.y) {
            continue;
        }

        auto map = session.GetMap();

        std::vector<collision_detector::CollectionResult> move_through_offices = MoveThroughOffice(map, pos_before_move, pos_after_move);

        auto lost_objects = const_cast<Map*>(map)->GetLostObjects();
        for (const auto& [id_on_map, lost_object] : lost_objects) {
            collision_detector::CollectionResult collect_result = 
                                        collision_detector::TryCollectPoint(pos_before_move, pos_after_move, lost_object.pos);
            auto it = std::find_if(move_through_offices.begin(), move_through_offices.end(), 
                                    [&collect_result](collision_detector::CollectionResult& office) {
                                        return office.proj_ratio < collect_result.proj_ratio;
            });
            if (it != move_through_offices.end()) {
                dog->IncreaseScore(dog->GetBagScore());
                dog->ClearBag();
            }

            if (collect_result.IsCollected(DOG_WIDTH / 2)) {
                if (dog->GetBagSize() < map->GetBagCapacity()) {
                    dog->AddToBag(id_on_map, lost_object.loot->GetLootType());
                    dog->IncreaseBagScore(lost_object.loot->GetRate());
                    const_cast<Map*>(map)->RemoveCollectedObj(id_on_map);
                } else {
                    continue;
                }
            }
        }
    }
}

void Game::UpdateGameState(int interval) {
    for (const auto& session : sessions_) {
        UpdateLostObjects(&session, interval);
        const_cast<GameSession&>(session).MoveDogs(dog_retirement_time_, interval);
        CollectLostObjects(session);  
    }
    game_time_ += interval;
}

std::unordered_map<std::string, std::pair<int, int>> Game::RetirePlayers() {
    std::unordered_map<std::string, std::pair<int, int>> retire_players;
    auto players_and_tokens = players_.GetPlayersWithTokens();
    for (const auto& [token, player] : players_and_tokens->TokenAndPlayer()) {
        if (player && player->GetDog()->IsNeedToRetire()) {
            auto dog = *player->GetDog();
            retire_players[dog.GetName()] = std::make_pair(dog.GetScore(), dog.GetGameTime() + dog.GetRetireTime());
            //db->Save(dog.GetName(), dog.GetScore(), dog.GetGameTime() + dog.GetRetireTime());
            players_.DeletePlayerByToken(token);
        }
    }

    return retire_players;
}

void Game::SetLootGenerator(double period, double probability)
{
    std::chrono::duration<double, std::milli> chrono_milliseconds{ period * 1000.0 };
    loot_generator_ = loot_gen::LootGenerator(std::chrono::duration_cast<std::chrono::milliseconds>(chrono_milliseconds), probability);
}

Sessions* Game::GetSessions()
{
    return &sessions_;
}

double Game::GetGameTime() {
    return game_time_;
}

} // namespace model