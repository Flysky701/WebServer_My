#pragma once

// struct
#include <string.h>
#include <vector>
#include <queue>
#include <unordered_map>
// condition
#include <mutex>
#include <unistd.h>
#include <condition_variable>
#include <memory>
#include <signal.h>
// net
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <netinet/in.h>
// My
#include "epollmanager.h"
#include "connection.h"
#include "threadpool.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "authhandler.h"
#include "userdao.h"
#include "sqlconnpool.h"
#include "staticfilehandle.h"
#include "timer.h"
#include "log.h"



class Server
{
public:
    explicit Server(int port)
        : port_(port),
          running_(false),
          pool_(32),
          sqlConnPool_(SqlConnPool::instance()),
          userDao_(SqlConnPool::instance()),
          authHandler_(userDao_),
          timer_([this](int fd)
                 { HandleTimeout(fd); })
    {

        InitSocket();
        M_epoll_.AddFd(server_fd_, EPOLLIN);
    }

    ~Server() { Stop(); }
    void Run();
    void Stop()
    {
        if (running_)
        {
            running_ = false;
            close(server_fd_);
            {
                std::lock_guard<std::mutex> lock(task_mtx);
                for (auto &pair : Conns_)
                    CloseConnection(pair.second);
                Conns_.clear();
            }
            ProcessPendingTask();
            LOG_INFO("服务器关闭");
        }
    }

private:
    int port_;
    int server_fd_;
    bool running_;
    EpollManager M_epoll_;
    ThreadPool pool_;
    Timer timer_;
    SqlConnPool& sqlConnPool_;
    UserDao userDao_;
    AuthHandler authHandler_;
    static constexpr int CONN_TIMEOUT = 60000;
    StaticFileHandler static_handler_{"public"};
    Router route_;

    std::unordered_map<int, std::shared_ptr<Connection>> Conns_;
    std::mutex epoll_mtx;
    std::mutex task_mtx;
    std::queue<std::function<void()>> pending_tasks;


    void InitSocket();
    void HandelConnection();
    void HandleEvent(epoll_event& event);
    void HandleTimeout(int fd);
    bool HandleRead(Connection *conn);
    bool HandleWrite(Connection* conn);
    void SubmitToThreadPool(std::shared_ptr<Connection> conn);
    void CloseConnection(std::shared_ptr<Connection> conn);
    void ProcessPendingTask();
    void Routes_Init();
};