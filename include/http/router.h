#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include "httprequest.h"

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
    using Handler = std::function<void(HttpRequest &)>;
    using Middleware = std::function<void(HttpRequest &, std::function<void()>)>;
    /**
     * @brief 注册静态路由
     * @param method HTTP方法（GET/POST等）
     * @param path 静态路径（如"/api/data"）
     * @param handler 处理函数
     */
    Router &add(const string &method, const string &path, Handler handler){
        static_route_[method][path] = handler;
        return *this;
    }
    // 快捷注册GET路由
    Router &get(const string &path, Handler handler){ return add("GET", path, handler); }
    Router &post(const string &path, Handler handler){ return add("POST", path, handler);}
    /**
     * @brief 添加全局中间件
     * @param middleware 中间件函数
     *
     * 中间件执行顺序：先进先出
     * 示例：日志记录、权限校验等
     */
    Router &use(Middleware middleware)
    {
        middleware_.push_back(middleware);
        return *this;
    }
    /**
     * @brief 处理HTTP请求
     * @param req 解析后的请求对象
     * @return 是否找到匹配路由
     */
    bool handle(HttpRequest &req)
    {
        execute_middlewares(req, 0); // 执行中间件链

        // 在方法路由表中查找
        if (auto method_iter = static_route_.find(req.method());
            method_iter != static_route_.end())
        {
            // 匹配静态路径
            if (auto handler_iter = method_iter->second.find(req.path());
                handler_iter != method_iter->second.end())
            {
                handler_iter->second(req);
                return true;
            }
        }
        return false;
    }

private:
    /// 中间件执行控制器（递归执行）
    void execute_middlewares(HttpRequest &req, size_t index){
        if (index < middleware_.size()){
            middleware_[index](req, [this, &req, index]()
                               { execute_middlewares(req, index + 1); });
        }
    }
    /// 静态路由表结构：方法 -> 路径映射表
    /// 中间件链存储
    unordered_map<string, unordered_map<string, Handler>> static_route_;
    vector<Middleware> middleware_;

    /* 预留动态路由接口（已注释，供未来扩展参考）
    struct DynamicRoute{...};
    unordered_map<string, vector<DynamicRoute>> dynamic_routes_;
    void register_dynamic_route(...);
    */
};