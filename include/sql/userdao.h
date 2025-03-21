#pragma once

#include <memory>
#include <cppconn/prepared_statement.h>
#include "sqlconnpool.h"
#include "log.h"


class UserDao{
    public:
        explicit UserDao(SqlConnPool &pool): pool_(pool) {};
        // 验证用户
        bool validata(const std::string &username, const std::string &password);
        bool create(const std::string &username, const std::string &password);
    
    private:
        SqlConnPool& pool_;
};

bool UserDao::create(const std::string &username, const std::string &password)
{
    SqlGuard conn(pool_);

    try{
        auto stmt = conn->prepareStatement(
            "INSERT INTO users (username, password) VALUES (?, ?)");
        stmt -> setString(1, username);
        stmt -> setString(2, password);
        return stmt->executeUpdate() > 0;
    }
    catch (sql::SQLException &e){
        if(e.getErrorCode() == 1062){
            return false;
        }
        LOG_ERROR("创建用户错误" + std::string(e.what()));
        throw;
    }
    return false;
}
bool  UserDao::validata(const std::string &username, const std::string &password){
    SqlGuard conn(pool_);
    try{
        auto stmt = conn->prepareStatement(
            "SELECT password FROM users WHERE username = ?");
        stmt->setString(1, username);
        auto res = stmt->executeQuery();
        if(res -> next()){
            return res->getString("password") == password;
        }
        return false;
    }
    catch (sql::SQLException &e)
    {
        LOG_ERROR("用户验证SQL错误" + std::string(e.what()));
        throw;
    }
}