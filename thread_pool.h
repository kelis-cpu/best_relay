/*
 * @Author: kelise
 * @Date: 2024-10-22 22:04:10
 * @LastEditors: kelis-cpu
 * @LastEditTime: 2024-10-29 23:10:50
 * @Description: file content
 */
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <utility>
#include <vector>
#include <thread>
#include <condition_variable>
#include <future>

template <class T, int max_size = 1000>
class SafeQueue {
 public:
    SafeQueue() {
        mutex_ = std::make_unique<std::mutex>();
    }

    SafeQueue(SafeQueue&& other) {
        if (&other != this) {
            mutex_.reset(other.mutex_.release());
            q_ = std::move(other.q_);
        }
    }

    bool Pop(T &t) {
        std::lock_guard<std::mutex> lk(*mutex_);
        if (q_.empty()) {
            return false;
        }
        t = std::move(q_.front());
        q_.pop();
        return true;
    }

    void Push(T &t) {
        std::lock_guard<std::mutex> lk(*mutex_);
        q_.push(t);
    }

    bool Pop(T&& t) {
        std::lock_guard<std::mutex> lk(*mutex_);
        if (q_.empty()) {
            return false;
        }
        t = std::move(q_.front());
        q_.pop();
        return true;
    }

    void Push(T&& t) {
        std::lock_guard<std::mutex> lk(*mutex_);
        q_.push(std::move(t));
    }

    bool Empty() {
        std::lock_guard<std::mutex> lk(*mutex_);
        return q_.empty();
    }

    int Size() {
        std::lock_guard<std::mutex> lk(*mutex_);
        return q_.size();
    }

    bool ValidPush() {
        std::lock_guard<std::mutex> lk(*mutex_);
        return q_.size() < max_size;
    }

 private:
    std::queue<T> q_;
    // std::mutex mutex_;
    std::unique_ptr<std::mutex> mutex_;
};

class ThreadPool {
 private:
    class ThreadWorker {
     public:
        ThreadWorker(ThreadPool* belong) : belong_to_(belong) {}

        void operator()() {
            
            while (true) {
                std::function<void()> work;
                std::unique_lock<std::mutex> lk(belong_to_->condition_mutex_);

                belong_to_->cv_.wait(lk, [this]() { return belong_to_->stop_ || !belong_to_->tasks_.Empty(); });
                if (belong_to_->stop_ && belong_to_->tasks_.Empty()) {
                    return;
                }

                bool pop = belong_to_->tasks_.Pop(work);
                if (pop) {
                    work();
                }
            }
        }
     
     private:
        ThreadPool* belong_to_;
    };

 public:
    explicit ThreadPool(int n) {
        for (int i = 0; i < n; i++) {
            threads_.emplace_back(ThreadWorker(this));
        }
    }

    template <class F, class ...Args>
    auto Submit(F &&f, Args&&... args) -> std::future<decltype(f(args...))> {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);

        std::function<void()> wrapper_func = [task_ptr]() {
            (*task_ptr)();
        };

        std::unique_lock<std::mutex> lk(condition_mutex_);
        cv_.wait(lk, [this](){ return tasks_.ValidPush(); });

        tasks_.Push(wrapper_func);
        cv_.notify_one();
        return task_ptr->get_future();
    }

    void Stop() { 
        stop_ = true;
        cv_.notify_all();
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    ~ThreadPool() {
        Stop();
    }

 private:
    std::vector<std::thread> threads_;
    SafeQueue<std::function<void()>> tasks_;
    bool stop_{false};
    std::mutex condition_mutex_;
    std::condition_variable cv_;
};