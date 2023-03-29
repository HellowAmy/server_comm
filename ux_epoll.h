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

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

#include <mutex>
#include <sstream>
#include <map>
#include <iostream>
#include <functional>

using namespace std;
using namespace std::placeholders;

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


//== 字符串类型转换 ==
template<typename T>
string to_string(const T& t){ ostringstream os; os<<t; return os.str(); }

template<typename T>
T from_string(const string& str){ T t; istringstream iss(str); iss>>t; return t; }
//== 字符串类型转换 ==


//===== 线程池 =====
class vpool_th
{
public:
    //创建线程池
    vpool_th(size_t number)
    {
        //准备一个循环函数--给线程池内的线程[等待任务/执行任务]
        auto create_func = [=](){
            while(true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(_mutex);              //独占锁--获取队列任务
                    while (_tasks.empty() && _run) { _cond.wait(lock); }    //假唤醒--退出且队列为空
                    if(_run == false && _tasks.empty()) { return; }         //等待队列任务完成并退出任务
                    task = std::move(_tasks.front()); _tasks.pop();         //取任务
                }
                task(); //执行从队列获取的任务函数
            }
        };
        for(size_t i = 0;i<number;i++) { _workers.emplace_back(create_func); }
    }

    //释放线程池
    ~vpool_th()
    {
        { std::unique_lock<std::mutex> lock(_mutex); _run = false; }    //这里锁的用处--add_work执行时不给释放
        _cond.notify_all();                                             //唤醒所有线程准备退出
        for(std::thread &worker: _workers) { worker.join(); }           //等待所有线程完成任务后释放
    }

    //加入任务函数
    //      typename std::result_of<Tfunc(Targs...)>::type -- 获取外部函数的返回值类型
    template<class Tfunc, class... Targs>
    auto add_work(Tfunc&& func, Targs&&... args)
        -> std::future<typename std::result_of<Tfunc(Targs...)>::type>
    {
        using ret_type = typename std::result_of<Tfunc(Targs...)>::type;                //任务函数的返回类型
        auto pack = std::bind(std::forward<Tfunc>(func), std::forward<Targs>(args)...); //任务函数打包
        auto task = std::make_shared<std::packaged_task<ret_type()>>(pack);             //打包为连接future类
        auto res = task->get_future();                                                  //从future类获取函数返回值
        {
            std::unique_lock<std::mutex> lock(_mutex);              //锁住并准备将任务插入队列
            std::function<void()> func = [task](){ (*task)(); };    //包装外部任务函数到function
            if(_run) { _tasks.emplace(func); }                      //插入function到队列
        }
        _cond.notify_one(); //通知一个线程去完成任务
        return res;
    }

private:
    bool _run = true;                           //运行标记
    std::vector<std::thread> _workers;          //线程容器
    std::mutex _mutex;                          //线程池锁
    std::queue<std::function<void()>> _tasks;   //任务队列
    std::condition_variable _cond;              //条件变量
};
//===== 线程池 =====


//===== 分包协议 =====
struct ct_msg
{
    size_t len;
    string content;
};
//===== 分包协议 =====


//===== 数据管道 =====
class channel
{
public:
    function<int(int)> close_cb = nullptr;
    channel(int fd) : _fd(fd){}
    int get_fd() const { return _fd; }
    bool send_msg(const string &msg)
    {
        size_t ret = 0;
        ct_msg ct;
        ct.len = msg.size();
        ct.content = msg;
        if(send_msg(_fd,ct,ret) == false)
            { close_cb(_fd); return false; }
        else return true;
    }

private:
    int _fd;

    size_t writen(int sock,const void *buf,size_t len) const
    {
        size_t all = len;
        const char *pos = (const char *)buf;

        while (all > 0)
        {
            size_t res = write (sock,pos,all);
            if (res <= 0)
            { if (errno == EINTR) res = 0; else return -1; }
            pos += res; all -= res;
        }
        return len;
    }

    bool send_msg(int sock,const ct_msg &msg,size_t &all) const
    {
        size_t ret1 = writen(sock,&msg.len,sizeof(msg.len));
        size_t ret2 = writen(sock,msg.content.c_str(),msg.content.size());
        all += ret1 + ret2;
        return ret1 != -1u && ret2 != -1u;
    }
};
//===== 数据管道 =====


//===== epoll事件循环 =====
class ux_epoll
{
public:
    ux_epoll();
    ~ux_epoll();
    int open_epoll(int port);

    function<void(channel &ch)> sock_new = nullptr;//新连接
    function<void(channel &ch)> sock_close = nullptr;//关闭连接
    function<void(channel &ch,const string &msg)> sock_read = nullptr;//读取数据

protected:
    int v_max_ev = 1024;
    int v_max_read = 4096;
    int epollfd = 0;//epoll的描述符，用于添加和移除连接

    int set_non_block(int fd);//设置为非阻塞套接字
    int epoll_del(int fd);//从epoll移除套接字
    int epoll_add(int fd);//套接字加入epoll
    int init_port(int port);//放入监听端口号,返回套接字
    void event_new(int fd);
    void event_read(int fd);
    void parse_buf(int fd,const char *buf,size_t size);
    vpool_th *vpoll;
    mutex _mutex_send;

    map<int,ct_msg> map_read_cb;
};
//===== epoll事件循环 =====

#endif // UX_EPOLL_H
