#pragma once

#include <iterator>
#include <utility>
#include <vector>

#include "model/event.h"
#include "model/trade.h"

namespace matching_engine::model {

struct ExecutionReport {
    std::vector<Event> events {};
    std::vector<Trade> trades {};

    [[nodiscard]] bool empty() const noexcept {
        return events.empty() && trades.empty();
    }

    [[nodiscard]] std::size_t event_count() const noexcept {
        return events.size();
    }

    [[nodiscard]] std::size_t trade_count() const noexcept {
        return trades.size();
    }

    void add_event(const Event& event) {
        events.push_back(event);
    }

    void add_event(Event&& event) {
        events.push_back(std::move(event));
    }

    void add_trade(const Trade& trade) {
        trades.push_back(trade);
    }

    void add_trade(Trade&& trade) {
        trades.push_back(std::move(trade));
    }

    void reserve(std::size_t event_cap, std::size_t trade_cap) {
        events.reserve(event_cap);
        trades.reserve(trade_cap);
    }

    void append(ExecutionReport&& other) {
        events.insert(events.end(),
                      std::make_move_iterator(other.events.begin()),
                      std::make_move_iterator(other.events.end()));

        trades.insert(trades.end(),
                      std::make_move_iterator(other.trades.begin()),
                      std::make_move_iterator(other.trades.end()));
    }
};

} // namespace matching_engine::model