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

    int task_register(int fd);
};

#endif // UX_SERVER_H
