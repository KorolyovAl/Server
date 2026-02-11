#pragma once
 
#include <stdexcept>
#include <vector>
#include <optional>

#include "loot_struct.h"

namespace model {

class LootStore {
public:
    LootStore() {
        items_.reserve(100);
        free_ids_.reserve(100);
    }

    const LootItem& Create(const LootInfo info, const pos::Coordinate& coord) {
        ItemId id;

        // If free_ids_ contains elements, then there is a free id
        if (!free_ids_.empty()) {
            id = free_ids_.back();
            free_ids_.pop_back();
            items_[id] = LootItem {
                                    .id = id,
                                    .info = info,
                                    .coordinate = coord
                                };

            return items_[id].value();
        }

        // else create a new item with new id 
        id = static_cast<ItemId>(items_.size());
        items_.push_back(LootItem {
                                    .id = id,
                                    .info = info,
                                    .coordinate = coord
        });
        
        return items_.back().value();
    }

    void Remove(ItemId id) {
        // The id should not be more than the number of items
        if (id >= items_.size() || !items_[id].has_value()) {
            throw std::invalid_argument("incorrect item id");
        }

        // after deleting an item, its id is moved to free_ids_
        items_[id].reset();
        free_ids_.push_back(id);
    }

    void Clear() {
        items_.clear();
        free_ids_.clear();
    }

    const LootItem& RestoreItem(ItemId id, const LootInfo info, const pos::Coordinate& coord, double width) {
        if (id >= items_.size()) {
            items_.resize(id + 1);
        }
        
        items_[id] = LootItem {
                                .id = id,
                                .info = info,
                                .coordinate = coord,
                                .width = width
        };

        return items_[id].value();
    }

    void FinalizeAfterRestore() {
        free_ids_.clear();
        for (ItemId id = 0; id < items_.size(); ++id) {
            if (!items_[id].has_value()) {
                free_ids_.push_back(id);
            }
        }
    }

    const LootItem* GetItem(ItemId id) const noexcept {
        if (id >= items_.size() || !items_[id]) {
            return nullptr;
        }

        const auto& opt = items_[id];
        if (!opt.has_value()) {
            return nullptr;
        }

        return &opt.value();
    }

    std::vector<LootItem> GetAllItems() const {
        std::vector<LootItem> res;
        for (auto& item_opt : items_) {
            if (!item_opt.has_value()) {
                continue;
            }

            res.push_back(item_opt.value());
        }

        return res;
    }

    size_t GetItemNumber() const {
        return items_.size() - free_ids_.size();
    }

private:
    std::vector<std::optional<LootItem>> items_;  
    std::vector<ItemId> free_ids_;
};

} // namespace model