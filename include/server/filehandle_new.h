#pragma once

#include <string>
#include <filesystem>
#include <fstream>

#include "httprequest.h"
#include "httpresponse.h"

#include "downloader.h"
#include "uploader.h"

class FileHandler
{
    public:
        explicit FileHandler(const std::string &base_dir)
            : base_dir_(base_dir){}

        bool handle_request(const HttpRequest &req, HttpResponse &res, Connection &conn);
        bool handle_upload(const HttpRequest &req, HttpResponse &res);

    private:
        std::string base_dir_;

        // 路径解析相关方法
        std::string resolve_path(const std::string &path) { 
            return path == "/" ? "/Main.html" : path; 
        }

        bool is_safe_path(const std::string &path) { 
            return path.find("..") == std::string::npos; 
        }

        bool ensure_directory_exists(const std::string &path){
            try{
                auto parent_dir = std::filesystem::path(path).parent_path();
                if(!std::filesystem::exists(parent_dir))
                    std::filesystem::create_directories(parent_dir);

                    return true;
            }
            catch (const std::exception &e){
                LOG_ERROR("创建目录失败：{}", std::string(e.what()));
                return false;
            }
        }

        std::string read_file(const std::string &path)
        {
            std::ifstream file(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
        }
};


bool FileHandler::handle_request(const HttpRequest &req, HttpResponse &res, Connection &conn)
{
    if (req.method() != "GET" && req.method() != "HEAD")
        return false;

    std::string path = resolve_path(req.path());

    if (!is_safe_path(path)){
        res.set_status(403);
        return true;
    }
    std::string full_path = base_dir_ + path;
    LOG_DEBUG("尝试访问文件路径: " + full_path);

    if (!ensure_directory_exists(full_path)){
        res.set_status(500);
        return true;
    }
    if(!DownLoader::HandleDownload(full_path, res, conn)){
        res.set_status(500);
        return true;
    }

    if (req.method() == "HEAD"){
        res.set_content("");
    }
    res.set_status(201);
    return true;
}

bool FileHandler::handle_upload(const HttpRequest &req, HttpResponse &res){
    if(req.method() != "POST" && req.method() != "PUT")
        return false;
    
    std:string path = resolve_path(req.path());

    if(is_safe_path(path)){
        res.set_status(403);
        return true;
    }

    std::string full_path = base_dir_ + path;
    LOG_DEBUG("尝试访问文件路径: " + full_path);

    if (!ensure_directory_exists(full_path)){
        res.set_status(500);
        return true;
    }

    if(!UpLoader::UploadHandle(full_path, req, res)){
        res.set_status(500);
        return true;
    }

    res.set_status(201);
    return true;
}