#include <stdio.h>

#include "ux_epoll.h"
#include "ux_server.h"

int main()
{
    printf("Hello World!\n");

//    ux_epoll aep;

//    aep.open_epoll(5005);

    ux_server u;
    u.open_epoll(5005);



    printf("===== end =====\n");




    return 0;
}
