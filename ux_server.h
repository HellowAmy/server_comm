#ifndef UX_SERVER_H
#define UX_SERVER_H

#include <map>
#include <functional>
#include <string>
//#include

#include "ux_epoll.h"
#include "ux_protocol.h"

using std::function;
using std::map;
using std::pair;
using std::bind;
using std::placeholders::_1;

class ux_server : public ux_epoll
{
public:
    ux_server();

protected:
    int fd_task = -1;

    map<enum_transmit,function<int(int)>> map_func;

    void sock_new(int fd, sockaddr_in sock) override;
    void sock_close(int fd) override;
    int sock_read(int fd) override;

    //===== 任务函数特点：参数为fd，返回两次读取的总和 =====
    int task_register(int fd);//账号注册
    int task_login(int fd);//登录请求
    int task_logout(int fd);//账号退出

    //===== 任务函数特点：参数为fd，返回两次读取的总和 =====
};

#endif // UX_SERVER_H
