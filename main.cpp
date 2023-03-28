#include <stdio.h>

#include "ux_epoll.h"

int main()
{
    ux_epoll op;
    int ret = op.open_epoll(5005);

    vlogd("open_epoll ret: " vv(ret));
    printf("===== end =====\n");
    return 0;
}
