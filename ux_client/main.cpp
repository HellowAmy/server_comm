#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

using namespace std;

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

struct ct_msg
{
    size_t len;
    string content;
};

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

bool send_msg(int sock,const ct_msg &msg)
{
    if((writen(sock,&msg.len,sizeof(msg.len)) == -1u) ||
        (writen(sock,msg.content .c_str(),msg.content .size()) == -1u))
            { return false; }
    else return true;
}

int main()
{

    int sock = init_port("127.0.0.1",5005);
    if(sock < 0) { vloge("init_port err"); }




    while (1) {

        ct_msg msg;
//        string str;
        cout<<"cin buf: "<<endl;
        cin>>msg.content;
        msg.len = msg.content.size();

//        size_t ret = 0;

        cout<<"buf: "<<msg.content<<endl;
        cout<<"size: "<<msg.len<<endl;
        for(int i=0;i<10;i++)
        {
            if(send_msg(sock,msg) == false)
            { cout<<"send_msg err"<<endl; }
//            ret += send(sock,str.c_str(),str.size(),MSG_WAITALL);//MSG_WAITALL
        }


//        cout<<"size: "<<ret<<endl;
    }


    cout << "Hello World!" << endl;
    return 0;
}
