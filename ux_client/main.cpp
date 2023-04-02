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
#include <cstring>
#include <vector>

using namespace std;


//===== stmv =====
//功能:字符串切割,按分隔符将字符串切割到数组
//算法:利用vector<bool>生成与字符串一样长的标记位
//      切割算法扫描到切割符时将vector<bool>对应标记位置1(切割符占领位)
//      然后将连续0段加入结果数组
//用法示例:
//      [1]
//      string a = "11--22--33";
//      string b = "11--22++33";
//      string c = "11 22 33 44++55--66";
//      vector<string> vec = vts::stmv(a)("--");
//      [ret = 11,22,33]
//      vector<string> vec1 = vts::stmv(b)("--");
//      [ret = 11,22++33]
//      vector<string> vec2 = vts::stmv(c)(" ","++","--");
//      [ret = 11,22,33,44,55,66]
//
struct stmv
{
    string v_str;
    vector<string> vec_flg;
    vector<bool> vec_bit;

    stmv(const string &str) : v_str(str) { vec_bit.resize(str.size(),false); }

    template<class ...Tarr>
    vector<string> operator()(const Tarr &...arg) { return push_flg(arg...); }

    //获取切割符
    template<class ...Tarr> vector<string> push_flg()
    { return split_value(v_str,vec_flg); }
    template<class ...Tarr>
    vector<string> push_flg(const string &flg,Tarr ...arg)
    { vec_flg.push_back(flg); return push_flg(arg...); };

    //根据标记切割字符串
    vector<string> split_value(const string &in_str,const vector<string> &in_flg)
    {
        vector<string> vec;

        //标记数循环
        for(size_t iflg=0;iflg<in_flg.size();iflg++)
        {
            //字符串标记排查,存在用bit标记
            size_t pos_begin = 0;
            while(true)
            {
                pos_begin = in_str.find(in_flg[iflg],pos_begin);
                if(pos_begin != in_str.npos)
                {
                    for(size_t il=0;il<in_flg[iflg].size();il++)
                    { vec_bit[pos_begin+il]=1; }
                    pos_begin+=1;
                }
                else break;
            }
        }

        //根据0/1状态获取字符串,加入返回结果
        string str;
        for(size_t i=0;i<vec_bit.size();i++)
        {
            if(vec_bit[i] == false)
            {
                if(i>0 && (vec_bit[i-1] == true)) str.clear();
                str+=in_str[i];
            }
            else if(i>0 && (vec_bit[i-1] == false)) vec.push_back(str);
        }

        //末尾无状态转跳时加入结果
        if(vec_bit[vec_bit.size()-1] == false)
        { vec.push_back(str); }

        return vec;
    }
};
//===== stmv =====


//== 字符串类型转换 ==
template<typename T>
string to_string(const T& t){ ostringstream os; os<<t; return os.str(); }

template<typename T>
T from_string(const string& str){ T t; istringstream iss(str); iss>>t; return t; }
//== 字符串类型转换 ==


//===== 结构体转换string函数 =====
//结构体转string
//      语法解析：(char*)&ct ，由&ct获取结构体地址，在由该地址(char*)转为char*类型的指针
//      根据string构造函数，参数1：char*地址，参数2：长度，可从地址内存中复制二进制内容
template <class T_ct>
static string ct_s(T_ct ct)
{ return string((char*)&ct,sizeof(T_ct)); }

//string转结构体
//      语法解析：*(T_ct*)str.c_str() ，由str.c_str()从string类获取const char*指针，
//      由const char*指针转为T_ct*指针，再*（T_ct*）从指针中获取值，从而返回值
template <class T_ct>
static T_ct st_c(const string &str)
{ T_ct ct = *(T_ct*)str.c_str(); return ct; }
//===== 结构体转换string函数 =====


//===== 消息处理结构体 =====
//== 请求类型 ==
enum en_transmit
{
    e_login,    //登陆
    e_swap,     //交换
    e_notify,   //通知
};

//== 消息权限 ==
enum en_msg
{
    e_public,   //群发
    e_private,  //私聊
};

//== 服务器解析聊天消息交互协议 ==
struct ct_msg_swap
{
    en_transmit et;     //用于判断是登陆还是发送信息
    en_msg em;          //用于判断是私聊还是群发
    size_t number_to;   //用于判断私聊时的目标ID
    size_t number_from; //用于存储发送者ID
    char name[64];      //发送者的昵称
    char buf[2048];     //存放发送的内容(登陆时:昵称,转发时:信息)
};
//===== 消息处理结构体 =====


//===== 网络连接初始化 =====
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
//===== 网络连接初始化 =====


//===== 发送协议 =====
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

//发送string字符串
bool send_msg(int sock,const string &msg,size_t *all)
{
    size_t len = msg.size();
    string buf;
    buf += string((char*)&len,sizeof(len));
    buf += msg;

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
//===== 发送协议 =====


//解析命令内容
string pares_send_cmd(const string &cmd,const string &name,size_t *number)
{
    ct_msg_swap ct_swap;
    memset(&ct_swap,0,sizeof(ct_swap));

    //查询所有用户信息
    if(cmd == "show") { ct_swap.et = e_notify; }
    else
    {
        string content = cmd;
        ct_swap.et = e_swap;
        ct_swap.em = e_public;

        //私聊,使用stmv按照分割副切割出容器,如果容器存在数据则标识为私聊
        vector<string> vec = stmv(cmd)(":");
        if(vec.size() >= 2)
        {
            ct_swap.em = e_private;
            ct_swap.number_to = from_string<size_t>(vec[0]);
            content = vec[1];
        }

        ct_swap.number_from = *number;
        strncpy(ct_swap.name,name.c_str(),sizeof(ct_swap.buf));
        strncpy(ct_swap.buf,content.c_str(),sizeof(ct_swap.buf));
    }
    return ct_s(ct_swap);
}


int main()
{
    size_t number = -1;

    int sock = init_port("127.0.0.1",5005);
    if(sock < 0) { cout<<"== init port err =="<<endl; return -1; }

    string name;
    cout<<"please input your name: "<<endl; cin>>name;

    //连接成功并登陆
    ct_msg_swap ct_login;
    memset(&ct_login,0,sizeof(ct_login));
    ct_login.et = e_login;
    strncpy(ct_login.buf,name.c_str(),sizeof(ct_login.buf));
    send_msg(sock,ct_s(ct_login),nullptr);

    //处理反馈信息--回显到标准输出
    auto func_msg = [&](string msg){
        ct_msg_swap ct = st_c<ct_msg_swap>(msg); //解析字符串成结构体

        //登陆反馈信息
        if(ct.et == e_login)
        {
            number = from_string<size_t>(ct.buf);
            if(number != -1u)
            { cout<<"<<<< login ID:"<<number<<" >>>>"<<endl; }
            else { cout<<"login err"<<number<<endl; exit(-1); }
        }

        //消息转发信息
        else if(ct.et == e_swap)
        {
            string stc_ms = "private";
            if(ct.em == e_public) stc_ms = "public";
            char buf[1024];
            snprintf(buf,sizeof(buf),"<<<< {%s: %s <%ld>} [%s] >>>>",
                     stc_ms.c_str(),ct.name,ct.number_from,ct.buf);
            cout<<buf<<endl;
        }

        //系统通知信息
        else if(ct.et == e_notify)
        {
            char buf[1024];
            snprintf(buf,sizeof(buf),"<<<< system: %s >>>>",ct.buf);
            cout<<buf<<endl;
        }
    };

    //线程检测反馈信息
    thread (read_msg_th,sock,func_msg).detach();

    //循环用户的信息输入
    while (true)
    {
        string str;
        cin>>str;
        if(str == "exit") { break; }

        str = pares_send_cmd(str,name,&number); //解析输入命令
        if(send_msg(sock,str,nullptr) == false)
        { cout<<"== send err =="<<endl; }
    }

    cout<<"===== end ====="<<endl;
    return 0;
}

//      输入命令规则:(三种命令输入方式)
//          [文字]:直接发送--公开模式群发
//                  例子:     大家好,我是小黄
//          ID:[文字]:用冒号分割,前面是登陆ID,后面是发送内容
//                  例子:     3:你好阿!3号,我是小黄
//          [show]:查看所有的已登陆用户
//                  例子:     show
//                      返回内容:
//                          <<<< system:
//                          1:小名
//                          2:小黄
//                          3:小虎
//                           >>>>
//
