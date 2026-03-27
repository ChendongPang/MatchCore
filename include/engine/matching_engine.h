#pragma once

#include <cstddef>
#include <unordered_map>

#include "book/order_book.h"
#include "common/enums.h"
#include "common/types.h"
#include "engine/command.h"
#include "model/event.h"
#include "model/execution_report.h"
#include "model/requests.h"

namespace matching_engine::engine {

class MatchingEngine {
public:
    MatchingEngine() = default;

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    ~MatchingEngine() = default;

public:
    // ------------------------------
    // Symbol management
    // ------------------------------
    bool add_symbol(SymbolId symbol_id);
    bool remove_symbol(SymbolId symbol_id) noexcept;

    [[nodiscard]] bool has_symbol(SymbolId symbol_id) const noexcept;
    [[nodiscard]] std::size_t symbol_count() const noexcept;

public:
    // ------------------------------
    // Direct business API
    // ------------------------------
    model::ExecutionReport submit_order(const model::NewOrderRequest& request);
    model::ExecutionReport cancel_order(const model::CancelRequest& request);
    model::ExecutionReport replace_order(const model::ReplaceRequest& request);

public:
    // ------------------------------
    // Transport command dispatch API
    // ------------------------------
    model::ExecutionReport dispatch(const Command& command);

public:
    // ------------------------------
    // Book lookup
    // ------------------------------
    [[nodiscard]] const book::OrderBook* find_order_book(SymbolId symbol_id) const noexcept;
    [[nodiscard]] book::OrderBook* find_order_book_mutable(SymbolId symbol_id) noexcept;

private:
    [[nodiscard]] model::ExecutionReport make_unknown_symbol_reject(
        const model::NewOrderRequest& request) const noexcept;

    [[nodiscard]] model::ExecutionReport make_unknown_symbol_reject(
        const model::CancelRequest& request) const noexcept;

    [[nodiscard]] model::ExecutionReport make_unknown_symbol_reject(
        const model::ReplaceRequest& request) const noexcept;

private:
    std::unordered_map<SymbolId, book::OrderBook> books_ {};

public:
    [[nodiscard]] std::size_t total_resting_orders(SymbolId symbol_id) const noexcept;
    [[nodiscard]] std::size_t total_price_levels(SymbolId symbol_id) const noexcept;
};

} // namespace matching_engine::engine