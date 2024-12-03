#include "dog.h"

std::uint64_t Dog::GetId() const {
    return id_;
}

std::string Dog::GetName() const {
    return name_;
}

DogPosition Dog::GetPosition() const {
    return position_;
}

DogSpeed Dog::GetSpeed() const {
    return speed_;
}

Direction Dog::GetDirection() const {
    return direction_;
}

void Dog::SetSpeedAndDirection(DogSpeed speed, Direction dir) {
    if (speed.x == .0 && speed.y == .0) {
        speed_ = DogSpeed{0.0, 0.0};
    } else {
        speed_ = speed;
        direction_ = dir;
    }
}

void Dog::SetSpeed(DogSpeed speed) {
    speed_ = speed;
}

void Dog::SetRoadToMove(Road* road) {
    road_to_move_ = road;
}

Road *Dog::GetRoadToMove() const
{
    return road_to_move_;
}

void Dog::SetPosition(DogPosition pos) {
    position_ = pos;
}

void Dog::SetPrevPosition(DogPosition pos)
{
    prev_position_ = pos;
}

void Dog::Move(double delta_time, const std::vector<Road>& roads) {
    // Вычисляем новое положение на основе скорости и времени
    const double MS_TO_SEC_COEF = 0.001;

    DogPosition new_position = position_;
    new_position.x = position_.x + (speed_.x * (delta_time * MS_TO_SEC_COEF));
    new_position.y = position_.y + (speed_.y * (delta_time * MS_TO_SEC_COEF));

    const double EPSILON = 1e-6;
    if (std::abs(new_position.x - position_.x) > EPSILON || std::abs(new_position.y - position_.y) > EPSILON) {
        if (CanMoveTo(new_position, road_to_move_)) {
            prev_position_ = position_;
            position_ = new_position;
        } else {
            auto current_roads = GetCurrentRoad(roads);

            bool can_move = false;
            for (const auto& road : current_roads) {
                if (CanMoveTo(new_position, road)) {
                    prev_position_ = position_;
                    position_ = new_position;
                    road_to_move_ = road;
                    can_move = true;
                    break;
                } else {
                    if (road != road_to_move_) {
                        road_to_move_ = road;
                    }
                }
            }        
            if (!can_move) {
                SetSpeed(DogSpeed{0.0, 0.0});
                StopAtRoadBoundary(road_to_move_, new_position);
            }
        }
    }
}

bool Dog::CanMoveTo(const DogPosition& new_position, const Road* current_road) const {
    if (current_road) {
        Point start = current_road->GetStart();
        Point end = current_road->GetEnd();

        // Определяем минимальные и максимальные координаты
        auto [min_x, max_x] = sdk::GetMinMax(start.x, end.x);
        auto [min_y, max_y] = sdk::GetMinMax(start.y, end.y);

        if (current_road->IsHorizontal() && new_position.y <= max_y + map_const::HALF_OF_ROAD && new_position.y >= min_y - map_const::HALF_OF_ROAD &&
            new_position.x >= min_x - map_const::HALF_OF_ROAD && new_position.x <= max_x + map_const::HALF_OF_ROAD) {
            return true; // Новая позиция на текущей горизонтальной дороге
        }
        if (current_road->IsVertical() && new_position.x <= max_x + map_const::HALF_OF_ROAD && new_position.x >= min_x - map_const::HALF_OF_ROAD &&
            new_position.y >= min_y - map_const::HALF_OF_ROAD && new_position.y <= max_y + map_const::HALF_OF_ROAD) {
            return true; // Новая позиция на текущей вертикальной дороге
        }
    }
    return false; // Новая позиция не на текущей дороге
}

void Dog::StopAtRoadBoundary(const Road* current_road, const DogPosition& new_position) {
    // Логика остановки на границе текущей дороги
    if (current_road) {
        Point start = current_road->GetStart();
        Point end = current_road->GetEnd();

        // Определяем минимальные и максимальные координаты
        auto [min_x, max_x] = sdk::GetMinMax(start.x, end.x);
        auto [min_y, max_y] = sdk::GetMinMax(start.y, end.y);

        if (current_road->IsHorizontal()) {
            // Остановка на границе горизонтальной дороги
            if (direction_ == Direction::NORTH) { // Движение U
                position_.y = min_y - map_const::HALF_OF_ROAD; // Остановка выше дороги
            } else if (direction_ == Direction::SOUTH) { // Движение D
                position_.y = max_y + map_const::HALF_OF_ROAD; // Остановка ниже дороги
            } else if (direction_ == Direction::WEST && new_position.x <= min_x) {  // L
                position_.x = min_x - map_const::HALF_OF_ROAD; // Остановка слева от дороги
            } else if (direction_ == Direction::EAST && new_position.x >= max_x) { // R
                position_.x = max_x + map_const::HALF_OF_ROAD; // Остановка справа от дороги
            }
        } else if (current_road->IsVertical()) {
            // Остановка на границе вертикальной дороги
            if (direction_ == Direction::EAST) { // Движение R
                position_.x = max_x + map_const::HALF_OF_ROAD; // Остановка справа от дороги
            } else if (direction_ == Direction::WEST) { // Движение L
                position_.x = min_x - map_const::HALF_OF_ROAD; // Остановка слева от дороги
            } else if (direction_ == Direction::NORTH && new_position.y <= min_y) {
                // Если собака движется на север, останавливаем её выше дороги
                position_.y = min_y - map_const::HALF_OF_ROAD; // Остановка выше дороги
            } else if (direction_ == Direction::SOUTH && new_position.y >= max_y) {
                // Если собака движется на юг, останавливаем её ниже дороги
                position_.y = max_y + map_const::HALF_OF_ROAD; // Остановка ниже дороги
            }
        }
    }
}

const std::vector<Road*> Dog::GetCurrentRoad(const std::vector<Road>& roads) const {
   std::vector<Road*> founded_roads;
    for (const auto& road : roads) {
        Point start = road.GetStart();
        Point end = road.GetEnd();

        // Определяем минимальные и максимальные координаты
        auto [min_x, max_x] = sdk::GetMinMax(start.x, end.x);
        auto [min_y, max_y] = sdk::GetMinMax(start.y, end.y);

        if (road.IsHorizontal() && position_.y <= max_y + map_const::HALF_OF_ROAD && position_.y >= min_y - map_const::HALF_OF_ROAD &&
            position_.x >= min_x - map_const::HALF_OF_ROAD && position_.x <= max_x + map_const::HALF_OF_ROAD) {
            founded_roads.push_back(const_cast<Road*>(&road)); // Собака на горизонтальной дороге
        }
        if (road.IsVertical() && position_.x <= max_x + map_const::HALF_OF_ROAD && position_.x >= min_x - map_const::HALF_OF_ROAD &&
            position_.y >= min_y - map_const::HALF_OF_ROAD && position_.y <= max_y + map_const::HALF_OF_ROAD) {
            founded_roads.push_back(const_cast<Road*>(&road)); // Собака на вертикальной дороге
        }
    }
    return founded_roads; // Возвращаем найденные дороги
}

size_t Dog::GetBagSize() const noexcept {
    return bag_.size();
}

Bag& Dog::GetBag()
{
    return bag_;
}

void Dog::ClearBag()
{
    bag_.clear();
    bag_score_ = 0;
}

DogPosition Dog::GetPreviousPosition() const
{
    return prev_position_;
}

void Dog::IncreaseScore(int value)
{
    score_ += value;
}

void Dog::IncreaseBagScore(int value)
{
    bag_score_ += value;
}

int Dog::GetScore() const noexcept
{
    return score_;
}

int Dog::GetBagScore() const noexcept
{
    return bag_score_;
}

DogRepr Dog::GetSerializedDog()
{
    return DogRepr();
}

void Dog::AddToBag(int obj_id, int type_id) {
    bag_[obj_id] = type_id;
}

void Dog::IncreaseGameTime(double delta) {
    game_time_ += delta;
}

void Dog::IncreaseRetireTime(double delta) {
    retire_time_ += delta;
}

void Dog::SetNeedToRetire(bool need_to_retire) {
    need_to_retire_ = need_to_retire;
}

double Dog::GetRetireTime() {
    return retire_time_;
}

double Dog::GetGameTime() {
    return game_time_;
}

bool Dog::IsNeedToRetire() {
    return need_to_retire_;
}
