#pragma once

#include "../detail/position.h"
#include "loot_struct.h"

#include <vector>
#include <string>
 
namespace model {
    
    class Dog {
    public:
        explicit Dog(std::string name, int id, pos::Coordinate&& coordinates, int bag_capacity)
            : name_{std::move(name)}
            , id_{id}
            , pos_{pos::Position{std::move(coordinates), pos::Velocity{0, 0}, pos::Direction::NORTH}}
            , bag_capacity_{bag_capacity} {
        }

        const std::string& GetName() const;
        const pos::Position& GetPosition() const;
        const pos::Coordinate& GetCoordinates() const;
        const pos::Velocity& GetVelocity() const;
        const pos::Direction& GetDirection() const;
        const int GetId() const;
        int GetScore() const;
        const double GetPickupRadius() const;
        const int GetBagCapacity() const;
        const std::vector<std::pair<ItemId, LootInfo>>& GetCollectedItems() const;

        void SetCoordinates(const pos::Coordinate& coordinates);
        void SetVelocity(const pos::Velocity& velocity);
        void SetDirection(const pos::Direction& direction);

        bool AddItem(const LootItem& item);
        void ClearItems();
        void AddScore(int score);        

    private:
        std::string name_;
        int id_;
        pos::Position pos_;
        std::vector<std::pair<ItemId, LootInfo>> collected_items_;
        int bag_capacity_ = 0;
        double pickup_length_ = 0.6;
        int score_ = 0;
    };

}