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
#include"log.h"



class Server{
    public:
        explicit Server(int port)
            : port_(port), running(false), pool(4){
                InitSocket();
                M_epoll.AddFd(server_fd, EPOLLIN);
        }
        ~Server() { Stop(); }
        void Run();
        void Stop()
        {
            if(running){
                running = false;
                close(server_fd);
                LOG_INFO("服务器关闭");
            }
        }

    private:
        int port_;
        int server_fd;
        bool running;
        EpollManager M_epoll;
        ThreadPool pool;
        std::unordered_map<int, std::unique_ptr<Connection>> Conns_;
        std::mutex epoll_mtx;
        std::mutex task_mtx;
        std::queue<std::function<void()>> pending_tasks;

        void InitSocket();
        void HandelConnection();
        void HandleEvent(epoll_event& event);
        bool HandleRead(Connection* conn);
        bool HandleWrite(Connection* conn);
        void SubmitToThreadPool(Connection* conn);
        void CloseConnection(Connection* conn);
        void ProcessPendingTask();
};

void Server::Run(){
    running = true;
    LOG_INFO("服务器于端口" + std::to_string(port_) + "开放");

    while(running){
        int num_events = M_epoll.WaitEvents();
        ProcessPendingTask();

        for (int i = 0; i < num_events; i ++){
            auto& events = M_epoll.GetEvent()[i];
            if(events.data.fd == server_fd)
                HandelConnection();
            else
                HandleEvent(events);
        }
    }
}
void Server::InitSocket(){
    LOG_INFO("InitSocket");
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        LOG_ERROR("Socket创建失败");
        throw std::system_error(errno, std::system_category());
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1){
        LOG_ERROR("fcntl F_GETFL failed");
        throw std::system_error(errno, std::system_category());
    }
    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1){
        LOG_ERROR("fcntl F_SETFL non-blocking failed");
        throw std::system_error(errno, std::system_category());
    }
    // SO_REUSEADDR允许端口重用，防止TIME_WAIT状态导致绑定失败
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET; // IPV4;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        LOG_ERROR("绑定失败");
        throw std::system_error(errno, std::system_category());
    }
    // 
    if (listen(server_fd, 1024) < 0){
        LOG_ERROR("监听fd失败");
        throw std::system_error(errno, std::system_category());
    }
}

void Server::HandelConnection(){
    LOG_INFO("HandelConnection");
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);

    while(true){
        LOG_DEBUG("HandelConnection_循环");
        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &len);
        LOG_DEBUG("HandelConnection_client_fd");
        if(client_fd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                LOG_DEBUG("HandelConnection_client_fd_NO");
                break;
            }
            LOG_ERROR("接受连接失败");
            continue;
        }
        LOG_DEBUG("HandelConnection_client_fd_YES");

        auto conn = std::make_unique<Connection>(client_fd);
        Conns_.emplace(client_fd, std::move(conn));
        M_epoll.AddFd(client_fd, EPOLLET | EPOLLIN);
        LOG_DEBUG("添加一个新连接" + std::to_string(client_fd));
    }
    LOG_INFO("HandelConnection_quit");
    
}

bool Server::HandleRead(Connection* conn){
    LOG_INFO("HandleRead");
    if(!conn -> ReadData()){
        LOG_DEBUG("读取于Fd" + std::to_string(conn->GetFd()) + "上失败");
        return false;
    }
    return true;
}
bool Server::HandleWrite(Connection* conn){
    LOG_INFO("HandleWrite");
    if(!conn -> Flush()){
        LOG_DEBUG("写操作在Fd" + std::to_string(conn->GetFd()) + "上失败");
        return false;
    }
    if(conn -> HasPendingWrite()){
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll.ModifyFd(conn->GetFd(), EPOLLOUT | EPOLLET);
    }else {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        M_epoll.ModifyFd(conn->GetFd(), EPOLLIN | EPOLLET);
    }
    return true;
}

void Server::HandleEvent(epoll_event& event){
    LOG_INFO("HandleEvent");
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
        else
            SubmitToThreadPool(conn);
        
    if (event.events & EPOLLOUT){
        if(!HandleWrite(conn)){
            CloseConnection(conn);
            return;
        }
        else{
            std::lock_guard<std::mutex> lock(epoll_mtx);
            if(!conn ->HasPendingWrite())
                M_epoll.ModifyFd(fd, EPOLLIN | EPOLLET);
        }
    }
        
}

void Server::SubmitToThreadPool(Connection *conn)
{
    LOG_INFO("SubmitToThreadPool");
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
                    M_epoll.ModifyFd(conn->GetFd(), events); });
            }
        }
    };
    pool.enqueue(fun);
}
// 解释功能
void Server::ProcessPendingTask(){
    std::lock_guard<std::mutex> lock(task_mtx);
    while(!pending_tasks.empty()){
        auto task = pending_tasks.front();
        pending_tasks.pop();
        task();
    }
}
void Server::CloseConnection(Connection* conn){
    int fd = conn->GetFd();
    {
        std::lock_guard<std::mutex> lock(epoll_mtx);
        if (Conns_.count(fd))
        {
            M_epoll.RemoveFd(fd, 0);
            Conns_.erase(fd);
            close(fd);
            LOG_DEBUG("关闭连接: " + std::to_string(fd));
        }
    }
}

#endif