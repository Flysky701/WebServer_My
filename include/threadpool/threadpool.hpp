#pragma once

#include <boost/lockfree/queue.hpp>
#include <atomic>
#include <thread>
#include <functional>
#include <condition_variable>
#include <memory>
#include <vector>    


class ThreadPool{
    public:
        using Task = std::function<void()>;
        explicit ThreadPool(size_t thread_num, size_t queue_size);
        ~ThreadPool(){stop();};
        void stop();
        template<class F>
        bool enqueue(F&& task) {
            if(stop_.load()) return false;
        
            auto wrapper = std::make_unique<TaskWrapper<F>>(std::forward<F>(task));
            while(!stop_.load()) {
                if(task_.push(wrapper.get())) {
                    wrapper.release();
                    cv_.notify_one();
                    return true;
                }
                std::this_thread::yield();
            }
            return false;
        }
    
    private:
        // 虚函数机制， 统一不同类型接口
        
        struct TaskBase {
            virtual ~TaskBase() = default;
            virtual void execute() = 0;
        };
        // 将泛型任务类型F适配到TaskBase体系

        template<typename F>
        struct TaskWrapper : TaskBase {
            F func;
            explicit TaskWrapper(F&& f) : func(std::forward<F>(f)) {}
            void execute() override { func(); }
        };
        
        boost::lockfree::queue<TaskBase*> task_;
        std::vector<std::thread> worker_;
        std::atomic<bool> stop_;
        std::condition_variable cv_;
        std::mutex cv_mtx_;
        
        void workloop(){
            while(!stop_){
                TaskBase* task = nullptr;
                if(task_.pop(task)){
                    std::unique_ptr<TaskBase> guard(task);
                    task -> execute();
                }else {
                    std::unique_lock<std::mutex> lock(cv_mtx_);
                    cv_.wait_for(lock, std::chrono::milliseconds(100), [this]{
                        return stop_ || !task_.empty();
                    });
                }
            }
            drain_queue();
        }
        //  清空队列
        void drain_queue(){
            TaskBase* task = nullptr;
            while(task_.pop(task)){
                std::unique_ptr<TaskBase> guard(task);
                task -> execute();
            }
        }
};

ThreadPool::ThreadPool(size_t thread_num = std::thread::hardware_concurrency(), 
                       size_t queue_size = 1024):
                       task_(queue_size), 
                       stop_(false)
{
    worker_.reserve(thread_num);
    for(size_t i = 0; i < thread_num; i ++)
        worker_.emplace_back([this]{workloop();});
}

void ThreadPool::stop(){
    if(!stop_.exchange(true)){
        cv_.notify_all();
        for(auto & worker : worker_)
            if(worker.joinable())
                worker.join();
        drain_queue();
    }
}