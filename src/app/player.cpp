#include "player.h"
 
namespace application {

Player::Id Player::GetId() const {
    return id_;
}

const pos::Position& Player::GetPosition() const {
    return dog_->GetPosition();
}

const model::Dog* Player::GetDog() const {
    return dog_;
}

std::vector<std::pair<size_t, size_t>> Player::GetCollectedItems() const {
    const auto& items = dog_->GetCollectedItems();
    std::vector<std::pair<size_t, size_t>> result;
    for (const auto& item : items) {
        result.push_back({static_cast<size_t>(item.first), static_cast<size_t>(item.second.type)});
    }
    return result;
}

int Player::GetScore() const {
    return dog_->GetScore();
}

const model::Map::Id& Player::GetMapId() const {
    return map_id_;
}

} // namespace application
