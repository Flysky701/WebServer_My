#pragma once

#include <memory>
#include <cppconn/prepared_statement.h>
#include "sqlconnpool.hpp"
#include "log.h"
struct UserQuota {
    uint64_t total;
    uint64_t used;
};

class UserDao{
    public:
        

        explicit UserDao(SqlConnPool &pool): pool_(pool) {};
        // 验证用户
        bool validata(const std::string &username, const std::string &password);
        bool create(const std::string &username, const std::string &password);
        int userId(const std::string &username);
        UserQuota usedInfo(const int user_id);

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
        // LOG_DEBUG("尝试创建用户{}" , username);
        
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
bool UserDao::validata(const std::string &username, const std::string &password){
    SqlGuard conn(pool_);
    try{
        auto stmt = conn->prepareStatement(
            "SELECT password FROM users WHERE username = ?");
        stmt->setString(1, username);
        auto res = stmt->executeQuery();
        LOG_DEBUG("验证用户中");
        if(res -> next()){
            return res->getString("password") == password;
        }else {
            throw std::runtime_error("用户不存在");
            return false;
        }        
    }
    catch (sql::SQLException &e)
    {
        LOG_ERROR("用户验证SQL错误" + std::string(e.what()));
        throw;
    }
}
int UserDao::userId(const std::string &username){
    SqlGuard conn(pool_);
    try{
        auto stmt = conn->prepareStatement(
            "SELECT id FROM users WHERE username = ?");
        stmt->setString(1, username);
        auto res = stmt->executeQuery();
        if(res -> next())
            return res->getInt("id");
        else
            throw std::runtime_error("用户不存在");
    }catch(sql::SQLException &e){
        LOG_ERROR("用户查询SQL错误" + std::string(e.what()));
        throw;
    }
}

UserQuota UserDao::usedInfo (const int user_id){
    SqlGuard conn(pool_);
    try{
        auto stmt = conn -> prepareStatement(
            "SELECT total_quota, used_quota FROM users WHERE id = ?"
        );
        stmt -> setInt(1, user_id);
        auto res = stmt ->executeQuery();

        if(res -> next()){
            UserQuota tmp;
            tmp.total = res->getUInt64("totle_quota");
            tmp.used = res->getUInt64("used_quota");
            return tmp;
        }else{
            throw std::runtime_error("User not found");
        }
    }catch(sql::SQLException &e){
        LOG_ERROR("用户查询SQL错误" + std::string(e.what()));
        throw;
    }
}