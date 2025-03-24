#pragma once
#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <thread> 
#include "logqueue.h"

class Log {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    static Log& instance() {
        static Log instance;
        return instance;
    }
    ~Log(){
        if(worker_.joinable())worker_.join();
        if(file_.is_open())file_.close();
        logqueue_.shutdown();
    }

    void init(const std::string& filepath, Level consoleLevel, Level currentLevel);
    template <typename... Args>
    void log(Level lv, const std::string &format, Args... args);

private:
    std::ofstream file_;
    std::atomic<Level> lv_current;
    std::atomic<Level> lv_console;
    std::mutex mtx_;
    std::thread worker_;
    LogQueue logqueue_;


    Log() : lv_current(INFO), lv_console(INFO) {}
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    void ProcessLogs(){
        LogQueue::Logentry log_entry;
        while(logqueue_.pop(log_entry)){
            std::lock_guard<std::mutex> lock(mtx_);
            if(file_.is_open()){
                file_ << log_entry.msg << std::endl;
                file_.flush();
            }
            if(log_entry.inputConsole)
                std::cout << log_entry.msg << std::endl;
        }
    }

    void formatLog(std::ostream &os, Level lv);
    std::string Lvstr(Level lv);
    template <typename... Args>
    std::string formatMessage(const std::string &msg, Args... args);
    template <typename F>
    std::string to_string_helper(F &&arg); 
};



void Log::init(const std::string& filepath = "./webserver.log", Level consoleLevel = DEBUG, Level currentLevel = DEBUG){
    
    std::lock_guard<std::mutex> lock(mtx_);
    file_.open(filepath, std::ios::app);
    
    if (!file_.is_open()) {
        std::cerr << "无法打开文件: " << filepath << std::endl;
    }
    
    lv_console = consoleLevel;
    lv_current = currentLevel;

    worker_ = std::thread(&Log::ProcessLogs, this);
}

template<typename... Args>
void Log::log(Level lv, const std::string &format, Args... args) {
    if (lv < lv_current && lv < lv_console) return;

    std::stringstream ss;
    formatLog(ss, lv);
    ss << formatMessage(format, args...);
    logqueue_.push({ss.str(), lv >= lv_console});
}

template<typename... Args>
std::string Log::formatMessage(const std::string& msg, Args... args) {
    std::vector<std::string> arg_string_  = {to_string_helper(std::forward<Args>(args))...};
    std::ostringstream oss;

    size_t pos = 0, args_idx = 0;
    size_t place_hold = msg.find("{}");
    while(place_hold != std::string::npos){
        oss << msg.substr(pos, place_hold - pos);
        oss << (args_idx < arg_string_.size() ? arg_string_[args_idx ++] : "{}");
        
        pos = place_hold + 2;
        place_hold = msg.find("{}", pos);
    }
    oss << msg.substr(pos);
    while(args_idx < arg_string_.size())oss << arg_string_[args_idx ++];
    return oss.str();
}

std::string Log::Lvstr(Level lv) {
    switch (lv) {
        case DEBUG: 
            return "DEBUG";
        case INFO:  
            return "INFO";
        case WARN:  
            return "WARN";
        case ERROR: 
            return "ERROR";
        default:    
            return "UNKNOWN";
    }
}
void Log::formatLog(std::ostream &os, Level lv) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto t = std::chrono::system_clock::to_time_t(now);

    os << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count()
       << " [" << Lvstr(lv) << "] ";
}
template<typename F>
std::string Log::to_string_helper(F &&args){
    std::ostringstream oss;
    oss << std::forward<F>(args);
    return oss.str();
}

#define LOG_DEBUG(format, ...) Log::instance().log(Log::DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::instance().log(Log::INFO,  format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::instance().log(Log::WARN,  format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::instance().log(Log::ERROR, format, ##__VA_ARGS__)