#pragma once
#include <atomic>
#include <vector>

namespace SortSynth {

/** Single-producer single-consumer lock-free queue. */
template <typename T, size_t Capacity = 2048>
class SPSCQueue {
public:
    SPSCQueue() : readIdx(0), writeIdx(0) { buffer.resize(Capacity); }

    bool push(const T& item) {
        size_t w = writeIdx.load(std::memory_order_relaxed);
        size_t next = (w + 1) % Capacity;
        if (next == readIdx.load(std::memory_order_acquire)) return false; // full
        buffer[w] = item;
        writeIdx.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t r = readIdx.load(std::memory_order_relaxed);
        if (r == writeIdx.load(std::memory_order_acquire)) return false; // empty
        item = buffer[r];
        readIdx.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool isEmpty() const {
        return readIdx.load(std::memory_order_acquire) == writeIdx.load(std::memory_order_acquire);
    }

    void reset() {
        readIdx.store(0, std::memory_order_release);
        writeIdx.store(0, std::memory_order_release);
    }

private:
    std::vector<T> buffer;
    std::atomic<size_t> readIdx;
    std::atomic<size_t> writeIdx;
};

} // namespace SortSynth
