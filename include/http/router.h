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

class Router
{
    public: 
        void add_route(const std::string& path, const std::string method,
            std::function<void(const HttpRequest&, const HttpResponse&)> handler){
            routes_[path][method] = handler;
        }
    private:
        std::unordered_map<
            std::string, std::unordered_map<
            std::string, std::function<void(const HttpRequest&, const HttpResponse&)>>>
            

};