#pragma once

#include "json_loader.h"


class ApplicationListener {
public:
    ApplicationListener() = default;
    ~ApplicationListener() = default; // make dtor
    ApplicationListener(ApplicationListener&&) = default;  // support moving
    ApplicationListener& operator=(ApplicationListener&&) = default;
    ApplicationListener(const ApplicationListener&) = default; // support copying
    ApplicationListener& operator=(const ApplicationListener&) = default;
    
    virtual void OnTick(double delta, model::Game* game) = 0;

    virtual void SaveState(model::Game* game) = 0;
};

class Application {
public:
    explicit Application(model::Game* game, ConnectionPool* conn_pool) 
    :game_(game),
    db_{conn_pool}
    {}

    Application& operator=(const Application&) = delete;

    const Map* FindMapById(Map::Id id);

    const model::Maps& GetAllMaps();

    const Players* GetAllPlayers();

    Player* FindPlayerByToken(const Token token);

    std::pair<Player *, Token> JoinPlayer(std::string& username, std::string& map_id);

    void MakePlayerAction(Player* player, std::string dir);

    void UpdateState(int tick);

    void SetTickAvailable();

    bool IsCommandTickSet();

    void SetRandomSpawnAvailable();

    bool IsRandomSpawnSet();

    void SetApplicationListener(std::shared_ptr<ApplicationListener> listener);

    void Tick(double delta);

    void SaveState();

    std::unordered_map<std::string, std::pair<int, int>> GetRecords(int start_elem, int elem_count);

private:
    postgres::Database db_;
    model::Game* game_;
    std::shared_ptr<ApplicationListener> listener_ = nullptr;
    bool is_command_tick_set_ = false;
    bool is_random_spawn_set_ = false;
    double last_save_time_ = .0;
};