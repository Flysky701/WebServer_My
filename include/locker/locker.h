#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
// #include <pthread.h>
#include <semaphore.h>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

class sem{
    private:
        sem_t sem_m;
    public:
        sem(){
            if(sem_init(&sem_m, 0, 0) != false){
                throw std::exception();
            }
        }
        ~sem(){
            sem_destroy(&sem_m);
        }
        bool wait(){
            sem_wait(&sem_m);
        }
        bool post(){
            sem_post(&sem_m);
        }
};

class locker{
    private:
        std::mutex mtx;
    public:
        locker(){
            if(!mtx.native_handle()){
                throw std::runtime_error("Mutex initialization failed");
            }
        }
        void lock(){
            mtx.lock();
        }
        void unlock(){
            mtx.unlock();
        }
        bool try_lock(){
            return mtx.try_lock();
        }
        auto get(){
            return mtx.native_handle();
        }
        locker(const locker &) = delete;
        locker &operator=(const locker &) = delete;
};

class cond{
    private:
        
    public:
};

#endif