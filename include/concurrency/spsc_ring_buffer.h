#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace matching_engine::concurrency {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "Capacity must be at least 2.");
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two.");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable.");

private:
    static constexpr std::size_t kCacheLineSize = 64;
    static constexpr std::size_t kMask = Capacity - 1;

public:
    SpscRingBuffer() noexcept = default;
    ~SpscRingBuffer() = default;

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

public:
    // ------------------------------------------------------------
    // Producer-side API
    // ------------------------------------------------------------
    //
    // Single producer thread only.
    //
    [[nodiscard]] bool push(const T& value) noexcept {
        const std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        const std::size_t next_tail = increment(tail);

        // Fast path: use producer-local cached head first.
        if (next_tail == producer_cache_.head_cache) {
            producer_cache_.head_cache =
                head_.value.load(std::memory_order_acquire);

            if (next_tail == producer_cache_.head_cache) {
                return false;
            }
        }

        buffer_[tail] = value;
        tail_.value.store(next_tail, std::memory_order_release);
        return true;
    }

    // Optional observation helper. Producer thread only recommended.
    [[nodiscard]] bool full() const noexcept {
        const std::size_t tail = tail_.value.load(std::memory_order_relaxed);
        const std::size_t next_tail = increment(tail);

        std::size_t cached_head = producer_cache_.head_cache;
        if (next_tail != cached_head) {
            return false;
        }

        cached_head = head_.value.load(std::memory_order_acquire);
        producer_cache_.head_cache = cached_head;
        return next_tail == cached_head;
    }

public:
    // ------------------------------------------------------------
    // Consumer-side API
    // ------------------------------------------------------------
    //
    // Single consumer thread only.
    //
    [[nodiscard]] bool pop(T& out) noexcept {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);

        // Fast path: use consumer-local cached tail first.
        if (head == consumer_cache_.tail_cache) {
            consumer_cache_.tail_cache =
                tail_.value.load(std::memory_order_acquire);

            if (head == consumer_cache_.tail_cache) {
                return false;
            }
        }

        out = buffer_[head];
        head_.value.store(increment(head), std::memory_order_release);
        return true;
    }

    // Peek current front element without consuming it.
    //
    // Consumer thread only.
    // Returned pointer remains valid only until the next consumer-side state
    // change on this queue (pop/consume_one/clear), or until the slot is later
    // reused after consumption.
    [[nodiscard]] const T* peek() const noexcept {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);

        if (head == consumer_cache_.tail_cache) {
            consumer_cache_.tail_cache =
                tail_.value.load(std::memory_order_acquire);

            if (head == consumer_cache_.tail_cache) {
                return nullptr;
            }
        }

        return &buffer_[head];
    }

    // Consume one element without copying it out.
    //
    // Consumer thread only.
    [[nodiscard]] bool consume_one() noexcept {
        const std::size_t head = head_.value.load(std::memory_order_relaxed);

        if (head == consumer_cache_.tail_cache) {
            consumer_cache_.tail_cache =
                tail_.value.load(std::memory_order_acquire);

            if (head == consumer_cache_.tail_cache) {
                return false;
            }
        }

        head_.value.store(increment(head), std::memory_order_release);
        return true;
    }

    // Discard all currently published elements up to the observed tail.
    //
    // Consumer thread only.
    void clear() noexcept {
        const std::size_t tail = tail_.value.load(std::memory_order_acquire);
        consumer_cache_.tail_cache = tail;
        head_.value.store(tail, std::memory_order_release);
    }

public:
    // ------------------------------------------------------------
    // Observers
    // ------------------------------------------------------------
    //
    // These are observational helpers only.
    // Under concurrency, size() is an approximate snapshot.
    //
    [[nodiscard]] bool empty() const noexcept {
        const std::size_t head = head_.value.load(std::memory_order_acquire);
        const std::size_t tail = tail_.value.load(std::memory_order_acquire);
        return head == tail;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const std::size_t head = head_.value.load(std::memory_order_acquire);
        const std::size_t tail = tail_.value.load(std::memory_order_acquire);
        return distance(head, tail);
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

    [[nodiscard]] static constexpr std::size_t usable_capacity() noexcept {
        return Capacity - 1;
    }

private:
    [[nodiscard]] static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) & kMask;
    }

    [[nodiscard]] static constexpr std::size_t distance(std::size_t head,
                                                        std::size_t tail) noexcept {
        return (tail - head) & kMask;
    }

private:
    struct alignas(kCacheLineSize) AlignedAtomicIndex {
        std::atomic<std::size_t> value {0};
    };

    struct alignas(kCacheLineSize) ProducerCache {
        mutable std::size_t head_cache {0};
    };

    struct alignas(kCacheLineSize) ConsumerCache {
        mutable std::size_t tail_cache {0};
    };

    struct alignas(kCacheLineSize) AlignedBuffer {
        T data[Capacity] {};
    };

private:
    // Written only by consumer, read by producer.
    AlignedAtomicIndex head_;

    // Producer-local cached copy of head_.
    mutable ProducerCache producer_cache_;

    // Written only by producer, read by consumer.
    AlignedAtomicIndex tail_;

    // Consumer-local cached copy of tail_.
    mutable ConsumerCache consumer_cache_;

    // Data slots.
    AlignedBuffer buffer_storage_;

private:
    T* const buffer_ = buffer_storage_.data;
};

}  // namespace matching_engine::concurrency