#pragma once 

#include <string>
#include <unordered_map>
#include <random>
#include <chrono>
#include <mutex>

#include "httprequest.hpp"

class TokenManager{
    public:
        struct Session{
            int user_id;
            time_t create_time;
        };

        std::string Generate(int user_id);
        bool Validate(std::string &token, int *out_user_id = nullptr);

        void CleanupExpired(int max_age = DEFAULT_MAX_AGE);

    private:
        static const int DEFAULT_MAX_AGE = 3600;
        std::mutex mtx_;
        std::unordered_map<std::string, Session> tokens_;

        std::string GenerateRandToken(){
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::string chars =
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "0123456789";
            static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

            std::string token(32, '\0');
            for(auto &c: token)
                c = chars[dis(gen)];
            return token;
        }

        bool IsExpired(time_t create_time, int max_age = DEFAULT_MAX_AGE){
            return time(nullptr) - create_time > max_age;
        }
};

bool TokenManager::Validate(std::string &token, int *out_user_id)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tokens_.find(token);

    if(it != tokens_.end()){
        if(IsExpired(it -> second.create_time)){
            tokens_.erase(it);
            return false;
        }
        if(out_user_id)
            *out_user_id = it->second.user_id;
        return true;
    }
    LOG_DEBUG("验证token失败");
    return false;
}

std::string TokenManager::Generate(int user_id){
    std::string token = GenerateRandToken();
    {
        std::lock_guard<std::mutex> lock(mtx_);
        tokens_[token] = {user_id, time(nullptr)};
    }
    return token;
}

void TokenManager::CleanupExpired(int max_age){
    std::lock_guard<std::mutex> lock(mtx_);
    auto now = time(nullptr);
    for(auto it : tokens_){
        if(IsExpired(it.second.create_time))
            tokens_.erase(it.first);
    }
}