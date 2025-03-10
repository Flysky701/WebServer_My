#ifndef EPOLLMANAGER_H
#define EPOLLMANAGER_H

#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>
#include <string>
#include "log.h"

class EpollManager{
    public:
        EpollManager();
        ~EpollManager();
        void AddFd(int fd, uint32_t events);
        void ModifyFd(int fd, uint32_t events);
        void RemoveFd(int fd, uint32_t events);
        int WaitEvents();
        struct epoll_event *GetEvent(){
            return events;
        }

    private:
        int epoll_fd;
        static const int MAX_EVENTS = 1024;
        struct epoll_event events[MAX_EVENTS];
};

EpollManager::EpollManager(){
    epoll_fd = epoll_create1(1);
    if(epoll_fd < 0){
        LOG_ERROR("创建EPOLL实例失败");
        throw std::system_error(errno, std::system_category());
    }
    LOG_INFO("成功创建EPOLL实例");
}
EpollManager::~EpollManager(){
    if(epoll_fd >= 0){
        // close(epoll_fd);
        LOG_INFO("EPOLL实例关闭");
    }
}
void EpollManager::AddFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.events = events; // 设置监听事件类型
    ev.data.fd = fd;    // 携带文件描述符信息

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0){
        LOG_ERROR("添加FD到EPOLL失败");
        throw std::system_error(errno, std::system_category());
    }
    LOG_DEBUG("添加 fd" + std::to_string(fd) + "到EPOLL中");
}

void EpollManager::ModifyFd(int fd, uint32_t events){
    struct epoll_event ev;
    ev.events = events; // 设置监听事件类型
    ev.data.fd = fd;    // 携带文件描述符信息

    // 参数 EPOLL_CTL_MOD
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        LOG_ERROR("修改FD到EPOLL失败");
        throw std::system_error(errno, std::system_category());
    }
    LOG_DEBUG("修改 fd" + std::to_string(fd) + "到EPOLL中");
}
void EpollManager::RemoveFd(int fd, uint32_t events){
    // EPOLL_CTL_DEL 表示移除监控（最后一个参数可忽略）
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0)
    {
        LOG_ERROR("从EPOLL移除FD失败");
        throw std::system_error(errno, std::system_category());
    }
    LOG_DEBUG("Removed fd " + std::to_string(fd) + " from epoll");
}

int EpollManager::WaitEvents(){
    int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if(num_events < 0){
        LOG_ERROR("错误发生于epoll_wait");
        throw std::system_error(errno, std::system_category());
    }
}

#endif
