#ifndef TIMER_H
#define TIMER_H

#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <memory>
#include "log.h"

class Timer{
    public:
        using Callback = std::function<void(int)>;

        Timer(Callback cb) : timeout_cb_(std::move(cb)){} ;
        ~Timer(){Clear();} ;

        void Add(int fd, int timeout_ms);
        void Tick();
        void Clear();
        void Remove(int fd);

    private:
        struct TimerNode{
            int fd;
            std::chrono::steady_clock::time_point expire;
            bool operator<(const TimerNode& other)const {
                return this -> expire < other.expire;
            }
        };
        
        std::vector<TimerNode> heap_;
        std::unordered_map<int, size_t> fd_to_index_;
        Callback timeout_cb_;
        std::mutex mutex_;


        void Heap_up(size_t idx);
        void Heap_down(size_t idx);
        void Heap_pop();
        void Swap_Node(size_t i, size_t j){
            std::swap(heap_[i], heap_[j]);
            fd_to_index_[heap_[i].fd] = i;
            fd_to_index_[heap_[j].fd] = j;
        }

};
void Timer::Add(int fd, int timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto new_expire = now + std::chrono::milliseconds(timeout_ms);
    
    if (fd_to_index_.count(fd)) {
        size_t idx = fd_to_index_[fd];
        if (heap_[idx].expire > new_expire) {
            heap_[idx].expire = new_expire;
            Heap_up(idx);
        } else {
            heap_[idx].expire = new_expire;
            Heap_down(idx);
        }
    } else {
        heap_.push_back({fd, new_expire});
        size_t idx = heap_.size() - 1;
        fd_to_index_[fd] = idx;
        Heap_up(idx);
    }
}
void Timer::Tick() {
    std::vector<int> expired_fds;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        while (!heap_.empty() && heap_[0].expire <= now) {
            expired_fds.push_back(heap_.front().fd);
            Heap_pop();
        }
    }
    for (int fd : expired_fds) {
        timeout_cb_(fd); 
    }
}
void Timer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    heap_.clear();
    fd_to_index_.clear();
}
void Timer::Remove(int fd){
    std::lock_guard<std::mutex> lock(mutex_);
    if(!fd_to_index_.count(fd))
        return;
    size_t idx = fd_to_index_[fd];
    Swap_Node(idx, heap_.size() - 1);
    heap_.pop_back();
    fd_to_index_.erase(fd);
    
    if(idx < heap_.size()){
        Heap_up(idx);
        Heap_down(idx);
    }
}

void Timer::Heap_up(size_t idx){
    while(idx > 0){
        size_t parent = (idx - 1) / 2;
        if (heap_[idx].expire >= heap_[parent].expire) break;
        Swap_Node(idx, parent);
        idx = parent;
    }
}
void Timer::Heap_down(size_t idx){
    size_t smallest = idx;
    while (true) {
        size_t child_l = 2 * idx + 1;
        size_t child_r = 2 * idx + 2;
        
        if (child_l < heap_.size() && heap_[child_l].expire < heap_[smallest].expire) 
            smallest = child_l;
        if (child_r < heap_.size() && heap_[child_r].expire < heap_[smallest].expire)
            smallest = child_r;
        if (smallest == idx) break;
        Swap_Node(idx, smallest);
        idx = smallest;
    }
}
void Timer::Heap_pop() {
    if(!heap_.size())return;
    std::cout << "Heappop()" << heap_[0].fd << std::endl;
    Timer::Swap_Node(0, heap_.size() - 1);

    fd_to_index_.erase(heap_.back().fd);
    heap_.pop_back();

    if (!heap_.empty()) {
        Heap_down(0);
    }
}
#endif