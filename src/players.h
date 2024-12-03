#pragma once

#include "game_session.h"

struct TokenTag {
    std::string tag;
};

using Token = util::Tagged<std::string, TokenTag>;

class Player {
public:
    Player(GameSession* game_session, Dog* dog)
    : game_session_(game_session),
    dog_(dog)
    {}

    Dog* GetDog() const;

    GameSession* GetSession() const;

private:
    GameSession* game_session_;
    Dog* dog_;
};

struct TokenHasher {
    size_t operator()(const Token& token) const;
};

class PlayerTokens {
    
public:
    Player* FindPlayerByToken(Token token);

    Token AddPlayer(Player& player);

    void AddPlayerWithToken(std::string token, Player& player);

    void DeleteByToken(Token token);

    std::unordered_map<Token, Player*, TokenHasher>& TokenAndPlayer();

private:

    std::unordered_map<Token, Player*, TokenHasher> token_to_player_;

    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};


class Players {
public:
    std::pair<Player*, Token> Add(Dog& dog, GameSession& session);

    void AddPlayer(Player player);

    Player* FindByToken(Token token);

    std::deque<Player>& GetAllPlayers();

    PlayerTokens* GetPlayersWithTokens();

    void DeletePlayerByToken(Token token);

private:
    PlayerTokens player_tokens_;
    std::deque<Player> players_;
};
