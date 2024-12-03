#include "application.h"
#include "application.h"

//! ------------------------- Application --------------------------------

const Map* Application::FindMapById(Map::Id id) {
    return game_->FindMap(id);
}

const model::Maps& Application::GetAllMaps() {
    return game_->GetMaps();
}

const Players* Application::GetAllPlayers() {
    return game_->GetPlayers();
}

Player* Application::FindPlayerByToken(const Token token) {
    return const_cast<Players*>(game_->GetPlayers())->FindByToken(token);
}

std::pair<Player*, Token> Application::JoinPlayer(std::string& username, std::string& map_id) {
    auto roads = game_->FindMap(Map::Id{map_id})->GetRoads();
    double start_x, start_y;
    Road* road_to_move = nullptr;
    if (is_random_spawn_set_) {
        int r_road = sdk::GetRandomInt(0, roads.size());
        auto start = roads[r_road].GetStart();
        auto end = roads[r_road].GetEnd();
        road_to_move = &roads[r_road];

        auto [min_x, max_x] = sdk::GetMinMax(start.x, end.x);
        auto [min_y, max_y] = sdk::GetMinMax(start.y, end.y);

        start_x = sdk::GetRandomDouble(min_x, max_x);
        start_y = sdk::GetRandomDouble(min_y, max_y);
    } else {
        start_x = roads.begin()->GetStart().x;
        start_y = roads.begin()->GetStart().y;
        road_to_move = &*roads.begin();
    }
    return game_->AddPlayerToSession(username, map_id, start_x, start_y, road_to_move); 
}

void Application::MakePlayerAction(Player* player, std::string dir) {
    if (dir == "") {
        player->GetDog()->SetSpeedAndDirection(DogSpeed{.0, .0}, Direction{});
    } else {
        Direction d = static_cast<Direction>(dir[0]);
        auto speed = player->GetSession()->GetMap()->GetSpeed();
        switch (d)
        {
        case 'L':
            player->GetDog()->SetSpeedAndDirection(DogSpeed{-speed, .0}, d);
            break;
        case 'R':
            player->GetDog()->SetSpeedAndDirection(DogSpeed{speed, .0}, d);
            break;
        case 'U':
            player->GetDog()->SetSpeedAndDirection(DogSpeed{.0, -speed}, d);
            break;
        case 'D':
            player->GetDog()->SetSpeedAndDirection(DogSpeed{.0, speed}, d);
            break;            
        default:
            std::cerr << "Wrong direction!" << '\n';
            break;
        }
    }
}

void Application::UpdateState(int tick) {
    game_->UpdateGameState(tick);
    auto retire_players = game_->RetirePlayers();
    if (retire_players.size() != 0) {
        for (const auto& [name, score_play_time] : retire_players) {
            db_.Save(name, score_play_time.first, score_play_time.second);
        }
    }
}

void Application::SetTickAvailable() {
    is_command_tick_set_ = true;
}

bool Application::IsCommandTickSet() {
    return is_command_tick_set_;
}

void Application::SetRandomSpawnAvailable() {
    is_random_spawn_set_ = true;
}

bool Application::IsRandomSpawnSet() {
    return is_random_spawn_set_;
}

void Application::SetApplicationListener(std::shared_ptr<ApplicationListener> listener)
{
    listener_ = listener;
}

void Application::Tick(double delta)
{
    UpdateState(delta);
    if(listener_) {
        listener_->OnTick(delta, game_);
    }
    last_save_time_ += delta;
}

void Application::SaveState()
{
    if(listener_) {
        listener_->SaveState(game_);
    }
}

std::unordered_map<std::string, std::pair<int, int>> Application::GetRecords(int start_elem, int elem_count)
{
    return db_.GetLeaderboard(start_elem, elem_count);
}
