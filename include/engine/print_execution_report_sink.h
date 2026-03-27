#pragma once

#include <iostream>
#include <ostream>

#include "engine/matching_loop.h"
#include "model/event.h"
#include "model/execution_report.h"
#include "model/trade.h"

namespace matching_engine::engine {

class PrintExecutionReportSink final : public IExecutionReportSink {
public:
    explicit PrintExecutionReportSink(std::ostream& os = std::cout) noexcept
        : os_(os) {
    }

    void on_execution_report(const model::ExecutionReport& report) override {
        if (report.empty()) {
            return;
        }

        os_ << "=== ExecutionReport ===\n";

        if (!report.events.empty()) {
            os_ << "events:\n";
            for (const auto& event : report.events) {
                print_event(event);
            }
        }

        if (!report.trades.empty()) {
            os_ << "trades:\n";
            for (const auto& trade : report.trades) {
                print_trade(trade);
            }
        }

        os_ << '\n';
    }

private:
    static const char* to_string(EventType type) noexcept {
        switch (type) {
            case EventType::Ack: return "Ack";
            case EventType::Reject: return "Reject";
            case EventType::PartialFill: return "PartialFill";
            case EventType::Fill: return "Fill";
            case EventType::CancelAck: return "CancelAck";
            case EventType::CancelReject: return "CancelReject";
            case EventType::ReplaceAck: return "ReplaceAck";
            case EventType::ReplaceReject: return "ReplaceReject";
        }
        return "UnknownEventType";
    }

    static const char* to_string(RejectReason reason) noexcept {
        switch (reason) {
            case RejectReason::None: return "None";
            case RejectReason::DuplicateOrderId: return "DuplicateOrderId";
            case RejectReason::UnknownSymbol: return "UnknownSymbol";
            case RejectReason::InvalidPrice: return "InvalidPrice";
            case RejectReason::InvalidQuantity: return "InvalidQuantity";
            case RejectReason::OrderNotFound: return "OrderNotFound";
            case RejectReason::OrderAlreadyClosed: return "OrderAlreadyClosed";
            case RejectReason::InvalidRequest: return "InvalidRequest";
        }
        return "UnknownRejectReason";
    }

    void print_event(const model::Event& event) {
        os_ << "  [Event]"
            << " type=" << to_string(event.type)
            << " symbol=" << event.symbol_id
            << " order_id=" << event.order_id
            << " related_order_id=" << event.related_order_id
            << " price=" << event.price
            << " quantity=" << event.quantity
            << " leaves=" << event.leaves_quantity
            << " reason=" << to_string(event.reason)
            << " seq=" << event.sequence
            << " ts=" << event.timestamp
            << '\n';
    }

    void print_trade(const model::Trade& trade) {
        os_ << "  [Trade]"
            << " trade_id=" << trade.trade_id
            << " symbol=" << trade.symbol_id
            << " buy_order_id=" << trade.buy_order_id
            << " sell_order_id=" << trade.sell_order_id
            << " price=" << trade.trade_price
            << " qty=" << trade.trade_quantity
            << " seq=" << trade.sequence
            << " ts=" << trade.timestamp
            << '\n';
    }

private:
    std::ostream& os_;
};

} // namespace matching_engine::engine