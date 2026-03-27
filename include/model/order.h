#pragma once

#include "common/enums.h"
#include "common/types.h"
#include "model/requests.h"

namespace matching_engine::model {

// Internal order object owned/managed by engine/book.
struct Order {
    OrderId order_id {kInvalidOrderId};
    SymbolId symbol_id {kInvalidSymbolId};

    Side side {Side::Buy};
    OrderType type {OrderType::Limit};

    Price price {kInvalidPrice};

    // Original requested quantity.
    Quantity quantity {kInvalidQuantity};

    // Remaining open quantity.
    Quantity leaves_quantity {kInvalidQuantity};

    // Monotonic sequence assigned by engine/book.
    // This is the real source of strict time priority in V1.
    Sequence sequence {kInvalidSequence};

    // Optional caller timestamp for observability/debug.
    Timestamp created_at {kInvalidTimestamp};
    Timestamp updated_at {kInvalidTimestamp};

    OrderStatus status {OrderStatus::New};

    Order* prev_in_level {nullptr};
    Order* next_in_level {nullptr};

    [[nodiscard]] Quantity filled_quantity() const noexcept {
        return quantity - leaves_quantity;
    }

    [[nodiscard]] bool is_active() const noexcept {
        return status == OrderStatus::New
            || status == OrderStatus::Resting
            || status == OrderStatus::PartiallyFilled;
    }

    [[nodiscard]] bool is_terminal() const noexcept {
        return !is_active();
    }

    [[nodiscard]] bool is_filled() const noexcept {
        return leaves_quantity == 0;
    }

    [[nodiscard]] bool is_resting() const noexcept {
        return status == OrderStatus::Resting
            || status == OrderStatus::PartiallyFilled;
    }

    [[nodiscard]] bool is_detached_from_level() const noexcept {
        return prev_in_level == nullptr && next_in_level == nullptr;
    }

    void reset_level_links() noexcept {
        prev_in_level = nullptr;
        next_in_level = nullptr;
    }

    static Order from_request(const NewOrderRequest& request, Sequence seq) noexcept {
        Order order;
        order.order_id = request.order_id;
        order.symbol_id = request.symbol_id;
        order.side = request.side;
        order.type = request.type;
        order.price = request.price;
        order.quantity = request.quantity;
        order.leaves_quantity = request.quantity;
        order.sequence = seq;
        order.created_at = request.timestamp;
        order.updated_at = request.timestamp;
        order.status = OrderStatus::New;
        return order;
    }
};

} // namespace matching_engine::model