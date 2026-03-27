#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <thread>
#include <vector>

#include "common/enums.h"
#include "common/types.h"
#include "concurrency/spsc_ring_buffer.h"
#include "engine/command.h"
#include "engine/matching_engine.h"
#include "engine/matching_loop.h"
#include "model/event.h"
#include "model/execution_report.h"
#include "model/requests.h"

namespace {

using Clock = std::chrono::steady_clock;

using matching_engine::OrderId;
using matching_engine::Price;
using matching_engine::Quantity;
using matching_engine::Side;
using matching_engine::SymbolId;
using matching_engine::Timestamp;
using matching_engine::OrderType;
using matching_engine::EventType;

using matching_engine::concurrency::SpscRingBuffer;
using matching_engine::engine::Command;
using matching_engine::engine::MatchingEngine;
using matching_engine::engine::MatchingLoop;

constexpr std::size_t kQueueCapacity = 1u << 16;
constexpr SymbolId kSymbolId = 1;

// ------------------------------------------------------------
// Insert-only sweep config
// ------------------------------------------------------------

constexpr std::uint64_t kInsertOnlyNumCommands = 1'000'000;
constexpr Price kInsertOnlyBasePrice = 100;
constexpr std::array<std::uint32_t, 8> kInsertOnlyLevelSweep = {
    1, 2, 4, 8, 16, 32, 64, 128
};

// ------------------------------------------------------------
// Alternating-match config
// ------------------------------------------------------------

constexpr std::uint64_t kAlternatingNumCommands = 1'000'000;
constexpr Price kAlternatingBuyPrice = 101;
constexpr Price kAlternatingSellPrice = 100;

// ------------------------------------------------------------
// Sweep-book config
// ------------------------------------------------------------

constexpr std::uint64_t kSweepPreloadOrders = 65'536;
constexpr std::uint32_t kSweepPriceLevels = 64;
constexpr std::uint32_t kSweepOrdersPerLevel = 1'024; // 64 * 1024 = 65536
constexpr std::uint64_t kSweepAggressiveOrders = 100'000;
constexpr Quantity kSweepAggressiveQty = 16;
constexpr Price kSweepBaseAskPrice = 100;
constexpr Price kSweepAggressiveBuyPrice = 1'000'000;

// ------------------------------------------------------------
// Types
// ------------------------------------------------------------

enum class BenchmarkScenario : std::uint8_t {
    InsertOnly = 0,
    AlternatingMatch,
    SweepBook
};

struct BenchmarkCounters {
    std::uint64_t submitted_commands {0};
    std::uint64_t processed_commands {0};

    std::uint64_t new_order_count {0};

    std::uint64_t ack_count {0};
    std::uint64_t reject_count {0};
    std::uint64_t partial_fill_count {0};
    std::uint64_t fill_count {0};
    std::uint64_t cancel_ack_count {0};
    std::uint64_t cancel_reject_count {0};
    std::uint64_t replace_ack_count {0};
    std::uint64_t replace_reject_count {0};

    std::uint64_t trade_count {0};

    std::uint64_t idle_spins {0};

    std::uint64_t final_resting_orders {0};
    std::uint64_t final_price_levels {0};
};

struct BenchmarkTiming {
    double elapsed_seconds {0.0};
    double throughput_cmd_per_sec {0.0};
    double avg_ns_per_command {0.0};
};

struct BenchmarkResult {
    BenchmarkScenario scenario {BenchmarkScenario::InsertOnly};

    std::uint32_t price_levels_config {0};
    std::uint64_t preload_orders {0};
    std::uint64_t aggressive_orders {0};
    Quantity aggressive_qty {0};

    BenchmarkCounters counters {};
    BenchmarkTiming timing {};
};

// ------------------------------------------------------------
// Counting sink
// ------------------------------------------------------------

class CountingExecutionReportSink final : public matching_engine::engine::IExecutionReportSink {
public:
    CountingExecutionReportSink() = default;

    void on_execution_report(const matching_engine::model::ExecutionReport& report) override {
        counters_.trade_count += static_cast<std::uint64_t>(report.trade_count());

        for (const auto& event : report.events) {
            switch (event.type) {
                case EventType::Ack:
                    ++counters_.ack_count;
                    break;
                case EventType::Reject:
                    ++counters_.reject_count;
                    break;
                case EventType::PartialFill:
                    ++counters_.partial_fill_count;
                    break;
                case EventType::Fill:
                    ++counters_.fill_count;
                    break;
                case EventType::CancelAck:
                    ++counters_.cancel_ack_count;
                    break;
                case EventType::CancelReject:
                    ++counters_.cancel_reject_count;
                    break;
                case EventType::ReplaceAck:
                    ++counters_.replace_ack_count;
                    break;
                case EventType::ReplaceReject:
                    ++counters_.replace_reject_count;
                    break;
                default:
                    break;
            }
        }
    }

    [[nodiscard]] const BenchmarkCounters& counters() const noexcept {
        return counters_;
    }

    void reset() noexcept {
        counters_ = BenchmarkCounters {};
    }

private:
    BenchmarkCounters counters_ {};
};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------

[[nodiscard]] const char* to_string(BenchmarkScenario scenario) noexcept {
    switch (scenario) {
        case BenchmarkScenario::InsertOnly:
            return "InsertOnly";
        case BenchmarkScenario::AlternatingMatch:
            return "AlternatingMatch";
        case BenchmarkScenario::SweepBook:
            return "SweepBook";
        default:
            return "Unknown";
    }
}

[[nodiscard]] Price make_insert_only_price(std::uint32_t price_levels,
                                           std::uint64_t sequence) noexcept {
    if (price_levels <= 1) {
        return kInsertOnlyBasePrice;
    }

    return static_cast<Price>(
        kInsertOnlyBasePrice +
        static_cast<Price>((sequence - 1) % price_levels)
    );
}

matching_engine::model::NewOrderRequest make_insert_only_request(std::uint32_t price_levels,
                                                                 std::uint64_t sequence) {
    matching_engine::model::NewOrderRequest req {};
    req.order_id = static_cast<OrderId>(sequence);
    req.symbol_id = kSymbolId;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = make_insert_only_price(price_levels, sequence);
    req.quantity = 1;
    req.timestamp = static_cast<Timestamp>(sequence);
    return req;
}

matching_engine::model::NewOrderRequest make_alternating_request(std::uint64_t sequence) {
    matching_engine::model::NewOrderRequest req {};
    req.order_id = static_cast<OrderId>(sequence);
    req.symbol_id = kSymbolId;
    req.side = ((sequence & 1u) != 0u) ? Side::Buy : Side::Sell;
    req.type = OrderType::Limit;
    req.price = (req.side == Side::Buy) ? kAlternatingBuyPrice : kAlternatingSellPrice;
    req.quantity = 1;
    req.timestamp = static_cast<Timestamp>(sequence);
    return req;
}

matching_engine::model::NewOrderRequest make_sweep_preload_request(std::uint64_t sequence) {
    matching_engine::model::NewOrderRequest req {};
    req.order_id = static_cast<OrderId>(sequence);
    req.symbol_id = kSymbolId;
    req.side = Side::Sell;
    req.type = OrderType::Limit;

    const std::uint64_t level_index = (sequence - 1) / kSweepOrdersPerLevel;
    req.price = static_cast<Price>(kSweepBaseAskPrice + static_cast<Price>(level_index));
    req.quantity = 1;
    req.timestamp = static_cast<Timestamp>(sequence);
    return req;
}

matching_engine::model::NewOrderRequest make_sweep_aggressive_request(std::uint64_t order_id,
                                                                      std::uint64_t timestamp) {
    matching_engine::model::NewOrderRequest req {};
    req.order_id = static_cast<OrderId>(order_id);
    req.symbol_id = kSymbolId;
    req.side = Side::Buy;
    req.type = OrderType::Limit;
    req.price = kSweepAggressiveBuyPrice;
    req.quantity = kSweepAggressiveQty;
    req.timestamp = static_cast<Timestamp>(timestamp);
    return req;
}

template <typename ProducerFn>
BenchmarkResult run_new_order_only_benchmark(BenchmarkScenario scenario,
                                             ProducerFn&& producer_fn) {
    BenchmarkResult result {};
    result.scenario = scenario;

    SpscRingBuffer<Command, kQueueCapacity> queue;

    MatchingEngine engine;
    if (!engine.add_symbol(kSymbolId)) {
        std::cerr << "failed to add symbol\n";
        return result;
    }

    CountingExecutionReportSink sink;
    sink.reset();

    MatchingLoop<kQueueCapacity> loop(engine, queue, sink);
    loop.start();

    const auto start = Clock::now();

    std::thread producer([&]() {
        producer_fn(queue, result);
    });

    producer.join();

    while (loop.total_commands_processed() < result.counters.submitted_commands) {
        std::this_thread::yield();
    }

    const auto end = Clock::now();

    loop.stop_and_join();

    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    result.counters.processed_commands = loop.total_commands_processed();
    result.counters.idle_spins = loop.idle_spins();

    const BenchmarkCounters& sink_counters = sink.counters();
    result.counters.ack_count = sink_counters.ack_count;
    result.counters.reject_count = sink_counters.reject_count;
    result.counters.partial_fill_count = sink_counters.partial_fill_count;
    result.counters.fill_count = sink_counters.fill_count;
    result.counters.cancel_ack_count = sink_counters.cancel_ack_count;
    result.counters.cancel_reject_count = sink_counters.cancel_reject_count;
    result.counters.replace_ack_count = sink_counters.replace_ack_count;
    result.counters.replace_reject_count = sink_counters.replace_reject_count;
    result.counters.trade_count = sink_counters.trade_count;

    result.counters.final_resting_orders =
        static_cast<std::uint64_t>(engine.total_resting_orders(kSymbolId));
    result.counters.final_price_levels =
        static_cast<std::uint64_t>(engine.total_price_levels(kSymbolId));

    result.timing.elapsed_seconds = static_cast<double>(elapsed_ns) / 1e9;

    if (result.timing.elapsed_seconds > 0.0 && result.counters.submitted_commands > 0) {
        result.timing.throughput_cmd_per_sec =
            static_cast<double>(result.counters.submitted_commands) /
            result.timing.elapsed_seconds;

        result.timing.avg_ns_per_command =
            static_cast<double>(elapsed_ns) /
            static_cast<double>(result.counters.submitted_commands);
    }

    return result;
}

void push_new_order(SpscRingBuffer<Command, kQueueCapacity>& queue,
                    const matching_engine::model::NewOrderRequest& req,
                    BenchmarkResult& result) {
    const Command cmd = Command::make_new_order(req);

    while (!queue.push(cmd)) {
        std::this_thread::yield();
    }

    ++result.counters.submitted_commands;
    ++result.counters.new_order_count;
}

void print_result(const BenchmarkResult& result) {
    std::cout << "=== Benchmark: " << to_string(result.scenario) << " ===\n";

    if (result.scenario == BenchmarkScenario::InsertOnly) {
        std::cout << "price_levels_config      : " << result.price_levels_config << '\n';
    }

    if (result.scenario == BenchmarkScenario::SweepBook) {
        std::cout << "preload_orders           : " << result.preload_orders << '\n';
        std::cout << "aggressive_orders        : " << result.aggressive_orders << '\n';
        std::cout << "aggressive_qty           : " << result.aggressive_qty << '\n';
        std::cout << "preload_levels_config    : " << result.price_levels_config << '\n';
    }

    std::cout << "submitted_commands       : " << result.counters.submitted_commands << '\n';
    std::cout << "processed_commands       : " << result.counters.processed_commands << '\n';

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "elapsed_seconds          : " << result.timing.elapsed_seconds << '\n';

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "throughput_cmd_per_s     : " << result.timing.throughput_cmd_per_sec << '\n';
    std::cout << "avg_ns_per_command       : " << result.timing.avg_ns_per_command << '\n';

    std::cout << "new_orders               : " << result.counters.new_order_count << '\n';
    std::cout << "acks                     : " << result.counters.ack_count << '\n';
    std::cout << "rejects                  : " << result.counters.reject_count << '\n';
    std::cout << "partial_fills            : " << result.counters.partial_fill_count << '\n';
    std::cout << "fills                    : " << result.counters.fill_count << '\n';
    std::cout << "cancel_acks              : " << result.counters.cancel_ack_count << '\n';
    std::cout << "cancel_rejects           : " << result.counters.cancel_reject_count << '\n';
    std::cout << "replace_acks             : " << result.counters.replace_ack_count << '\n';
    std::cout << "replace_rejects          : " << result.counters.replace_reject_count << '\n';
    std::cout << "trades                   : " << result.counters.trade_count << '\n';
    std::cout << "idle_spins               : " << result.counters.idle_spins << '\n';
    std::cout << "final_resting_orders     : " << result.counters.final_resting_orders << '\n';
    std::cout << "final_price_levels       : " << result.counters.final_price_levels << '\n';
    std::cout << '\n';
}

void print_insert_only_summary(const std::vector<BenchmarkResult>& results) {
    std::cout << "================ Insert-Only Level Sweep Summary ================\n";
    std::cout << std::left
              << std::setw(10) << "levels"
              << std::setw(20) << "throughput(cmd/s)"
              << std::setw(18) << "avg_ns/cmd"
              << std::setw(14) << "idle_spins"
              << std::setw(18) << "resting_orders"
              << std::setw(14) << "book_levels"
              << '\n';

    for (const auto& result : results) {
        std::cout << std::left
                  << std::setw(10) << result.price_levels_config
                  << std::setw(20) << std::fixed << std::setprecision(2)
                  << result.timing.throughput_cmd_per_sec
                  << std::setw(18) << result.timing.avg_ns_per_command
                  << std::setw(14) << result.counters.idle_spins
                  << std::setw(18) << result.counters.final_resting_orders
                  << std::setw(14) << result.counters.final_price_levels
                  << '\n';
    }

    std::cout << "===============================================================\n\n";
}

// ------------------------------------------------------------
// Cases
// ------------------------------------------------------------

BenchmarkResult run_insert_only_benchmark(std::uint32_t price_levels) {
    BenchmarkResult result = run_new_order_only_benchmark(
        BenchmarkScenario::InsertOnly,
        [price_levels](SpscRingBuffer<Command, kQueueCapacity>& queue, BenchmarkResult& out) {
            out.price_levels_config = price_levels;

            for (std::uint64_t i = 1; i <= kInsertOnlyNumCommands; ++i) {
                const auto req = make_insert_only_request(price_levels, i);
                push_new_order(queue, req, out);
            }
        });

    return result;
}

BenchmarkResult run_alternating_match_benchmark() {
    BenchmarkResult result = run_new_order_only_benchmark(
        BenchmarkScenario::AlternatingMatch,
        [](SpscRingBuffer<Command, kQueueCapacity>& queue, BenchmarkResult& out) {
            for (std::uint64_t i = 1; i <= kAlternatingNumCommands; ++i) {
                const auto req = make_alternating_request(i);
                push_new_order(queue, req, out);
            }
        });

    return result;
}

BenchmarkResult run_sweep_book_benchmark() {
    BenchmarkResult result = run_new_order_only_benchmark(
        BenchmarkScenario::SweepBook,
        [](SpscRingBuffer<Command, kQueueCapacity>& queue, BenchmarkResult& out) {
            out.preload_orders = kSweepPreloadOrders;
            out.aggressive_orders = kSweepAggressiveOrders;
            out.aggressive_qty = kSweepAggressiveQty;
            out.price_levels_config = kSweepPriceLevels;

            std::uint64_t order_id = 1;
            std::uint64_t ts = 1;

            // Phase 1: preload asks
            for (std::uint64_t i = 1; i <= kSweepPreloadOrders; ++i, ++order_id, ++ts) {
                const auto req = make_sweep_preload_request(i);
                push_new_order(queue, req, out);
            }

            // Phase 2: aggressive buys
            for (std::uint64_t i = 0; i < kSweepAggressiveOrders; ++i, ++order_id, ++ts) {
                const auto req = make_sweep_aggressive_request(order_id, ts);
                push_new_order(queue, req, out);
            }
        });

    return result;
}

void run_insert_only_suite() {
    std::vector<BenchmarkResult> results;
    results.reserve(kInsertOnlyLevelSweep.size());

    for (const std::uint32_t levels : kInsertOnlyLevelSweep) {
        const BenchmarkResult result = run_insert_only_benchmark(levels);
        print_result(result);
        results.push_back(result);
    }

    print_insert_only_summary(results);
}

} // namespace

int main() {
    run_insert_only_suite();

    const BenchmarkResult alternating = run_alternating_match_benchmark();
    print_result(alternating);

    const BenchmarkResult sweep_book = run_sweep_book_benchmark();
    print_result(sweep_book);

    return 0;
}