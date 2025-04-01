#pragma once
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <vector>
#include <string.h>
#include <string>

#include "httpresponse.h"  
#include "log.h"

class UpLoader{
    public:
        static bool UploadHandle(const std::string &save_path, const HttpRequest &req, HttpResponse &res){
            FileRall file_fd(open(save_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644));
            if (file_fd.GetFd() < 0){
                LOG_ERROR("创建文件失败： {}", strerror(errno));
                res.set_status(500);
                return false;
            }
            const std::string &file_data = req.body();

            bool check = AutoWriteData(file_fd, file_data);

            if(!check){
                res.set_status(500);
                return false;
            }
            res.set_status(201);
            return true;
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

        static bool AutoWriteData(FileRall &file, const string &data){
            if (data.size() > 1024 * 1024)
                return MmpWrite(file, data);
            else
                return StreamWrite(file, data);
        }

        static bool StreamWrite(FileRall &file, const string &data){
            ssize_t total = 0;
            const char *buffer = data.data();
            size_t remaining = data.size();

            while(remaining > 0){
                ssize_t written = write(file.GetFd(), buffer + total, remaining);
                if(written < 0){
                    if(errno == EAGAIN)
                        continue;
                    LOG_ERROR("");
                    return false;
                }
                total += written;
                remaining -= written;
            }
            return false;
        }
        static bool MmpWrite(FileRall &file, const string &data){
            if(ftruncate(file.GetFd(), data.size()) < 0)
                return false;

            void *map = mmap(nullptr, data.size(), PROT_WRITE, MAP_SHARED, file.GetFd(), 0);

            if(map == MAP_FAILED)
                return false;
            
            memcpy(map, data.data(), data.size());
            munmap(map, data.size());
            return true;
        }
};