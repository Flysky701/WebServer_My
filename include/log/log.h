#ifndef LOG_H
#define LOG_H

#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <ctime>

class Log {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };

    static Log& instance() {
        static Log instance;
        return instance;
    }

    void init(const std::string& filepath = "./webserver.log", Level consoleLevel = INFO, Level currentLevel = INFO) {
        std::lock_guard<std::mutex> lock(mtx);
        file_.open(filepath, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "Failed to open log file: " << filepath << std::endl;
        }
        lv_console = consoleLevel;
        lv_current = currentLevel;
    }

    template<typename... Args>
    void log(Level lv, const std::string &format, Args... args) {
        if (lv < lv_current) return;

        std::stringstream ss;
        formatLog(ss, lv);
        ss << formatMessage(format, args...);

        std::lock_guard<std::mutex> lock(mtx);
        if (file_.is_open()) {
            file_ << ss.str() << std::endl;
            file_.flush(); // 确保及时写入
        }
        if (lv >= lv_console) {
            std::cout << ss.str() << std::endl;
        }
    }

private:
    Log() : lv_current(INFO), lv_console(INFO) {}

    std::ofstream file_;
    Level lv_current;
    Level lv_console;
    std::mutex mtx;

    void formatLog(std::ostream &os, Level lv) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto t = std::chrono::system_clock::to_time_t(now);

        os << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count()
           << " [" << Lvstr(lv) << "] ";
    }

    std::string Lvstr(Level lv) {
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

    template<typename... Args>
    std::string formatMessage(const std::string& msg, Args... args) {
        std::vector<char> buffer(4096);
        snprintf(buffer.data(), buffer.size(), msg.c_str(), args...);
        return std::string(buffer.data());
    }
};

#define LOG_DEBUG(format, ...) Log::instance().log(Log::DEBUG, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...)  Log::instance().log(Log::INFO,  format, ##__VA_ARGS__)
#define LOG_WARN(format, ...)  Log::instance().log(Log::WARN,  format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::instance().log(Log::ERROR, format, ##__VA_ARGS__)

#endif