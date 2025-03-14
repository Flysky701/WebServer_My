#include <string> 
#include "log.h"  
#include "server.h"

int main(){
    Log::instance().init("Webserver.log", Log::DEBUG, Log::DEBUG);
    try{
        Server server(8080);
        server.Run();
    }catch(const std::exception& e){
        LOG_ERROR( "服务器错误:" + std::string(e.what()));
    }
    return 0;
}