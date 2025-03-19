#pragma once

#include "httprequest.h"
#include "httpresponse.h"
#include "userdao.h"
#include "log.h"

class AuthHandler{
    public:
        explicit AuthHandler(UserDao &dao) : dao_(dao) {};

        void handle_login(const HttpRequest &req, HttpRequest &res);
        void handle_register(const HttpRequest &req, HttpRequest &res);

    private:
        UserDao &dao_;

        void build_succecc_response(HttpResponse &res, const std::string &msg){
            res.set_status(200)
                .set_content(R"({"status":"success", "message":")" + msg + "\"}", "application/json")
                .set_header("Access-Control-Allow-Origin", "*");
        }
        void build_error_response(HttpResponse &res, const std::string &msg){
            res.set_status(code)
                .set_content(R"({"status":"error", "message":")" + error + "\"}", "application/json")
                .set_header("Access-Control-Allow-Origin", "*");
        }

};