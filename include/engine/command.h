#pragma once

#include <cstdint>

#include "common/enums.h"
#include "common/types.h"
#include "model/requests.h"

namespace matching_engine::engine {

enum class CommandType : std::uint8_t {
    NewOrder = 0,
    Cancel   = 1,
    Replace  = 2
};

// Fixed-size transport message for low-latency ingress.
//
// Design goals:
// - fixed-size
// - no dynamic memory
// - trivially copyable friendly
// - transport-layer object, not business-layer request object
//
// Semantics:
// - The valid interpretation depends on `type`.
// - Requests are reconstructed from this fixed command message.
//
struct Command {
    CommandType type {CommandType::NewOrder};

    SymbolId symbol_id {kInvalidSymbolId};
    Timestamp timestamp {kInvalidTimestamp};

    // Generic id fields:
    // - NewOrder: order_id = new order id
    // - Cancel:   order_id = target order id
    // - Replace:  order_id = old order id, order_id_2 = new order id
    OrderId order_id {kInvalidOrderId};
    OrderId order_id_2 {kInvalidOrderId};

    // Used by NewOrder
    Side side {Side::Buy};
    OrderType order_type {OrderType::Limit};

    // Used by:
    // - NewOrder: price / quantity
    // - Replace:  new_price / new_quantity
    Price price {kInvalidPrice};
    Quantity quantity {kInvalidQuantity};

    [[nodiscard]] model::NewOrderRequest to_new_order_request() const noexcept {
        model::NewOrderRequest req;
        req.order_id = order_id;
        req.symbol_id = symbol_id;
        req.side = side;
        req.type = order_type;
        req.price = price;
        req.quantity = quantity;
        req.timestamp = timestamp;
        return req;
    }

    [[nodiscard]] model::CancelRequest to_cancel_request() const noexcept {
        model::CancelRequest req;
        req.symbol_id = symbol_id;
        req.order_id = order_id;
        req.timestamp = timestamp;
        return req;
    }

    [[nodiscard]] model::ReplaceRequest to_replace_request() const noexcept {
        model::ReplaceRequest req;
        req.symbol_id = symbol_id;
        req.old_order_id = order_id;
        req.new_order_id = order_id_2;
        req.new_price = price;
        req.new_quantity = quantity;
        req.timestamp = timestamp;
        return req;
    }

    static Command make_new_order(const model::NewOrderRequest& req) noexcept {
        Command cmd;
        cmd.type = CommandType::NewOrder;
        cmd.symbol_id = req.symbol_id;
        cmd.timestamp = req.timestamp;
        cmd.order_id = req.order_id;
        cmd.side = req.side;
        cmd.order_type = req.type;
        cmd.price = req.price;
        cmd.quantity = req.quantity;
        return cmd;
    }

    static Command make_cancel(const model::CancelRequest& req) noexcept {
        Command cmd;
        cmd.type = CommandType::Cancel;
        cmd.symbol_id = req.symbol_id;
        cmd.timestamp = req.timestamp;
        cmd.order_id = req.order_id;
        return cmd;
    }

    static Command make_replace(const model::ReplaceRequest& req) noexcept {
        Command cmd;
        cmd.type = CommandType::Replace;
        cmd.symbol_id = req.symbol_id;
        cmd.timestamp = req.timestamp;
        cmd.order_id = req.old_order_id;
        cmd.order_id_2 = req.new_order_id;
        cmd.price = req.new_price;
        cmd.quantity = req.new_quantity;
        return cmd;
    }
};

static_assert(std::is_trivially_copyable_v<Command>,
              "Command should remain trivially copyable for low-latency queue transport.");

} // namespace matching_engine::engine