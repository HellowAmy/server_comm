#include "ux_epoll.h"


struct ct_msg
{
    size_t len;
    string content;
};

size_t readn(int sock, void *buf, size_t len)
{
    size_t all = len;
    char *pos = (char *)buf;

    while (all > 0)
    {
        size_t size = read(sock,pos,all);
         if (size == -1u)
         {
             if (errno == EINTR) size = 0;
             else return -1;
         }
         else if (size == 0) break;
         pos += size;
         all -= size;
    }
    return len - all;
}

bool read_msg(int sock,ct_msg &msg)
{
    size_t size = readn(sock,&msg.len,sizeof(msg.len));
    cout<<"msg:len:  "<<msg.len<<endl;
    if(size == -1u) return false;

    char huf[1000];
    if(readn(sock,huf,size) == -1u)//msg.content.data()
    { return false; }

    cout<<"huf: " <<huf<<endl;
    return true;
}






ux_epoll::ux_epoll()
{
    sock_read = [](int sock){
        ct_msg msg;
        if(read_msg(sock,msg))
        {
            cout<<"buf: "<<msg.content<<endl;
            cout<<"size: "<<msg.len<<endl;
        }

//        char buf[4096];
//        memset(buf,0,sizeof(buf));
//        size_t ret = recv(sock, buf,sizeof(buf),MSG_WAITALL);
//        cout<<"buf: "<<buf<<endl;
//        cout<<"size: "<<ret<<endl;
        return 100;
    };
}

//!
//! 返回值：
//!     -1：socket打开失败
//!     -2：bind建立失败
//!     sock：返回成功，建立的套接字
//!
int ux_epoll::init_port(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0); //设置TCP连接模式
    if (sock < 0) { return -1; }

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
             sizeof(servaddr)) < 0 || listen(sock, v_max_ev) != 0)
    { close(sock); return -2; }

    return sock;
}

//!
//! 返回值：
//!      0：正常退出
//!     -1：监听失败
//!     -2：创建epoll失败 epoll_wait
//!     -3：epoll_wait失败
//!     -4：监听套接字加入epoll失败
//!
int ux_epoll::open_epoll(int port)
{
    int listensock = init_port(port);//获取套接字--监听
    if(listensock <= 0) { return -1; }

    vlogd("启动epoll成功:" vv(port));

    //创建一个epoll描述符
    //参数1：无效数，不过要求必须大于0
    epollfd = epoll_create(1);
    if(epollfd <= 0) { return -2; }

    if(epoll_add(listensock) != 0) { return -4; }//将监听套接字加入epoll
    struct epoll_event events[v_max_ev];//存放epool事件的数组

    //epoll进入监听状态
    while (true)
    {
        //等待监视的socket有事件发生 | 参数4设置超时时间,-1为无限制
        //参数1：epoll描述符，参数2：epoll事件数组，
        //      参数3：同时处理的fd数量，参数4：超时（-1则无视超时时间）
        int infds = epoll_wait(epollfd, events, v_max_ev, -1);
        if (infds < 0) { return -3; }

        //遍历有事件发生的结构数组
        for (int i = 0; i < infds; i++)
        {
            //EPOLLIN事件：（1）新连接 （2）有数据可读 （3）连接正常关闭
            //（1）新连接,响应套接字等于监听套接字listensock
            if ((events[i].data.fd == listensock)
                    && (events[i].events & EPOLLIN))
            {
                //建立请求，接收客户端的套接字（accept返回之后双方套接字可通信）
                struct sockaddr_in client;
                socklen_t len = sizeof(client);
                int clientsock = accept(listensock,(struct sockaddr *)&client, &len);
                set_non_block(clientsock);

                //新连接进入
                if (clientsock != -1)
                {
                    //把新的客户端添加到epoll中
                    if(epoll_add(clientsock) != 0)
                    { vlogw("epoll_add err"); continue; }
                    const char *str_ip = inet_ntoa(client.sin_addr);
                    int port = ntohs(client.sin_port);
                    vlogd("新连接--成功加入:" vv(str_ip) vv(port));

                    if(sock_new) sock_new(events[i].data.fd,client);
                }
                else vlogw("新连接--接入失败");
            }

            //事件触发:有数据可读,或者连接断开
            else if (events[i].events & EPOLLIN)
            {
                int size = 0;
                if(sock_read) { size = sock_read(events[i].data.fd); }

                //发生了错误或socket被对方关闭
                if(size <= 0)
                {
                    if(sock_close) sock_close(events[i].data.fd);

                    //把已断开的客户端从epoll中删除
                    if(epoll_del(events[i].data.fd) != 0)
                    { vlogw("epoll_del err"); continue; }
                    vlogd("客户端断开，断开的fd:" vv(events[i].data.fd));
                }

            }//<<事件触发
        }//<<遍历有事件发生的结构数组
    }//<<epoll进入监听状态
    return 0;
}

int ux_epoll::set_non_block(int fd)
{
    //将O_NONBLOCK无堵塞选项设置到fd中
    int old_op = fcntl(fd, F_GETFL);
    int new_op = old_op | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_op);
    return old_op;
}

int ux_epoll::epoll_del(int fd)
{
    //从epoll移除fd，根据结构体信息
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    int ret = epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
    if(ret != 0) close(fd);
    return ret;
}

int ux_epoll::epoll_add(int fd)
{
    //将临时结构体加入到epoll--水平触发
    struct epoll_event ev;
    memset(&ev,0,sizeof(struct epoll_event));
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    return epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}
