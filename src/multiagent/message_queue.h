#pragma once
/**
 * @file message_queue.h
 * @brief Hàng đợi thông điệp thread-safe (std::queue + std::mutex +
 * std::condition_variable) — kênh giao tiếp giữa các sub-agent chạy song song
 * trên nhiều std::thread (mục 10.3, bonus Multi-agent Coordination).
 *
 * TEMPLATE CLASS tổng quát (giống tinh thần util::Registry<T> ở tầng Tool) —
 * có thể tái sử dụng cho bất kỳ kiểu thông điệp T nào, không riêng gì
 * AgentMessage.
 */
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace agent::multiagent {

template <typename T>
class MessageQueue {
public:
    void push(T item) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /// Lấy phần tử ngay nếu có, không chặn (non-blocking). Trả std::nullopt nếu rỗng.
    [[nodiscard]] std::optional<T> try_pop() {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /// Chặn (block) tới khi có phần tử hoặc hết timeout_ms (0 = chờ vô hạn).
    [[nodiscard]] std::optional<T> wait_and_pop(int timeout_ms = 0) {
        std::unique_lock lock(mutex_);
        auto has_item = [this] { return !queue_.empty(); };
        if (timeout_ms <= 0) {
            cv_.wait(lock, has_item);
        } else if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), has_item)) {
            return std::nullopt;  // timeout
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

}  // namespace agent::multiagent
