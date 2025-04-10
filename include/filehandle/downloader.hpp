#pragma once

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "httpresponse.hpp"
#include "connection.hpp"
#include "log.h"

class DownLoader{
    public:
        static bool HandleDownload(const std::string &path, HttpResponse &res, int sent_fd){
            FileRall file_fd(open(path.c_str(), O_RDONLY));
            if(file_fd.GetFd() < 0){
                LOG_ERROR("打开文件失败: {} - {}", path, strerror(errno));
                return false;
            }

            struct stat file_stat;
            if (fstat(file_fd.GetFd(), &file_stat) < 0){
                LOG_ERROR("获取文件状态失败: {} - {}", path, strerror(errno));
                return false;
            }

            res.set_header("Content-Length", std::to_string(file_stat.st_size))
                .set_header("Content-Type", "application/octet-stream")
                .set_keep_alive(true);
            LOG_DEBUG("传输数据(Downloader)");

            return SendFile(sent_fd, file_fd.GetFd(), file_stat.st_size);
        }
    private:
        class FileRall{
            public:
                explicit FileRall(int fd): fd_(fd) {};
                ~FileRall(){ if(fd_ >= 0) close(fd_); }
                int GetFd() { return fd_; };
            private:
                int fd_;
        };

        static bool SendFile(int sock_fd, int file_fd, size_t file_size){
            off_t offset = 0;
            ssize_t sent = 0;
            size_t remaining = file_size;

            while (remaining > 0)
            {
                sent = sendfile(sock_fd, file_fd, &offset, remaining);

                if(sent <= 0){
                    if(errno == EAGAIN)
                        continue;
                    LOG_ERROR("文件传输失败: {}", strerror(errno));
                    return false;
                }
                remaining -= sent;
            }
            return true;
        }
};

