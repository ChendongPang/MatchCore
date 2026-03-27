#include "book/order_book.h"

#include <algorithm>
#include <utility>

namespace matching_engine::book {

using matching_engine::model::Event;
using matching_engine::model::ExecutionReport;
using matching_engine::model::Order;
using matching_engine::model::Trade;

// ------------------------------
// Level helpers
// ------------------------------

PriceLevel* OrderBook::find_level_mutable(Side side, Price price) noexcept {
    if (is_buy(side)) {
        auto it = bids_.find(price);
        return (it == bids_.end()) ? nullptr : &it->second;
    }

    auto it = asks_.find(price);
    return (it == asks_.end()) ? nullptr : &it->second;
}

const PriceLevel* OrderBook::find_level(Side side, Price price) const noexcept {
    if (is_buy(side)) {
        auto it = bids_.find(price);
        return (it == bids_.end()) ? nullptr : &it->second;
    }

    auto it = asks_.find(price);
    return (it == asks_.end()) ? nullptr : &it->second;
}

PriceLevel& OrderBook::get_or_create_level(Side side, Price price) {
    if (is_buy(side)) {
        auto [it, inserted] = bids_.try_emplace(price, PriceLevel(price));
        (void)inserted;
        return it->second;
    }

    auto [it, inserted] = asks_.try_emplace(price, PriceLevel(price));
    (void)inserted;
    return it->second;
}

void OrderBook::cleanup_empty_price_level(Side side, Price price) noexcept {
    if (is_buy(side)) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second.empty()) {
            bids_.erase(it);
        }
        return;
    }

    auto it = asks_.find(price);
    if (it != asks_.end() && it->second.empty()) {
        asks_.erase(it);
    }
}

// ------------------------------
// Ownership / index helpers
// ------------------------------

Order* OrderBook::store_order(Order&& order) {
    const OrderId order_id = order.order_id;
    auto [it, inserted] = orders_.try_emplace(order_id, std::make_unique<Order>(std::move(order)));
    if (!inserted) {
        return nullptr;
    }
    return it->second.get();
}

void OrderBook::erase_order(OrderId order_id) noexcept {
    orders_.erase(order_id);
}

void OrderBook::index_order(Order* order) noexcept {
    if (order == nullptr) {
        return;
    }

    order_index_[order->order_id] = OrderHandle{
        .side = order->side,
        .price = order->price,
        .order_ptr = order,
    };
}

void OrderBook::unindex_order(OrderId order_id) noexcept {
    order_index_.erase(order_id);
}

// ------------------------------
// State helpers
// ------------------------------

void OrderBook::mark_resting(Order& order, Timestamp timestamp) noexcept {
    order.status = OrderStatus::Resting;
    order.updated_at = timestamp;
}

void OrderBook::mark_partially_filled(Order& order, Timestamp timestamp) noexcept {
    order.status = OrderStatus::PartiallyFilled;
    order.updated_at = timestamp;
}

void OrderBook::mark_filled(Order& order, Timestamp timestamp) noexcept {
    order.status = OrderStatus::Filled;
    order.updated_at = timestamp;
}

void OrderBook::mark_cancelled(Order& order, Timestamp timestamp) noexcept {
    order.status = OrderStatus::Cancelled;
    order.updated_at = timestamp;
}

// ------------------------------
// Validation
// ------------------------------

bool OrderBook::validate_new_order(const model::NewOrderRequest& request,
                                   RejectReason& reason) const noexcept {
    if (request.symbol_id != symbol_id_) {
        reason = RejectReason::UnknownSymbol;
        return false;
    }

    if (!is_valid_order_id(request.order_id)) {
        reason = RejectReason::InvalidRequest;
        return false;
    }

    if (has_order(request.order_id)) {
        reason = RejectReason::DuplicateOrderId;
        return false;
    }

    if (!is_valid_quantity(request.quantity)) {
        reason = RejectReason::InvalidQuantity;
        return false;
    }

    if (is_limit(request.type) && !is_valid_price(request.price)) {
        reason = RejectReason::InvalidPrice;
        return false;
    }

    // Market order in V1 may carry 0 / sentinel price.
    if (is_market(request.type)) {
        reason = RejectReason::None;
        return true;
    }

    reason = RejectReason::None;
    return true;
}

bool OrderBook::validate_cancel(const model::CancelRequest& request,
                                RejectReason& reason) const noexcept {
    if (request.symbol_id != symbol_id_) {
        reason = RejectReason::UnknownSymbol;
        return false;
    }

    auto it = order_index_.find(request.order_id);
    if (it == order_index_.end() || it->second.order_ptr == nullptr) {
        reason = RejectReason::OrderNotFound;
        return false;
    }

    if (it->second.order_ptr->is_terminal()) {
        reason = RejectReason::OrderAlreadyClosed;
        return false;
    }

    reason = RejectReason::None;
    return true;
}

bool OrderBook::validate_replace(const model::ReplaceRequest& request,
                                 RejectReason& reason) const noexcept {
    if (request.symbol_id != symbol_id_) {
        reason = RejectReason::UnknownSymbol;
        return false;
    }

    auto old_it = order_index_.find(request.old_order_id);
    if (old_it == order_index_.end() || old_it->second.order_ptr == nullptr) {
        reason = RejectReason::OrderNotFound;
        return false;
    }

    if (old_it->second.order_ptr->is_terminal()) {
        reason = RejectReason::OrderAlreadyClosed;
        return false;
    }

    if (!is_valid_order_id(request.new_order_id) || has_order(request.new_order_id)) {
        reason = RejectReason::DuplicateOrderId;
        return false;
    }

    if (!is_valid_quantity(request.new_quantity)) {
        reason = RejectReason::InvalidQuantity;
        return false;
    }

    if (!is_valid_price(request.new_price)) {
        reason = RejectReason::InvalidPrice;
        return false;
    }

    reason = RejectReason::None;
    return true;
}

// ------------------------------
// Object creation
// ------------------------------

Order OrderBook::make_order(const model::NewOrderRequest& request) noexcept {
    return Order::from_request(request, next_order_sequence());
}

Trade OrderBook::make_trade(const Order& incoming,
                            const Order& resting,
                            Quantity executed_quantity,
                            Price executed_price,
                            Timestamp timestamp) noexcept {
    Trade trade;
    trade.trade_id = next_trade_id();
    trade.symbol_id = symbol_id_;

    if (is_buy(incoming.side)) {
        trade.buy_order_id = incoming.order_id;
        trade.sell_order_id = resting.order_id;
    } else {
        trade.buy_order_id = resting.order_id;
        trade.sell_order_id = incoming.order_id;
    }

    trade.trade_price = executed_price;
    trade.trade_quantity = executed_quantity;
    trade.sequence = next_event_sequence();
    trade.timestamp = timestamp;
    return trade;
}

// ------------------------------
// Event creation
// ------------------------------

Event OrderBook::make_ack_event(const Order& order) noexcept {
    Event e = Event::make_ack(order);
    e.sequence = next_event_sequence();
    return e;
}

Event OrderBook::make_reject_event(OrderId order_id,
                                   RejectReason reason,
                                   Timestamp timestamp) noexcept {
    return Event::make_reject(order_id,
                              symbol_id_,
                              reason,
                              next_event_sequence(),
                              timestamp);
}

Event OrderBook::make_partial_fill_event(const Order& order) noexcept {
    Event e = Event::make_partial_fill(order);
    e.sequence = next_event_sequence();
    return e;
}

Event OrderBook::make_fill_event(const Order& order) noexcept {
    Event e = Event::make_fill(order);
    e.sequence = next_event_sequence();
    return e;
}

Event OrderBook::make_cancel_ack_event(const Order& order,
                                       Timestamp timestamp) noexcept {
    return Event::make_cancel_ack(order,
                                  next_event_sequence(),
                                  timestamp);
}

Event OrderBook::make_cancel_reject_event(OrderId order_id,
                                          RejectReason reason,
                                          Timestamp timestamp) noexcept {
    return Event::make_cancel_reject(order_id,
                                     symbol_id_,
                                     reason,
                                     next_event_sequence(),
                                     timestamp);
}

Event OrderBook::make_replace_ack_event(OrderId old_order_id,
                                        const Order& new_order,
                                        Timestamp timestamp) noexcept {
    return Event::make_replace_ack(old_order_id,
                                   new_order,
                                   next_event_sequence(),
                                   timestamp);
}

Event OrderBook::make_replace_reject_event(OrderId old_order_id,
                                           RejectReason reason,
                                           Timestamp timestamp) noexcept {
    return Event::make_replace_reject(old_order_id,
                                      symbol_id_,
                                      reason,
                                      next_event_sequence(),
                                      timestamp);
}

// ------------------------------
// Resting order management
// ------------------------------

void OrderBook::add_resting_order(Order* order) {
    if (order == nullptr) {
        return;
    }

    PriceLevel& level = get_or_create_level(order->side, order->price);
    level.push_back(order);
    index_order(order);
}

void OrderBook::remove_resting_order(Order* order) noexcept {
    if (order == nullptr) {
        return;
    }

    PriceLevel* level = find_level_mutable(order->side, order->price);
    if (level == nullptr) {
        return;
    }

    level->erase(order);
    cleanup_empty_price_level(order->side, order->price);
}

// ------------------------------
// Matching helpers
// ------------------------------

bool OrderBook::can_match(const Order& incoming,
                          const Order& resting) const noexcept {
    if (incoming.symbol_id != resting.symbol_id) {
        return false;
    }

    if (incoming.side == resting.side) {
        return false;
    }

    if (incoming.leaves_quantity == 0 || resting.leaves_quantity == 0) {
        return false;
    }

    if (is_market(incoming.type)) {
        return true;
    }

    if (is_buy(incoming.side)) {
        return incoming.price >= resting.price;
    }

    return incoming.price <= resting.price;
}

Price OrderBook::execution_price(const Order& incoming,
                                 const Order& resting) const noexcept {
    (void)incoming;
    // Standard V1 rule: execution price = resting order price.
    return resting.price;
}

Quantity OrderBook::execution_quantity(const Order& incoming,
                                       const Order& resting) const noexcept {
    return std::min(incoming.leaves_quantity, resting.leaves_quantity);
}

// ------------------------------
// Matching core
// ------------------------------

ExecutionReport OrderBook::match_buy_order(Order* incoming) {
    ExecutionReport report;
    if (incoming == nullptr) {
        return report;
    }

    while (incoming->leaves_quantity > 0 && !asks_.empty()) {
        auto best_ask_it = asks_.begin();
        PriceLevel& level = best_ask_it->second;

        Order* resting = level.front();
        if (resting == nullptr) {
            cleanup_empty_price_level(Side::Sell, best_ask_it->first);
            continue;
        }

        if (!can_match(*incoming, *resting)) {
            break;
        }

        const Timestamp ts = incoming->updated_at;
        const Quantity exec_qty = execution_quantity(*incoming, *resting);
        const Price exec_price = execution_price(*incoming, *resting);

        report.add_trade(make_trade(*incoming, *resting, exec_qty, exec_price, ts));

        incoming->leaves_quantity -= exec_qty;
        resting->leaves_quantity -= exec_qty;

        if (incoming->leaves_quantity == 0) {
            mark_filled(*incoming, ts);
            report.add_event(make_fill_event(*incoming));
        } else {
            mark_partially_filled(*incoming, ts);
            report.add_event(make_partial_fill_event(*incoming));
        }

        if (resting->leaves_quantity == 0) {
            mark_filled(*resting, ts);
            report.add_event(make_fill_event(*resting));

            const OrderId resting_id = resting->order_id;
            level.erase(resting);
            unindex_order(resting_id);
            erase_order(resting_id);
            cleanup_empty_price_level(Side::Sell, best_ask_it->first);
        } else {
            mark_partially_filled(*resting, ts);
            report.add_event(make_partial_fill_event(*resting));
        }
    }

    return report;
}

ExecutionReport OrderBook::match_sell_order(Order* incoming) {
    ExecutionReport report;
    if (incoming == nullptr) {
        return report;
    }

    while (incoming->leaves_quantity > 0 && !bids_.empty()) {
        auto best_bid_it = bids_.begin();
        PriceLevel& level = best_bid_it->second;

        Order* resting = level.front();
        if (resting == nullptr) {
            cleanup_empty_price_level(Side::Buy, best_bid_it->first);
            continue;
        }

        if (!can_match(*incoming, *resting)) {
            break;
        }

        const Timestamp ts = incoming->updated_at;
        const Quantity exec_qty = execution_quantity(*incoming, *resting);
        const Price exec_price = execution_price(*incoming, *resting);

        report.add_trade(make_trade(*incoming, *resting, exec_qty, exec_price, ts));

        incoming->leaves_quantity -= exec_qty;
        resting->leaves_quantity -= exec_qty;

        if (incoming->leaves_quantity == 0) {
            mark_filled(*incoming, ts);
            report.add_event(make_fill_event(*incoming));
        } else {
            mark_partially_filled(*incoming, ts);
            report.add_event(make_partial_fill_event(*incoming));
        }

        if (resting->leaves_quantity == 0) {
            mark_filled(*resting, ts);
            report.add_event(make_fill_event(*resting));

            const OrderId resting_id = resting->order_id;
            level.erase(resting);
            unindex_order(resting_id);
            erase_order(resting_id);
            cleanup_empty_price_level(Side::Buy, best_bid_it->first);
        } else {
            mark_partially_filled(*resting, ts);
            report.add_event(make_partial_fill_event(*resting));
        }
    }

    return report;
}

// ------------------------------
// Submit helpers
// ------------------------------

ExecutionReport OrderBook::submit_limit_order(Order* incoming) {
    ExecutionReport report;
    if (incoming == nullptr) {
        return report;
    }

    if (is_buy(incoming->side)) {
        report.append(match_buy_order(incoming));
    } else {
        report.append(match_sell_order(incoming));
    }

    if (incoming->leaves_quantity > 0) {
        mark_resting(*incoming, incoming->updated_at);
        add_resting_order(incoming);
    } else {
        // fully filled immediately, remove from ownership store
        const OrderId order_id = incoming->order_id;
        erase_order(order_id);
    }

    return report;
}

ExecutionReport OrderBook::submit_market_order(Order* incoming) {
    ExecutionReport report;
    if (incoming == nullptr) {
        return report;
    }

    if (is_buy(incoming->side)) {
        report.append(match_buy_order(incoming));
    } else {
        report.append(match_sell_order(incoming));
    }

    // Market order never rests on book in V1.
    // Remaining quantity, if any, is simply discarded.
    const OrderId order_id = incoming->order_id;
    erase_order(order_id);

    return report;
}

ExecutionReport OrderBook::submit_new_order_internal(const model::NewOrderRequest& request,
                                                     model::Order** created_order) {
    ExecutionReport report;

    if (created_order != nullptr) {
        *created_order = nullptr;
    }

    RejectReason reason = RejectReason::None;
    if (!validate_new_order(request, reason)) {
        report.add_event(make_reject_event(request.order_id, reason, request.timestamp));
        return report;
    }

    Order order = make_order(request);
    Order* incoming = store_order(std::move(order));
    if (incoming == nullptr) {
        report.add_event(make_reject_event(request.order_id,
                                           RejectReason::DuplicateOrderId,
                                           request.timestamp));
        return report;
    }

    if (created_order != nullptr) {
        *created_order = incoming;
    }

    report.add_event(make_ack_event(*incoming));

    if (is_limit(incoming->type)) {
        report.append(submit_limit_order(incoming));
    } else {
        report.append(submit_market_order(incoming));
    }

    return report;
}
// ------------------------------
// Public API
// ------------------------------

ExecutionReport OrderBook::submit_order(const model::NewOrderRequest& request) {
    return submit_new_order_internal(request, nullptr);
}

ExecutionReport OrderBook::cancel_order(const model::CancelRequest& request) {
    ExecutionReport report;

    RejectReason reason = RejectReason::None;
    if (!validate_cancel(request, reason)) {
        report.add_event(make_cancel_reject_event(request.order_id, reason, request.timestamp));
        return report;
    }

    auto idx_it = order_index_.find(request.order_id);
    Order* order = idx_it->second.order_ptr;
    if (order == nullptr) {
        report.add_event(make_cancel_reject_event(request.order_id,
                                                  RejectReason::OrderNotFound,
                                                  request.timestamp));
        return report;
    }

    remove_resting_order(order);
    mark_cancelled(*order, request.timestamp);

    report.add_event(make_cancel_ack_event(*order, request.timestamp));

    const OrderId order_id = order->order_id;
    unindex_order(order_id);
    erase_order(order_id);

    return report;
}

ExecutionReport OrderBook::replace_order(const model::ReplaceRequest& request) {
    ExecutionReport report;

    RejectReason reason = RejectReason::None;
    if (!validate_replace(request, reason)) {
        report.add_event(make_replace_reject_event(request.old_order_id, reason, request.timestamp));
        return report;
    }

    auto idx_it = order_index_.find(request.old_order_id);
    Order* old_order = idx_it->second.order_ptr;
    if (old_order == nullptr) {
        report.add_event(make_replace_reject_event(request.old_order_id,
                                                   RejectReason::OrderNotFound,
                                                   request.timestamp));
        return report;
    }

    // Preserve old side/type in V1.
    const Side side = old_order->side;
    const OrderType type = old_order->type;

    // 1) cancel old
    {
        model::CancelRequest cancel_req;
        cancel_req.symbol_id = request.symbol_id;
        cancel_req.order_id = request.old_order_id;
        cancel_req.timestamp = request.timestamp;

        report.append(cancel_order(cancel_req));
    }

    // 2) submit new and capture the real created order
    model::NewOrderRequest new_req;
    new_req.order_id = request.new_order_id;
    new_req.symbol_id = request.symbol_id;
    new_req.side = side;
    new_req.type = type;
    new_req.price = request.new_price;
    new_req.quantity = request.new_quantity;
    new_req.timestamp = request.timestamp;

    Order* new_order = nullptr;
    ExecutionReport new_report = submit_new_order_internal(new_req, &new_order);

    // If new order submission failed, convert it to ReplaceReject semantics.
    // We already cancelled the old order, so this means replace failed after cancel.
    bool new_submit_rejected = false;
    RejectReason new_reject_reason = RejectReason::None;

    for (const auto& event : new_report.events) {
        if (event.type == EventType::Reject) {
            new_submit_rejected = true;
            new_reject_reason = event.reason;
            break;
        }
    }

    if (new_submit_rejected || new_order == nullptr) {
        report.add_event(make_replace_reject_event(request.old_order_id,
                                                   new_reject_reason == RejectReason::None
                                                       ? RejectReason::InvalidRequest
                                                       : new_reject_reason,
                                                   request.timestamp));

        // Optional: do not append the raw new-order reject event if you want the
        // replace API to expose only replace semantics.
        // For now, keep the new order report as well for observability.
        report.append(std::move(new_report));
        return report;
    }

    // 3) now we can emit ReplaceAck using the real new order
    report.add_event(make_replace_ack_event(request.old_order_id, *new_order, request.timestamp));

    // 4) append actual new-order submission result
    report.append(std::move(new_report));

    return report;
}

// ------------------------------
// Stats helpers (for benchmark)
// ------------------------------

std::size_t OrderBook::total_resting_orders() const noexcept {
    return order_index_.size();
}

std::size_t OrderBook::total_price_levels() const noexcept {
    return bids_.size() + asks_.size();
}

} // namespace matching_engine::book