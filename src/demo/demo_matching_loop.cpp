#include <iostream>
#include <thread>

#include "concurrency/spsc_ring_buffer.h"
#include "engine/command.h"
#include "engine/matching_engine.h"
#include "engine/matching_loop.h"
#include "engine/print_execution_report_sink.h"
#include "model/requests.h"

int main() {
    using matching_engine::OrderId;
    using matching_engine::Price;
    using matching_engine::Quantity;
    using matching_engine::Side;
    using matching_engine::SymbolId;
    using matching_engine::Timestamp;
    using matching_engine::OrderType;
    constexpr std::size_t kQueueCapacity = 1024;
    constexpr SymbolId kSymbolId = 1;

    using matching_engine::concurrency::SpscRingBuffer;
    using matching_engine::engine::Command;
    using matching_engine::engine::MatchingEngine;
    using matching_engine::engine::MatchingLoop;
    using matching_engine::engine::PrintExecutionReportSink;

    SpscRingBuffer<Command, kQueueCapacity> queue;

    MatchingEngine engine;
    engine.add_symbol(kSymbolId);

    PrintExecutionReportSink sink(std::cout);
    MatchingLoop<kQueueCapacity> loop(engine, queue, sink);

    loop.start();

    {
        matching_engine::model::NewOrderRequest req;
        req.order_id = 1;
        req.symbol_id = kSymbolId;
        req.side = Side::Sell;
        req.type = OrderType::Limit;
        req.price = 100;
        req.quantity = 10;
        req.timestamp = 1;

        while (!queue.push(Command::make_new_order(req))) {
            std::this_thread::yield();
        }
    }

    {
        matching_engine::model::NewOrderRequest req;
        req.order_id = 2;
        req.symbol_id = kSymbolId;
        req.side = Side::Buy;
        req.type = OrderType::Limit;
        req.price = 100;
        req.quantity = 5;
        req.timestamp = 2;

        while (!queue.push(Command::make_new_order(req))) {
            std::this_thread::yield();
        }
    }

    {
        matching_engine::model::CancelRequest req;
        req.symbol_id = kSymbolId;
        req.order_id = 1;
        req.timestamp = 3;

        while (!queue.push(Command::make_cancel(req))) {
            std::this_thread::yield();
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    loop.stop_and_join();

    return 0;
}