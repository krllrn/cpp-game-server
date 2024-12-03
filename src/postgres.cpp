#include "postgres.h"

#include <pqxx/zview.hxx>

#include <string>
#include <iostream>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

Database::Database(ConnectionPool* connection_pool) 
: connection_pool_{connection_pool}
{
} 

void Database::Save(std::string name, int score, int game_time) {
    auto conn = connection_pool_->GetConnection();
    pqxx::transaction work{*conn};
    try {
        work.exec_params(
            R"(
    INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3);
    )"_zv,
        name, score, game_time);
        work.commit();
    } catch (const std::exception& e) {
        work.abort();
        throw;
    }
}

std::unordered_map<std::string, std::pair<int, int>> Database::GetLeaderboard(int start_elem, int elem_count) {
    std::unordered_map<std::string, std::pair<int, int>> result;
    auto conn = connection_pool_->GetConnection();
    pqxx::work work{*conn};
    auto query_result = work.exec_params(R"(SELECT * FROM retired_players LIMIT $1 OFFSET $2;)"_zv, elem_count, start_elem);
    const std::size_t num_rows = std::size(query_result);
    if (num_rows > 0) {
        for (std::size_t row_num = 0u; row_num < num_rows; ++row_num)
        {
            const pqxx::row row = query_result[row_num];
            result[row["name"].c_str()] = std::make_pair(std::stoi(row["score"].c_str()), std::stoi(row["play_time_ms"].c_str()));
        }
    }
    return result;
}

} // namespace postgres