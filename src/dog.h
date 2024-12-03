#pragma once

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include "map.h"
#include "sdk.h"

#include <cstdint>
#include <iostream>

static const double DOG_WIDTH = 0.6;

using Bag = std::unordered_map<int, int>;

struct DogPosition {
    double x = 0.0;
    double y = 0.0;

    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & x;
        ar & y;
    }
};

struct DogSpeed {
    double x = 0.0;
    double y = 0.0;

    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & x;
        ar & y;
    }
};

enum Direction : char{
    NORTH = 'U',
    SOUTH = 'D',
    WEST = 'L',
    EAST = 'R'
};

class DogRepr;

class Dog {
public:

    Dog(std::uint64_t id, std::string name)
    : name_(std::move(name)),
        id_(id),
        road_to_move_(nullptr)
    {}

    std::uint64_t GetId() const;

    std::string GetName() const;

    DogPosition GetPosition() const;

    DogSpeed GetSpeed() const;

    Direction GetDirection() const;

    void SetSpeedAndDirection(DogSpeed speed, Direction dir);
    
    void SetSpeed(DogSpeed speed);

    void SetPosition(DogPosition pos);

    void SetPrevPosition(DogPosition pos);

    void SetRoadToMove(Road* road); // for tests

    Road* GetRoadToMove() const;

    void Move(double delta_time, const std::vector<Road>& roads);

    void AddToBag(int obj_id, int type_id);

    size_t GetBagSize() const noexcept;

    Bag& GetBag();

    void ClearBag();

    DogPosition GetPreviousPosition() const;

    void IncreaseScore(int value);

    void IncreaseBagScore(int value);

    int GetScore() const noexcept;

    int GetBagScore() const noexcept;

    DogRepr GetSerializedDog();

    void IncreaseGameTime(double delta);

    void IncreaseRetireTime(double delta);

    void SetNeedToRetire(bool need_to_retire);

    double GetRetireTime();

    double GetGameTime();

    bool IsNeedToRetire();

private:
    std::uint64_t id_;
    std::string name_;
    DogPosition position_ = {};
    DogPosition prev_position_ = {};
    DogSpeed speed_ = {};
    Direction direction_ = Direction::NORTH;
    Road* road_to_move_;
    Bag bag_;
    int score_ = 0;
    int bag_score_ = 0;
    double game_time_ = .0;
    double retire_time_ = .0;
    bool need_to_retire_ = false;

    bool CanMoveTo(const DogPosition& new_position, const Road* current_road) const;

    void StopAtRoadBoundary(const Road* current_road, const DogPosition& new_position);

    const std::vector<Road*> GetCurrentRoad(const std::vector<Road>& roads) const;

};

class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const Dog& dog)
        : id_(dog.GetId())
        , name_(dog.GetName())
        , position_(dog.GetPosition())
        , prev_position_(dog.GetPreviousPosition())
        , speed_(dog.GetSpeed())
        , direction_(dog.GetDirection())
        , bag_(const_cast<Dog&>(dog).GetBag())
        , score_(dog.GetScore())
        , bag_score_(dog.GetBagScore())
    {
    }

    [[nodiscard]] Dog Restore() const {
        Dog dog{id_, name_};
        dog.SetPosition(position_);
        dog.SetPrevPosition(prev_position_);
        dog.SetSpeedAndDirection(speed_, direction_);
        for (const auto& [obj_id, type_id] : bag_) {
            dog.AddToBag(obj_id, type_id);
        }
        dog.IncreaseScore(score_);
        dog.IncreaseBagScore(bag_score_);
        dog.IncreaseGameTime(game_time_);
        dog.IncreaseRetireTime(retire_time_);
        dog.SetNeedToRetire(need_to_retire_);

        return dog;
    }

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar& id_;
        ar& name_;
        ar& position_;
        ar& prev_position_;
        ar& speed_;
        ar& direction_;
        ar& bag_;
        ar& score_;
        ar& bag_score_;
    }

private:
    std::uint64_t id_;
    std::string name_;
    DogPosition position_ = {};
    DogPosition prev_position_ = {};
    DogSpeed speed_ = {};
    Direction direction_ = Direction::NORTH;
    Bag bag_;
    int score_ = 0;
    int bag_score_ = 0;
    double game_time_ = .0;
    double retire_time_ = .0;
    bool need_to_retire_ = false;
};