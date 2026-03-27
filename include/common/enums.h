#pragma once

#include <cstdint>

namespace matching_engine {

// ------------------------------
// Side
// ------------------------------
//
// Buy  = bid side
// Sell = ask side
//
enum class Side : std::uint8_t {
    Buy  = 0,
    Sell = 1
};

// ------------------------------
// OrderType
// ------------------------------
//
// V1 supports only:
// - Limit
// - Market
//
enum class OrderType : std::uint8_t {
    Limit  = 0,
    Market = 1
};

// ------------------------------
// OrderStatus
// ------------------------------
//
// Lifecycle in V1:
//
// New               : accepted by engine, not yet resting
// Resting           : currently on book
// PartiallyFilled   : partially traded, still alive
// Filled            : fully traded, terminal
// Cancelled         : cancelled, terminal
// Rejected          : rejected by validation, terminal
//
enum class OrderStatus : std::uint8_t {
    New             = 0,
    Resting         = 1,
    PartiallyFilled = 2,
    Filled          = 3,
    Cancelled       = 4,
    Rejected        = 5
};

// ------------------------------
// EventType
// ------------------------------
//
// External observable events produced by engine.
//
enum class EventType : std::uint8_t {
    Ack           = 0,
    Reject        = 1,
    PartialFill   = 2,
    Fill          = 3,
    CancelAck     = 4,
    CancelReject  = 5,
    ReplaceAck    = 6,
    ReplaceReject = 7
};

// ------------------------------
// RejectReason
// ------------------------------
//
// Reason codes for reject-style events.
//
// Keep values stable once exposed externally.
//
enum class RejectReason : std::uint8_t {
    None                = 0,

    // New order validation
    DuplicateOrderId    = 1,
    UnknownSymbol       = 2,
    InvalidPrice        = 3,
    InvalidQuantity     = 4,

    // Cancel / replace validation
    OrderNotFound       = 5,
    OrderAlreadyClosed  = 6,

    // Generic / future-safe bucket
    InvalidRequest      = 7
};

// ------------------------------
// Utility helpers
// ------------------------------

[[nodiscard]] inline constexpr bool is_buy(Side side) noexcept {
    return side == Side::Buy;
}

[[nodiscard]] inline constexpr bool is_sell(Side side) noexcept {
    return side == Side::Sell;
}

[[nodiscard]] inline constexpr bool is_limit(OrderType type) noexcept {
    return type == OrderType::Limit;
}

[[nodiscard]] inline constexpr bool is_market(OrderType type) noexcept {
    return type == OrderType::Market;
}

[[nodiscard]] inline constexpr bool is_terminal(OrderStatus status) noexcept {
    return status == OrderStatus::Filled
        || status == OrderStatus::Cancelled
        || status == OrderStatus::Rejected;
}

[[nodiscard]] inline constexpr bool is_active(OrderStatus status) noexcept {
    return !is_terminal(status);
}
}