/*
 * @Author: kelise
 * @Date: 2024-10-20 16:35:00
 * @LastEditors: kelis-cpu
 * @LastEditTime: 2024-10-21 17:58:08
 * @Description: file content
 */
#include <vector>
#include <algorithm>
#include <iostream>

#include <functional>

template <class T, class Compare = std::less<>>
class PriorityQueue {
 public:
    T Top();
    void Push(T ele);
    T Pop();
    int Size() { return contain_.size(); };
 
 private:
    void HeapDown(int index);
    void HeapUp(int index);

    std::vector<T> contain_;
    Compare com;
};

template <class T, class Compare>
T PriorityQueue<T, Compare>::Top() {
    if (contain_.empty()) {
        return T();
    }

    return contain_[0];
}

template <class T, class Compare>
void PriorityQueue<T, Compare>::HeapUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (!com(contain_[parent], contain_[index])) {
            break;
        }
        std::swap(contain_[parent], contain_[index]);
        index = parent;
    }
}

template <class T, class Compare>
void PriorityQueue<T, Compare>::HeapDown(int index) {
    int parent = index;
    int child = 2 * parent + 1;
    int size = contain_.size();

    while (child < size) {
        if (child + 1 < size && com(contain_[child], contain_[child + 1])) {
            ++child;
        }
        if (!com(contain_[parent], contain_[child])) {
            break;
        }
        std::swap(contain_[parent], contain_[child]);
        parent = child;
        child = parent * 2 + 1;
    }
}

template <class T, class Compare>
T PriorityQueue<T, Compare>::Pop() {
    if (contain_.empty()) {
        return T();
    }

    T value = contain_[0];
    contain_[0] = contain_.back();
    contain_.pop_back();
    HeapDown(0);
    return value;
}

template <class T, class Compare>
void PriorityQueue<T, Compare>::Push(T ele) {
    contain_.push_back(ele);
    HeapUp(contain_.size() - 1);
}

template <class T, class Compare = std::less<>> 
void Heapify(std::vector<T>& arr, int start, int end, Compare com = Compare{}) {
    int parent = start;
    int child = 2 * parent + 1;

    while (child <= end) {
        if (child + 1 <= end && com(arr[child], arr[child + 1])) {
            ++child;
        }

        if (!com(arr[parent], arr[child])) {
            break;
        }
        std::swap(arr[parent], arr[child]);
        parent = child;
        child = 2 * parent + 1;
    }
}

template <class T, class Compare = std::less<>>
void HeapSort(std::vector<T>& arr, Compare com = Compare{}) {
    int end = arr.size() - 1;
    for (int start = (end - 1) / 2; start >= 0; start--) {
        Heapify<T, Compare>(arr, start, end);
    }
    for (int i = end; i > 0; i--) {
        std::swap(arr[0], arr[i]);
        Heapify<T, Compare>(arr, 0, i - 1);
    }
}

int main() {
    PriorityQueue<int> pq;
    pq.Push(10);
    pq.Push(20);
    pq.Push(5);
    pq.Push(30);

    while (pq.Size() > 0) {
        std::cout << pq.Pop() << std::endl;
    }

    std::hash<std::string> shash;
    std::hash<int> inthash;
    std::cout << shash("test") << std::endl;
    std::cout << inthash(42) << std::endl;
}



