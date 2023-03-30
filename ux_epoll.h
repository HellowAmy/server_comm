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
#include <sstream>
#include <map>
#include <queue>
#include <iostream>

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
struct ct_message
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

    //发送string字符串，带锁
    bool send_msg(const string &msg)
    {
        unique_lock<mutex> lock(_mutex);
        ct_message ct;
        ct.len = msg.size();
        ct.content = msg;
        if(send_msg(_fd,ct,NULL) == false)
            { close_cb(_fd); return false; }
        else return true;
    }

private:
    int _fd;            //连接套接字
    std::mutex _mutex;  //互斥锁--发送

    //指定发送N个字节的数据
    size_t writen(int sock,const void *buf,size_t len) const
    {
        size_t all = len;
        const char *pos = (const char *)buf;
        while (all > 0)
        {
            size_t res = write (sock,pos,all);
            if (res <= 0){ if (errno == EINTR){res = 0;} else{return -1;} }
            pos += res; all -= res;
        }
        return len;
    }

    //发送信息
    bool send_msg(int sock,const ct_message &msg,size_t *all)
    {
        string buf;
        buf += string((char*)&msg.len,sizeof(msg.len));
        buf += msg.content;
        size_t ret = writen(sock,buf.c_str(),buf.size());
        if(all != nullptr) *all = ret;
        return ret != -1u;
    }
};
//===== 数据管道 =====


//===== epoll事件循环 =====
class ux_epoll
{
public:
    ux_epoll();
    ~ux_epoll();
    int open_epoll(int port); //启动epoll服务器

    //新连接
    function<void(shared_ptr<channel> pch,const string &ip)>
            sock_new = nullptr;
    //关闭连接
    function<void(shared_ptr<channel> pch)>
            sock_close = nullptr;
    //读取数据
    function<void(shared_ptr<channel> pch,const string &msg)>
            sock_read = nullptr;

protected:
    int _size_event = 1024;     //单次IO扫描最大事件数
    int _size_buf = 4096;       //接收数据缓冲区大小
    int _fd_epoll = 0;          //epoll描述符
    mutex _mutex;               //互斥锁
    vpool_th *_pool;            //线程池
    mutex _mutex_parse;         //互斥锁--用于拆包解析函数
    condition_variable _cond;   //条件变量
    queue<function<void()>> _queue_task;    //解析任务队列
    map<int,ct_message> _map_save_read;     //存储fd拆包剩余数据

    int set_non_block(int fd);  //设置为非阻塞套接字
    int epoll_del(int fd);      //从epoll移除套接字
    int epoll_add(int fd);      //套接字加入epoll
    int init_port(int port);    //初始化监听端口，返回套接字
    void parse_buf(int fd,const char *buf,size_t size); //epoll水平触发拆包函数
    void parse_buf_th(int fd,string buf); //epoll水平触发拆包函数
    void work_parse_th();
    void add_work(function<void()> task);
};
//===== epoll事件循环 =====

#endif // UX_EPOLL_H
