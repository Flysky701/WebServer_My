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

class Router{
    
    public:
        using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;
        void add_route(const std::string &path, const std::string &method,Handler handler){
            routes_[path][method] = handler;
        }

        bool HandleRequest(const HttpRequest& req, HttpResponse& res){
            auto path_it = routes_.find(req.path());
            if(path_it == routes_.end())
                return false;
            
            auto method_it = path_it -> second.find(req.method());
            if(method_it == path_it -> second.end())
                return false;
            method_it->second(req, res);
            return true;
        }
    private:
        std::unordered_map<
            std::string, std::unordered_map<
                std::string, std::function<void(const HttpRequest &, HttpResponse &)>>> routes_;
            
};