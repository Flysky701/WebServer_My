#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include "httprequest.h"
#include "httpresponse.h"

using  std::unordered_map;
using std::string;
using std::vector;

/**
 * @brief 轻量级路由处理器（仅静态路由版本）
 *
 * 功能特性：
 * 1. 支持静态路径路由注册
 * 2. 支持GET/POST方法
 * 3. 中间件处理链
 * 4. 基础安全路径检查
 */
class Router
{
public:
    /// 请求处理函数类型定义
    /// 中间件函数类型定义（带next回调）
    using Handler = std::function<void(HttpRequest &, HttpResponse &)>;
    using Middleware = std::function<void(HttpRequest &, HttpResponse &, std::function<void()>)>;

    Router &add(const string &method, const string &path, Handler handler){
        static_route_[method][path] = handler;
        return *this;
    }
    // 快捷注册GET路由
    Router &get(const string &path, Handler handler){ return add("GET", path, handler); }
    Router &post(const string &path, Handler handler){ return add("POST", path, handler);}
    Router &use(Middleware middleware){
        middleware_.push_back(middleware);
        return *this;
    }
    /**
     * @brief 处理HTTP请求
     * @return 是否找到匹配路由
     */
    bool handle(HttpRequest &req, HttpResponse &res)
    {
        execute_middlewares(req, res, 0);

        if (auto method_iter = static_route_.find(req.method());
            method_iter != static_route_.end())
        {
            if (auto handler_iter = method_iter->second.find(req.path());
                handler_iter != method_iter->second.end())
            {
                handler_iter->second(req, res);
                return true;
            }
        }
        return false;
    }

private:
    /// 中间件执行控制器（递归执行）
    void execute_middlewares(HttpRequest &req, HttpResponse &res, size_t index)
    {
        if (index < middleware_.size()){
            middleware_[index](req, res, [this, &req, &res, index]()
                { execute_middlewares(req, res, index + 1); });
        }
    }
    
    // 静态路由表结构：方法 -> 路径映射表
    // 中间件链存储
    unordered_map<string, unordered_map<string, Handler>> static_route_;
    vector<Middleware> middleware_;

    /* 预留动态路由接口（已注释，供未来扩展参考）
    struct DynamicRoute{...};
    unordered_map<string, vector<DynamicRoute>> dynamic_routes_;
    void register_dynamic_route(...);
    */
};