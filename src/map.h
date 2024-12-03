#pragma once

#include "tagged.h"
#include "extra_data.h"

#include <deque>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using Dimension = int;
using Coord = Dimension;

namespace map_const {
    const double HALF_OF_ROAD = 0.4;
}

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct LostObjectPosition {
    double x;
    double y;

    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & x;
        ar & y;
    }
};

struct LostObject {
    LostObjectPosition pos;
    Loot* loot;
};

struct LostObjectRepr {
    LostObjectPosition pos;
    int type;

    template<class Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version){
        ar & pos;
        ar & type;
    }
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept;

    bool IsVertical() const noexcept;

    Point GetStart() const noexcept;

    Point GetEnd() const noexcept;

private:
    Point start_;
    Point end_;
    const double HALF_OF_ROAD = 0.4;
};


class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept;

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept;

    Point GetPosition() const noexcept;

    Offset GetOffset() const noexcept;

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;
    using LootType = int;

    Map(Id id, std::string name, double speed, int bag_capacity) noexcept
        : id_(std::move(id)),
        name_(std::move(name)),
        dog_speed_(std::move(speed)),
        bag_capacity_(std::move(bag_capacity))
        
    {
    }

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;

    const Roads& GetRoads() const noexcept;

    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building);

    void AddOffice(Office office);

    double GetSpeed() const noexcept;

    void AddLootTypes(Loot loot);

    std::deque<Loot> GetLootTypes() const noexcept;

    Loot* GetLootByType(int type);

    Loot* GetLootTypeByPos(int idx);

    void AddLootOnMap(double x, double y, Loot* loot);

    size_t GetLostObjectsCount() const noexcept;

    std::unordered_map<int, LostObject> GetLostObjects() const;

    int GetBagCapacity() const noexcept;

    void RemoveCollectedObj(int id) noexcept;

    void SetLostObject(int id, LostObject obj);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    double dog_speed_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    std::deque<Loot> loot_types_;
    int lost_objects_count_ = 0;
    std::unordered_map<int, LostObject> lost_objects_;
    std::vector<int> founded_obj_;
    int bag_capacity_;
};