#pragma once
#include <string> 
#include <vector>
#include <mutex>
#include <queue> 
#include <condition_variable>
#include <atomic>  
// #include <condition_varable>

class LogQueue{
    public:
        struct Logentry{
            std::string msg;
            bool inputConsole = false;
        };

        void push(const Logentry& msg){
            std::lock_guard<std::mutex> lock(q_mtx_);
            queue_.push(msg);
            cond_var_.notify_one();
        }
        bool pop(Logentry& msg){
            std::unique_lock<std::mutex> lock(q_mtx_);
            cond_var_.wait(lock, [this](){
                return queue_.size() || is_shut_;
            });
            if(is_shut_ && queue_.empty())
                return false;

            msg = queue_.front();
            queue_.pop();
            return true;
        }
        
        void shutdown(){
            std::lock_guard<std::mutex> lock(q_mtx_);
            is_shut_ = true;
            cond_var_.notify_all();
        }
    private:
        std::queue<Logentry> queue_;
        std::mutex q_mtx_;
        std::condition_variable cond_var_;
        std::atomic<bool> is_shut_ = false;
};