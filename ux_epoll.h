#ifndef UX_EPOLL_H
#define UX_EPOLL_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "log_show.h"

class ux_epoll
{
public:
    ux_epoll();

    void open_epoll(int port);



protected:
    int v_max_ev = 100;
    int v_max_read = 4096;
    int epollfd = 0;//epoll的描述符，用于添加和移除连接

    void epoll_del(int fd);//从epoll移除套接字
    void epoll_add(int fd);//套接字加入epoll

    int init_port(int port);//放入监听端口号,返回套接字

    //交给子类
    virtual void sock_new(int fd,sockaddr_in sock);//新连接加入之后
    virtual void sock_close(int fd);//关闭连接之前
    virtual int sock_read(int fd);//读取数据


};

#endif // UX_EPOLL_H
