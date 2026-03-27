#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

#include "book/price_level.h"
#include "common/enums.h"
#include "common/types.h"
#include "model/event.h"
#include "model/execution_report.h"
#include "model/order.h"
#include "model/requests.h"
#include "model/trade.h"

namespace matching_engine::book {

class OrderBook {
public:
    explicit OrderBook(SymbolId symbol_id) noexcept
        : symbol_id_(symbol_id) {
    }

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    ~OrderBook() = default;

public:
    // ------------------------------
    // Basic book identity / state
    // ------------------------------
    [[nodiscard]] SymbolId symbol_id() const noexcept {
        return symbol_id_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return bids_.empty() && asks_.empty();
    }

    [[nodiscard]] bool has_order(OrderId order_id) const noexcept {
        return order_index_.find(order_id) != order_index_.end();
    }

    [[nodiscard]] std::size_t order_count() const noexcept {
        return orders_.size();
    }

    [[nodiscard]] std::size_t bid_level_count() const noexcept {
        return bids_.size();
    }

    [[nodiscard]] std::size_t ask_level_count() const noexcept {
        return asks_.size();
    }

    [[nodiscard]] std::optional<Price> best_bid() const noexcept {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.begin()->first;
    }

    [[nodiscard]] std::optional<Price> best_ask() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }

public:
    // ------------------------------
    // External API
    // ------------------------------
    model::ExecutionReport submit_order(const model::NewOrderRequest& request);
    model::ExecutionReport cancel_order(const model::CancelRequest& request);
    model::ExecutionReport replace_order(const model::ReplaceRequest& request);

private:
    model::ExecutionReport submit_new_order_internal(const model::NewOrderRequest& request,
                                                    model::Order** created_order = nullptr);

public:
    // ------------------------------
    // Debug / test helpers
    // ------------------------------
    [[nodiscard]] const model::Order* find_order(OrderId order_id) const noexcept {
        auto it = order_index_.find(order_id);
        if (it == order_index_.end()) {
            return nullptr;
        }
        return it->second.order_ptr;
    }

    [[nodiscard]] std::size_t total_resting_orders() const noexcept;
    [[nodiscard]] std::size_t total_price_levels() const noexcept;

private:
    using BidLevels = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskLevels = std::map<Price, PriceLevel, std::less<Price>>;
    using OwnedOrders = std::unordered_map<OrderId, std::unique_ptr<model::Order>>;

    struct OrderHandle {
        Side side {Side::Buy};
        Price price {kInvalidPrice};
        model::Order* order_ptr {nullptr};
    };

    using OrderIndex = std::unordered_map<OrderId, OrderHandle>;

private:
    SymbolId symbol_id_ {kInvalidSymbolId};

    // Strict ordering sources
    Sequence next_order_sequence_ {1};
    Sequence next_event_sequence_ {1};
    TradeId next_trade_id_ {1};

    // Order book state
    BidLevels bids_ {};
    AskLevels asks_ {};

    // Ownership and fast lookup
    OwnedOrders orders_ {};
    OrderIndex order_index_ {};

private:
    // ------------------------------
    // Validation
    // ------------------------------
    [[nodiscard]] bool validate_new_order(const model::NewOrderRequest& request,
                                          RejectReason& reason) const noexcept;

    [[nodiscard]] bool validate_cancel(const model::CancelRequest& request,
                                       RejectReason& reason) const noexcept;

    [[nodiscard]] bool validate_replace(const model::ReplaceRequest& request,
                                        RejectReason& reason) const noexcept;

private:
    // ------------------------------
    // Object creation
    // ------------------------------
    [[nodiscard]] model::Order make_order(const model::NewOrderRequest& request) noexcept;

    [[nodiscard]] model::Trade make_trade(const model::Order& incoming,
                                          const model::Order& resting,
                                          Quantity executed_quantity,
                                          Price executed_price,
                                          Timestamp timestamp) noexcept;

private:
    // ------------------------------
    // Event creation
    // ------------------------------
    [[nodiscard]] model::Event make_ack_event(const model::Order& order) noexcept;
    [[nodiscard]] model::Event make_reject_event(OrderId order_id,
                                                 RejectReason reason,
                                                 Timestamp timestamp) noexcept;

    [[nodiscard]] model::Event make_partial_fill_event(const model::Order& order) noexcept;
    [[nodiscard]] model::Event make_fill_event(const model::Order& order) noexcept;

    [[nodiscard]] model::Event make_cancel_ack_event(const model::Order& order,
                                                     Timestamp timestamp) noexcept;

    [[nodiscard]] model::Event make_cancel_reject_event(OrderId order_id,
                                                        RejectReason reason,
                                                        Timestamp timestamp) noexcept;

    [[nodiscard]] model::Event make_replace_ack_event(OrderId old_order_id,
                                                      const model::Order& new_order,
                                                      Timestamp timestamp) noexcept;

    [[nodiscard]] model::Event make_replace_reject_event(OrderId old_order_id,
                                                         RejectReason reason,
                                                         Timestamp timestamp) noexcept;

private:
    // ------------------------------
    // Sequence / id helpers
    // ------------------------------
    [[nodiscard]] Sequence next_order_sequence() noexcept {
        return next_order_sequence_++;
    }

    [[nodiscard]] Sequence next_event_sequence() noexcept {
        return next_event_sequence_++;
    }

    [[nodiscard]] TradeId next_trade_id() noexcept {
        return next_trade_id_++;
    }

private:
    // ------------------------------
    // Level access helpers
    // ------------------------------
    [[nodiscard]] BidLevels& bid_levels() noexcept {
        return bids_;
    }

    [[nodiscard]] AskLevels& ask_levels() noexcept {
        return asks_;
    }

    [[nodiscard]] const BidLevels& bid_levels() const noexcept {
        return bids_;
    }

    [[nodiscard]] const AskLevels& ask_levels() const noexcept {
        return asks_;
    }

    [[nodiscard]] PriceLevel* find_level_mutable(Side side, Price price) noexcept;
    [[nodiscard]] const PriceLevel* find_level(Side side, Price price) const noexcept;

    [[nodiscard]] PriceLevel& get_or_create_level(Side side, Price price);
    void cleanup_empty_price_level(Side side, Price price) noexcept;

private:
    // ------------------------------
    // Order ownership / index helpers
    // ------------------------------
    [[nodiscard]] model::Order* store_order(model::Order&& order);
    void erase_order(OrderId order_id) noexcept;

    void index_order(model::Order* order) noexcept;
    void unindex_order(OrderId order_id) noexcept;

private:
    // ------------------------------
    // Resting order management
    // ------------------------------
    void add_resting_order(model::Order* order);
    void remove_resting_order(model::Order* order) noexcept;

private:
    // ------------------------------
    // Matching core
    // ------------------------------
    model::ExecutionReport submit_limit_order(model::Order* incoming);
    model::ExecutionReport submit_market_order(model::Order* incoming);

    model::ExecutionReport match_buy_order(model::Order* incoming);
    model::ExecutionReport match_sell_order(model::Order* incoming);

private:
    // ------------------------------
    // Matching predicates / helpers
    // ------------------------------
    [[nodiscard]] bool can_match(const model::Order& incoming,
                                 const model::Order& resting) const noexcept;

    [[nodiscard]] Price execution_price(const model::Order& incoming,
                                        const model::Order& resting) const noexcept;

    [[nodiscard]] Quantity execution_quantity(const model::Order& incoming,
                                              const model::Order& resting) const noexcept;

private:
    // ------------------------------
    // Order state helpers
    // ------------------------------
    void mark_resting(model::Order& order, Timestamp timestamp) noexcept;
    void mark_partially_filled(model::Order& order, Timestamp timestamp) noexcept;
    void mark_filled(model::Order& order, Timestamp timestamp) noexcept;
    void mark_cancelled(model::Order& order, Timestamp timestamp) noexcept;
};
} // namespace matching_engine::book