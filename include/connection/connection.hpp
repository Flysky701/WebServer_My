#pragma once

#include <vector> 
#include <string>
#include <fcntl.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <sys/sendfile.h>

#include "log.h"

class Connection{
    public:
        Connection(int socket_fd);
        ~Connection();

        Connection(const Connection &) = delete;
        Connection &operator=(const Connection &) = delete;

        bool ReadData();                         // 从socket读取数据
        bool WriteData(const std::string &data); // 写入数据到缓冲区
        bool Flush();
        void ClearReadBuffer() { r_buffer.clear(); };
        
        std::vector<char> GetReadBuffer() { 
            std::lock_guard<std::mutex> lock(r_mtx_);
            return r_buffer; 
        }
        std::string &GetWriteBuffer() { return w_buffer; }
        int GetFd() const {return fd_;}
        
        bool HasPendingWrite() const { return !w_buffer.empty();}
        
        // bool SendFileData();
        void SetFileToSend(int fd, size_t size){ 
            file_fd_ = fd, file_size_ = size, file_sent_ = 0;
        }

    private:
    
        int fd_;
        std::vector<char> r_buffer;
        std::string w_buffer;

        // 文件类， 用于发送。。
        int file_fd_ = -1;     // 待发送的文件描述符
        size_t file_size_ = 0; // 文件总大小
        size_t file_sent_ = 0; // 已发送字节数

        std::mutex r_mtx_;
        std::mutex w_mtx_;
        
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

Connection::Connection(int socket_fd) : fd_(socket_fd){
    if (fd_ < 0) 
        throw std::invalid_argument("Invalid socket descriptor");
    SetNonBlocking();
    LOG_DEBUG("创建连接 {}" , fd_);
}

Connection::~Connection(){
    if(fd_ >= 0){
        close(fd_);
        LOG_DEBUG("关闭连接 {}" , fd_);
    }
}
bool Connection::ReadData(){
    // std::lock_guard<std::mutex> lock(r_mtx_);
    std::vector<char> buffer(4096);
    ssize_t tot_read = 0;
    while (true)
    {
        ssize_t r_bytes = recv(fd_, buffer.data(), buffer.size(), MSG_DONTWAIT);
        
        if (r_bytes > 0){
            tot_read += r_bytes;
            {
                std::lock_guard<std::mutex> lock(r_mtx_);
                r_buffer.insert(r_buffer.end(), buffer.data(), buffer.data() + r_bytes);
            }
        }
        else if (r_bytes == 0){
            return false; // 连接关闭
        }
        else{
            if (errno == EAGAIN || errno == EWOULDBLOCK)break;
            LOG_ERROR("读取错误");
            return false;
        }
    }
    LOG_DEBUG("读取到 {} 字节", tot_read);
    return true;
}

bool Connection::WriteData(const std::string& data){
    std::lock_guard<std::mutex> lock(w_mtx_);
    try{
        w_buffer += data;
        return true;
    }catch(const std::bad_alloc&){
        LOG_ERROR("line 87 on conn");
        return false;
    }
}
// 普通发送
bool Connection::Flush()
{
    if (w_buffer.empty())
    return true;
    
    ssize_t total_sent = 0;
    const char *data = w_buffer.data();
    size_t remaining = w_buffer.size();
    
    std::lock_guard<std::mutex> lock(w_mtx_);
    while (remaining > 0)
    {
        ssize_t sent = send(fd_, data + total_sent, remaining, 0);
        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                break; // 非阻塞模式下无法继续发送
            }
            else{
                LOG_ERROR("发送错误: " + std::string(strerror(errno)));
                return false;
            }
        }
        else if (sent == 0){
            
            return false; // 连接关闭
        }
        total_sent += sent;
        remaining -= sent;
    }
    
    if (total_sent > 0){
        w_buffer.erase(0, total_sent);
    }
    
    //-------------------整合测试
    if(file_fd_ != -1){
        while(file_sent_ < file_size_){
            off_t offset = file_sent_;
            ssize_t sent = sendfile(fd_, file_fd_, &offset, file_size_ - file_sent_);
            if (sent < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK){
                    break;
                }
                LOG_ERROR("文件发送失败");
                close(file_fd_);
                file_fd_ = -1;
                return false; // 发送失败
            }
            else if (sent == 0 || file_sent_ + sent >= file_size_){
                close(file_fd_);
                file_fd_ = -1;
                return true;
            }
            file_sent_ += sent;
        }
    }
    //-------------------
    return true;
}
// bool Connection::SendFileData(){
//     if(file_fd_ == -1)return true;
//     off_t offset = file_sent_;
//     ssize_t sent = sendfile(fd_, file_fd_, &offset, file_size_ - file_sent_);

//     if (sent < 0){
//         if (errno == EAGAIN || errno == EWOULDBLOCK)
//         {
//             return false; // 需要下次继续发送
//         }
//         close(file_fd_);
//         file_fd_ = -1;
//         return false; // 发送失败
//     }
//     else if (sent == 0 || file_sent_ + sent >= file_size_){
//         close(file_fd_);
//         file_fd_ = -1;
//         return true; // 发送完成
//     }

//     file_sent_ += sent;
//     return false; // 还有剩余数据
// }