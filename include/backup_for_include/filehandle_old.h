#pragma once

#include <string>
#include <fstream>
#include "router.h"
#include "httprequest.h"
#include "httpresponse.h"

class FileHandler
{
public:
    explicit FileHandler(const std::string &base_dir)
        : base_dir_(base_dir) {}

    bool handle_request(const HttpRequest &req, HttpResponse &res)
    {
        if (req.method() != "GET" && req.method() != "HEAD")
            return false;

        std::string path = resolve_path(req.path());

        if (!is_safe_path(path))
        {
            res.set_status(403);
            return true;
        }
        std::string full_path = base_dir_ + path;
        // LOG_DEBUG("尝试访问文件路径: " + full_path);

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

private:
    std::string base_dir_;

    // 路径解析相关方法
    std::string resolve_path(const std::string &path)
    {
        return path == "/" ? "/Main.html" : path;
    }
    bool is_safe_path(const std::string &path)
    {
        return path.find("..") == std::string::npos;
    }

    // 文件操作相关方法
    bool file_exists(const std::string &path)
    {
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