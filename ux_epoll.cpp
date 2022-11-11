#include "ux_epoll.h"

ux_epoll::ux_epoll()
{

}

int ux_epoll::init_port(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0); //设置TCP连接模式
    if (sock < 0)
    {
        vlog("socket连接失败");
        return -1; //错误返回
    }

    int opt = 1;
    unsigned int len = sizeof(opt);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, len); //打开复用
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &opt, len); //打开心跳

    //设置网络连接模式
    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;				  // TCP协议族
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); //监听所有
    servaddr.sin_port = htons(port);			  //兼容端口

    //监听或绑定失败返回错误 | listen函数 参数2：正在连接的队列容量
    if (bind(sock, (struct sockaddr *)&servaddr,
             sizeof(servaddr)) < 0 || listen(sock, 5) != 0)
    {
        vlog("监听端口初始化失败");
        close(sock);
        return -1;
    }

    return sock;
}

void ux_epoll::sock_new(int fd, sockaddr_in sock)
{

}

void ux_epoll::open_epoll(int port)
{
    //存放epool事件的数组
    struct epoll_event events[v_max_ev];

    //获取套接字--监听
    int listensock = init_port(port);

    if(listensock <= 0)
    {
        vlog("监听套接字初始化失败 --程序退出");
        return;
    }

    vlog("启动epoll成功: %d",port);

    //创建一个epoll描述符
    epollfd = epoll_create(1);

    if(epollfd <= 0)
    {
        vlog("epoll创建失败 --程序退出");
        return;
    }

    //将监听套接字加入epoll
    epoll_add(listensock);

    //epoll进入监听状态
    while (true)
    {
        //等待监视的socket有事件发生 | 参数4设置超时时间,-1为无限制
        int infds = epoll_wait(epollfd, events, v_max_ev, -1);

        if (infds < 0)
        {
            vlog("epoll_wait启动失败");
            break; //返回失败，退出
        }

        //遍历有事件发生的结构数组
        for (int i = 0; i < infds; i++)
        {
            //EPOLLIN事件：（1）新连接 （2）有数据可读 （3）连接正常关闭
            //（1）新连接,响应套接字等于监听套接字listensock
            if ((events[i].data.fd == listensock)
                    && (events[i].events & EPOLLIN))
            {
                vlog("进入新连接");

                //接收客户端的套接字
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock,(struct sockaddr *)&client, &len);

                //新连接进入
                if (clientsock != -1)
                {
                    //把新的客户端添加到epoll中
                    epoll_add(clientsock);

                    const char *str_ip = inet_ntoa(client.sin_addr);
                    int port = ntohs(client.sin_port);
                    vlog("新连接--成功加入 : (ip:%s) (port:%d)",str_ip,port);

                    sock_new(events[i].data.fd,client);
                }
                else vlog("新连接--接入失败");
            }

            //事件触发:有数据可读,或者连接断开
            else if (events[i].events & EPOLLIN)
            {
                int size = sock_read(events[i].data.fd);

                //发生了错误或socket被对方关闭
                if(size <= 0)
                {
                    sock_close(events[i].data.fd);

                    //把已断开的客户端从epoll中删除
                    epoll_del(events[i].data.fd);
                    vlog("客户端断开，断开的fd: %d",events[i].data.fd);
                }

            }//<<事件触发

        }//<<遍历有事件发生的结构数组

    }//<<epoll进入监听状态
}

void ux_epoll::sock_close(int fd)
{

}

int ux_epoll::sock_read(int fd)
{
    return fd;
}

void ux_epoll::epoll_del(int fd)
{
    //把已断开的客户端从epoll中删除--临时获取事件结构体
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    close(fd);
}

void ux_epoll::epoll_add(int fd)
{
    //把新的客户端添加到epoll中--临时获取事件结构体
    struct epoll_event ev;
    memset(&ev,0,sizeof(struct epoll_event));
    ev.data.fd = fd;//套接字
    ev.events = EPOLLIN;

    //将套接字加入epoll监听队列中,既可监听
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}
