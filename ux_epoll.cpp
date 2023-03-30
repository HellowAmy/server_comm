#include "ux_epoll.h"

ux_epoll::ux_epoll()
{
    _pool = new vpool_th(10);//线程池初始化
    _pool->add_work(&ux_epoll::work_parse_th,this);//启动拆包函数线程

    //===== 回调区 =====
    sock_new = [=](shared_ptr<channel> pch,const string &ip){
        string str = "sock_new channel back";
        for(int i=0;i<10000;i++)
        {
            if(pch->send_msg(str + to_string(i)) == false)
            { vloge("== send : err =="); }
        }
        vlogd("sock_new: " vv(pch->get_fd()) vv(ip));
    };

    sock_read = [=](shared_ptr<channel> pch,const string &msg){
        {
            unique_lock<mutex> lock(_mutex);
            vlogd("sock_read: " vv(msg) vv(this_thread::get_id()));
        }

        if(pch->send_msg("back: "+msg) == false)
        { vlogw("========send_msg======="); }
    };

    sock_close = [=](shared_ptr<channel> pch){
        vlogd("channel 断开的fd :" vv(pch->get_fd()));
    };
    //===== 回调区 =====
}

ux_epoll::~ux_epoll()
{
    delete _pool;
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
             sizeof(servaddr)) < 0 || listen(sock, _size_event) != 0)
    { close(sock); return -2; }

    return sock;
}

void ux_epoll::parse_buf(int fd,const char *buf,size_t size)
{
    size_t all_len = 0;
    string all_content;

    //查找是否有上次剩余部分
    auto it = _map_save_read.find(fd);
    if(it != _map_save_read.end())
    {
        all_len += it->second.len + size;
        all_content += it->second.content + string(buf,size);
    }
    else
    {
        all_len = size;
        all_content = string(buf,size);
    }

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
                //解析的内容
                string buf_content(all_content,sizeof(all_len),con_len);

                //重置信息剩余量
                all_len -= sizeof(all_len) + con_len;
                all_content = string(all_content.begin() +
                                sizeof(all_len) + con_len,all_content.end());
                //触发读回调
                auto pch = make_shared<channel>(fd);
                pch->close_cb = bind(&ux_epoll::epoll_del,this,_1);
                if(sock_read) _pool->add_work(sock_read,pch,buf_content);
            }
            else is_break = true;
        }
        else is_break = true;

        if(is_break)
        {
            //如果已经存在则插入剩余容器
            if(it != _map_save_read.end())
            {
                it->second.len = all_len;
                it->second.content = all_content;
            }
            else
            {
                ct_message ct;
                ct.len = all_len;
                ct.content = all_content;
                _map_save_read.emplace(fd,ct);
            }
            break;
        }
    }
}

void ux_epoll::parse_buf_th(int fd,string buf)
{
    parse_buf(fd,buf.c_str(),buf.size());
}

void ux_epoll::work_parse_th()
{
    //拆包子线程，单线程执行,自行运行一个循环防止退出，并永久占用一个线程池中的子线程
    while (true)
    {
        unique_lock<std::mutex> lock(_mutex_parse);     //此处单线程启动，无需锁，用于唤醒
        while(_queue_task.empty()){ _cond.wait(lock); } //假唤醒--退出且队列为空

        //取任务并执行
        function<void()> task = std::move(_queue_task.front());
        _queue_task.pop();
        task();
    }
}

void ux_epoll::add_work(function<void()> task)
{
    _queue_task.push(task);
    _cond.notify_one();
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
    _fd_epoll = epoll_create(1);
    if(_fd_epoll <= 0) { return -2; }

    if(epoll_add(listensock) != 0) { return -4; }//将监听套接字加入epoll
    struct epoll_event events[_size_event];//存放epool事件的数组

    //epoll进入监听状态
    while (true)
    {
        //等待监视的socket有事件发生 | 参数4设置超时时间,-1为无限制
        //参数1：epoll描述符，参数2：epoll事件数组，
        //      参数3：同时处理的fd数量，参数4：超时（-1则无视超时时间）
        int infds = epoll_wait(_fd_epoll, events, _size_event, -1);
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
                    if(epoll_add(clientsock) == 0)
                    {
                        //触发新连接回调
                        string str_ip = inet_ntoa(client.sin_addr);
                        if(sock_new) _pool->add_work
                                (sock_new,make_shared<channel>(clientsock),str_ip);
                    }
                    else vlogw("新连接--加入epoll失败");
                }
                else vlogw("新连接--接入失败");
            }

            //事件触发:有数据可读,或者连接断开
            else if (events[i].events & EPOLLIN)
            {
                char buf[_size_buf];
                memset(buf,0,sizeof(buf));
                size_t size = read(events[i].data.fd,&buf,sizeof(buf));

                //发生了错误或socket被对方关闭
                if(size <= 0)
                {
                    //把已断开的客户端从epoll中删除
                    if(epoll_del(events[i].data.fd) == 0)
                    {

                        //触发关闭回调
                        int fd = events[i].data.fd;
                        if(sock_close) _pool->add_work
                                (sock_close,make_shared<channel>(fd));
                    }
                    else vlogw("连接断开：epoll删除失败");
                }
                else
                {
                    //子线程解析（需要将字符串复制一份而不是引用）
                    int fd = events[i].data.fd;
                    string str(buf,size);
                    add_work(bind(&ux_epoll::parse_buf_th,this,fd,str));//

                    //原地拆包解析(无需多线程，但是可能降低IO遍历的能力)
                    //parse_buf(events[i].data.fd,buf,size);
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
    int ret = epoll_ctl(_fd_epoll, EPOLL_CTL_DEL, fd, &ev);
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
    return epoll_ctl(_fd_epoll,EPOLL_CTL_ADD,fd,&ev);
}
