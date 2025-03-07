#ifndef LOG_H
#define LOG_H

#include<string>
#include<mutex>
#include<iostream>
#include<fstream>
#include<sstream>
#include<iomanip>

class Log{
    public:
        enum Level{ DEBUG, INFO, WARN, ERROR };

        static Log& instance(){
            static Log instance;
            return instance;
        }

        void init(const std::string& filepath = "./webserver.log", Level consoleLevel = INFO){}

    private:
        std::ostream file_;
        Level lv_current;
        Level lv_console;
        std::mutex mtx;


        void formatLog(std::ostream &os, Level lv){
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>
            (now.time_since_epoch()) %1000;
            auto t = std::chrono::system_clock::to_time_t(now);

            os << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
               << '.' << std::setfill('0') << std::setw(3) << ms.count()
               << " [" << (lv) << "] ";
        }

        std::string tostr(Level lv){
            
        }
};

#endif