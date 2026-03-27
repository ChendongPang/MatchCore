#pragma once

#include <cstddef>
#include <utility>

#include "common/types.h"
#include "model/order.h"

namespace matching_engine::book {

// One price level = one price + one intrusive FIFO queue of orders.
//
// IMPORTANT:
// - PriceLevel does NOT own Order objects.
// - Order memory/lifetime must be managed by OrderBook (or another owner).
// - This container only links/unlinks Order* through embedded intrusive pointers.
class PriceLevel {
public:
    explicit PriceLevel(Price price = kInvalidPrice) noexcept
        : price_(price) {
    }

    [[nodiscard]] Price price() const noexcept {
        return price_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_ == nullptr;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] model::Order* front() noexcept {
        return head_;
    }

    [[nodiscard]] const model::Order* front() const noexcept {
        return head_;
    }

    [[nodiscard]] model::Order* back() noexcept {
        return tail_;
    }

    [[nodiscard]] const model::Order* back() const noexcept {
        return tail_;
    }

    // Append one detached order at tail.
    // FIFO semantics: older orders stay closer to head.
    void push_back(model::Order* order) noexcept {
        if (order == nullptr) {
            return;
        }

        // Always sanitize the node first. This makes the function tolerant
        // to callers that pass a previously detached node.
        order->prev_in_level = nullptr;
        order->next_in_level = nullptr;

        if (tail_ == nullptr) {
            head_ = order;
            tail_ = order;
            size_ = 1;
            return;
        }

        order->prev_in_level = tail_;
        tail_->next_in_level = order;
        tail_ = order;
        ++size_;
    }

    // Remove an order from this price level in O(1).
    // Safe to call only if the order currently belongs to this level.
    void erase(model::Order* order) noexcept {
        if (order == nullptr) {
            return;
        }

        if (order->prev_in_level != nullptr) {
            order->prev_in_level->next_in_level = order->next_in_level;
        } else {
            // order is head
            head_ = order->next_in_level;
        }

        if (order->next_in_level != nullptr) {
            order->next_in_level->prev_in_level = order->prev_in_level;
        } else {
            // order is tail
            tail_ = order->prev_in_level;
        }

        order->prev_in_level = nullptr;
        order->next_in_level = nullptr;

        if (size_ > 0) {
            --size_;
        }

        if (size_ == 0) {
            head_ = nullptr;
            tail_ = nullptr;
        }
    }

    // Remove the head order in O(1).
    void pop_front() noexcept {
        if (head_ == nullptr) {
            return;
        }
        erase(head_);
    }

    // Remove all links from the level without destroying any Order object.
    // Useful only for destructive reset scenarios.
    void clear() noexcept {
        model::Order* cur = head_;
        while (cur != nullptr) {
            model::Order* next = cur->next_in_level;
            cur->prev_in_level = nullptr;
            cur->next_in_level = nullptr;
            cur = next;
        }

        head_ = nullptr;
        tail_ = nullptr;
        size_ = 0;
    }

    // Linear search helper for debug / assertion / tests.
    [[nodiscard]] bool contains(const model::Order* order) const noexcept {
        const model::Order* cur = head_;
        while (cur != nullptr) {
            if (cur == order) {
                return true;
            }
            cur = cur->next_in_level;
        }
        return false;
    }

private:
    Price price_ {kInvalidPrice};
    model::Order* head_ {nullptr};
    model::Order* tail_ {nullptr};
    std::size_t size_ {0};
};

} // namespace matching_engine::book
