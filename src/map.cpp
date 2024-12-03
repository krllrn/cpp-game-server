#include "map.h"

#include <iostream>

//! -------------------------Road --------------------------------

bool Road::IsHorizontal() const noexcept {
    return start_.y == end_.y;
}

bool Road::IsVertical() const noexcept {
    return start_.x == end_.x;
}

Point Road::GetStart() const noexcept {
    return start_;
}

Point Road::GetEnd() const noexcept {
    return end_;
}

//! ------------------------- Building --------------------------------

const Rectangle& Building::GetBounds() const noexcept {
    return bounds_;
}

//! ------------------------- Office --------------------------------

const Office::Id& Office::GetId() const noexcept {
    return id_;
}

Point Office::GetPosition() const noexcept {
    return position_;
}

Offset Office::GetOffset() const noexcept {
    return offset_;
}

//! ------------------------- Map --------------------------------

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

double Map::GetSpeed() const noexcept {
    return dog_speed_;
}

void Map::AddLootTypes(Loot loot)
{
    loot_types_.emplace_back(loot);
}

std::deque<Loot> Map::GetLootTypes() const noexcept
{
    return loot_types_;
}

Loot* Map::GetLootByType(int type)
{
    auto it = std::find_if(loot_types_.begin(), loot_types_.end(), [&type](const Loot& l){
        return type == l.GetLootType();
    });
    return &it.operator*();
}


Loot* Map::GetLootTypeByPos(int idx) {
    return &loot_types_.at(idx);
}

void Map::AddLootOnMap(double x, double y, Loot* loot) {
    lost_objects_[lost_objects_.size() + founded_obj_.size()] = LostObject(LostObjectPosition{x, y}, loot);
}

size_t Map::GetLostObjectsCount() const noexcept {
    return lost_objects_.size();
}

std::unordered_map<int, LostObject> Map::GetLostObjects() const {
    return lost_objects_;
}

void Map::RemoveCollectedObj(int id) noexcept {
    if (lost_objects_.count(id)) {
        founded_obj_.push_back(id);
        lost_objects_.erase(id);
    }
}

void Map::SetLostObject(int id, LostObject obj)
{
    lost_objects_[id] = obj;
}

int Map::GetBagCapacity() const noexcept {
    return bag_capacity_;
}
