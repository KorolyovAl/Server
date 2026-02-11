#include "dog.h"

namespace model {

const std::string& Dog::GetName() const {
    return name_;
}
 
const pos::Position& Dog::GetPosition() const {
    return pos_;
}

const pos::Coordinate& Dog::GetCoordinates() const {
    return pos_.coordinates_;
}

const pos::Velocity& Dog::GetVelocity() const {
    return pos_.velocity_;
}

const pos::Direction& Dog::GetDirection() const {
    return pos_.direction_;
}

const int Dog::GetId() const {
    return id_;
}

int Dog::GetScore() const {
    return score_;
}

const double Dog::GetPickupRadius() const {
    return pickup_length_ / 2.0;
}

const int Dog::GetBagCapacity() const {
    return bag_capacity_;
}

void Dog::SetCoordinates(const pos::Coordinate& coordinates) {
    pos_.coordinates_ = coordinates;
}

void Dog::SetVelocity(const pos::Velocity& velocity) {
    pos_.velocity_ = velocity;
}

void Dog::SetDirection(const pos::Direction& direction) {
    pos_.direction_ = direction;
}

bool Dog::AddItem(const LootItem& item) {
    if (collected_items_.size() < bag_capacity_) {
        collected_items_.push_back({item.id, item.info});
        return true;
    }

    return false;
}

const std::vector<std::pair<ItemId, LootInfo>>& Dog::GetCollectedItems() const {
    return collected_items_;
}

void Dog::ClearItems() {
    for (auto& item : collected_items_) {
        AddScore(item.second.value);
    }
    collected_items_.clear();
}

void Dog::AddScore(int score) {
    score_ += score;
}

} // namespace model