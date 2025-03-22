#pragma once

#include "httprequest.h"
#include "httpresponse.h"

#include "userdao.h"
#include "log.h"

class AuthHandler{
    public:
        explicit AuthHandler(UserDao& dao) : dao_(dao) {};

        void handle_login(const HttpRequest &req, HttpResponse &res);
        void handle_register(const HttpRequest &req, HttpResponse &res);

    private:
        UserDao &dao_;

        void build_success_response(HttpResponse &res, const std::string &msg){
            res.set_status(200)
               .set_content(R"({"status":"success", "message":")" + msg + "\"}", "application/json")
               .set_header("Access-Control-Allow-Origin", "*");
        }

        void build_error_response(HttpResponse& res, int code, const std::string& error) {
            res.set_status(code)
               .set_content(R"({"status":"error", "message":")" + error + "\"}", "application/json")
               .set_header("Access-Control-Allow-Origin", "*");
        }

};


void AuthHandler::handle_login(const HttpRequest &req, HttpResponse &res){
    if(req.method() != "POST"){
        build_error_response(res, 405, "Method Not Allowed");
        return;
    }
    const auto& form_params = req.form_params();
    auto username_it = form_params.find("username");
    auto password_it = form_params.find("password");

    if(username_it == form_params.end() || password_it == form_params.end()){
        build_error_response(res, 400, "Missing username or password");
        return;
    }
    std::string username = username_it -> second;
    std::string password = password_it -> second;

    try{
        if(dao_.validata(username, password)){
            build_success_response(res, "Login successful");
        }else {
            build_error_response(res, 401, "Invalid credentials");
        }

    }catch(const std::exception &e){
        LOG_ERROR("登录错误 ：" + std::string(e.what()));
        build_error_response(res, 500, "Internal server error");
    }
}
void AuthHandler::handle_register(const HttpRequest &req, HttpResponse &res){
    LOG_DEBUG("进入函数{}", "handle_register");
    if(req.method() != "POST"){
        build_error_response(res, 405, "Method Not Allowed");
        return;
    }
    const auto& form_params = req.form_params();
    auto username_it = form_params.find("username");
    auto password_it = form_params.find("password");
    if (username_it == form_params.end() || password_it == form_params.end()) {
        build_error_response(res, 400, "Missing username or password");
        return;
    }

    std::string username = username_it->second;
    std::string password = password_it->second;

    try {
        LOG_DEBUG("AUTH层创建用户");
        if (dao_.create(username, password)) {
            build_success_response(res, "User registered");
        } else {
            build_error_response(res, 409, "Username exists");
        }
    } catch (const std::exception &e) {
        LOG_ERROR("注册错误: " + std::string(e.what()));
        build_error_response(res, 500, "Internal error");
    }
}