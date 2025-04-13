#pragma once

#include <string>
#include <filesystem>
#include <fstream>

#include "httprequest.hpp"
#include "httpresponse.hpp"

#include "downloader.hpp"
#include "uploader.hpp"

#include "filedao.hpp"
#include "userdao.hpp"

#include "log.h"

class FileHandler
{
    public:
        explicit FileHandler(const std::string &base_dir, UserDao &user_dao, FileDao &file_dao)
            : base_dir_(base_dir), file_dao_(file_dao), user_dao_(user_dao){}

        // bool handle_request(const HttpRequest &req, HttpResponse &res, Connection &conn);
        bool static_handle(const HttpRequest &req, HttpResponse &res);
        bool handle_download(const HttpRequest &req, HttpResponse &res, int fd);
        bool handle_upload(const HttpRequest &req, HttpResponse &res);
        bool handle_userinfo(const HttpRequest &req, HttpResponse &res);
        bool handle_fileinfo(const HttpRequest &req, HttpResponse &res);
        bool handle_delfile(const HttpRequest &req, HttpResponse &res);

    private:
        std::string base_dir_;
        UserDao &user_dao_;
        FileDao &file_dao_;
        // 处理文件上传和下载的类

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

        bool file_exists(const std::string &path){
            std::ifstream f(path);
            return f.good();
        }

        std::string read_file(const std::string &path)
        {
            std::ifstream file(path, std::ios::binary);
            return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
        }
};

bool FileHandler::static_handle(const HttpRequest &req, HttpResponse &res){
    if (req.method() != "GET" && req.method() != "HEAD")
        return false;

    std::string path = resolve_path(req.path());

    if (!is_safe_path(path))
    {
        res.set_status(403);
        return true;
    }
    std::string full_path = base_dir_ + path;
    LOG_DEBUG("尝试访问文件路径: " + full_path);

    if (!file_exists(full_path))
    {
        res.set_status(404);
        return true;
    }
    std::string content = read_file(full_path);
    res.set_content(content, HttpRequest::get_mime_type(full_path)).set_keep_alive(req.keep_alive());

    if (req.method() == "HEAD")
        res.set_content("");
    return true;
}

bool FileHandler::handle_userinfo(const HttpRequest &req, HttpResponse &res){
    if(req.method() != "GET")
        return false;

    int user_id = 0;
    try{
        string key = "user_id";
        user_id = std::stoi(req.get_context(key));
    }catch (const std::exception &e){
        LOG_ERROR("获取用户ID失败: {}", std::string(e.what()));
        res.set_status(403);
        return true;
    }
    
    string username = user_dao_.username(user_id);
    auto msg = user_dao_.usedInfo(user_id);

    string json =  R"({
        "code": 200,
        "data": {
            "username": ")" + username + R"(",
            "storage_used": )" + std::to_string(msg.used) + R"(,
            "storage_total": )" + std::to_string(msg.total) + R"(
        }
    })";

    res.set_json_content(json)
        .set_status(200);
    return true;
}
// 下方为主要实现
// 需要融合 filedao 和 userdao
// 下方未测试 
bool FileHandler::handle_fileinfo(const HttpRequest &req, HttpResponse &res){
    if(req.method() != "GET")
        return false;

    int user_id = 0;
    try{
        string key = "user_id";
        user_id = std::stoi(req.get_context(key));
    }catch (const std::exception &e){
        LOG_ERROR("获取用户ID失败: {}", std::string(e.what()));
        res.set_status(403);
        return true;
    }

    auto files = file_dao_.ListFilesByUser(user_id);
    
    std::string json_body = "[";
    for (auto& fileinfo_ : files) {
        json_body += R"({
            "file_id": )" + std::to_string(fileinfo_.id) + R"(,
            "filename": ")" + fileinfo_.file_name + R"(",
            "filesize": )" + std::to_string(fileinfo_.file_size) + R"(,
            "created_at": ")" + fileinfo_.creatat + R"("
        })";
        if (&fileinfo_ != &files.back()) {
            json_body += ",";
        }
    }
    json_body += "]";
    std::string json = R"({
        "code": 200,
        "data": )" + json_body + R"(})";
    
    res.set_json_content(json)
        .set_status(200);
    return true;
}

// 下面需要重写
bool FileHandler::handle_download(const HttpRequest &req, HttpResponse &res, int sent_fd){
    
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

    if(!DownLoader::HandleDownload(full_path, res, sent_fd)){
        res.set_status(500);
        return true;
    }

    if (req.method() == "HEAD"){
        res.set_content("");
    }
    res.set_status(200);
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
