#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include <thread>
#include <functional>

using namespace std;


//== 字符串类型转换 ==
template<typename T>
string to_string(const T& t){ ostringstream os; os<<t; return os.str(); }

template<typename T>
T from_string(const string& str){ T t; istringstream iss(str); iss>>t; return t; }
//== 字符串类型转换 ==



//!
//! 返回值：
//!     -1：socket打开失败
//!     -2：IP转换失败
//!     -3：connect连接失败
//!     sock：返回成功，建立的套接字
//!
int init_port(string ip,int port)
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
    servaddr.sin_port = htons(port);			  //兼容端口

    //IP转换
    if(inet_pton(AF_INET,ip.c_str(), &servaddr.sin_addr) <=0 )
    { return -2; }

    //建立连接
    if(connect(sock,(struct sockaddr*)&servaddr,sizeof(servaddr)) < 0)
    { return -3; }

    return sock;
}


//===== 分包协议 =====
struct ct_message
{
    size_t len;
    string content;
};
//===== 分包协议 =====


//size_t readn(int fd, void *buf, size_t len)
//{
//    size_t all = len;
//    char *pos = (char *)buf;
//    while (all > 0)
//    {
//        size_t size = read(fd,pos,all);
//        if (size == -1u)
//        {
//            if (errno == EINTR) size = 0;
//            else return -1;
//        }
//        else if (size == 0) return 0;
//        pos += size;
//        all -= size;
//    }
//    return len - all;
//}

//指定发送N个字节的内容（TCP可能会出现提前返回的情况，并不一定全部发送）
size_t writen(int sock,const void *buf,size_t len)
{
    size_t all = len;
    const char *pos = (const char *)buf;

    while (all > 0)
    {
        size_t res = write (sock,pos,all);
        if (res <= 0)
        {
            if (errno == EINTR) res = 0;
            else return -1;
        }
        pos += res;
        all -= res;
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

//读取反馈信息--线程启动
void read_msg_th(int fd,function<void(string)> func_cb)
{
    size_t all_len = 0;
    string all_content;
    while(true)
    {
        char buf[1024];
        memset(buf,0,sizeof(buf));
        size_t size = read(fd,&buf,sizeof(buf));

        //加入新内容（可能存在上一次的人剩余信息）
        all_len += size;
        all_content += string(buf,size);

        while(true)
        {
            //超过八个字节（判断是否能完整读出头部大小）
            if(all_len > sizeof(all_len))
            {
                //解析出ct_msg结构体的信息--长度
                size_t con_len = *(size_t*)string(all_content,0,sizeof(con_len)).c_str();

                //判断目前剩余量是否大于等于一个包的长度
                if((all_len - sizeof(all_len)) >= con_len)
                {
                    //解析的内容
                    string buf_content(all_content,sizeof(all_len),con_len);
                    if(func_cb) func_cb(buf_content);//解析出完整包后触发回调

                    //存放剩余的内容
                    all_len -= sizeof(all_len) + con_len;
                    all_content = string(all_content.begin() +
                                    sizeof(all_len) + con_len,all_content.end());
                }
                else break;
            }
            else break;
        }
    }
}

int main()
{
    int sock = init_port("127.0.0.1",5005);
    if(sock < 0) { cout<<"== init port err =="<<endl; return -1; }

    //处理反馈信息--回显到标准输出
    auto func_msg = [](string msg){
        cout<<msg<<endl;
    };

    //线程检测反馈信息
    thread (read_msg_th,sock,func_msg).detach();

    while (true)
    {
        string str;

        cout<<"send: "<<endl;
        cin>>str;
        if(str == "exit") { break; }

        ct_message msg;
        for(int i=0;i<1000;i++)
        {
            msg.content = str + to_string(i);
            msg.len = msg.content.size();

            if(send_msg(sock,msg,nullptr) == false)
            { cout<<"== send err =="<<endl; }
        }
    }

    cout<<"===== end ====="<<endl;
    return 0;
}
