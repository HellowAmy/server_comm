#include "ux_epoll.h"

ux_epoll::ux_epoll()
{
    vpoll = new vpool_th(10);

    sock_new = [=](channel &ch){

        for(int i=0;i<10000;i++)
        {
            ch.send_msg("sock_new channel asjdvagsdfuyasd");
        }
        vlogd("sock_new: " vv(ch.get_fd()));
    };

    sock_read = [=](channel &ch,const string &msg){

        {
            unique_lock<mutex> lock(_mutex_send);
            vlogd("sock_read: " vv(msg) vv(this_thread::get_id()));
            if(ch.send_msg(msg) == false)
            { vlogw("========send_msg======="); }
        }


//        ch._fd
    };

}

ux_epoll::~ux_epoll()
{
    delete vpoll;
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

void ux_epoll::event_new(int fd)
{
    channel ch(fd);
    if(sock_new) sock_new(ch);
}

void ux_epoll::event_read(int fd)
{
    vlogf("read_th: " vv(this_thread::get_id()));
    string msg;
    channel ch(fd);
    ch.close_cb = bind(&ux_epoll::epoll_del,this,_1);
//    ch.read_msg(msg);
    if(sock_read) sock_read(ch,msg);
}

void ux_epoll::parse_buf(int fd,const char *buf,size_t size)
{
    size_t all_len = 0;
    string all_content;

    //查找是否有上次剩余部分
    auto it = map_read_cb.find(fd);
    if(it != map_read_cb.end())
    {
        all_len += it->second.len + size;
        all_content += it->second.content + string(buf,size);
    }
    else
    {
        all_len = size;
        all_content = string(buf,size);
    }


//    static int ic=0;
//    ic++;
//    vlogw("\n\n in len: " vv(ic) vv(size) vv(all_len));

    //循环解析，一次读取可能有多个任务包
    bool is_break = false;
    while (true)
    {
        //超过八个字节（判断是否能完整读出头部大小）
        if(all_len > sizeof(all_len))
        {
            //解析出ct_msg结构体的信息--长度
            size_t con_len = *(size_t*)string(all_content.begin(),
                                all_content.begin()+sizeof(all_len)).c_str();

            //判断目前剩余量是否大于等于一个包的长度
            if((all_len - sizeof(all_len)) >= con_len)
            {
                string buf_content(all_content,sizeof(all_len),con_len);//解析的内容

//                static int i=0;
//                i++;
//                vlogw("show: " vv(buf_content) vv(i) vv(con_len) vv(buf_content.size()));

                //重置信息剩余量
                all_len -= sizeof(all_len) + con_len;
                all_content = string(all_content.begin() +
                                sizeof(all_len) + con_len,all_content.end());
                channel ch(fd);
                ch.close_cb = bind(&ux_epoll::epoll_del,this,_1);
                vpoll->add_work(sock_read,ch,buf_content);
            }
            else is_break = true;
        }
        else is_break = true;

        if(is_break)
        {
            //如果已经存在则插入剩余容器
            if(it != map_read_cb.end())
            {
                it->second.len = all_len;
                it->second.content = all_content;
            }
            else
            {
                ct_msg ct;
                ct.len = all_len;
                ct.content = all_content;
                map_read_cb.emplace(fd,ct);
            }
            break;
        }
    }
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
                    vlogd("新连接--成功加入:" vv(str_ip) vv(port) vv(clientsock));//

                    channel ch(clientsock);
                    if(sock_new) sock_new(ch);
//                    int fd = events[i].data.fd;
//                    vpoll->add_work(&ux_epoll::event_new,this,fd);
                }
                else vlogw("新连接--接入失败");
            }

            //事件触发:有数据可读,或者连接断开
            else if (events[i].events & EPOLLIN)
            {
                char buf[1024];
                memset(buf,0,sizeof(buf));
                size_t size = read(events[i].data.fd,&buf,sizeof(buf));

//                for(int i=0;i<100;i++)
//                {
//                    printf("[%c] ",buf[i]);
//                }
//                cout<<"============"<<endl;

                //发生了错误或socket被对方关闭
                if(size <= 0)
                {
                    //把已断开的客户端从epoll中删除
                    if(epoll_del(events[i].data.fd) != 0)
                    { vlogw("epoll_del err"); continue; }
                    vlogd("客户端断开，断开的fd:" vv(events[i].data.fd));
                }
                else
                { parse_buf(events[i].data.fd,buf,size); }

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
