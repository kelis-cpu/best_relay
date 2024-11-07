#include <cstdio>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <utility>
#include <vector>
#include <queue>
#include <cmath>
#include <string>
#include <random>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <map>
#include <atomic>
#include <csignal>

#include "coordinate.h"
#include "thread_pool.h"
#include "utils.h"

#define memcle(a) memset(a, 0, sizeof(a))

using namespace std;
const static int K = 8;
const int N = 8500;
const double pi = acos(-1);
const double R = 6371000; // radius of the earth
const double inf = 1e8;
const int MAX_DEPTH = 40;
const double FIXED_DELAY = 250;
const int ROOT_FANOUT = 64;
const int SECOND_FANOUT = 64;
const int FANOUT = 8;
const int INNER_DEG = 4;
const int MAX_TEST_N = 8000;
const int MAX_OUTBOUND = 8;
//typedef unsigned int int;
int testid = 0;
/// @brief 节点数
int n;
mt19937 rd(1000);
bool recv_flag[N]; // 节点是否已经接收到该消息
int recv_parent[N]; // 节点接收到消息的来源
double recv_time[N]; // 节点接收到消息的具体时间
double recv_dist[N]; // 节点接收到消息的时间
int depth[N];

int mal_flag[N]; // 恶意节点
FILE* fig_csv;


int recv_step[N]; // 每个节点接收到交易时的交易step
unordered_map<int ,int> dup_msg_cnts; // 记录在哪一步得到的重复消息

std::atomic<int> unique_message_id(0);
std::atomic<bool> stop(false);
const int kPriority = 100;
const double kAdjustPriority = 0.1;
double kEighty = 0.8;
double kNinty = 0.9;

std::atomic<double> bandwith(0.0);
std::atomic<double> latency(0.0);

typedef SafeQueue<std::future<TestResult>> TestResultQueue;

// coordinate, using longitude and latitude
class LatLonCoordinate {
  public:
    double lat, lon;
};
/// @brief 经纬度坐标集合
LatLonCoordinate coord[N];

// from degree to radian 
double rad(double deg) {return deg * pi / 180;}

// distance between two coordinate
double distance(const LatLonCoordinate &a, const LatLonCoordinate &b) {
    if (abs(a.lat - b.lat) < 0.1 && abs(a.lon - b.lon) < 0.1)
        return 0;
    double latA = rad(a.lat), lonA = rad(a.lon);
    double latB = rad(b.lat), lonB = rad(b.lon);
    double C = cos(latA) * cos(latB) * cos(lonA - lonB) + sin(latA) * sin(latB);
    double dist = acos(C) * R ;
    return dist / 100000 * 2;
}

class message {
  public:
    int root, src, dst, step;
    double send_time, recv_time;

    message(int _root, int _src, int _dst, int _step, double _send_time, double _recv_time) : 
        root(_root), src(_src), dst(_dst), step(_step), send_time(_send_time), recv_time(_recv_time) {
    }

    void print_info() {
        fprintf(stderr, "message rooted at %d sent from node %d to %d\n, step %d", root, src, dst, step);
        fprintf(stderr, "send time at %.2f, recv time at %.2f, delay is %.2f\n", send_time, recv_time, recv_time - send_time);
    }
};

bool operator>(const message &a, const message &b) {
    return a.recv_time > b.recv_time;
}

class Message {
 public:
    Message(int id, int root, int src, int dst, int step, double send_time, double recv_time) :
            id_(id), root_(root),  src_(src), dst_(dst), step_(step), send_time_(send_time), recv_time_(recv_time) {}
    
    int GetStep() { return step_; }
    
    void AddStep(int step) {
        step_ += step;
    }

    int GetSrc() { return src_; }

    int GetDst() { return dst_; }

    int GetId() { return id_; }

    double GetRecvTime() const { return recv_time_; }
    
    bool operator>(const Message& other) const {
        return recv_time_ > other.GetRecvTime();
    }

 private:
    int id_;
    int root_;
    int src_, dst_, step_;
    double send_time_, recv_time_;
};

struct OutNode {
    int node_id;
    int priority = kPriority;
    int duplicate_msgs = 0;
    int received_msgs = 0;
};

class Node {
 public:
    Node(int id) : id_(id) {}

    vector<int> GetRelayList(int relay_cnts) {
        vector<int> rets;
        rets.reserve(relay_cnts);
        for (auto node : outbounds_) {
            if (relay_cnts-- <= 0) {
                break;
            }
            rets.push_back(node.first);
        }
        return rets;
    }

    int GetId() const { return id_; }

    bool IsRelay(int msg_id) const {
        return recv_msgs_.find(msg_id) != recv_msgs_.end();
    }

    static bool PriorityCompare(const OutNode& a, const OutNode& b) {
        return a.priority < b.priority;
    }

    void SetNodePriority(std::shared_ptr<Node> node, int priority) {
        std::lock_guard<std::mutex> lk(mutex_);
        int node_id = node->GetId();

        auto find_node = FindNode(node_id);
        if (find_node != nullptr)
        {
            find_node->priority = priority;
        }
        std::sort(outnodes_.begin(), outnodes_.end(), PriorityCompare);
    }

    void ReceiveMsg(Message& msg, std::shared_ptr<Node> src, std::shared_ptr<Node> dst) {
        recv_msgs_.insert(msg.GetId());
        if (src->GetId() == dst->GetId()) {
            return;
        }
        auto out = src->FindNode(msg.GetDst());
        ++out->received_msgs;
    }

    OutNode* FindNode(int node_id) {
        for (auto& node : outnodes_) {
            if (node_id == node.node_id) {
                return &node;
            }
        }
        return nullptr;
    }

    void Connect(OutNode&& out_node) {
        outnodes_.push_back(out_node);
        std::sort(outnodes_.begin(), outnodes_.end(), PriorityCompare);
    }

    map<int, OutNode> GetOutbounds() { return outbounds_; }

    std::vector<OutNode> GetOutNodes() { return outnodes_; }

 private:
    int id_;
    unordered_set<int> recv_msgs_; // 接收到的消息ID
    // map<Node*, int, compare> outbounds_; // kv: (node, priority)
    map<int, OutNode> outbounds_; // (node_id, priority)
    std::vector<OutNode> outnodes_;
    std::mutex mutex_;
};

// 邻接表
class Graph {
 public:
    explicit Graph(int n) {
        for (int i = 0; i < n; i++) {
            auto node = std::make_shared<Node>(i);
            out_bound_.push_back(node);

        }
    }

    bool AddEdge(int u, int v) {
        if (u == v) {
            return false;
        }

        auto v_node = out_bound_[u]->FindNode(v);
        if (v_node != nullptr) {
            return false;
        }

        OutNode* out_node = new OutNode();
        out_node->node_id = v;
        out_bound_[u]->Connect(std::move(*out_node));
        return true;
    }

    std::vector<OutNode> GetOutBound(int src) {
        auto bounds = out_bound_[src]->GetOutNodes();
        return bounds;
    }

    std::shared_ptr<Node> GetNode(int node_id) {
        return out_bound_[node_id];
    }

 private:
    vector<std::shared_ptr<Node>> out_bound_;
};

class graph {
  public:
    vector<int> in_bound[N];
    vector<int> out_bound[N];
    vector<pair<int, int>> new_out_bound[N]; // 连接节点，以及对应节点的优先级
    int n; 
    int m;

    graph(int _n) : n(_n) {
       m = 0; 
    }

    bool add_edge(int u, int v) {
        // avoid self-loop and duplicate edge
        if (u == v) return false;
        for (auto nb_u : out_bound[u]) 
            if (nb_u == v) 
                return false;
        out_bound[u].push_back(v);
        in_bound[v].push_back(u);
        m++;
        return true;
    }

    void del_edge(int u, int v) {
        bool ok = false;
        for (size_t i = 0; i < out_bound[u].size(); i++)
            if (out_bound[u][i] == v) {
                int len = out_bound[u].size();
                out_bound[u][i] = out_bound[u][len - 1];
                out_bound[u].pop_back();
                ok = true;
                break;
            }
        if (ok == false)
            printf("cannot del an edge\n");

        for (size_t i = 0; i < in_bound[v].size(); i++)
            if (in_bound[v][i] == u) {
                int len = in_bound[v].size();
                in_bound[v][i] = in_bound[v][len - 1];
                in_bound[v].pop_back();
                break;
            }
    }

    vector<pair<int, int>> new_outbound(int src) {
        auto v = new_outbound(src);
        return v;
    }
    vector<int> outbound(int u) {
        auto v = out_bound[u];
        return v;
    }
    vector<int> inbound(int u) {
        auto v = in_bound[u];
        return v;
    }

    void print_info() {
        double avg_outbound = 0;
        for (int i = 0; i < n; i++) {
            fprintf(stderr, "node %d's outbound:", i);
            avg_outbound += out_bound[i].size();
            for (auto j : out_bound[i])
                fprintf(stderr, " %d", j);
            fprintf(stderr, "\n");
        }
        avg_outbound /= n;
        fprintf(stderr, "%.2f\n", avg_outbound);
    }
};

int random_num(int n) {
    return rand() % n;
}

class basic_algo {
// strategy base class
//
// respond(msg): 
// one message delivered at [msg.dst],
// return the index list of its choice of relay nodes
//

  public:
    basic_algo() {}
    virtual vector<int> respond(message msg) = 0;
    virtual void set_root(int _root) {}
    //static const char* get_algo_name();
};

class BasicAlgorithm {
 public:
    virtual vector<int> Response(Message msg) {}
    virtual void SetRoot(int root) {}
    virtual void ProcessDupMsg(std::shared_ptr<Node> src_node,  std::shared_ptr<Node> dst_node) {}
    virtual void Statistical(TestResult& result) {}
    virtual void ShowStatistical() {}

    virtual std::shared_ptr<Node> GetNode(int node_id) {}

    void AddNormal(int cnts) { normal_msgs_.fetch_add(cnts); }
    void AddDup(int cnts) { dup_msgs_.fetch_add(cnts); }

    long long GetNormal() { return normal_msgs_.load(); }
    long long GetDup() { return dup_msgs_.load(); }
    long long GetTotalMsg() { return normal_msgs_.load() + dup_msgs_.load(); }

    long long CalVarintPercentage(std::vector<long>& sequence, double percentage) {
        if (percentage > 1) {
            return -1;
        }
        
        int sz = sequence.size();
        int index = sz * percentage;
        return sz == index ? sequence[index - 1] : sequence[index];
    };

    void Init() {
        normal_msgs_.store(0);
        dup_msgs_.store(0);
    }

 private:
    std::atomic<long long> normal_msgs_{0};
    std::atomic<long long> dup_msgs_{0};
};


template <int fanout = FANOUT>
class BestRelay : public BasicAlgorithm {
 public:
    BestRelay(int n, LatLonCoordinate* /* coord */, int /*root*/) {
        G = std::move(std::make_unique<Graph>(n));
        for (int src = 0; src < n; src++) {
            for (int k = 0; k < fanout; k++) {
                int dst = random_num(n);
                while (G->AddEdge(src, dst) == false) {
                    dst = random_num(n);
                }
            }
        }
    }
  // 返回转发的节点
  vector<int> Response(Message msg) override {
    
    auto relay_cnts = RelayNodeCnts(msg.GetStep());
    auto src = G->GetNode(msg.GetDst());

    vector<int> dsts;
    for (auto& node : src->GetOutNodes()) {
        if (relay_cnts-- < 0) {
            break;
        }
        dsts.push_back(node.node_id);
    }
    return dsts;
  }

  // 调整出度节点的优先级
  void ProcessDupMsg(std::shared_ptr<Node> src_node,  std::shared_ptr<Node> dst_node) override {
    int out_node_id = dst_node->GetId();
    auto out_node = src_node->FindNode(out_node_id);
    if (out_node == nullptr) {
        return;
    }
    ++out_node->duplicate_msgs;
    // 根据接受消息和重复消息的比例改变节点转发优先级
    if (out_node->received_msgs == 0) {
        return;
    }

    if (static_cast<double>(out_node->duplicate_msgs) / out_node->received_msgs > kAdjustPriority) {
        src_node->SetNodePriority(dst_node, --out_node->priority);
    }
  }

  // 根据message传播步数，计算转发的节点数量
  int RelayNodeCnts(int step) {
    if (step <= 2) {
        return FANOUT;
    }

    return std::max(Log(step), 2);
  }

  std::shared_ptr<Node> GetNode(int node_id) {
    return G->GetNode(node_id);
  }

  void Statistical(TestResult& result) override {
    auto avg_latency = result.GetReceiveTime() / result.GetReceiveNode();
     auto sequence = result.GetSequence();
     std::sort(sequence.begin(), sequence.end());
    varint_percentage_.Eighty = CalVarintPercentage(sequence, kEighty);
    varint_percentage_.Ninty = CalVarintPercentage(sequence, kNinty);
    varint_percentage_.All = CalVarintPercentage(sequence, 1.0);
    if (statistic_data_.size() > 100) {
        statistic_data_.pop();
    }
    statistic_data_.push({statistic_cnts++, avg_latency});
  }

    void ShowStatistical() override {
        std::cout << "*** " << algo_name << " ***" << std::endl;
        double total_latency = 0;
        int simulate_cnts = statistic_data_.size();
        while (!statistic_data_.empty()) {
            total_latency += statistic_data_.top().second;
            statistic_data_.pop();
        }
        std::cout << "avg_latency: " << std::fixed << 
        total_latency / simulate_cnts << ", avg_bandwith: " 
        << static_cast<double>(GetNormal() + GetDup()) / GetNormal() << std::endl;

        std::cout << varint_percentage_ << std::endl;
    }

private:
  std::unique_ptr<Graph> G;
  static constexpr const char *algo_name = "best_relay";
  VarintPercentage varint_percentage_;
  std::priority_queue<std::pair<int, double>>statistic_data_;
  int statistic_cnts = 0;
  static bool compare(pair<int, int> &a, pair<int, int> &b) {
    return a.second > b.second;
  }

  // n为底，m的对数
  int Log(int m, double n = 0.5) {
    return std::log(m) / std::log(n) + FANOUT;
  }

};

template <int root_fanout = ROOT_FANOUT, int second_fanout = SECOND_FANOUT, int fanout = FANOUT>
class RandomFlood : public BasicAlgorithm {
 public:
    RandomFlood(int n, LatLonCoordinate* /* coord */, int /* root */) {
        G = std::move(std::make_unique<Graph>(n));
        for (int src = 0; src < n; src++) {
            for (int k = 0; k < fanout; k++) {
                int dst = random_num(n);
                while (G->AddEdge(src, dst) == false) {
                    dst = random_num(n);
                }
            }
        }
    }

    vector<int> Response(Message msg) override {
        int u = msg.GetDst();

        vector<int> ret;
        auto outbound = G->GetOutBound(u);
        for (auto& node : outbound) {
            if (node.node_id != msg.GetSrc()) {
                ret.push_back(node.node_id);
            }
        }
        return ret;
    }

    std::shared_ptr<Node> GetNode(int node_id) {
        return G->GetNode(node_id);
    }

    void Statistical(TestResult& result) override {
        auto avg_latency = result.GetReceiveTime() / result.GetReceiveNode();
        auto sequence = result.GetSequence();
        std::sort(sequence.begin(), sequence.end());
        varint_percentage_.Eighty = CalVarintPercentage(sequence, kEighty);
        varint_percentage_.Ninty = CalVarintPercentage(sequence, kNinty);
        varint_percentage_.All = CalVarintPercentage(sequence, 1.0);
        statistic_data_.push_back(avg_latency);
    }

    void ShowStatistical() override {
        if (statistic_data_.empty()) {
            return;
        }
        std::cout << "*** " << algo_name << " ***" << std::endl;
        std::cout << "avg_latency: " << std::fixed << 
        std::accumulate(statistic_data_.begin(), statistic_data_.end(), 0) / statistic_data_.size() 
        << ", avg_bandwith: " << static_cast<double>(GetNormal() + GetDup()) / GetNormal() << std::endl;
        std::cout << varint_percentage_ << std::endl;
    }

 private:
    std::unique_ptr<Graph> G;
    static constexpr const char* algo_name = "random_flood";
    int tree_root;
    std::vector<double> statistic_data_;
    VarintPercentage varint_percentage_;
};

template<int root_fanout = ROOT_FANOUT, int second_fanout = SECOND_FANOUT, int fanout = FANOUT>
class random_flood : public basic_algo {

// random flood : 
// 1. Connet the graph as a ring to prevent partition
// 2. Every node selects other 7 random outbounds

  private: 
    graph G; // random graph
    static constexpr const char* algo_name = "random_flood";
    int tree_root;
  public:
    const static bool specified_root = true;
    random_flood(int n, LatLonCoordinate *coord, int root = 0) : G(n) {
        tree_root = root;
        // firstly connect a ring, then random connect

        for (int u = 0; u < n; u++) {
            int dg = fanout;
            for (int k = 0; k < dg; k++) {
                int v = random_num(n);
                while (G.add_edge(u, v) == false)
                    v = random_num(n);
            }
        }
    }

    vector<int> respond(message msg)  {
        // Directly return all [msg.dst]'s outbounds.

        int u = msg.dst;
        vector<int> nb_u = G.outbound(u);
        vector<int> ret;
        for (auto v : nb_u) 
            if (v != msg.src) 
                ret.push_back(v);
  
        if (u == tree_root) {
            int remain_deg = root_fanout - ret.size();
            for (int i = 0; i < remain_deg; i++) {
                int v = rand() % n;
                if (v != msg.src) 
                    ret.push_back(v);
            }
        }
        return ret;
    }

    static const char* get_algo_name() {return algo_name;} 
    void print_info() {
        G.print_info();
    }
    static int get_fanout() {return root_fanout;}
};

// the difference of lontitudes should be in the range of [-180, 180]
double fit_in_a_ring(double x)  {
    if (x < -180) x += 360;
    if (x > 180) x -= 360;
    return x;
}

// the angle between the vector r ---> u and u ---> v should be in [-90, 90]
// notice: simply use (lontitude, latitude) as the normal 2-d (x, y) coordinate
bool angle_check(const LatLonCoordinate &r, const LatLonCoordinate &u, const LatLonCoordinate &v) {
    double x1 = u.lon - r.lon, y1 = u.lat - r.lat;
    double x2 = v.lon - u.lon, y2 = v.lat - u.lat;
    x1 = fit_in_a_ring(x1);
    x2 = fit_in_a_ring(x2);

    // get the vertical vector of (u - r)
    double x3 = y1, y3 = -x1;

    // use cross dot to check the angle
    return (x3 * y2 - x2 * y3) > -1e-3;
}


template<int root_deg = ROOT_FANOUT, int second_deg = SECOND_FANOUT, int normal_deg = FANOUT>
class static_build_tree : public basic_algo {
// static_build_tree:
// Suppose the broadcast root is [root].
// Firstly, sort the nodes based on the distance between them and [root].
// The sorted list is [list].
//
// Build the broadcast tree as following rules:
// For the node, u = list[i]
// The father should be:
//    v in list[0...i-1]:
//    minimize: tree_distance(root, v) + distance(v, u)
//    subject to: out_bound[v] < 8 

  private: 
    graph G; // random graph
    static constexpr const char* algo_name = "static_build";
    double dist[N];
    int out_bound_cnt[N];
    int list[N];
    int depth[N];
  
  public: 
    const static bool specified_root = true;
    static_build_tree(int n, LatLonCoordinate *coord, int root = 0) : G(n) {
        memcle(dist);
        memcle(out_bound_cnt);
        memcle(list);
        memcle(depth);

        vector<pair<double, int> > rk;
        for (int j = 0; j < n; j++) 
            if (j != root) 
                rk.push_back(make_pair(distance(coord[root], coord[j]), j));
        sort(rk.begin(), rk.end());
        list[0] = root;
        for (int j = 1; j < n - 1; j++) 
            list[j] = rk[j - 1].second;

        for (int i = 0; i < n - 1; i++) {
            int u = list[i + 1];

            double cur_min = 1e100;
            int cur_parent = 0;
            for (int j = 0; j <= i; j++) {
                int v = list[j];
                if ((v == root && out_bound_cnt[v] < root_deg) || (out_bound_cnt[v] < normal_deg && dist[v] + distance(coord[u], coord[v]) + FIXED_DELAY < cur_min)) {
                    cur_min = distance(coord[u], coord[v]) + dist[v] + FIXED_DELAY;
                    cur_parent = v;
                }
            }
            // set parent of u 
            G.add_edge(cur_parent, u);
            dist[u] = cur_min;
            out_bound_cnt[cur_parent]++;
        }

        printf("root deg = %d\n", out_bound_cnt[root]);
    }

    vector<int> respond(message msg)  {
        int u = msg.dst;
        vector<int> nb_u = G.outbound(u);
        vector<int> ret;
        for (auto v : nb_u) 
            if (v != msg.src) 
                ret.push_back(v);
        return ret;
    }

    static const char* get_algo_name() {return algo_name;} 
    void print_info() {
        G.print_info();
    }
};

const static int D = 3;
const static int COORDINATE_UPDATE_ROUND = 100;
/// @brief vivalModel集合，一个节点对应一个vivalModel
VivaldiModel<D> vivaldi_model[N];

void generate_random_virtual_coordinate() {
    for (int i = 0; i < n; i++) {
        vivaldi_model[i] = VivaldiModel<D>(i);
        double tmp[2] = {random_between_0_1() * 1000, random_between_0_1() * 1000};
        vivaldi_model[i].local_coord = Coordinate<D>(tmp, 0, 0.1);
    }
}

void generate_virtual_coordinate() {
    // init，为每个vivaldiModel标号
    for (int i = 0; i < n; i++)
        vivaldi_model[i] = VivaldiModel<D>(i);
    // 100轮更新达到坐标稳定
    for (int round = 0; round < COORDINATE_UPDATE_ROUND; round++) {
        //printf("%d\n", round);
        for (int x = 0; x < n; x++) {
            vector<int> selected_neighbor;
            // 随机选择邻居节点
            if (vivaldi_model[x].have_enough_peer) {
                for (auto &y: vivaldi_model[x].random_peer_set)
                    selected_neighbor.push_back(y);
            } else {
                for (int j = 0; j < 16; j++) {
                    int y = rand() % n;
                    while (y == x) y = rand() % n;
                    selected_neighbor.push_back(y);
                }
            }

            for (auto y: selected_neighbor)
            {
                // 根据与邻居节点的距离更新自身坐标
                double rtt = distance(coord[x], coord[y]) + FIXED_DELAY;
                vivaldi_model[x].observe(y, vivaldi_model[y].coordinate(), rtt);
            }
        }
    }

}

const static int max_iter = 100;
int cluster_cnt[K];
int cluster_result[N];
vector<int> cluster_list[K];

// k_means: 8 cluster
// cluster_cnt[i]: #nodes in cluster i
// cluster_list[i]: list of nodes in cluster i
// cluster_result[u]: u belongs to this cluster

void k_means() {
    srand(11);
    memcle(cluster_cnt);
    memcle(cluster_result);
    LatLonCoordinate center[K];
    LatLonCoordinate avg[K];
    vector<int> tmp_list;

    for (int i = 0; i < K; i++) {
        while (true) {
            int u = random_num(n);
            //int u = i;
            if (find(tmp_list.begin(), tmp_list.end(), u) == tmp_list.end()) {
                center[i] = coord[u];
                tmp_list.push_back(u);
                break;
            }
        }
    }

    // K means
    for (int iter = 0; iter < max_iter; iter++) {

        // find the nearest center
        for (int i = 0; i < n; i++) {
            double dist = 1e100;
            int cur_cluster = 0;
            for (int j = 0; j < K; j++)
                if (distance(center[j], coord[i]) < dist) {
                    dist = distance(center[j], coord[i]);
                    cur_cluster = j;
                }
            cluster_result[i] = cur_cluster;
        }

        // re-calculate center
        memcle(avg);
        memcle(cluster_cnt);
        for (int i = 0; i < n; i++) {
            avg[cluster_result[i]].lon += coord[i].lon;
            avg[cluster_result[i]].lat += coord[i].lat;
            cluster_cnt[cluster_result[i]]++;
        }
        for (int i = 0; i < K; i++) 
            if (cluster_cnt[i] > 0) {
                center[i].lon = avg[i].lon / cluster_cnt[i];
                center[i].lat = avg[i].lat / cluster_cnt[i];
            }
    }


    for (int i = 0; i < K; i++)
        cluster_list[i].clear();

    for (int i = 0; i < n; i++) 
        cluster_list[cluster_result[i]].push_back(i);
    
    printf("cluster result \n");
    for (int i = 0; i < K; i++)
        printf("%lu ", cluster_list[i].size());
    printf("\n");
}
void k_means_based_on_virtual_coordinate() {
    srand(13);
    memcle(cluster_cnt);
    memcle(cluster_result);

    EuclideanVector<D> center[K];
    EuclideanVector<D> avg[K];
    vector<int> tmp_list;
    // 随机选择K个中心点
    for (int i = 0; i < K; i++) {
        while (true) {
            int u = random_num(n);
            if (find(tmp_list.begin(), tmp_list.end(), u) == tmp_list.end()) {
                center[i] = vivaldi_model[u].vector();
                tmp_list.push_back(u);
                break;
            }
        }
    }

    // K means
    for (int iter = 0; iter < max_iter; iter++) {

        // find the nearest center，根据节点与每个cluster中心点的距离将每个节点聚类到每个cluster
        for (int i = 0; i < n; i++) {
            double dist = 1e100;
            int cur_cluster = 0;
            for (int j = 0; j < K; j++)
                if (distance(center[j], vivaldi_model[i].vector()) < dist) {
                    dist = distance(center[j], vivaldi_model[i].vector());
                    cur_cluster = j;
                }
            cluster_result[i] = cur_cluster;
        }

        // re-calculate center
        memcle(avg);
        memcle(cluster_cnt);
        for (int i = 0; i < n; i++) {
            avg[cluster_result[i]] = avg[cluster_result[i]] + vivaldi_model[i].vector();
            cluster_cnt[cluster_result[i]]++;
        }
        for (int i = 0; i < K; i++) 
            if (cluster_cnt[i] > 0) {
                center[i] = avg[i] / cluster_cnt[i];
            }
    }

    //for (int i = 0; i < n; i++)
    //    printf("%d ", cluster_result[i]);
    //printf("\n");

    for (int i = 0; i < K; i++)
        cluster_list[i].clear();

    for (int i = 0; i < n; i++) 
        cluster_list[cluster_result[i]].push_back(i);

    printf("cluster result \n");
    for (int i = 0; i < K; i++)
        printf("%lu ", cluster_list[i].size());
    printf("\n");
}

/// @brief 输出step数据到指定文件
/// @param output_filename 
void write_steps_data(string output_filename) {
    ofstream outfile;
    outfile.open(output_filename, ios::out);

    for (int i=0; i<n; i++) {
        if (!recv_step[i]) continue;
        outfile << recv_step[i] << endl;
    }
    outfile << "---end---" << endl;
    outfile.close();
}
void write_dupmsg(string output_file = "dupmsg.dt") {
    ofstream outfile;
    outfile.open(output_file);
    for (auto [step, cnts] : dup_msg_cnts) {
        outfile << step << " " << cnts << endl;
    }
    outfile << "---end---" << endl;
    outfile.close();
}
/// @brief 
/// @tparam root_fanout 128 
/// @tparam second_fanout 8
/// @tparam fanout 8
/// @tparam enable_nearest true 
/// @tparam worst_attack 
template <int root_fanout = ROOT_FANOUT, int second_fanout = SECOND_FANOUT, int fanout = FANOUT, bool enable_nearest = false, bool worst_attack = false>
class k_means_cluster : public basic_algo {
// k_means_cluster:
// firstly build K clusters (K = 8)
// For the [root], it randomly connects to root_deg_per_cluster nodes in every cluster. (1, 2, 4...)
// For other nodes, they randomly connects to 4 nodes in the same cluster and 4 nodes in other clusters.

  private: 
    graph G; // random graph
    graph G_near;
    const int random_out = 4;
    static constexpr const char* algo_name = "cluster";
    mt19937 rng;

  public: 
    const static bool specified_root = true;
    k_means_cluster(int n, LatLonCoordinate *coord, int root = 0) : G(n), G_near(n), rng(100) {
        
        for (int i = 0; i < n; i++)  {
            int c = cluster_result[i];
            // 4 out_bound in the same cluster
            int inner_deg = INNER_DEG;
            // 随机选取4个同一集群中的节点连接
            if (vivaldi_model[i].coordinate().error() < 0.4) {
                if (cluster_cnt[c] <= inner_deg + 1) {
                    for (int j : cluster_list[c])
                        if (i != j)
                            G.add_edge(i, j);
                } else {
                    int deg = inner_deg;
                    vector<pair<double, int> > cluster_peer;
                    for (int trial = 0, cnt = 0; trial < 100 && cnt < deg; trial++) {
                        int j = cluster_list[c][random_num(cluster_cnt[c])];
                        int j1 = cluster_list[c][random_num(cluster_cnt[c])];
                        if (distance(vivaldi_model[i].vector(), vivaldi_model[j].vector()) > 
                            distance(vivaldi_model[i].vector(), vivaldi_model[j1].vector()))
                                j = j1;
                        if (i != j) {
                            double dist = distance(vivaldi_model[i].vector(), vivaldi_model[j].vector());
                            cluster_peer.push_back(make_pair(dist, j));
                            cnt += 1;
                        }
                    }
                    sort(cluster_peer.begin(), cluster_peer.end());
                    for (int j = 0, cnt = 0; j < cluster_peer.size() && cnt < deg; j++) {
                        if (G.add_edge(i, cluster_peer[j].second)) {
                            cnt += 1;
                        }
                    }
                }

            }

            // build the near graph 每个节点与所在集群中最近的4个节点相连
            if (vivaldi_model[i].coordinate().error() < 0.4) {
                vector<pair<double, int> > nearest_peer;
                for (int j : cluster_list[c]) {
                    if (i != j) {
                        double dist = distance(vivaldi_model[i].vector(), vivaldi_model[j].vector());
                        nearest_peer.push_back(make_pair(dist, j));
                        for (int k = nearest_peer.size() - 1; k > 0; k--) {
                            if (nearest_peer[k - 1].first > nearest_peer[k].first) 
                                swap(nearest_peer[k - 1], nearest_peer[k]);
                            else 
                                break;
                        }
                        if (nearest_peer.size() > inner_deg) {
                            nearest_peer.pop_back();
                        }
                    }
                }

                for (auto pr: nearest_peer) {
                    //printf("near peer : (%d %d) %.3f\n", i, pr.second, pr.first);
                    G_near.add_edge(i, pr.second);
                }
            }
        }
    }
        
    vector<int> respond(message msg)  {
        int u = msg.dst;
        vector<int> nb_u = G.outbound(u);
        vector<int> ret;

        // 消息来自别的集群或者刚由客户端发出
        if (enable_nearest && (cluster_result[msg.src] != cluster_result[u] || msg.step == 0 || msg.recv_time - msg.send_time > 100)) {
            int cnt = 0;
            for (auto v : G_near.out_bound[u]) {
                if (v != msg.src) {
                    ret.push_back(v);
                    cnt++;
                }
            }
        } 
        else {
            int cnt = 0;
            for (auto v : nb_u) 
                if (v != msg.src) {
                    ret.push_back(v);
                    cnt++;
                }
        }

        int remain_deg = 0;
        if (msg.step == 0) {
            remain_deg = root_fanout - ret.size();
        } else if (msg.step == 1) {
            remain_deg = second_fanout - ret.size();
        } else {
            remain_deg = fanout - ret.size();
        }

        // if (msg.step == 0) {
        //     remain_deg = root_fanout - ret.size();
        // } else {
        //     // remain_deg = root_fanout / (msg.step + second_fanout * 2 + 2) - ret.size();
        //     remain_deg = root_fanout / (second_fanout * 2 + log2(msg.step)) - ret.size();
        // }

        // !!!!!!!!!!!!!!!!!
        // If worst_attack happens, we assume all the peer selection related to distance/coordinate/clustering fails
        if (worst_attack == true) {
            ret.clear();
        }

        //printf("remain deg %d\n", remain_deg);

        for (int i = 0; i < remain_deg; i++) {
            int v = rng() % n;
            if (u != v && std::find(ret.begin(), ret.end(), v) == ret.end()) {
                ret.push_back(v);
            }
        }
        return ret;
    }

    static const char* get_algo_name() {return algo_name;} 
    void print_info() {
        G.print_info();
    }
};



class perigee_observation {
  public:
    vector<double> obs; // the time difference
    int u; // src
    int v; // dst

    perigee_observation() {}
    perigee_observation(int _u, int _v) {
        init(_u, _v);
    }
    void init(int _u, int _v) {
        u = _u;
        v = _v;
        obs.clear();
    }

    void add(double t) {
        if (t < 0) {
            printf("t = %.2f\n", t);
            printf("(%d, %d)\n", u, v);
        }
        obs.push_back(t);
    }

    pair<double, double> get_lcb_ucb() {
        int len = obs.size();
        if (len == 0) {
            return make_pair(1e10, 1e10);
        }
        int pos = int(len * 0.9);
        //use fast selection to avoid sorting
        nth_element(obs.begin(), obs.begin() + pos, obs.end()); 

        double per90obs = obs[pos];
        double bias = 125.0 * sqrt(log(len) / (2 * len));
        return make_pair(per90obs - bias, per90obs + bias);
    }
};

template<int root_fanout = ROOT_FANOUT, int fanout = FANOUT, int max_outbound = MAX_OUTBOUND>
class perigee_ubc : public basic_algo {
// perigee_ubc
// https://arxiv.org/pdf/2006.14186.pdf
// Firstly, execute warmup phase for 640 random messages.
// For an edge (u, v), node v will maintain an observation array O.
// When u is sending a message m to v, v will store the timestamp of the 
// receiving time T(u, v, m), and the time difference since v firstly sees the message: 
// T(u, v, m)  - min_u' T(u', v, m)
// For every 10 message, every nodes updates their outbound based on the UBC method

  private: 
    mt19937 rng;
    graph G; // random graph
    //static constexpr int deg = 8;
    static constexpr const char* algo_name = "perigee_ubc";
    //perigee_observation obs[N][deg];
    vector<unique_ptr<perigee_observation> > obs[N];

    // use for warmup phase
    static constexpr int total_warmup_message = 640;
    static constexpr int warmup_round_len = 10; // for every 10 message, execute a reselection
    int recv_flag[N]; // keep track of the newest warmup message token
    double recv_time[N];  // record the new message deliever time

  public: 
    const static bool specified_root = false;
    perigee_ubc(int n, LatLonCoordinate *coord, int root = 0) : rng(root), G(n) {
        for (int u = 0; u < n; u++) {
            int dg = fanout - INNER_DEG;
            //if (u == root)
            //    dg = 32 - 1;
            // should reverse the connection
            for (int k = 0; k < dg; k++) {
                int v = random_num(n);
                while (G.add_edge(u, v) == false)
                    v = random_num(n);
            }
        }

        // TODO: inbound has far more than 8
        for (int u = 0; u < n; u++) {
            int dg = INNER_DEG;
            //if (u == root)
            //    dg = 32 - 1;
            // should reverse the connection
            for (int k = 0; k < dg; k++) {
                int v = random_num(n);
                while (G.add_edge(u, v) == false)
                    v = random_num(n);

                //obs[v][k].init(u, v);
                if (obs[v].size() < INNER_DEG) {
                    unique_ptr<perigee_observation> ptr(new perigee_observation(u, v));
                    obs[v].push_back(move(ptr));
                }
            }
        }

        //warmup phase
        memset(recv_flag, -1, sizeof(recv_flag));

        for (int warmup_message = 0; warmup_message < total_warmup_message; warmup_message++) {

            int root = random_num(n);

            priority_queue<message, vector<message>, greater<message> > msg_queue;
            msg_queue.push(message(root, root, root, 0, 0, 0)); // initial message

            for (; !msg_queue.empty(); ) {
                message msg = msg_queue.top();
                msg_queue.pop();

                int u = msg.dst; // current node

                // a new message
                if (recv_flag[u] < warmup_message) {
                    recv_flag[u] = warmup_message;
                    recv_time[u] = msg.recv_time;

                    {
                    //if (mal_flag[u] == false) {
                        auto relay_list = respond(msg);
                        double delay_time = 0;
                        if (u == root) delay_time = 0;
                        for (auto v : relay_list) {
                            double dist = distance(coord[u], coord[v]) * 3 + FIXED_DELAY; 
                            message new_msg = message(root, u, v, msg.step + 1, recv_time[u] + delay_time, recv_time[u] + dist + delay_time);
                            msg_queue.push(new_msg);
                        }
                    }

                } 
                // add observation, find the corresponding queue
                for (auto &it: obs[u]) 
                    if (it -> u == msg.src) 
                        it -> add(msg.recv_time - recv_time[u]);
            }

            if ((warmup_message + 1) % warmup_round_len == 0) {
                //printf("%d\n", warmup_message);
                int kill_cnt = 0;
                for (int i = 0; i < n; i++)  {
                    if (neighbor_reselection(i) == 1) {
                        //printf("obs size %d\n", obs[i].size());
                        kill_cnt += 1;
                    }
                }
                //printf("round = %d, kill = %d\n", warmup_message / warmup_round_len, kill_cnt);
                //printf("finish\n");
            }
        }

        for (int u = 0; u < n; u++) {
            int dg = max_outbound - G.out_bound[u].size();
            for (int k = 0; k < dg; k++) {
                int v = random_num(n);
                while (G.add_edge(u, v) == false)
                    v = random_num(n);
            }
        }

        double out_bound_pdf[100];
        double avg_outbound = 0;

        memcle(out_bound_pdf);
        for (int i = 0; i < n; i++) {
            size_t s = G.out_bound[i].size();
            out_bound_pdf[s] += 1.0;
            avg_outbound += s;
        }

        avg_outbound /= n;
        printf("avg_outbound = %.3f\n", avg_outbound);

        //for (int i = 0; i < 20; i++) {
        //    out_bound_pdf[i] /= n;
        //    printf("outbound[%d] = %.3f\n", i, out_bound_pdf[i]);
        //}
    }

    // if reselect -- return 1
    int neighbor_reselection(int v) {
        double max_lcb = 0;
        int arg_max_lcb = 0;
        double min_ucb = 1e18;
        int arg_min_ucb = 0;

        for (size_t i = 0; i < obs[v].size(); i++) {
            auto lcb_ucb = obs[v][i] -> get_lcb_ucb();
            if (lcb_ucb.first > max_lcb) {
                arg_max_lcb = i;
                max_lcb = lcb_ucb.first;
            }
            if (lcb_ucb.second < min_ucb) {
                arg_min_ucb = i;
                min_ucb = lcb_ucb.second;
            }
        }

        if (max_lcb > min_ucb) {
            int u = obs[v][arg_max_lcb] -> u;
            //auto lcb_ucb = obs[v][arg_max_lcb] -> get_lcb_ucb();
            //int len = obs[v][arg_max_lcb] -> obs.size();

            //auto bst = obs[v][arg_min_ucb] -> get_lcb_ucb();
            //int bst_u = obs[v][arg_min_ucb] -> u;
            //printf("best (%.2f %.2f) (%d, %d), distance = %.2f\n", bst.first, bst.second, bst_u, v, distance(coord[bst_u], coord[v]));
            //printf("worst (%.2f %.2f) (%d, %d), distance = %.2f\n", lcb_ucb.first, lcb_ucb.second, u, v, distance(coord[u], coord[v]));
            G.del_edge(u, v);

            int new_u = random_num(n);
            while (G.out_bound[new_u].size() >= max_outbound || G.add_edge(new_u, v) == false)
                new_u = random_num(n);

            obs[v][arg_max_lcb].reset(new perigee_observation(new_u, v));
            return 1;
        }
        return 0;
    }
        
    vector<int> respond(message msg)  {
        int u = msg.dst;
        vector<int> nb_u = G.outbound(u);
        vector<int> ret;
        int cnt = 0;
        for (auto v : nb_u) 
            if (v != msg.src) {
                ret.push_back(v);
                cnt++;
            }

        if (msg.step == 0) {
            //mt19937 rng(u);
            int remain_deg = root_fanout - ret.size();
            for (int i = 0; i < remain_deg; i++) {
                int v = rng() % n;
                if (u != v && std::find(ret.begin(), ret.end(), v) == ret.end()) {
                    ret.push_back(v);
                }
            }
        }
        return ret;
    }

    static const char* get_algo_name() {return algo_name;} 
    void print_info() {
        G.print_info();
    }
};



template<int fanout = FANOUT>
class block_p2p : public basic_algo {
// block p2p
// firstly build K clusters (K = 8)
// Inside a cluster, it connects Chord-type graph
// Every cluster has one entry point. One entry point connects to all other entry points.

  private: 
    graph G; // random graph
    static constexpr int random_out = fanout / 2;
    static constexpr int dist_out = fanout - random_out;
    static constexpr const char* algo_name = "blockp2p";


  public: 
    const static bool specified_root = false;
    block_p2p(int n, LatLonCoordinate *coord, int root = 0) : G(n) {
        // the first node in every cluster's list is the entry points
        for (int i = 0; i < K; i++)
            for (int j = 0; j < K; j++)
                if (i != j) {
                    G.add_edge(cluster_list[i][0], cluster_list[j][0]);
                }
            
        // connect a Chord-type graph
        for (int i = 0; i < K; i++) {
            int cn = cluster_cnt[i];
            for (int j = 0; j < cn; j++) {
                int u = cluster_list[i][j];
                if (cn <= 8) {
                    // if the cluster size is small, connect it as a fully-connected graph
                    for (auto v : cluster_list[i])
                        if (u != v)
                            G.add_edge(u, v);
                } else {
                    // Chord-type graph
                    for (int k = 1; k < cn; k *= 2)  
                        G.add_edge(u, cluster_list[i][(j + k) % cn]); // connect u and (u + 2^k) mod cn
                    G.add_edge(u, cluster_list[i][(j + cn / 2) % cn]); // connect the diagonal
                }

            }
        }
    }
        
    vector<int> respond(message msg)  {
        int u = msg.dst;
        vector<int> nb_u = G.outbound(u);
        vector<int> ret;
        //int cnt = 0;
        for (auto v : nb_u) 
            if (v != msg.src) {
                ret.push_back(v);
            }
        return ret;
    }

    static const char* get_algo_name() {return algo_name;} 
    void print_info() {
        G.print_info();
    }
};



class test_result {
  public : 
    double avg_bnd;
    double avg_latency;
    vector<double> latency;
    double depth_cdf[MAX_DEPTH];
    double avg_dist[MAX_DEPTH];

    vector<double> cluster_avg_latency;
    vector<double> cluster_avg_depth;

    test_result() : avg_bnd(0), avg_latency(0), latency(21, 0), 
        cluster_avg_latency(21, 0),
        cluster_avg_depth(21, 0) {
        memcle(depth_cdf);
        memcle(avg_dist);
    }
    void print_info() {
        fprintf(stderr, "bandwidth");
        for (int i = 0; i < 21; i++)
            fprintf(stderr, ", %.2f", i * 0.05);
        fprintf(stderr, "\n");
        fprintf(stderr, "%.2f", avg_bnd);
        for (int i = 0; i < 21; i++)
            fprintf(stderr, ", %.2f", latency[i]);
        fprintf(stderr, "\n");
    }
};

template <class algo_T>
test_result single_root_simulation(int root, int rept_time, double mal_node, shared_ptr<algo_T> algo) {
// Test the latency of the message originated from [root].
// 1) Use a global heap to maintain the message queue and fetch the next delivered message.
// 2) For every delivered message, ignore it if it is a duplicated message.
// 3) Otherwise, invoke algo_T's respond function to get the relay node list.
// 4) Then create new messages to the relay nodes.

// Delay model: 
// When a node receives a message, it has 50ms delay to handle it. Then it sends to other nodes without delay.

    std::default_random_engine generator;
    std::normal_distribution<double> rand_latency(50.0, 10.0);

    test_result result;

    // initialize test algorithm class if it needs a specific root
    if (algo_T::specified_root == true) {
        algo.reset(new algo_T(n, coord, root));
    }
    algo -> set_root(root);
    
    FILE* dupOutput = fopen("dup_ouput.txt", "a");
    for (int rept = 0; rept < rept_time; rept++)  {

        priority_queue<message, vector<message>, greater<message> > msg_queue;
        msg_queue.push(message(root, root, root, 0, 0, 0)); // initial message

        memcle(recv_step);
        memcle(recv_flag);
        memcle(recv_time);
        memcle(recv_dist);
        memset(recv_parent, -1, sizeof(recv_parent));
        memcle(depth);
        dup_msg_cnts.clear();
        vector<int> recv_list;

        int dup_msg = 0;

        for (; !msg_queue.empty(); ) {
            message msg = msg_queue.top();
            msg_queue.pop();

            int u = msg.dst; // current node


            // duplicate msg -- ignore
            if (recv_flag[u] == true) {
                dup_msg++;
                dup_msg_cnts[msg.step + 1]++;
                continue;
            }
            //msg.print_info();
            if (msg.step == 0) cout << msg.step << endl;
            recv_step[u] = msg.step + 1; // 记录节点在第几步接收到交易

            recv_flag[u] = true;
            recv_time[u] = msg.recv_time;
            recv_dist[u] = msg.recv_time - msg.send_time;
            recv_parent[u] = msg.src;
            recv_list.push_back(u);
            if (u != root)
                depth[u] = depth[msg.src] + 1; 

            // malicious node -- no response
            if (mal_flag[u] == true) continue;

            auto relay_list = (*algo).respond(msg); // 计算转发交易节点列表

            double delay_time = (FIXED_DELAY - 50) + std::min(std::max(rand_latency(generator), 0.0), 100.0);  // avg is 250ms, in simulation: make it 200ms + Gaussian(50, 10)
            for (auto v: relay_list) {
                double dist = distance(coord[u], coord[v]) * 3;
                if (msg.step == 0) {
                    dist = distance(coord[u], coord[v]) * 3;
                }
                message new_msg = message(root, u, v, msg.step + 1, recv_time[u] + delay_time, recv_time[u] + dist + delay_time);
                msg_queue.push(new_msg);
            }
        }
        fprintf(dupOutput, "dupMsg %d at %d test\n", dup_msg, testid++);
        // write_steps_data("normal_stepdata.dt");
        write_steps_data("less_stepdata.dt");

        if (algo_T::get_algo_name() == "random_flood")
        write_dupmsg(to_string(algo_T::get_fanout()));

        int cluster_recv_count[10];
        memcle(cluster_recv_count);

        int recv_count = 0;
        double avg_latency = 0;

        for (int i = 0; i < n; i++)
            if (recv_flag[i] == false && mal_flag[i] == false) {
                //printf("not receive %d\n", i);
                recv_time[i] = inf;
                recv_list.push_back(i);
                depth[i] = MAX_DEPTH - 1; //uncovered node
            } else {
                recv_count++;
                avg_latency += recv_time[i];

                int c = cluster_result[i];
                cluster_recv_count[c]++;
                result.cluster_avg_depth[c] += depth[i];
                result.cluster_avg_latency[c] += recv_time[i];
            }

        avg_latency /= recv_count; // 平均延迟 = 所有节点接收到该msg的时间之和 / 节点数
        for (int c = 0; c < K; c++) {
            result.cluster_avg_depth[c] /= cluster_recv_count[c];
            result.cluster_avg_latency[c] /= cluster_recv_count[c];
        }


        int non_mal_node = recv_list.size();
        result.avg_bnd += (double(dup_msg + non_mal_node) / (non_mal_node)); // 平均带宽 = (重复消息数 + 正常节点数) / 正常节点数
        int depth_cnt[100];
        memcle(depth_cnt);

        for (int u: recv_list) {
            result.depth_cdf[depth[u]] += 1;
            result.avg_dist[depth[u]] += recv_dist[u];
            depth_cnt[depth[u]] += 1;
        }

        result.avg_latency = avg_latency;

        for (int i = 0; i < MAX_DEPTH; i++) {
            result.depth_cdf[i] /= non_mal_node;
            result.avg_dist[i] /= depth_cnt[i];
        }

        int cnt = 0;
        for (double pct = 0.05; pct <= 1; pct += 0.05, cnt++) {
            int i = non_mal_node * pct;
            result.latency[cnt] += recv_time[recv_list[i]];
        }

    }

    fclose(dupOutput);
    result.avg_bnd /= rept_time; 
    for (int i = 0; i < MAX_DEPTH; i++) 
        result.depth_cdf[i] /= rept_time; 
    for (size_t i = 0; i < result.latency.size(); i++) {
        double tmp = int(result.latency[i] / inf);
        result.latency[i] -= tmp * inf;
        if (rept_time - tmp == 0)
            result.latency[i] = 0;
        else
            result.latency[i] /= (rept_time - tmp);

        if (result.latency[i] < 0.1)
            result.latency[i] = inf;
    }


    // Print the tree structure (only when the root is 0)

    if (algo_T::get_algo_name() == "cluster") {
        FILE* pf = fopen("tree_struct.txt", "w");
        if (pf != NULL) {
            fprintf(pf, "%d %d\n", n, root);
            for (int i = 0; i < n; i++) {
                fprintf(pf, "%d\n", recv_parent[i]);
            }
        } else 
            fprintf(stderr, "cannot open tree_struct.txt\n");
    }
              
    return result;
}

// 一个消息传播全网
template <class algo_T>
TestResult Simulation(int root, int rept_time, shared_ptr<algo_T> algo) {
    TestResult result;
    std::default_random_engine generator;
    std::normal_distribution<double> rand_latency(50.0, 10.0);

    for (int rept = 0; rept < rept_time; rept++) {
        priority_queue<Message, vector<Message>, greater<Message>> msg_queue;
        int msg_id = unique_message_id.load();
        unique_message_id.fetch_add(1);
        msg_queue.push(Message(msg_id, root, root, root, 0, 0, 0));

        algo->Init();

        while (!msg_queue.empty()) {
            auto msg = msg_queue.top();
            msg_queue.pop();
            auto msg_id = msg.GetId();
            auto cur_node = algo->GetNode(msg.GetDst());
            auto src_node=  algo->GetNode(msg.GetSrc());
            auto cur_nodeId = cur_node->GetId();

            if (cur_node->IsRelay(msg_id)) {
                algo->AddDup(1);
                if (std::is_same_v<algo_T, BestRelay<>>) {
                    algo->ProcessDupMsg(src_node, cur_node);
                }
                continue;
            }

            algo->AddNormal(1);

            cur_node->ReceiveMsg(msg, src_node, cur_node);

            result.AddReceiveNode(1);
            result.AddReceiveTime(msg.GetRecvTime(), algo->GetTotalMsg());

            auto relay_list = algo->Response(msg);
            double delay_time = (FIXED_DELAY - 50) + std::min(std::max(rand_latency(generator), 0.0), 100.0);
            for (auto relay_node : relay_list) {
                double dist = distance(coord[cur_nodeId], coord[relay_node]) * 3;
                Message new_msg = Message(msg_id, root, cur_nodeId, relay_node, msg.GetStep() + 1, msg.GetRecvTime() + delay_time, msg.GetRecvTime() + dist + delay_time);
                msg_queue.push(new_msg);
            }
        }

    }
    return result; 
}


template <class algo_T>
test_result simulation(int rept_time = 1, double mal_node = 0.0) {

// Test the latency and duplication rate for the whole network.
// Firstly ranomly select some malicious nodes.
// Then, for every honest node, do a single_root_simulation.

    srand(100);

    test_result result;

    FILE* output = fopen("sim_output.csv", "a");
    if (output == NULL) {
        fprintf(stderr, "cannot open file\n");
        return result;
    }

    int test_time = 0;
    for (int rept = 0; rept < rept_time; rept++) {
        //fprintf(stderr, "rept %d\n", rept);
        // 1) generate malicious node list
        memcle(mal_flag);
        for (int i = 0; i < mal_node * n; i++){
            int picked_num = random_num(n);
            while (mal_flag[picked_num] == true)  
                picked_num = random_num(n);
            mal_flag[picked_num] = true;
        }

        // 2) simulate the message at source i
        int test_node = 10;

        shared_ptr<algo_T> algo(new algo_T(n, coord, 0)); // initialize an algo instance, regardless of the root
        for (; test_node > 0; test_node--) {
            printf("%d\n", test_node);
            int root = rand() % n;
            while (mal_flag[root] == true) root = rand() % n;
            test_time++;
            auto res = single_root_simulation<algo_T>(root, 1, mal_node, algo);
            //printf("%d\n", test_node);
            result.avg_bnd += res.avg_bnd;
            for (size_t i = 0; i < result.latency.size(); i++) {
                result.latency[i] += res.latency[i];
            }
            for (int i = 0; i < MAX_DEPTH; i++) {
                result.depth_cdf[i] += res.depth_cdf[i];
                result.avg_dist[i] += res.avg_dist[i];
            }
            result.avg_latency += res.avg_latency;

            for (int c = 0; c < K; c++) {
                result.cluster_avg_depth[c] += res.cluster_avg_depth[c];
                result.cluster_avg_latency[c] += res.cluster_avg_latency[c];
            }
        }
    }

    result.avg_latency /= test_time;
    result.avg_bnd /= test_time;
    for (int c = 0; c < K; c++) {
        result.cluster_avg_depth[c] /= test_time;
        result.cluster_avg_latency[c] /= test_time;
    }

    for (size_t i = 0; i < result.latency.size(); i++) {
        double tmp = int(result.latency[i] / inf);
        result.latency[i] -= tmp * inf;
        result.latency[i] /= (test_time - tmp);
        if (test_time - tmp == 0)
            result.latency[i] = 0;
    }
    for (int i = 0; i < MAX_DEPTH; i++) {
        result.depth_cdf[i] /= test_time;
        result.avg_dist[i] /= test_time;
    }

    //fprintf(stderr, "latency sum at 0.95 %.2f\n", result.latency[19]);
    fprintf(output, "%s\n", algo_T::get_algo_name());
    printf("%s\n", algo_T::get_algo_name());
    fprintf(output, "#node, mal node, Bandwidth, ");
    printf("#node, mal node, Bandwidth, ");
    for (double p = 0.05; p <= 1; p += 0.05) {
        fprintf(output, "%.2f, ", p);
        printf("%.2f, ", p);
    }
    fprintf(output, "\n");
    printf("\n");

    fprintf(output, "%d, %.2f, %.2f, ", n, mal_node, result.avg_bnd);
    printf("%d, %.2f, %.2f, ", n, mal_node, result.avg_bnd);
    //printf
    int cnt = 0;
    for (double p = 0.05; p <= 1; p += 0.05, cnt++) {
        fprintf(output, "%.2f, ", result.latency[cnt]);
        printf("%.2f, ", result.latency[cnt]);
    }
    fprintf(output, "\n");
    printf("\n");

    fprintf(output, "depth pdf\n");
    printf("depth pdf\n");
    for (int i = 0; i < MAX_DEPTH; i++) {
        fprintf(output, "%d, ", i);
        printf("%d, ", i);
    }
    fprintf(output, "\n");
    printf("\n");

    double avg = 0;
    for (int i = 0; i < MAX_DEPTH; i++) {
        fprintf(output, "%.4f, ", result.depth_cdf[i]);
        printf("%.4f, ", result.depth_cdf[i]);
        avg += result.depth_cdf[i] * i;
    }
    fprintf(output, "\n");
    printf("\n");

    fprintf(output, "avg depth = %.2f\n", avg);
    printf("avg depth = %.2f\n", avg);
    fprintf(output, "avg latency = %.2f\n", result.avg_latency);
    printf("avg latency = %.2f\n", result.avg_latency);

    fprintf(output, "cluster avg depth\n");
    printf("cluster avg depth\n");
    for (int i = 0; i < K; i++) {
        fprintf(output, "%.2f, ", result.cluster_avg_depth[i]);
        printf("%.2f, ", result.cluster_avg_depth[i]);
    }
    fprintf(output, "\n");
    printf("\n");

    fprintf(output, "cluster avg latency\n");
    printf("cluster avg latency\n");
    for (int i = 0; i < K; i++) {
        fprintf(output, "%.2f, ", result.cluster_avg_latency[i]);
        printf("%.2f, ", result.cluster_avg_latency[i]);
    }
    fprintf(output, "\n");
    printf("\n");

    fprintf(output, "avg distance by depth\n");
    printf("avg distance by depth\n");
    for (int i = 0; i < MAX_DEPTH; i++) {
        fprintf(output, "%.2f, ", result.avg_dist[i]);
        printf("%.2f, ", result.avg_dist[i]);
    }
    fprintf(output, "\n");
    printf("\n");

    fclose(output);



    fig_csv = fopen("fig.csv", "a");
    if (fig_csv == NULL) {
        fprintf(stderr, "cannot open file\n");
        return result;
    }


    fprintf(fig_csv, "%s, ", algo_T::get_algo_name());
    cnt = 0;
    for (double p = 0.05; p <= 1; p += 0.05, cnt++) {
        fprintf(fig_csv, "%.2f, ", result.latency[cnt]);
        printf("%.2f, ", result.latency[cnt]);
    }
    fprintf(fig_csv, "\n");

    fclose(fig_csv);

    return result;
}

void init() { 
    // Read the geo information from input.
    n = 0;
    FILE* f = fopen("geolocation.txt", "r");
    fscanf(f, "%d", &n);
    for (int i = 0; i < n; i++) {
        fscanf(f, "%lf%lf", &coord[i].lat, &coord[i].lon);
    }

    n = std::min(n, MAX_TEST_N);

    fclose(f);
}

template <class algo_T>
void Submit(ThreadPool& net, shared_ptr<algo_T> algo, SafeQueue<std::future<TestResult>>& queue) {
    int root = random_num(n);
    auto result = net.Submit(Simulation<algo_T>, root, 1, algo);
    queue.Push(std::move(result));
}

template<>
void Submit(ThreadPool& net, shared_ptr<RandomFlood<>> algo, SafeQueue<std::future<TestResult>>& queue) {
    int root = random_num(n);
    auto result = net.Submit(Simulation<RandomFlood<>>, root, 1, algo);
    queue.Push(std::move(result));
}

template<>
void Submit(ThreadPool& net, shared_ptr<BestRelay<>> algo, SafeQueue<std::future<TestResult>>& queue) {
    int root = random_num(n);
    auto result = net.Submit(Simulation<BestRelay<>>, root, 1, algo);
    queue.Push(std::move(result));
}

template<class algo_T>
void RunOneAlgorithm(std::shared_ptr<algo_T> algo, ThreadPool& net,
                    std::vector<std::thread>& result_threads,
                    std::vector<std::thread>& algo_threads,
                    TestResultQueue& result_queue) {
    result_threads.emplace_back(
        [&result_queue, &algo]{
            std::future<TestResult> fu;
            while (!stop.load()) {
                if (result_queue.Pop(fu)) {
                    auto result = fu.get();
                    algo->Statistical(result);
                }
            }
        }
    );

    algo_threads.emplace_back(
        [&algo, &net, &result_queue]{
            while (!stop.load()) {
                Submit<algo_T>(net, algo, result_queue);
            }
        }
    );


}

void Stop(int signal) {
    std::cout << "Stop..." << std::endl;
    stop.store(true);
}

int main() {
    init(); // 读入经纬度坐标
    std::signal(SIGINT, Stop);

    std::vector<std::shared_ptr<BasicAlgorithm>> algos;
    std::vector<thread> result_threads;
    std::vector<thread> algo_threads;
    std::vector<std::shared_ptr<TestResultQueue>> result_queues;

    ThreadPool net(3);
    TestResultQueue result_queue;

    auto best_relay = std::make_shared<BestRelay<>>(n, nullptr, 0);
    // auto random = std::make_shared<RandomFlood<>>(n, nullptr, 0);

    std::thread result_thread([&result_queue, &best_relay](){
        std::future<TestResult> fu;
        while (!stop) {
            if (result_queue.Pop(fu)) {
                auto res = fu.get();
                best_relay->Statistical(res);
            }
        }
    });

    // std::thread result_thread([&result_queue, &random](){
    //     std::future<TestResult> fu;
    //     while (!stop) {
    //         if (result_queue.Pop(fu)) {
    //             auto res = fu.get();
    //             random->Statistical(res);
    //         }
    //     }
    // });

    while (!stop) {
        Submit<BestRelay<8>>(net, best_relay, result_queue);
        // Submit<RandomFlood<>>(net, random, result_queue);
    }

    while (!stop) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    net.Stop();

    result_thread.join();

    best_relay->ShowStatistical();
    // random->ShowStatistical();


    /*
    auto best_relay = std::make_shared<BestRelay<>>(n, nullptr, 0); // todo: 运行30s，统计结果
    auto random_flood = std::make_shared<RandomFlood<>>(n, nullptr, 0);

    for (int i = 0; i < 2; i++) {
        result_queues.emplace_back(std::make_shared<TestResultQueue>());
    }

    RunOneAlgorithm<BestRelay<>>(best_relay, net, result_threads, algo_threads, *result_queues[0]);
    RunOneAlgorithm<RandomFlood<>>(random_flood, net, result_threads, algo_threads, *result_queues[1]);

    for (auto& th : result_threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    for (auto& th : algo_threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    net.Stop();

    for (auto algo : algos) {
        algo->ShowStatistical();
    } */

    //MERCURY
    // generate_virtual_coordinate();
    // k_means_based_on_virtual_coordinate();
    // simulation<k_means_cluster<128, 8, 8, true> >(rept, mal_node);
    //MERCURYCLUSTER(with the early outburset optimization disabled)
    // simulation<k_means_cluster<8, 8, 8, true> >(rept, mal_node);

    //RANDOM
    // simulation<random_flood<8, 8, 8> >(rept, mal_node);

    //Perigee
    //simulation<perigee_ubc<6, 6, 8> >(rept, mal_node);
    
    //BlockP2P
    //k_means();
    //simulation<block_p2p<8> >(rept, mal_node);

    return 0;
}


