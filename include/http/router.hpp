#pragma once

#include <unordered_map>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include "httprequest.hpp"
#include "httpresponse.hpp"

using  std::unordered_map;
using std::string;
using std::vector;

class Router{
    
    public:
        using Middleware = std::function<bool(HttpRequest &, HttpResponse &)>;
        using Handler = std::function<void(const HttpRequest &, HttpResponse &)>;

        void add_middleware(Middleware mw){
            middlewares_.push_back(mw);
        }

        void add_route(const std::string &path, const std::string &method,Handler handler){
            routes_[path][method] = handler;
        }

        void add_token_Validate(const std::string &path, const std::string &method){
            tokenValidate_.emplace(std::make_pair(path, method));
        }

        bool IsVaildate(const HttpRequest &req){
            auto key = make_pair(req.path(), req.method());
            return tokenValidate_.count(key) > 0;
        }

        bool HandleRequest(HttpRequest &req, HttpResponse &res){
            for(auto& mw: middlewares_){
                if(!mw(req, res))
                    return false;
            }

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
        std::vector<Middleware> middlewares_;
        std::unordered_map<
            std::string, std::unordered_map<
                std::string, Handler>> routes_;
        std::set<std::pair<std::string, std::string>> tokenValidate_;
};