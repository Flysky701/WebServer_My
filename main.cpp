#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h> // 添加 inet_ntop 函数头文件

int main()
{
    // 1. 创建 socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        perror("socket");
        return 1;
    }

    // 2. 绑定地址
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(8888);
    if (bind(listenfd, (sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind");
        close(listenfd);
        return 1;
    }

    // 3. 监听
    if (listen(listenfd, 5) == -1)
    {
        perror("listen");
        close(listenfd);
        return 1;
    }

    std::cout << "Server running on port 8888..." << std::endl;

    // 4. 接受连接
    sockaddr_in cli_addr{};
    socklen_t cli_len = sizeof(cli_addr);
    int connfd = accept(listenfd, (sockaddr *)&cli_addr, &cli_len);
    if (connfd == -1)
    {
        perror("accept");
        close(listenfd);
        return 1;
    }

    // 打印客户端 IP
    char cli_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, INET_ADDRSTRLEN);
    std::cout << "Client connected: " << cli_ip << std::endl;

    // 5. 发送 HTTP 响应
    const char *msg =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 18\r\n"
        "\r\n"
        "Hello from server!";
    write(connfd, msg, strlen(msg));

    // 6. 关闭连接
    close(connfd);
    close(listenfd);
    return 0;
}