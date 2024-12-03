#pragma once
#include <pqxx/pqxx>
#include <pqxx/connection>

#include "connection_pool.h"

namespace postgres {

using pqxx::operator"" _zv;

class Database {
public:
    explicit Database(ConnectionPool* connection_pool);

    ConnectionPool::ConnectionWrapper GetConnection() {
        return connection_pool_->GetConnection();
    }

    void Save(std::string name, int score, int game_time);

    std::unordered_map<std::string, std::pair<int, int>> GetLeaderboard(int start_elem, int elem_count);

private:
    ConnectionPool* connection_pool_ = nullptr;
};

}  // namespace postgres