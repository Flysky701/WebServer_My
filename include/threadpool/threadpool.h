#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <functional> 
#include <condition_variable>
#include <atomic>


class ThreadPool{
    public:
        using Task = std::function<void()> ;
        ThreadPool(size_t);
        ~ThreadPool();
        template <class T>
        void enqueue(T &&task);
        

    private:
        std::vector<std::thread> workers;
        std::queue<Task> tasks;
        std::mutex mtx;
        std::condition_variable cv;
        std::atomic<bool> stop;


        void work_loop(){
            while(true){
                Task task;
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    this->cv.wait(lock, [this]{return this->stop || !this->tasks.empty();});
    
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        }
};

ThreadPool::ThreadPool(size_t thread_num = 4)
    : stop(false)
{
    workers.reserve(4);

    for (size_t i = 0; i < thread_num; i++)
        workers.emplace_back([this]{this -> work_loop();});
}
ThreadPool::~ThreadPool()
{
    stop = true;
    cv.notify_all();

    for(auto &worker : workers)
        if(worker.joinable())
            worker.join();
}
template<class F>
void ThreadPool::enqueue(F &&task)
{
    {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.emplace(std::forward<F>(task));
    }
    cv.notify_one();
}

#endif