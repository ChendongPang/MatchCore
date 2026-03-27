#pragma once

#include <cstddef>
#include <cstdint>

namespace matching_engine {
    using OrderId   = std::uint64_t;
    using TradeId   = std::uint64_t;
    using SymbolId  = std::uint64_t;
    using Price     = std::int64_t;
    using Quantity  = std::int64_t;
    using Timestamp = std::uint64_t;
    using Sequence  = std::uint64_t;

    inline constexpr Price kInvalidPrice = 0;
    inline constexpr Quantity kInvalidQuantity = 0;
    inline constexpr SymbolId kInvalidSymbolId = 0;
    inline constexpr OrderId kInvalidOrderId = 0;
    inline constexpr TradeId kInvalidTradeId = 0;
    inline constexpr Timestamp kInvalidTimestamp = 0;
    inline constexpr Sequence kInvalidSequence = 0;
    inline constexpr Price MarketOrderPrice = 0;

    [[nodiscard]] inline constexpr bool is_valid_price(Price price) noexcept {
        return price > 0;
    }

    [[nodiscard]] inline constexpr bool is_valid_quantity(Quantity qty) noexcept {
        return qty > 0;
    }

    [[nodiscard]] inline constexpr bool is_valid_symbol(SymbolId symbol_id) noexcept {
        return symbol_id != kInvalidSymbolId;
    }

    [[nodiscard]] inline constexpr bool is_valid_order_id(OrderId order_id) noexcept {
        return order_id != kInvalidOrderId;
    }

    [[nodiscard]] inline constexpr bool is_valid_trade_id(TradeId trade_id) noexcept {
        return trade_id != kInvalidTradeId;
    }
}