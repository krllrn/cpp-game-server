#pragma once

#include "dog.h"

#include <deque>
#include <map>

class GameSession {
public:
    using Dogs = std::map<std::uint64_t, Dog*>;

    GameSession& operator=(const GameSession&) = delete;

    explicit GameSession(const Map* map) 
    : map_(map)
    {}

    const Map* GetMap() const;

    uint64_t GetDogsCount() const;

    Dogs* GetDogs();

    std::deque<Dog>& GetDogsList();

    void AddDog(Dog dog);

    std::map<uint64_t, std::string> GetListIdWithName() const;

    void MoveDogs(double dog_retirment_time, double delta_time);
    
private:
    std::deque<Dog> dogs_;
    Dogs id_and_dogs_;
    const Map* map_;
};
