#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <string>
// 已经实现的功能， 功能较为初级， 后续可扩展
#include "threadpool.h"
#include "httprequest.h"
#include "log.h"

void CallFail(std::string word)
{
    std::cout << "Fail on ";
    std::cout << word << std::endl;
    return;
}

int main()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        CallFail("TPC");
        return 1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET; // IPV4;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        CallFail("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) < 0)
    {
        CallFail("listen");
        close(server_fd);
        return 1;
    }

    ThreadPool pool(5);
    Log::instance().init("server.log");
    LOG_INFO("server start");

    while (true)
    {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            LOG_ERROR("Fail on conn", client_fd);
            std::cout << "Fail on conn" << std::endl;
            continue;
        }

        auto fun = [client_fd]
        {
            std::vector<char> buffer(4096);
            ssize_t read = recv(client_fd, buffer.data(), buffer.size() - 1, 0);

            HttpRequest reqst;
            if (!reqst.parse(buffer))
            {
                LOG_ERROR("Fail on Httprequest");
                close(client_fd);
                return;
            }

            std::string resp = "HTTP/1.1 200 OK\r\n";
            resp += "Content-Type: text/html\r\n";
            resp += "Connection: " + std::string(reqst.keep_alive() ? "keep-alive" : "close") + "\r\n";
            resp += "\r\n<h1>Hello from C++ Server!</h1>";

            resp += "\r\n<h1>" + resp + "</h1>"; // test of Http

            send(client_fd, resp.c_str(), resp.size(), 0);
            close(client_fd);
            return;
        };
        pool.enqueue(fun);
    }

    close(server_fd);
    return 0;
}