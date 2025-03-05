#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>


void Fail(std::string word){
    std::cout << "Fail on ";
    std::cout << word << std::endl;
    return;
}
int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0){
        Fail("TPC");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // IPV4;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if(bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0){ 
        Fail("bind");
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 5) < 0){
        Fail("listen");
        close(server_fd);
        return 1;
    }

    std::cout << "Server start" << std::endl;

    while(1){
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if(client_fd < 0){
            std::cout << "Fail on conn" << std::endl;
            continue;
        }

        char buffer[1024] = {0};
        ssize_t read = recv(client_fd, buffer, sizeof(buffer), 0);
        if(read < 0){
            Fail("read");
            close(client_fd);
            continue;
        }

        std::string resp = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "\r\n"
                           "<h1>欢迎访问C++服务器！</h1>"
                           "<p>当前连接处理方式：单线程</p>";
        send(client_fd, resp.c_str(), resp.size(), 0);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}