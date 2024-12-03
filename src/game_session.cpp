#include "game_session.h"

const Map* GameSession::GetMap() const {
    return map_;
}

uint64_t GameSession::GetDogsCount() const {
    return dogs_.size();
}

GameSession::Dogs* GameSession::GetDogs() {
    return &id_and_dogs_;
}

void GameSession::AddDog(Dog dog) {
    dogs_.emplace_back(std::move(dog));
    id_and_dogs_[dogs_.back().GetId()] = &dogs_.back();
}

std::map<uint64_t, std::string> GameSession::GetListIdWithName() const {
    std::map<uint64_t, std::string> players_list;
    for (auto& [id, dog] : id_and_dogs_) {
        players_list[id] = dog->GetName();
    }

    return players_list;
}

void GameSession::MoveDogs(double dog_retirment_time, double delta_time) {
    for (const auto& [id, dog] : id_and_dogs_) {
        if (dog->GetSpeed().x == .0 && dog->GetSpeed().y == .0) {
            dog->IncreaseRetireTime(delta_time);
            if (dog->GetRetireTime() >= dog_retirment_time) {
                dog->SetNeedToRetire(true);
            }
        } else {
            dog->Move(delta_time, map_->GetRoads());
            dog->IncreaseGameTime(delta_time);
        }
    }
}

std::deque<Dog>& GameSession::GetDogsList() {
    return dogs_;
}