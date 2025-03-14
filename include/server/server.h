#ifndef SERVER_H
#define SERVER_H

// struct
#include<string.h>
#include<vector>
#include<queue>
#include<unordered_map>  
//condition
#include<mutex>
#include<unistd.h>
#include<condition_variable>
#include<memory>
// net
#include <sys/socket.h>
#include <netinet/in.h>
//My
#include"epollmanager.h"
#include"connection.h"
#include"threadpool.h"
#include"httprequest.h"
#include"timer.h"
#include"log.h"



class Server{
    public:
        explicit Server(int port)
            : port_(port), running_(false), pool_(std::thread::hardware_concurrency() * 2),
              timer_([this](int fd) { HandleTimeout(fd); })
        {
                InitSocket();
                M_epoll_.AddFd(server_fd_, EPOLLIN);
        }
        ~Server() { Stop(); }
        void Run();
        void Stop()
        {
            if(running_){
                running_ = false;
                close(server_fd_);
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

        std::unordered_map<int, std::unique_ptr<Connection>> Conns_;
        std::mutex epoll_mtx;
        std::mutex task_mtx;
        std::queue<std::function<void()>> pending_tasks;


        void InitSocket();
        void HandelConnection();
        void HandleEvent(epoll_event& event);
        void HandleTimeout(int fd);
        bool HandleRead(Connection *conn);
        bool HandleWrite(Connection* conn);
        void SubmitToThreadPool(Connection* conn);
        void CloseConnection(Connection* conn);
        void ProcessPendingTask();
};

void Server::Run(){
    running_ = true;
    LOG_INFO("服务器于端口" + std::to_string(port_) + "开放");

    while(running_){
        int num_events = M_epoll_.WaitEvents(50);
        ProcessPendingTask();

        for (int i = 0; i < num_events; i ++){
            auto& events = M_epoll_.GetEvent()[i];
            if(events.data.fd == server_fd_)
                HandelConnection();
            else
                HandleEvent(events);
        }

        timer_.Tick();
    }
}
void Server::InitSocket(){
    LOG_INFO("InitSocket");
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd_ < 0){
        LOG_ERROR("Socket创建失败");
        throw std::system_error(errno, std::system_category());
    }

    int flags = fcntl(server_fd_, F_GETFL, 0);
    if (flags == -1){
        LOG_ERROR("fcntl F_GETFL failed");
        throw std::system_error(errno, std::system_category());
    }
    if (fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK) == -1){
        LOG_ERROR("fcntl F_SETFL non-blocking failed");
        throw std::system_error(errno, std::system_category());
    }
    // SO_REUSEADDR允许端口重用，防止TIME_WAIT状态导致绑定失败
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; // IPV4;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        LOG_ERROR("绑定失败");
        throw std::system_error(errno, std::system_category());
    }
    // 
    if (listen(server_fd_, 1024) < 0){
        LOG_ERROR("监听fd失败");
        throw std::system_error(errno, std::system_category());
    }
}

void Server::HandelConnection(){

    LOG_INFO("HandelConnection");
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    std::lock_guard<std::mutex> lock(task_mtx);

    while(true){
        LOG_DEBUG("HandelConnection_循环");
        int client_fd = accept(server_fd_, (sockaddr *)&client_addr, &len);
        if(client_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
            LOG_ERROR("接受连接失败");
            continue;
        }
        LOG_DEBUG("HandelConnection_client_fd_YES");

        auto conn = std::make_unique<Connection>(client_fd);
        Conns_.emplace(client_fd, std::move(conn));
        M_epoll_.AddFd(client_fd, EPOLLET | EPOLLIN);
        timer_.Add(client_fd, CONN_TIMEOUT);

        LOG_DEBUG("添加一个新连接" + std::to_string(client_fd));
    }
    LOG_INFO("HandelConnection_quit");
    
}
void Server::HandleTimeout(int fd){
    std::lock_guard<std::mutex> lock(task_mtx);
    if (Conns_.count(fd)){
        LOG_INFO("连接超时 fd： %d", fd);
        CloseConnection(Conns_[fd].get());
    }
    else{
        LOG_DEBUG("连接 %d 已关闭，忽略超时", fd);
    }
}

bool Server::HandleRead(Connection* conn){
    if(!conn -> ReadData()){
        LOG_DEBUG("读取于Fd" + std::to_string(conn->GetFd()) + "上失败");
        return false;
    }
    return true;
}
bool Server::HandleWrite(Connection* conn){
    if(!conn -> Flush()){
        LOG_DEBUG("写操作在Fd" + std::to_string(conn->GetFd()) + "上失败");
        return false;
    }
    if(conn -> HasPendingWrite()){
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll_.ModifyFd(conn->GetFd(), EPOLLOUT | EPOLLET);
    }else {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll_.ModifyFd(conn->GetFd(), EPOLLIN | EPOLLET);
    }
    return true;
}

void Server::HandleEvent(epoll_event& event){
    int fd = event.data.fd;
    auto it = Conns_.find(fd);
    if (it == Conns_.end())
        return;

    Connection *conn = it->second.get();

    if (event.events & EPOLLERR || event.events & EPOLLHUP){
        CloseConnection(conn);
        return;
    }

    if (event.events & EPOLLIN)
        if (!HandleRead(conn))
            CloseConnection(conn);
        else{
            timer_.Add(fd, CONN_TIMEOUT);
            SubmitToThreadPool(conn);
        }
        
    if (event.events & EPOLLOUT){
        if(!HandleWrite(conn)){
            CloseConnection(conn);
            return;
        }
        else{
            std::lock_guard<std::mutex> lock(epoll_mtx);
            timer_.Add(fd, CONN_TIMEOUT);
            if (!conn->HasPendingWrite())
                M_epoll_.ModifyFd(fd, EPOLLIN | EPOLLET);
        }
    }
        
}

void Server::SubmitToThreadPool(Connection *conn)
{
    auto fun = [this, conn]()
    {
        HttpRequest req;
        const auto &buffer = conn->GetReadBuffer();
        if (req.parse(buffer))
        {
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 26\r\nConnection: keep-alive\r\n\r\n<h1>Hello from Server</h1>";
            
            {
                std::lock_guard<std::mutex> lock(epoll_mtx);
                conn->WriteData(resp);
            }
            // 提交Flush及事件修改到主线程
            {
                std::lock_guard<std::mutex> t_lock(task_mtx);
                pending_tasks.push([this, conn]()
                                   {
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
    pool_.enqueue(fun);
}
// 解释下面的功能
void Server::ProcessPendingTask(){
    std::lock_guard<std::mutex> lock(task_mtx);
    while(!pending_tasks.empty()){
        auto task = pending_tasks.front();
        pending_tasks.pop();
        task();
    }
}
void Server::CloseConnection(Connection* conn){
    if(!conn) return;
    int fd = conn->GetFd();
    {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        timer_.Remove(fd);

        if (Conns_.count(fd))
        {
            M_epoll_.RemoveFd(fd, 0);
            Conns_.erase(fd);
            close(fd);
            LOG_DEBUG("关闭连接: " + std::to_string(fd));
        }
    }
}

#endif