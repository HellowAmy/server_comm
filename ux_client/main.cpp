#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>

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

size_t readn(int fd, void *buf, size_t len)
{
    size_t all = len;
    char *pos = (char *)buf;
    while (all > 0)
    {
        size_t size = read(fd,pos,all);
        if (size == -1u)
        {
            if (errno == EINTR) size = 0;
            else return -1;
        }
        else if (size == 0) return 0;
        pos += size;
        all -= size;
    }
    return len - all;
}

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

bool send_msg(int sock,const ct_msg &msg,size_t &all)
{
    size_t ret1 = writen(sock,&msg.len,sizeof(msg.len));
    size_t ret2 = writen(sock,msg.content.c_str(),msg.content.size());
    all += ret1 + ret2;
    return ret1 != -1u && ret2 != -1u;
}

void read_work(int fd)
{
    cout<<"read_work: "<<fd<<endl;
    size_t all_len = 0;
    string all_content;
    while(true)
    {
        char buf[1024];
        memset(buf,0,sizeof(buf));
        size_t size = read(fd,&buf,sizeof(buf));
        static int ic=0;
        ic++;


        all_len += size;
//        all_content += string(buf,size);
        all_content.append(string(buf,size));
        vlogw("read===: " vv(size) vv(ic) vv(all_len));

        bool is_break = false;
        while(true)
        {


            //超过八个字节（判断是否能完整读出头部大小）
            if(all_len > sizeof(all_len))
            {
                //解析出ct_msg结构体的信息--长度
                size_t con_len = *(size_t*)string(all_content,0,sizeof(con_len)).c_str();

                //判断目前剩余量是否大于等于一个包的长度
                vlogw("read vv===: " vv(all_len - sizeof(all_len)) vv(con_len));
                if((all_len - sizeof(all_len)) >= con_len)
                {
                    string buf_content(all_content,sizeof(all_len),con_len);

                    static int i=0;
                    i++;
                    size_t po = all_len - sizeof(all_len);
                    vlogw("show: " vv(buf_content) vv(i) vv(con_len) vv(po));

                    all_len -= sizeof(all_len) + con_len;
                    all_content = string(all_content.begin() +
                                    sizeof(all_len) + con_len,all_content.end());
                }
                else
                {
                    is_break = true;
                    vlogw("read  1");
                }
            }
            else
            {
                is_break = true;
                vlogw("read  2");
            }
            if(is_break) break;
        }
    }
}



//== 字符串类型转换 ==
template<typename T>
string to_string(const T& t){ ostringstream os; os<<t; return os.str(); }

template<typename T>
T from_string(const string& str){ T t; istringstream iss(str); iss>>t; return t; }
//== 字符串类型转换 ==

#include <cstring>
#include <thread>
int main()
{


//    size_t lo = 6;
//    char oi[8];
//    strncpy(oi,(char*)&lo,sizeof(lo));
////    string po(oi,sizeof(lo));

////    size_t lo2 = stoll(oi);
////    cout<<"po:"<<lo2<<endl;
//    for(int i=0;i<8;i++)
//    {
//        printf("[%x] ",oi[i]);
//    }

//    cout<<"============"<<endl;

    int sock = init_port("127.0.0.1",5005);
    if(sock < 0) { vloge("init_port err"); }

//    char buf[1024];
    thread th(read_work,sock);


    while (1) {

        ct_msg msg;
        cout<<"cin buf: "<<endl;
        cin>>msg.content;
        size_t ret = 0;
        string str = msg.content;
        msg.len = msg.content.size();

        cout<<"buf: "<<msg.content<<endl;
        cout<<"size: "<<msg.len<<endl;
        for(int i=0;i<10000;i++)
        {
//            str += to_string(i);
//            msg.content = str + to_string(i);
//            msg.len = msg.content.size();
            if(send_msg(sock,msg,ret) == false)
            { cout<<"send_msg err"<<endl; }
        }
        cout<<"ret: "<<ret<<endl;
    }

    th.join();


    cout << "Hello World!" << endl;
    return 0;
}
