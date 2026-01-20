/**
 * @file unified_bus.cpp
 * @brief UnifiedBus implementation
 */
#include "sx/infra/unified_bus.h"
#include "sx/utils/mpmc_queue.h"
#include "sx/utils/overwrite_queue.h"
#include <zmq.h>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <system_error>

namespace sx::infra {

namespace {

class ZmqErrorCategory final : public std::error_category {
public:
    const char* name() const noexcept override { return "zmq"; }
    std::string message(int ev) const override { return std::string(zmq_strerror(ev)); }
};

const std::error_category& zmq_category() {
    static ZmqErrorCategory cat;
    return cat;
}

std::error_code make_zmq_error_from_errno() {
    return std::error_code(errno, zmq_category());
}

}  // namespace

class UnifiedBus::Impl {
public:
    Impl() = default;
    ~Impl();
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    // 数据流 Topic 管理
    struct StreamTopic {
        // 存储所有订阅该 Topic 的队列
        // 队列类型统一为 IQueue<shared_ptr<void>>
        std::vector<std::shared_ptr<sx::utils::IQueue<std::shared_ptr<void>>>> queues;
        std::mutex mutex;
    };

    // 控制流 Topic 管理
    std::unordered_map<std::string, std::vector<std::function<void(const std::string&)>>> control_topics_;
    std::mutex control_mutex_;

    // ---------------- ZeroMQ control-plane ----------------
    void* zmq_context_ = nullptr;
    std::mutex zmq_mutex_;

    struct SubWorker {
        void* socket = nullptr;
        std::thread thread;
        std::atomic<bool> stop{false};
        std::string endpoint;
    };

    // Scheme A: control-plane topic == ZMQ endpoint, keyed by endpoint
    std::unordered_map<std::string, void*> pub_sockets_;
    std::unordered_map<std::string, std::unique_ptr<SubWorker>> sub_workers_;

    // 数据流 Topic 表
    std::unordered_map<std::string, std::shared_ptr<StreamTopic>> stream_topics_;
    std::mutex stream_mutex_;

    // -------------------------------------------------------------------------

    [[nodiscard]] std::error_code ensure_zmq_context_locked() {
        if (zmq_context_ == nullptr) {
            zmq_context_ = zmq_ctx_new();
            if (zmq_context_ == nullptr) {
                return make_zmq_error_from_errno();
            }
        }
        return {};
    }

    [[nodiscard]] std::error_code publish_control(const std::string& endpoint, const std::string& message) {
        std::lock_guard<std::mutex> lock(zmq_mutex_);
        if (const auto ec = ensure_zmq_context_locked()) return ec;

        void*& pub = pub_sockets_[endpoint];
        if (pub == nullptr) {
            pub = zmq_socket(zmq_context_, ZMQ_PUB);
            if (pub == nullptr) {
                return make_zmq_error_from_errno();
            }
            const int linger = 0;
            (void)zmq_setsockopt(pub, ZMQ_LINGER, &linger, sizeof(linger));

            if (zmq_bind(pub, endpoint.c_str()) != 0) {
                const auto ec = make_zmq_error_from_errno();
                zmq_close(pub);
                pub = nullptr;
                return ec;
            }
        }

        if (zmq_send(pub, message.data(), message.size(), 0) < 0) {
            return make_zmq_error_from_errno();
        }
        return {};
    }

    void sub_worker_loop(SubWorker* w) {
        while (!w->stop.load(std::memory_order_relaxed)) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);

            const int rc = zmq_msg_recv(&msg, w->socket, 0);
            if (rc < 0) {
                zmq_msg_close(&msg);
                if (errno == EAGAIN) continue; // timeout
                // likely interrupted/shutdown
                continue;
            }
            const std::string recv_msg(
                static_cast<const char*>(zmq_msg_data(&msg)),
                static_cast<size_t>(zmq_msg_size(&msg)));

            zmq_msg_close(&msg);

            // dispatch callbacks for this endpoint (topic == endpoint)
            std::vector<std::function<void(const std::string&)>> callbacks;
            {
                std::lock_guard<std::mutex> lock(control_mutex_);
                auto it = control_topics_.find(w->endpoint);
                if (it != control_topics_.end()) callbacks = it->second;
            }
            for (auto& cb : callbacks) {
                cb(recv_msg);
            }
        }
    }

    [[nodiscard]] std::error_code subscribe_control(const std::string& endpoint,
                                                    std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(zmq_mutex_);
        if (const auto ec = ensure_zmq_context_locked()) return ec;

        if (sub_workers_.find(endpoint) == sub_workers_.end()) {
            auto worker = std::make_unique<SubWorker>();
            worker->endpoint = endpoint;

            worker->socket = zmq_socket(zmq_context_, ZMQ_SUB);
            if (worker->socket == nullptr) {
                return make_zmq_error_from_errno();
            }

            const int linger = 0;
            (void)zmq_setsockopt(worker->socket, ZMQ_LINGER, &linger, sizeof(linger));
            const int rcvtimeo = 100; // ms, for cooperative shutdown
            (void)zmq_setsockopt(worker->socket, ZMQ_RCVTIMEO, &rcvtimeo, sizeof(rcvtimeo));

            if (zmq_connect(worker->socket, endpoint.c_str()) != 0) {
                const auto ec = make_zmq_error_from_errno();
                zmq_close(worker->socket);
                worker->socket = nullptr;
                return ec;
            }

            // Subscribe to all messages on this endpoint.
            (void)zmq_setsockopt(worker->socket, ZMQ_SUBSCRIBE, "", 0);

            SubWorker* raw = worker.get();
            raw->thread = std::thread([this, raw]() { sub_worker_loop(raw); }); // TODO(luke): 使用内存池管理
            sub_workers_[endpoint] = std::move(worker);
        }

        {
            std::lock_guard<std::mutex> c_lock(control_mutex_);
            control_topics_[endpoint].push_back(std::move(callback));
        }

        return {};
    }

    void shutdown_zmq() {
        // stop receive threads
        {
            std::lock_guard<std::mutex> lock(zmq_mutex_);
            for (auto& [endpoint, w] : sub_workers_) {
                if (w) w->stop.store(true, std::memory_order_relaxed);
            }
        }

        // join threads (no lock to avoid deadlocks)
        for (auto& [endpoint, w] : sub_workers_) {
            if (w && w->thread.joinable()) w->thread.join();
        }

        // close sockets and context
        std::lock_guard<std::mutex> lock(zmq_mutex_);
        for (auto& [endpoint, sock] : pub_sockets_) {
            if (sock != nullptr) zmq_close(sock);
        }
        for (auto& [endpoint, w] : sub_workers_) {
            if (w && (w->socket != nullptr)) zmq_close(w->socket);
        }
        pub_sockets_.clear();
        sub_workers_.clear();

        if (zmq_context_ != nullptr) {
            zmq_ctx_term(zmq_context_);
            zmq_context_ = nullptr;
        }
    }

    void shutdown() {
        shutdown_zmq();
        // best-effort: clear in-process stream topics too
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_topics_.clear();

        std::lock_guard<std::mutex> c_lock(control_mutex_);
        control_topics_.clear();
    }

    void publish_stream(const std::string& topic, std::shared_ptr<void> data) {
        std::shared_ptr<StreamTopic> topic_ptr;
        {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            auto it = stream_topics_.find(topic);
            if (it == stream_topics_.end()) {
                // 若 Topic 不存在，直接返回（无副作用）
                return; 
            }
            topic_ptr = it->second;
        }

        // 分发给所有订阅队列
        std::lock_guard<std::mutex> lock(topic_ptr->mutex);
        for (auto& queue : topic_ptr->queues) {
            queue->push(data);
        }
    }

    std::shared_ptr<void> subscribe_stream(const std::string& topic, sx::types::StreamMode mode) {
        std::shared_ptr<StreamTopic> topic_ptr;
        {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            if (stream_topics_.find(topic) == stream_topics_.end()) {
                stream_topics_[topic] = std::make_shared<StreamTopic>();
            }
            topic_ptr = stream_topics_[topic];
        }

        // 创建新队列
        std::shared_ptr<sx::utils::IQueue<std::shared_ptr<void>>> new_queue;
        if (mode == sx::types::StreamMode::kReliableFifo) {
             new_queue = std::make_shared<sx::utils::MPMCQueue<std::shared_ptr<void>>>();
        } else if (mode == sx::types::StreamMode::kRealTimeLatest) {
             // OverwriteQueue 容量通常为 1
             new_queue = std::make_shared<sx::utils::OverwriteQueue<std::shared_ptr<void>>>(1);
        } else {
            return nullptr;
        }

        {
            std::lock_guard<std::mutex> lock(topic_ptr->mutex);
            topic_ptr->queues.push_back(new_queue);
        }
        
        // 返回 shared_ptr<void> 进行类型擦除，头文件会将其转回
        return std::static_pointer_cast<void>(new_queue);
    }
};

UnifiedBus::Impl::~Impl() {
    shutdown_zmq();
}

// ================= UnifiedBus Implementation =================

UnifiedBus::UnifiedBus() : impl_(std::make_unique<Impl>()) {}
UnifiedBus::~UnifiedBus() = default;

std::error_code UnifiedBus::publish(const std::string& topic, const std::string& message) {
    return impl_->publish_control(topic, message);
}

std::error_code UnifiedBus::subscribe(const std::string& topic, std::function<void(const std::string&)> callback) {
    return impl_->subscribe_control(topic, std::move(callback));
}

void UnifiedBus::shutdown() {
    impl_->shutdown();
}

void UnifiedBus::publish_stream_impl(const std::string& topic, std::shared_ptr<void> data) {
    impl_->publish_stream(topic, data);
}

std::shared_ptr<void> UnifiedBus::subscribe_stream_impl(const std::string& topic, sx::types::StreamMode mode) {
    return impl_->subscribe_stream(topic, mode);
}

} // namespace sx::infra
