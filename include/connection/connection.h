#ifndef CONNECTION_H
#define CONNECTION_H

#include <vector> 
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>

#include "log.h"

class Connection{
    public:
        explicit Connection(int socket_fd);
        ~Connection();

        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        bool ReadData();                         // 从socket读取数据
        bool WriteData(const std::string &data); // 写入数据到缓冲区
        bool Flush();                           // 尝试发送缓冲区数据

        int GetFd() const {
            return fd_;
        }
        bool HasPendingWrite() const {
            return !w_buffer.empty();
        }
    private:
    
        int fd_;
        std::vector<char> r_buffer;
        std::string w_buffer;
        
        void SetNonBlocking(){
            int flag = fcntl(fd_, F_GETFL, 0);
            if(flag == -1){
                LOG_ERROR("");
                throw std::system_error(errno, std::system_category());
            }
            if (fcntl(fd_, F_SETFL, flag | O_NONBLOCK) == -1){
                LOG_ERROR("fcntl F_SETFL non-blocking failed");
                throw std::system_error(errno, std::system_category());
            }
        }
};


explicit Connection::Connection(int socket_fd){
    SetNonBlocking();
    LOG_DEBUG("");
}
Connection::~Connection(){
    if(fd_ >= 0){
        close(fd_);
        LOG_DEBUG("");
    }
}
bool Connection::ReadData(){
    std::vector<char> buffer(4096);
    ssize_t r_bytes = recv(fd_, buffer.data(), buffer.size(), MSG_DONTWAIT);

    if(r_bytes > 0){
        r_buffer = buffer;
        LOG_DEBUG("");
        return true;
    }else if(r_bytes == 0){
        LOG_INFO("");
        return false;
    }else {
        if(errno == EAGAIN | errno == EWOULDBLOCK){
            return true;
        }
        LOG_ERROR("");
        return false;
    }
}

bool Connection::WriteData(const std::string& data){
    try{
        w_buffer += data;
        LOG_DEBUG("");
        return true;
    }catch(const std::bad_alloc&){
        LOG_ERROR("");
        return false;
    }
}

bool Connection::Flush(){
    if(w_buffer.empty())
        return true;
    ssize_t w_byte = send(fd_, w_buffer.data(), w_buffer.size());

    if(w_byte > 0){
        w_buffer.erase(0, w_byte);
        LOG_DEBUG("");
        return true;
    }

    if(w_byte < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            LOG_DEBUG("Send would block, waiting next opportunity");
            return true; // 非阻塞模式下正常情况
        }
        LOG_ERROR("Send error on fd " + std::to_string(fd_));
    }
    return false; // 其他错误需要关闭连接
}

#endif