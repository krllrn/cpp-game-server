#include "players.h"
#include "players.h"

#include <iomanip>

//! ------------------------- Player --------------------------------

Dog* Player::GetDog() const {
    return dog_;
}

GameSession* Player::GetSession() const{
    return game_session_;
}

//! ------------------------- PlayerTokens --------------------------------

size_t TokenHasher::operator()(const Token &token) const
{
    size_t hash_s = std::hash<std::string>{}(*token) * 37;
    return hash_s;
}

Player* PlayerTokens::FindPlayerByToken(Token token) {
    return token_to_player_.count(token) == 0 ? nullptr : token_to_player_.at(token);
}

Token PlayerTokens::AddPlayer(Player& player) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << generator1_.operator()();
    ss << std::hex << std::setfill('0') << std::setw(16) << generator2_.operator()();
    Token token{ss.str()};
    token_to_player_[token] = &player;

    return token;
}

void PlayerTokens::AddPlayerWithToken(std::string t, Player &player)
{
    Token token{t};
    token_to_player_[token] = &player;
}

std::unordered_map<Token, Player *, TokenHasher>& PlayerTokens::TokenAndPlayer()
{
    return token_to_player_;
}

void PlayerTokens::DeleteByToken(Token token) {
    if (token_to_player_.count(token) != 0) {
        token_to_player_.at(token) = nullptr;
    }
}

//! ------------------------- Players --------------------------------

std::pair<Player*, Token> Players::Add(Dog& dog, GameSession& session) {
    players_.emplace_back(Player{&session, &dog});
    auto token = player_tokens_.AddPlayer(players_.back());
    auto player_ptr = player_tokens_.FindPlayerByToken(token);

    return std::make_pair(player_ptr, token);
}

void Players::AddPlayer(Player player)
{
    players_.emplace_back(std::move(player));
}

Player* Players::FindByToken(Token token) {
    return player_tokens_.FindPlayerByToken(token);
}

std::deque<Player>& Players::GetAllPlayers() {
    return players_;
}

PlayerTokens* Players::GetPlayersWithTokens()
{
    return &player_tokens_;
}

void Players::DeletePlayerByToken(Token token) {
    if (FindByToken(token)) {
        player_tokens_.DeleteByToken(token);
    }
}

