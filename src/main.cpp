#include <string> 
#include "log.h"  
#include "server.h"


int main(){
    //                                    控制台     日志
    Log::instance().init("Webserver.log", Log::INFO, Log::INFO);
    SqlConnPool &db_pool = SqlConnPool::instance();
    db_pool.init("127.0.0.1", "root", "123456", "mydb", 3306, 10);
    try{
        Server server(8080);
        server.Run();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR( "服务器错误:" + std::string(e.what()));
    }
    return 0;
}