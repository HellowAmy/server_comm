#ifndef UX_EPOLL_H
#define UX_EPOLL_H

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <functional>

using namespace std;

//===== 日志宏 =====
#define vv(value) "["#value": "<<value<<"] "
#define vloge(...) std::cout<<"\033[31m[Err] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogw(...) std::cout<<"\033[33m[War] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogd(...) std::cout<<"\033[32m[Deb] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<"\033[0m"<<endl
#define vlogf(...) std::cout<<"[Inf] ["<<__FILE__<<":<"<<__LINE__ \
    <<">] <<<< "<<__VA_ARGS__<<endl
//===== 日志宏 =====


class ux_epoll
{
public:
    ux_epoll();
    int open_epoll(int port);

    function<int(int fd,sockaddr_in sock)> sock_new = nullptr;//新连接
    function<int(int fd)> sock_close = nullptr;//关闭连接
    function<int(int fd)> sock_read = nullptr;//读取数据

protected:
    int v_max_ev = 1024;
    int v_max_read = 4096;
    int epollfd = 0;//epoll的描述符，用于添加和移除连接

    int set_non_block(int fd);//设置为非阻塞套接字
    int epoll_del(int fd);//从epoll移除套接字
    int epoll_add(int fd);//套接字加入epoll
    int init_port(int port);//放入监听端口号,返回套接字
};

#endif // UX_EPOLL_H
