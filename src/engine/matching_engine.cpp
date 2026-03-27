#include "engine/matching_engine.h"

namespace matching_engine::engine {

using matching_engine::book::OrderBook;
using matching_engine::model::ExecutionReport;

// ------------------------------
// Symbol management
// ------------------------------

bool MatchingEngine::add_symbol(SymbolId symbol_id) {
    if (!is_valid_symbol(symbol_id)) {
        return false;
    }

    auto [it, inserted] = books_.try_emplace(symbol_id, symbol_id);
    (void)it;
    return inserted;
}

bool MatchingEngine::remove_symbol(SymbolId symbol_id) noexcept {
    return books_.erase(symbol_id) > 0;
}

bool MatchingEngine::has_symbol(SymbolId symbol_id) const noexcept {
    return books_.find(symbol_id) != books_.end();
}

std::size_t MatchingEngine::symbol_count() const noexcept {
    return books_.size();
}

// ------------------------------
// Book lookup
// ------------------------------

const OrderBook* MatchingEngine::find_order_book(SymbolId symbol_id) const noexcept {
    auto it = books_.find(symbol_id);
    if (it == books_.end()) {
        return nullptr;
    }
    return &it->second;
}

OrderBook* MatchingEngine::find_order_book_mutable(SymbolId symbol_id) noexcept {
    auto it = books_.find(symbol_id);
    if (it == books_.end()) {
        return nullptr;
    }
    return &it->second;
}

// ------------------------------
// Unknown symbol reject helpers
// ------------------------------

ExecutionReport MatchingEngine::make_unknown_symbol_reject(
    const model::NewOrderRequest& request) const noexcept {
    ExecutionReport report;
    report.add_event(model::Event::make_reject(
        request.order_id,
        request.symbol_id,
        RejectReason::UnknownSymbol,
        kInvalidSequence,
        request.timestamp));
    return report;
}

ExecutionReport MatchingEngine::make_unknown_symbol_reject(
    const model::CancelRequest& request) const noexcept {
    ExecutionReport report;
    report.add_event(model::Event::make_cancel_reject(
        request.order_id,
        request.symbol_id,
        RejectReason::UnknownSymbol,
        kInvalidSequence,
        request.timestamp));
    return report;
}

ExecutionReport MatchingEngine::make_unknown_symbol_reject(
    const model::ReplaceRequest& request) const noexcept {
    ExecutionReport report;
    report.add_event(model::Event::make_replace_reject(
        request.old_order_id,
        request.symbol_id,
        RejectReason::UnknownSymbol,
        kInvalidSequence,
        request.timestamp));
    return report;
}

// ------------------------------
// Direct business API
// ------------------------------

ExecutionReport MatchingEngine::submit_order(const model::NewOrderRequest& request) {
    OrderBook* book = find_order_book_mutable(request.symbol_id);
    if (book == nullptr) {
        return make_unknown_symbol_reject(request);
    }
    return book->submit_order(request);
}

ExecutionReport MatchingEngine::cancel_order(const model::CancelRequest& request) {
    OrderBook* book = find_order_book_mutable(request.symbol_id);
    if (book == nullptr) {
        return make_unknown_symbol_reject(request);
    }
    return book->cancel_order(request);
}

ExecutionReport MatchingEngine::replace_order(const model::ReplaceRequest& request) {
    OrderBook* book = find_order_book_mutable(request.symbol_id);
    if (book == nullptr) {
        return make_unknown_symbol_reject(request);
    }
    return book->replace_order(request);
}

// ------------------------------
// Transport command dispatch API
// ------------------------------

ExecutionReport MatchingEngine::dispatch(const Command& command) {
    switch (command.type) {
        case CommandType::NewOrder:
            return submit_order(command.to_new_order_request());

        case CommandType::Cancel:
            return cancel_order(command.to_cancel_request());

        case CommandType::Replace:
            return replace_order(command.to_replace_request());
    }

    // Defensive fallback: impossible in normal enum usage,
    // but keep a safe return path.
    ExecutionReport report;
    return report;
}

// ------------------------------
// Stats helpers (for benchmark)
// ------------------------------

std::size_t MatchingEngine::total_resting_orders(SymbolId symbol_id) const noexcept {
    const OrderBook* book = find_order_book(symbol_id);
    return book == nullptr ? 0 : book->total_resting_orders();
}

std::size_t MatchingEngine::total_price_levels(SymbolId symbol_id) const noexcept {
    const OrderBook* book = find_order_book(symbol_id);
    return book == nullptr ? 0 : book->total_price_levels();
}

} // namespace matching_engine::engine