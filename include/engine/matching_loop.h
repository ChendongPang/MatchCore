#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "concurrency/spsc_ring_buffer.h"
#include "engine/command.h"
#include "engine/matching_engine.h"
#include "model/execution_report.h"

namespace matching_engine::engine {

class IExecutionReportSink {
public:
    virtual ~IExecutionReportSink() = default;
    virtual void on_execution_report(const model::ExecutionReport& report) = 0;
};

class NullExecutionReportSink final : public IExecutionReportSink {
public:
    void on_execution_report(const model::ExecutionReport& report) override {
        (void)report;
    }
};

template <std::size_t QueueCapacity>
class MatchingLoop {
public:
    using Queue = concurrency::SpscRingBuffer<Command, QueueCapacity>;

public:
    MatchingLoop(MatchingEngine& engine,
                 Queue& ingress_queue,
                 IExecutionReportSink& sink) noexcept
        : engine_(engine),
          ingress_queue_(ingress_queue),
          sink_(sink) {
    }

    MatchingLoop(const MatchingLoop&) = delete;
    MatchingLoop& operator=(const MatchingLoop&) = delete;
    MatchingLoop(MatchingLoop&&) = delete;
    MatchingLoop& operator=(MatchingLoop&&) = delete;

    ~MatchingLoop() = default;

public:
    void start() {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
            return;
        }

        stop_requested_.store(false, std::memory_order_release);

        worker_ = std::thread([this]() {
            this->run();
        });
    }

    void stop() noexcept {
        stop_requested_.store(true, std::memory_order_release);
    }

    void stop_and_join() noexcept {
        stop();

        if (worker_.joinable()) {
            worker_.join();
        }

        running_.store(false, std::memory_order_release);
    }

    bool poll_once() {
        Command cmd;
        if (!ingress_queue_.pop(cmd)) {
            return false;
        }

        model::ExecutionReport report = engine_.dispatch(cmd);
        sink_.on_execution_report(report);

        total_commands_processed_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    std::size_t poll_batch(std::size_t max_count) {
        std::size_t consumed = 0;

        for (; consumed < max_count; ++consumed) {
            Command cmd;
            if (!ingress_queue_.pop(cmd)) {
                break;
            }

            model::ExecutionReport report = engine_.dispatch(cmd);
            sink_.on_execution_report(report);

            total_commands_processed_.fetch_add(1, std::memory_order_relaxed);
        }

        return consumed;
    }

    void run() {
        while (!stop_requested_.load(std::memory_order_acquire)) {
            const std::size_t consumed = poll_batch(64);

            if (consumed == 0) {
                idle_spins_.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }

        running_.store(false, std::memory_order_release);
    }

public:
    [[nodiscard]] bool running() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool stop_requested() const noexcept {
        return stop_requested_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t total_commands_processed() const noexcept {
        return total_commands_processed_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t idle_spins() const noexcept {
        return idle_spins_.load(std::memory_order_relaxed);
    }

private:
    MatchingEngine& engine_;
    Queue& ingress_queue_;
    IExecutionReportSink& sink_;

    std::atomic<bool> running_ {false};
    std::atomic<bool> stop_requested_ {false};

    std::atomic<std::uint64_t> total_commands_processed_ {0};
    std::atomic<std::uint64_t> idle_spins_ {0};

    std::thread worker_;
};

} // namespace matching_engine::engine