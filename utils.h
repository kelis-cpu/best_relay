/*
 * @Author: kelise
 * @Date: 2024-10-27 14:38:40
 * @LastEditors: kelis-cpu
 * @LastEditTime: 2024-11-03 22:44:34
 * @Description: file content
 */
#include <vector>
#include <mutex>
#include <atomic>
#include <iostream>

// 统计每个节点接收到的消息的时间
class TestResult {
 public:
    TestResult() {}

    TestResult(TestResult& other) {
        receive_msg_nodes_ = other.GetReceiveNode();
        receive_msg_times_ = other.GetReceiveTime();
        receive_sequence_ = std::move(other.GetSequence());
    }

    TestResult(TestResult&& other) {
        if (this != &other) {
            receive_msg_nodes_ = other.GetReceiveNode();
            receive_msg_times_ = other.GetReceiveTime();
            receive_sequence_ = std::move(other.GetSequence());
        }
    }

    TestResult& operator=(TestResult& other) {
        if (this != &other) {
            receive_msg_nodes_ = other.GetReceiveNode();
            receive_msg_times_ = other.GetReceiveTime();
            receive_sequence_ = std::move(other.GetSequence());
        }
        return *this;
    }

    void AddReceiveNode(int node_cnts) {
        receive_msg_nodes_.fetch_add(node_cnts);
    }

    void AddReceiveTime(double time, long long total_msgs = 0) {
        std::lock_guard<std::mutex> lk(time_mutex_);
        receive_msg_times_.store(receive_msg_times_.load() + time);
        receive_sequence_.push_back(total_msgs);
    }

    double GetReceiveTime() const { return receive_msg_times_.load(); }

    int GetReceiveNode() const { return receive_msg_nodes_.load(); }

    std::vector<long> GetSequence() { return receive_sequence_; }

 private:
    std::atomic<int> receive_msg_nodes_{0}; // 接收到消息的节点数
    std::atomic<double> receive_msg_times_{0}; // 所有接收到消息的节点延迟
    std::vector<long> receive_sequence_;
    std::mutex time_mutex_;
};

struct VarintPercentage {
    long long Eighty = 0; // 交易传播全网80%节点的总交易数量
    long long Ninty = 0;
    long long All = 0;

    // 重载输出运算符
    friend std::ostream& operator<<(std::ostream& os, const VarintPercentage& vp) {
        os << "Eighty: " << vp.Eighty << ", Ninty: " << vp.Ninty << ", Hundred: " << vp.All;
        return os;
    }
};
