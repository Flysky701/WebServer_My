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
#include "router.h"
#include "sqlconnpool.h"
#include "timer.h"
#include "log.h"

// 需要测试的类
#include "filehandle.h"
#include "tokenmanager.h"

class Server
{
public:
    explicit Server(int port)
        : port_(port),
          running_(false),
          pool_(32),
          sqlConnPool_(SqlConnPool::instance()),
          userDao_(SqlConnPool::instance()),
          authHandler_(userDao_, tokenManager_),
          timer_([this](int fd)
                 { HandleTimeout(fd); })
    {

        InitSocket();
        Routes_Init();
        M_epoll_.AddFd(server_fd_, EPOLLIN);
    }

    ~Server() { Stop(); }
    void Run();
    inline void Stop()
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
    static constexpr int CONN_TIMEOUT = 60000;
    SqlConnPool &sqlConnPool_;
    UserDao userDao_;
    AuthHandler authHandler_;
    Router route_;
    TokenManager tokenManager_;

    // for test
    FileHandler static_handler_{"public"};

    std::unordered_map<int, std::shared_ptr<Connection>> Conns_;
    std::mutex epoll_mtx;
    std::mutex task_mtx;
    std::mutex token_mtx;
    std::queue<std::function<void()>> pending_tasks;

    void InitSocket();
    void HandelConnection();
    void HandleEvent(epoll_event &event);
    void HandleTimeout(int fd);
    bool HandleRead(Connection *conn);
    bool HandleWrite(Connection *conn);
    void SubmitToThreadPool(std::shared_ptr<Connection> conn);
    void CloseConnection(std::shared_ptr<Connection> conn);
    void ProcessPendingTask();
    void Routes_Init();
};

void Server::Run()
{
    running_ = true;
    LOG_INFO("服务器于端口 {} 开放", port_);

    while (running_)
    {
        int num_events = M_epoll_.WaitEvents(50);
        ProcessPendingTask();

        for (int i = 0; i < num_events; i++){
            auto &events = M_epoll_.GetEvent()[i];

            if (events.data.fd == server_fd_)
                HandelConnection();
            else
                HandleEvent(events);
        }

        timer_.Tick();
    }
}
void Server::InitSocket()
{

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0){
        LOG_ERROR("Socket创建失败");
        throw std::system_error(errno, std::system_category());
    }

    int flags = fcntl(server_fd_, F_GETFL, 0);
    if (flags == -1)
    {
        LOG_ERROR("获取Socket标志失败");
        throw std::system_error(errno, std::system_category());
    }
    if (fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK) == -1)
    {

        LOG_ERROR("设置Socket非阻塞模式失败");
        throw std::system_error(errno, std::system_category());
    }
    // SO_REUSEADDR允许端口重用，防止TIME_WAIT状态导致绑定失败
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; // IPV4;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        LOG_ERROR("地址绑定失败");
        throw std::system_error(errno, std::system_category());
    }
    //
    if (listen(server_fd_, 1024) < 0)
    {
        LOG_ERROR("监听fd失败");
        throw std::system_error(errno, std::system_category());
    }
}

void Server::HandelConnection()
{

    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    std::lock_guard<std::mutex> lock(task_mtx);

    while (true)
    {
        int client_fd = accept(server_fd_, (sockaddr *)&client_addr, &len);
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            LOG_ERROR("接受连接失败");
            continue;
        }

        auto conn = std::make_shared<Connection>(client_fd);
        Conns_.emplace(client_fd, std::move(conn));
        M_epoll_.AddFd(client_fd, EPOLLET | EPOLLIN);
        timer_.Add(client_fd, CONN_TIMEOUT);

        LOG_DEBUG("添加一个新连接 {}", client_fd);
    }
}

void Server::HandleTimeout(int fd)
{
    std::lock_guard<std::mutex> lock(task_mtx);
    if (Conns_.count(fd))
    {
        LOG_INFO("连接超时 fd： {}", fd);
        CloseConnection(Conns_[fd]);
    }
    else
    {
        LOG_DEBUG("连接 {} 已关闭，忽略超时", fd);
    }
}

bool Server::HandleRead(Connection *conn)
{
    if (!conn->ReadData())
    {
        LOG_DEBUG("读取在Fd {} 上失败", conn->GetFd());
        return false;
    }
    return true;
}

bool Server::HandleWrite(Connection *conn)
{
    if (!conn->Flush())
    {
        LOG_DEBUG("写操作在Fd {} 上失败", conn->GetFd());
        return false;
    }
    if (conn->HasPendingWrite())
    {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll_.ModifyFd(conn->GetFd(), EPOLLOUT | EPOLLET);
    }
    else
    {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll_.ModifyFd(conn->GetFd(), EPOLLIN | EPOLLET);
    }
    return true;
}

void Server::HandleEvent(epoll_event &event)
{
    int fd = event.data.fd;
    std::shared_ptr<Connection> conn;
    {
        std::lock_guard<std::mutex> lock(task_mtx);
        auto it = Conns_.find(fd);
        if (it == Conns_.end())
            return;
        conn = it->second;
    }

    if (event.events & (EPOLLERR | EPOLLHUP))
    {
        CloseConnection(conn);
        return;
    }

    if (event.events & EPOLLIN)
    {
        if (!HandleRead(conn.get()))
        {
            CloseConnection(conn);
        }
        else
        {
            timer_.Add(fd, CONN_TIMEOUT);
            SubmitToThreadPool(conn);
        }
    }

    if (event.events & EPOLLOUT)
    {
        if (!HandleWrite(conn.get()))
        {
            CloseConnection(conn);
        }
        else
        {
            std::lock_guard<std::mutex> lock(epoll_mtx);
            timer_.Add(fd, CONN_TIMEOUT);
            if (!conn->HasPendingWrite())
            {
                M_epoll_.ModifyFd(fd, EPOLLIN | EPOLLET);
            }
        }
    }
}

void Server::SubmitToThreadPool(std::shared_ptr<Connection> conn)
{
    auto fun = [this, conn]()
    {
        HttpRequest req;
        HttpResponse res;
        bool request_handled = false;
        const auto &buffer = conn->GetReadBuffer();

        if (req.parse(buffer))
        {
            conn->ClearReadBuffer();
            LOG_DEBUG("{}, {}, {}", req.method(), req.path(), req.version());
            try
            {
                bool check = true;
                if (route_.IsVaildate(req))
                    check = tokenManager_.Validate(req);
                
                if(check){
                    request_handled = route_.HandleRequest(req, res);
                    // 下面需要整合 到route里面
                    if (req.method() == "GET" || req.method() == "HEAD")
                        request_handled = static_handler_.handle_request(req, res);
                }
                if (!request_handled){
                    res.set_status(404)
                        .set_content("<h1>404 Not Found</h1>", "text/html");
                }
            }
            catch (const std::exception &e)
            {
                res.set_status(500)
                    .set_content("<h1>500 Internal Server Error</h1>", "text/html");
                LOG_ERROR("请求处理异常: " + std::string(e.what()));
            }

            res.set_keep_alive(req.keep_alive())
                .set_header("Server", "MyServer/1.0");

            // 序列化响应
            std::string resp_str = std::move(res.serialize());
            // DEBUG
            // LOG_DEBUG("响应内容:\n" + resp_str);
            {
                std::lock_guard<std::mutex> lock(epoll_mtx);
                conn->WriteData(resp_str);
            }
            // 提交Flush及事件修改到主线程
            {
                std::lock_guard<std::mutex> t_lock(task_mtx);
                pending_tasks.push([this, fd = conn->GetFd()]()
                                   {
                    if (!Conns_.count(fd)) return;
                    auto conn = Conns_[fd];
                    std::lock_guard<std::mutex> e_lock(epoll_mtx);
                    if (!conn->Flush()) {
                        CloseConnection(conn);
                        return;
                    }
                    uint32_t events = EPOLLIN | EPOLLET;
                    if (conn->HasPendingWrite()) {
                        events |= EPOLLOUT;
                    }
                    M_epoll_.ModifyFd(conn->GetFd(), events); });
            }
        }
    };
    pool_.enqueue(std::move(fun));
}

void Server::ProcessPendingTask()
{
    std::lock_guard<std::mutex> lock(task_mtx);
    while (!pending_tasks.empty())
    {
        auto task = pending_tasks.front();
        pending_tasks.pop();
        task();
    }
}

void Server::CloseConnection(std::shared_ptr<Connection> conn)
{
    if (!conn)
        return;
    int fd = conn->GetFd();
    {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        timer_.Remove(fd);

        if (Conns_.count(fd))
        {
            M_epoll_.RemoveFd(fd, 0);
            Conns_.erase(fd);
            LOG_DEBUG("关闭连接: " + std::to_string(fd));
        }
    }
}

void Server::Routes_Init()
{
    route_.add_token_Validate("/upload", "POST");
    route_.add_token_Validate("/download", "POST");
    route_.add_token_Validate("/dashboard.html", "GET");

    route_.add_route("/register", "POST", [this](const HttpRequest &req, HttpResponse &res)
                     { authHandler_.handle_register(req, res); });

    route_.add_route("/login", "POST", [this](const HttpRequest &req, HttpResponse &res)
                     { authHandler_.handle_login(req, res); });
}